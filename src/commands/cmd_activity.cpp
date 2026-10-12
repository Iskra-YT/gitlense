#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "analysis/activity.h"
#include "cli/progress.h"
#include "commands/command.h"
#include "core/error.h"
#include "format/chart.h"
#include "format/color.h"
#include "format/format.h"
#include "format/table.h"

namespace gitlense {
namespace {

std::string dayName(std::size_t index)
{
    static const char* names[] = {"poniedzialek", "wtorek", "sroda", "czwartek",
                                  "piatek", "sobota", "niedziela"};
    return names[index % 7];
}

} // namespace

int runActivity(const CommandContext& ctx, const Arguments& args)
{
    args.rejectUnknown({"by", "since", "until", "author", "grep", "path", "all", "first-parent",
                        "reverse", "no-merges", "include", "exclude", "buckets", "width",
                        "top", "header"},
                       "gitlense activity --by=month");

    const Repository& repo = ctx.repo;
    if (isNull(repo.headOrNull()) && !args.flag("all")) {
        std::cout << "repozytorium nie ma jeszcze commitow\n";
        return 0;
    }

    ActivityOptions options;
    options.walk = walkOptionsFromArgs(args);
    options.path = pathFilterFromArgs(args);
    options.threads = ctx.threads;
    const std::string by = args.get("by", "month");
    const bool byAuthor = (by == "author");
    options.granularity = byAuthor ? Granularity::Month : parseGranularity(by);
    options.maxBuckets = args.getSize("buckets", 24);

    Progress progress("licze aktywnosc");
    ActivityResult result = analyzeActivity(repo, ctx.detector, options);
    progress.done();

    if (ctx.showHeader) {
        printRepoHeader(ctx, "gitlense activity",
                        plural(result.totalCommits, "commit", "commity", "commitow") + ", "
                            + number(result.totalChurn) + " linii zmienionych, "
                            + std::to_string(result.activeDays.size()) + " aktywnych dni");
    }

    if (result.buckets.empty()) {
        std::cout << "brak danych w wybranym zakresie\n";
        return 0;
    }

    // ------------------------------------------------------------------
    // Wykres
    // ------------------------------------------------------------------
    std::vector<std::uint64_t> values;
    values.reserve(result.buckets.size());
    for (const ActivityBucket& bucket : result.buckets) {
        values.push_back(bucket.commits);
    }
    const std::size_t width = args.getSize("width", 0);
    const std::size_t chartWidth = width == 0 ? std::max<std::size_t>(result.buckets.size(), 1)
                                              : width;

    std::cout << Style::bold().wrap("Commity w czasie ("
                                    + std::string(toString(result.granularity)) + ")")
              << "\n";
    std::cout << verticalBars(values, chartWidth, Style::cyan(), Style::dim());

    std::cout << "\n" << Style::dim().wrap("  trend: ")
              << (result.trendPerPeriod > 0.5
                      ? Style::green().wrap("rosnacy")
                      : (result.trendPerPeriod < -0.5 ? Style::red().wrap("mniejacy")
                                                      : Style::yellow().wrap("plaski")))
              << Style::dim().wrap(" (" + [&] {
                     char buffer[32];
                     std::snprintf(buffer, sizeof buffer, "%+.1f", result.trendPerPeriod);
                     return std::string(buffer);
                 }() + " commitow na okres)")
              << "\n";
    std::cout << Style::dim().wrap("  najdluzsza seria aktywnych dni: "
                                   + std::to_string(result.longestStreak))
              << "\n";
    std::cout << Style::dim().wrap("  obecna seria: " + std::to_string(result.currentStreak))
              << "\n\n";

    // ------------------------------------------------------------------
    // Tabela okresow
    // ------------------------------------------------------------------
    Table buckets({{"OKRES", Align::Left},
                   {"COMMITY", Align::Right},
                   {"DODANE", Align::Right},
                   {"USUNIETE", Align::Right},
                   {"CHURN", Align::Right},
                   {"AUTORZY", Align::Right},
                   {"PLIKI", Align::Right},
                   {"UDZIAL", Align::Left}});

    for (const ActivityBucket& bucket : result.buckets) {
        const double share = result.totalCommits == 0
            ? 0.0
            : static_cast<double>(bucket.commits) / static_cast<double>(result.totalCommits);
        buckets.addRow({bucket.label,
                        Style::bold().wrap(number(static_cast<std::int64_t>(bucket.commits))),
                        Style::green().wrap(number(bucket.additions)),
                        Style::red().wrap(number(bucket.deletions)),
                        number(bucket.churn()),
                        std::to_string(bucket.authors),
                        number(static_cast<std::int64_t>(bucket.files.size())),
                        progressBar(share, 14, Style::cyan(), Style::dim())});
    }
    buckets.setFooter({"RAZEM",
                       number(static_cast<std::int64_t>(result.totalCommits)),
                       Style::green().wrap(number(result.additionsTotal)),
                       Style::red().wrap(number(result.deletionsTotal)),
                       number(result.totalChurn), "", "", "100.0%"});
    buckets.setCaption("Rozklad aktywnosci w okresach");
    buckets.print();

    // ------------------------------------------------------------------
    // Heatmapa godzin
    // ------------------------------------------------------------------
    if (options.granularity == Granularity::Day) {
        std::cout << "\n" << Style::bold().wrap("Kiedy w ciagu dnia powstaja commity") << "\n";
        Table heat({{"GODZINA", Align::Left}, {"COMMITY", Align::Right},
                    {"HISTOGRAM", Align::Left}});
        std::size_t max = 0;
        for (const std::size_t count : result.commitsOnHours) {
            max = std::max(max, count);
        }
        for (std::size_t hour = 0; hour < 24; ++hour) {
            char label[8];
            std::snprintf(label, sizeof label, "%02u:00", static_cast<unsigned>(hour));
            heat.addRow({label,
                         std::to_string(result.commitsOnHours[hour]),
                         progressBar(max == 0 ? 0.0
                                              : static_cast<double>(result.commitsOnHours[hour])
                                                    / static_cast<double>(max),
                                     30, Style::blue(), Style::dim())});
        }
        heat.print();
    } else {
        std::cout << "\n" << Style::bold().wrap("Rozklad w tygodniu") << "\n";
        Table week({{"DZIEN", Align::Left}, {"COMMITY", Align::Right},
                    {"HISTOGRAM", Align::Left}});
        std::size_t max = 0;
        for (const std::size_t count : result.commitsOnWeekdays) {
            max = std::max(max, count);
        }
        for (std::size_t day = 1; day < 7; ++day) { // od poniedzialku (tm_wday=1)
            const std::size_t count = result.commitsOnWeekdays[day % 7];
            week.addRow({dayName(day), std::to_string(count),
                         progressBar(max == 0 ? 0.0
                                              : static_cast<double>(count)
                                                    / static_cast<double>(max),
                                     30, Style::blue(), Style::dim())});
        }
        week.print();
    }

    // ------------------------------------------------------------------
    // Ranking autorow
    // ------------------------------------------------------------------
    const std::size_t top = args.getSize("top", 10);
    if (top != 0 && !result.authors.empty()) {
        std::cout << "\n";
        Table authors({{"#", Align::Right},
                       {"AUTOR", Align::Left},
                       {"COMMITY", Align::Right},
                       {"CHURN", Align::Right},
                       {"TYDZ./TYDZ", Align::Right},
                       {"PIERWSZY", Align::Left},
                       {"OSTATNI", Align::Left},
                       {"TYDZ. TEMU", Align::Left}});
        std::size_t index = 0;
        for (const AuthorActivity& author : result.authors) {
            ++index;
            if (index > top) {
                break;
            }
            char perWeek[16];
            std::snprintf(perWeek, sizeof perWeek, "%.1f", author.commitsPerWeek);
            authors.addRow({std::to_string(index),
                            ellipsize(author.author, 22),
                            number(static_cast<std::int64_t>(author.commits)),
                            number(author.churn),
                            perWeek,
                            author.firstCommit == 0 ? "-" : ctx.date(author.firstCommit),
                            author.lastCommit == 0 ? "-" : ctx.date(author.lastCommit),
                            author.lastCommit == 0
                                ? "-"
                                : formatDate(author.lastCommit, DateStyle::Relative)});
        }
        authors.setCaption("Aktywnosc autorow");
        authors.print();
    }

    // heatmapa autora x okres
    if (byAuthor && !result.authors.empty()) {
        std::cout << "\n";
        Table matrix({{"AUTOR", Align::Left}});
        for (const ActivityBucket& bucket : result.buckets) {
            matrix.addColumn({ellipsize(bucket.label, 8), Align::Right});
        }
        for (const AuthorActivity& author : result.authors) {
            std::vector<std::string> cells;
            cells.push_back(ellipsize(author.author, 18));
            for (const ActivityBucket& bucket : result.buckets) {
                const auto it = std::find_if(author.perPeriod.begin(), author.perPeriod.end(),
                                             [&](const auto& item) {
                                                 return item.first == bucket.start;
                                             });
                const std::size_t value = it == author.perPeriod.end() ? 0 : it->second;
                cells.push_back(value == 0 ? Style::dim().wrap(".") : std::to_string(value));
            }
            matrix.addRow(std::move(cells));
        }
        matrix.setCaption("Autor x okres - liczba commitow");
        matrix.print();
    }
    return 0;
}

extern const Command activityCommand = {
    "activity",
    "wykres aktywnosci w czasie, rozklad commitow i autorow",
    R"(Uzycie: gitlense activity [opcje]

  --by OKRES         granulacja: day, week, month, year (domyslnie month)
  --by author        dodatkowa macierz autor x okres
  --buckets N        ile okresow pokazac (domyslnie 24)
  --width N          szerokosc wykresu
  --top N            ilu autorow w rankingu (domyslnie 10)
  --since / --until / --author / --path / --all
                      filtry historii tak samo jak w `gitlense log`)",
    R"(  gitlense activity
  gitlense activity --by=day --buckets 30
  gitlense activity --by=week --since=1.year
  gitlense activity --by=month --by-author --top 5)",
    &runActivity,
};

} // namespace gitlense
