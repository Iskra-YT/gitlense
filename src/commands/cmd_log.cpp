#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>

#include "analysis/history.h"
#include "cli/progress.h"
#include "commands/command.h"
#include "core/error.h"
#include "core/walker.h"
#include "format/chart.h"
#include "format/color.h"
#include "format/format.h"
#include "format/table.h"

namespace gitlense {
namespace {

/// Kolorowanie autorow: ten sam kolor dla tego samego autora w calym
/// wypisie (jak w `git log --graph`).
class AuthorPalette {
public:
    Style styleFor(const std::string& author)
    {
        static const char* colors[] = {"36", "33", "32", "35", "34", "31", "96", "93",
                                       "92", "95", "94", "91"};
        const auto it = index_.find(author);
        std::size_t slot = 0;
        if (it != index_.end()) {
            slot = it->second;
        } else {
            slot = next_++ % (sizeof(colors) / sizeof(colors[0]));
            index_[author] = slot;
        }
        return Style::custom(std::string(colors[slot]));
    }

private:
    std::map<std::string, std::size_t> index_;
    std::size_t next_{0};
};

std::string statusColor(ChangeStatus status)
{
    switch (status) {
    case ChangeStatus::Added:
        return Style::green().wrap("A");
    case ChangeStatus::Deleted:
        return Style::red().wrap("D");
    case ChangeStatus::Modified:
        return Style::yellow().wrap("M");
    case ChangeStatus::Renamed:
        return Style::blue().wrap("R");
    case ChangeStatus::Copied:
        return Style::blue().wrap("C");
    case ChangeStatus::TypeChange:
        return Style::magenta().wrap("T");
    default:
        return Style::dim().wrap(std::string(1, statusCode(status)));
    }
}

} // namespace

int runLog(const CommandContext& ctx, const Arguments& args)
{
    args.rejectUnknown({"limit", "n", "since", "until", "author", "committer", "grep", "path",
                        "all", "first-parent", "reverse", "no-merges", "include", "exclude",
                        "stat", "name-only", "name-status", "short", "decorate", "header"},
                       "gitlense log --stat --since=6.months");

    const Repository& repo = ctx.repo;
    const Oid start = args.flag("all") ? kNullOid : repo.headOrNull();
    if (isNull(start) && !args.flag("all")) {
        std::cout << "repozytorium nie ma jeszcze commitow\n";
        return 0;
    }

    WalkOptions walk = walkOptionsFromArgs(args);
    const bool showStat = args.flag("stat");
    const bool nameOnly = args.flag("name-only");
    const bool nameStatus = args.flag("name-status");
    const bool shortHash = args.flag("short");
    const bool decorate = args.flag("decorate");

    const std::string branch = repo.headDescription();
    if (ctx.showHeader) {
        std::cout << Style::bold().wrap("gitlense log") << "  "
                  << Style::dim().wrap(branch) << "\n\n";
    }

    AuthorPalette palette;
    HistoryOptions historyOptions;
    historyOptions.walk = walk;
    HistoryScan scan(repo, historyOptions, ctx.detector);
    const std::vector<CommitInfo> commits = scan.commits();

    std::size_t shown = 0;
    for (const CommitInfo& commit : commits) {
        const std::size_t parents = commit.parents.size();

        // --stat / --name-status wymagaja diffu
        DiffStats diff;
        bool haveDiff = false;
        if (showStat || nameOnly || nameStatus || parents == 0) {
            diff = repo.diff(parents == 0 ? kNullOid : commit.parents.front(), commit.id);
            haveDiff = true;
        }

        const bool isHead = !parents || commit.id == repo.headOrNull();
        std::string decoration;
        if (decorate) {
            if (isHead) {
                decoration = "HEAD";
            }
            if (!branch.empty() && branch.find("HEAD") == std::string::npos) {
                decoration += decoration.empty() ? "" : " -> ";
                decoration += branch;
            }
        }

        std::cout << Style::yellow().wrap(ellipsize(toShortHex(
                             commit.id, shortHash ? 7 : 10), shortHash ? 7 : 10))
                  << " ";
        std::cout << Style::dim().wrap(ctx.date(commit.author.time)) << " ";
        std::cout << palette.styleFor(commit.author.name).wrap(commit.author.name);
        std::cout << "  " << commit.subject << "\n";
        if (!decoration.empty()) {
            std::cout << "    " << Style::cyan().wrap(decoration) << "\n";
        }

        if (showStat && haveDiff) {
            Table stat({{"PLIK", Align::Left},
                        {"LANG", Align::Left},
                        {"DODANE", Align::Right},
                        {"USUNIETE", Align::Right},
                        {Style::dim().wrap("ZMIANY"), Align::Left}});
            for (const FileChange& file : diff.files) {
                const std::string language = ctx.detector.languageOf(file.path);
                stat.addRow({shortenPath(ctx, file.path),
                             language.empty() ? "-" : language,
                             file.additions == 0 ? "-"
                                                 : Style::green().wrap(number(file.additions)),
                             file.deletions == 0 ? "-"
                                                 : Style::red().wrap(number(file.deletions)),
                             file.binary ? Style::dim().wrap("binarny")
                                         : progressBar(static_cast<double>(file.additions)
                                                           / static_cast<double>(
                                                               std::max<std::uint64_t>(1,
                                                                                       file.churn())),
                                                       12, Style::green(), Style::red())});
            }
            std::cout << "  " << stat.render() << "\n";
        } else if (nameStatus || nameOnly) {
            for (const FileChange& file : diff.files) {
                if (nameOnly) {
                    std::cout << "    " << shortenPath(ctx, file.displayPath()) << "\n";
                } else {
                    std::cout << "    " << statusColor(file.status) << "  "
                              << shortenPath(ctx, file.displayPath()) << "\n";
                }
            }
        }
        ++shown;
    }

    if (!ctx.quiet) {
        std::cerr << "\n" << Style::dim().wrap("pokazano " + std::to_string(shown) + " z "
                                               + plural(commits.size(), "commit", "commity",
                                                        "commitow"))
                  << "\n";
    }
    return 0;
}

extern const Command logCommand = {
    "log",
    "historia commitow z filtrowaniem i statystykami plikow",
    R"(Uzycie: gitlense log [opcje]

  -n, --limit N        ile commitow pokazac
      --since DATA     od kiedy (2026-01-01, "2 weeks ago", 30 days)
      --until DATA     do kiedy
      --author PAT     filtr autora (fragment nazwiska lub maila)
      --committer PAT  filtr committera
      --grep PAT       filtr tresci commita
      --path GLOB      tylko commity dotykajace plikow
      --all            cale repozytorium, nie tylko aktualna galeaz
      --first-parent   bez commity scalajacych
      --reverse        od najstarszego
      --no-merges      pomija commity scalajace
      --stat           tabela plikow z wykresem zmian
      --name-status    lista plikow ze statusem (A/D/M)
      --name-only      sama lista plikow
      --short          krotsze skroty commitow
      --decorate       pokaz HEAD i nazwe galezi
      --include GLOB / --exclude GLOB
                       filtr sciezek
      --no-header      bez naglowka)",
    R"(  gitlense log -n 20
  gitlense log --stat --since=3.months --author=anna
  gitlense log --path "src/core/**" --name-status
  gitlense log --all --first-parent --reverse)",
    &runLog,
};

} // namespace gitlense
