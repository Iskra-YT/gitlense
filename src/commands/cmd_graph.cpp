#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "commands/command.h"
#include "core/error.h"
#include "core/walker.h"
#include "format/color.h"
#include "format/format.h"
#include "format/table.h"

namespace gitlense {
namespace {

/// Rysuje graf commitow w stylu `git log --graph`.
///
/// Kazda kolumna odpowiada jednej galezi. Wiersz commita dostaje
/// gwiazdke w swojej kolumnie, a galezie rodzace odchodza ukosnikiem
/// `|\`. Gdy galaz wpada z powrotem do glownej kolumny, rysujemy `|/`.
class GraphRenderer {
public:
    explicit GraphRenderer(const Repository& repo, std::size_t maxLanes)
        : repo_(repo), maxLanes_(maxLanes == 0 ? 8 : maxLanes)
    {
    }

    /// Zwraca linie wiersza commita (bez tekstu commita) oraz
    /// ewentualna linie lacznika (pusta, gdy nie ma galezi bocznych).
    struct Row {
        std::string prefix;
        std::string connector;
    };

    Row next(const CommitInfo& commit)
    {
        Row row;
        if (lanes_.empty()) {
            lanes_.push_back(kNullOid);
        }

        // 1. znajdz swoja kolumne
        std::size_t lane = lanes_.size();
        for (std::size_t i = 0; i < lanes_.size(); ++i) {
            if (lanes_[i] == commit.id) {
                lane = i;
                break;
            }
        }
        if (lane == lanes_.size()) {
            lane = firstFreeColumn();
        }
        if (lane >= lanes_.size()) {
            lanes_.resize(lane + 1, kNullOid);
        }

        // 2. zamknij galezie, ktore wpadaja z powrotem do tego commita
        std::vector<std::size_t> closing;
        for (std::size_t i = 0; i < lanes_.size(); ++i) {
            if (i == lane || isNull(lanes_[i])) {
                continue;
            }
            if (repo_.isAncestor(lanes_[i], commit.id)) {
                closing.push_back(i);
            }
        }

        // 3. przydziel kolumny rodzicom (pierwszy zostaje w obecnej)
        std::vector<std::size_t> parentLanes;
        parentLanes.push_back(lane);
        for (std::size_t p = 1; p < commit.parents.size(); ++p) {
            std::size_t target = firstFreeColumnExcept(lane);
            if (target == lanes_.size() && lanes_.size() >= maxLanes_) {
                break; // za duzo galezi - pokaz tylko glowowa
            }
            if (target >= lanes_.size()) {
                lanes_.resize(target + 1, kNullOid);
            }
            parentLanes.push_back(target);
        }

        // 4. narysuj wiersz
        std::size_t width = std::max(lanes_.size(), lane + 1);
        for (const std::size_t index : parentLanes) {
            width = std::max(width, index + 1);
        }
        std::vector<char> cells(width, ' ');
        for (std::size_t i = 0; i < lanes_.size(); ++i) {
            if (i != lane && !isNull(lanes_[i])) {
                cells[i] = '|';
            }
        }
        for (const std::size_t index : closing) {
            cells[index] = '/';
        }
        cells[lane] = '*';

        std::string prefix;
        for (std::size_t i = 0; i < width; ++i) {
            if (i != 0) {
                prefix += Style::dim().wrap("-");
            }
            if (cells[i] == '*') {
                prefix += Style::yellow().wrap("*");
            } else if (cells[i] == '/') {
                prefix += Style::red().wrap("/");
            } else if (cells[i] == '|') {
                prefix += Style::dim().wrap("|");
            } else {
                prefix += " ";
            }
        }
        row.prefix = prefix;

        // 5. lacznik `|\` dla nowych galezi
        if (parentLanes.size() > 1 || !closing.empty()) {
            std::vector<char> connector(width, ' ');
            connector[lane] = '|';
            for (std::size_t p = 1; p < parentLanes.size(); ++p) {
                connector[parentLanes[p]] = '\\';
            }
            for (const std::size_t index : closing) {
                connector[index] = '|';
            }
            std::string text;
            for (std::size_t i = 0; i < width; ++i) {
                if (i != 0) {
                    text += Style::dim().wrap("-");
                }
                text += connector[i] == '\\' ? Style::dim().wrap("\\") : std::string(1, connector[i]);
            }
            row.connector = text;
        }

        // 6. przesun kolumny na rodzicow
        lanes_[lane] = commit.parents.empty() ? kNullOid : commit.parents.front();
        for (std::size_t p = 1; p < parentLanes.size(); ++p) {
            lanes_[parentLanes[p]] = commit.parents[p];
        }
        for (const std::size_t index : closing) {
            lanes_[index] = kNullOid;
        }

        // 7. usun puste kolumny po prawej
        while (!lanes_.empty() && isNull(lanes_.back())) {
            lanes_.pop_back();
        }
        return row;
    }

