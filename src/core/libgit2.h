#pragma once

#include <git2.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/types.h"

// libgit2 nie ma wlasnych smart-pointerow, wiec robimy je tutaj.
// Typy sa juz zadeklarowane w git2.h - powtarzamy tylko nazwy, zeby
// naglowek dal sie przegladac bez wiedzy o API.
struct git_repository;
struct git_object;
struct git_commit;
struct git_tree;
struct git_blob;
struct git_diff;
struct git_revwalk;
struct git_blame_options;
struct git_blame;
struct git_config;
struct git_oid;
struct git_reference;
struct git_index;
struct git_status_list;
struct git_annotated_commit;
struct git_diff_stats;
struct git_merge_base;
struct git_remote;

namespace gitlense::git {

/// Jednorazowa inicjalizacja globalna biblioteki.
class Library {
public:
    static Library& instance();
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;
    ~Library();

private:
    Library();
    bool initialized_{false};
};

/// Rzuca gitlense::Error z komunikatem z libgit2, gdy error != 0.
void check(int error);
/// To samo, ale z kontekstem ("Nie moge otworzyc repozytorium").
[[noreturn]] void fail(const std::string& context, int error);
/// Wersja z wlasnym komunikatem bledu.
[[noreturn]] void fail(const std::string& context, int error, const std::string& detail);

std::string lastErrorMessage();

/// RAII dla wszystkich obiektow libgit2 posiadajacych `free` w API.
///
/// W odróżnieniu od `std::unique_ptr` udostepnia takze `out()`, czyli
/// adres wskaznika wewnetrznego - libgit2 ma wlasnie taki styl API
/// (`git_tree_lookup(Handle.out(), repo, oid)`).
template <typename T, void (*Free)(T*)>
class Handle {
public:
    Handle() = default;
    explicit Handle(T* pointer) noexcept : pointer_(pointer) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&&) noexcept = default;
    Handle& operator=(Handle&&) noexcept = default;
    ~Handle() { reset(); }

    /// Zwalnia poprzednia zawartosc i zwraca adres wskaznika dla
    /// parametru `out` w API libgit2.
    T** out() noexcept {
        reset();
        return &pointer_;
    }

    [[nodiscard]] T* get() const noexcept { return pointer_; }
    explicit operator bool() const noexcept { return pointer_ != nullptr; }
    T* release() noexcept { return std::exchange(pointer_, nullptr); }

    void reset(T* pointer = nullptr) noexcept {
        if (pointer_ != nullptr && pointer_ != pointer) {
            Free(pointer_);
        }
        pointer_ = pointer;
    }

private:
    T* pointer_{nullptr};
};

template <typename T, void (*Free)(T*)>
using Ptr = Handle<T, Free>;

using ObjectPtr = Ptr<git_object, git_object_free>;
using CommitPtr = Ptr<git_commit, git_commit_free>;
using TreePtr = Ptr<git_tree, git_tree_free>;
using BlobPtr = Ptr<git_blob, git_blob_free>;
using DiffPtr = Ptr<git_diff, git_diff_free>;
using RevwalkPtr = Ptr<git_revwalk, git_revwalk_free>;
using ReferencePtr = Ptr<git_reference, git_reference_free>;
using IndexPtr = Ptr<git_index, git_index_free>;
using StatusListPtr = Ptr<git_status_list, git_status_list_free>;
using RemotePtr = Ptr<git_remote, git_remote_free>;
using BlamePtr = Ptr<git_blame, git_blame_free>;
using DiffStatsPtr = Ptr<git_diff_stats, git_diff_stats_free>;
using TreeEntryPtr = Ptr<git_tree_entry, git_tree_entry_free>;
using BranchIteratorPtr = Ptr<git_branch_iterator, git_branch_iterator_free>;
using DescribeResultPtr = Ptr<git_describe_result, git_describe_result_free>;
using PatchPtr = Ptr<git_patch, git_patch_free>;
// Uwaga: git_blame_options i git_merge_base to struktury na stosie -
// libgit2 nie udostepnia dla nich `free`, wiec nie opakowujemy ich w `Ptr`.

/// Konwersje między naszym `Oid` (20 bajtów) a `git_oid`.
inline git_oid toGitOid(const Oid& oid) noexcept
{
    git_oid out{};
    std::memcpy(out.id, oid.data(), oid.size());
    return out;
}

inline Oid fromGitOid(const git_oid& oid) noexcept
{
    Oid out{};
    std::memcpy(out.data(), oid.id, out.size());
    return out;
}

/// Otwiera repozytorium libgit2 i trzyma je otwarte.
class RepositoryHandle {
public:
    RepositoryHandle() = default;
    /// `path` moze byc plikiem albo katalogiem roboczym; libgit2
    /// odnajduje katalog .git takze w katalogach nadrzednych.
    explicit RepositoryHandle(const std::filesystem::path& path);
    ~RepositoryHandle();
    RepositoryHandle(RepositoryHandle&&) noexcept;
    RepositoryHandle& operator=(RepositoryHandle&&) noexcept;
    RepositoryHandle(const RepositoryHandle&) = delete;
    RepositoryHandle& operator=(const RepositoryHandle&) = delete;

    [[nodiscard]] git_repository* get() const noexcept { return repo_; }
    [[nodiscard]] explicit operator bool() const noexcept { return repo_ != nullptr; }

private:
    git_repository* repo_{nullptr};
};

} // namespace gitlense::git
