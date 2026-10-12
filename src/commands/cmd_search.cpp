#include <algorithm>
#include <cctype>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "analysis/history.h"
#include "cli/progress.h"
#include "commands/command.h"
#include "core/error.h"
#include "core/parallel.h"
#include "core/walker.h"
#include "format/color.h"
#include "format/format.h"
#include "format/table.h"

namespace gitlense {
namespace {

std::string toLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

/// Proste wyszukiwanie: regex (opcjonalnie, --regex) albo podciag
/// bez rozroznienia wielkosci liter.
class Matcher {
public:
    Matcher(const std::string& pattern, bool caseSensitive, bool wholeWord)
        : pattern_(pattern), caseSensitive_(caseSensitive), wholeWord_(wholeWord)
    {
    }

    [[nodiscard]] bool matches(const std::string& text) const
    {
        if (pattern_.empty()) {
            return false;
        }
        const std::string haystack = caseSensitive_ ? text : lower(text);
        const std::string needle = caseSensitive_ ? pattern_ : lower(pattern_);

        if (wholeWord_) {
            std::size_t position = 0;
            while ((position = haystack.find(needle, position)) != std::string::npos) {
                const bool leftOk = position == 0
                    || std::isalnum(static_cast<unsigned char>(haystack[position - 1])) == 0;
                const std::size_t end = position + needle.size();
                const bool rightOk = end >= haystack.size()
                    || std::isalnum(static_cast<unsigned char>(haystack[end])) == 0;
                if (leftOk && rightOk) {
                    return true;
                }
                position = end;
            }
            return false;
        }
        return haystack.find(needle) != std::string::npos;
    }

