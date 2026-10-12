#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "analysis/history.h"
#include "analysis/line_counts.h"
#include "core/language.h"
#include "core/repository.h"
#include "core/walker.h"

namespace gitlense {

/// Tabela 1: aktywnosc w historii (tyle commitow, tyle linii dodanych).
struct ContributorHistory {
    std::string name;
    std::string email;
    std::size_t commits{0};
    std::uint64_t additions{0};
    std::uint64_t deletions{0};
    std::size_t filesTouched{0};
    std::size_t activeDays{0};
    Timestamp firstCommit{0};
    Timestamp lastCommit{0};

    [[nodiscard]] std::uint64_t churn() const noexcept { return additions + deletions; }
};

/// Tabela 2: stan biezacy - kto "posiada" pliki w HEAD i kto ostatnio
/// zmienial linie (odpowiednik `git blame`).
struct ContributorNow {
    std::string name;
    std::size_t commits{0};
    std::uint64_t additions{0};
    std::uint64_t deletions{0};
    std::size_t filesTouched{0};
    std::size_t activeDays{0};
    Timestamp firstCommit{0};
    Timestamp lastCommit{0};

    // --- aktualna wlasnosc (z HEAD) ---
    std::size_t ownedFiles{0};
    std::size_t ownedLines{0};
    std::size_t totalLines{0};
    double ownershipPercent{0.0};

    // --- ostatnie dzialanie w HEAD ---
    Timestamp lastTouch{0};
    std::size_t lastTouchFiles{0};
    std::size_t lastTouchLines{0};

    // --- churn w oknie --recent ---
    std::uint64_t recentChurn{0};
    std::size_t recentCommits{0};

    [[nodiscard]] std::uint64_t churn() const noexcept { return additions + deletions; }
};

struct ContributorOptions {
    WalkOptions walk;
    std::string language;          // ogranicz do jednego jezyka
    PathFilter path;
    /// 0 = policz dla wszystkich, >0 = tylko dla N najwiekszych (szybszy blame)
    std::size_t ownershipTop{0};
    bool computeOwnership{true};
    /// Okno w sekundach dla kolumny "ostatnio" (0 = wylaczone).
    Timestamp recentWindow{0};
    std::size_t threads{0};
};

struct ContributorResult {
    std::vector<ContributorHistory> history;
    std::vector<ContributorNow> current;
    std::size_t commitsTotal{0};
    std::size_t authorsTotal{0};
    std::size_t filesTotal{0};
    std::uint64_t churnTotal{0};
    std::uint64_t additionsTotal{0};
    std::uint64_t deletionsTotal{0};
    std::size_t linesTotal{0};
    std::size_t filesOwnedTotal{0};
    bool ownershipComputed{false};
};

ContributorResult analyzeContributors(const Repository& repo, const LanguageDetector& detector,
                                      const ContributorOptions& options);

} // namespace gitlense
