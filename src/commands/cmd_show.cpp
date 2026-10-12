#include <algorithm>
#include <iostream>
#include <vector>

#include "cli/progress.h"
#include "commands/command.h"
#include "core/error.h"
#include "format/chart.h"
#include "format/color.h"
#include "format/format.h"
#include "format/table.h"

namespace gitlense {
namespace {

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
    default:
        return Style::dim().wrap(std::string(1, statusCode(status)));
    }
}

} // namespace

int runShow(const CommandContext& ctx, const Arguments& args)
{
    args.rejectUnknown({"header", "stat", "name-status", "name-only", "short", "patch",
                        "decorate"},
                       "gitlense show <commit>");

    if (args.positional().empty()) {
        throw UsageError("gitlense show wymaga argumentu, np. `gitlense show HEAD`");
    }
    const std::string spec = args.positional().front();
    const Repository& repo = ctx.repo;
    const Oid id = repo.revparse(spec);
    const CommitInfo commit = repo.commit(id);
    const Oid parent = commit.parents.empty() ? kNullOid : commit.parents.front();
    const DiffStats diff = repo.diff(parent, id);

    if (ctx.showHeader) {
        std::cout << Style::bold().wrap("commit ") << toHex(commit.id) << "\n";
        if (!commit.parents.empty()) {
            std::string parents;
            for (const Oid& item : commit.parents) {
                parents += (parents.empty() ? "" : " ") + toShortHex(item);
            }
            std::cout << Style::dim().wrap("parent: ") << parents << "\n";
        }
        std::cout << "autor:     " << commit.author.name << " <" << commit.author.email
                  << ">\n";
        std::cout << "committer: " << commit.committer.name << " <" << commit.committer.email
                  << ">\n";
        std::cout << "data:      " << ctx.date(commit.author.time) << " ("
                  << commit.author.timezone << ")\n";
        std::cout << "opis:      " << repo.describe(id) << "\n\n";
    }

    // pelna tresc komunikatu
    std::string message = commit.subject;
    if (commit.message.size() > commit.subject.size()) {
        message += commit.message.substr(commit.subject.size());
    }
    // obetnij koncowe puste linie
    while (!message.empty() && (message.back() == '\n')) {
        message.pop_back();
    }
    std::cout << Style::bold().wrap("Wiadomosc") << "\n";
    std::cout << "    " << message << "\n\n";

    if (args.flag("patch")) {
        std::cout << Style::bold().wrap("Patch") << "\n";
        std::cout << Style::dim().wrap("  (uzyj `gitlense diff " + toShortHex(parent) + ".."
                                       + toShortHex(id) + "` po szczegoly, lub `git show "
                                       + toShortHex(id) + "`)")
                  << "\n\n";
    }

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
                                        10, Style::green(), Style::red())});
    }
    table.setCaption("Zmiany (" + plural(diff.filesChanged, "plik", "pliki", "plikow") + ", +"
                     + number(diff.additions) + "/-" + number(diff.deletions) + ")");
    table.print();
    return 0;
}

extern const Command showCommand = {
    "show",
    "szczegoly commita: autor, data, pliki, statystyki",
    R"(Uzycie: gitlense show <commit> [opcje]

  --stat           tabela plikow (domyslnie)
  --name-status    lista plikow ze statusem
  --name-only      sama lista plikow
  --patch          naglowek sekcji patcha
  --no-header      bez metryk commita)",
    R"(  gitlense show HEAD
  gitlense show v1.0.0
  gitlense show main~3 --name-status)",
    &runShow,
};

} // namespace gitlense
