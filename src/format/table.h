#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "format/color.h"

namespace gitlense {

enum class Align { Left, Right };

/// Prosty renderer tabel ASCII z automatycznym dopasowaniem kolumn
/// do tresci. Sklada sie z naglowka, separatorow i wierszy danych.
class Table {
public:
    struct Column {
        std::string title;
        Align align{Align::Left};
        Style style;
    };

    explicit Table(std::vector<Column> columns);
    /// Doklada kolumne na koncu (przydatne dla macierzy budowanych
    /// dynamicznie); istniejace wiersze dostaja pusta komorke.
    void addColumn(Column column);

    void addRow(std::vector<std::string> cells);
    void setFooter(std::vector<std::string> cells);
    void setCaption(std::string caption) { caption_ = std::move(caption); }

    /// Rysuje tabele i zwraca komplet tekstu (bez znaku nowej linii
    /// na koncu - dodaje go drukujaca strona).
    [[nodiscard]] std::string render() const;
    void print() const;

    [[nodiscard]] std::size_t rows() const { return rows_.size(); }
    /// Limituje liczbe wierszy; nadmiar laczy w ostatni wiersz.
    void truncate(std::size_t maxRows);

    void setBorder(bool enabled) { border_ = enabled; }

private:
    std::size_t visibleWidth(const std::string& text) const;

    std::vector<Column> columns_;
    std::vector<std::vector<std::string>> rows_;
    std::vector<std::string> footer_;
    std::string caption_;
    bool border_{true};
};

/// Skraca tekst do `width`, dopisujac "..." (bez kolorow w tresci).
std::string ellipsize(const std::string& text, std::size_t width);
/// Zwraca najszersza czesc tekstu sprzed slashy, zachowujac reszte w "...".
std::string middleEllipsize(const std::string& path, std::size_t width);

} // namespace gitlense
