#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "analysis/history.h"
#include "core/language.h"
#include "core/repository.h"

namespace gitlense {

/// Stan plikow w danym commicie (domyslnie HEAD) - "teraz".
struct FileFacts {
    std::string path;
    std::string language;
    FileKind kind{FileKind::Other};
    std::size_t lines{0};
    std::uint64_t size{0};
    Timestamp lastModified{0};
    Oid lastCommit{};
    std::string lastAuthor;
    std::uint64_t commitCount{0}; // ile razy plik byl zmieniany w historii
    std::uint64_t churn{0};       // suma dodanych + usunietych linii w historii
    std::uint64_t authors{0};     // liczba roznych autorow dotykajacych pliku
    Timestamp created{0};         // kiedy plik pojawil sie w repo
};

/// Zagregowany stan repozytorium w danym commicie.
struct RepoFacts {
    Oid commit{};
    std::vector<FileFacts> files;
    std::unordered_map<std::string, std::size_t> linesByLanguage;
    std::unordered_map<std::string, std::size_t> filesByLanguage;
    std::size_t totalLines{0};
    std::size_t totalFiles{0};
    std::size_t totalBytes{0};
    std::size_t vendoredFiles{0};
    std::size_t generatedFiles{0};
    std::size_t binaryFiles{0};
};

struct FactsOptions {
    Oid commit{ kNullOid };
    PathFilter path;
    bool includeVendored{false};
    bool includeGenerated{false};
    std::size_t threads{0};
};

/// Liczy stan biezacy (pliki, linie, jezyki) na podstawie drzewa commita.
RepoFacts collectRepoFacts(const Repository& repo, const LanguageDetector& detector,
                           const FactsOptions& options);

/// Wzbogaca pliki o statystyki z historii (churn, liczba autorow, commitow).
/// Wymaga wczesniejszego skanu historii.
void enrichWithHistory(RepoFacts& facts, const std::vector<CommitChange>& history);

/// Tabela stanu dla jednego jezyka.
struct LanguageFact {
    std::string language;
    std::size_t files{0};
    std::size_t lines{0};
    std::uint64_t bytes{0};
};

std::vector<LanguageFact> languagesOf(const RepoFacts& facts);

} // namespace gitlense