    [[nodiscard]] const std::string& pattern() const { return pattern_; }

private:
    static std::string lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    std::string pattern_;
    bool caseSensitive_;
    bool wholeWord_;
};

void highlightLine(const std::string& text, const std::string& needle, bool caseSensitive)
{
    const std::string haystack = caseSensitive ? text : toLowerCopy(text);
    const std::string pattern = caseSensitive ? needle : toLowerCopy(needle);
    const std::size_t position = haystack.find(pattern);
    if (position == std::string::npos || pattern.empty()) {
        std::cout << ellipsize(text, 90);
        return;
    }
    const std::size_t head = std::min(position, static_cast<std::size_t>(12));
    std::cout << Style::dim().wrap(ellipsize(text.substr(0, head), 12)) << text.substr(head,
                                                                                   std::min(pattern.size(),
                                                                                       text.size() - head))
              << Style::bold().wrap(text.substr(head, pattern.size())) << " "
              << Style::dim().wrap(ellipsize(text.substr(head + pattern.size()), 60));
}

} // namespace

int runSearch(const CommandContext& ctx, const Arguments& args)
{
    args.rejectUnknown({"message", "author", "file", "code", "path", "case-sensitive",
                        "whole-word", "limit", "n", "since", "until", "all", "first-parent",
                        "reverse", "no-merges", "include", "exclude", "context", "header",
                        "language", "file-only", "grep", "committer"},
                       "gitlense search \"blame\" --file");

    std::string pattern;
    for (const std::string& value : args.positional()) {
        if (value == "-e") {
            continue;
        }
        if (!pattern.empty()) {
            pattern += " ";
        }
        pattern += value;
    }
    if (pattern.empty() && args.has("grep")) {
        pattern = args.get("grep");
    }
    if (pattern.empty()) {
        throw UsageError(
            "gitlense search wymaga wzorca, np. `gitlense search \"TODO\" --file`");
    }

    const bool caseSensitive = args.flag("case-sensitive") || args.flag("i");
    const bool wholeWord = args.flag("whole-word") || args.flag("w");
    const bool wantCode = args.flag("code") || args.flag("file");
    const bool wantMessages = args.flag("message") || !wantCode;
    const std::size_t limit = args.getSize("limit", args.getSize("n", 100));
    const std::size_t context = args.getSize("context", 0);
    const std::string language = args.get("language", "");

    const Repository& repo = ctx.repo;
    const Oid head = repo.headOrNull();
    if (isNull(head) && !args.flag("all")) {
        std::cout << "brak commitow\n";
        return 0;
    }

    const Matcher matcher(pattern, caseSensitive, wholeWord);
    std::size_t shown = 0;

    if (ctx.showHeader) {
        std::string what;
        if (wantMessages && wantCode) {
            what = "w commitach i w kodzie";
        } else if (wantMessages) {
            what = "w komunikatach commitow";
        } else {
            what = "w kodzie (HEAD)";
        }
        printRepoHeader(ctx, "gitlense search", "szukam \"" + pattern + "\" " + what);
    }

    // ------------------------------------------------------------------
    // 1. Komunikaty commitow
    // ------------------------------------------------------------------
    if (wantMessages) {
        WalkOptions walk;
        walk.limit = 0;
        CommitWalker walker(repo, walk, args.flag("all") ? kNullOid : head);

        Table table({{"COMMIT", Align::Left},
                     {"DATA", Align::Left},
                     {"AUTOR", Align::Left},
                     {"PLIKI", Align::Right},
                     {"+/-", Align::Right},
                     {"KOMUNIKAT", Align::Left}});
        walker.forEach([&](const CommitInfo& commit) {
            if (shown >= limit) {
                return false;
            }
            if (!matcher.matches(commit.message)) {
                return true;
            }
            const Oid parent = commit.parents.empty() ? kNullOid : commit.parents.front();
            const DiffStats diff = repo.diff(parent, commit.id);
            ++shown;
            std::string message = commit.subject;
            if (commit.message.size() > commit.subject.size()) {
                message += commit.message.substr(commit.subject.size());
            }
            while (!message.empty() && message.back() == '\n') {
                message.pop_back();
            }
            for (char& character : message) {
                if (character == '\n' || character == '\r') {
                    character = ' ';
                }
            }
            table.addRow({Style::yellow().wrap(toShortHex(commit.id)),
                          ctx.date(commit.author.time),
                          ellipsize(commit.author.name, 18),
                          std::to_string(diff.filesChanged),
                          Style::green().wrap("+" + number(diff.additions)) + " "
                              + Style::red().wrap("-" + number(diff.deletions)),
                          ellipsize(message, 56)});
            return true;
        });
        if (table.rows() > 0) {
            table.setCaption("KOMUNIKATY COMMITOW (" + std::to_string(table.rows()) + " trafien)");
            table.print();
        }
    }

    // ------------------------------------------------------------------
    // 2. Tresc plikow w HEAD
    // ------------------------------------------------------------------
    if (wantCode) {
        const std::vector<TreeEntry> entries = repo.tree(head);
        const PathFilter path = pathFilterFromArgs(args);
        std::vector<const TreeEntry*> candidates;
        for (const TreeEntry& entry : entries) {
            if (entry.directory || entry.submodule) {
                continue;
            }
            if (!ctx.detector.isTextual(entry.path)) {
                continue;
            }
            if (!language.empty() && ctx.detector.languageOf(entry.path) != language) {
                continue;
            }
            if (!path.accepts(entry.path)) {
                continue;
            }
            candidates.push_back(&entry);
        }

        struct Hit {
            std::string path;
            std::size_t lineNumber{0};
            std::string text;
            std::size_t matchColumn{0};
            std::size_t matchLength{0};
        };
        std::vector<Hit> hits;
        std::mutex mutex;

        Progress progress("przeszukuje pliki");
        progress.start(candidates.size());
        parallelFor(candidates.size(), ctx.threads, [&](std::size_t index) {
            const TreeEntry& entry = *candidates[index];
            std::vector<std::string> lines;
            try {
                lines = repo.fileLines(head, entry.path);
            } catch (const Error&) {
                progress.advance();
                return;
            }
            std::vector<Hit> local;
            for (std::size_t line = 0; line < lines.size(); ++line) {
                const std::string& text = lines[line];
                std::string haystack = caseSensitive ? text : toLowerCopy(text);
                const std::string needle = caseSensitive ? pattern : toLowerCopy(pattern);
                std::size_t position = 0;
                while ((position = haystack.find(needle, position)) != std::string::npos) {
                    Hit hit;
                    hit.path = entry.path;
                    hit.lineNumber = line + 1;
                    hit.text = text;
                    hit.matchColumn = position;
                    hit.matchLength = needle.size();
                    local.push_back(std::move(hit));
                    position += needle.empty() ? 1 : needle.size();
                }
            }
            if (!local.empty()) {
                std::lock_guard<std::mutex> guard(mutex);
                hits.insert(hits.end(), local.begin(), local.end());
            }
            progress.advance();
        });
        progress.done();

        std::sort(hits.begin(), hits.end(),
                  [](const Hit& a, const Hit& b) {
                      if (a.path != b.path) {
                          return a.path < b.path;
                      }
                      return a.lineNumber < b.lineNumber;
                  });

        if (!hits.empty()) {
            std::cout << "\n";
            Table table({{"PLIK", Align::Left},
                         {"LINIA", Align::Right},
                         {"KOD", Align::Left}});
            std::size_t count = 0;
            for (const Hit& hit : hits) {
                if (count++ >= limit) {
                    break;
                }
                const std::size_t head = std::min(hit.matchColumn, static_cast<std::size_t>(12));
                const std::size_t matchLength
                    = std::min(hit.matchLength, hit.text.size() - head);
                const std::string code
                    = Style::dim().wrap(ellipsize(hit.text.substr(0, head), 12))
                    + Style::bold().wrap(hit.text.substr(head, matchLength)) + " "
                    + Style::dim().wrap(ellipsize(hit.text.substr(head + matchLength), 60));
                table.addRow({shortenPath(ctx, hit.path), std::to_string(hit.lineNumber), code});
            }
            table.setCaption("KOD W HEAD (" + std::to_string(std::min(hits.size(), limit))
                             + " z " + std::to_string(hits.size()) + " trafien)");
            table.print();
        }
    }

    if (shown == 0) {
        std::cout << "brak trafien dla \"" << pattern << "\"\n";
    }
    return 0;
}

extern const Command searchCommand = {
    "search",
    "szukanie w komunikatach commitow i w kodzie",
    R"(Uzycie: gitlense search <wzorzec> [opcje]

  -m, --message       tylko komunikaty commitow
  -f, --file          tylko kod w HEAD (domyslnie szuka w obu)
  -c, --code          to samo co --file
  -i, --case-sensitive
                      rozroznij wielkosc liter
  -w, --whole-word    dopasuj cale slowa
  -n, --limit N       ile trafien pokazac (domyslnie 100)
  -C, --context N     ile linii kontekstu (w przygotowaniu)
  --language LANG     przeszukaj tylko pliki danego jezyka
  --path GLOB         ogranicz do sciezek
  --since / --until / --all
                      filtry historii dla komunikatow

  Wzorzec jest zwyklym tekstem; `--grep=-slowo` dla wzorca zaczynajacego
  sie od myslnika.)",
    R"(  gitlense search TODO --file
  gitlense search "fix crash" --message
  gitlense search FIXME --language C++ -i
  gitlense search handleAlloc --file --path src/)",
    &runSearch,
};

} // namespace gitlense
