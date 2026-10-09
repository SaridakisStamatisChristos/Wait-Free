#include "affinity.hpp"
#include "environment.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    constexpr std::size_t capacity = 2;
    constexpr std::size_t samples = 100'000;
    veriqueue::spsc_queue<std::uint64_t, capacity> a;
    veriqueue::spsc_queue<std::uint64_t, capacity> b;
    const unsigned n = vqbench::hardware_threads();
    const unsigned c1 = n > 1 ? 1U : 0U;
    std::atomic<bool> start{false};

    std::thread echo([&] {
        static_cast<void>(vqbench::pin_current_thread(c1));
        while (!start.load(std::memory_order_acquire)) {}
        for (std::size_t i = 0; i < samples; ++i) {
            std::uint64_t x = 0;
            while (!a.try_pop(x)) {}
            while (!b.try_push(x)) {}
        }
    });

    static_cast<void>(vqbench::pin_current_thread(0));
    std::vector<double> ns;
    ns.reserve(samples);
    start.store(true, std::memory_order_release);
    for (std::size_t i = 0; i < samples; ++i) {
        const auto begin = std::chrono::steady_clock::now();
        while (!a.try_push(static_cast<std::uint64_t>(i))) {}
        std::uint64_t x = 0;
        while (!b.try_pop(x)) {}
        const auto end = std::chrono::steady_clock::now();
        ns.push_back(std::chrono::duration<double, std::nano>(end - begin).count());
    }
    echo.join();
    std::sort(ns.begin(), ns.end());
    auto at = [&](double p) { return ns[static_cast<std::size_t>(p * static_cast<double>(ns.size() - 1))]; };
    std::cout << "{\"benchmark\":\"rtt_latency\",\"samples\":" << samples
              << ",\"median_ns\":" << at(0.50) << ",\"p90_ns\":" << at(0.90)
              << ",\"p95_ns\":" << at(0.95) << ",\"p99_ns\":" << at(0.99)
              << ",\"p999_ns\":" << at(0.999) << ",\"min_ns\":" << ns.front()
              << ",\"max_ns\":" << ns.back() << ",\"environment\":"
              << vqbench::environment_json() << "}\n";
}
