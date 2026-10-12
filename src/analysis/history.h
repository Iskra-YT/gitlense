#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/language.h"
#include "core/repository.h"
#include "core/types.h"
#include "core/walker.h"

namespace gitlense {

/// Jedna zmiana pliku w commicie, z przypisanym jezykiem.
struct FileChangeRecord {
    std::string path;
    std::string oldPath;
    ChangeStatus status{ChangeStatus::Modified};
    std::uint64_t additions{0};
    std::uint64_t deletions{0};
    bool binary{false};
    std::string language;       // pusty = nierozpoznany
    FileKind kind{FileKind::Other};
    std::uint64_t newSize{0};

    [[nodiscard]] std::uint64_t churn() const noexcept { return additions + deletions; }
};

/// Commit wraz z kompletem zmian - wynik skanowania historii.
struct CommitChange {
    Oid id{};
    Timestamp time{0};
    std::string authorName;
    std::string authorEmail;
    std::string subject;
    bool merge{false};
    bool root{false};
    std::size_t parentCount{0};
    std::vector<FileChangeRecord> files;
    std::uint64_t additions{0};
    std::uint64_t deletions{0};

    [[nodiscard]] std::uint64_t churn() const noexcept { return additions + deletions; }
    [[nodiscard]] std::size_t fileCount() const noexcept { return files.size(); }
};

struct HistoryOptions {
    WalkOptions walk;
    bool skipVendored{true};
    bool skipGenerated{true};
    bool skipBinary{true};
    /// 0 = bez limitu; po przekroczeniu pliki sa przycinane (dla `gitlense log`).
    std::size_t maxFilesPerCommit{0};
};

/// Jednorazowy kosztowny krok: przejscie po historii z policzeniem diffow.
/// Po nim wszystkie statystyki liczone sa z wynikow w pamieci.
class HistoryScan {
public:
    using Callback = std::function<void(const CommitChange&)>;

    HistoryScan(const Repository& repo, HistoryOptions options,
                const LanguageDetector& detector);

    /// Wykonuje skan. `callback` moze byc wolany z wielu watkow
    /// (musi byc bezpieczny watkowo - np. blokada mutexem).
    void forEach(Callback callback, std::size_t threads = 0) const;

    /// Zbiera wszystkie commity do wektora (uzywane przez male komendy).
    [[nodiscard]] std::vector<CommitChange> collect(std::size_t threads = 0) const;

    /// Same commity (bez diffow) - najszybsza sciezka.
    [[nodiscard]] std::vector<CommitInfo> commits() const;

    [[nodiscard]] std::size_t commitCount() const { return commitCount_; }
    [[nodiscard]] std::size_t mergeCount() const { return mergeCount_; }
    [[nodiscard]] std::size_t skippedFiles() const { return skippedFiles_; }

private:
    const Repository& repo_;
    HistoryOptions options_;
    const LanguageDetector& detector_;
    mutable std::size_t commitCount_{0};
    mutable std::size_t mergeCount_{0};
    mutable std::size_t skippedFiles_{0};
};

/// Filtruje liste plikow zgodnie z `--exclude` i `--include`.
class PathFilter {
public:
    /// Pusty filtr - przepuszcza wszystko.
    PathFilter() = default;
    explicit PathFilter(std::vector<std::string> include, std::vector<std::string> exclude);

    [[nodiscard]] bool accepts(const std::string& path) const;
    [[nodiscard]] bool empty() const { return include_.empty() && exclude_.empty(); }

private:
    std::vector<std::string> include_;
    std::vector<std::string> exclude_;
};

} // namespace gitlense
