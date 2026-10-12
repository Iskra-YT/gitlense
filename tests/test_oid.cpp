#include "framework.h"

#include "core/error.h"
#include "core/types.h"

#include <string>

using namespace gitlense;

TEST(zerowy_oid_ma_same_zera)
{
    CHECK(isNull(kNullOid));
    CHECK_EQ(toHex(kNullOid), std::string("0000000000000000000000000000000000000000"));
}

TEST(oid_ma_20_bajtow)
{
    CHECK_EQ(kNullOid.size(), static_cast<std::size_t>(20));
}

TEST(from_hex_i_to_hex_powtarzaja_sie)
{
    const std::string hex = "0123456789abcdef0123456789abcdef01234567";
    const Oid oid = fromHex(hex);
    CHECK(!isNull(oid));
    CHECK_EQ(toHex(oid), hex);
}

TEST(from_hex_akceptuje_skrót)
{
    const Oid full = fromHex("0123456789abcdef0123456789abcdef01234567");
    const Oid shortOid = fromHex("0123456");
    // skrot to prefiks pelnego identyfikatoru, reszta zerami
    CHECK_EQ(toHex(shortOid).substr(0, 7), toHex(full).substr(0, 7));
}

TEST(to_short_hex_obcina_na_zadana_dlugosc)
{
    const Oid oid = fromHex("0123456789abcdef0123456789abcdef01234567");
    CHECK_EQ(toShortHex(oid).size(), static_cast<std::size_t>(7));
    CHECK_EQ(toShortHex(oid, 4).size(), static_cast<std::size_t>(4));
    CHECK_EQ(toShortHex(oid, 40).size(), static_cast<std::size_t>(40));
    CHECK_EQ(toShortHex(oid).substr(0, 4), std::string("0123"));
}

TEST(to_short_hex_dla_zerowego_oidu_nie_wybucha)
{
    CHECK_EQ(toShortHex(kNullOid, 7).size(), static_cast<std::size_t>(7));
}

TEST(from_hex_odrzuca_nieprawidlowa_dlugosc)
{
    bool thrown = false;
    try {
        // za krotkie (mniej niz 4 znaki) i za dlugie (wiecej niz 40)
        fromHex("012");
    } catch (const Error&) {
        thrown = true;
    }
    CHECK(thrown);

    bool tooLong = false;
    try {
        fromHex("0123456789abcdef0123456789abcdef0123456789ab");
    } catch (const Error&) {
        tooLong = true;
    }
    CHECK(tooLong);
}

TEST(status_zmiany_ma_kod_jednoliterowy)
{
    CHECK_EQ(statusCode(ChangeStatus::Added), 'A');
    CHECK_EQ(statusCode(ChangeStatus::Deleted), 'D');
    CHECK_EQ(statusCode(ChangeStatus::Modified), 'M');
    CHECK_EQ(statusCode(ChangeStatus::Renamed), 'R');
}

TEST(from_hex_odrzuca_nie_cyfry_szesnastkowe)
{
    bool thrown = false;
    try {
        fromHex("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
    } catch (const Error&) {
        thrown = true;
    }
    CHECK(thrown);
}

TEST(oidy_moga_byc_porownywane)
{
    const Oid a = fromHex("1111111111111111111111111111111111111111");
    const Oid b = fromHex("2222222222222222222222222222222222222222");
    const Oid c = fromHex("1111111111111111111111111111111111111111");
    CHECK(a != b);
    CHECK(a == c);
}

TEST(status_zmiany_ma_nazwe)
{
    CHECK_EQ(std::string(toString(ChangeStatus::Added)), std::string("Added"));
    CHECK_EQ(std::string(toString(ChangeStatus::Deleted)), std::string("Deleted"));
    CHECK_EQ(std::string(toString(ChangeStatus::Modified)), std::string("Modified"));
}

TEST(diff_stats_sumuje_zmiany)
{
    FileChange file;
    file.additions = 10;
    file.deletions = 3;
    CHECK_EQ(file.churn(), static_cast<std::uint64_t>(13));

    DiffStats stats;
    stats.additions = 10;
    stats.deletions = 3;
    CHECK_EQ(stats.churn(), static_cast<std::uint64_t>(13));
}

TEST(wyświetlana_sciezka_to_nowa)
{
    FileChange file;
    file.oldPath = "stara.cpp";
    file.path = "nowa.cpp";
    CHECK_EQ(file.displayPath(), std::string("nowa.cpp"));
}
