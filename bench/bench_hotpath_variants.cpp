#include "affinity.hpp"
#include "environment.hpp"
#include "experimental_spsc_variants.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <array>
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
    bool producer_pinned{false};
    bool consumer_pinned{false};
};

template <std::size_t Bytes>
struct payload final {
    static_assert(Bytes >= sizeof(std::uint64_t));
    static_assert(Bytes % sizeof(std::uint64_t) == 0);
    std::array<std::uint64_t, Bytes / sizeof(std::uint64_t)> words{};
};

template <std::size_t Bytes>
[[nodiscard]] payload<Bytes> make_payload(std::uint64_t sequence) noexcept {
    payload<Bytes> value{};
    for (std::size_t i = 0; i < value.words.size(); ++i) {
        value.words[i] = sequence ^ (0x9e3779b97f4a7c15ULL * static_cast<std::uint64_t>(i + 1));
    }
    return value;
}

template <std::size_t Bytes>
[[nodiscard]] bool payload_matches(const payload<Bytes>& value, std::uint64_t sequence) noexcept {
    for (std::size_t i = 0; i < value.words.size(); ++i) {
        const auto expected = sequence ^
            (0x9e3779b97f4a7c15ULL * static_cast<std::uint64_t>(i + 1));
        if (value.words[i] != expected) return false;
    }
    return true;
}

