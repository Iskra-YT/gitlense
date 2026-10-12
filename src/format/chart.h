#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/types.h"
#include "format/color.h"

namespace gitlense {

/// Wydruk poziomej i pionowej kolumny w terminalu.
std::string verticalBars(const std::vector<std::uint64_t>& values, std::size_t width,
                         const Style& barStyle, const Style& axisStyle);

/// Wydruk poziomego wykresu ASCII z etykietami i wartosciami.
std::string horizontalBars(const std::vector<std::pair<std::string, std::uint64_t>>& items,
                           std::size_t barWidth, bool showValues);

/// "sparkline" - kompaktowy ciag znakow (do podsumowania aktywnosci).
std::string sparkline(const std::vector<std::uint64_t>& values);

/// Wypelnienie proporcjonalne (pasek postepu) np. 37.5% => "##########...."
std::string progressBar(double fraction, std::size_t width, const Style& fillStyle,
                        const Style& emptyStyle);

/// Znaki do pionowego wykresu skumulowanego (udzial procentowy).
constexpr const char* kBlockLevels[] = {" ", "\u2581", "\u2582", "\u2583", "\u2584",
                                        "\u2585", "\u2586", "\u2587", "\u2588"};

} // namespace gitlense
