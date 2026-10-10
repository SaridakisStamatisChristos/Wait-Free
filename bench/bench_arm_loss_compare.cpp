#include "affinity.hpp"
#include "arm_loss_variants.hpp"
#include "environment.hpp"
#include "experimental_policy_variants.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <boost/lockfree/spsc_queue.hpp>
#include <boost/version.hpp>
#include <dro/spsc-queue.hpp>
#include <readerwriterqueue.h>
#include <rigtorp/SPSCQueue.h>

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

#ifndef VERIQUEUE_POLICY_CAPACITY
#define VERIQUEUE_POLICY_CAPACITY 1024
#endif

namespace {

constexpr std::size_t configured_capacity = static_cast<std::size_t>(VERIQUEUE_POLICY_CAPACITY);
static_assert(configured_capacity >= 1 && (configured_capacity & (configured_capacity - 1)) == 0);

constexpr std::string_view rigtorp_commit = "59a6a938513ea5004817383711ed35d32385d3ee";
constexpr std::string_view moodycamel_commit = "6867b56452352acf077fccd5f6cc7e3a8cfde0fb";
constexpr std::string_view drogalis_commit = "c959bd4f7204dd73c4e75a2b00c3459c3a5e9a96";

template <std::size_t Bytes>
struct payload final {
    static_assert(Bytes > sizeof(std::uint64_t));
    std::uint64_t sequence{0};
    std::array<std::byte, Bytes - sizeof(std::uint64_t)> data{};
};

template <>
struct payload<sizeof(std::uint64_t)> final {
    std::uint64_t sequence{0};
};

template <std::size_t Bytes>
[[nodiscard]] payload<Bytes> make_payload(std::uint64_t sequence) noexcept {
    payload<Bytes> value{};
    value.sequence = sequence;
    if constexpr (Bytes > sizeof(std::uint64_t)) {
        const auto marker = static_cast<unsigned char>((sequence * 131U) & 0xffU);
        for (auto& byte : value.data) byte = static_cast<std::byte>(marker);
    }
    return value;
}

template <std::size_t Bytes>
[[nodiscard]] bool valid_payload(const payload<Bytes>& value, std::uint64_t expected) noexcept {
    if (value.sequence != expected) return false;
    if constexpr (Bytes > sizeof(std::uint64_t)) {
        const auto marker = static_cast<std::byte>(
            static_cast<unsigned char>((expected * 131U) & 0xffU));
        for (const auto byte : value.data) {
            if (byte != marker) return false;
        }
    }
    return true;
}

static_assert(sizeof(payload<8>) == 8);
static_assert(sizeof(payload<16>) == 16);
static_assert(sizeof(payload<64>) == 64);
static_assert(sizeof(payload<256>) == 256);

struct run_result final {
    double rate{0.0};
    bool valid{false};
    bool producer_pinned{false};
    bool consumer_pinned{false};
    std::uint64_t produced{0};
    std::uint64_t consumed{0};
    std::uint64_t checksum{0};
};

template <std::size_t Bytes, class Push, class Pop>
run_result run_pair(Push&& push, Pop&& pop, std::uint64_t transfers,
                    unsigned producer_cpu, unsigned consumer_cpu) {
    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    std::atomic<bool> producer_pinned{false};
    std::atomic<bool> consumer_pinned{false};
    std::atomic<unsigned> ready{0};
    std::atomic<std::uint64_t> produced{0};
    std::atomic<std::uint64_t> consumed{0};
    std::atomic<std::uint64_t> checksum{0};

    std::thread producer([&] {
        producer_pinned.store(vqbench::pin_current_thread(producer_cpu), std::memory_order_relaxed);
        ready.fetch_add(1U, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        for (std::uint64_t sequence = 0;
             sequence < transfers && !failed.load(std::memory_order_relaxed);) {
            const auto value = make_payload<Bytes>(sequence);
            if (push(value)) {
                ++sequence;
                produced.store(sequence, std::memory_order_relaxed);
            }
        }
    });

    std::thread consumer([&] {
        consumer_pinned.store(vqbench::pin_current_thread(consumer_cpu), std::memory_order_relaxed);
        ready.fetch_add(1U, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        std::uint64_t local_checksum = 0;
        for (std::uint64_t expected = 0;
             expected < transfers && !failed.load(std::memory_order_relaxed);) {
            payload<Bytes> out{};
            if (pop(out)) {
                if (!valid_payload(out, expected)) {
                    failed.store(true, std::memory_order_relaxed);
                    break;
                }
                local_checksum += out.sequence * 0x9e3779b185ebca87ULL;
                ++expected;
                consumed.store(expected, std::memory_order_relaxed);
            }
        }
        checksum.store(local_checksum, std::memory_order_relaxed);
    });

    while (ready.load(std::memory_order_acquire) != 2U) std::this_thread::yield();
    const auto begin = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto end = std::chrono::steady_clock::now();

    const bool producer_pin_ok = producer_pinned.load(std::memory_order_relaxed);
    const bool consumer_pin_ok = consumer_pinned.load(std::memory_order_relaxed);
    const auto produced_count = produced.load(std::memory_order_relaxed);
    const auto consumed_count = consumed.load(std::memory_order_relaxed);
    const bool valid = producer_pin_ok && consumer_pin_ok &&
                       !failed.load(std::memory_order_relaxed) &&
                       produced_count == transfers && consumed_count == transfers;
    const double seconds = std::chrono::duration<double>(end - begin).count();
    return {
        valid ? static_cast<double>(transfers) / seconds : 0.0,
        valid,
        producer_pin_ok,
        consumer_pin_ok,
        produced_count,
        consumed_count,
        checksum.load(std::memory_order_relaxed),
    };
}

void emit(std::string_view name, const run_result& result, std::uint64_t transfers,
          std::size_t payload_bytes, std::size_t capacity, vqbench::cpu_pair cpus) {
    const bool affinity_valid = result.producer_pinned && result.consumer_pinned;
    std::cout << "{\"benchmark\":\"baseline_compare_v2\",\"implementation\":\"" << name
              << "\",\"payload_bytes\":" << payload_bytes
              << ",\"capacity\":" << capacity
              << ",\"producer_cpu\":" << cpus.producer
              << ",\"consumer_cpu\":" << cpus.consumer
              << ",\"topology\":\"" << vqbench::topology_label()
              << "\",\"transfers\":" << transfers
              << ",\"pinning_requested\":true"
              << ",\"producer_pinned\":" << (result.producer_pinned ? "true" : "false")
              << ",\"consumer_pinned\":" << (result.consumer_pinned ? "true" : "false")
              << ",\"affinity_valid\":" << (affinity_valid ? "true" : "false")
              << ",\"valid\":" << (result.valid ? "true" : "false")
              << ",\"produced\":" << result.produced
              << ",\"consumed\":" << result.consumed
              << ",\"checksum\":" << result.checksum
              << ",\"transfers_per_second\":" << result.rate
              << ",\"boost_version\":" << BOOST_VERSION
              << ",\"rigtorp_commit\":\"" << rigtorp_commit
              << "\",\"moodycamel_commit\":\"" << moodycamel_commit
              << "\",\"drogalis_commit\":\"" << drogalis_commit
              << "\",\"environment\":" << vqbench::environment_json() << "}\n";
}

template <class Queue, class Payload>
run_result run_veriqueue_style(std::uint64_t transfers, vqbench::cpu_pair cpus) {
    auto q = std::make_unique<Queue>();
    return run_pair<sizeof(Payload)>(
        [&](const Payload& value) { return q->try_push(value); },
        [&](Payload& out) { return q->try_pop(out); },
        transfers, cpus.producer, cpus.consumer);
}

template <class Payload, std::size_t Capacity>
run_result run_implementation(std::string_view implementation, std::uint64_t transfers,
                              vqbench::cpu_pair cpus) {
    using namespace vqbench::experimental::policy;
    using namespace vqbench::experimental::arm_loss;

    if (implementation == "veriqueue")
        return run_veriqueue_style<veriqueue::spsc_queue<Payload, Capacity>, Payload>(transfers, cpus);
    if (implementation == "raw_cached_seq")
        return run_veriqueue_style<raw_cached_seq<Payload, Capacity>, Payload>(transfers, cpus);
    if (implementation == "raw_cached_global")
        return run_veriqueue_style<raw_cached_global<Payload, Capacity>, Payload>(transfers, cpus);
    if (implementation == "raw_cached_tiled")
        return run_veriqueue_style<raw_cached_tiled<Payload, Capacity>, Payload>(transfers, cpus);
    if (implementation == "tile32_l8")
        return run_veriqueue_style<tile32_l8<Payload, Capacity>, Payload>(transfers, cpus);
    if (implementation == "tile64_l8")
        return run_veriqueue_style<tile64_l8<Payload, Capacity>, Payload>(transfers, cpus);
    if (implementation == "tile128_l8")
        return run_veriqueue_style<tile128_l8<Payload, Capacity>, Payload>(transfers, cpus);
    if (implementation == "tile256_l8")
        return run_veriqueue_style<tile256_l8<Payload, Capacity>, Payload>(transfers, cpus);

    if (implementation == "rigtorp") {
        auto q = std::make_unique<rigtorp::SPSCQueue<Payload>>(Capacity);
        return run_pair<sizeof(Payload)>(
            [&](const Payload& value) { return q->try_push(value); },
            [&](Payload& out) {
                auto* ptr = q->front();
                if (ptr == nullptr) return false;
                out = *ptr;
                q->pop();
                return true;
            }, transfers, cpus.producer, cpus.consumer);
    }
    if (implementation == "boost_lockfree") {
        auto q = std::make_unique<boost::lockfree::spsc_queue<Payload>>(Capacity);
        return run_pair<sizeof(Payload)>(
            [&](const Payload& value) { return q->push(value); },
            [&](Payload& out) { return q->pop(out); },
            transfers, cpus.producer, cpus.consumer);
    }
    if (implementation == "moodycamel") {
        auto q = std::make_unique<moodycamel::ReaderWriterQueue<Payload>>(Capacity);
        return run_pair<sizeof(Payload)>(
            [&](const Payload& value) { return q->try_enqueue(value); },
            [&](Payload& out) { return q->try_dequeue(out); },
            transfers, cpus.producer, cpus.consumer);
    }
    if (implementation == "drogalis") {
        auto q = std::make_unique<dro::SPSCQueue<Payload>>(Capacity);
        return run_pair<sizeof(Payload)>(
            [&](const Payload& value) { return q->try_push(value); },
            [&](Payload& out) { return q->try_pop(out); },
            transfers, cpus.producer, cpus.consumer);
    }
    throw std::invalid_argument("unknown implementation: " + std::string(implementation));
}

template <std::size_t Capacity, std::size_t Bytes>
int run_case(std::string_view implementation, std::uint64_t transfers, vqbench::cpu_pair cpus) {
    using value_type = payload<Bytes>;
    const auto result = run_implementation<value_type, Capacity>(implementation, transfers, cpus);
    emit(implementation, result, transfers, Bytes, Capacity, cpus);
    return result.valid ? 0 : 3;
}

template <std::size_t Capacity>
int dispatch_payload(std::string_view implementation, std::size_t payload_bytes,
                     std::uint64_t transfers, vqbench::cpu_pair cpus) {
    switch (payload_bytes) {
    case 8: return run_case<Capacity, 8>(implementation, transfers, cpus);
    case 16: return run_case<Capacity, 16>(implementation, transfers, cpus);
    case 64: return run_case<Capacity, 64>(implementation, transfers, cpus);
    case 256: return run_case<Capacity, 256>(implementation, transfers, cpus);
    default: throw std::invalid_argument("payload must be one of 8,16,64,256");
    }
}

int dispatch(std::string_view implementation, std::size_t capacity,
             std::size_t payload_bytes, std::uint64_t transfers, vqbench::cpu_pair cpus) {
    if (capacity != configured_capacity)
        throw std::invalid_argument("runtime capacity does not match VERIQUEUE_POLICY_CAPACITY");
    return dispatch_payload<configured_capacity>(implementation, payload_bytes, transfers, cpus);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: bench_arm_loss_compare <implementation> <capacity> <payload-bytes> <transfers>\n";
        return 2;
    }
    try {
        const std::string implementation = argv[1];
        const auto capacity = static_cast<std::size_t>(std::stoull(argv[2]));
        const auto payload_bytes = static_cast<std::size_t>(std::stoull(argv[3]));
        const auto transfers = static_cast<std::uint64_t>(std::stoull(argv[4]));
        if (transfers == 0) throw std::invalid_argument("transfers must be positive");
        return dispatch(implementation, capacity, payload_bytes, transfers,
                        vqbench::selected_cpu_pair());
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 2;
    }
}
