#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace gitlense {

using Oid = std::array<std::uint8_t, 20>;

/// Oid zerowy - uzywany m.in. jako "pusty" koniec zakresu w diffach.
constexpr Oid kNullOid{};

bool isNull(const Oid& oid) noexcept;
std::string toHex(const Oid& oid);
/// Skrot o zadanej dlugosci (domyslnie 7, jak w `git log --oneline`).
std::string toShortHex(const Oid& oid, std::size_t length = 7);
/// Parsuje skrot lub pelny hex; rzuca Error gdy niepoprawny.
Oid fromHex(const std::string& hex);

/// Znacznik czasu z gita (sekundy od epoki, czas UTC).
using Timestamp = std::int64_t;

struct Identity {
    std::string name;
    std::string email;
    Timestamp time{0};
    std::string timezone; // np. "+0200"
};

struct CommitInfo {
    Oid id{};
    std::vector<Oid> parents;
    Identity author;
    Identity committer;
    std::string subject;  // pierwsza linia komunikatu
    std::string body;     // reszta komunikatu (moze zawierac "\n")
    std::string message;  // caly komunikat
    std::string decoration; // np. "HEAD -> main" (wypelniane przez repo)
};

enum class ChangeStatus {
    Added,
    Deleted,
    Modified,
    Renamed,
    Copied,
    TypeChange,
    Untracked,
    Conflicted,
    Ignored,
};

/// Pelna nazwa statusu (do opisow i komunikatow bledow).
const char* toString(ChangeStatus status) noexcept;
/// Jednoliterowy skrot jak w `git diff --name-status` ("A", "D", "M", ...).
char statusCode(ChangeStatus status) noexcept;

struct FileChange {
    std::string path;    // nowa sciezka (przy zmianie nazwy: nowa)
    std::string oldPath; // poprzednia sciezka (przy zmianie nazwy: stara)
    ChangeStatus status{ChangeStatus::Modified};
    std::uint64_t additions{0};
    std::uint64_t deletions{0};
    std::uint64_t oldSize{0};
    std::uint64_t newSize{0};
    bool binary{false};

    [[nodiscard]] std::string displayPath() const
    {
        return oldPath.empty() ? path : path;
    }
    [[nodiscard]] std::uint64_t churn() const noexcept { return additions + deletions; }
};

/// Zagregowany diff: lista plikow + suma zmian.
struct DiffStats {
    std::vector<FileChange> files;
    std::uint64_t additions{0};
    std::uint64_t deletions{0};
    std::uint32_t filesChanged{0};
    bool truncated{false};

    [[nodiscard]] std::uint64_t churn() const noexcept { return additions + deletions; }
};

/// Wpis drzewa katalogow (dane z commit.tree).
struct TreeEntry {
    std::string path;
    Oid id{};
    std::string mode;   // np. "100644"
    std::uint64_t size{0};
    bool directory{false};
    bool submodule{false};
};

struct BlameLine {
    std::size_t lineNumber{0}; // 1-based
    Oid commit{};
    Identity author;
    std::string finalPath;
    std::uint32_t origLine{0};
    bool boundary{false};
};

} // namespace gitlense
