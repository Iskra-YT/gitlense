#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "analysis/history.h"
#include "analysis/line_counts.h"
#include "core/language.h"
#include "core/repository.h"
#include "core/walker.h"

namespace gitlense {

/// Tabela "historia": ile pracy wlozono w kazdy jezyk.
struct LanguageHistory {
    std::string language;
    std::size_t commits{0};       // unikalne commity dotykajace jezyka
    std::size_t filesTouched{0};  // unikalne sciezki
    std::size_t authors{0};       // unikalni autorzy
    std::uint64_t additions{0};
    std::uint64_t deletions{0};
    Timestamp firstCommit{0};
    Timestamp lastCommit{0};
    double churnShare{0.0};       // % calego churnu repozytorium
    std::vector<std::pair<std::string, std::uint64_t>> topAuthors; // autor -> churn

    [[nodiscard]] std::uint64_t churn() const noexcept { return additions + deletions; }
};

/// Wiersz macierzy "autor x jezyk".
struct AuthorLanguageRow {
    std::string author;
    std::size_t commits{0};
    std::uint64_t additions{0};
    std::uint64_t deletions{0};
    std::uint64_t churn{0};
    std::map<std::string, std::uint64_t> churnByLanguage;
    std::map<std::string, std::size_t> commitsByLanguage;
    std::map<std::string, std::size_t> filesByLanguage;
};

struct LanguageOptions {
    WalkOptions walk;
    PathFilter path;
    std::size_t topN{0};            // 0 = wszystkie
    std::size_t topAuthorsPerLanguage{3};
    std::size_t threads{0};
};

struct LanguageResult {
    std::vector<LanguageHistory> history;   // tabela: jezyk -> historia
    std::vector<LanguageFact> current;      // tabela: jezyk -> stan HEAD
    std::vector<AuthorLanguageRow> authors; // macierz autor x jezyk
    std::uint64_t churnTotal{0};
    std::size_t languagesTotal{0};
    /// Jedyne jezyki obecne w HEAD.
    std::vector<std::string> currentLanguages;
    /// Jedyne jezyki dotyte kiedys w historii.
    std::vector<std::string> historicalLanguages;
};

LanguageResult analyzeLanguages(const Repository& repo, const LanguageDetector& detector,
                                const LanguageOptions& options);

} // namespace gitlense
