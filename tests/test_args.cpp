#include "framework.h"

#include "cli/args.h"
#include "core/error.h"

#include <string>
#include <vector>

using namespace gitlense;

namespace {

Arguments make(const std::vector<std::string>& tokens, std::string command = {})
{
    return Arguments(tokens, std::move(command));
}

} // namespace

TEST(pusty_argumenty_nie_maja_nic)
{
    const Arguments args = make({});
    CHECK(!args.has("cos"));
    CHECK(!args.flag("cos"));
    CHECK_EQ(args.get("cos", "domyslna"), std::string("domyslna"));
    CHECK(args.positional().empty());
    CHECK_EQ(args.getInt("cos", 5), static_cast<long long>(5));
}

TEST(dlugie_opcje_z_jedna_wartoscia)
{
    Arguments args = make({"--top", "10"});
    args.declareValueOptions({"top"});
    CHECK(args.has("top"));
    CHECK_EQ(args.get("top"), std::string("10"));
    CHECK_EQ(args.getSize("top", 0), static_cast<std::size_t>(10));
}

TEST(dlugie_opcje_z_znakiem_rownosci)
{
    const Arguments args = make({"--top=25"});
    CHECK(args.has("top"));
    CHECK_EQ(args.get("top"), std::string("25"));
    CHECK_EQ(args.getSize("top", 0), static_cast<std::size_t>(25));
}

TEST(przelaczniki_bez_wartosci)
{
    const Arguments args = make({"--stat", "--all"});
    CHECK(args.flag("stat"));
    CHECK(args.flag("all"));
    CHECK(!args.has("name-only"));
}

TEST(przelacznik_nie_wyglada_jak_opcja_gdy_da_wartosc)
{
    // "--stat plik.txt" - "plik.txt" jest pozycyjne, nie wartoscia --stat
    const Arguments args = make({"--stat", "plik.txt"});
    CHECK(args.flag("stat"));
    CHECK_EQ(args.positional().size(), static_cast<std::size_t>(1));
    CHECK_EQ(args.positional().front(), std::string("plik.txt"));
}

TEST(krotkie_opcje)
{
    Arguments args = make({"-n", "5"});
    args.declareValueOptions({"n"});
    CHECK_EQ(args.getInt("n", 0), static_cast<long long>(5));
}

TEST(krotkie_opcje_laczone)
{
    const Arguments args = make({"-abc"});
    CHECK(args.flag("a"));
    CHECK(args.flag("b"));
    CHECK(args.flag("c"));
}

TEST(ujemna_liczba_nie_jest_opcja)
{
    Arguments args = make({"--top", "-5"});
    args.declareValueOptions({"top"});
    CHECK_EQ(args.getInt("top", 0), static_cast<long long>(-5));
}

TEST(opcja_odziedziczyc_kolejne_wartosci)
{
    Arguments args = make({"--include", "a", "--include", "b", "--include", "c"});
    args.declareValueOptions({"include"});
    const std::vector<std::string> values = args.getAll("include");
    CHECK_EQ(values.size(), static_cast<std::size_t>(3));
    CHECK_EQ(values[0], std::string("a"));
    CHECK_EQ(values[2], std::string("c"));
    // get() zwraca ostatnia wartosc
    CHECK_EQ(args.get("include"), std::string("c"));
}

TEST(podwojna_rozdzielacz_konczy_opcje)
{
    const Arguments args = make({"--stat", "--", "--top", "10"});
    CHECK(args.flag("stat"));
    CHECK(!args.has("top"));
    CHECK_EQ(args.positional().size(), static_cast<std::size_t>(2));
    CHECK_EQ(args.positional().front(), std::string("--top"));
}

TEST(deklarowane_opcje_wartosciowe_biora_nawet_opcje)
{
    Arguments args = make({"--grep", "--all"});
    args.declareValueOptions({"grep"});
    CHECK_EQ(args.get("grep"), std::string("--all"));
    CHECK(!args.flag("all"));
}

TEST(pozytkowe_argumenty_pomijaja_nazwe_komendy)
{
    const Arguments args = make({"log", "--stat", "plik"}, "log");
    CHECK(args.flag("stat"));
    CHECK_EQ(args.positional().size(), static_cast<std::size_t>(1));
    CHECK_EQ(args.positional().front(), std::string("plik"));
}

TEST(nazwa_komendy_nie_jest_pozycyjna)
{
    const Arguments args = make({"log", "a", "b"}, "log");
    CHECK_EQ(args.positional().size(), static_cast<std::size_t>(2));
    CHECK_EQ(args.command(), std::string("log"));
}

