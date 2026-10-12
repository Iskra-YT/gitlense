#include "format/chart.h"

#include "format/format.h"

#include <algorithm>
#include <cstdio>
#include <map>

namespace gitlense {
namespace {

/// Powtarza `unit` `count` razy - wlasnie dlatego, ze wielobajtowe
/// znaki ramek nie mieszcza sie w `char`.
std::string repeat(const std::string& unit, std::size_t count)
{
    std::string out;
    out.reserve(unit.size() * count);
    for (std::size_t i = 0; i < count; ++i) {
        out += unit;
    }
    return out;
}

std::uint64_t maxOf(const std::vector<std::uint64_t>& values)
{
    std::uint64_t result = 0;
    for (const std::uint64_t value : values) {
        result = std::max(result, value);
    }
    return result;
}

} // namespace

std::string verticalBars(const std::vector<std::uint64_t>& values, std::size_t width,
                         const Style& barStyle, const Style& axisStyle)
{
    constexpr int kLevels = 9; // 0 = pusto, 1..8 = bloki
    constexpr std::size_t kRows = 8;
    if (values.empty() || width == 0) {
        return {};
    }
    // wykres rysujemy tylko z przedzialu, ktory mieści sie w `width`
    std::vector<std::uint64_t> bars;
    const std::size_t count = std::min(values.size(), width);
    bars.assign(values.end() - static_cast<std::ptrdiff_t>(count), values.end());

    const std::uint64_t maxValue = maxOf(bars);
    std::string out;
    for (int level = static_cast<int>(kRows); level >= 1; --level) {
        std::string line;
        for (const std::uint64_t value : bars) {
            // ile "calkowitych" poziomow i ile reszty
            const double ratio = maxValue == 0
                ? 0.0
                : static_cast<double>(value) / static_cast<double>(maxValue);
            const double scaled = ratio * static_cast<double>(kRows);
            const int full = static_cast<int>(scaled);
            const double rest = scaled - full;
            const int level1 = static_cast<int>(rest * kLevels);
            int symbol = 0;
            if (full >= level) {
                symbol = 8;
            } else if (full == level - 1) {
                symbol = level1;
            }
            line += barStyle.wrap(kBlockLevels[symbol]);
        }
        out += line;
        out += "\n";
    }

    // oś: co trzeci tick
    std::string axis;
    for (std::size_t i = 0; i < bars.size(); ++i) {
        axis += (i % 3 == 0) ? "\u2517" : "\u2500";
    }
    out += axisStyle.wrap(axis);
    out += "\n";

    out += axisStyle.wrap(number(maxValue));
    out += "\n";
    return out;
}

std::string horizontalBars(const std::vector<std::pair<std::string, std::uint64_t>>& items,
                           std::size_t barWidth, bool showValues)
{
    if (items.empty()) {
        return {};
    }
    std::size_t labelWidth = 0;
    std::uint64_t maxValue = 0;
    for (const auto& [label, value] : items) {
        labelWidth = std::max(labelWidth, displayWidth(label));
        maxValue = std::max(maxValue, value);
    }

    const Style barStyle = Style::cyan();
    std::string out;
    for (const auto& [label, value] : items) {
        std::string pad(labelWidth - displayWidth(label), ' ');
        out += Style::bold().wrap(label + pad);
        out += "  ";
        const std::size_t filled = maxValue == 0
            ? 0
            : static_cast<std::size_t>((static_cast<double>(value) / static_cast<double>(maxValue))
                                       * static_cast<double>(barWidth));
        out += barStyle.wrap(repeat("\u2588", filled));
        out += Style::dim().wrap(repeat("\u00b7", barWidth - filled));
        if (showValues) {
            out += " " + number(value);
        }
        out += "\n";
    }
    return out;
}

std::string sparkline(const std::vector<std::uint64_t>& values)
{
    static const char* kMarks[] = {"\u2581", "\u2582", "\u2583", "\u2584",
                                  "\u2585", "\u2586", "\u2587", "\u2588"};
    if (values.empty()) {
        return {};
    }
    const std::uint64_t maxValue = maxOf(values);
    if (maxValue == 0) {
        return repeat("\u00b7", values.size());
    }
    std::string out;
    out.reserve(values.size());
    for (const std::uint64_t value : values) {
        const int index = static_cast<int>((static_cast<double>(value)
                                            / static_cast<double>(maxValue))
                                           * 7.0 + 0.5);
        out += kMarks[std::clamp(index, 0, 7)];
    }
    return out;
}

std::string progressBar(double fraction, std::size_t width, const Style& fillStyle,
                        const Style& emptyStyle)
{
    const double clamped = std::clamp(fraction, 0.0, 1.0);
    const auto filled = static_cast<std::size_t>(clamped * static_cast<double>(width) + 0.5);
    return fillStyle.wrap(repeat("\u2588", filled))
        + emptyStyle.wrap(repeat("\u00b7", width - filled));
}

} // namespace gitlense
