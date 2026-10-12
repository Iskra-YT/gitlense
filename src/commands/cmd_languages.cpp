#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "analysis/languages.h"
#include "cli/progress.h"
#include "commands/command.h"
#include "core/error.h"
#include "format/chart.h"
#include "format/color.h"
#include "format/format.h"
#include "format/table.h"

namespace gitlense {

namespace {

/// Odczyt z const mapy - `operator[]` nie dziala na obiekcie const.
template <typename V>
V lookup(const std::map<std::string, V>& values, const std::string& key)
{
    const auto it = values.find(key);
    return it == values.end() ? V{} : it->second;
}

} // namespace

int runLanguages(const CommandContext& ctx, const Arguments& args)
{
    args.rejectUnknown({"limit", "n", "top", "since", "until", "author", "grep", "path",
                        "all", "first-parent", "reverse", "no-merges", "include", "exclude",
                        "by-author", "matrix", "by-commits", "by-lines", "no-current",
                        "top-authors", "header"},
                       "gitlense languages --by-author");

    const Repository& repo = ctx.repo;
    if (isNull(repo.headOrNull()) && !args.flag("all")) {
        std::cout << "repozytorium nie ma jeszcze commitow\n";
        return 0;
    }

    LanguageOptions options;
    options.walk = walkOptionsFromArgs(args);
    options.path = pathFilterFromArgs(args);
    options.threads = ctx.threads;
    options.topN = args.getSize("top", 0);
    options.topAuthorsPerLanguage = args.getSize("top-authors", 3);

    Progress progress("licze statystyki jezykow");
    const LanguageResult result = analyzeLanguages(repo, ctx.detector, options);
    progress.done();

    if (ctx.showHeader) {
        printRepoHeader(ctx, "gitlense languages",
                        number(result.languagesTotal) + " jezykow w historii, "
                            + std::to_string(result.currentLanguages.size())
                            + " w HEAD, " + number(result.churnTotal) + " linii zmienionych");
    }

    // ------------------------------------------------------------------
    // Tabela 1: jezyki w historii
    // ------------------------------------------------------------------
    Table history({{"JEZYK", Align::Left},
                   {"PLIKI", Align::Right},
                   {"COMMITY", Align::Right},
                   {"AUTORZY", Align::Right},
                   {"DODANE", Align::Right},
                   {"USUNIETE", Align::Right},
                   {"CHURN", Align::Right},
                   {"UDZIAL", Align::Right},
                   {"PIERWSZY", Align::Left},
                   {"OSTATNI", Align::Left},
                   {"NAJWIEKSZY AUTORZY", Align::Left}});

    std::size_t shown = 0;
    for (const LanguageHistory& item : result.history) {
        ++shown;
        if (options.topN != 0 && shown > options.topN) {
            break;
        }
        std::string topAuthors;
        for (std::size_t i = 0; i < item.topAuthors.size(); ++i) {
            if (i != 0) {
                topAuthors += ", ";
            }
            topAuthors += ellipsize(item.topAuthors[i].first, 14) + " ("
                + number(item.topAuthors[i].second) + ")";
        }
        history.addRow({Style::magenta().wrap(item.language),
                        number(static_cast<std::int64_t>(item.filesTouched)),
                        number(static_cast<std::int64_t>(item.commits)),
                        std::to_string(item.authors),
                        Style::green().wrap(number(item.additions)),
                        Style::red().wrap(number(item.deletions)),
                        number(item.churn()),
                        percent(item.churnShare),
                        item.firstCommit == 0 ? "-" : ctx.date(item.firstCommit),
                        item.lastCommit == 0 ? "-" : ctx.date(item.lastCommit),
                        topAuthors});
    }
    history.setFooter({"RAZEM (" + std::to_string(shown) + ")", "", "", "", "", "", "", "", "", "", ""});
    history.setCaption("HISTORIA - ile pracy wlozono w kazdy jezyk");
    history.print();

    // ------------------------------------------------------------------
    // Tabela 2: jezyki w HEAD
    // ------------------------------------------------------------------
    if (!args.flag("no-current") && !result.current.empty()) {
        std::cout << "\n";
        Table current({{"JEZYK", Align::Left},
                       {"PLIKI", Align::Right},
                       {"LINIE", Align::Right},
                       {"ROZMIAR", Align::Right},
                       {"UDZIAL", Align::Left},
                       {"% KODU", Align::Right}});
        std::uint64_t totalLines = 0;
        for (const LanguageFact& item : result.current) {
            totalLines += item.lines;
        }
        for (const LanguageFact& item : result.current) {
            const double share = totalLines == 0
                ? 0.0
                : static_cast<double>(item.lines) / static_cast<double>(totalLines);
            current.addRow({Style::magenta().wrap(item.language),
                            number(static_cast<std::int64_t>(item.files)),
                            number(static_cast<std::int64_t>(item.lines)),
                            bytes(item.bytes),
                            progressBar(share, 18, Style::magenta(), Style::dim()),
                            percent(share)});
        }
        current.setFooter({"RAZEM", "", number(static_cast<std::int64_t>(totalLines)), "", "", "100.0%"});
        current.setCaption("STAN OBECNY - co jest w HEAD teraz");
        current.print();
    }

    // ------------------------------------------------------------------
    // Macierz: autor x jezyk
    // ------------------------------------------------------------------
    if (args.flag("by-author") || args.flag("matrix")) {
        std::cout << "\n";

        // kolumny: jezyki wraz z danymi, wybrane wg --by-*
        enum class Order { Churn, Commits, Lines };
        Order order = Order::Churn;
        if (args.flag("by-commits")) {
            order = Order::Commits;
        } else if (args.flag("by-lines")) {
            order = Order::Lines;
        }

        // wybierz jezyki do macierzy
        std::vector<std::string> columns;
        switch (order) {
        case Order::Churn: {
            for (const LanguageHistory& item : result.history) {
                columns.push_back(item.language);
            }
            break;
        }
        case Order::Commits: {
            std::vector<LanguageHistory> sorted = result.history;
            std::sort(sorted.begin(), sorted.end(),
                      [](const LanguageHistory& a, const LanguageHistory& b) {
                          if (a.commits != b.commits) {
                              return a.commits > b.commits;
                          }
                          return a.churn() > b.churn();
                      });
            for (const LanguageHistory& item : sorted) {
                columns.push_back(item.language);
            }
            break;
        }
        case Order::Lines:
            for (const LanguageFact& item : result.current) {
                columns.push_back(item.language);
            }
            break;
        }
        if (options.topN != 0 && columns.size() > options.topN) {
            columns.resize(options.topN);
        }

        // mapa jezyk -> liczba linii w HEAD (do opcji by-lines)
        std::map<std::string, std::size_t> currentLines;
        for (const LanguageFact& item : result.current) {
            currentLines[item.language] = item.lines;
        }

        Table matrix({{"AUTOR", Align::Left}});
        for (const std::string& language : columns) {
            matrix.addColumn({ellipsize(language, 11), Align::Right});
        }
        matrix.addColumn({"SUMA", Align::Right});
        matrix.addColumn({"% CALOSCI", Align::Right});

        // suma po kolumnach (dla udzialu w calosci)
        std::map<std::string, std::uint64_t> columnTotals;
        for (const AuthorLanguageRow& row : result.authors) {
            for (const std::string& language : columns) {
                if (order == Order::Commits) {
                    columnTotals[language]
                        += static_cast<std::uint64_t>(lookup(row.commitsByLanguage, language));
                } else {
                    columnTotals[language]
                        += static_cast<std::uint64_t>(lookup(row.churnByLanguage, language));
                }
            }
        }
        std::uint64_t grandTotal = 0;
        for (const std::string& language : columns) {
            grandTotal += columnTotals[language];
        }

        for (const AuthorLanguageRow& row : result.authors) {
            std::vector<std::string> cells;
            cells.push_back(ellipsize(row.author, 20));
            std::uint64_t rowTotal = 0;
            for (const std::string& language : columns) {
                const std::uint64_t churn = lookup(row.churnByLanguage, language);
                std::string cell;
                switch (order) {
                case Order::Churn:
                    cell = churn == 0 ? Style::dim().wrap(".") : number(churn);
                    rowTotal += churn;
                    break;
                case Order::Commits: {
                    const std::uint64_t commits
                        = static_cast<std::uint64_t>(lookup(row.commitsByLanguage, language));
                    cell = commits == 0 ? Style::dim().wrap(".") : number(commits);
                    rowTotal += commits;
                    break;
                }
                case Order::Lines: {
                    // procent linii danego jezyka w HEAD usunietych przez autora
                    const std::size_t lines = lookup(currentLines, language);
                    const double share = lines == 0
                        ? 0.0
                        : 100.0 * static_cast<double>(churn) / static_cast<double>(lines);
                    cell = churn == 0 ? Style::dim().wrap(".") : percent(share, 0);
                    rowTotal += churn;
                    break;
                }
                }
                cells.push_back(std::move(cell));
            }
            cells.push_back(Style::bold().wrap(number(rowTotal)));
            cells.push_back(percent(grandTotal == 0
                                        ? 0.0
                                        : static_cast<double>(rowTotal)
                                              / static_cast<double>(grandTotal),
                                    0));
            matrix.addRow(std::move(cells));
        }
        matrix.setCaption(std::string("AUTOR x JEZYK - ")
                          + (order == Order::Commits
                                 ? "ile commitow w danym jezyku"
                                 : (order == Order::Lines
                                        ? "procent usunietych linii danego jezyka (z HEAD)"
                                        : "ile linii zmienil autor w danym jezyku")));
        matrix.print();
    }
    return 0;
}

extern const Command languagesCommand = {
    "languages",
    "statystyki jezykow: co jest w repo, ile pracy wlozono, kto co zmienial",
    R"(Uzycie: gitlense languages [opcje]

      --by-author       macierz autor x jezyk
      --matrix          to samo co --by-author
      --by-commits      macierz liczona wzgledem liczby commitow
      --by-lines        macierz jako procent usunietych linii jezyka
      --top N           tylko N jezykow z najwiekszym churnem
      --top-authors N   ilu autorow pokazac w kolumnie (domyslnie 3)
      --no-current      pomija stan biezacy (HEAD)
      --since / --until / --author / --path / --all
                       filtry historii tak samo jak w `gitlense log`)",
    R"(  gitlense languages
  gitlense languages --top 8
  gitlense languages --by-author --top 6
  gitlense languages --since=1.year --by-lines)",
    &runLanguages,
};

} // namespace gitlense
