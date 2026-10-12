#include <algorithm>
#include <iostream>
#include <vector>

#include "commands/command.h"
#include "core/error.h"
#include "core/walker.h"
#include "format/chart.h"
#include "format/color.h"
#include "format/format.h"
#include "format/table.h"

namespace gitlense {

int runBranches(const CommandContext& ctx, const Arguments& args)
{
    args.rejectUnknown({"all", "remote", "no-remote", "verbose", "header", "merged", "no-merged"},
                       "gitlense branches --remote");

    const Repository& repo = ctx.repo;
    const bool wantRemote = !args.flag("no-remote");
    std::vector<BranchInfo> branches = repo.branches(wantRemote);

    if (args.has("merged") || args.has("no-merged")) {
        const bool merged = args.has("merged");
        const Oid base = repo.revparse(args.get(merged ? "merged" : "no-merged", "HEAD"));
        // jeden spacer po osiagalnych commitach, potem test zawartosci
        std::vector<Oid> reachable = [&] {
            CommitWalker walker(repo, WalkOptions{}, base);
            return walker.collect();
        }();
        std::vector<BranchInfo> filtered;
        for (const BranchInfo& branch : branches) {
            if (isNull(branch.tip)) {
                continue;
            }
            const bool isAncestor
                = std::find(reachable.begin(), reachable.end(), branch.tip) != reachable.end();
            if (isAncestor == merged) {
                filtered.push_back(branch);
            }
        }
        branches = std::move(filtered);
    }

    if (ctx.showHeader) {
        printRepoHeader(ctx, "gitlense branches",
                        plural(branches.size(), "ref", "refy", "refow"));
    }

    Table table({{"GALAZ", Align::Left},
                 {"OSTATNI COMMIT", Align::Left},
                 {"DATA", Align::Left},
                 {"WIEK", Align::Left},
                 {"AHEAD/BEHIND", Align::Right},
                 {"UDZIAL", Align::Left},
                 {"OPIS", Align::Left}});

    for (const BranchInfo& branch : branches) {
        const bool isCurrent = branch.current;
        std::string name = isCurrent ? Style::bold().wrap("* " + branch.name) : "  " + branch.name;
        if (branch.remote) {
            name = Style::dim().wrap(name);
        }

        std::string tracking;
        if (branch.hasTracking) {
            std::string text;
            if (branch.ahead > 0) {
                text += Style::green().wrap("\u2191" + std::to_string(branch.ahead));
            }
            if (branch.behind > 0) {
                text += Style::red().wrap("\u2193" + std::to_string(branch.behind));
            }
            if (text.empty()) {
                text = Style::dim().wrap("=");
            }
            tracking = text + Style::dim().wrap(" " + branch.upstream);
        } else if (!branch.remote) {
            tracking = Style::dim().wrap("brak upstream");
        }

        // udzial commitow galezi, ktore sa jeszcze nie wlane do upstream
        std::string share = "-";
        if (branch.hasTracking && (branch.ahead > 0 || branch.behind > 0)) {
            share = progressBar(
                static_cast<double>(branch.ahead)
                    / static_cast<double>(std::max<std::size_t>(1, branch.ahead + branch.behind)),
                10, Style::green(), Style::red());
        }

        table.addRow({name,
                      Style::yellow().wrap(toShortHex(branch.tip, 8)),
                      ctx.date(branch.author.time),
                      branch.author.time == 0 ? "-"
                                              : formatDate(branch.author.time, DateStyle::Relative),
                      tracking,
                      share,
                      ellipsize(branch.subject, 44)});
    }

    if (table.rows() == 0) {
        std::cout << "brak galezi\n";
        return 0;
    }
    table.print();
    return 0;
}

extern const Command branchesCommand = {
    "branches",
    "galezie lokalne i zdalne wraz z relacja ahead/behind",
    R"(Uzycie: gitlense branches [opcje]

  --remote        pokaz galezie zdalne (domyslnie tak)
  --no-remote     tylko lokalne
  --all           to samo co --remote
  --merged [ref]  tylko galezie wlane do ref (domyslnie HEAD)
  --no-merged [ref]  tylko galezie, ktore nie sa wlane)",
    R"(  gitlense branches
  gitlense branches --no-remote
  gitlense branches --merged main)",
    &runBranches,
};

} // namespace gitlense
