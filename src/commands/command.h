#pragma once

#include <string>
#include <vector>

#include "cli/args.h"
#include "core/language.h"
#include "core/repository.h"
#include "core/walker.h"
#include "format/format.h"

namespace gitlense {

/// Kontekst przekazywany do kazdej podkomendy.
struct CommandContext {
    const Repository& repo;
    const LanguageDetector& detector;
    DateStyle dateStyle{DateStyle::Local};
    std::string dateFormat;
    std::size_t threads{0};
    std::size_t tabWidth{4};
    std::size_t pathWidth{60};
    bool quiet{false};
    bool verbose{false};
    bool showHeader{true};

    [[nodiscard]] std::string date(Timestamp when) const
    {
        return formatDate(when, dateStyle, dateFormat);
    }
};

struct Command {
    const char* name;
    const char* summary;   // jedno zdanie do listy komend
    const char* details;   // pelny opis do `gitlense help <komenda>`
    const char* examples;  // przyklady uzycia
    int (*run)(const CommandContext&, const Arguments&);
};

/// Rejestr komend (uzywany tez przez `--help`).
class Registry {
public:
    static Registry& instance();
    void add(Command command);
    [[nodiscard]] const Command* find(const std::string& name) const;
    [[nodiscard]] std::vector<const Command*> sorted() const;
    [[nodiscard]] std::string overview() const;
    [[nodiscard]] static std::string helpFor(const Command& command);

private:
    std::vector<Command> commands_;
};

/// Rejestracja wszystkich komend (wołana raz z main).
void registerAllCommands();

// --- helpery wspoldzielone miedzy komendami ------------------------------

/// Buduje `WalkOptions` z opcji wspolnych dla wszystkich komend historycznych.
WalkOptions walkOptionsFromArgs(const Arguments& args);

/// `--include a --include b --exclude vendor` -> filtr sciezek.
class PathFilter;
PathFilter pathFilterFromArgs(const Arguments& args);

/// Naglowek wypisywany przed tabela (nazwa repozytorium, branch, zakres).
void printRepoHeader(const CommandContext& ctx, const std::string& title,
                     const std::string& subtitle = {});

/// Sciezka skracana do szerokosci `--path-width`.
std::string shortenPath(const CommandContext& ctx, const std::string& path);

} // namespace gitlense