    [[nodiscard]] bool active() const { return !lanes_.empty(); }

private:
    std::size_t firstFreeColumn() const
    {
        for (std::size_t i = 0; i < lanes_.size(); ++i) {
            if (isNull(lanes_[i])) {
                return i;
            }
        }
        return lanes_.size();
    }

    std::size_t firstFreeColumnExcept(std::size_t skip) const
    {
        for (std::size_t i = 0; i < lanes_.size(); ++i) {
            if (i != skip && isNull(lanes_[i])) {
                return i;
            }
        }
        return lanes_.size();
    }

    const Repository& repo_;
    std::size_t maxLanes_;
    std::vector<Oid> lanes_;
};

} // namespace

int runGraph(const CommandContext& ctx, const Arguments& args)
{
    args.rejectUnknown({"limit", "n", "all", "first-parent", "no-merges", "since", "until",
                        "author", "grep", "reverse", "width", "short", "no-metadata"},
                       "gitlense graph -n 40");

    const Repository& repo = ctx.repo;
    const Oid start = args.flag("all") ? kNullOid : repo.headOrNull();
    if (isNull(start) && !args.flag("all")) {
        std::cout << "brak commitow\n";
        return 0;
    }

    WalkOptions walk = walkOptionsFromArgs(args);
    if (walk.limit == 0) {
        walk.limit = 40;
    }
    const std::size_t width = args.getSize("width", 8);
    const bool shortHash = args.flag("short");
    const bool withMetadata = !args.flag("no-metadata");

    if (ctx.showHeader) {
        printRepoHeader(ctx, "gitlense graph",
                        std::string("graf commitow, pokazujemy najwyzej ")
                            + plural(walk.limit, "commit", "commity", "commitow"));
    }

    CommitWalker walker(repo, walk, start);
    GraphRenderer renderer(repo, width);

    std::size_t count = 0;
    walker.forEach([&](const CommitInfo& commit) {
        const GraphRenderer::Row row = renderer.next(commit);
        std::cout << row.prefix;
        if (withMetadata) {
            std::cout << " " << Style::yellow().wrap(toShortHex(commit.id, shortHash ? 7 : 8));
            if (!commit.parents.empty() && commit.parents.size() > 1) {
                std::cout << Style::magenta().wrap(" (" + std::to_string(commit.parents.size())
                                                   + " rodzicow)");
            }
            std::cout << "  " << Style::dim().wrap(ctx.date(commit.author.time)) << "  "
                      << Style::cyan().wrap(ellipsize(commit.author.name, 18)) << "  "
                      << commit.subject;
        } else {
            std::cout << " " << Style::yellow().wrap(toShortHex(commit.id, shortHash ? 7 : 8))
                      << " " << commit.subject;
        }
        std::cout << "\n";
        if (!row.connector.empty()) {
            std::cout << row.connector << "\n";
        }
        ++count;
        return true;
    });

    if (!ctx.quiet) {
        std::cerr << Style::dim().wrap("\n" + plural(count, "commit", "commity", "commitow"))
                  << "\n";
    }
    return 0;
}

extern const Command graphCommand = {
    "graph",
    "graf commitow pokazujacy galezie i commity scalajace",
    R"(Uzycie: gitlense graph [opcje]

  -n, --limit N        ile commitow (domyslnie 40)
  --width KOL           ile kolumn grafu (domyslnie 8)
  --all                cale repozytorium, nie tylko HEAD
  --first-parent        bez galezi pobocznych
  --no-merges           bez commitow scalajacych
  --short              krotsze skroty commitow
  --no-metadata         sama gwiazdka, skrot i opis
  --since / --until / --author / --grep  filtry jak w `gitlense log`)",
    R"(  gitlense graph
  gitlense graph -n 100 --all
  gitlense graph --since=1.year --first-parent)",
    &runGraph,
};

} // namespace gitlense
