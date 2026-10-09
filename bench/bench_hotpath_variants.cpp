#include "affinity.hpp"
#include "environment.hpp"
#include "experimental_spsc_variants.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {

struct result final {
    double rate{0.0};
    bool valid{false};
};

template <class Queue>
result measure(Queue& q, std::uint64_t transfers, unsigned producer_cpu, unsigned consumer_cpu) {
    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    std::atomic<unsigned> ready{0};
    std::atomic<std::uint64_t> produced{0};
    std::atomic<std::uint64_t> consumed{0};

    std::thread producer([&] {
        static_cast<void>(vqbench::pin_current_thread(producer_cpu));
        ready.fetch_add(1U, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        for (std::uint64_t value = 0;
             value < transfers && !failed.load(std::memory_order_relaxed);) {
            if (q.try_push(value)) {
                ++value;
                produced.store(value, std::memory_order_relaxed);
            }
        }
    });

    std::thread consumer([&] {
        static_cast<void>(vqbench::pin_current_thread(consumer_cpu));
        ready.fetch_add(1U, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        for (std::uint64_t expected = 0;
             expected < transfers && !failed.load(std::memory_order_relaxed);) {
            std::uint64_t value = 0;
            if (q.try_pop(value)) {
                if (value != expected) {
                    failed.store(true, std::memory_order_relaxed);
                    break;
                }
                ++expected;
                consumed.store(expected, std::memory_order_relaxed);
            }
        }
    });

    while (ready.load(std::memory_order_acquire) != 2U) std::this_thread::yield();
    const auto begin = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto end = std::chrono::steady_clock::now();

    const bool valid = !failed.load(std::memory_order_relaxed) &&
                       produced.load(std::memory_order_relaxed) == transfers &&
                       consumed.load(std::memory_order_relaxed) == transfers;
    const double seconds = std::chrono::duration<double>(end - begin).count();
    return {valid ? static_cast<double>(transfers) / seconds : 0.0, valid};
}

template <std::size_t Capacity>
result run_variant(std::string_view variant, std::uint64_t transfers, vqbench::cpu_pair cpus) {
    if (variant == "veriqueue") {
        auto q = std::make_unique<veriqueue::spsc_queue<std::uint64_t, Capacity>>();
        return measure(*q, transfers, cpus.producer, cpus.consumer);
    }
    if (variant == "cached_limit") {
        auto q = std::make_unique<vqbench::experimental::cached_limit_queue<std::uint64_t, Capacity>>();
        return measure(*q, transfers, cpus.producer, cpus.consumer);
    }
    if (variant == "split_control") {
        auto q = std::make_unique<
            vqbench::experimental::split_control_queue<std::uint64_t, Capacity, false>>();
        return measure(*q, transfers, cpus.producer, cpus.consumer);
    }
    if (variant == "split_cached_limit") {
        auto q = std::make_unique<
            vqbench::experimental::split_control_queue<std::uint64_t, Capacity, true>>();
        return measure(*q, transfers, cpus.producer, cpus.consumer);
    }
    throw std::invalid_argument("unknown variant: " + std::string(variant));
}

int dispatch(std::string_view variant, std::size_t capacity, std::uint64_t transfers,
             vqbench::cpu_pair cpus) {
    result measured{};
    switch (capacity) {
    case 64: measured = run_variant<64>(variant, transfers, cpus); break;
    case 1024: measured = run_variant<1024>(variant, transfers, cpus); break;
    case 65536: measured = run_variant<65536>(variant, transfers, cpus); break;
    default: throw std::invalid_argument("capacity must be one of 64,1024,65536");
    }

    std::cout << "{\"benchmark\":\"hotpath_variant\",\"implementation\":\"" << variant
              << "\",\"payload_bytes\":8,\"capacity\":" << capacity
              << ",\"producer_cpu\":" << cpus.producer
              << ",\"consumer_cpu\":" << cpus.consumer
              << ",\"topology\":\"" << vqbench::topology_label()
              << "\",\"transfers\":" << transfers
              << ",\"valid\":" << (measured.valid ? "true" : "false")
              << ",\"transfers_per_second\":" << measured.rate
              << ",\"environment\":" << vqbench::environment_json() << "}\n";
    return measured.valid ? 0 : 3;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: bench_hotpath_variants <variant> <capacity> <transfers>\n"
                  << "variants: veriqueue cached_limit split_control split_cached_limit\n";
        return 2;
    }
    try {
        const std::string variant = argv[1];
        const auto capacity = static_cast<std::size_t>(std::stoull(argv[2]));
        const auto transfers = static_cast<std::uint64_t>(std::stoull(argv[3]));
        if (transfers == 0) throw std::invalid_argument("transfers must be positive");
        return dispatch(variant, capacity, transfers, vqbench::selected_cpu_pair());
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 2;
    }
}
