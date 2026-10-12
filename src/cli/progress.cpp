#include "cli/progress.h"

#include <unistd.h>

#include <string>

namespace gitlense {

void Progress::start(std::size_t total)
{
    total_ = total;
    current_ = 0;
    active_ = total > 0;
    enabled_ = enabled_ && ::isatty(STDERR_FILENO) == 1;
    draw();
}

void Progress::draw()
{
    if (!active_ || !enabled_) {
        return;
    }
    std::fprintf(stderr, "\r\033[K%s: %zu/%zu", label_.c_str(), current_, total_);
    std::fflush(stderr);
}

void Progress::advance(std::size_t step)
{
    if (!active_) {
        return;
    }
    current_ += step;
    draw();
}

void Progress::done(const std::string& message)
{
    if (!active_) {
        return;
    }
    if (enabled_) {
        std::fprintf(stderr, "\r\033[K");
    }
    if (!message.empty()) {
        std::fprintf(stderr, "%s\n", message.c_str());
    }
    active_ = false;
}

Progress::~Progress()
{
    if (active_ && enabled_) {
        std::fprintf(stderr, "\r\033[K");
    }
}

} // namespace gitlense
