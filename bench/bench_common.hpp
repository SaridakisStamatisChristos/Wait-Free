#pragma once

#include "affinity.hpp"
#include "environment.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace vqbench {

template <class Payload, std::size_t Capacity>
double throughput(std::uint64_t transfers, unsigned producer_cpu, unsigned consumer_cpu) {
    veriqueue::spsc_queue<Payload, Capacity> q;
    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    Payload value{};

    std::thread producer([&] {
        static_cast<void>(pin_current_thread(producer_cpu));
        while (!start.load(std::memory_order_acquire)) {}
        for (std::uint64_t i = 0; i < transfers;) {
            if (q.try_push(value)) ++i;
        }
    });

    std::thread consumer([&] {
        static_cast<void>(pin_current_thread(consumer_cpu));
        Payload out{};
        while (!start.load(std::memory_order_acquire)) {}
        for (std::uint64_t i = 0; i < transfers;) {
            if (q.try_pop(out)) ++i;
        }
    });

    const auto begin = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto end = std::chrono::steady_clock::now();
    if (failed.load()) return 0.0;
    const auto seconds = std::chrono::duration<double>(end - begin).count();
    return static_cast<double>(transfers) / seconds;
}

inline double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const auto pos = static_cast<std::size_t>(p * static_cast<double>(values.size() - 1));
    return values[pos];
}

} // namespace vqbench
