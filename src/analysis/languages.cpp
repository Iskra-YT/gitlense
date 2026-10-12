#include "analysis/languages.h"

#include "core/parallel.h"

#include <algorithm>
#include <map>
#include <set>

namespace gitlense {
namespace {

struct LanguageAccumulator {
    std::uint64_t additions{0};
    std::uint64_t deletions{0};
    std::set<std::string> files;
    std::set<std::string> authors;
    std::set<Oid> commits;
    Timestamp first{0};
    Timestamp last{0};
    std::map<std::string, std::uint64_t> churnByAuthor;
    /// commity autora w danym jezyku (unikalne, liczone raz na commit)
    std::map<std::string, std::set<Oid>> commitsByAuthor;
};

} // namespace

LanguageResult analyzeLanguages(const Repository& repo, const LanguageDetector& detector,
                                const LanguageOptions& options)
{
    LanguageResult result;

    const Oid start = options.walk.includeAllRefs ? kNullOid : repo.headOrNull();
    if (isNull(start) && !options.walk.includeAllRefs) {
        return result;
    }

    HistoryOptions historyOptions;
    historyOptions.walk = options.walk;
    HistoryScan scan(repo, historyOptions, detector);
    const std::vector<CommitChange> history = scan.collect(options.threads);

    std::map<std::string, LanguageAccumulator> byLanguage;
    std::map<std::string, AuthorLanguageRow> byAuthor;

    for (const CommitChange& change : history) {
        std::set<std::string> languagesInCommit;
        for (const FileChangeRecord& file : change.files) {
            if (file.language.empty()) {
                continue;
            }
            if (!options.path.accepts(file.path) && !options.path.accepts(file.oldPath)) {
                continue;
            }
            LanguageAccumulator& language = byLanguage[file.language];
            language.additions += file.additions;
            language.deletions += file.deletions;
            language.files.insert(file.path);
            language.authors.insert(change.authorName);
            language.commits.insert(change.id);
            language.first = language.first == 0
                ? change.time
                : std::min(language.first, change.time);
            language.last = std::max(language.last, change.time);
            language.churnByAuthor[change.authorName] += file.churn();
            language.commitsByAuthor[change.authorName].insert(change.id);
            languagesInCommit.insert(file.language);

            AuthorLanguageRow& row = byAuthor[change.authorName];
            row.author = change.authorName;
            row.additions += file.additions;
            row.deletions += file.deletions;
            row.churn += file.churn();
            row.churnByLanguage[file.language] += file.churn();
            row.filesByLanguage[file.language] += 1;
        }
        AuthorLanguageRow& row = byAuthor[change.authorName];
        row.author = change.authorName;
        row.commits += 1;
        for (const std::string& language : languagesInCommit) {
            row.commitsByLanguage[language] += 1;
        }
    }

    for (const auto& [language, value] : byLanguage) {
        result.churnTotal += value.additions + value.deletions;
    }

    for (const auto& [language, value] : byLanguage) {
        LanguageHistory item;
        item.language = language;
        item.additions = value.additions;
        item.deletions = value.deletions;
        item.filesTouched = value.files.size();
        item.authors = value.authors.size();
        item.commits = value.commits.size();
        item.firstCommit = value.first;
        item.lastCommit = value.last;
        for (const auto& [author, churn] : value.churnByAuthor) {
            item.topAuthors.emplace_back(author, churn);
        }
        result.history.push_back(std::move(item));
        result.historicalLanguages.push_back(language);
    }

    for (LanguageHistory& item : result.history) {
        item.churnShare = result.churnTotal == 0
            ? 0.0
            : static_cast<double>(item.churn()) / static_cast<double>(result.churnTotal);
        std::sort(item.topAuthors.begin(), item.topAuthors.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        if (options.topAuthorsPerLanguage != 0
            && item.topAuthors.size() > options.topAuthorsPerLanguage) {
            item.topAuthors.resize(options.topAuthorsPerLanguage);
        }
    }

    std::sort(result.history.begin(), result.history.end(),
              [](const LanguageHistory& a, const LanguageHistory& b) {
                  if (a.churn() != b.churn()) {
                      return a.churn() > b.churn();
                  }
                  return a.language < b.language;
              });

    if (options.topN != 0 && result.history.size() > options.topN) {
        result.history.resize(options.topN);
    }
    result.languagesTotal = byLanguage.size();

    for (auto& [author, row] : byAuthor) {
        (void)author;
        result.authors.push_back(std::move(row));
    }
    std::sort(result.authors.begin(), result.authors.end(),
              [](const AuthorLanguageRow& a, const AuthorLanguageRow& b) {
                  if (a.churn != b.churn) {
                      return a.churn > b.churn;
                  }
                  return a.author < b.author;
              });

    // stan biezacy
    FactsOptions factsOptions;
    factsOptions.path = options.path;
    factsOptions.threads = options.threads;
    const RepoFacts facts = collectRepoFacts(repo, detector, factsOptions);
    result.current = languagesOf(facts);
    for (const LanguageFact& fact : result.current) {
        result.currentLanguages.push_back(fact.language);
    }
    std::sort(result.current.begin(), result.current.end(),
              [](const LanguageFact& a, const LanguageFact& b) {
                  if (a.lines != b.lines) {
                      return a.lines > b.lines;
                  }
                  return a.language < b.language;
              });

    return result;
}

} // namespace gitlense
