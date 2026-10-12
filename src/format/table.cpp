#include "format/table.h"

#include "format/format.h"

#include <algorithm>
#include <iostream>

namespace gitlense {
namespace {

constexpr const char* kVertical = "\u2502";   // |
constexpr const char* kHorizontal = "\u2500"; // ─
constexpr const char* kTopLeft = "\u256d";    // ╭
constexpr const char* kTopRight = "\u256e";   // ╮
constexpr const char* kBottomLeft = "\u2570"; // ╰
constexpr const char* kBottomRight = "\u256f";// ╯
constexpr const char* kTeeDown = "\u252c";    // ┬
constexpr const char* kTeeUp = "\u2534";      // ┴
constexpr const char* kCross = "\u253c";      // ┼
constexpr const char* kMiddleLeft = "\u251c";  // ├
constexpr const char* kMiddleRight = "\u2524"; // ┤
constexpr const char* kHorizontalDash = "\u2500";

std::string repeat(const std::string& unit, std::size_t count)
{
    std::string out;
    out.reserve(unit.size() * count);
    for (std::size_t i = 0; i < count; ++i) {
        out += unit;
    }
    return out;
}

} // namespace

Table::Table(std::vector<Column> columns) : columns_(std::move(columns)) {}

void Table::addColumn(Column column)
{
    columns_.push_back(std::move(column));
    for (auto& row : rows_) {
        row.emplace_back();
    }
    if (!footer_.empty()) {
        footer_.emplace_back();
    }
}

std::size_t Table::visibleWidth(const std::string& text) const
{
    // liczymy znaki, nie bajty - polskie znaki i ramki to wielobajtowy UTF-8
    return displayWidth(text);
}

void Table::addRow(std::vector<std::string> cells)
{
    cells.resize(columns_.size());
    rows_.push_back(std::move(cells));
}

void Table::setFooter(std::vector<std::string> cells)
{
    cells.resize(columns_.size());
    footer_ = std::move(cells);
}

void Table::truncate(std::size_t maxRows)
{
    if (rows_.size() <= maxRows) {
        return;
    }
    const std::size_t hidden = rows_.size() - maxRows;
    rows_.resize(maxRows);
    std::vector<std::string> summary(columns_.size());
    summary[0] = "... (" + std::to_string(hidden) + " pozostalych pozycji)";
    for (std::size_t i = 1; i < columns_.size(); ++i) {
        summary[i].clear();
    }
    rows_.push_back(std::move(summary));
}

std::string Table::render() const
{
    if (columns_.empty()) {
        return {};
    }

    std::vector<std::size_t> widths(columns_.size(), 0);
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        widths[i] = visibleWidth(columns_[i].title);
    }
    for (const auto& row : rows_) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            widths[i] = std::max(widths[i], visibleWidth(row[i]));
        }
    }
    if (!footer_.empty()) {
        for (std::size_t i = 0; i < footer_.size(); ++i) {
            widths[i] = std::max(widths[i], visibleWidth(footer_[i]));
        }
    }

    const std::string sep = border_ ? std::string(kVertical) : std::string("  ");
    std::string out;
    if (!caption_.empty()) {
        out += Style::bold().wrap(caption_) + "\n";
    }

    const auto line = [&](const char* left, const char* mid, const char* right) {
        std::string result = border_ ? left : "";
        for (std::size_t i = 0; i < widths.size(); ++i) {
            if (i != 0) {
                result += border_ ? mid : "--";
            }
            result += repeat(kHorizontalDash, widths[i] + 2);
        }
        if (border_) {
            result += right;
        }
        result += "\n";
        return result;
    };

    // jedna komorka = margines 1 spacja z obu stron + tekst wyrownany do
    // szerokosci kolumny; uzywamy tego samego miejsca dla naglowka, wierszy
    // i stopki, inaczej kolumny sie rozjezdzaja
    const auto cell = [&](const Column& column, const std::string& text, std::size_t width) {
        const std::size_t visible = visibleWidth(text);
        const std::string filler(visible < width ? width - visible : 0, ' ');
        const std::string inner = column.align == Align::Left ? text + filler : filler + text;
        return " " + inner + " ";
    };

    // jeden wiersz: pionowa krawedz, komorki rozdzielone `sep` (albo dwiema
    // spacjami bez ramki) i pionowa krawedz na koncu
    const auto row = [&](const std::vector<std::string>& cells) {
        std::string result = border_ ? std::string(kVertical) : "";
        for (std::size_t i = 0; i < columns_.size(); ++i) {
            result += cell(columns_[i], cells[i], widths[i]);
            if (i + 1 < columns_.size()) {
                result += sep;
            }
        }
        result += border_ ? std::string(kVertical) : "";
        result += "\n";
        return result;
    };

    // naglowek
    out += line(kTopLeft, kTeeDown, kTopRight);
    std::vector<std::string> titles;
    for (const Column& column : columns_) {
        titles.push_back(column.title);
    }
    out += row(titles);
    out += line(kMiddleLeft, kCross, kMiddleRight);

    for (const auto& cells : rows_) {
        out += row(cells);
    }

    if (!footer_.empty()) {
        out += line(kBottomLeft, kCross, kBottomRight);
        out += row(footer_);
    } else {
        out += line(kBottomLeft, kTeeUp, kBottomRight);
    }

    if (!out.empty() && out.back() == '\n') {
        out.pop_back();
    }
    return out;
}

void Table::print() const
{
    const std::string text = render();
    if (!text.empty()) {
        std::cout << text << "\n";
    }
}

std::string ellipsize(const std::string& text, std::size_t width)
{
    if (width == 0 || text.size() <= width) {
        return text;
    }
    if (width <= 3) {
        return text.substr(0, width);
    }
    return text.substr(0, width - 3) + "...";
}

std::string middleEllipsize(const std::string& path, std::size_t width)
{
    if (width == 0 || path.size() <= width) {
        return path;
    }
    if (width <= 5) {
        return path.substr(0, width);
    }
    const std::size_t keep = width - 3;
    const std::size_t head = keep / 2;
    const std::size_t tail = keep - head;
    return path.substr(0, head) + "..." + path.substr(path.size() - tail);
}

} // namespace gitlense
