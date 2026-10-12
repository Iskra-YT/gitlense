#include <git2.h>

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "cli/args.h"
#include "cli/progress.h"
#include "commands/command.h"
#include "core/error.h"
#include "core/language.h"
#include "core/libgit2.h"
#include "core/repository.h"
#include "format/color.h"
#include "format/format.h"
#include "format/table.h"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitError = 1;
constexpr int kExitUsage = 2;

/// Opcje, ktore przyjmuja wartosc. Bez tej listy kazda opcja bylaby
/// przelacznikiem, a `gitlense log --top 10` zjadlo by "10" jako argument
/// pozycyjny. Lista jest wspolna dla wszystkich komend.
const std::vector<std::string>& valueOptions()
{
    static const std::vector<std::string> options = {
        // globalne
        "C", "repo", "j", "threads", "date", "path-width", "tab-width",
        // filtry historii
        "limit", "n", "since", "until", "author", "committer", "grep", "path", "include", "exclude",
        "language", "recent", "author-search",
        // selekcja i formatowanie
        "top", "top-authors", "ownership-top", "by", "buckets", "width", "context", "at",
        "merged", "no-merged",
    };
    return options;
}

void printVersion()
{
    std::cout << "gitlense " << GITLENSE_VERSION << " (libgit2 " << LIBGIT2_VERSION << ")\n";
}

void printUsage()
{
    std::cout << "gitlense " << GITLENSE_VERSION
              << " - przegladarka historii i statystyk repozytorium Git\n\n"
              << "Uzycie:\n"
              << "  gitlense [globalne opcje] <komenda> [opcje komendy]\n"
              << "  gitlense help <komenda>\n"
              << "  gitlense <komenda> --help\n\n"
              << "Komendy:\n"
              << gitlense::Registry::instance().overview() << "\n"
              << "Globalne opcje:\n"
              << "  -C, --repo DIR       katalog roboczy (jak git -C)\n"
              << "  -j, --threads N      liczba watkow (0 = tyle, ile rdzeni)\n"
              << "      --date=STYL      local|iso|short|relative|unix|raw:<format>\n"
              << "      --path-width N   szerokosc kolumny sciezki (domyslnie 60)\n"
              << "      --tab-width N    szerokosc tabulatora (domyslnie 4)\n"
              << "      --color          wymus kolory\n"
              << "      --no-color       wylacz kolory\n"
              << "  -q, --quiet          bez komunikatow pomocniczych\n"
              << "  -v, --verbose        wiecej szczegolow\n"
              << "      --no-header      bez naglowka repozytorium\n"
              << "  -V, --version        wersja\n"
              << "  -h, --help           ta pomoc lub pomoc komendy\n\n"
              << "Przyklady:\n"
              << "  gitlense log -n 20 --stat\n"
              << "  gitlense contributors --since=1.year --top 10\n"
              << "  gitlense languages --by-author --top 6\n"
              << "  gitlense hotspots --top 10 --by-author\n"
              << "  gitlense activity --by=month\n"
              << "  gitlense file src/main.cpp --blame --lines\n\n"
              << "Zmienne srodowiskowe:\n"
              << "  NO_COLOR=1           wylacza kolory (jak --no-color)\n";
}

/// Wlasciwa robota - w calosci objeta `try` w `main`, zeby bledna opcja
/// globalna tez dostala czytelny komunikat zamiast wyjatku.
/// `commandName` wypelniamy na zewnatrz, bo trafia do komunikatu bledu.
int run(const std::vector<std::string>& rawArgs, std::string& commandName);

} // namespace

int main(int argc, char** argv)
{
    std::ios::sync_with_stdio(false);
    const std::vector<std::string> rawArgs(argv, argv + argc);
    if (rawArgs.empty()) {
        printUsage();
        return kExitUsage;
    }

    gitlense::registerAllCommands();

    // Nazwa komendy wchodzi w komunikat bledu, wiec zyje wylacznie
    // na zewnatrz bloku try.
    std::string commandName;
    try {
        return run(rawArgs, commandName);
    } catch (const gitlense::UsageError& error) {
        std::cerr << "gitlense";
        if (!commandName.empty()) {
            std::cerr << " " << commandName;
        }
        std::cerr << ": " << error.what() << "\n";
        if (!commandName.empty()) {
            std::cerr << "Uzyj `gitlense help " << commandName << "`, aby zobaczyc opcje.\n";
        }
        return kExitUsage;
    } catch (const std::exception& error) {
        std::cerr << "gitlense: " << error.what() << "\n";
        return kExitError;
    }
}

