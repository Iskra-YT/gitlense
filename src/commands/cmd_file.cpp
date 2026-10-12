#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
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

/// Kolory per autor dla blame - ten sam autor zawsze ten sam kolor.
class BlamePalette {
public:
    Style styleFor(const std::string& author)
    {
        static const char* colors[] = {"36", "33", "32", "35", "34", "31",
                                       "96", "93", "92", "95", "94", "91"};
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

std::string statusMarker(ChangeStatus status)
{
    switch (status) {
    case ChangeStatus::Added:
        return Style::green().wrap("+");
    case ChangeStatus::Deleted:
        return Style::red().wrap("-");
    case ChangeStatus::Renamed:
        return Style::blue().wrap(">");
    case ChangeStatus::Copied:
        return Style::blue().wrap("^");
    default:
        return Style::yellow().wrap("~");
    }
}

} // namespace

int runFile(const CommandContext& ctx, const Arguments& args)
{
    args.rejectUnknown({"blame", "no-blame", "history", "lines", "no-lines", "author",
                        "since", "until", "author-search", "numstat", "follow", "header",
                        "language", "at"},
                       "gitlense <plik> --blame");

    if (args.positional().empty()) {
        throw UsageError(
            "gitlense file wymaga sciezki, np. `gitlense file src/main.cpp`");
    }
    const Repository& repo = ctx.repo;
    const std::string path = repo.normalizePath(args.positional().front());
    const Oid head = repo.headOrNull();
    if (isNull(head)) {
        throw Error("repozytorium nie ma jeszcze commitow");
    }
    Oid target = head;
    if (args.has("at")) {
        target = repo.revparse(args.get("at"));
    }

    const std::optional<TreeEntry> entry = repo.fileAt(target, path);
    if (!entry.has_value()) {
        throw Error("plik \"" + path + "\" nie istnieje w " + toShortHex(target));
    }

    const std::string language = ctx.detector.languageOf(path);
    if (ctx.showHeader) {
        std::string header = Style::bold().wrap(path);
        if (!language.empty()) {
            header += "  " + Style::magenta().wrap(language);
        }
        header += "  " + Style::dim().wrap(bytes(entry->size) + ", "
                                           + std::to_string(repo.lineCount(target, path))
                                           + " linii, " + toShortHex(target));
        std::cout << header << "\n\n";
    }

    // ------------------------------------------------------------------
    // Historia pliku
    // ------------------------------------------------------------------
    if (args.flag("history") || !args.flag("blame") || args.has("since") || args.has("until")) {
        WalkOptions walk;
        walk.pathPattern = path;
        if (args.has("since")) {
            walk.since = parseTimestamp(args.get("since"));
        }
        if (args.has("until")) {
            walk.until = parseTimestamp(args.get("until"), true);
        }
        if (args.flag("follow") && target == head) {
            // sledzenie przemowczen: najprostsza wersja - cala historia pliku
            walk.pathPattern = path;
        }
        if (args.has("author-search")) {
            walk.authorPattern = args.get("author-search");
        }
        const Oid start = args.flag("all") ? kNullOid : head;
        CommitWalker walker(repo, walk, start);

        Table table({{"COMMIT", Align::Left},
                     {"DATA", Align::Left},
                     {"WIEK", Align::Left},
                     {"AUTOR", Align::Left},
                     {"DODANE", Align::Right},
                     {"USUNIETE", Align::Right},
                     {"OPIS", Align::Left}});
        BlamePalette palette;
        walker.forEach([&](const CommitInfo& commit) {
            const Oid parent = commit.parents.empty() ? kNullOid : commit.parents.front();
            const DiffStats diff = repo.diff(parent, commit.id);
            const FileChange* mine = nullptr;
            for (const FileChange& file : diff.files) {
                if (file.path == path || file.oldPath == path) {
                    mine = &file;
                    break;
                }
            }
            if (mine == nullptr) {
                return true;
            }
            const std::string status = statusMarker(mine->status);
            table.addRow({Style::yellow().wrap(toShortHex(commit.id)),
                          ctx.date(commit.author.time),
                          formatDate(commit.author.time, DateStyle::Relative),
                          palette.styleFor(commit.author.name).wrap(commit.author.name),
                          status + Style::green().wrap(std::to_string(mine->additions)),
                          status + Style::red().wrap(std::to_string(mine->deletions)),
                          ellipsize(commit.subject, 50)});
            return true;
        });
        table.setCaption("Historia pliku ("
                         + plural(table.rows(), "zmiana", "zmiany", "zmian") + ")");
        table.print();
        if (args.flag("blame")) {
            std::cout << "\n";
        } else {
            return 0;
        }
    }

    if (args.flag("no-blame") || repo.isBare()) {
        return 0;
    }

    // ------------------------------------------------------------------
    // Blame
    // ------------------------------------------------------------------
    std::vector<BlameLine> lines = repo.blame(path, target);
    if (lines.empty()) {
        return 0;
    }

    // podsumowanie: kto jest "wlascicielem" najwiekszej liczby linii
    std::map<std::string, std::size_t> byAuthor;
    for (const BlameLine& line : lines) {
        byAuthor[line.author.name] += 1;
    }
    std::vector<std::pair<std::string, std::size_t>> ranking(byAuthor.begin(), byAuthor.end());
    std::sort(ranking.begin(), ranking.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    Table owners({{"AUTOR", Align::Left}, {"LINIE", Align::Right},
                  {"% PLIKU", Align::Right}, {"UDZIAL", Align::Left}});
    for (const auto& [author, count] : ranking) {
        owners.addRow({author,
                       number(count),
                       percent(static_cast<double>(count) / static_cast<double>(lines.size())),
                       progressBar(static_cast<double>(count) / static_cast<double>(lines.size()),
                                   16, Style::cyan(), Style::dim())});
    }
    owners.setFooter({"RAZEM", number(lines.size()), "100.0%", ""});
    owners.setCaption("Wlasciciele linii (blame " + toShortHex(target) + ")");
    owners.print();

    if (!args.flag("lines")) {
        return 0;
    }

    // ------------------------------------------------------------------
    // Tresc pliku z anotacja
    // ------------------------------------------------------------------
    BlamePalette palette;
    const std::vector<std::string> fileLines = repo.fileLines(target, path);
    Table content({{"NR", Align::Right},
                   {"COMMIT", Align::Left},
                   {"DATA", Align::Left},
                   {"AUTOR", Align::Left},
                   {"KOD", Align::Left}});
    content.setBorder(false);
    for (const BlameLine& line : lines) {
        const std::string text
            = line.lineNumber > 0 && line.lineNumber <= fileLines.size()
            ? fileLines[line.lineNumber - 1]
            : std::string();
        content.addRow({std::to_string(line.lineNumber),
                        Style::yellow().wrap(toShortHex(line.commit, 8)),
                        ctx.date(line.author.time),
                        palette.styleFor(line.author.name).wrap(
                            ellipsize(line.author.name, 18)),
                        ellipsize(text, ctx.tabWidth * 12)});
    }
    content.setCaption("Zawartosc pliku z blame");
    content.print();
    return 0;
}

extern const Command fileCommand = {
    "file",
    "przegladarka pojedynczego pliku: tresc, blame i historia zmian",
    R"(Uzycie: gitlense file <sciezka> [opcje]

      --blame         pokaz wlascicieli linii
      --lines         pokaz tresc pliku z anotacja blame
      --history       tylko historia zmian pliku
      --no-blame      pomin podsumowanie wlasicieli
      --since DATA / --until DATA
                     ogranicz historię pliku do okresu
      --author-search PAT
                     filtr autorow w historii pliku
      --all           historia z calego repozytorium
      --at REF        stan pliku w innym commicie (domyslnie HEAD))",
    R"(  gitlense file src/main.cpp
  gitlense file src/main.cpp --blame
  gitlense file src/main.cpp --blame --lines
  gitlense file CMakeLists.txt --history --since=1.year)",
    &runFile,
};

} // namespace gitlense
