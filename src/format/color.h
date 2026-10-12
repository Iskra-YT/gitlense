#pragma once

#include <string>

namespace gitlense {

/// Kolorowana etykieta z odzyskiwaniem oryginalnego tekstu.
/// Zawsze zwraca czysty tekst w `plain()`, dzieki czemu testy
/// i przekierowanie do pliku dzialaja tak samo jak bez ANSI.
class Style {
public:
    /// Styl pusty - czyli brak kodow SGR. Potrzebny, zeby `Style` dalo sie
    /// inicjalizowac w agregatach (np. `Table::Column{"Tytul", Align::Left}`).
    Style() = default;

    static Style bold();
    static Style dim();
    static Style red();
    static Style green();
    static Style yellow();
    static Style blue();
    static Style magenta();
    static Style cyan();
    static Style boldColor(const std::string& color);
    /// Dowolny kod SGR, np. "36" lub "1;4".
    static Style custom(const std::string& ansiCodes);
    static Style combine(const Style& a, const Style& b);

    [[nodiscard]] std::string wrap(const std::string& text) const;
    [[nodiscard]] bool empty() const { return codes_.empty(); }
    [[nodiscard]] const std::string& codes() const { return codes_; }
    [[nodiscard]] Style& operator+=(const Style& other);

private:
    explicit Style(std::string codes);
    std::string codes_;
};

/// Globalny stan kolorow. Wywolywany raz przy starcie programu.
class ColorMode {
public:
    static void initialize(bool forceColor, bool forceNoColor);
    static bool enabled();
    /// Czy katalog roboczy jest urterminalem (nieprzekierowanie do pliku).
    static bool stdoutIsTerminal();
};

/// Usuwa sekwencje ANSI (uzywane przy `--no-color` na danych wejsciowych
/// oraz w testach).
std::string stripAnsi(const std::string& text);

} // namespace gitlense