namespace {

/// Wlasciwa robota - w calosci objeta `try` w `main`, zeby bledna opcja
/// globalna tez dostala czytelny komunikat zamiast wyjatku.
int run(const std::vector<std::string>& rawArgs, std::string& commandName)
{
    // --- globalne opcje -------------------------------------------------
    // Analizujemy caly argv, ale komende bierzemy z tokenow, ktore nie sa
    // opcjami ani ich wartosciami.
    gitlense::Arguments all(rawArgs, rawArgs.front());
    all.declareValueOptions(valueOptions());

    if (all.has("version") || all.flag("V")) {
        printVersion();
        return kExitOk;
    }

    const bool forceColor = all.flag("color");
    const bool forceNoColor = all.flag("no-color") || all.flag("no-colour");
    const bool quiet = all.flag("quiet") || all.flag("q");
    const bool verbose = all.flag("verbose") || all.flag("v");
    const bool showHeader = !all.flag("no-header");
    const std::size_t threads = all.getSize("threads", all.getSize("j", 0));
    const std::size_t pathWidth
        = static_cast<std::size_t>(std::max(10LL, all.getInt("path-width", 60)));
    const std::size_t tabWidth
        = static_cast<std::size_t>(std::max(1LL, all.getInt("tab-width", 4)));
    std::string dateFormat;
    const gitlense::DateStyle dateStyle
        = gitlense::parseDateStyle(all.get("date", "local"), dateFormat);
    std::string repoPath = all.get("repo", all.get("C", "."));

    gitlense::ColorMode::initialize(forceColor, forceNoColor);

    // --- wybor komendy --------------------------------------------------
    std::size_t commandIndex = 0;
    if (!all.positional().empty()) {
        commandName = all.positional().front();
        for (std::size_t i = 1; i < rawArgs.size(); ++i) {
            if (rawArgs[i] == commandName) {
                commandIndex = i;
                break;
            }
        }
    }

    // `gitlense help [komenda]`
    if (commandName == "help" || (all.flag("help") && commandName.empty())) {
        if (all.positional().size() >= 2) {
            const gitlense::Command* target
                = gitlense::Registry::instance().find(all.positional()[1]);
            if (target == nullptr) {
                std::cerr << "gitlense: nieznana komenda \"" << all.positional()[1] << "\"\n";
                return kExitUsage;
            }
            std::cout << gitlense::Registry::helpFor(*target) << "\n";
            return kExitOk;
        }
        printUsage();
        return kExitOk;
    }

    if (all.flag("h") && commandName.empty()) {
        printUsage();
        return kExitOk;
    }

    if (commandName.empty()) {
        printUsage();
        return kExitUsage;
    }

    const gitlense::Command* command = gitlense::Registry::instance().find(commandName);
    if (command == nullptr) {
        std::cerr << "gitlense: nieznana komenda \"" << commandName << "\"\n\n";
        std::cerr << "Dostepne komendy:\n" << gitlense::Registry::instance().overview() << "\n";
        return kExitUsage;
    }

    // Do komendy przekazujemy tylko to, co stoi za nazwa komendy.
    // Opcje globalne sprzed komendy zostaly juz obsluzone powyzej.
    std::vector<std::string> commandArgv;
    commandArgv.push_back(command->name);
    for (std::size_t i = commandIndex + 1; i < rawArgs.size(); ++i) {
        commandArgv.push_back(rawArgs[i]);
    }
    gitlense::Arguments args(commandArgv, command->name);
    args.declareValueOptions(valueOptions());

    if (args.flag("help") || all.flag("h")) {
        std::cout << gitlense::Registry::helpFor(*command) << "\n";
        return kExitOk;
    }

    gitlense::Repository repo(repoPath);

    // .gitattributes moze przypisac jezyki do konkretnych plikow
    // (atrybut linguist-language).
    gitlense::LanguageDetector detector;
    if (repo.hasHead()) {
        try {
            const std::string attributes = repo.fileContent(repo.head(), ".gitattributes");
            if (!attributes.empty()) {
                detector.loadAttributes(attributes);
            }
        } catch (const gitlense::Error&) {
            // brak .gitattributes to nie blad
        }
    }

    const gitlense::CommandContext ctx{repo,
                                       detector,
                                       dateStyle,
                                       dateFormat,
                                       threads,
                                       tabWidth,
                                       pathWidth,
                                       quiet,
                                       verbose,
                                       showHeader};

    return command->run(ctx, args);
}

} // namespace
