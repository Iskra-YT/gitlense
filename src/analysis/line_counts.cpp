#include "analysis/line_counts.h"

#include "core/parallel.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <set>

namespace gitlense {
namespace {

struct FileAccumulator {
    std::string language;
    FileKind kind{FileKind::Other};
    std::size_t lines{0};
    std::uint64_t size{0};
};

void mergeInto(std::unordered_map<std::string, std::size_t>& target, const std::string& key,
               std::size_t value)
{
    if (key.empty()) {
        return;
    }
    target[key] += value;
}

} // namespace

RepoFacts collectRepoFacts(const Repository& repo, const LanguageDetector& detector,
                           const FactsOptions& options)
{
    const Oid commitId = isNull(options.commit) ? repo.head() : options.commit;
    const std::vector<TreeEntry> entries = repo.tree(commitId);

    std::vector<FileAccumulator> accumulators(entries.size());
    std::atomic<std::size_t> vendored{0};
    std::atomic<std::size_t> generated{0};
    std::atomic<std::size_t> binaries{0};

    const bool perThread = options.threads != 1;
    const std::filesystem::path repoPath = repo.path();
    std::vector<std::unique_ptr<Repository>> threadRepos;

    parallelFor(entries.size(), options.threads, [&](std::size_t index) {
        const TreeEntry& entry = entries[index];
        if (entry.directory || entry.submodule) {
            return;
        }
        if (!options.path.accepts(entry.path)) {
            return;
        }
        const FileKind kind = detector.kindOf(entry.path);
        if (kind == FileKind::Vendored) {
            vendored.fetch_add(1);
            if (!options.includeVendored) {
                return;
            }
        }
        if (kind == FileKind::Generated) {
            generated.fetch_add(1);
            if (!options.includeGenerated) {
                return;
            }
        }
        if (!detector.isTextual(entry.path)) {
            binaries.fetch_add(1);
        }

        std::unique_ptr<Repository> local;
        if (perThread) {
            local = std::make_unique<Repository>(repoPath);
        }
        const Repository& source = local ? *local : repo;

        FileAccumulator accumulator;
        accumulator.language = detector.languageOf(entry.path);
        accumulator.kind = kind;
        accumulator.size = source.blobSize(entry.id);
        if (detector.isTextual(entry.path) && !source.blobIsBinary(entry.id)) {
            accumulator.lines = source.blobLineCount(entry.id);
        }
        accumulators[index] = std::move(accumulator);
    });

    RepoFacts facts;
    facts.commit = commitId;
    facts.vendoredFiles = vendored.load();
    facts.generatedFiles = generated.load();
    facts.binaryFiles = binaries.load();
    facts.files.reserve(entries.size());

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const TreeEntry& entry = entries[i];
        if (entry.directory || entry.submodule) {
            continue;
        }
        if (!options.path.accepts(entry.path)) {
            continue;
        }
        const FileKind kind = detector.kindOf(entry.path);
        if (kind == FileKind::Vendored && !options.includeVendored) {
            continue;
        }
        if (kind == FileKind::Generated && !options.includeGenerated) {
            continue;
        }
        FileFacts file;
        file.path = entry.path;
        file.language = accumulators[i].language;
        file.kind = kind;
        file.lines = accumulators[i].lines;
        file.size = accumulators[i].size;
        facts.files.push_back(std::move(file));
        ++facts.totalFiles;
        facts.totalLines += file.lines;
        facts.totalBytes += file.size;
        mergeInto(facts.linesByLanguage, file.language, file.lines);
        mergeInto(facts.filesByLanguage, file.language, 1);
    }
    (void)threadRepos;
    return facts;
}

void enrichWithHistory(RepoFacts& facts, const std::vector<CommitChange>& history)
{
    std::unordered_map<std::string, std::size_t> index;
    index.reserve(facts.files.size());
    for (std::size_t i = 0; i < facts.files.size(); ++i) {
        index.emplace(facts.files[i].path, i);
    }

    struct Accumulator {
        std::uint64_t churn{0};
        std::uint64_t commits{0};
        std::set<std::string> authors;
        Timestamp last{0};
        std::string lastAuthor;
        Oid lastCommit{};
        Timestamp created{0};
    };
    std::unordered_map<std::string, Accumulator> perFile;
    perFile.reserve(facts.files.size());

    for (const CommitChange& commit : history) {
        for (const FileChangeRecord& change : commit.files) {
            const std::string key = change.path;
            const auto it = index.find(key);
            if (it == index.end()) {
                continue; // plik nie istnieje w HEAD
            }
            Accumulator& accumulator = perFile[key];
            accumulator.churn += change.churn();
            accumulator.commits += 1;
            accumulator.authors.insert(commit.authorName);
            if (commit.time > accumulator.last) {
                accumulator.last = commit.time;
                accumulator.lastAuthor = commit.authorName;
                accumulator.lastCommit = commit.id;
            }
            if (accumulator.created == 0 || commit.time < accumulator.created) {
                accumulator.created = commit.time;
            }
        }
    }

    for (FileFacts& file : facts.files) {
        const auto it = perFile.find(file.path);
        if (it == perFile.end()) {
            continue;
        }
        file.churn = it->second.churn;
        file.commitCount = it->second.commits;
        file.authors = it->second.authors.size();
        file.lastModified = it->second.last;
        file.lastAuthor = it->second.lastAuthor;
        file.lastCommit = it->second.lastCommit;
        file.created = it->second.created;
    }
}

std::vector<LanguageFact> languagesOf(const RepoFacts& facts)
{
    std::map<std::string, LanguageFact> merged;
    for (const FileFacts& file : facts.files) {
        if (file.language.empty()) {
            continue;
        }
        LanguageFact& fact = merged[file.language];
        fact.language = file.language;
        fact.files += 1;
        fact.lines += file.lines;
        fact.bytes += file.size;
    }
    std::vector<LanguageFact> result;
    result.reserve(merged.size());
    for (auto& [language, fact] : merged) {
        (void)language;
        result.push_back(fact);
    }
    return result;
}

} // namespace gitlense
