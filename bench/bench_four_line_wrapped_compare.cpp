#include "affinity.hpp"
#include "environment.hpp"
#include "four_line_wrapped_variants.hpp"
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
constexpr std::string_view rigtorp_commit = "59a6a938513ea5004817383711ed35d32385d3ee";
constexpr std::string_view moodycamel_commit = "6867b56452352acf077fccd5f6cc7e3a8cfde0fb";
constexpr std::string_view drogalis_commit = "c959bd4f7204dd73c4e75a2b00c3459c3a5e9a96";

template <std::size_t Bytes>
struct payload final {
    static_assert(Bytes > sizeof(std::uint64_t));
    std::uint64_t sequence{0};
    std::array<std::byte, Bytes - sizeof(std::uint64_t)> data{};
};
template <> struct payload<8> final { std::uint64_t sequence{0}; };

template <std::size_t Bytes>
[[nodiscard]] payload<Bytes> make_payload(std::uint64_t sequence) noexcept {
    payload<Bytes> value{};
    value.sequence = sequence;
    if constexpr (Bytes > 8) {
        const auto marker = static_cast<unsigned char>((sequence * 131U) & 0xffU);
        for (auto& byte : value.data) byte = static_cast<std::byte>(marker);
    }
    return value;
}

template <std::size_t Bytes>
[[nodiscard]] bool valid_payload(const payload<Bytes>& value, std::uint64_t expected) noexcept {
    if (value.sequence != expected) return false;
    if constexpr (Bytes > 8) {
        const auto marker = static_cast<std::byte>(static_cast<unsigned char>((expected * 131U) & 0xffU));
        for (const auto byte : value.data) if (byte != marker) return false;
    }
    return true;
}

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
    std::atomic<bool> start{false}, failed{false}, producer_pinned{false}, consumer_pinned{false};
    std::atomic<unsigned> ready{0};
    std::atomic<std::uint64_t> produced{0}, consumed{0}, checksum{0};

    std::thread producer([&] {
        producer_pinned.store(vqbench::pin_current_thread(producer_cpu), std::memory_order_relaxed);
        ready.fetch_add(1U, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        for (std::uint64_t sequence = 0; sequence < transfers && !failed.load(std::memory_order_relaxed);) {
            const auto value = make_payload<Bytes>(sequence);
            if (push(value)) { ++sequence; produced.store(sequence, std::memory_order_relaxed); }
        }
    });

    std::thread consumer([&] {
        consumer_pinned.store(vqbench::pin_current_thread(consumer_cpu), std::memory_order_relaxed);
        ready.fetch_add(1U, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        std::uint64_t local_checksum = 0;
        for (std::uint64_t expected = 0; expected < transfers && !failed.load(std::memory_order_relaxed);) {
            payload<Bytes> out{};
            if (pop(out)) {
                if (!valid_payload(out, expected)) { failed.store(true, std::memory_order_relaxed); break; }
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
    producer.join(); consumer.join();
    const auto end = std::chrono::steady_clock::now();

    const bool pp = producer_pinned.load(std::memory_order_relaxed);
    const bool cp = consumer_pinned.load(std::memory_order_relaxed);
    const auto pc = produced.load(std::memory_order_relaxed);
    const auto cc = consumed.load(std::memory_order_relaxed);
    const bool valid = pp && cp && !failed.load(std::memory_order_relaxed) && pc == transfers && cc == transfers;
    const double seconds = std::chrono::duration<double>(end - begin).count();
    return {valid ? static_cast<double>(transfers) / seconds : 0.0, valid, pp, cp, pc, cc,
            checksum.load(std::memory_order_relaxed)};
}

void emit(std::string_view name, const run_result& result, std::uint64_t transfers,
          std::size_t payload_bytes, vqbench::cpu_pair cpus) {
    const bool affinity_valid = result.producer_pinned && result.consumer_pinned;
    std::cout << "{\"benchmark\":\"baseline_compare_v2\",\"implementation\":\"" << name
              << "\",\"payload_bytes\":" << payload_bytes
              << ",\"capacity\":" << configured_capacity
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
run_result run_queue(std::uint64_t transfers, vqbench::cpu_pair cpus) {
    auto q = std::make_unique<Queue>();
    return run_pair<sizeof(Payload)>([&](const Payload& v){ return q->try_push(v); },
                                     [&](Payload& out){ return q->try_pop(out); },
                                     transfers, cpus.producer, cpus.consumer);
}

template <class Payload>
run_result run_impl(std::string_view impl, std::uint64_t transfers, vqbench::cpu_pair cpus) {
    using namespace vqbench::experimental::four_line_wrapped;
    if (impl == "veriqueue") return run_queue<veriqueue::spsc_queue<Payload, configured_capacity>, Payload>(transfers, cpus);
    if (impl == "grouped_inline") return run_queue<grouped_inline<Payload, configured_capacity>, Payload>(transfers, cpus);
    if (impl == "four_line_inline") return run_queue<four_line_inline<Payload, configured_capacity>, Payload>(transfers, cpus);
    if (impl == "four_line_heap") return run_queue<four_line_heap<Payload, configured_capacity>, Payload>(transfers, cpus);
    if (impl == "rigtorp") {
        auto q = std::make_unique<rigtorp::SPSCQueue<Payload>>(configured_capacity);
        return run_pair<sizeof(Payload)>([&](const Payload& v){ return q->try_push(v); }, [&](Payload& out){
            auto* ptr = q->front(); if (ptr == nullptr) return false; out = *ptr; q->pop(); return true;
        }, transfers, cpus.producer, cpus.consumer);
    }
    if (impl == "boost_lockfree") {
        auto q = std::make_unique<boost::lockfree::spsc_queue<Payload>>(configured_capacity);
        return run_pair<sizeof(Payload)>([&](const Payload& v){ return q->push(v); }, [&](Payload& out){ return q->pop(out); },
                                         transfers, cpus.producer, cpus.consumer);
    }
    if (impl == "moodycamel") {
        auto q = std::make_unique<moodycamel::ReaderWriterQueue<Payload>>(configured_capacity);
        return run_pair<sizeof(Payload)>([&](const Payload& v){ return q->try_enqueue(v); },
                                         [&](Payload& out){ return q->try_dequeue(out); },
                                         transfers, cpus.producer, cpus.consumer);
    }
    if (impl == "drogalis") {
        auto q = std::make_unique<dro::SPSCQueue<Payload>>(configured_capacity);
        return run_pair<sizeof(Payload)>([&](const Payload& v){ return q->try_push(v); }, [&](Payload& out){ return q->try_pop(out); },
                                         transfers, cpus.producer, cpus.consumer);
    }
    throw std::invalid_argument("unknown implementation: " + std::string(impl));
}

template <std::size_t Bytes>
int run_case(std::string_view impl, std::uint64_t transfers, vqbench::cpu_pair cpus) {
    using P = payload<Bytes>;
    const auto r = run_impl<P>(impl, transfers, cpus);
    emit(impl, r, transfers, Bytes, cpus);
    return r.valid ? 0 : 3;
}

int dispatch(std::string_view impl, std::size_t payload_bytes, std::uint64_t transfers, vqbench::cpu_pair cpus) {
    switch (payload_bytes) {
    case 8: return run_case<8>(impl, transfers, cpus);
    case 16: return run_case<16>(impl, transfers, cpus);
    case 64: return run_case<64>(impl, transfers, cpus);
    case 256: return run_case<256>(impl, transfers, cpus);
    default: throw std::invalid_argument("payload must be one of 8,16,64,256");
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 5) return 2;
    try {
        const std::string impl = argv[1];
        const auto capacity = static_cast<std::size_t>(std::stoull(argv[2]));
        const auto payload_bytes = static_cast<std::size_t>(std::stoull(argv[3]));
        const auto transfers = static_cast<std::uint64_t>(std::stoull(argv[4]));
        if (capacity != configured_capacity || transfers == 0) return 2;
        return dispatch(impl, payload_bytes, transfers, vqbench::selected_cpu_pair());
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 2;
    }
}
