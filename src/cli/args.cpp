#include "cli/args.h"

#include "core/error.h"

#include <algorithm>
#include <cctype>

namespace gitlense {
namespace {

/// Wartosc zapisana dla opcji, ktora nie dostala argumentu.
constexpr const char* kSwitchValue = "true";

std::string normalizeName(const std::string& raw)
{
    std::string name = raw;
    if (name.rfind("--", 0) == 0) {
        name = name.substr(2);
    } else if (name.size() == 2 && name[0] == '-') {
        name = name.substr(1);
    }
    std::replace(name.begin(), name.end(), '_', '-');
    return name;
}

} // namespace

Arguments::Arguments(const std::vector<std::string>& argv, std::string command)
    : argv_(argv), command_(std::move(command))
{
}

void Arguments::declareValueOptions(std::vector<std::string> names) const
{
    bool added = false;
    for (std::string& name : names) {
        name = normalizeName(name);
        if (std::find(valueOptions_.begin(), valueOptions_.end(), name) == valueOptions_.end()) {
            valueOptions_.push_back(std::move(name));
            added = true;
        }
    }
    if (added) {
        // nowe opcje warteciowe moga zmienic sposob podzialu argumentow
        parsed_ = false;
    }
}

void Arguments::parse() const
{
    if (parsed_) {
        return;
    }
    parsed_ = true;
    values_.clear();
    order_.clear();
    positional_.clear();

    // argv[0] to program - pomijamy go, wiec komenda laduje do `positional`
    // (dla obiektu "globalnego"), a dla komendy jest juz pominieta.
    std::size_t i = command_.empty() ? 0 : 1;
    bool optionsEnded = false;

    for (; i < argv_.size(); ++i) {
        const std::string& token = argv_[i];
        if (optionsEnded) {
            positional_.push_back(token);
            continue;
        }
        if (token == "--") {
            optionsEnded = true;
            continue;
        }
        const bool isLong = token.size() > 2 && token.rfind("--", 0) == 0;
        if (isLong) {
            const std::size_t equals = token.find('=');
            if (equals != std::string::npos) {
                store(normalizeName(token.substr(0, equals)), token.substr(equals + 1));
                continue;
            }
            const std::string name = normalizeName(token);
            if (isValueOption(name) && i + 1 < argv_.size()) {
                store(name, argv_[++i]);
            } else {
                store(name, kSwitchValue);
            }
            continue;
        }
        const bool isShort = token.size() >= 2 && token[0] == '-' && token[1] != '-'
            && std::isdigit(static_cast<unsigned char>(token[1])) == 0;
        if (isShort) {
            if (token.size() == 2) {
                const std::string name = normalizeName(token);
                if (isValueOption(name) && i + 1 < argv_.size()) {
                    store(name, argv_[++i]);
                } else {
                    store(name, kSwitchValue);
                }
                continue;
            }
            // laczone przelaczniki: -abc
            for (std::size_t c = 1; c < token.size(); ++c) {
                store(normalizeName(std::string("-") + token[c]), kSwitchValue);
            }
            continue;
        }
        positional_.push_back(token);
    }
}

bool Arguments::isValueOption(const std::string& name) const
{
    return std::find(valueOptions_.begin(), valueOptions_.end(), normalizeName(name))
        != valueOptions_.end();
}

bool Arguments::looksLikeOption(const std::string& token) const
{
    // helper zostawiony dla czytelnosci wywolan w testach; parser sam
    // rozroznia opcje po prefiksie
    if (token.size() < 2 || token[0] != '-') {
        return false;
    }
    // "-5" to liczba, nie opcja
    return std::isdigit(static_cast<unsigned char>(token[1])) == 0;
}

void Arguments::store(const std::string& name, std::string value) const
{
    const std::string key = normalizeName(name);
    values_[key].push_back(std::move(value));
    if (std::find(order_.begin(), order_.end(), key) == order_.end()) {
        order_.push_back(key);
    }
}

const std::vector<std::string>& Arguments::positional() const
{
    parse();
    return positional_;
}

const std::vector<std::string>& Arguments::names() const
{
    parse();
    return order_;
}

bool Arguments::has(const std::string& name) const
{
    parse();
    return values_.find(normalizeName(name)) != values_.end();
}

bool Arguments::flag(const std::string& name) const
{
    parse();
    const auto it = values_.find(normalizeName(name));
    if (it == values_.end() || it->second.empty()) {
        return false;
    }
    const std::string& value = it->second.front();
    return value == kSwitchValue || value == "1" || value == "yes" || value == "on"
        || value.empty();
}

std::string Arguments::get(const std::string& name, const std::string& fallback) const
{
    parse();
    const auto it = values_.find(normalizeName(name));
    if (it == values_.end() || it->second.empty()) {
        return fallback;
    }
    return it->second.back();
}

std::optional<std::string> Arguments::getOptional(const std::string& name) const
{
    parse();
    const auto it = values_.find(normalizeName(name));
    if (it == values_.end() || it->second.empty()) {
        return std::nullopt;
    }
    return it->second.back();
}

long long Arguments::getInt(const std::string& name, long long fallback) const
{
    parse();
    const auto it = values_.find(normalizeName(name));
    if (it == values_.end() || it->second.empty()) {
        return fallback;
    }
    const std::string& text = it->second.back();
    if (text == kSwitchValue) {
        // `--top` bez liczby - nie zgadujmy, tylko powiedz wprost
        throw UsageError("opcja --" + normalizeName(name)
                         + " wymaga wartosci, np. --" + normalizeName(name) + " 10");
    }
    try {
        std::size_t consumed = 0;
        const long long value = std::stoll(text, &consumed);
        if (consumed != text.size()) {
            throw std::invalid_argument("smieci na koncu");
        }
        return value;
    } catch (const std::exception&) {
        throw UsageError("opcja --" + normalizeName(name) + " wymaga liczby, a nie \"" + text
                         + "\"");
    }
}

std::size_t Arguments::getSize(const std::string& name, std::size_t fallback) const
{
    const long long value = getInt(name, static_cast<long long>(fallback));
    if (value < 0) {
        throw UsageError("opcja --" + normalizeName(name) + " nie moze byc ujemna");
    }
    return static_cast<std::size_t>(value);
}

std::vector<std::string> Arguments::getAll(const std::string& name) const
{
    parse();
    const auto it = values_.find(normalizeName(name));
    if (it == values_.end()) {
        return {};
    }
    return it->second;
}

void Arguments::rejectUnknown(const std::vector<std::string>& allowed,
                              const std::string& hint) const
{
    parse();
    std::vector<std::string> unknown;
    for (const std::string& name : order_) {
        if (std::find(allowed.begin(), allowed.end(), name) == allowed.end()) {
            unknown.push_back("--" + name);
        }
    }
    if (unknown.empty()) {
        return;
    }
    std::string message = "nieznana opcja: ";
    for (std::size_t i = 0; i < unknown.size(); ++i) {
        message += (i != 0 ? ", " : "") + unknown[i];
    }
    if (!hint.empty()) {
        message += " (" + hint + ")";
    }
    throw UsageError(message);
}

} // namespace gitlense
