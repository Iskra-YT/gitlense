#include "core/walker.h"

#include "core/error.h"
#include "core/libgit2.h"

#include <git2.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <regex>

namespace gitlense {

using git::fromGitOid;
using git::toGitOid;

namespace {

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool containsInsensitive(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) {
        return true;
    }
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

/// Prosty glob -> regex: * = dowolny ciag (takze /), ? = jeden znak.
std::regex globToRegex(const std::string& glob)
{
    std::string pattern;
    pattern.reserve(glob.size() * 2 + 2);
    pattern += "^";
    for (const char c : glob) {
        switch (c) {
        case '*':
            pattern += ".*";
            break;
        case '?':
            pattern += ".";
            break;
        case '.':
            pattern += "\\.";
            break;
        case '+':
            pattern += "\\+";
            break;
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
        case '^':
        case '$':
        case '|':
        case '\\':
            pattern.push_back('\\');
            pattern.push_back(c);
            break;
        default:
            pattern.push_back(c);
            break;
        }
    }
    pattern += "$";
    return std::regex(pattern, std::regex::ECMAScript);
}

bool matchesAnyGlob(const std::string& path, const std::vector<std::regex>& globs)
{
    for (const std::regex& glob : globs) {
        if (std::regex_match(path, glob)) {
            return true;
        }
    }
    return false;
}

} // namespace

bool commitTouchesPaths(const Repository& repo, const CommitInfo& commit,
                        const std::vector<std::string>& patterns)
{
    if (patterns.empty()) {
        return true;
    }
    std::vector<std::regex> globs;
    globs.reserve(patterns.size());
    for (const std::string& pattern : patterns) {
        globs.push_back(globToRegex(pattern));
    }
    const DiffStats stats = repo.diff(
        commit.parents.empty() ? kNullOid : commit.parents.front(), commit.id);
    for (const FileChange& change : stats.files) {
        if (matchesAnyGlob(change.path, globs) || matchesAnyGlob(change.oldPath, globs)) {
            return true;
        }
    }
    return false;
}

namespace {

/// Commit przechodzi filtr `--path`, gdy dotyka chociaz jednego pliku
/// pasujacego do wzorca. Wykluczenia (`--exclude`) obsluguje osobno
/// `PathFilter` w analizach statystycznych - tutaj nie chcemy rezygnowac
/// z commitow, ktore miesza pliki dozwolone i wykluczone.
bool matchesPathFilters(const Repository& repo, const CommitInfo& commit,
                        const std::vector<std::regex>& include)
{
    if (include.empty()) {
        return true;
    }
    const DiffStats stats = repo.diff(
        commit.parents.empty() ? kNullOid : commit.parents.front(), commit.id);
    for (const FileChange& change : stats.files) {
        for (const std::regex& glob : include) {
            if (matchesAnyGlob(change.path, {glob}) || matchesAnyGlob(change.oldPath, {glob})) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

CommitWalker::CommitWalker(const Repository& repo, WalkOptions options, Oid start)
    : repo_(repo), options_(std::move(options)), start_(start)
{
}

std::size_t CommitWalker::forEach(Callback onCommit) const
{
    git::RevwalkPtr walk;
    if (git_revwalk_new(walk.out(), repo_.handle().get()) != GIT_OK || !walk) {
        throw Error("nie moge utworzyc spacerki po commitach");
    }

    // libgit2 nie ma filtrow po dacie ani po tresci wiadomosci w spacerce,
    // wiec oba warunki sprawdzamy recznie w petli ponizej.
    git::check(git_revwalk_sorting(walk.get(), options_.reverse ? GIT_SORT_REVERSE : GIT_SORT_NONE));
    if (options_.firstParentOnly || !options_.includeMergeCommits) {
        git::check(git_revwalk_simplify_first_parent(walk.get()));
    }

    if (options_.includeAllRefs) {
        // git_revwalk_push_glob: wszystkie referencje.
        git::check(git_revwalk_push_glob(walk.get(), "refs/*"));
    }
    if (isNull(start_)) {
        if (!options_.includeAllRefs) {
            return 0;
        }
    } else {
        const git_oid start = toGitOid(start_);
        git::check(git_revwalk_push(walk.get(), &start));
    }

    std::vector<std::regex> pathGlobs;
    if (!options_.pathPattern.empty()) {
        pathGlobs.push_back(globToRegex(options_.pathPattern));
    }


    std::optional<std::regex> messageRegex;
    if (!options_.messagePattern.empty()) {
        try {
            messageRegex.emplace(options_.messagePattern,
                                 std::regex::ECMAScript | std::regex::icase
                                     | std::regex::nosubs | std::regex::optimize);
        } catch (const std::regex_error&) {
            throw UsageError("bledny wzorzec --grep: " + options_.messagePattern);
        }
    }

    std::size_t processed = 0;
    git_oid oid{};
    while (git_revwalk_next(&oid, walk.get()) == GIT_OK) {
        if (options_.limit != 0 && processed >= options_.limit) {
            break;
        }
        const CommitInfo info = repo_.commit(fromGitOid(oid));

        // spacerska jest posortowana topologicznie, wiec po wejsciu w
        // okres starszy niz `since` mozemy przerwac.
        if (options_.since.has_value() && info.author.time < *options_.since) {
            break;
        }
        if (options_.until.has_value() && info.author.time > *options_.until) {
            continue;
        }
        if (!options_.authorPattern.empty()
            && !containsInsensitive(info.author.name, options_.authorPattern)
            && !containsInsensitive(info.author.email, options_.authorPattern)) {
            continue;
        }
        if (!options_.committerPattern.empty()
            && !containsInsensitive(info.committer.name, options_.committerPattern)
            && !containsInsensitive(info.committer.email, options_.committerPattern)) {
            continue;
        }
        if (messageRegex.has_value() && !std::regex_search(info.message, *messageRegex)) {
            continue;
        }
        if (!pathGlobs.empty() && !matchesPathFilters(repo_, info, pathGlobs)) {
            continue;
        }

        ++processed;
        if (!onCommit(info)) {
            break;
        }
    }
    return processed;
}

std::vector<Oid> CommitWalker::collect(std::size_t max) const
{
    WalkOptions options = options_;
    if (max != 0 && (options.limit == 0 || options.limit > max)) {
        options.limit = max;
    }
    std::vector<Oid> result;
    CommitWalker walker(repo_, options, start_);
    walker.forEach([&result](const CommitInfo& info) {
        result.push_back(info.id);
        return true;
    });
    return result;
}

} // namespace gitlense
