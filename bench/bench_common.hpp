#pragma once

#include "affinity.hpp"
#include "environment.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace vqbench {

struct pacing final {
    std::uint64_t producer_yield_every{0};
    std::uint64_t consumer_yield_every{0};
};

template <class Payload, std::size_t Capacity>
double throughput(std::uint64_t transfers, unsigned producer_cpu, unsigned consumer_cpu,
                  pacing pace = {}) {
    veriqueue::spsc_queue<Payload, Capacity> q;
    std::atomic<bool> start{false};
    Payload value{};

    std::thread producer([&] {
        static_cast<void>(pin_current_thread(producer_cpu));
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        for (std::uint64_t completed = 0; completed < transfers;) {
            if (q.try_push(value)) {
                ++completed;
                if (pace.producer_yield_every != 0 &&
                    completed % pace.producer_yield_every == 0) {
                    std::this_thread::yield();
                }
            }
        }
    });

    std::thread consumer([&] {
        static_cast<void>(pin_current_thread(consumer_cpu));
        Payload out{};
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        for (std::uint64_t completed = 0; completed < transfers;) {
            if (q.try_pop(out)) {
                ++completed;
                if (pace.consumer_yield_every != 0 &&
                    completed % pace.consumer_yield_every == 0) {
                    std::this_thread::yield();
                }
            }
        }
    });

    const auto begin = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto end = std::chrono::steady_clock::now();
    const auto seconds = std::chrono::duration<double>(end - begin).count();
    return static_cast<double>(transfers) / seconds;
}

template <class Payload, std::size_t Capacity>
double fill_drain_throughput(std::uint64_t requested_transfers, unsigned producer_cpu,
                             unsigned consumer_cpu) {
    veriqueue::spsc_queue<Payload, Capacity> q;
    const std::uint64_t capacity = static_cast<std::uint64_t>(Capacity);
    const std::uint64_t cycles = std::max<std::uint64_t>(1, requested_transfers / capacity);
    const std::uint64_t transfers = cycles * capacity;
    std::atomic<bool> start{false};
    std::atomic<unsigned> phase{0};
    Payload value{};

    std::thread producer([&] {
        static_cast<void>(pin_current_thread(producer_cpu));
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        for (std::uint64_t cycle = 0; cycle < cycles; ++cycle) {
            while (phase.load(std::memory_order_acquire) != 0U) {
                std::this_thread::yield();
            }
            for (std::uint64_t i = 0; i < capacity;) {
                if (q.try_push(value)) {
                    ++i;
                }
            }
            phase.store(1U, std::memory_order_release);
        }
    });

    std::thread consumer([&] {
        static_cast<void>(pin_current_thread(consumer_cpu));
        Payload out{};
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        for (std::uint64_t cycle = 0; cycle < cycles; ++cycle) {
            while (phase.load(std::memory_order_acquire) != 1U) {
                std::this_thread::yield();
            }
            for (std::uint64_t i = 0; i < capacity;) {
                if (q.try_pop(out)) {
                    ++i;
                }
            }
            phase.store(0U, std::memory_order_release);
        }
    });

    const auto begin = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto end = std::chrono::steady_clock::now();
    const auto seconds = std::chrono::duration<double>(end - begin).count();
    return static_cast<double>(transfers) / seconds;
}

inline double percentile(std::vector<double> values, double p) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const auto pos = static_cast<std::size_t>(p * static_cast<double>(values.size() - 1));
    return values[pos];
}

} // namespace vqbench
