#include "core/libgit2.h"

#include "core/error.h"

#include <git2.h>

#include <cstdio>
#include <utility>

namespace gitlense::git {

Library& Library::instance()
{
    static Library lib;
    return lib;
}

Library::Library()
{
    git_libgit2_init();
    initialized_ = true;
}

Library::~Library()
{
    if (initialized_) {
        git_libgit2_shutdown();
        initialized_ = false;
    }
}

std::string lastErrorMessage()
{
    const git_error* err = git_error_last();
    if (err == nullptr || err->message == nullptr) {
        return "nieznany blad biblioteki git";
    }
    std::string message = err->message;
    switch (err->klass) {
    case GIT_ERROR_NOMEMORY:
        return "brak pamieci: " + message;
    case GIT_ERROR_OS:
        return "system: " + message;
    case GIT_ERROR_INVALID:
        return "nieprawidlowy argument: " + message;
    case GIT_ERROR_REFERENCE:
        return "bledny ref: " + message;
    case GIT_ERROR_ZLIB:
        return "blad kompresji: " + message;
    case GIT_ERROR_REPOSITORY:
        return "bledne repozytorium: " + message;
    case GIT_ERROR_CONFIG:
        return "bledna konfiguracja: " + message;
    case GIT_ERROR_REGEX:
        return "bledny wyrazenie: " + message;
    case GIT_ERROR_ODB:
        return "baza obiektow: " + message;
    case GIT_ERROR_INDEX:
        return "indeks: " + message;
    case GIT_ERROR_OBJECT:
        return "obiekt: " + message;
    case GIT_ERROR_NET:
        return "siec: " + message;
    case GIT_ERROR_TAG:
        return "tag: " + message;
    case GIT_ERROR_TREE:
        return "drzewo: " + message;
    case GIT_ERROR_INDEXER:
        return "indeksowanie: " + message;
    case GIT_ERROR_SSH:
        return "ssh: " + message;
    case GIT_ERROR_FILTER:
        return "filtr: " + message;
    case GIT_ERROR_REVERT:
        return "revert: " + message;
    case GIT_ERROR_CALLBACK:
        return "callback: " + message;
    case GIT_ERROR_CHERRYPICK:
        return "cherry-pick: " + message;
    case GIT_ERROR_DESCRIBE:
        return "opisywanie commita: " + message;
    case GIT_ERROR_REBASE:
        return "rebase: " + message;
    case GIT_ERROR_FILESYSTEM:
        return "system plikow: " + message;
    case GIT_ERROR_PATCH:
        return "latka: " + message;
    case GIT_ERROR_WORKTREE:
        return "katalog roboczy: " + message;
    case GIT_ERROR_SHA1:
        return "sha: " + message;
    case GIT_ERROR_HTTP:
        return "http: " + message;
    case GIT_ERROR_NONE:
    default:
        break;
    }
    return message;
}

void check(int error)
{
    if (error != GIT_OK) {
        throw Error(lastErrorMessage());
    }
}

void fail(const std::string& context, int error)
{
    (void)error;
    throw Error(context + ": " + lastErrorMessage());
}

void fail(const std::string& context, int error, const std::string& detail)
{
    if (error == GIT_OK) {
        throw Error(context + ": " + detail);
    }
    throw Error(context + ": " + detail + " (" + lastErrorMessage() + ")");
}

RepositoryHandle::RepositoryHandle(const std::filesystem::path& path)
{
    (void)Library::instance();

    git_repository* repo = nullptr;
    int rc = git_repository_open_ext(
        &repo, path.c_str(), GIT_REPOSITORY_OPEN_CROSS_FS | GIT_REPOSITORY_OPEN_NO_SEARCH, nullptr);
    if (rc != GIT_OK) {
        // NO_SEARCH nam przeszkadza - sprobujmy standardowe wyszukiwanie w gore.
        repo = nullptr;
        rc = git_repository_open(&repo, path.c_str());
    }
    if (rc != GIT_OK) {
        throw Error("nie moge otworzyc repozytorium Git w \"" + path.string()
                    + "\": " + lastErrorMessage());
    }
    repo_ = repo;
}

RepositoryHandle::~RepositoryHandle()
{
    if (repo_) {
        git_repository_free(repo_);
        repo_ = nullptr;
    }
}

RepositoryHandle::RepositoryHandle(RepositoryHandle&& other) noexcept
    : repo_(std::exchange(other.repo_, nullptr))
{
}

RepositoryHandle& RepositoryHandle::operator=(RepositoryHandle&& other) noexcept
{
    if (this != &other) {
        if (repo_) {
            git_repository_free(repo_);
        }
        repo_ = std::exchange(other.repo_, nullptr);
    }
    return *this;
}

} // namespace gitlense::git
