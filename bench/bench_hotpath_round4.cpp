#include "affinity.hpp"
#include "environment.hpp"
#include "experimental_spsc_round4.hpp"
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
    std::array<std::uint64_t, Bytes / sizeof(std::uint64_t)> words{};
};

template <std::size_t Bytes>
[[nodiscard]] payload<Bytes> make_payload(std::uint64_t sequence) noexcept {
    payload<Bytes> value{};
    for (std::size_t i = 0; i < value.words.size(); ++i) {
        value.words[i] = sequence ^
            (0x9e3779b97f4a7c15ULL * static_cast<std::uint64_t>(i + 1));
    }
    return value;
}

template <std::size_t Bytes>
[[nodiscard]] bool payload_matches(const payload<Bytes>& value,
                                   std::uint64_t sequence) noexcept {
    for (std::size_t i = 0; i < value.words.size(); ++i) {
        const auto expected = sequence ^
            (0x9e3779b97f4a7c15ULL * static_cast<std::uint64_t>(i + 1));
        if (value.words[i] != expected) return false;
    }
    return true;
}

template <std::size_t PayloadBytes, class Queue>
result measure(Queue& q, std::uint64_t transfers, vqbench::cpu_pair cpus) {
    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    std::atomic<bool> producer_pinned{false};
    std::atomic<bool> consumer_pinned{false};
    std::atomic<unsigned> ready{0};
    std::atomic<std::uint64_t> produced_final{0};
    std::atomic<std::uint64_t> consumed_final{0};

    std::thread producer([&] {
        producer_pinned.store(vqbench::pin_current_thread(cpus.producer),
                              std::memory_order_relaxed);
        ready.fetch_add(1U, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        std::uint64_t sequence = 0;
        while (sequence < transfers && !failed.load(std::memory_order_relaxed)) {
            const auto value = make_payload<PayloadBytes>(sequence);
            if (q.try_push(value)) ++sequence;
        }
        produced_final.store(sequence, std::memory_order_relaxed);
    });

    std::thread consumer([&] {
        consumer_pinned.store(vqbench::pin_current_thread(cpus.consumer),
                              std::memory_order_relaxed);
        ready.fetch_add(1U, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        std::uint64_t expected = 0;
        while (expected < transfers && !failed.load(std::memory_order_relaxed)) {
            payload<PayloadBytes> value{};
            if (!q.try_pop(value)) continue;
            if (!payload_matches(value, expected)) {
                failed.store(true, std::memory_order_relaxed);
                break;
            }
            ++expected;
        }
        consumed_final.store(expected, std::memory_order_relaxed);
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
                       produced_final.load(std::memory_order_relaxed) == transfers &&
                       consumed_final.load(std::memory_order_relaxed) == transfers;
    const double seconds = std::chrono::duration<double>(end - begin).count();
    return {valid ? static_cast<double>(transfers) / seconds : 0.0,
            valid, producer_pin_ok, consumer_pin_ok};
}

template <std::size_t PayloadBytes>
result run_variant(std::string_view variant, std::uint64_t transfers,
                   vqbench::cpu_pair cpus) {
    using value_type = payload<PayloadBytes>;
    constexpr std::size_t capacity = 1024;

    if (variant == "production") {
        auto q = std::make_unique<veriqueue::spsc_queue<value_type, capacity>>();
        return measure<PayloadBytes>(*q, transfers, cpus);
    }
    if (variant == "stripe2") {
        auto q = std::make_unique<vqbench::experimental::round4::stripe2<value_type, capacity>>();
        return measure<PayloadBytes>(*q, transfers, cpus);
    }
    if (variant == "stripe4") {
        auto q = std::make_unique<vqbench::experimental::round4::stripe4<value_type, capacity>>();
        return measure<PayloadBytes>(*q, transfers, cpus);
    }
    if (variant == "stripe8") {
        auto q = std::make_unique<vqbench::experimental::round4::stripe8<value_type, capacity>>();
        return measure<PayloadBytes>(*q, transfers, cpus);
    }
    if (variant == "stripe16") {
        auto q = std::make_unique<vqbench::experimental::round4::stripe16<value_type, capacity>>();
        return measure<PayloadBytes>(*q, transfers, cpus);
    }
    if (variant == "adaptive_full") {
        auto q = std::make_unique<vqbench::experimental::round4::adaptive_full<value_type, capacity>>();
        return measure<PayloadBytes>(*q, transfers, cpus);
    }
    if (variant == "adaptive_half") {
        auto q = std::make_unique<vqbench::experimental::round4::adaptive_half<value_type, capacity>>();
        return measure<PayloadBytes>(*q, transfers, cpus);
    }
    throw std::invalid_argument("unknown variant: " + std::string(variant));
}

result dispatch_payload(std::string_view variant, std::size_t payload_bytes,
                        std::uint64_t transfers, vqbench::cpu_pair cpus) {
    switch (payload_bytes) {
    case 8: return run_variant<8>(variant, transfers, cpus);
    case 16: return run_variant<16>(variant, transfers, cpus);
    default: throw std::invalid_argument("payload_bytes must be 8 or 16");
    }
}

[[nodiscard]] std::size_t stripe_for(std::string_view variant,
                                     std::size_t payload_bytes) noexcept {
    if (variant == "stripe2") return 2;
    if (variant == "stripe4") return 4;
    if (variant == "stripe8") return 8;
    if (variant == "stripe16") return 16;
    if (variant == "adaptive_full") return payload_bytes == 8 ? 8 : 4;
    if (variant == "adaptive_half") return payload_bytes == 8 ? 4 : 2;
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: bench_hotpath_round4 <variant> <payload_bytes> <transfers>\n";
        return 2;
    }
    try {
        const std::string variant = argv[1];
        const auto payload_bytes = static_cast<std::size_t>(std::stoull(argv[2]));
        const auto transfers = static_cast<std::uint64_t>(std::stoull(argv[3]));
        if (transfers == 0) throw std::invalid_argument("transfers must be positive");

        const auto cpus = vqbench::selected_cpu_pair();
        const result measured = dispatch_payload(variant, payload_bytes, transfers, cpus);
        const bool affinity_valid = measured.producer_pinned && measured.consumer_pinned;

        std::cout << "{\"benchmark\":\"hotpath_round4\",\"implementation\":\"" << variant
                  << "\",\"mode\":\"scalar\",\"batch_size\":1"
                  << ",\"payload_bytes\":" << payload_bytes
                  << ",\"capacity\":1024"
                  << ",\"storage_stripe\":" << stripe_for(variant, payload_bytes)
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
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 2;
    }
}
