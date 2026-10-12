#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace gitlense {

/// Prosty pool watkow do rozlozenia pracy na indeksy [0, count).
/// Wyjatki z watkow sa przenoszone i rzucane w watku glownym.
template <typename Function>
void parallelFor(std::size_t count, std::size_t threadCount, Function&& function)
{
    if (count == 0) {
        return;
    }
    if (threadCount == 0) {
        threadCount = std::max(1u, std::thread::hardware_concurrency());
    }
    threadCount = std::clamp<std::size_t>(threadCount, 1, count);

    if (threadCount == 1) {
        for (std::size_t i = 0; i < count; ++i) {
            function(i);
        }
        return;
    }

    std::atomic<std::size_t> cursor{0};
    std::mutex errorMutex;
    std::exception_ptr firstError;

    const auto worker = [&] {
        while (firstError == nullptr) {
            const std::size_t index = cursor.fetch_add(1);
            if (index >= count) {
                return;
            }
            try {
                function(index);
            } catch (...) {
                std::lock_guard<std::mutex> lock(errorMutex);
                if (firstError == nullptr) {
                    firstError = std::current_exception();
                }
                cursor.store(count);
                return;
            }
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(threadCount - 1);
    for (std::size_t i = 1; i < threadCount; ++i) {
        workers.emplace_back(worker);
    }
    worker();
    for (std::thread& thread : workers) {
        thread.join();
    }
    if (firstError) {
        std::rethrow_exception(firstError);
    }
}

} // namespace gitlense
