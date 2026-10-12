#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace gitlense {

/// Rodzaje plikow rozpoznawane heurystycznie (bez rozszerzenia
/// lub o dziwnych rozszerzeniach).
enum class FileKind {
    Source,     // kod zrodlowy
    Header,     // naglowki
    Config,     // konfiguracja, manifesty
    Doc,        // dokumentacja
    Data,       // dane, tekst, zasoby
    Image,
    Audio,
    Video,
    Archive,
    Executable, // binarka
    Font,
    Notebook,   // .ipynb
    Lock,       // pliki blokad
    Generated,  // *.min.js, *_pb2.py, *.g.dart itp.
    Vendored,   // third_party/, vendor/, node_modules/
    Other,
};

const char* toString(FileKind kind) noexcept;

/// Rozpoznanie jezyka (w stylu GitHub Linguist), ale bez zaleznosci
/// zewnetrznych: wbudowana tabela + heurystyki + opcjonalnie
/// `.gitattributes` (klucze `linguist-language=`).
class LanguageDetector {
public:
    /// Wczytuje `.gitattributes` z HEAD (jesli istnieje) - nadpisuje
    /// mapowanie dla konkretnych plikow. Blad jest ignorowany.
    void loadAttributes(const std::string& attributesText);

    /// Zwraca nazwe jezyka ("C++", "Python", ...) albo pusty string.
    [[nodiscard]] std::string languageOf(const std::string& path) const;

    /// Pelna klasyfikacja pliku.
    [[nodiscard]] FileKind kindOf(const std::string& path) const;

    /// Czy plik ma sens liczyc linijki (nie jest binarny/obraz/archiwum).
    [[nodiscard]] bool isTextual(const std::string& path) const;

    /// Nazwa jezyka dla wskazanego rozszerzenia, np. "cpp" -> "C++".
    [[nodiscard]] static std::optional<std::string> languageForExtension(const std::string& ext);
    /// Nazwa jezyka dla plikow bez rozszerzenia: Makefile, Dockerfile...
    [[nodiscard]] static std::optional<std::string> languageForFilename(const std::string& filename);

    /// Wszystkie znane jezyki (do naglowkow i sortowania).
    [[nodiscard]] static std::vector<std::string> knownLanguages();

    /// Czy sciezka wyglada na plik z vendowanego kodu.
    [[nodiscard]] static bool isVendoredPath(const std::string& path);
    /// Czy plik jest wygenerowany.
    [[nodiscard]] static bool isGeneratedPath(const std::string& path);

private:
    /// Nadpisania z `.gitattributes` w kolejnosci wystepowania w pliku -
    /// ostatnie pasujace wygrywa, tak jak w git.
    std::vector<std::pair<std::string, std::string>> attributeOverrides_;
};

} // namespace gitlense
