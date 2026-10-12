#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/types.h"

namespace gitlense {

/// "1 234 567" (cienkie spacje, jak w `git shortlog -n`).
std::string number(std::int64_t value);
std::string number(std::uint64_t value);
/// Procent z jednym miejscem po przecinku: "42.3%".
std::string percent(double fraction, int precision = 1);
/// "42%" - zaokraglenie do calosci.
std::string percentWhole(double fraction);
/// Rozmiar pliku w wygodnych jednostkach.
std::string bytes(std::uint64_t value);

/// Parsuje `--date=local|iso|short|relative|raw:<strftime>`.
enum class DateStyle {
    Local,     // 2026-09-28 14:03
    Short,     // 28.09.2026 (w zaleznosci od lokalizacji: 2026-09-28)
    Iso,       // 2026-09-28 14:03:21 +0200
    Relative,  // 3 dni temu
    Raw,       // wlasny format strftime
    Unix,      // 1789934567
};

DateStyle parseDateStyle(const std::string& value, std::string& rawFormat);
std::string formatDate(Timestamp when, DateStyle style, const std::string& rawFormat = {});
/// Parsuje `--since`/`--until`: "2 weeks ago", "3 days", "2026-01-01",
/// "2026-01-01 12:00:00", "1735689600" (epoch).
Timestamp parseTimestamp(const std::string& value, bool endOfDay = false);

/// Poczatek okresu w czasie lokalnym.
Timestamp startOfDay(Timestamp ts);
Timestamp startOfWeek(Timestamp ts);
Timestamp startOfMonth(Timestamp ts);
Timestamp startOfYear(Timestamp ts);
std::string monthLabel(Timestamp ts);
std::string yearLabel(Timestamp ts);
/// Nazwy dni tygodnia (od poniedzialku).
std::string weekdayLabel(Timestamp ts);

/// Polska odmiana rzeczownika po liczebniku, np.
/// `plural(1, "plik", "pliki", "plikow")` -> "1 plik".
std::string plural(std::size_t count, const char* one, const char* few, const char* many);

/// Aktualny czas (ukryty w jednym miejscu dla testow).
Timestamp now();

/// Liczba znakow widocznych: bez kodow ANSI, a UTF-8 liczony po znakach
/// (nie bajtach) - inaczej polskie znaki rozpychaja kolumny tabel.
std::size_t displayWidth(const std::string& text);

} // namespace gitlense
