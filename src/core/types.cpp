#include "core/types.h"

#include "core/error.h"

#include <git2.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <stdexcept>

namespace gitlense {

bool isNull(const Oid& oid) noexcept
{
    return std::all_of(oid.begin(), oid.end(), [](std::uint8_t b) { return b == 0; });
}

std::string toHex(const Oid& oid)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(oid.size() * 2);
    for (const std::uint8_t b : oid) {
        out.push_back(kHex[b >> 4]);
        out.push_back(kHex[b & 0x0F]);
    }
    return out;
}

std::string toShortHex(const Oid& oid, std::size_t length)
{
    std::string hex = toHex(oid);
    if (length < 4) {
        length = 4;
    }
    if (length > hex.size()) {
        length = hex.size();
    }
    return hex.substr(0, length);
}

Oid fromHex(const std::string& hex)
{
    // Przyjmujemy tez skrot (min. 4 znaki, jak w git). Skrot jest prefiksem
    // pelnego identyfikatora, wiec nieparzysta liczba znakow oznacza, ze
    // dolna polowa ostatniego bajtu jest zerowa: "0123456" -> 01 23 45 60.
    if (hex.size() < 4 || hex.size() > 40) {
        throw Error("niepoprawny identyfikator commit \"" + hex
                    + "\" (oczekiwano 4-40 znakow hex)");
    }
    Oid oid{};
    for (std::size_t i = 0; i < hex.size(); ++i) {
        const char c = hex[i];
        int value = 0;
        if (c >= '0' && c <= '9') {
            value = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            value = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            value = c - 'A' + 10;
        } else {
            throw Error("niepoprawny identyfikator commit \"" + hex + "\"");
        }
        // parzysty indeks to gora polowa bajtu, nieparzysty dolna
        const int previous = static_cast<int>(oid[i / 2]);
        oid[i / 2] = static_cast<std::uint8_t>(
            i % 2 == 0 ? value << 4 : (previous & 0xF0) | value);
    }
    return oid;
}

const char* toString(ChangeStatus status) noexcept
{
    switch (status) {
    case ChangeStatus::Added:
        return "Added";
    case ChangeStatus::Deleted:
        return "Deleted";
    case ChangeStatus::Modified:
        return "Modified";
    case ChangeStatus::Renamed:
        return "Renamed";
    case ChangeStatus::Copied:
        return "Copied";
    case ChangeStatus::TypeChange:
        return "TypeChange";
    case ChangeStatus::Untracked:
        return "Untracked";
    case ChangeStatus::Conflicted:
        return "Conflicted";
    case ChangeStatus::Ignored:
        return "Ignored";
    }
    return "Unknown";
}

char statusCode(ChangeStatus status) noexcept
{
    switch (status) {
    case ChangeStatus::Added:
        return 'A';
    case ChangeStatus::Deleted:
        return 'D';
    case ChangeStatus::Modified:
        return 'M';
    case ChangeStatus::Renamed:
        return 'R';
    case ChangeStatus::Copied:
        return 'C';
    case ChangeStatus::TypeChange:
        return 'T';
    case ChangeStatus::Untracked:
        return '?';
    case ChangeStatus::Conflicted:
        return 'U';
    case ChangeStatus::Ignored:
        return '!';
    }
    return '?';
}

} // namespace gitlense
