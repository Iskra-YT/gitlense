#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/libgit2.h"
#include "core/types.h"

namespace gitlense {

struct BranchInfo {
    std::string name;      // "main" (lokalna) lub "origin/main"
    std::string fullName;  // "refs/heads/main"
    std::string shortName; // "main" z "refs/heads/"
    std::string upstream;  // "origin/main" lub pusty
    std::string upstreamRef;
    Oid tip{};
    Identity author;
    std::string subject;
    bool remote{false};
    bool current{false};
    std::size_t ahead{0};
    std::size_t behind{0};
    bool hasTracking{false};
    bool detached{false};
    std::string tag; // opcjonalny opis najblizszego taga
};

/// Katalog roboczy + sciezka wskazujaca wprost na .git.
struct RepoLocation {
    std::filesystem::path workdir;   // katalog roboczy (bez koncowego '/')
    std::filesystem::path gitdir;    // /path/to/.git
    bool bare{false};
};

/// Cienka, bezpieczna dla watkow warstwa na libgit2.
///
/// Obiekt jest lekki (otwarcie repo = mmap indeksu), wiec mozna go
/// otworzyc osobno w kazdym watku zamiast chronic mutexem.
class Repository {
public:
    explicit Repository(std::filesystem::path path = ".");
    ~Repository();
    Repository(Repository&&) noexcept;
    Repository& operator=(Repository&&) noexcept;
    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;

    [[nodiscard]] git::RepositoryHandle& handle() noexcept { return handle_; }
    [[nodiscard]] const git::RepositoryHandle& handle() const noexcept { return handle_; }
    [[nodiscard]] const RepoLocation& location() const noexcept { return location_; }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return sourcePath_; }

    // --- refy -------------------------------------------------------------
    [[nodiscard]] bool isBare() const noexcept { return location_.bare; }
    [[nodiscard]] bool hasHead() const;
    [[nodiscard]] Oid head() const;            // rzuca jesli repo puste
    [[nodiscard]] Oid headOrNull() const;      // kNullOid jesli puste
    [[nodiscard]] std::string headDescription() const; // "main", "HEAD (detached)"
    [[nodiscard]] Oid revparse(const std::string& spec) const;
    /// Nazwa commitu w stylu `git describe`: tag, nazwa galezi/tagu
    /// wskazujacego na commit albo skrot oida. Nigdy nie zwraca samego
    /// oida, jesli da sie go opisac inaczej.
    [[nodiscard]] std::string describe(const Oid& id) const;
    [[nodiscard]] std::vector<BranchInfo> branches(bool includeRemote = true) const;
    [[nodiscard]] std::optional<BranchInfo> currentBranch() const;
    [[nodiscard]] std::vector<std::string> tags() const;

    // --- commity ----------------------------------------------------------
    [[nodiscard]] CommitInfo commit(const Oid& id) const;
    [[nodiscard]] std::string commitMessageBody(const Oid& id) const;
    /// Liczba commitow osiagalnych z podanego refa (0 dla pustego).
    [[nodiscard]] std::size_t countReachable(const Oid& from) const;
    /// Czy `ancestor` jest przodkiem `descendant` (0 = nie / nieznane).
    [[nodiscard]] bool isAncestor(const Oid& ancestor, const Oid& descendant) const;

    // --- drzewo i pliki ---------------------------------------------------
    [[nodiscard]] std::vector<TreeEntry> tree(const Oid& commitId) const;
    [[nodiscard]] std::vector<TreeEntry> tree(const Oid& commitId,
                                              const std::string& directory) const;
    /// Rozmiar bloba w bajtach (0 dla brakujacego).
    [[nodiscard]] std::uint64_t blobSize(const Oid& blobId) const;
    /// Liczba linii tekstu w blobie, liczona bez kopiowania tresci.
    [[nodiscard]] std::size_t blobLineCount(const Oid& blobId) const;
    /// Czy blob jest binarny (null + niedrukowalne znaki w pierwszych 8 kB).
    [[nodiscard]] bool blobIsBinary(const Oid& blobId) const;
    [[nodiscard]] std::optional<TreeEntry> fileAt(const Oid& commitId,
                                                 const std::string& path) const;
    [[nodiscard]] std::string fileContent(const Oid& commitId, const std::string& path) const;
    [[nodiscard]] std::vector<std::string> fileLines(const Oid& commitId,
                                                     const std::string& path) const;
    /// Blame dla pliku w danym commicie (domyslnie HEAD).
    [[nodiscard]] std::vector<BlameLine> blame(const std::string& path,
                                               const Oid& commitId) const;
    /// Liczba linii pliku w danym commicie (uzywane przez statystyki).
    [[nodiscard]] std::size_t lineCount(const Oid& commitId, const std::string& path) const;

    // --- diffy ------------------------------------------------------------
    /// `from` == kNullOid oznacza porownanie z pustym drzewem.
    [[nodiscard]] DiffStats diff(const Oid& from, const Oid& to) const;
    [[nodiscard]] DiffStats diffTrees(const Oid& fromTree, const Oid& toTree) const;
    [[nodiscard]] DiffStats diffWorkdir(const Oid& commitId) const;
    /// Oid pustego drzewa - do diffow pierwszego commita.
    [[nodiscard]] static Oid emptyTree();

    // --- pomocnicze -------------------------------------------------------
    [[nodiscard]] bool isWorkingTreeClean() const;
    /// Ilosc niezacommitowanych zmian (0 = czysto).
    [[nodiscard]] std::size_t workingTreeChanges() const;
    [[nodiscard]] std::string normalizePath(std::string path) const;

private:
    void init();

    git::RepositoryHandle handle_;
    RepoLocation location_;
    std::filesystem::path sourcePath_;
};

/// Repozytorium wskazane przez --repo/-C albo biezace (dokladnie to,
/// czego uzywa libgit2 dla biezacego katalogu).
Repository& defaultRepository(const std::filesystem::path& path);

} // namespace gitlense
