#include "framework.h"

#include "core/error.h"
#include "format/format.h"

#include <ctime>
#include <string>

using namespace gitlense;

namespace {

/// Zamienia znacznik czasu na strukture tm w strefie lokalnej.
std::tm toLocal(Timestamp ts)
{
    const std::time_t raw = static_cast<std::time_t>(ts);
    std::tm result{};
    ::localtime_r(&raw, &result);
    return result;
}

} // namespace

TEST(number_grupuje_cyfry_cienka_spacja)
{
    CHECK_EQ(number(static_cast<std::int64_t>(0)), std::string("0"));
    CHECK_EQ(number(static_cast<std::int64_t>(7)), std::string("7"));
    CHECK_EQ(number(static_cast<std::int64_t>(999)), std::string("999"));
    CHECK_EQ(number(static_cast<std::int64_t>(1000)), std::string("1\u2009000"));
    CHECK_EQ(number(static_cast<std::int64_t>(1234567)), std::string("1\u2009234\u2009567"));
    CHECK_EQ(number(static_cast<std::int64_t>(-1500)), std::string("-1\u2009500"));
    // separator to wcieza spacja, nie zwykla
    CHECK_EQ(number(static_cast<std::int64_t>(1234567)).find(' '), std::string::npos);
    CHECK_EQ(number(static_cast<std::int64_t>(1234567)).find("\u2009") != std::string::npos, true);
}

TEST(percent_zwraca_znak_procenta)
{
    CHECK_EQ(percent(0.0), std::string("0.0%"));
    CHECK_EQ(percent(1.0), std::string("100.0%"));
    CHECK_EQ(percent(0.5, 0), std::string("50%"));
    CHECK_EQ(percent(1.0 / 3.0), std::string("33.3%"));
    CHECK_EQ(percent(2.0 / 3.0), std::string("66.7%"));
    CHECK_EQ(percentWhole(0.5), std::string("50%"));
    CHECK_EQ(percentWhole(1.0), std::string("100%"));
}

TEST(bytes_uzywa_jednostek_dwochpotegowych)
{
    CHECK_EQ(bytes(0), std::string("0 B"));
    CHECK_EQ(bytes(512), std::string("512 B"));
    CHECK_EQ(bytes(1024), std::string("1.0 kB"));
    CHECK_EQ(bytes(1536), std::string("1.5 kB"));
    CHECK_EQ(bytes(1024 * 1024), std::string("1.0 MB"));
    CHECK_EQ(bytes(static_cast<std::uint64_t>(1024) * 1024 * 1024), std::string("1.0 GB"));
}

TEST(parse_date_style_rozpoznaje_warianty)
{
    std::string format;
    CHECK(parseDateStyle("local", format) == DateStyle::Local);
    CHECK(parseDateStyle("iso", format) == DateStyle::Iso);
    CHECK(parseDateStyle("short", format) == DateStyle::Short);
    CHECK(parseDateStyle("relative", format) == DateStyle::Relative);
    CHECK(parseDateStyle("unix", format) == DateStyle::Unix);
    CHECK(parseDateStyle("raw:%Y-%m", format) == DateStyle::Raw);
    CHECK_EQ(format, std::string("%Y-%m"));
}

TEST(parse_date_style_zwraca_usage_error_dla_nieznanej)
{
    std::string format;
    bool thrown = false;
    try {
        parseDateStyle("nieznany", format);
    } catch (const UsageError&) {
        thrown = true;
    }
    CHECK(thrown);
}

TEST(parse_date_style_odrzuca_pusty_raw)
{
    std::string format;
    bool thrown = false;
    try {
        parseDateStyle("raw:", format);
    } catch (const UsageError&) {
        thrown = true;
    }
    CHECK(thrown);
}

