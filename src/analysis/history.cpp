#include "analysis/history.h"

#include "core/error.h"
#include "core/parallel.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <memory>
#include <mutex>
#include <regex>

namespace gitlense {
namespace {

std::regex globToRegex(const std::string& glob)
{
    std::string pattern = "^";
    for (const char c : glob) {
        switch (c) {
        case '*':
            pattern += ".*";
            break;
        case '?':
            pattern += ".";
            break;
        default:
            if (std::string(".+()[]{}|^$\\").find(c) != std::string::npos) {
                pattern.push_back('\\');
            }
            pattern.push_back(c);
            break;
        }
    }
    pattern += "$";
    return std::regex(pattern, std::regex::ECMAScript);
}

} // namespace

PathFilter::PathFilter(std::vector<std::string> include, std::vector<std::string> exclude)
    : include_(std::move(include)), exclude_(std::move(exclude))
{
    // normalizacja: bez wiodacych "./" i z koncowym "/"
    auto clean = [](std::string& path) {
        while (path.rfind("./", 0) == 0) {
            path = path.substr(2);
        }
        if (path.size() > 1 && path.back() == '/') {
            path.pop_back();
        }
    };
    for (std::string& path : include_) {
        clean(path);
    }
    for (std::string& path : exclude_) {
        clean(path);
    }
}

bool PathFilter::accepts(const std::string& path) const
{
    if (!include_.empty()) {
        bool matched = false;
        for (const std::string& pattern : include_) {
            if (path == pattern || path.rfind(pattern + "/", 0) == 0
                || std::regex_match(path, globToRegex(pattern))) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            return false;
        }
    }
    if (!exclude_.empty()) {
        for (const std::string& pattern : exclude_) {
            if (path == pattern || path.rfind(pattern + "/", 0) == 0
                || std::regex_match(path, globToRegex(pattern))) {
                return false;
            }
        }
    }
    return true;
}

HistoryScan::HistoryScan(const Repository& repo, HistoryOptions options,
                         const LanguageDetector& detector)
    : repo_(repo), options_(std::move(options)), detector_(detector)
{
}

std::vector<CommitInfo> HistoryScan::commits() const
{
    const Oid start = options_.walk.includeAllRefs ? kNullOid : repo_.headOrNull();
    if (isNull(start) && !options_.walk.includeAllRefs) {
        return {};
    }
    CommitWalker walker(repo_, options_.walk, start);
    std::vector<CommitInfo> result;
    walker.forEach([&result](const CommitInfo& info) {
        result.push_back(info);
        return true;
    });
    return result;
}

void HistoryScan::forEach(Callback callback, std::size_t threads) const
{
    const std::vector<CommitInfo> commitList = commits();
    commitCount_ = commitList.size();
    if (commitList.empty()) {
        return;
    }

    std::mutex counterMutex;
    std::atomic<std::size_t> mergeCounter{0};
    std::atomic<std::size_t> skippedCounter{0};

    // Kazdy watek dostaje wlasne repozytorium - libgit2 nie lubi
    // dzielenia uchwytow między watki.
    const std::filesystem::path repoPath = repo_.path();
    const bool perThread = threads != 1;

    parallelFor(commitList.size(), threads, [&](std::size_t index) {
        std::unique_ptr<Repository> threadRepo;
        if (perThread) {
            threadRepo = std::make_unique<Repository>(repoPath);
        }
        const Repository& source = threadRepo ? *threadRepo : repo_;

        const CommitInfo& info = commitList[index];
        const Oid parent = info.parents.empty() ? kNullOid : info.parents.front();

        CommitChange change;
        change.id = info.id;
        change.time = info.author.time;
        change.authorName = info.author.name;
        change.authorEmail = info.author.email;
        change.subject = info.subject;
        change.merge = info.parents.size() > 1;
        change.root = info.parents.empty();
        change.parentCount = info.parents.size();
        if (change.merge) {
            mergeCounter.fetch_add(1);
        }

        DiffStats diff = source.diff(parent, info.id);
        for (const FileChange& file : diff.files) {
            if (options_.maxFilesPerCommit != 0 && change.files.size() >= options_.maxFilesPerCommit) {
                skippedCounter.fetch_add(1);
                continue;
            }
            const std::string& path = file.path.empty() ? file.oldPath : file.path;
            const FileKind kind = detector_.kindOf(path);
            if (options_.skipVendored && kind == FileKind::Vendored) {
                skippedCounter.fetch_add(1);
                continue;
            }
            if (options_.skipGenerated && kind == FileKind::Generated) {
                skippedCounter.fetch_add(1);
                continue;
            }
            if (options_.skipBinary && file.binary) {
                skippedCounter.fetch_add(1);
                continue;
            }

            FileChangeRecord record;
            record.path = path;
            record.oldPath = file.oldPath.empty() ? path : file.oldPath;
            record.status = file.status;
            record.additions = file.additions;
            record.deletions = file.deletions;
            record.binary = file.binary;
            record.newSize = file.newSize;
            record.kind = kind;
            record.language = detector_.languageOf(record.path);
            if (record.language.empty() && record.oldPath != record.path) {
                record.language = detector_.languageOf(record.oldPath);
            }
            change.additions += record.additions;
            change.deletions += record.deletions;
            change.files.push_back(std::move(record));
        }
        callback(change);
    });

    mergeCount_ = mergeCounter.load();
    skippedFiles_ = skippedCounter.load();
    (void)counterMutex;
}

std::vector<CommitChange> HistoryScan::collect(std::size_t threads) const
{
    std::mutex mutex;
    std::vector<CommitChange> result;
    result.reserve(commitCount_);
    forEach(
        [&](const CommitChange& change) {
            std::lock_guard<std::mutex> lock(mutex);
            result.push_back(change);
        },
        threads);
    return result;
}

} // namespace gitlense