template <class Queue, std::size_t PayloadBytes>
result measure(Queue& q, std::uint64_t transfers, unsigned producer_cpu, unsigned consumer_cpu) {
    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    std::atomic<bool> producer_pinned{false};
    std::atomic<bool> consumer_pinned{false};
    std::atomic<unsigned> ready{0};
    std::atomic<std::uint64_t> produced{0};
    std::atomic<std::uint64_t> consumed{0};

    std::thread producer([&] {
        producer_pinned.store(vqbench::pin_current_thread(producer_cpu), std::memory_order_relaxed);
        ready.fetch_add(1U, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        for (std::uint64_t sequence = 0;
             sequence < transfers && !failed.load(std::memory_order_relaxed);) {
            const auto value = make_payload<PayloadBytes>(sequence);
            if (q.try_push(value)) {
                ++sequence;
                produced.store(sequence, std::memory_order_relaxed);
            }
        }
    });

    std::thread consumer([&] {
        consumer_pinned.store(vqbench::pin_current_thread(consumer_cpu), std::memory_order_relaxed);
        ready.fetch_add(1U, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        for (std::uint64_t expected = 0;
             expected < transfers && !failed.load(std::memory_order_relaxed);) {
            payload<PayloadBytes> value{};
            if (q.try_pop(value)) {
                if (!payload_matches(value, expected)) {
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

    const bool producer_pin_ok = producer_pinned.load(std::memory_order_relaxed);
    const bool consumer_pin_ok = consumer_pinned.load(std::memory_order_relaxed);
    const bool valid = producer_pin_ok && consumer_pin_ok &&
                       !failed.load(std::memory_order_relaxed) &&
                       produced.load(std::memory_order_relaxed) == transfers &&
                       consumed.load(std::memory_order_relaxed) == transfers;
    const double seconds = std::chrono::duration<double>(end - begin).count();
    return {valid ? static_cast<double>(transfers) / seconds : 0.0,
            valid, producer_pin_ok, consumer_pin_ok};
}

template <std::size_t Capacity, std::size_t PayloadBytes>
result run_variant(std::string_view variant, std::uint64_t transfers, vqbench::cpu_pair cpus) {
    using value_type = payload<PayloadBytes>;
    if (variant == "veriqueue") {
        auto q = std::make_unique<veriqueue::spsc_queue<value_type, Capacity>>();
        return measure<decltype(*q), PayloadBytes>(*q, transfers, cpus.producer, cpus.consumer);
    }
    if (variant == "single_owner_cursor") {
        auto q = std::make_unique<
            vqbench::experimental::single_owner_cursor_queue<value_type, Capacity>>();
        return measure<decltype(*q), PayloadBytes>(*q, transfers, cpus.producer, cpus.consumer);
    }
    if (variant == "cached_limit") {
        auto q = std::make_unique<vqbench::experimental::cached_limit_queue<value_type, Capacity>>();
        return measure<decltype(*q), PayloadBytes>(*q, transfers, cpus.producer, cpus.consumer);
    }
    if (variant == "split_control") {
        auto q = std::make_unique<
            vqbench::experimental::split_control_queue<value_type, Capacity, false>>();
        return measure<decltype(*q), PayloadBytes>(*q, transfers, cpus.producer, cpus.consumer);
    }
    if (variant == "split_cached_limit") {
        auto q = std::make_unique<
            vqbench::experimental::split_control_queue<value_type, Capacity, true>>();
        return measure<decltype(*q), PayloadBytes>(*q, transfers, cpus.producer, cpus.consumer);
    }
    throw std::invalid_argument("unknown variant: " + std::string(variant));
}

template <std::size_t PayloadBytes>
result dispatch_capacity(std::string_view variant, std::size_t capacity,
                         std::uint64_t transfers, vqbench::cpu_pair cpus) {
    switch (capacity) {
    case 2: return run_variant<2, PayloadBytes>(variant, transfers, cpus);
    case 64: return run_variant<64, PayloadBytes>(variant, transfers, cpus);
    case 256: return run_variant<256, PayloadBytes>(variant, transfers, cpus);
    case 1024: return run_variant<1024, PayloadBytes>(variant, transfers, cpus);
    case 65536: return run_variant<65536, PayloadBytes>(variant, transfers, cpus);
    default: throw std::invalid_argument("capacity must be one of 2,64,256,1024,65536");
    }
}

result dispatch_payload(std::string_view variant, std::size_t capacity,
                        std::size_t payload_bytes, std::uint64_t transfers,
                        vqbench::cpu_pair cpus) {
    switch (payload_bytes) {
    case 8: return dispatch_capacity<8>(variant, capacity, transfers, cpus);
    case 16: return dispatch_capacity<16>(variant, capacity, transfers, cpus);
    case 64: return dispatch_capacity<64>(variant, capacity, transfers, cpus);
    case 256: return dispatch_capacity<256>(variant, capacity, transfers, cpus);
    default: throw std::invalid_argument("payload_bytes must be one of 8,16,64,256");
    }
}

int dispatch(std::string_view variant, std::size_t capacity, std::size_t payload_bytes,
             std::uint64_t transfers, vqbench::cpu_pair cpus) {
    const result measured = dispatch_payload(variant, capacity, payload_bytes, transfers, cpus);
    const bool affinity_valid = measured.producer_pinned && measured.consumer_pinned;

    std::cout << "{\"benchmark\":\"hotpath_variant\",\"implementation\":\"" << variant
              << "\",\"payload_bytes\":" << payload_bytes
              << ",\"capacity\":" << capacity
              << ",\"producer_cpu\":" << cpus.producer
              << ",\"consumer_cpu\":" << cpus.consumer
              << ",\"topology\":\"" << vqbench::topology_label()
              << "\",\"transfers\":" << transfers
              << ",\"pinning_requested\":true"
              << ",\"producer_pinned\":" << (measured.producer_pinned ? "true" : "false")
              << ",\"consumer_pinned\":" << (measured.consumer_pinned ? "true" : "false")
              << ",\"affinity_valid\":" << (affinity_valid ? "true" : "false")
              << ",\"valid\":" << (measured.valid ? "true" : "false")
              << ",\"transfers_per_second\":" << measured.rate
              << ",\"environment\":" << vqbench::environment_json() << "}\n";
    return measured.valid ? 0 : 3;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4 && argc != 5) {
        std::cerr << "usage: bench_hotpath_variants <variant> <capacity> [payload_bytes] <transfers>\n"
                  << "variants: veriqueue single_owner_cursor cached_limit split_control split_cached_limit\n";
        return 2;
    }
    try {
        const std::string variant = argv[1];
        const auto capacity = static_cast<std::size_t>(std::stoull(argv[2]));
        const auto payload_bytes = argc == 5
            ? static_cast<std::size_t>(std::stoull(argv[3])) : std::size_t{8};
        const auto transfers = static_cast<std::uint64_t>(std::stoull(argv[argc - 1]));
        if (transfers == 0) throw std::invalid_argument("transfers must be positive");
        return dispatch(variant, capacity, payload_bytes, transfers, vqbench::selected_cpu_pair());
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 2;
    }
}
