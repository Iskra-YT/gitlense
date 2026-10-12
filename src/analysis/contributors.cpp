#include "analysis/contributors.h"

#include "core/error.h"
#include "core/parallel.h"
#include "format/format.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>

namespace gitlense {
namespace {

struct HistoryAccumulator {
    std::size_t commits{0};
    std::uint64_t additions{0};
    std::uint64_t deletions{0};
    std::set<std::string> files;
    std::set<Timestamp> days;
    Timestamp first{0};
    Timestamp last{0};
};

struct NameAndEmail {
    std::string name;
    std::string email;
};

std::string authorKey(const std::string& name, const std::string& email)
{
    return email.empty() ? name : email + " (" + name + ")";
}

NameAndEmail splitKey(const std::string& key)
{
    NameAndEmail result;
    const std::size_t paren = key.rfind(" (");
    if (paren != std::string::npos && key.back() == ')') {
        result.email = key.substr(0, paren);
        result.name = key.substr(paren + 2, key.size() - paren - 3);
    } else {
        result.name = key;
    }
    return result;
}

} // namespace

ContributorResult analyzeContributors(const Repository& repo, const LanguageDetector& detector,
                                      const ContributorOptions& options)
{
    ContributorResult result;

    const Oid start = options.walk.includeAllRefs ? kNullOid : repo.headOrNull();
    if (isNull(start) && !options.walk.includeAllRefs) {
        return result;
    }

    // ------------------------------------------------------------------
    // Historia: jedno przejscie po commitach z diffami
    // ------------------------------------------------------------------
    HistoryOptions historyOptions;
    historyOptions.walk = options.walk;
    HistoryScan scan(repo, historyOptions, detector);
    const std::vector<CommitChange> history = scan.collect(options.threads);

    std::map<std::string, HistoryAccumulator> aggregated;
    std::set<std::string> allFiles;

    for (const CommitChange& change : history) {
        const std::string key = authorKey(change.authorName, change.authorEmail);
        HistoryAccumulator& accumulator = aggregated[key];
        accumulator.commits += 1;
        accumulator.first = accumulator.first == 0
            ? change.time
            : std::min(accumulator.first, change.time);
        accumulator.last = std::max(accumulator.last, change.time);
        accumulator.days.insert(startOfDay(change.time));

        for (const FileChangeRecord& file : change.files) {
            if (!options.path.accepts(file.path) && !options.path.accepts(file.oldPath)) {
                continue;
            }
            if (!options.language.empty() && file.language != options.language) {
                continue;
            }
            accumulator.additions += file.additions;
            accumulator.deletions += file.deletions;
            accumulator.files.insert(file.path);
            allFiles.insert(file.path);
        }
    }

    result.commitsTotal = history.size();
    result.filesTotal = allFiles.size();
    result.authorsTotal = aggregated.size();

    for (const CommitChange& change : history) {
        result.additionsTotal += change.additions;
        result.deletionsTotal += change.deletions;
    }

    for (const auto& [key, value] : aggregated) {
        ContributorHistory item;
        const NameAndEmail identity = splitKey(key);
        item.name = identity.name;
        item.email = identity.email;
        item.commits = value.commits;
        item.additions = value.additions;
        item.deletions = value.deletions;
        item.filesTouched = value.files.size();
        item.activeDays = value.days.size();
        item.firstCommit = value.first;
        item.lastCommit = value.last;
        result.churnTotal += value.additions + value.deletions;
        result.history.push_back(std::move(item));
    }

    std::sort(result.history.begin(), result.history.end(),
              [](const ContributorHistory& a, const ContributorHistory& b) {
                  if (a.churn() != b.churn()) {
                      return a.churn() > b.churn();
                  }
                  return a.commits > b.commits;
              });

    // ------------------------------------------------------------------
    // Stan biezacy: pliki, linie, ostatnie zmiany
    // ------------------------------------------------------------------
    FactsOptions factsOptions;
    factsOptions.path = options.path;
    factsOptions.threads = options.threads;
    RepoFacts facts = collectRepoFacts(repo, detector, factsOptions);
    enrichWithHistory(facts, history);
    result.linesTotal = facts.totalLines;

    // ranking autorow wg liczby linii (dla --ownership-top)
    std::map<std::string, std::size_t> linesByAuthor;
    for (const FileFacts& file : facts.files) {
        if (file.lastAuthor.empty()) {
            continue;
        }
        linesByAuthor[file.lastAuthor] += file.lines;
    }
    std::vector<std::pair<std::string, std::size_t>> ranking(linesByAuthor.begin(),
                                                             linesByAuthor.end());
    std::sort(ranking.begin(), ranking.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    std::map<std::string, std::size_t> rankOf;
    for (std::size_t i = 0; i < ranking.size(); ++i) {
        rankOf[ranking[i].first] = i;
    }

    for (const auto& [key, value] : aggregated) {
        ContributorNow item;
        item.name = splitKey(key).name;
        item.commits = value.commits;
        item.additions = value.additions;
        item.deletions = value.deletions;
        item.filesTouched = value.files.size();
        item.activeDays = value.days.size();
        item.firstCommit = value.first;
        item.lastCommit = value.last;
        result.current.push_back(std::move(item));
    }

    // kto ostatnio dotykal ktorego pliku w HEAD
    std::unordered_map<std::string, std::pair<std::size_t, std::size_t>> touch; // autor -> pliki, linie
    std::unordered_map<std::string, Timestamp> lastTouch;
    for (const FileFacts& file : facts.files) {
        if (file.lastAuthor.empty()) {
            continue;
        }
        auto& entry = touch[file.lastAuthor];
        entry.first += 1;
        entry.second += file.lines;
        if (file.lastModified > 0) {
            Timestamp& slot = lastTouch[file.lastAuthor];
            slot = std::max(slot, file.lastModified);
        }
    }
    for (ContributorNow& item : result.current) {
        const auto it = touch.find(item.name);
        if (it != touch.end()) {
            item.lastTouchFiles = it->second.first;
            item.lastTouchLines = it->second.second;
        }
        const auto it2 = lastTouch.find(item.name);
        if (it2 != lastTouch.end()) {
            item.lastTouch = it2->second;
        }
    }

    // churn w ostatnich `--recent` dniach
    if (options.recentWindow > 0) {
        const Timestamp cutoff = now() - options.recentWindow;
        std::unordered_map<std::string, std::pair<std::size_t, std::uint64_t>> recent;
        for (const CommitChange& change : history) {
            if (change.time < cutoff) {
                continue;
            }
            auto& entry = recent[change.authorName];
            entry.first += 1;
            entry.second += change.churn();
        }
        for (ContributorNow& item : result.current) {
            const auto it = recent.find(item.name);
            if (it != recent.end()) {
                item.recentCommits = it->second.first;
                item.recentChurn = it->second.second;
            }
        }
    }

    // ------------------------------------------------------------------
    // Wlasnosc linii w HEAD (blame)
    // ------------------------------------------------------------------
    if (options.computeOwnership && !repo.isBare() && !facts.files.empty()) {
        const Oid targetCommit = facts.commit;
        const std::size_t totalLines = facts.totalLines;
        result.ownershipComputed = true;

        std::vector<const FileFacts*> targets;
        targets.reserve(facts.files.size());
        for (const FileFacts& file : facts.files) {
            if (options.ownershipTop != 0 && !file.lastAuthor.empty()
                && rankOf.count(file.lastAuthor) != 0
                && rankOf[file.lastAuthor] >= options.ownershipTop) {
                continue;
            }
            targets.push_back(&file);
        }

        std::mutex blameMutex;
        std::map<std::string, std::pair<std::size_t, std::size_t>> ownership; // autor -> pliki, linie
        std::atomic<std::size_t> blamed{0};
        std::atomic<std::size_t> failed{0};
        const std::filesystem::path repoPath = repo.path();

        parallelFor(targets.size(), options.threads, [&](std::size_t index) {
            static thread_local std::unique_ptr<Repository> threadRepo;
            if (threadRepo == nullptr) {
                threadRepo = std::make_unique<Repository>(repoPath);
            }
            const FileFacts& file = *targets[index];
            std::vector<BlameLine> lines;
            try {
                lines = threadRepo->blame(file.path, targetCommit);
            } catch (const Error&) {
                failed.fetch_add(1); // pojedynczy plik nie zerwie calego raportu
                return;
            }
            std::map<std::string, std::size_t> perAuthor;
            for (const BlameLine& line : lines) {
                perAuthor[line.author.name] += 1;
            }
            std::lock_guard<std::mutex> guard(blameMutex);
            for (const auto& [author, count] : perAuthor) {
                auto& entry = ownership[author];
                entry.first += 1;
                entry.second += count;
            }
            blamed.fetch_add(1);
        });

        if (blamed.load() > 0) {
            for (ContributorNow& item : result.current) {
                const auto it = ownership.find(item.name);
                if (it != ownership.end()) {
                    item.ownedFiles = it->second.first;
                    item.ownedLines = it->second.second;
                    item.totalLines = totalLines;
                    item.ownershipPercent = totalLines == 0
                        ? 0.0
                        : 100.0 * static_cast<double>(item.ownedLines)
                              / static_cast<double>(totalLines);
                }
            }
            // autorzy widoczni tylko w blame (np. commit spoza wybranego zakresu)
            for (const auto& [author, value] : ownership) {
                const bool known = std::any_of(result.current.begin(), result.current.end(),
                                               [&](const ContributorNow& item) {
                                                   return item.name == author;
                                               });
                if (known) {
                    continue;
                }
                ContributorNow item;
                item.name = author;
                item.ownedFiles = value.first;
                item.ownedLines = value.second;
                item.totalLines = totalLines;
                item.ownershipPercent = totalLines == 0
                    ? 0.0
                    : 100.0 * static_cast<double>(item.ownedLines)
                          / static_cast<double>(totalLines);
                result.current.push_back(item);
            }
            for (const ContributorNow& item : result.current) {
                result.filesOwnedTotal += item.ownedFiles;
            }
        }
    }

    std::sort(result.current.begin(), result.current.end(),
              [](const ContributorNow& a, const ContributorNow& b) {
                  if (a.ownedLines != b.ownedLines) {
                      return a.ownedLines > b.ownedLines;
                  }
                  if (a.churn() != b.churn()) {
                      return a.churn() > b.churn();
                  }
                  return a.name < b.name;
              });
    return result;
}

} // namespace gitlense
