#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

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

std::string paddedName(const std::string& name, std::size_t width)
{
    if (name.size() >= width) {
        return middleEllipsize(name, width);
    }
    return name + std::string(width - name.size(), ' ');
}

} // namespace

int runShortlog(const CommandContext& ctx, const Arguments& args)
{
    args.rejectUnknown({"summary", "numbered", "number", "email", "since", "until", "author",
                        "committer", "grep", "path", "all", "first-parent", "reverse",
                        "no-merges", "include", "exclude", "header", "language"},
                       "gitlense shortlog --summary");

    const Repository& repo = ctx.repo;
    const Oid start = args.flag("all") ? kNullOid : repo.headOrNull();
    if (isNull(start) && !args.flag("all")) {
        std::cout << "brak commitow\n";
        return 0;
    }

    WalkOptions walk = walkOptionsFromArgs(args);
    const PathFilter path = pathFilterFromArgs(args);
    const std::string language = args.get("language", "");
    const bool summary = args.flag("summary");
    const bool numbered = args.flag("numbered") || args.flag("number");
    const bool showEmail = args.flag("email");

    HistoryOptions historyOptions;
    historyOptions.walk = walk;
    HistoryScan scan(repo, historyOptions, ctx.detector);

    struct Entry {
        std::string name;
        std::string email;
        std::size_t commits{0};
        std::uint64_t churn{0};
        std::vector<std::string> subjects;
        Timestamp lastCommit{0};
    };
    std::map<std::string, Entry> byAuthor;

    Progress progress("zbieram commitow");
    scan.forEach(
        [&](const CommitChange& change) {
            std::uint64_t churn = 0;
            for (const FileChangeRecord& file : change.files) {
                if (!path.accepts(file.path) && !path.accepts(file.oldPath)) {
                    continue;
                }
                if (!language.empty() && file.language != language) {
                    continue;
                }
                churn += file.churn();
            }
            const std::string key = change.authorEmail.empty()
                ? change.authorName
                : change.authorEmail + "\t" + change.authorName;
            Entry& entry = byAuthor[key];
            entry.name = change.authorName;
            entry.email = change.authorEmail;
            entry.commits += 1;
            entry.churn += churn;
            entry.lastCommit = std::max(entry.lastCommit, change.time);
            if (summary) {
                entry.subjects.push_back("      " + change.subject);
            }
        },
        ctx.threads);
    progress.done();

    std::vector<Entry> entries;
    for (auto& [key, entry] : byAuthor) {
        entries.push_back(std::move(entry));
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.commits != b.commits) {
            return a.commits > b.commits;
        }
        return a.name < b.name;
    });

    if (ctx.showHeader) {
        std::size_t total = 0;
        for (const Entry& entry : entries) {
            total += entry.commits;
        }
        printRepoHeader(ctx, "gitlense shortlog",
                        plural(total, "commit", "commity", "commitow") + ", "
                            + plural(entries.size(), "autor", "autorow", "autorow"));
    }

    if (summary) {
        // klasyczny uklad shortlog
        std::size_t nameWidth = 6;
        for (const Entry& entry : entries) {
            const std::string label = showEmail && !entry.email.empty()
                ? entry.name + " <" + entry.email + ">"
                : entry.name;
            nameWidth = std::max(nameWidth, label.size());
        }
        for (const Entry& entry : entries) {
            const std::string label = showEmail && !entry.email.empty()
                ? entry.name + " <" + entry.email + ">"
                : entry.name;
            std::cout << Style::bold().wrap(paddedName(label, nameWidth)) << "  "
                      << Style::yellow().wrap(std::to_string(entry.commits)) << "  "
                      << Style::dim().wrap("ostatni: "
                                           + (entry.lastCommit == 0
                                                  ? std::string("-")
                                                  : ctx.date(entry.lastCommit)))
                      << "\n";
            for (const std::string& subject : entry.subjects) {
                std::cout << subject << "\n";
            }
            std::cout << "\n";
        }
        return 0;
    }

    // tryb tabelaryczny
    Table table({{"#", Align::Right},
                 {"AUTOR", Align::Left},
                 {"COMMITY", Align::Right},
                 {"CHURN", Align::Right},
                 {"UDZIAL", Align::Right},
                 {"PROCENT", Align::Left}});

    std::size_t totalCommits = 0;
    std::uint64_t totalChurn = 0;
    for (const Entry& entry : entries) {
        totalCommits += entry.commits;
        totalChurn += entry.churn;
    }

    std::size_t index = 0;
    for (const Entry& entry : entries) {
        ++index;
        const double share = totalCommits == 0
            ? 0.0
            : static_cast<double>(entry.commits) / static_cast<double>(totalCommits);
        const std::string label = showEmail && !entry.email.empty()
            ? entry.name + " <" + entry.email + ">"
            : entry.name;
        table.addRow({numbered ? std::to_string(index) : "",
                      ellipsize(label, 30),
                      Style::bold().wrap(number(static_cast<std::int64_t>(entry.commits))),
                      number(entry.churn),
                      percent(share, 0),
                      progressBar(share, 20, Style::cyan(), Style::dim())});
    }
    table.setFooter({"", "RAZEM", number(static_cast<std::int64_t>(totalCommits)),
                     number(totalChurn), "100.0%", ""});
    table.print();
    return 0;
}

extern const Command shortlogCommand = {
    "shortlog",
    "podsumowanie autorow: ile commitow i tematy ich zmian",
    R"(Uzycie: gitlense shortlog [opcje]

  -s, --summary        lista commitow pod kazdym autorem
  -n, --numbered       numeracja autorow
  -e, --email          pokaz adresy e-mail
      --language LANG  licz tylko zmiany w jednym jezyku
      --since / --until / --author / --path / --all
                      filtry tak samo jak w `gitlense log`)",
    R"(  gitlense shortlog
  gitlense shortlog --summary
  gitlense shortlog --numbered --email --since=1.year)",
    &runShortlog,
};

} // namespace gitlense