TEST(get_optional_zwraca_nullopt_gdy_brak)
{
    const Arguments args = make({"--stat"});
    CHECK(!args.getOptional("top").has_value());
    CHECK(args.getOptional("stat").has_value());
    CHECK_EQ(args.getOptional("stat").value(), std::string("true"));
}

TEST(get_int_zwraca_usage_error_dla_nie_liczby)
{
    Arguments args = make({"--top", "dużo"});
    args.declareValueOptions({"top"});
    bool thrown = false;
    try {
        static_cast<void>(args.getInt("top", 0));
    } catch (const UsageError&) {
        thrown = true;
    }
    CHECK(thrown);
}

TEST(get_int_odrzuca_smieci_na_koncu)
{
    const Arguments args = make({"--top=12x"});
    bool thrown = false;
    try {
        static_cast<void>(args.getInt("top", 0));
    } catch (const UsageError&) {
        thrown = true;
    }
    CHECK(thrown);
}

TEST(get_size_odrzuca_ujemne)
{
    Arguments args = make({"--top", "-3"});
    args.declareValueOptions({"top"});
    bool thrown = false;
    try {
        static_cast<void>(args.getSize("top", 0));
    } catch (const UsageError&) {
        thrown = true;
    }
    CHECK(thrown);
}

TEST(flag_akceptuje_wartosci_prawdziwe)
{
    for (const char* value : {"true", "1", "yes", "on"}) {
        const Arguments args = make({std::string("--color=") + value});
        CHECK(args.flag("color"));
    }
    const Arguments off = make({"--color=false"});
    CHECK(!off.flag("color"));
    CHECK(off.has("color"));
}

TEST(nazwy_opcji_zachowuja_kolejnosc)
{
    const Arguments args = make({"--stat", "--all", "--short"});
    CHECK_EQ(args.names().size(), static_cast<std::size_t>(3));
    CHECK_EQ(args.names()[0], std::string("stat"));
    CHECK_EQ(args.names()[1], std::string("all"));
    CHECK_EQ(args.names()[2], std::string("short"));
}

TEST(odrzucenie_nieznanej_opcji_wymienia_ja)
{
    const Arguments args = make({"--stat", "--cos"});
    bool thrown = false;
    std::string message;
    try {
        args.rejectUnknown({"stat", "all"}, "gitlense log --stat");
    } catch (const UsageError& error) {
        thrown = true;
        message = error.what();
    }
    CHECK(thrown);
    CHECK(message.find("--cos") != std::string::npos);
}

TEST(odrzucenie_opcji_nie_wywala_sie_gdy_wszystkie_znane)
{
    const Arguments args = make({"--stat", "--all"});
    bool thrown = false;
    try {
        args.rejectUnknown({"stat", "all", "short"});
    } catch (const UsageError&) {
        thrown = true;
    }
    CHECK(!thrown);
}

TEST(podwojna_rozdzielacz_w_nazwie_jest_normalizowana)
{
    const Arguments args = make({"--no_color"});
    CHECK(args.has("no-color"));
    CHECK(args.flag("no-color"));
}

TEST(wartosc_moze_byc_pusta)
{
    const Arguments args = make({"--grep="});
    CHECK(args.has("grep"));
    CHECK_EQ(args.get("grep"), std::string(""));
}

TEST(kazdy_argument_po_komendzie_jest_pozycyjny)
{
    const Arguments args = make({"diff", "main..feature", "--name-status"}, "diff");
    CHECK_EQ(args.positional().size(), static_cast<std::size_t>(1));
    CHECK_EQ(args.positional().front(), std::string("main..feature"));
    CHECK(args.flag("name-status"));
}

TEST(wartosciowa_bez_wartosci_konczy_sie_bledem)
{
    // `--top` bez liczby to nie "top=1", tylko blad z podpowiedzia
    Arguments args = make({"--top"});
    args.declareValueOptions({"top"});
    bool thrown = false;
    std::string message;
    try {
        static_cast<void>(args.getSize("top", 0));
    } catch (const UsageError& error) {
        thrown = true;
        message = error.what();
    }
    CHECK(thrown);
    CHECK(message.find("--top 10") != std::string::npos);
}

TEST(opcja_niezadeklarowana_nie_zjada_argumentu)
{
    // bez deklaracji `--top` jest przelacznikiem, a "10" zostaje pozycyjne
    const Arguments args = make({"--top", "10"});
    CHECK(args.flag("top"));
    CHECK_EQ(args.positional().size(), static_cast<std::size_t>(1));
    CHECK_EQ(args.positional().front(), std::string("10"));
}
