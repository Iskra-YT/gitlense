#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "commands/command.h"
#include "core/error.h"
#include "format/chart.h"
#include "format/color.h"
#include "format/format.h"
#include "format/table.h"

namespace gitlense {
namespace {

/// Rozbija argumenty na dwa refy. Obsluguje `A..B`, `A...B` oraz
/// pojedynczy ref (porownanie z katalogiem roboczym).
bool parseRange(const std::vector<std::string>& positional, std::string& left,
                std::string& right)
{
    if (positional.empty()) {
        return false;
    }
    const std::string& first = positional.front();
    const std::size_t triple = first.find("...");
    if (triple != std::string::npos) {
        left = first.substr(0, triple);
        right = first.substr(triple + 3);
        return true;
    }
    const std::size_t doubleDot = first.find("..");
    if (doubleDot != std::string::npos) {
        left = first.substr(0, doubleDot);
        right = first.substr(doubleDot + 2);
        return true;
    }
    return false;
}

std::string statusCell(ChangeStatus status)
{
    switch (status) {
    case ChangeStatus::Added:
        return Style::green().wrap("dodany");
    case ChangeStatus::Deleted:
        return Style::red().wrap("usuniety");
    case ChangeStatus::Modified:
        return Style::yellow().wrap("zmieniony");
    case ChangeStatus::Renamed:
        return Style::blue().wrap("przeniesiony");
    case ChangeStatus::Copied:
        return Style::blue().wrap("skopiowany");
    case ChangeStatus::TypeChange:
        return Style::magenta().wrap("typ");
    case ChangeStatus::Untracked:
        return Style::cyan().wrap("niesledzony");
    default:
        return Style::dim().wrap(std::string(1, statusCode(status)));
    }
}

} // namespace

int runDiff(const CommandContext& ctx, const Arguments& args)
{
    args.rejectUnknown({"stat", "name-status", "name-only", "short", "header", "summary",
                        "workdir"},
                       "gitlense diff main..feature");

    const Repository& repo = ctx.repo;
    std::string fromSpec;
    std::string toSpec;
    const bool hasRange = parseRange(args.positional(), fromSpec, toSpec);

    Oid fromId = kNullOid;
    Oid toId = kNullOid;
    bool useWorkdir = false;

    if (hasRange) {
        fromId = repo.revparse(fromSpec);
        toId = repo.revparse(toSpec);
    } else if (!args.positional().empty() && !args.flag("workdir")) {
        toId = repo.revparse(args.positional().front());
        fromId = repo.headOrNull();
        if (isNull(fromId)) {
            fromId = toId;
        }
    } else {
        // porownanie z katalogiem roboczym
        useWorkdir = true;
        const Oid head = repo.headOrNull();
        if (isNull(head)) {
            throw Error("repozytorium nie ma jeszcze commitow - nie ma z czym porownac");
        }
        fromId = head;
    }

    const DiffStats diff = useWorkdir ? repo.diffWorkdir(fromId) : repo.diff(fromId, toId);

    if (ctx.showHeader) {
        std::string subtitle = useWorkdir
            ? toShortHex(fromId) + " \u2192 katalog roboczy"
            : toShortHex(fromId) + " \u2192 " + toShortHex(toId);
        if (!diff.files.empty()) {
            subtitle += "  |  " + plural(diff.filesChanged, "plik", "pliki", "plikow") + ", +"
                + number(diff.additions) + "/-" + number(diff.deletions);
        }
        printRepoHeader(ctx, "gitlense diff", subtitle);
    }

    if (diff.files.empty()) {
        std::cout << "brak zmian\n";
        return 0;
    }

    const bool nameOnly = args.flag("name-only");
    const bool nameStatus = args.flag("name-status");
    const bool shortStat = args.flag("short");

    if (nameOnly || nameStatus) {
        Table table({{"STATUS", Align::Left}, {"PLIK", Align::Left}});
        for (const FileChange& file : diff.files) {
            std::string path = shortenPath(ctx, file.displayPath());
            if (!file.oldPath.empty() && file.oldPath != file.path) {
                path = shortenPath(ctx, file.oldPath) + " \u2192 " + path;
            }
            // --name-only zostawia sama litere statusu, jak w git
            table.addRow({nameOnly ? Style::dim().wrap(std::string(1, statusCode(file.status)))
                                   : statusCell(file.status),
                          path});
        }
        table.print();
        return 0;
    }

    // tabela per plik + wykres zmian
    Table table({{"PLIK", Align::Left},
                 {"JEZYK", Align::Left},
                 {"STATUS", Align::Left},
                 {"DODANE", Align::Right},
                 {"USUNIETE", Align::Right},
                 {"ZMIANY", Align::Left}});
    for (const FileChange& file : diff.files) {
        const std::string language = ctx.detector.languageOf(file.displayPath());
        std::string path = shortenPath(ctx, file.displayPath());
        if (!file.oldPath.empty() && file.oldPath != file.path) {
            path = shortenPath(ctx, file.oldPath) + " \u2192 " + path;
        }
        table.addRow({path,
                      language.empty() ? "-" : language,
                      statusCell(file.status),
                      file.binary || file.additions == 0
                          ? "-"
                          : Style::green().wrap(number(file.additions)),
                      file.binary || file.deletions == 0
                          ? "-"
                          : Style::red().wrap(number(file.deletions)),
                      file.binary ? Style::dim().wrap("binarny")
                                  : progressBar(
                                        static_cast<double>(file.additions)
                                            / static_cast<double>(
                                                std::max<std::uint64_t>(1, file.churn())),
                                        12, Style::green(), Style::red())});
    }

    // wiersz sumaryczny: te same kolory co wiersze, bez prefiksow +/-
    table.setFooter({plural(diff.filesChanged, "plik", "pliki", "plikow"), "",
                      Style::dim().wrap("lacznie"),
                      diff.additions == 0 ? "-"
                                          : Style::green().wrap(number(diff.additions)),
                      diff.deletions == 0 ? "-" : Style::red().wrap(number(diff.deletions)),
                      ""});
    table.print();

    if (shortStat) {
        return 0;
    }

    // podsumowanie: kto najwiecej zmienil w tym diffie
    const Oid target = useWorkdir ? fromId : toId;
    std::cout << "\n" << Style::bold().wrap("Autorzy ostatniego commita tego stanu")
              << "\n";
    Table authors({{"AUTOR", Align::Left}, {"DATA", Align::Left}, {"COMMIT", Align::Left}});
    const CommitInfo commit = repo.commit(target);
    authors.addRow({commit.author.name, ctx.date(commit.author.time),
                    Style::yellow().wrap(toShortHex(commit.id)) + "  " + commit.subject});
    authors.print();
    return 0;
}

extern const Command diffCommand = {
    "diff",
    "porownanie dwoch stanow repozytorium lub katalogu roboczego",
    R"(Uzycie: gitlense diff [<A>..<B>] [<ref>] [opcje]

  --name-status    lista plikow ze statusem
  --name-only      sama lista plikow
  --short          bez tabeli podsumowania
  --workdir        wymusz porownanie z katalogiem roboczym

  Zapis:
    A..B            diff między dwoma refami
    A...B           to samo co A..B
    <ref>           stan tego refu wobec HEAD
    (bez argumentow) HEAD wobec katalogu roboczego)",
    R"(  gitlense diff
  gitlense diff main..feature
  gitlense diff v1.0.0...HEAD --name-status
  gitlense diff HEAD~5)",
    &runDiff,
};

} // namespace gitlense
