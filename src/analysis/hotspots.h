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

/// Plik "goracy": duzy, czesto zmieniany, z wieloma autorami.
struct HotspotFile {
    std::string path;
    std::string language;
    std::size_t lines{0};
    std::uint64_t churn{0};
    std::size_t commits{0};
    std::size_t authors{0};
    Timestamp lastModified{0};
    std::string lastAuthor;
    double churnPerLine{0.0};  // churn / linie (gdy lines == 0 -> 0)
    double score{0.0};         // znormalizowany wynik 0..1
    double riskPercent{0.0};   // 0..100 - ile ruchu przypada na "niestabilny" kod
};

/// Zbiorczy ranking autorow: kto generuje najwiekszy ruch.
struct HotspotAuthor {
    std::string author;
    std::size_t commits{0};
    std::uint64_t churn{0};
    std::size_t filesTouched{0};
    double churnShare{0.0};
};

struct HotspotOptions {
    WalkOptions walk;
    PathFilter path;
    std::string language;
    std::size_t topN{20};
    std::size_t threads{0};
};

struct HotspotResult {
    std::vector<HotspotFile> files;
    std::vector<HotspotAuthor> authors;
    std::uint64_t churnTotal{0};
    std::size_t filesAnalyzed{0};
    std::size_t linesAnalyzed{0};
    double churnHalfLifeYears{0.0};
    /// Rozklad ryzyka: suma churnu plikow starszych niz `ageThreshold` dni.
    double riskPercent{0.0};
};

HotspotResult analyzeHotspots(const Repository& repo, const LanguageDetector& detector,
                              const HotspotOptions& options);

} // namespace gitlense
