#include "affinity.hpp"
#include "environment.hpp"
#include "experimental_spsc_round4b.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

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

struct timed_run final {
    double seconds{0.0};
    bool valid{false};
};

template <std::size_t PayloadBytes, class Queue>
[[nodiscard]] timed_run measure_once(std::uint64_t transfers,
                                     vqbench::cpu_pair cpus) {
    Queue q{};
    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    std::atomic<bool> producer_pinned{false};
    std::atomic<bool> consumer_pinned{false};
    std::atomic<unsigned> ready{0};
    std::uint64_t produced_final = 0;
    std::uint64_t consumed_final = 0;

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
        produced_final = sequence;
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
        consumed_final = expected;
    });

    while (ready.load(std::memory_order_acquire) != 2U) std::this_thread::yield();
    const auto begin = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto end = std::chrono::steady_clock::now();

    const bool valid = producer_pinned.load(std::memory_order_relaxed) &&
                       consumer_pinned.load(std::memory_order_relaxed) &&
                       !failed.load(std::memory_order_relaxed) &&
                       produced_final == transfers && consumed_final == transfers;
    return {std::chrono::duration<double>(end - begin).count(), valid};
}

struct paired_block final {
    double production_tps{0.0};
    double candidate_tps{0.0};
    double ratio{0.0};
    bool valid{false};
};

template <std::size_t PayloadBytes>
[[nodiscard]] paired_block run_abba(std::uint64_t transfers,
                                    vqbench::cpu_pair cpus) {
    using value_type = payload<PayloadBytes>;
    using production_queue = veriqueue::spsc_queue<value_type, 1024>;
    using candidate_queue =
        vqbench::experimental::round4b::selective16_stripe8<value_type, 1024>;

    const auto p1 = measure_once<PayloadBytes, production_queue>(transfers, cpus);
    const auto c1 = measure_once<PayloadBytes, candidate_queue>(transfers, cpus);
    const auto c2 = measure_once<PayloadBytes, candidate_queue>(transfers, cpus);
    const auto p2 = measure_once<PayloadBytes, production_queue>(transfers, cpus);

    const bool valid = p1.valid && c1.valid && c2.valid && p2.valid &&
                       p1.seconds > 0.0 && c1.seconds > 0.0 &&
                       c2.seconds > 0.0 && p2.seconds > 0.0;
    if (!valid) return {};

    const double doubled_transfers = 2.0 * static_cast<double>(transfers);
    const double production_seconds = p1.seconds + p2.seconds;
    const double candidate_seconds = c1.seconds + c2.seconds;
    const double production_tps = doubled_transfers / production_seconds;
    const double candidate_tps = doubled_transfers / candidate_seconds;
    return {production_tps, candidate_tps,
            candidate_tps / production_tps, true};
}

paired_block dispatch(std::size_t payload_bytes, std::uint64_t transfers,
                      vqbench::cpu_pair cpus) {
    switch (payload_bytes) {
    case 8: return run_abba<8>(transfers, cpus);
    case 16: return run_abba<16>(transfers, cpus);
    default: throw std::invalid_argument("payload_bytes must be 8 or 16");
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: bench_hotpath_round4c <payload_bytes> <transfers_per_leg>\n";
        return 2;
    }
    try {
        const auto payload_bytes = static_cast<std::size_t>(std::stoull(argv[1]));
        const auto transfers = static_cast<std::uint64_t>(std::stoull(argv[2]));
        if (transfers == 0) throw std::invalid_argument("transfers must be positive");
        const auto cpus = vqbench::selected_cpu_pair();
        const auto measured = dispatch(payload_bytes, transfers, cpus);

        std::cout << "{\"benchmark\":\"hotpath_round4c_abba\""
                  << ",\"payload_bytes\":" << payload_bytes
                  << ",\"capacity\":1024"
                  << ",\"transfers_per_leg\":" << transfers
                  << ",\"producer_cpu\":" << cpus.producer
                  << ",\"consumer_cpu\":" << cpus.consumer
                  << ",\"topology\":\"" << vqbench::topology_label()
                  << "\",\"valid\":" << (measured.valid ? "true" : "false")
                  << ",\"production_tps\":" << measured.production_tps
                  << ",\"candidate_tps\":" << measured.candidate_tps
                  << ",\"paired_ratio\":" << measured.ratio
                  << ",\"environment\":" << vqbench::environment_json() << "}\n";
        return measured.valid ? 0 : 3;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 2;
    }
}
