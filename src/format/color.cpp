#include "format/color.h"

#include <cstdlib>
#include <string>
#include <unistd.h>

namespace gitlense {
namespace {
bool g_enabled = true;
}

Style Style::bold() { return Style{"1"}; }
Style Style::dim() { return Style{"2"}; }
Style Style::red() { return Style{"31"}; }
Style Style::green() { return Style{"32"}; }
Style Style::yellow() { return Style{"33"}; }
Style Style::blue() { return Style{"34"}; }
Style Style::magenta() { return Style{"35"}; }
Style Style::cyan() { return Style{"36"}; }

Style Style::boldColor(const std::string& color)
{
    return Style{"1;3" + color};
}

Style Style::custom(const std::string& ansiCodes)
{
    return Style{ansiCodes};
}

Style::Style(std::string codes) : codes_(std::move(codes)) {}

Style Style::combine(const Style& a, const Style& b)
{
    if (a.codes_.empty()) {
        return b;
    }
    if (b.codes_.empty()) {
        return a;
    }
    return Style{a.codes_ + ";" + b.codes_};
}

std::string Style::wrap(const std::string& text) const
{
    // kluczowy warunek: bez `g_enabled` `--no-color` i `NO_COLOR` bylyby
    // calkowicie bezczynne, a wyjscie w plik zawieraloby kody ANSI
    if (codes_.empty() || text.empty() || !g_enabled) {
        return text;
    }
    return "\033[" + codes_ + "m" + text + "\033[0m";
}

Style& Style::operator+=(const Style& other)
{
    if (other.codes_.empty()) {
        return *this;
    }
    if (codes_.empty()) {
        codes_ = other.codes_;
    } else {
        codes_ += ";" + other.codes_;
    }
    return *this;
}

bool ColorMode::stdoutIsTerminal()
{
    return ::isatty(STDOUT_FILENO) == 1;
}

void ColorMode::initialize(bool forceColor, bool forceNoColor)
{
    if (forceNoColor) {
        g_enabled = false;
        return;
    }
    if (forceColor) {
        g_enabled = true;
        return;
    }
    if (const char* noColor = std::getenv("NO_COLOR"); noColor && *noColor != '\0') {
        g_enabled = false;
        return;
    }
    if (const char* term = std::getenv("TERM"); term && std::string(term) == "dumb") {
        g_enabled = false;
        return;
    }
    g_enabled = stdoutIsTerminal();
}

bool ColorMode::enabled()
{
    return g_enabled;
}

std::string stripAnsi(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] == '\033' && i + 1 < text.size() && text[i + 1] == '[') {
            std::size_t j = i + 2;
            while (j < text.size() && text[j] != 'm' && text[j] != '\033') {
                ++j;
            }
            if (j < text.size() && text[j] == 'm') {
                i = j + 1;
                continue;
            }
            i = j;
            continue;
        }
        out.push_back(text[i]);
        ++i;
    }
    return out;
}

} // namespace gitlense