TEST(start_of_day_obcina_godzine)
{
    const Timestamp day = startOfDay(1'700'000'000);
    const std::tm tm = toLocal(day);
    CHECK_EQ(tm.tm_hour, 0);
    CHECK_EQ(tm.tm_min, 0);
    CHECK_EQ(tm.tm_sec, 0);
    CHECK_EQ(startOfDay(day), day);
}

TEST(start_of_month_pada_na_pierwszy_dzien)
{
    const std::tm tm = toLocal(startOfMonth(1'700'000'000));
    CHECK_EQ(tm.tm_mday, 1);
    CHECK_EQ(tm.tm_hour, 0);
}

TEST(start_of_year_pada_na_pierwszy_stycznia)
{
    const std::tm tm = toLocal(startOfYear(1'700'000'000));
    CHECK_EQ(tm.tm_mon, 0);
    CHECK_EQ(tm.tm_mday, 1);
    CHECK_EQ(tm.tm_hour, 0);
}

TEST(okresy_nestegaja_sie_rosnaco)
{
    const Timestamp ts = 1'700'000'000;
    CHECK(startOfYear(ts) <= startOfMonth(ts));
    CHECK(startOfMonth(ts) <= startOfWeek(ts));
    CHECK(startOfWeek(ts) <= startOfDay(ts));
    CHECK(startOfDay(ts) <= ts);
}

TEST(start_of_week_wpada_w_poniedzialek)
{
    // sprawdzamy wszystkie dni tygodnia
    for (int offset = 0; offset < 7; ++offset) {
        const Timestamp day = startOfDay(1'700'000'000) + offset * 86400;
        const std::tm tm = toLocal(startOfWeek(day));
        CHECK_EQ(tm.tm_wday, 1); // poniedzialek
    }
}

TEST(etykieta_miesiaca_ma_miesiac_i_rok)
{
    const std::string label = monthLabel(1'700'000'000);
    // "lis 2023"
    CHECK_EQ(label.size(), static_cast<std::size_t>(8));
    CHECK_EQ(label.substr(0, 3), std::string("lis"));
    CHECK_EQ(label.substr(4), std::string("2023"));
}

TEST(etykieta_roku_to_rok)
{
    CHECK_EQ(yearLabel(1'700'000'000), std::string("2023"));
}

TEST(weekday_label_zwraca_skrot_dnia)
{
    static const char* const expected[]
        = {"pon", "wt", "sr", "czw", "pt", "sob", "nd"};
    for (int offset = 0; offset < 7; ++offset) {
        const Timestamp day = startOfDay(1'700'000'000) + offset * 86400;
        const int wday = toLocal(day).tm_wday;
        CHECK_EQ(weekdayLabel(day), std::string(expected[wday]));
    }
}

TEST(format_date_local_ma_date_i_godzine)
{
    const std::string text = formatDate(1'700'000'000, DateStyle::Local);
    CHECK_EQ(text.size(), static_cast<std::size_t>(16));
    CHECK_EQ(text.substr(0, 4), std::string("2023"));
    CHECK_EQ(text[4], '-');
    CHECK_EQ(text[10], ' ');
}

TEST(format_date_iso_dodaje_strefe_czasu)
{
    const std::string text = formatDate(1'700'000'000, DateStyle::Iso);
    CHECK_EQ(text.size(), static_cast<std::size_t>(25));
    CHECK_EQ(text.substr(10, 1), std::string(" "));   // data -> godzina
    CHECK_EQ(text.substr(19, 1), std::string(" "));   // godzina -> strefa
    CHECK((text[20] == '+' || text[20] == '-'));
}

TEST(format_date_short_uzywa_kropek)
{
    // "14.11.2023" - polska konwencja, dzien przed miesiacem
    const std::string text = formatDate(1'700'000'000, DateStyle::Short);
    CHECK_EQ(text.size(), static_cast<std::size_t>(10));
    CHECK_EQ(text.substr(0, 2), std::string("14"));
    CHECK_EQ(text[2], '.');
    CHECK_EQ(text.substr(3, 2), std::string("11"));
    CHECK_EQ(text[5], '.');
    CHECK_EQ(text.substr(6), std::string("2023"));
}

TEST(format_date_unix_to_liczba)
{
    CHECK_EQ(formatDate(1'700'000'000, DateStyle::Unix), std::string("1700000000"));
}

TEST(format_date_raw_uzywa_formatu)
{
    CHECK_EQ(formatDate(1'700'000'000, DateStyle::Raw, "%Y"), std::string("2023"));
}

TEST(format_date_dla_zera_zwraca_myślnik)
{
    CHECK_EQ(formatDate(0, DateStyle::Local), std::string("-"));
    CHECK_EQ(formatDate(-5, DateStyle::Relative), std::string("-"));
}

TEST(format_date_relative_uzywa_polskich_zlecen)
{
    const Timestamp reference = now();
    // wczoraj / dzisiaj / jutro
    CHECK_EQ(formatDate(reference - 86400, DateStyle::Relative), std::string("wczoraj"));
    CHECK_EQ(formatDate(startOfDay(reference) + 12 * 3600, DateStyle::Relative),
             std::string("dzisiaj"));
    // 3 dni temu
    const std::string threeDays = formatDate(reference - 3 * 86400, DateStyle::Relative);
    CHECK_EQ(threeDays, std::string("3 dni temu"));
    // 2 tygodnie temu
    const std::string twoWeeks = formatDate(reference - 14 * 86400, DateStyle::Relative);
    CHECK_EQ(twoWeeks, std::string("2 tygodnie temu"));
    // 2 miesiace temu
    const std::string twoMonths = formatDate(reference - 60 * 86400, DateStyle::Relative);
    CHECK_EQ(twoMonths, std::string("2 miesiące temu"));
    // 2 lata temu
    const std::string twoYears = formatDate(reference - 730 * 86400, DateStyle::Relative);
    CHECK_EQ(twoYears, std::string("2 lata temu"));
}

TEST(format_date_relative_jeden_rok)
{
    const std::string year = formatDate(now() - 400 * 86400, DateStyle::Relative);
    CHECK_EQ(year, std::string("1 rok temu"));
}

TEST(format_date_relative_jeden_dzien_po_polu_nocy)
{
    const Timestamp midday = startOfDay(now()) + 12 * 3600;
    CHECK_EQ(formatDate(midday, DateStyle::Relative), std::string("dzisiaj"));
    CHECK_EQ(formatDate(midday - 86400, DateStyle::Relative), std::string("wczoraj"));
    CHECK_EQ(formatDate(midday + 86400, DateStyle::Relative), std::string("jutro"));
}

TEST(parse_timestamp_odczytuje_epoke)
{
    CHECK_EQ(parseTimestamp("1700000000"), static_cast<Timestamp>(1700000000));
}

TEST(parse_timestamp_odczytuje_date_iso)
{
    CHECK_EQ(parseTimestamp("2023-11-14"), parseTimestamp("2023-11-14 00:00:00"));
    CHECK_EQ(toLocal(parseTimestamp("2023-11-14")).tm_mday, 14);
    CHECK_EQ(toLocal(parseTimestamp("2023-11-14")).tm_mon, 10);
}

TEST(parse_timestamp_odczytuje_date_z_godzina)
{
    const Timestamp ts = parseTimestamp("2023-11-14 13:45:00");
    const std::tm tm = toLocal(ts);
    CHECK_EQ(tm.tm_hour, 13);
    CHECK_EQ(tm.tm_min, 45);
}

TEST(parse_timestamp_czytuje_zdania_po_angielsku)
{
    const Timestamp twoWeeksAgo = parseTimestamp("2 weeks ago");
    const Timestamp yesterday = parseTimestamp("yesterday");
    const Timestamp threeDays = parseTimestamp("3 days");
    CHECK(twoWeeksAgo > 0);
    CHECK(yesterday > 0);
    CHECK(twoWeeksAgo < yesterday);
    CHECK(threeDays < yesterday);
}

TEST(parse_timestamp_czytuje_zapis_polski)
{
    const Timestamp weeks = parseTimestamp("2 tygodnie temu");
    const Timestamp days = parseTimestamp("3 dni");
    CHECK(weeks > 0);
    CHECK(days > 0);
    CHECK(weeks < days);
}

TEST(parse_timestamp_rozumie_slowa_kalendarzowe)
{
    const Timestamp today = parseTimestamp("dzisiaj");
    const Timestamp yesterday = parseTimestamp("wczoraj");
    const Timestamp tomorrow = parseTimestamp("jutro");
    CHECK_EQ(today, startOfDay(now()));
    CHECK_EQ(yesterday, today - 86400);
    CHECK_EQ(tomorrow, today + 86400);
}

TEST(parse_timestamp_zwraca_usage_error_dla_smieci)
{
    bool thrown = false;
    try {
        parseTimestamp("nie-data");
    } catch (const UsageError&) {
        thrown = true;
    }
    CHECK(thrown);
}

TEST(parse_timestamp_zwraca_usage_error_dla_nieznanej_jednostki)
{
    bool thrown = false;
    try {
        parseTimestamp("5 fortni");
    } catch (const UsageError&) {
        thrown = true;
    }
    CHECK(thrown);
}

TEST(parse_timestamp_zwraca_usage_error_dla_pustego)
{
    bool thrown = false;
    try {
        parseTimestamp("   ");
    } catch (const UsageError&) {
        thrown = true;
    }
    CHECK(thrown);
}

TEST(end_of_day_dla_until_przesuwa_na_koniec_dnia)
{
    const Timestamp midnight = parseTimestamp("2023-11-14");
    const Timestamp endOfDay = parseTimestamp("2023-11-14", true);
    CHECK_EQ(endOfDay - midnight, static_cast<Timestamp>(86399));
}
