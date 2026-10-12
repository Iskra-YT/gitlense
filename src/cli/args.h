#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gitlense {

/// Minimalny parser argumentow w stylu gita.
///
/// Obsluguje:
///   --nazwa=wartosc      --nazwa wartosc      --nazwa (przelacznik)
///   -n 5                 -abc                --  (koniec opcji)
///   powtórzone opcje     --opcja a --opcja b
///
/// Które opcje przyjmują wartość, trzeba zadeklarować przez
/// `declareValueOptions()` - bez tego każda opcja jest przłącznikiem.
/// Parsowanie jest leniwe, więc deklarację można wykonać już po
/// konstrukcji obiektu (nawet na obiekcie `const`).
class Arguments {
public:
    Arguments(const std::vector<std::string>& argv, std::string command = {});

    /// Deklaruje opcje, ktore przyjmuja wartosc (nawet gdy nastepny token
    /// wyglada jak opcja, np. `--grep -fix`).
    void declareValueOptions(std::vector<std::string> names) const;

    [[nodiscard]] bool has(const std::string& name) const;
    [[nodiscard]] bool flag(const std::string& name) const;
    [[nodiscard]] std::string get(const std::string& name,
                                  const std::string& fallback = {}) const;
    [[nodiscard]] std::optional<std::string> getOptional(const std::string& name) const;
    [[nodiscard]] long long getInt(const std::string& name, long long fallback) const;
    [[nodiscard]] std::size_t getSize(const std::string& name, std::size_t fallback) const;
    [[nodiscard]] std::vector<std::string> getAll(const std::string& name) const;

    [[nodiscard]] const std::vector<std::string>& positional() const;
    /// Opcje w kolejnosci podania na linii polecen.
    [[nodiscard]] const std::vector<std::string>& names() const;
    [[nodiscard]] const std::string& command() const { return command_; }

    /// Rzuca UsageError wrazie z podpowiedzia, gdy uzytkownik podal
    /// nieznana opcje.
    void rejectUnknown(const std::vector<std::string>& allowed,
                       const std::string& hint = {}) const;

private:
    void parse() const;
    void store(const std::string& name, std::string value) const;
    [[nodiscard]] bool isValueOption(const std::string& name) const;
    [[nodiscard]] bool looksLikeOption(const std::string& token) const;

    std::vector<std::string> argv_;
    std::string command_;
    // wynik parsowania trzymamy w mutable, bo jest wspoldzielony przez
    // metody `const` (leniwa inicjalizacja)
    mutable bool parsed_{false};
    mutable std::unordered_map<std::string, std::vector<std::string>> values_;
    mutable std::vector<std::string> order_;
    mutable std::vector<std::string> positional_;
    mutable std::vector<std::string> valueOptions_;
};

} // namespace gitlense
