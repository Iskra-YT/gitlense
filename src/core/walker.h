#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/repository.h"
#include "core/types.h"

namespace gitlense {

/// Filtry historii - wspolne dla log, contributors, languages, activity.
struct WalkOptions {
    std::size_t limit{0};           // 0 = bez limitu
    std::optional<Timestamp> since; // w sekundach UTC
    std::optional<Timestamp> until;
    std::string authorPattern;      // dopasowanie fragmentu nazwiska (case-insensitive)
    std::string committerPattern;
    std::string messagePattern;     // grep po treści commita (regex)
    std::string pathPattern;        // ograniczenie do sciezek (glob)
    bool includeAllRefs{false};     // cale repozytorium, nie tylko HEAD
    bool firstParentOnly{false};
    bool reverse{false};
    bool includeMergeCommits{true};
    std::vector<std::string> excludePaths; // proste globy pomijane w statystykach
};

/// Krokowy iterator po commitach (oparty o git_revwalk).
///
/// Pozwala przerwac przetwarzanie wczesnie (limit), a takze raportowac
/// postep w petli w watku glownym.
class CommitWalker {
public:
    using Callback = std::function<bool(const CommitInfo&)>;

    CommitWalker(const Repository& repo, WalkOptions options, Oid start);
    /// Przechodzi po commitach, az do konca lub do wyczerpania limitu.
    /// `onCommit` zwraca false, aby przerwac.
    std::size_t forEach(Callback onCommit) const;
    /// Zwraca tylko listę Oid (uzywane przez polecenia wylacznie tekstowe).
    std::vector<Oid> collect(std::size_t max = 0) const;

private:
    const Repository& repo_;
    WalkOptions options_;
    Oid start_;
};

/// Sprawdza, czy commit przechodzi po sciezkach (filtr -- path).
bool commitTouchesPaths(const Repository& repo, const CommitInfo& commit,
                        const std::vector<std::string>& patterns);

} // namespace gitlense
