#include "framework.h"

#include "format/chart.h"
#include "format/color.h"
#include "format/format.h"
#include "format/table.h"

#include <string>
#include <vector>

using namespace gitlense;

namespace {

/// Liczba znakow widocznych - ramki i polskie znaki to wielobajtowy UTF-8,
/// wiec `size()` na surowym napisie bywa 2-3 razy za duze.
std::size_t width(const std::string& text)
{
    return displayWidth(text);
}

/// Kody znakow UTF-8, zeby porownywac wysokosci blokow (nie bajty).
std::vector<std::uint32_t> codePoints(const std::string& text)
{
    std::vector<std::uint32_t> out;
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        std::size_t length = 1;
        if ((lead & 0xF0u) == 0xE0u) {
            length = 3;
        } else if ((lead & 0xE0u) == 0xC0u) {
            length = 2;
        }
        std::uint32_t value = 0;
        for (std::size_t k = 0; k < length && i + k < text.size(); ++k) {
            value = (value << 8) | static_cast<unsigned char>(text[i + k]);
        }
        out.push_back(value);
        i += length;
    }
    return out;
}

std::vector<std::string> lines(const std::string& text)
{
    std::vector<std::string> out;
    std::string current;
    for (const char c : text) {
        if (c == '\n') {
            out.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    out.push_back(current);
    return out;
}

} // namespace

TEST(styl_nadaje_kody_ansi)
{
    const std::string styled = Style::red().wrap("tekst");
    CHECK(styled != "tekst");
    CHECK(styled.find("tekst") != std::string::npos);
    CHECK_EQ(stripAnsi(styled), std::string("tekst"));
}

TEST(styl_laczy_kody)
{
    const Style combined = Style::combine(Style::bold(), Style::red());
    const std::string styled = combined.wrap("x");
    CHECK_EQ(stripAnsi(styled), std::string("x"));
    CHECK(styled.size() > 1);
}

TEST(styl_custom_przyjmuje_kody)
{
    const Style custom = Style::custom("36");
    const std::string styled = custom.wrap("x");
    CHECK(styled.find("\u001b[36m") != std::string::npos);
    CHECK_EQ(stripAnsi(styled), std::string("x"));
}

TEST(strip_ansi_zdejmuje_wszystkie_kody)
{
    const std::string text = Style::boldColor("green").wrap(Style::dim().wrap("abc"));
    CHECK_EQ(stripAnsi(text), std::string("abc"));
    CHECK_EQ(stripAnsi(""), std::string(""));
    CHECK_EQ(stripAnsi("brak"), std::string("brak"));
}

TEST(ellipsize_skraca_z_przodu)
{
    CHECK_EQ(ellipsize("abc", 10), std::string("abc"));
    CHECK_EQ(ellipsize("abcdefghij", 5), std::string("ab..."));
    CHECK_EQ(ellipsize("abcdef", 0), std::string("abcdef"));
    CHECK_EQ(ellipsize("abcdef", 2), std::string("ab"));
}

TEST(middle_ellipsize_zachowuje_nazwę_pliku)
{
    CHECK_EQ(middleEllipsize("a/b/c.cpp", 100), std::string("a/b/c.cpp"));
    const std::string short1 = middleEllipsize("src/core/repository.cpp", 16);
    CHECK_EQ(short1.size(), static_cast<std::size_t>(16));
    CHECK(short1.find("...") != std::string::npos);
    // nazwa pliku jest zachowana
    CHECK(short1.find("cpp") != std::string::npos);
    CHECK_EQ(middleEllipsize("abcdef", 3), std::string("abc"));
}

TEST(tabela_renderuje_naglowek_i_dane)
{
    Table table({{"A", Align::Left, {}}, {"B", Align::Right, {}}});
    table.addRow({"x", "1"});
    table.addRow({"yyyy", "22"});
    const std::string text = stripAnsi(table.render());
    const std::vector<std::string> rows = lines(text);
    // gora, naglowek, srodkowa linia, 2 wiersze, dol = 6
    CHECK_EQ(rows.size(), static_cast<std::size_t>(6));
    CHECK(rows[1].find("A") != std::string::npos);
    CHECK(rows[1].find("B") != std::string::npos);
    CHECK(rows[3].find("x") != std::string::npos);
    CHECK(rows[4].find("yyyy") != std::string::npos);
}

TEST(tabela_dopasowuje_szerokosc_do_najdluzszej_komorki)
{
    Table table({{"KOL", Align::Left, {}}});
    table.addRow({"a"});
    table.addRow({"abcdefghij"});
    const std::vector<std::string> rows = lines(stripAnsi(table.render()));
    // wszystkie wiersze maja identyczna szerokosc (w znakach widocznych)
    const std::size_t expected = width(rows[1]);
    for (const std::string& row : rows) {
        CHECK_EQ(width(row), expected);
    }
}

TEST(tabela_liczy_szerokosc_bez_kolorow)
{
    Table plain({{"KOL", Align::Left, {}}});
    plain.addRow({"ab"});
    Table colored({{"KOL", Align::Left, {}}});
    colored.addRow({Style::red().wrap("ab")});
    CHECK_EQ(width(plain.render()), width(colored.render()));
}

TEST(tabela_akceptuje_mniej_komorek_niz_kolumn)
{
    Table table({{"A", Align::Left, {}}, {"B", Align::Left, {}}, {"C", Align::Left, {}}});
    table.addRow({"tylko jedna"});
    const std::string text = stripAnsi(table.render());
    CHECK(text.find("tylko jedna") != std::string::npos);
}

TEST(tabela_nie_rozpycha_polskich_znakow)
{
    Table table({{"A", Align::Left, {}}, {"B", Align::Left, {}}});
    table.addRow({"zażółć", "gęślą jaźń"});
    table.addRow({"abc", "def"});
    const std::vector<std::string> rows = lines(stripAnsi(table.render()));
    const std::size_t expected = width(rows[1]);
    for (const std::string& row : rows) {
        CHECK_EQ(width(row), expected);
    }
    // sama komorka ma tyle znakow, co widocznych w napisie
    CHECK_EQ(displayWidth("ąć"), static_cast<std::size_t>(2));
    CHECK_EQ(displayWidth("abc"), static_cast<std::size_t>(3));
}

TEST(tabela_stopka_dodaje_wiersz_podsumowania)
{
    Table table({{"A", Align::Left, {}}});
    table.addRow({"x"});
    table.setFooter({"RAZEM"});
    const std::string text = stripAnsi(table.render());
    CHECK(text.find("RAZEM") != std::string::npos);
    CHECK_EQ(lines(text).size(), static_cast<std::size_t>(6));
}

TEST(tabela_podpis_jest_przed_tabelą)
{
    Table table({{"A", Align::Left, {}}});
    table.addRow({"x"});
    table.setCaption("Moj podpis");
    const std::vector<std::string> rows = lines(stripAnsi(table.render()));
    CHECK_EQ(rows.front(), std::string("Moj podpis"));
}

TEST(tabela_bez_ramki_uzywa_spacji)
{
    Table table({{"A", Align::Left, {}}, {"B", Align::Left, {}}});
    table.setBorder(false);
    table.addRow({"x", "y"});
    const std::string text = table.render();
    CHECK(text.find("\u2502") == std::string::npos);
    CHECK(text.find("x") != std::string::npos);
}

TEST(dodanie_kolumny_dopełnia_istniejace_wiersze)
{
    Table table({{"A", Align::Left, {}}});
    table.addRow({"x"});
    table.addColumn({"B", Align::Left, {}});
    table.addRow({"y", "z"});
    const std::string text = stripAnsi(table.render());
    CHECK(text.find("B") != std::string::npos);
    CHECK(text.find("z") != std::string::npos);
    // wiersz dodany przed kolumna nadal ma wartosc w swojej kolumnie
    CHECK(text.find("x") != std::string::npos);
}

TEST(truncate_ostawia_tyle_wierszy_ile_wolano)
{
    Table table({{"A", Align::Left, {}}});
    for (int i = 0; i < 10; ++i) {
        table.addRow({"pozycja " + std::to_string(i)});
    }
    table.truncate(4);
    CHECK_EQ(table.rows(), static_cast<std::size_t>(5));
    const std::string text = stripAnsi(table.render());
    CHECK(text.find("pozostalych") != std::string::npos);
}

TEST(truncate_nic_nie_robi_gdy_jest_mniej_wierszy)
{
    Table table({{"A", Align::Left, {}}});
    table.addRow({"jeden"});
    table.truncate(10);
    CHECK_EQ(table.rows(), static_cast<std::size_t>(1));
}

TEST(progress_bar_wypelnia_proporcjonalnie)
{
    const std::string empty = stripAnsi(progressBar(0.0, 10, Style::green(), Style::dim()));
    CHECK_EQ(displayWidth(empty), static_cast<std::size_t>(10));
    const std::string full = stripAnsi(progressBar(1.0, 10, Style::green(), Style::dim()));
    CHECK_EQ(displayWidth(full), static_cast<std::size_t>(10));
    CHECK_NE(full, empty);
}

TEST(progress_bar_nie_przekracza_szerokosci)
{
    for (int i = 0; i <= 10; ++i) {
        const std::string bar = stripAnsi(
            progressBar(static_cast<double>(i) / 10.0, 12, Style::cyan(), Style::dim()));
        CHECK_EQ(displayWidth(bar), static_cast<std::size_t>(12));
    }
}

TEST(progress_bar_dla_zerowej_szerokosci)
{
    const std::string bar = stripAnsi(progressBar(0.5, 0, Style::green(), Style::dim()));
    CHECK_EQ(displayWidth(bar), static_cast<std::size_t>(0));
}

TEST(sparkline_ma_jeden_znak_na_wartosc)
{
    const std::string line = stripAnsi(sparkline({1, 5, 3, 0, 9}));
    CHECK_EQ(displayWidth(line), static_cast<std::size_t>(5));
    CHECK_EQ(displayWidth(stripAnsi(sparkline({}))), static_cast<std::size_t>(0));
}

TEST(sparkline_odzwierciedla_wzrost)
{
    const std::string rising = stripAnsi(sparkline({1, 2, 3, 4}));
    const std::string falling = stripAnsi(sparkline({4, 3, 2, 1}));
    CHECK_NE(rising, falling);
    // bloki "wydostaja" sie w gore przy wzroście - porownujemy znaki, nie bajty
    const std::vector<std::uint32_t> up = codePoints(rising);
    const std::vector<std::uint32_t> down = codePoints(falling);
    CHECK(up.back() > up.front());
    CHECK(down.back() < down.front());
}

TEST(vertical_bars_zwraca_wykres)
{
    const std::string chart = stripAnsi(verticalBars({1, 3, 2}, 3, Style::cyan(), Style::dim()));
    CHECK(!chart.empty());
    CHECK(chart.find('\n') != std::string::npos);
}

TEST(vertical_bars_pusty_zestaw)
{
    const std::string chart = stripAnsi(verticalBars({}, 5, Style::cyan(), Style::dim()));
    CHECK(chart.empty());
}

TEST(horizontal_bars_wypisuje_etykiety)
{
    const std::string chart
        = stripAnsi(horizontalBars({{"alfa", 3}, {"beta", 1}}, 10, true));
    CHECK(chart.find("alfa") != std::string::npos);
    CHECK(chart.find("beta") != std::string::npos);
}
