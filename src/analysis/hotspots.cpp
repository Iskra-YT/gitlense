#include "analysis/hotspots.h"

#include "core/error.h"
#include "format/format.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <unordered_map>

namespace gitlense {
namespace {

// Okres poltrwania wagi czasowej zmian: 180 dni. Plik nietkniety od pol roku
// jest uznawany za mniej ryzykowny.
constexpr double kHalfLifeSeconds = 180.0 * 86400.0;

} // namespace

HotspotResult analyzeHotspots(const Repository& repo, const LanguageDetector& detector,
                              const HotspotOptions& options)
{
    HotspotResult result;

    const Oid start = options.walk.includeAllRefs ? kNullOid : repo.headOrNull();
    if (isNull(start) && !options.walk.includeAllRefs) {
        return result;
    }

    HistoryOptions historyOptions;
    historyOptions.walk = options.walk;
    HistoryScan scan(repo, historyOptions, detector);
    const std::vector<CommitChange> history = scan.collect(options.threads);

    // stan biezacy
    FactsOptions factsOptions;
    factsOptions.path = options.path;
    factsOptions.threads = options.threads;
    RepoFacts facts = collectRepoFacts(repo, detector, factsOptions);
    enrichWithHistory(facts, history);

    std::unordered_map<std::string, const FileFacts*> byPath;
    byPath.reserve(facts.files.size());
    for (const FileFacts& file : facts.files) {
        byPath.emplace(file.path, &file);
    }

    struct Accumulator {
        std::uint64_t churn{0};
        std::size_t commits{0};
        std::set<std::string> authors;
        Timestamp last{0};
        double decayedChurn{0.0};
    };
    std::unordered_map<std::string, Accumulator> perFile;
    std::map<std::string, HotspotAuthor> perAuthor;
    const Timestamp reference = now();

    for (const CommitChange& change : history) {
        const double age = std::max(0.0, static_cast<double>(reference - change.time));
        const double decay = std::pow(0.5, age / kHalfLifeSeconds);
        for (const FileChangeRecord& file : change.files) {
            if (!options.path.accepts(file.path)) {
                continue;
            }
            if (!options.language.empty() && file.language != options.language) {
                continue;
            }
            Accumulator& accumulator = perFile[file.path];
            accumulator.churn += file.churn();
            accumulator.decayedChurn += static_cast<double>(file.churn()) * decay;
            accumulator.authors.insert(change.authorName);
            accumulator.last = std::max(accumulator.last, change.time);
            if (file.path == file.oldPath) {
                accumulator.commits += 1;
            }

            HotspotAuthor& author = perAuthor[change.authorName];
            author.author = change.authorName;
            author.commits += 1;
            author.churn += file.churn();
        }
    }

    // pliki, ktorych juz nie ma (usuniete) - pokazujemy tylko gdy --deleted
    double maxScore = 0.0;
    for (const auto& [path, accumulator] : perFile) {
        const auto it = byPath.find(path);
        if (it == byPath.end()) {
            continue;
        }
        const FileFacts& file = *it->second;
        HotspotFile item;
        item.path = file.path;
        item.language = file.language;
        item.lines = file.lines;
        item.churn = accumulator.churn;
        item.commits = accumulator.commits;
        item.authors = accumulator.authors.size();
        item.lastModified = accumulator.last;
        item.lastAuthor = file.lastAuthor;
        item.churnPerLine = file.lines == 0 ? 0.0
                                            : static_cast<double>(item.churn)
                                                  / static_cast<double>(file.lines);
        // wynik: churn ważony czasem + udzial autorow
        const double authorFactor = 1.0 + 0.25 * static_cast<double>(item.authors);
        item.score = accumulator.decayedChurn * authorFactor;
        maxScore = std::max(maxScore, item.score);
        result.churnTotal += item.churn;
        result.filesAnalyzed += 1;
        result.linesAnalyzed += file.lines;
        result.files.push_back(std::move(item));
    }

    for (HotspotFile& item : result.files) {
        item.score = maxScore == 0 ? 0.0 : item.score / static_cast<double>(maxScore);
    }

    std::sort(result.files.begin(), result.files.end(),
              [](const HotspotFile& a, const HotspotFile& b) {
                  if (a.churn != b.churn) {
                      return a.churn > b.churn;
                  }
                  return a.path < b.path;
              });

    // udzial autorow w ruchu
    for (auto& [name, author] : perAuthor) {
        (void)name;
        author.churnShare = result.churnTotal == 0
            ? 0.0
            : 100.0 * static_cast<double>(author.churn)
                  / static_cast<double>(result.churnTotal);
        result.authors.push_back(std::move(author));
    }
    std::sort(result.authors.begin(), result.authors.end(),
              [](const HotspotAuthor& a, const HotspotAuthor& b) {
                  return a.churn > b.churn;
              });

    if (options.topN != 0 && result.files.size() > options.topN) {
        result.files.resize(options.topN);
    }

    // "wiek" repozytorium w sensie sredniej szybkosci zmian
    if (!history.empty()) {
        const Timestamp first = history.back().time;
        const Timestamp last = history.front().time;
        const double years = static_cast<double>(last - first) / (365.0 * 86400.0);
        if (years > 0) {
            result.churnHalfLifeYears = static_cast<double>(result.churnTotal)
                / (static_cast<double>(history.size()) * years);
        }
    }
    return result;
}

} // namespace gitlense
