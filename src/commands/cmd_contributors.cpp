#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "analysis/contributors.h"
#include "cli/progress.h"
#include "commands/command.h"
#include "core/error.h"
#include "format/chart.h"
#include "format/color.h"
#include "format/format.h"
#include "format/table.h"

namespace gitlense {
namespace {

std::string bar(double share, std::size_t width = 14)
{
    return progressBar(share, width, Style::cyan(), Style::dim());
}

std::string trendArrow(Timestamp first, Timestamp last, Timestamp reference)
{
    const Timestamp span = reference - first;
    if (span <= 0) {
        return Style::dim().wrap("-");
    }
    // dzielimy okres na 4 czesci i patrzymy, ktora jest najswiezsza
    const Timestamp quarter = span / 4;
    const Timestamp recent = reference - last;
    if (recent <= quarter / 2) {
        return Style::green().wrap("\u25b2 aktywny");
    }
    if (recent <= quarter) {
        return Style::yellow().wrap("\u25b0 ostyglo");
    }
    return Style::dim().wrap("\u25bc nieaktywny");
}

} // namespace

int runContributors(const CommandContext& ctx, const Arguments& args)
{
    args.rejectUnknown({"limit", "n", "top", "since", "until", "author", "committer", "grep",
                        "path", "all", "first-parent", "reverse", "no-merges", "include",
                        "exclude", "language", "no-ownership", "ownership-top", "recent",
                        "email", "header", "include-vendored", "include-generated"},
                       "gitlense contributors --since=1.year");

    const Repository& repo = ctx.repo;
    if (isNull(repo.headOrNull()) && !args.flag("all")) {
        std::cout << "repozytorium nie ma jeszcze commitow\n";
        return 0;
    }

    ContributorOptions options;
    options.walk = walkOptionsFromArgs(args);
    options.path = pathFilterFromArgs(args);
    options.language = args.get("language", "");
    options.threads = ctx.threads;
    options.computeOwnership = !args.flag("no-ownership");
    options.ownershipTop = args.getSize("ownership-top", 0);
    if (args.has("recent")) {
        const std::string value = args.get("recent");
        options.recentWindow = value == "week" ? 7 * 86400
                                : value == "month" ? 30 * 86400
                                : value == "quarter" ? 91 * 86400
                                : value == "year" ? 365 * 86400
                                                    : 30 * 86400;
    }

    Progress progress("licze statystyki autorow");
    ContributorResult result = analyzeContributors(repo, ctx.detector, options);
    progress.done();

    if (ctx.showHeader) {
        std::string subtitle = plural(result.commitsTotal, "commit", "commity", "commitow")
            + ", " + plural(result.authorsTotal, "autor", "autorow", "autorow") + ", "
            + number(static_cast<std::int64_t>(result.churnTotal)) + " linii zmienionych";
        if (!options.walk.since.has_value() && !options.walk.until.has_value()) {
            subtitle += " (cala historia)";
        }
        printRepoHeader(ctx, "gitlense contributors", subtitle);
    }

    const std::size_t top = args.getSize("top", args.getSize("n", 0));

    // ------------------------------------------------------------------
    // Tabela 1: HISTORIA
    // ------------------------------------------------------------------
    Table history({{"#", Align::Right},
                   {"AUTOR", Align::Left},
                   {"COMMITY", Align::Right},
                   {"DODANE", Align::Right},
                   {"USUNIETE", Align::Right},
                   {"CHURN", Align::Right},
                   {"UDZIAL", Align::Right},
                   {"PLIKI", Align::Right},
                   {"DZNI", Align::Right},
                   {"PIERWSZY", Align::Left},
                   {"OSTATNI", Align::Left},
                   {"AKTYWNOSC", Align::Left}});

    std::size_t index = 0;
    const Timestamp reference = now();
    for (const ContributorHistory& item : result.history) {
        ++index;
        if (top != 0 && index > top) {
            break;
        }
        const std::string label = args.flag("email") && !item.email.empty()
            ? item.name + " <" + item.email + ">"
            : item.name;
        const double share = result.churnTotal == 0
            ? 0.0
            : static_cast<double>(item.churn()) / static_cast<double>(result.churnTotal);
        history.addRow({std::to_string(index),
                        ellipsize(label, 22),
                        number(static_cast<std::int64_t>(item.commits)),
                        Style::green().wrap(number(item.additions)),
                        Style::red().wrap(number(item.deletions)),
                        number(item.churn()),
                        percent(share),
                        number(static_cast<std::int64_t>(item.filesTouched)),
                        std::to_string(item.activeDays),
                        item.firstCommit == 0 ? "-" : ctx.date(item.firstCommit),
                        item.lastCommit == 0 ? "-" : ctx.date(item.lastCommit),
                        trendArrow(item.firstCommit, item.lastCommit, reference)});
    }
    history.setFooter({std::to_string(index), "RAZEM",
                       number(static_cast<std::int64_t>(result.commitsTotal)),
                       Style::green().wrap(number(result.additionsTotal)),
                       Style::red().wrap(number(result.deletionsTotal)),
                       number(result.churnTotal), percentWhole(1.0), "", "", "", "", ""});
    history.setCaption("HISTORIA - kto i ile zmienil w commitach");
    history.print();

    // ------------------------------------------------------------------
    // Tabela 2: STAN OBECNY (wlasciciele plikow w HEAD)
    // ------------------------------------------------------------------
    if (result.ownershipComputed) {
        std::cout << "\n";
        Table current({{"#", Align::Right},
                       {"AUTOR", Align::Left},
                       {"PLIKI", Align::Right},
                       {"LINIE", Align::Right},
                       {"WLASNOSC", Align::Left},
                       {"% KODU", Align::Right},
                       {"OST. ZMIANA", Align::Left},
                       {"WIEK", Align::Left},
                       {"PLIKI MT", Align::Right},
                       {"LINIE MT", Align::Right},
                       {"CHURN (HISTORIA)", Align::Right}});

        std::size_t currentIndex = 0;
        for (const ContributorNow& item : result.current) {
            if (item.ownedLines == 0 && item.ownedFiles == 0) {
                continue;
            }
            ++currentIndex;
            if (top != 0 && currentIndex > top) {
                break;
            }
            // `percent()` przyjmuje ulamek 0..1, wiec nie mnozymy przez 100
            const double share = item.totalLines == 0
                                     ? 0.0
                                     : static_cast<double>(item.ownedLines)
                                           / static_cast<double>(item.totalLines);
            current.addRow({std::to_string(currentIndex),
                            ellipsize(item.name, 22),
                            number(static_cast<std::int64_t>(item.ownedFiles)),
                            number(static_cast<std::int64_t>(item.ownedLines)),
                            bar(share),
                            percent(share),
                            item.lastTouch == 0 ? "-" : ctx.date(item.lastTouch),
                            item.lastTouch == 0
                                ? "-"
                                : formatDate(item.lastTouch, DateStyle::Relative),
                            number(static_cast<std::int64_t>(item.lastTouchFiles)),
                            number(static_cast<std::int64_t>(item.lastTouchLines)),
                            number(item.churn())});
        }
        current.setFooter({"", "RAZEM", number(static_cast<std::int64_t>(result.filesOwnedTotal)),
                           number(static_cast<std::int64_t>(result.linesTotal)), "",
                           percentWhole(1.0), "", "", "", "", ""});
        current.setCaption("STAN OBECNY - kto jest wlascicielem plikow w HEAD (git blame)");
        current.print();
    }

    if (!ctx.quiet) {
        std::cerr << "\n"
                  << Style::dim().wrap("wszystkie liczby z commitow (dodane/usuniete linie); "
                                       "wlasciciel z blame dotyczy linii, ktore przetrwaly do "
                                       "HEAD")
                  << "\n";
    }
    return 0;
}

extern const Command contributorsCommand = {
    "contributors",
    "kto ile zmienil w historii i kto teraz jest wlascicielem plikow",
    R"(Uzycie: gitlense contributors [opcje]

  -n, --top N            ogranicz do N najwiekszych
      --top N            to samo co --top
      --since DATA       okres analizy (domyslnie cala historia)
      --until DATA
      --author PAT       tylko wybrany autor
      --language LANG    tylko zmiany w jednym jezyku
      --path GLOB        tylko wskazane pliki
      --include / --exclude
      --all              cale repozytorium
      --first-parent     bez commity scalajace
      --no-ownership     pomija kolumne blame (szybsze na duzych repo)
      --ownership-top N  licz blame tylko dla N najwiekszych autorow
      --email            pokaz adresy e-mail
      --recent OKRES    dodatkowa kolumna z ruchem (week|month|quarter|year)

  Stala opcja:
      --no-header        bez naglowka)",
    R"(  gitlense contributors
  gitlense contributors --since=6.months --top 10
  gitlense contributors --language C++
  gitlense contributors --no-ownership --top 20)",
    &runContributors,
};

} // namespace gitlense
