#include "core/repository.h"

#include "core/error.h"
#include "core/libgit2.h"

#include <git2.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace gitlense {
namespace {

std::string strdup_(const char* s)
{
    return s ? std::string(s) : std::string();
}

Identity identityOf(const git_signature* sig)
{
    Identity id;
    if (!sig) {
        return id;
    }
    id.name = strdup_(sig->name);
    id.email = strdup_(sig->email);
    id.time = static_cast<Timestamp>(sig->when.time);
    char sign = sig->when.sign ? sig->when.sign : '+';
    // offset jest w minutach, a sygnet "+0200" to godziny:minuty
    char buffer[16];
    const int offsetMinutes = sig->when.offset;
    std::snprintf(buffer, sizeof buffer, "%c%02d%02d", sign,
                  (offsetMinutes < 0 ? -offsetMinutes : offsetMinutes) / 60,
                  (offsetMinutes < 0 ? -offsetMinutes : offsetMinutes) % 60);
    id.timezone = buffer;
    return id;
}

using git::fromGitOid;
using git::toGitOid;

std::string branchRefName(const std::string& refName)
{
    constexpr std::string_view kHeads = "refs/heads/";
    constexpr std::string_view kRemotes = "refs/remotes/";
    if (refName.rfind(kHeads, 0) == 0) {
        return refName.substr(kHeads.size());
    }
    if (refName.rfind(kRemotes, 0) == 0) {
        return refName.substr(kRemotes.size());
    }
    return refName;
}

std::string stripTrailingSlash(std::string path)
{
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

void collectTreeEntries(git_repository* repo, git_tree* tree, const std::string& prefix, bool recurse,
                        bool resolveSizes, std::vector<TreeEntry>& out)
{
    const std::size_t count = git_tree_entrycount(tree);
    for (std::size_t i = 0; i < count; ++i) {
        const git_tree_entry* entry = git_tree_entry_byindex(tree, i);
        const char* name = git_tree_entry_name(entry);
        const git_oid* id = git_tree_entry_id(entry);
        const git_filemode_t mode = git_tree_entry_filemode(entry);

        TreeEntry item;
        item.path = prefix.empty() ? std::string(name) : prefix + "/" + name;
        item.id = fromGitOid(*id);
        char modeBuffer[8];
        std::snprintf(modeBuffer, sizeof modeBuffer, "%06o", static_cast<unsigned>(mode));
        item.mode = modeBuffer;

        const git_object_t type = git_tree_entry_type(entry);
        item.directory = (type == GIT_OBJECT_TREE);
        item.submodule = (type == GIT_OBJECT_COMMIT);

        if (item.directory && recurse) {
            git::TreePtr sub;
            if (git_tree_lookup(sub.out(), repo, id) == GIT_OK && sub) {
                collectTreeEntries(repo, sub.get(), item.path, true, resolveSizes, out);
            }
            continue;
        }
        if (resolveSizes && !item.directory && item.size == 0 && type == GIT_OBJECT_BLOB) {
            // Rozmiar jest znany dopiero po spojrzeniu z blobem; domyślnie
            // oszczędzamy I/O, a rozmiar doliczamy tylko gdy jest potrzebny.
            git::BlobPtr blob;
            if (git_blob_lookup(blob.out(), repo, id) == GIT_OK && blob) {
                item.size = static_cast<std::uint64_t>(git_blob_rawsize(blob.get()));
            }
        }
        out.push_back(std::move(item));
    }
}

std::optional<git_oid> treeOidOfCommit(git_repository* repo, const Oid& commitId)
{
    const git_oid id = toGitOid(commitId);
    git::CommitPtr commit;
    if (git_commit_lookup(commit.out(), repo, &id) != GIT_OK || !commit) {
        return std::nullopt;
    }
    return *git_commit_tree_id(commit.get());
}

/// Wpis drzewa znaleziony po sciezce.
struct FoundEntry {
    Oid id{};
    git_object_t type{GIT_OBJECT_BLOB};
};

/// Szuka wpisu w drzewie po pelnej sciezce. `git_tree_entry_bypath`
/// allokuje nowy obiekt, wiec trzeba go zwolnic - dlatego kopiujemy
/// tylko to, czego potrzebujemy.
std::optional<FoundEntry> findTreeEntry(git_tree* tree, const std::string& path)
{
    git::TreeEntryPtr entry;
    if (git_tree_entry_bypath(entry.out(), tree, path.c_str()) != GIT_OK || !entry) {
        return std::nullopt;
    }
    return FoundEntry{fromGitOid(*git_tree_entry_id(entry.get())), git_tree_entry_type(entry.get())};
}

/// Rozwiązuje ref (także symboliczny) do commita.
std::optional<Oid> commitOfReference(git_repository* repo, const git_reference* ref)
{
    static_cast<void>(repo);
    if (git_reference_type(ref) == GIT_REFERENCE_DIRECT) {
        const git_oid* target = git_reference_target(ref);
        if (target != nullptr) {
            return fromGitOid(*target);
        }
    }
    git::ObjectPtr obj;
    if (git_reference_peel(obj.out(), ref, GIT_OBJECT_COMMIT) == GIT_OK && obj) {
        return fromGitOid(*git_object_id(obj.get()));
    }
    return std::nullopt;
}

} // namespace

// ---------------------------------------------------------------------------
// Konstrukcja
// ---------------------------------------------------------------------------

Repository::Repository(std::filesystem::path path) : sourcePath_(std::move(path))
{
    init();
}

Repository::~Repository() = default;
Repository::Repository(Repository&&) noexcept = default;
Repository& Repository::operator=(Repository&&) noexcept = default;

void Repository::init()
{
    handle_ = git::RepositoryHandle(sourcePath_);

    location_.workdir = stripTrailingSlash(strdup_(git_repository_workdir(handle_.get())));
    location_.gitdir = stripTrailingSlash(strdup_(git_repository_path(handle_.get())));
    location_.bare = git_repository_is_bare(handle_.get()) != 0;
    if (location_.workdir.empty()) {
        location_.workdir = location_.gitdir;
    }
}

Repository& defaultRepository(const std::filesystem::path& path)
{
    static std::mutex mutex;
    static std::unordered_map<std::string, std::unique_ptr<Repository>> cache;
    const std::string key = std::filesystem::absolute(path).lexically_normal().string();

    std::lock_guard<std::mutex> lock(mutex);
    auto it = cache.find(key);
    if (it == cache.end()) {
        auto repo = std::make_unique<Repository>(key);
        it = cache.emplace(key, std::move(repo)).first;
    }
    return *it->second;
}

// ---------------------------------------------------------------------------
// Refy
// ---------------------------------------------------------------------------

bool Repository::hasHead() const
{
    git::ReferencePtr ref;
    if (git_repository_head(ref.out(), handle_.get()) == GIT_OK && ref) {
        return true;
    }
    return git_repository_head_detached(handle_.get()) == 1;
}

Oid Repository::headOrNull() const
{
    git::ReferencePtr ref;
    if (git_repository_head(ref.out(), handle_.get()) != GIT_OK || !ref) {
        return kNullOid;
    }
    const std::optional<Oid> id = commitOfReference(handle_.get(), ref.get());
    return id.value_or(kNullOid);
}

Oid Repository::head() const
{
    const Oid id = headOrNull();
    if (isNull(id)) {
        throw Error("repozytorium nie ma jeszcze zadnego commita (HEAD jest nieustalony)");
    }
    return id;
}

std::string Repository::headDescription() const
{
    git::ReferencePtr ref;
    if (git_repository_head(ref.out(), handle_.get()) != GIT_OK || !ref) {
        return "brak HEAD";
    }
    const char* name = git_reference_name(ref.get());
    if (name != nullptr && std::strncmp(name, "refs/heads/", 11) == 0) {
        return name + 11;
    }
    if (name != nullptr && std::strncmp(name, "refs/remotes/", 13) == 0) {
        return name + 13;
    }
    if (git_repository_head_detached(handle_.get()) == 1) {
        const std::optional<Oid> id = commitOfReference(handle_.get(), ref.get());
        if (id.has_value()) {
            return "HEAD (odlaczony na " + toShortHex(*id) + ")";
        }
        return "HEAD (odlaczony)";
    }
    return name != nullptr ? name : "HEAD";
}

Oid Repository::revparse(const std::string& spec) const
{
    if (spec.empty()) {
        throw UsageError("pusty odnosnik");
    }
    // Pelen hex bez prefiksu - szybsza sciezka bez parsowania refow.
    if (spec.size() == 40
        && std::all_of(spec.begin(), spec.end(),
                       [](unsigned char c) { return std::isxdigit(c) != 0; })) {
        const Oid id = fromHex(spec);
        const git_oid raw = toGitOid(id);
        git::ObjectPtr obj;
        if (git_object_lookup(obj.out(), handle_.get(), &raw, GIT_OBJECT_COMMIT) == GIT_OK
            && obj) {
            return id;
        }
    }

    git::ObjectPtr obj;
    if (git_revparse_single(obj.out(), handle_.get(), spec.c_str()) != GIT_OK || !obj) {
        git::fail("nie moge odczytac odnosnika \"" + spec + "\"", GIT_ERROR);
    }
    git::ObjectPtr commitObj;
    if (git_object_peel(commitObj.out(), obj.get(), GIT_OBJECT_COMMIT) != GIT_OK || !commitObj) {
        throw Error("\"" + spec + "\" nie jest commitem");
    }
    return fromGitOid(*git_object_id(commitObj.get()));
}

std::string Repository::describe(const Oid& id) const
{
    if (isNull(id)) {
        return "-";
    }
    git_describe_options options;
    git_describe_options_init(&options, GIT_DESCRIBE_OPTIONS_VERSION);
    // bez tego libgit2 zwraca skrot oida, gdy nie ma tagow - a wtedy
    // "opis" commita powtarzalby tylko to, co jest juz wypisane wyzej
    options.show_commit_oid_as_fallback = 0;

    const git_oid raw = toGitOid(id);
    git::CommitPtr commit;
    if (git_commit_lookup(commit.out(), handle_.get(), &raw) == GIT_OK && commit) {
        // git_commit dziedziczy po git_object, wiec wskazik castujemy w gore.
        auto* object = reinterpret_cast<git_object*>(commit.get());
        git::DescribeResultPtr described;
        if (git_describe_commit(described.out(), object, &options) == GIT_OK && described) {
            git_describe_format_options format;
            git_describe_format_options_init(&format, GIT_DESCRIBE_FORMAT_OPTIONS_VERSION);
            format.abbreviated_size = 12;
            format.dirty_suffix = "-dirty";

            git_buf buffer = {};
            if (git_describe_format(&buffer, described.get(), &format) == GIT_OK) {
                const std::string result(buffer.ptr != nullptr ? buffer.ptr : "");
                git_buf_dispose(&buffer);
                if (!result.empty()) {
                    return result;
                }
            } else {
                git_buf_dispose(&buffer);
            }
        }
    }

    // brak tagow: pokaz nazwe refa wskazujacego wprost na ten commit
    // (jak `git name-rev`), a dopiero potem skrot oida
    for (const BranchInfo& branch : branches(/*includeRemote=*/false)) {
        if (branch.tip == id) {
            return branch.name;
        }
    }
    return toShortHex(id);
}

// ---------------------------------------------------------------------------
// Commity
// ---------------------------------------------------------------------------

CommitInfo Repository::commit(const Oid& id) const
{
    const git_oid raw = toGitOid(id);
    git::CommitPtr commit;
    if (git_commit_lookup(commit.out(), handle_.get(), &raw) != GIT_OK || !commit) {
        throw Error("nie moge wczytac commita " + toShortHex(id));
    }
    CommitInfo info;
    info.id = id;
    info.author = identityOf(git_commit_author(commit.get()));
    info.committer = identityOf(git_commit_committer(commit.get()));
    info.message = strdup_(git_commit_message(commit.get()));
    info.subject = strdup_(git_commit_summary(commit.get()));
    if (info.message.size() > info.subject.size()) {
        info.body = info.message.substr(info.subject.size());
    }

    const std::size_t parents = git_commit_parentcount(commit.get());
    info.parents.reserve(parents);
    for (std::size_t i = 0; i < parents; ++i) {
        info.parents.push_back(fromGitOid(*git_commit_parent_id(commit.get(), i)));
    }
    return info;
}

std::string Repository::commitMessageBody(const Oid& id) const
{
    const CommitInfo info = commit(id);
    std::string body = info.subject;
    if (info.message.size() > info.subject.size()) {
        body += info.message.substr(info.subject.size());
    }
    return body;
}

std::size_t Repository::countReachable(const Oid& from) const
{
    if (isNull(from)) {
        return 0;
    }
    git::RevwalkPtr walk;
    if (git_revwalk_new(walk.out(), handle_.get()) != GIT_OK || !walk) {
        throw Error("nie moge utworzyc spacerki commitow");
    }
    const git_oid start = toGitOid(from);
    git::check(git_revwalk_push(walk.get(), &start));
    std::size_t count = 0;
    git_oid current{};
    while (git_revwalk_next(&current, walk.get()) == GIT_OK) {
        ++count;
    }
    return count;
}

bool Repository::isAncestor(const Oid& ancestor, const Oid& descendant) const
{
    if (ancestor == descendant) {
        return true;
    }
    if (isNull(ancestor) || isNull(descendant)) {
        return false;
    }
    const git_oid candidate = toGitOid(descendant);
    const git_oid possibleAncestor = toGitOid(ancestor);
    return git_graph_descendant_of(handle_.get(), &candidate, &possibleAncestor) == 1;
}

// ---------------------------------------------------------------------------
// Branchy
// ---------------------------------------------------------------------------

std::vector<BranchInfo> Repository::branches(bool includeRemote) const
{
    std::vector<BranchInfo> result;

    git_branch_t flags = GIT_BRANCH_LOCAL;
    if (includeRemote) {
        flags = static_cast<git_branch_t>(flags | GIT_BRANCH_REMOTE);
    }

    git::BranchIteratorPtr iter;
    git::check(git_branch_iterator_new(iter.out(), handle_.get(), flags));

    const std::string headName = headDescription();
    for (;;) {
        git::ReferencePtr ref;
        git_branch_t kind = GIT_BRANCH_LOCAL;
        if (git_branch_next(ref.out(), &kind, iter.get()) != GIT_OK) {
            break;
        }
        BranchInfo info;
        info.fullName = strdup_(git_reference_name(ref.get()));
        info.remote = info.fullName.rfind("refs/remotes/", 0) == 0;
        info.shortName = branchRefName(info.fullName);
        info.name = info.shortName;
        if (info.remote && info.name.size() > 5
            && info.name.compare(info.name.size() - 5, 5, "/HEAD") == 0) {
            // origin/HEAD wskazuje domyslna galaz zdalna - to nie jest
            // oddzielna galaz, wiec pomijamy (jak `git branch -r`).
            continue;
        }
        if (const std::optional<Oid> tip = commitOfReference(handle_.get(), ref.get())) {
            info.tip = *tip;
            const CommitInfo commitInfo = commit(info.tip);
            info.author = commitInfo.author;
            info.subject = commitInfo.subject;
        }

        if (!info.remote) {
            git::ReferencePtr upstreamRef;
            if (git_branch_upstream(upstreamRef.out(), ref.get()) == GIT_OK && upstreamRef) {
                const std::optional<Oid> upstreamTip
                    = commitOfReference(handle_.get(), upstreamRef.get());
                if (upstreamTip.has_value()) {
                    info.hasTracking = true;
                    info.upstreamRef = strdup_(git_reference_name(upstreamRef.get()));
                    info.upstream = branchRefName(info.upstreamRef);
                    std::size_t ahead = 0;
                    std::size_t behind = 0;
                    const git_oid local = toGitOid(info.tip);
                    const git_oid up = toGitOid(*upstreamTip);
                    if (git_graph_ahead_behind(&ahead, &behind, handle_.get(), &local, &up)
                        == GIT_OK) {
                        info.ahead = ahead;
                        info.behind = behind;
                    }
                }
            }
        }

        info.current = (!info.remote && info.name == headName);
        result.push_back(std::move(info));
    }

    std::sort(result.begin(), result.end(), [](const BranchInfo& a, const BranchInfo& b) {
        if (a.current != b.current) {
            return a.current;
        }
        if (a.remote != b.remote) {
            return !a.remote;
        }
        return a.name < b.name;
    });
    return result;
}

std::optional<BranchInfo> Repository::currentBranch() const
{
    for (const auto& branch : branches(false)) {
        if (branch.current) {
            return branch;
        }
    }
    return std::nullopt;
}

std::vector<std::string> Repository::tags() const
{
    git_strarray list = {};
    git::check(git_tag_list(&list, handle_.get()));
    std::vector<std::string> result;
    result.reserve(list.count);
    for (std::size_t i = 0; i < list.count; ++i) {
        result.emplace_back(list.strings[i]);
    }
    git_strarray_dispose(&list);
    std::sort(result.begin(), result.end());
    return result;
}

// ---------------------------------------------------------------------------
// Drzewo, pliki, blame
// ---------------------------------------------------------------------------

std::vector<TreeEntry> Repository::tree(const Oid& commitId) const
{
    const std::optional<git_oid> treeOid = treeOidOfCommit(handle_.get(), commitId);
    if (!treeOid) {
        throw Error("nie moge odczytac drzewa commita " + toShortHex(commitId));
    }
    git::TreePtr tree;
    if (git_tree_lookup(tree.out(), handle_.get(), &*treeOid) != GIT_OK || !tree) {
        throw Error("nie moge wczytac drzewa " + toShortHex(commitId));
    }
    std::vector<TreeEntry> entries;
    collectTreeEntries(handle_.get(), tree.get(), "", true, false, entries);
    std::sort(entries.begin(), entries.end(),
              [](const TreeEntry& a, const TreeEntry& b) { return a.path < b.path; });
    return entries;
}
std::uint64_t Repository::blobSize(const Oid& blobId) const
{
    const git_oid raw = toGitOid(blobId);
    git::BlobPtr blob;
    if (git_blob_lookup(blob.out(), handle_.get(), &raw) != GIT_OK || !blob) {
        return 0;
    }
    return static_cast<std::uint64_t>(git_blob_rawsize(blob.get()));
}

bool Repository::blobIsBinary(const Oid& blobId) const
{
    const git_oid raw = toGitOid(blobId);
    git::BlobPtr blob;
    if (git_blob_lookup(blob.out(), handle_.get(), &raw) != GIT_OK || !blob) {
        return false;
    }
    return git_blob_is_binary(blob.get()) != 0;
}

std::size_t Repository::blobLineCount(const Oid& blobId) const
{
    const git_oid raw = toGitOid(blobId);
    git::BlobPtr blob;
    if (git_blob_lookup(blob.out(), handle_.get(), &raw) != GIT_OK || !blob) {
        return 0;
    }
    const char* data = static_cast<const char*>(git_blob_rawcontent(blob.get()));
    const auto size = static_cast<std::size_t>(git_blob_rawsize(blob.get()));
    if (data == nullptr || size == 0) {
        return 0;
    }
    std::size_t lines = 0;
    const void* found = std::memchr(data, '\n', size);
    while (found != nullptr) {
        ++lines;
        const auto offset = static_cast<std::size_t>(static_cast<const char*>(found) - data) + 1;
        found = offset >= size ? nullptr : std::memchr(data + offset, '\n', size - offset);
    }
    // Plik bez znaku konca linii na koncu liczymy jako dodatkowa linia.
    if (data[size - 1] != '\n') {
        ++lines;
    }
    return lines;
}

std::vector<TreeEntry> Repository::tree(const Oid& commitId, const std::string& directory) const
{
    const std::optional<git_oid> treeOid = treeOidOfCommit(handle_.get(), commitId);
    if (!treeOid) {
        return {};
    }
    git::TreePtr tree;
    if (git_tree_lookup(tree.out(), handle_.get(), &*treeOid) != GIT_OK || !tree) {
        return {};
    }
    std::string prefix = normalizePath(directory);
    if (prefix == ".") {
        prefix.clear();
    }
    if (!prefix.empty()) {
        const std::optional<FoundEntry> found = findTreeEntry(tree.get(), prefix);
        if (!found.has_value() || found->type != GIT_OBJECT_TREE) {
            return {};
        }
        const git_oid entryId = toGitOid(found->id);
        git::TreePtr sub;
        if (git_tree_lookup(sub.out(), handle_.get(), &entryId) != GIT_OK || !sub) {
            return {};
        }
        tree = std::move(sub);
        prefix += "/";
    }
    std::vector<TreeEntry> entries;
    collectTreeEntries(handle_.get(), tree.get(), prefix, false, true, entries);
    std::sort(entries.begin(), entries.end(),
              [](const TreeEntry& a, const TreeEntry& b) { return a.path < b.path; });
    return entries;
}

std::optional<TreeEntry> Repository::fileAt(const Oid& commitId, const std::string& path) const
{
    const std::string normalized = normalizePath(path);
    for (const TreeEntry& entry : tree(commitId)) {
        if (entry.path == normalized) {
            return entry;
        }
    }
    return std::nullopt;
}

std::string Repository::fileContent(const Oid& commitId, const std::string& path) const
{
    const std::string normalized = normalizePath(path);
    const std::optional<git_oid> treeOid = treeOidOfCommit(handle_.get(), commitId);
    if (!treeOid) {
        return {};
    }
    git::TreePtr tree;
    if (git_tree_lookup(tree.out(), handle_.get(), &*treeOid) != GIT_OK || !tree) {
        return {};
    }
    const std::optional<FoundEntry> found = findTreeEntry(tree.get(), normalized);
    if (!found.has_value()) {
        throw Error("plik \"" + normalized + "\" nie istnieje w commicie " + toShortHex(commitId));
    }
    const git_oid entryId = toGitOid(found->id);
    git::BlobPtr blob;
    if (git_blob_lookup(blob.out(), handle_.get(), &entryId) != GIT_OK || !blob) {
        return {};
    }
    const char* data = static_cast<const char*>(git_blob_rawcontent(blob.get()));
    const std::size_t size = static_cast<std::size_t>(git_blob_rawsize(blob.get()));
    return std::string(data != nullptr ? data : "", size);
}

std::vector<std::string> Repository::fileLines(const Oid& commitId, const std::string& path) const
{
    const std::string content = fileContent(commitId, path);
    std::vector<std::string> lines;
    if (content.empty()) {
        return lines;
    }
    lines.reserve(64);
    std::string current;
    for (const char c : content) {
        if (c == '\n') {
            if (!current.empty() && current.back() == '\r') {
                current.pop_back();
            }
            lines.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        if (current.back() == '\r') {
            current.pop_back();
        }
        lines.push_back(std::move(current));
    }
    return lines;
}

std::size_t Repository::lineCount(const Oid& commitId, const std::string& path) const
{
    return fileLines(commitId, path).size();
}

std::vector<BlameLine> Repository::blame(const std::string& path, const Oid& commitId) const
{
    if (location_.bare) {
        throw Error("nie moge zrobic blame w repozytorium bare");
    }
    const std::string normalized = normalizePath(path);

    git_blame_options options;
    git_blame_options_init(&options, GIT_BLAME_OPTIONS_VERSION);
    // libgit2 nie implementuje jeszcze sledzenia kopiowania linii
    // (`-C`), wiec prosimy wylacznie o zwykly blame z mailmapem.
    options.flags = GIT_BLAME_USE_MAILMAP;
    if (!isNull(commitId)) {
        options.newest_commit = toGitOid(commitId);
    }

    git::BlamePtr blame;
    const int rc = git_blame_file(blame.out(), handle_.get(), normalized.c_str(), &options);
    if (rc != GIT_OK || !blame) {
        git::fail("nie moge policzyc blame dla \"" + normalized + "\"", rc);
    }

    const std::size_t count = git_blame_linecount(blame.get());
    std::vector<BlameLine> lines;
    lines.reserve(count);
    for (std::size_t i = 1; i <= count; ++i) { // numeracja libgit2 jest od 1
        const git_blame_line* line = git_blame_line_byindex(blame.get(), i);
        if (line == nullptr) {
            continue;
        }
        const git_blame_hunk* hunk = git_blame_hunk_byline(blame.get(), i);
        if (hunk == nullptr) {
            continue;
        }
        BlameLine item;
        item.lineNumber = i;
        item.commit = fromGitOid(hunk->final_commit_id);
        item.author = identityOf(hunk->final_signature);
        item.finalPath = hunk->orig_path != nullptr ? hunk->orig_path : normalized;
        item.origLine = hunk->orig_start_line_number;
        item.boundary = hunk->boundary != 0;
        lines.push_back(std::move(item));
    }
    return lines;
}

// ---------------------------------------------------------------------------
// Diffy
// ---------------------------------------------------------------------------

Oid Repository::emptyTree()
{
    // OID pustego drzewa jest stalą dla SHA-1 - libgit2 nie wystawia
    // funkcji, która by je wyliczała.
    return fromHex("4b825dc642cb6eb9a060e54bf8d69288fbee4904");
}

namespace {

/// Licznik linii w jednym pliku.
struct LineCounter {
    std::uint64_t additions{0};
    std::uint64_t deletions{0};
};

/// Zlicza dodane i usuniete linie dla pojedynczej delty.
///
/// libgit2 1.9 nie udostepnia `git_diff_file_stat` ani publicznego
/// `git_diff_stats_deltas`, a `git_diff_file` nie niesie juz licznikow.
/// Zostaje wiec `git_patch_line_stats`, ktore liczy to samo, co numstat.
LineCounter countDeltaLines(git_diff* diff, std::size_t index)
{
    LineCounter counter;
    git::PatchPtr patch;
    if (git_patch_from_diff(patch.out(), diff, index) != GIT_OK || !patch) {
        // plik binarny albo bez tekstowego diffu - zero zmian
        return counter;
    }
    std::size_t context = 0;
    std::size_t additions = 0;
    std::size_t deletions = 0;
    if (git_patch_line_stats(&context, &additions, &deletions, patch.get()) != GIT_OK) {
        return counter;
    }
    counter.additions = additions;
    counter.deletions = deletions;
    return counter;
}

ChangeStatus statusOf(git_delta_t status)
{
    switch (status) {
    case GIT_DELTA_ADDED:
        return ChangeStatus::Added;
    case GIT_DELTA_DELETED:
        return ChangeStatus::Deleted;
    case GIT_DELTA_RENAMED:
        return ChangeStatus::Renamed;
    case GIT_DELTA_COPIED:
        return ChangeStatus::Copied;
    case GIT_DELTA_TYPECHANGE:
        return ChangeStatus::TypeChange;
    case GIT_DELTA_UNTRACKED:
        return ChangeStatus::Untracked;
    case GIT_DELTA_IGNORED:
        return ChangeStatus::Ignored;
    case GIT_DELTA_CONFLICTED:
        return ChangeStatus::Conflicted;
    case GIT_DELTA_UNMODIFIED:
    case GIT_DELTA_UNREADABLE:
    default:
        return ChangeStatus::Modified;
    }
}

DiffStats buildStats(git_diff* diff, bool findRenames)
{
    DiffStats stats;
    const std::size_t deltas = git_diff_num_deltas(diff);

    for (std::size_t i = 0; i < deltas; ++i) {
        const git_diff_delta* delta = git_diff_get_delta(diff, i);
        if (!delta) {
            continue;
        }
        FileChange change;
        change.path = delta->new_file.path != nullptr ? delta->new_file.path : "";
        change.oldPath = delta->old_file.path != nullptr ? delta->old_file.path : "";
        change.oldSize = delta->old_file.size;
        change.newSize = delta->new_file.size;
        change.binary = (delta->flags & GIT_DIFF_FLAG_BINARY) != 0;
        change.status = statusOf(delta->status);

        if (!change.binary) {
            const LineCounter counter = countDeltaLines(diff, i);
            change.additions = counter.additions;
            change.deletions = counter.deletions;
            stats.additions += change.additions;
            stats.deletions += change.deletions;
        }
        if (change.status == ChangeStatus::Added && change.oldPath.empty()) {
            change.oldPath = change.path;
        }
        stats.files.push_back(std::move(change));
    }
    stats.filesChanged = static_cast<std::uint32_t>(stats.files.size());
    (void)findRenames;
    return stats;
}

} // namespace

DiffStats Repository::diffTrees(const Oid& fromTree, const Oid& toTree) const
{
    git::TreePtr oldTree;
    git::TreePtr newTree;

    if (!isNull(fromTree)) {
        const git_oid raw = toGitOid(fromTree);
        if (git_tree_lookup(oldTree.out(), handle_.get(), &raw) != GIT_OK) {
            oldTree.reset();
        }
    }
    if (!isNull(toTree)) {
        const git_oid raw = toGitOid(toTree);
        if (git_tree_lookup(newTree.out(), handle_.get(), &raw) != GIT_OK) {
            newTree.reset();
        }
    }

    git_diff_options options;
    git_diff_options_init(&options, GIT_DIFF_OPTIONS_VERSION);
    options.flags = static_cast<git_diff_option_t>(
        GIT_DIFF_INCLUDE_TYPECHANGE | GIT_DIFF_INCLUDE_UNTRACKED);
    options.context_lines = 3;
    options.interhunk_lines = 0;

    git_diff_find_options findOptions;
    git_diff_find_options_init(&findOptions, GIT_DIFF_FIND_OPTIONS_VERSION);
    findOptions.flags = static_cast<git_diff_find_t>(GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_COPIES);
    findOptions.rename_threshold = 50;

    git::DiffPtr diff;
    if (git_diff_tree_to_tree(diff.out(), handle_.get(), oldTree.get(), newTree.get(), &options)
            != GIT_OK
        || !diff) {
        throw Error("nie moge policzyc diffu");
    }
    git_diff_find_similar(diff.get(), &findOptions);
    return buildStats(diff.get(), true);
}

DiffStats Repository::diff(const Oid& from, const Oid& to) const
{
    Oid fromTree = emptyTree();
    if (!isNull(from)) {
        const std::optional<git_oid> found = treeOidOfCommit(handle_.get(), from);
        if (!found) {
            throw Error("nie moge odczytac drzewa commita " + toShortHex(from));
        }
        fromTree = fromGitOid(*found);
    }
    const std::optional<git_oid> toTree = treeOidOfCommit(handle_.get(), to);
    if (!toTree) {
        throw Error("nie moge odczytac drzewa commita " + toShortHex(to));
    }
    return diffTrees(fromTree, fromGitOid(*toTree));
}

DiffStats Repository::diffWorkdir(const Oid& commitId) const
{
    if (location_.bare) {
        throw Error("nie moge porownac z katalogiem roboczym w repozytorium bare");
    }
    git::TreePtr oldTree;
    if (!isNull(commitId)) {
        const std::optional<git_oid> treeOid = treeOidOfCommit(handle_.get(), commitId);
        if (treeOid && git_tree_lookup(oldTree.out(), handle_.get(), &*treeOid) != GIT_OK) {
            oldTree.reset();
        }
    }

    git_diff_options options;
    git_diff_options_init(&options, GIT_DIFF_OPTIONS_VERSION);
    options.flags = static_cast<git_diff_option_t>(
        GIT_DIFF_INCLUDE_TYPECHANGE | GIT_DIFF_INCLUDE_UNTRACKED);
    options.context_lines = 3;

    git::DiffPtr diff;
    if (git_diff_tree_to_workdir_with_index(diff.out(), handle_.get(), oldTree.get(), &options)
            != GIT_OK
        || !diff) {
        throw Error("nie moge policzyc diffu z katalogiem roboczym");
    }

    git_diff_find_options findOptions;
    git_diff_find_options_init(&findOptions, GIT_DIFF_FIND_OPTIONS_VERSION);
    findOptions.flags = static_cast<git_diff_find_t>(GIT_DIFF_FIND_RENAMES);
    git_diff_find_similar(diff.get(), &findOptions);
    return buildStats(diff.get(), true);
}

// ---------------------------------------------------------------------------
// Pomocnicze
// ---------------------------------------------------------------------------

bool Repository::isWorkingTreeClean() const
{
    return workingTreeChanges() == 0;
}

std::size_t Repository::workingTreeChanges() const
{
    if (location_.bare) {
        return 0;
    }
    git_status_options options;
    git_status_options_init(&options, GIT_STATUS_OPTIONS_VERSION);
    options.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    git::StatusListPtr status;
    if (git_status_list_new(status.out(), handle_.get(), &options) != GIT_OK || !status) {
        return 0;
    }
    return static_cast<std::size_t>(git_status_list_entrycount(status.get()));
}

std::string Repository::normalizePath(std::string path) const
{
    std::replace(path.begin(), path.end(), '\\', '/');
    while (path.size() > 1 && path.front() == '/' && !location_.workdir.empty()) {
        // absolutne sciezki sprowadzamy do formy relatywnej wobec workdir
        const std::string workdir = location_.workdir;
        if (path.rfind(workdir + "/", 0) == 0) {
            path = path.substr(workdir.size() + 1);
            break;
        }
        break;
    }
    if (path.rfind("./", 0) == 0) {
        path = path.substr(2);
    }
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

} // namespace gitlense
