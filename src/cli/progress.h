#pragma once

#include <cstdio>
#include <string>

namespace gitlense {

/// Postep na stderr (nie psuje potoku danych). Przy przekierowaniu
/// stderr do pliku wypisuje tylko koncowy komunikat.
class Progress {
public:
    explicit Progress(std::string label) : label_(std::move(label)) {}

    void start(std::size_t total);
    void advance(std::size_t step = 1);
    void done(const std::string& message = {});
    [[nodiscard]] bool active() const { return active_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    ~Progress();

private:
    void draw();

    std::string label_;
    std::size_t total_{0};
    std::size_t current_{0};
    bool active_{false};
    bool enabled_{true};
};

} // namespace gitlense
