#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "analysis/history.h"
#include "core/language.h"
#include "core/repository.h"
#include "core/walker.h"

namespace gitlense {

enum class Granularity { Day, Week, Month, Year };

Granularity parseGranularity(const std::string& value);
const char* toString(Granularity granularity);
/// Krok czasowy dla wybranego okresu (w sekundach).
std::int64_t stepOf(Granularity granularity);
/// Etykieta okresu (np. "wrz 2026", "tydz. wrz 2026", "2026", "28 wrz").
std::string bucketLabel(Timestamp ts, Granularity granularity);

struct ActivityBucket {
    Timestamp start{0};
    std::string label;
    std::size_t commits{0};
    std::uint64_t additions{0};
    std::uint64_t deletions{0};
    std::size_t authors{0};
    std::set<std::string> authorNames;
    std::set<std::string> files;

    [[nodiscard]] std::uint64_t churn() const noexcept { return additions + deletions; }
};

struct AuthorActivity {
    std::string author;
    std::size_t commits{0};
    std::uint64_t churn{0};
    std::size_t activeDays{0};
    Timestamp firstCommit{0};
    Timestamp lastCommit{0};
    std::size_t longestStreak{0};
    std::size_t currentStreak{0};
    double commitsPerWeek{0.0};
    /// Podzial commitow na kolejne okresy (tylko gdy granularity != Year).
    std::vector<std::pair<Timestamp, std::size_t>> perPeriod;
};

struct ActivityResult {
    Granularity granularity{Granularity::Month};
    std::vector<ActivityBucket> buckets; // rosnaco po czasie
    std::vector<AuthorActivity> authors;
    std::vector<Timestamp> activeDays;   // unikalne dni z commitami
    std::size_t commitsOnWeekdays[7]{};
    std::size_t commitsOnHours[24]{};
    std::size_t totalCommits{0};
    std::uint64_t totalChurn{0};
    std::uint64_t additionsTotal{0};
    std::uint64_t deletionsTotal{0};
    std::size_t longestStreak{0};
    std::size_t currentStreak{0};
    Timestamp firstCommit{0};
    Timestamp lastCommit{0};
    /// Zmiany w liczbie commitow miedzy kolejnymi okresami.
    double trendPerPeriod{0.0};
};

struct ActivityOptions {
    WalkOptions walk;
    PathFilter path;
    Granularity granularity{Granularity::Month};
    std::size_t maxBuckets{24};
    std::size_t threads{0};
};

ActivityResult analyzeActivity(const Repository& repo, const LanguageDetector& detector,
                               const ActivityOptions& options);

} // namespace gitlense
