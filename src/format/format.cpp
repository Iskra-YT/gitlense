#include "format/format.h"

#include "core/error.h"
#include "format/color.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>

namespace gitlense {
namespace {

std::string trimSpaces(const std::string& value)
{
    const auto notSpace = [](unsigned char c) { return std::isspace(c) == 0; };
    auto begin = std::find_if(value.begin(), value.end(), notSpace);
    auto end = std::find_if(value.rbegin(), value.rend(), notSpace).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool isAllDigits(const std::string& value)
{
    return !value.empty()
        && std::all_of(value.begin(), value.end(),
                       [](unsigned char c) { return std::isdigit(c) != 0; });
}

/// Polska odmiana rzeczownika po liczebniku: 1 / 2-4 / 5+.
std::string polishPlural(long long count, const char* one, const char* few, const char* many)
{
    if (count == 1) {
        return one;
    }
    const long long lastTwo = count % 100;
    if (lastTwo < 12 || lastTwo > 14) {
        const long long last = count % 10;
        if (last >= 2 && last <= 4) {
            return few;
        }
    }
    return many;
}

/// "3 dni", "2 tygodnie", "1 rok" - okres w czystym, bezokolicznikowym zapisie.
std::string periodText(long long seconds)
{
    struct Unit {
        long long size;
        const char* one;
        const char* few;
        const char* many;
    };
    // malejaco, wiec pierwsza pasujaca jednostka wygrywa
    static const Unit kUnits[] = {
        {365LL * 86400, "rok", "lata", "lat"},
        {30LL * 86400, "miesiąc", "miesiące", "miesięcy"},
        {7LL * 86400, "tydzień", "tygodnie", "tygodni"},
        {86400, "dzień", "dni", "dni"},
        {3600, "godzinę", "godziny", "godzin"},
        {60, "minutę", "minuty", "minut"},
    };
    for (const Unit& unit : kUnits) {
        if (seconds >= unit.size) {
            // zaokraglamy w dol do polowki jednostki ("11 miesięcy", nie "12")
            const long long count = (seconds + unit.size / 2) / unit.size;
            return std::to_string(count) + " "
                + polishPlural(count, unit.one, unit.few, unit.many);
        }
    }
    if (seconds >= 1) {
        return std::to_string(seconds) + " s";
    }
    return "0 s";
}

} // namespace

std::string plural(std::size_t count, const char* one, const char* few, const char* many)
{
    return std::to_string(count) + " " + polishPlural(static_cast<long long>(count), one, few, many);
}

Timestamp now()
{
    return static_cast<Timestamp>(std::time(nullptr));
}

std::size_t displayWidth(const std::string& text)
{
    const std::string plain = stripAnsi(text);
    std::size_t width = 0;
    for (const char c : plain) {
        // bajty kontynuacji UTF-8 (10xxxxxx) nie tworza nowego znaku
        if ((static_cast<unsigned char>(c) & 0xC0u) != 0x80u) {
            ++width;
        }
    }
    return width;
}

std::string number(std::int64_t value)
{
    const bool negative = value < 0;
    std::string digits = std::to_string(negative ? -value : value);

    // Grupowanie co 3 cyfry separatorem (spacja wg konwencji CLDR dla pl).
    const std::string separator = "\u2009"; // thin space
    std::string grouped;
    const std::size_t count = digits.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (i != 0 && (count - i) % 3 == 0) {
            grouped += separator;
        }
        grouped += digits[i];
    }
    return negative ? "-" + grouped : grouped;
}

std::string number(std::uint64_t value)
{
    return number(static_cast<std::int64_t>(value));
}

std::string percent(double fraction, int precision)
{
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.*f%%", precision, fraction * 100.0);
    return buffer;
}

std::string percentWhole(double fraction)
{
    return percent(fraction, 0);
}

std::string bytes(std::uint64_t value)
{
    static const char* units[] = {"B", "kB", "MB", "GB", "TB", "PB"};
    double size = static_cast<double>(value);
    std::size_t unit = 0;
    while (size >= 1024.0 && unit + 1 < sizeof(units) / sizeof(units[0])) {
        size /= 1024.0;
        ++unit;
    }
    char buffer[32];
    if (unit == 0) {
        std::snprintf(buffer, sizeof buffer, "%llu %s",
                      static_cast<unsigned long long>(value), units[0]);
    } else {
        std::snprintf(buffer, sizeof buffer, "%.1f %s", size, units[unit]);
    }
    return buffer;
}

DateStyle parseDateStyle(const std::string& value, std::string& rawFormat)
{
    rawFormat.clear();
    const std::string lowerValue = lower(value);
    if (lowerValue == "local" || lowerValue == "default") {
        return DateStyle::Local;
    }
    if (lowerValue == "iso" || lowerValue == "iso8601" || lowerValue == "iso-strict") {
        return DateStyle::Iso;
    }
    if (lowerValue == "short") {
        return DateStyle::Short;
    }
    if (lowerValue == "relative") {
        return DateStyle::Relative;
    }
    if (lowerValue == "unix") {
        return DateStyle::Unix;
    }
    if (lowerValue.rfind("raw:", 0) == 0) {
        rawFormat = value.substr(4);
        if (rawFormat.empty()) {
            throw UsageError("--date=raw: wymaga formatu, np. --date=raw:%d.%m.%Y");
        }
        return DateStyle::Raw;
    }
    if (lowerValue.rfind("format:", 0) == 0) {
        rawFormat = value.substr(7);
        return DateStyle::Raw;
    }
    throw UsageError("nieznany format daty \"" + value
                     + "\" (uzyj: local, short, iso, relative, unix, raw:<strftime>)");
}

std::string formatDate(Timestamp when, DateStyle style, const std::string& rawFormat)
{
    if (when <= 0) {
        return "-";
    }
    char buffer[128];
    const std::time_t time = static_cast<std::time_t>(when);

    switch (style) {
    case DateStyle::Unix:
        std::snprintf(buffer, sizeof buffer, "%lld", static_cast<long long>(when));
        return buffer;
    case DateStyle::Local: {
        std::tm tm{};
        ::localtime_r(&time, &tm);
        std::strftime(buffer, sizeof buffer, "%Y-%m-%d %H:%M", &tm);
        return buffer;
    }
    case DateStyle::Short: {
        std::tm tm{};
        ::localtime_r(&time, &tm);
        std::strftime(buffer, sizeof buffer, "%d.%m.%Y", &tm);
        return buffer;
    }
    case DateStyle::Iso: {
        std::tm tm{};
        ::localtime_r(&time, &tm);
        char offset[8] = "+0000";
        std::strftime(offset, sizeof offset, "%z", &tm);
        std::strftime(buffer, sizeof buffer, "%Y-%m-%d %H:%M:%S", &tm);
        return std::string(buffer) + " " + offset;
    }
    case DateStyle::Raw: {
        std::tm tm{};
        ::localtime_r(&time, &tm);
        const std::size_t written = std::strftime(buffer, sizeof buffer, rawFormat.c_str(), &tm);
        if (written == 0) {
            return formatDate(when, DateStyle::Local);
        }
        return buffer;
    }
    case DateStyle::Relative:
        break;
    }

    // --- czas przyszly ----------------------------------------------------
    const Timestamp today = startOfDay(now());
    if (when >= today + 86400) {
        if (when < today + 2 * 86400) {
            return "jutro";
        }
        return "za " + periodText(static_cast<long long>(when - now()));
    }
    // --- dziś, wczoraj, przedwczoraj --------------------------------------
    if (when >= today) {
        return "dzisiaj";
    }
    if (when >= today - 86400) {
        return "wczoraj";
    }
    if (when >= today - 2 * 86400) {
        return "przedwczoraj";
    }
    return periodText(static_cast<long long>(now() - when)) + " temu";
}

Timestamp startOfDay(Timestamp ts)
{
    const std::time_t time = static_cast<std::time_t>(ts);
    std::tm tm{};
    ::localtime_r(&time, &tm);
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    return static_cast<Timestamp>(std::mktime(&tm));
}

Timestamp startOfWeek(Timestamp ts)
{
    const std::time_t time = static_cast<std::time_t>(startOfDay(ts));
    std::tm tm{};
    ::localtime_r(&time, &tm);
    // tydzien zaczyna sie w poniedzialek
    const int offset = (tm.tm_wday + 6) % 7;
    tm.tm_mday -= offset;
    tm.tm_isdst = -1;
    return static_cast<Timestamp>(std::mktime(&tm));
}

Timestamp startOfMonth(Timestamp ts)
{
    const std::time_t time = static_cast<std::time_t>(startOfDay(ts));
    std::tm tm{};
    ::localtime_r(&time, &tm);
    tm.tm_mday = 1;
    tm.tm_isdst = -1;
    return static_cast<Timestamp>(std::mktime(&tm));
}

Timestamp startOfYear(Timestamp ts)
{
    const std::time_t time = static_cast<std::time_t>(startOfDay(ts));
    std::tm tm{};
    ::localtime_r(&time, &tm);
    tm.tm_mon = 0;
    tm.tm_mday = 1;
    tm.tm_isdst = -1;
    return static_cast<Timestamp>(std::mktime(&tm));
}

std::string monthLabel(Timestamp ts)
{
    static const char* months[] = {"sty", "lut", "mar", "kwi", "maj", "cze",
                                   "lip", "sie", "wrz", "paz", "lis", "gru"};
    std::tm tm{};
    const std::time_t time = static_cast<std::time_t>(ts);
    ::localtime_r(&time, &tm);
    return std::string(months[tm.tm_mon % 12]) + " " + std::to_string(1900 + tm.tm_year);
}

std::string yearLabel(Timestamp ts)
{
    std::tm tm{};
    const std::time_t time = static_cast<std::time_t>(ts);
    ::localtime_r(&time, &tm);
    return std::to_string(1900 + tm.tm_year);
}

std::string weekdayLabel(Timestamp ts)
{
    static const char* days[] = {"pon", "wt", "sr", "czw", "pt", "sob", "nd"};
    std::tm tm{};
    const std::time_t time = static_cast<std::time_t>(ts);
    ::localtime_r(&time, &tm);
    return days[tm.tm_wday % 7];
}

Timestamp parseTimestamp(const std::string& value, bool endOfDay)
{
    const std::string text = trimSpaces(value);
    if (text.empty()) {
        throw UsageError("pusta data");
    }
    const std::string normalized = lower(text);

    if (normalized == "now") {
        return now();
    }
    if (normalized == "today" || normalized == "dzisiaj") {
        return startOfDay(now());
    }
    if (normalized == "yesterday" || normalized == "wczoraj") {
        return startOfDay(now()) - 86400;
    }
    if (normalized == "tomorrow" || normalized == "jutro") {
        return startOfDay(now()) + 86400;
    }

    // znacznik czasu gita (YYYYMMDDHHMMSS) - 14 cyfr, wiec sprawdzamy go
    // zanim uznamy liczbe za epoke
    if (normalized.size() == 14 && isAllDigits(normalized)) {
        std::tm tm{};
        std::sscanf(normalized.c_str(), "%4d%2d%2d%2d%2d%2d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                    &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        return static_cast<Timestamp>(std::mktime(&tm));
    }

    // sama liczba: 8+ cyfr to epoka (Unix), krotsza to "tyle sekund temu"
    if (isAllDigits(normalized)) {
        if (normalized.size() >= 8) {
            return static_cast<Timestamp>(std::stoll(normalized));
        }
        return now() - static_cast<Timestamp>(std::stoll(normalized));
    }


    // ISO: YYYY-MM-DD[ HH:MM[:SS]]
    {
        std::tm tm{};
        std::memset(&tm, 0, sizeof tm);
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;
        int consumed = 0;
        if (std::sscanf(normalized.c_str(), "%d-%d-%d %d:%d:%d%n", &year, &month, &day, &hour,
                        &minute, &second, &consumed)
                >= 3
            && consumed == static_cast<int>(normalized.size())) {
            tm.tm_year = year - 1900;
            tm.tm_mon = month - 1;
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = minute;
            tm.tm_sec = second;
            if (hour == 0 && minute == 0 && second == 0 && endOfDay) {
                tm.tm_hour = 23;
                tm.tm_min = 59;
                tm.tm_sec = 59;
            }
            const Timestamp result = static_cast<Timestamp>(std::mktime(&tm));
            if (result == static_cast<Timestamp>(-1)) {
                throw UsageError("nie moge sparsowac daty \"" + text + "\"");
            }
            return result;
        }
        std::memset(&tm, 0, sizeof tm);
        if (std::sscanf(normalized.c_str(), "%d/%d/%d%n", &day, &month, &year, &consumed) == 3
            && consumed == static_cast<int>(normalized.size())) {
            tm.tm_year = year - 1900;
            tm.tm_mon = month - 1;
            tm.tm_mday = day;
            if (endOfDay) {
                tm.tm_hour = 23;
                tm.tm_min = 59;
                tm.tm_sec = 59;
            }
            return static_cast<Timestamp>(std::mktime(&tm));
        }
        std::memset(&tm, 0, sizeof tm);
        if (std::sscanf(normalized.c_str(), "%d-%d-%d%n", &year, &month, &day, &consumed) == 3
            && consumed == static_cast<int>(normalized.size())) {
            tm.tm_year = year - 1900;
            tm.tm_mon = month - 1;
            tm.tm_mday = day;
            if (endOfDay) {
                tm.tm_hour = 23;
                tm.tm_min = 59;
                tm.tm_sec = 59;
            }
            return static_cast<Timestamp>(std::mktime(&tm));
        }
    }

    // "3d", "3 days", "2 weeks ago", "2 tygodnie temu"
    {
        std::size_t i = 0;
        while (i < normalized.size() && std::isdigit(static_cast<unsigned char>(normalized[i]))) {
            ++i;
        }
        const std::string amount = normalized.substr(0, i);
        std::string unit = trimSpaces(normalized.substr(i));
        // odcinamy końcówki "ago" / "temu" ("2 tygodnie temu")
        for (const char* suffix : {" ago", " temu", " przed"}) {
            const std::string text(suffix);
            if (unit.size() > text.size() && unit.ends_with(text)) {
                unit = trimSpaces(unit.substr(0, unit.size() - text.size()));
                break;
            }
        }
        if (!amount.empty() && isAllDigits(amount)) {
            const std::int64_t count = std::stoll(amount);
            Timestamp base = now();
            const std::string u = unit;
            if (u.empty() || u == "s" || u == "sec" || u == "sek" || u == "seconds"
                || u == "second" || u == "second ago") {
                base -= count;
            } else if (u == "m" || u == "min" || u == "minut" || u == "minutes" || u == "minute") {
                base -= count * 60;
            } else if (u == "h" || u == "hour" || u == "godzin" || u == "godziny"
                       || u == "godz" || u == "hours") {
                base -= count * 3600;
            } else if (u == "d" || u == "day" || u == "days" || u == "dzien" || u == "dni") {
                base -= count * 86400;
            } else if (u == "w" || u == "week" || u == "weeks" || u == "tydzien"
                       || u == "tygodni" || u == "tygodnie" || u == "tygodnia") {
                base -= count * 7 * 86400;
            } else if (u == "mies" || u == "miesiac" || u == "miesiace" || u == "month"
                       || u == "months") {
                base -= count * 30 * 86400;
            } else if (u == "y" || u == "year" || u == "years" || u == "rok" || u == "lata"
                       || u == "lat") {
                base -= count * 365 * 86400;
            } else {
                throw UsageError("nie rozpoznaje jednostki czasu \"" + unit
                                 + "\" w \"" + text
                                 + "\" (np. 2 weeks ago, 30 days, 2026-01-01)");
            }
            return base;
        }
    }

    throw UsageError("nie moge zrozumiec daty \"" + text
                     + "\" (uzyj np. 2026-01-01, \"2 weeks ago\", 30 days, now)");
}

} // namespace gitlense
