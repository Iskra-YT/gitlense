#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "analysis/hotspots.h"
#include "cli/progress.h"
#include "commands/command.h"
#include "core/error.h"
#include "format/chart.h"
#include "format/color.h"
#include "format/format.h"
#include "format/table.h"

namespace gitlense {

int runHotspots(const CommandContext& ctx, const Arguments& args)
{
    args.rejectUnknown({"top", "limit", "n", "since", "until", "author", "grep", "path", "all",
                        "first-parent", "reverse", "no-merges", "include", "exclude", "language",
                        "by-author", "no-current", "sort", "header"},
                       "gitlense hotspots --top 20");

    const Repository& repo = ctx.repo;
    if (isNull(repo.headOrNull()) && !args.flag("all")) {
        std::cout << "repozytorium nie ma jeszcze commitow\n";
        return 0;
    }

    HotspotOptions options;
    options.walk = walkOptionsFromArgs(args);
    options.path = pathFilterFromArgs(args);
    options.language = args.get("language", "");
    options.threads = ctx.threads;
    options.topN = args.getSize("top", args.getSize("n", 20));

    Progress progress("licze hotspote");
    HotspotResult result = analyzeHotspots(repo, ctx.detector, options);
    progress.done();

    if (ctx.showHeader) {
        printRepoHeader(ctx, "gitlense hotspots",
                        plural(result.filesAnalyzed, "plik", "pliki", "plikow") + ", "
                            + number(result.linesAnalyzed) + " linii, "
                            + number(result.churnTotal) + " linii zmienionych w historii");
    }

    // ------------------------------------------------------------------
    // Tabela: najgorętsze pliki
    // ------------------------------------------------------------------
    Table table({{"#", Align::Right},
                 {"PLIK", Align::Left},
                 {"JEZYK", Align::Left},
                 {"LINIE", Align::Right},
                 {"COMMITY", Align::Right},
                 {"AUTORZY", Align::Right},
                 {"CHURN", Align::Right},
                 {"CHURN/LINIE", Align::Right},
                 {"WIEK", Align::Left},
                 {"OSTATNI AUTOR", Align::Left},
                 {"RYZYKO", Align::Left}});

    const Timestamp reference = now();
    std::size_t index = 0;
    for (const HotspotFile& file : result.files) {
        ++index;
        if (options.topN != 0 && index > options.topN) {
            break;
        }
        std::string age = "-";
        if (file.lastModified > 0) {
            age = formatDate(file.lastModified, DateStyle::Relative);
        }
        table.addRow({std::to_string(index),
                      shortenPath(ctx, file.path),
                      file.language.empty() ? "-" : Style::magenta().wrap(file.language),
                      number(static_cast<std::int64_t>(file.lines)),
                      number(static_cast<std::int64_t>(file.commits)),
                      std::to_string(file.authors),
                      Style::bold().wrap(number(file.churn)),
                      file.lines == 0 ? "-" : number(static_cast<std::int64_t>(file.churnPerLine)),
                      age,
                      ellipsize(file.lastAuthor, 16),
                      progressBar(file.score, 12, Style::red(), Style::dim())});
    }
    if (table.rows() == 0) {
        std::cout << "brak plikow spelniajacych kryteria\n";
    } else {
        table.setCaption("HOTSPOTY - pliki z najwiekszym ruchem (im wiekszy churn i wiecej "
                         "autorow, tym wieksze ryzyko)");
        table.print();
    }

    // ------------------------------------------------------------------
    // Ranking autorow po ruchu
    // ------------------------------------------------------------------
    if (args.flag("by-author") && !result.authors.empty()) {
        std::cout << "\n";
        Table authors({{"#", Align::Right},
                       {"AUTOR", Align::Left},
                       {"COMMITY", Align::Right},
                       {"CHURN", Align::Right},
                       {"UDZIAL RUCHU", Align::Right},
                       {"UDZIAL", Align::Left}});
        std::size_t authorIndex = 0;
        for (const HotspotAuthor& author : result.authors) {
            ++authorIndex;
            if (options.topN != 0 && authorIndex > options.topN) {
                break;
            }
            authors.addRow({std::to_string(authorIndex),
                            ellipsize(author.author, 22),
                            number(static_cast<std::int64_t>(author.commits)),
                            number(author.churn),
                            percentWhole(author.churnShare / 100.0),
                            progressBar(author.churnShare / 100.0, 18, Style::cyan(),
                                        Style::dim())});
        }
        authors.setCaption("KTO GENERUJE RUCH - ranking autorow");
        authors.print();
    }

    if (!ctx.quiet) {
        std::cerr << "\n"
                  << Style::dim().wrap("churn = dodane + usuniete linie; wiek liczony wzgledem "
                                           "terazniejszosci; ryzyko uwzglednia uplyw czasu "
                                           "(poltrwanie wagi 180 dni)")
                  << "\n";
    }
    return 0;
}

extern const Command hotspotsCommand = {
    "hotspots",
    "pliki o najwiekszym ruchu w historii - potencjalne miejsca konfliktow",
    R"(Uzycie: gitlense hotspots [opcje]

  --top N             ile plikow pokazac (domyslnie 20)
  --by-author         dodatkowo ranking autorow po ruchu
  --language LANG     tylko pliki jednego jezyka
  --sort churn|lines  kolejnosc sortowania (domyslnie churn)
  --since / --until / --path / --all
                      filtry historii tak samo jak w `gitlense log`)",
    R"(  gitlense hotspots
  gitlense hotspots --top 10 --by-author
  gitlense hotspots --language C++ --top 15
  gitlense hotspots --since=1.year --top 5)",
    &runHotspots,
};

} // namespace gitlense
