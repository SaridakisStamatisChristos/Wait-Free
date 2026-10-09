#include "environment.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <span>
#include <string_view>
#include <thread>

namespace {

struct result final {
    bool valid{};
    std::uint64_t produced{};
    std::uint64_t consumed{};
    double transfers_per_second{};
};

void wait_for_start(std::atomic<unsigned>& ready, std::atomic<bool>& go) {
    ready.fetch_add(1, std::memory_order_release);
    while (!go.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
}

result run_scalar(std::uint64_t transfers) {
    veriqueue::spsc_queue<std::uint64_t, 1024> q;
    std::atomic<unsigned> ready{0};
    std::atomic<bool> go{false};
    std::atomic<bool> failed{false};
    std::uint64_t produced = 0;
    std::uint64_t consumed = 0;

    std::thread producer([&] {
        wait_for_start(ready, go);
        std::uint64_t next = 1;
        while (next <= transfers && !failed.load(std::memory_order_relaxed)) {
            if (q.try_push(next)) {
                ++next;
                ++produced;
            }
        }
    });

    std::thread consumer([&] {
        wait_for_start(ready, go);
        std::uint64_t expected = 1;
        while (expected <= transfers && !failed.load(std::memory_order_relaxed)) {
            std::uint64_t value = 0;
            if (q.try_pop(value)) {
                if (value != expected) {
                    failed.store(true, std::memory_order_relaxed);
                    break;
                }
                ++expected;
                ++consumed;
            }
        }
    });

    while (ready.load(std::memory_order_acquire) != 2U) std::this_thread::yield();
    const auto start = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto stop = std::chrono::steady_clock::now();

    const double seconds = std::chrono::duration<double>(stop - start).count();
    return {!failed.load(std::memory_order_relaxed) && produced == transfers && consumed == transfers,
            produced, consumed, static_cast<double>(consumed) / seconds};
}

result run_bulk(std::uint64_t transfers, std::size_t batch_width) {
    veriqueue::spsc_queue<std::uint64_t, 1024> q;
    std::atomic<unsigned> ready{0};
    std::atomic<bool> go{false};
    std::atomic<bool> failed{false};
    std::uint64_t produced = 0;
    std::uint64_t consumed = 0;

    std::thread producer([&] {
        wait_for_start(ready, go);
        std::uint64_t next = 1;
        std::array<std::uint64_t, 64> values{};
        while (next <= transfers && !failed.load(std::memory_order_relaxed)) {
            const std::size_t remaining = static_cast<std::size_t>(transfers - next + 1U);
            const std::size_t wanted = (std::min)(batch_width, remaining);
            for (std::size_t i = 0; i < wanted; ++i) values[i] = next + i;
            const std::size_t count =
                q.try_push_bulk(std::span<const std::uint64_t>{values}.first(wanted));
            next += count;
            produced += count;
        }
    });

    std::thread consumer([&] {
        wait_for_start(ready, go);
        std::uint64_t expected = 1;
        std::array<std::uint64_t, 64> values{};
        while (expected <= transfers && !failed.load(std::memory_order_relaxed)) {
            const std::size_t count =
                q.try_pop_bulk(std::span<std::uint64_t>{values}.first(batch_width));
            for (std::size_t i = 0; i < count; ++i) {
                if (values[i] != expected) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }
                ++expected;
                ++consumed;
            }
        }
    });

    while (ready.load(std::memory_order_acquire) != 2U) std::this_thread::yield();
    const auto start = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto stop = std::chrono::steady_clock::now();

    const double seconds = std::chrono::duration<double>(stop - start).count();
    return {!failed.load(std::memory_order_relaxed) && produced == transfers && consumed == transfers,
            produced, consumed, static_cast<double>(consumed) / seconds};
}

result run_consume(std::uint64_t transfers) {
    veriqueue::spsc_queue<std::uint64_t, 1024> q;
    std::atomic<unsigned> ready{0};
    std::atomic<bool> go{false};
    std::atomic<bool> failed{false};
    std::uint64_t produced = 0;
    std::uint64_t consumed = 0;

    std::thread producer([&] {
        wait_for_start(ready, go);
        std::uint64_t next = 1;
        while (next <= transfers && !failed.load(std::memory_order_relaxed)) {
            if (q.try_push(next)) {
                ++next;
                ++produced;
            }
        }
    });

    std::thread consumer([&] {
        wait_for_start(ready, go);
        std::uint64_t expected = 1;
        while (expected <= transfers && !failed.load(std::memory_order_relaxed)) {
            q.try_consume([&](std::uint64_t& value) noexcept {
                if (value != expected) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }
                ++expected;
                ++consumed;
            });
        }
    });

    while (ready.load(std::memory_order_acquire) != 2U) std::this_thread::yield();
    const auto start = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto stop = std::chrono::steady_clock::now();

    const double seconds = std::chrono::duration<double>(stop - start).count();
    return {!failed.load(std::memory_order_relaxed) && produced == transfers && consumed == transfers,
            produced, consumed, static_cast<double>(consumed) / seconds};
}

void emit(std::string_view mode, std::size_t batch, std::uint64_t transfers, const result& r) {
    std::cout << std::fixed << std::setprecision(3)
              << "{\"mode\":\"" << mode
              << "\",\"batch\":" << batch
              << ",\"transfers\":" << transfers
              << ",\"produced\":" << r.produced
              << ",\"consumed\":" << r.consumed
              << ",\"valid\":" << (r.valid ? "true" : "false")
              << ",\"transfers_per_second\":" << r.transfers_per_second
              << ",\"environment\":" << vqbench::environment_json() << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    const std::uint64_t transfers =
        argc > 1 ? static_cast<std::uint64_t>(std::strtoull(argv[1], nullptr, 10)) : 1'000'000ULL;
    if (transfers == 0U) return 2;

    const result scalar = run_scalar(transfers);
    emit("scalar", 1, transfers, scalar);
    if (!scalar.valid) return 1;

    for (const std::size_t batch : {4U, 16U, 64U}) {
        const result bulk = run_bulk(transfers, batch);
        emit("bulk", batch, transfers, bulk);
        if (!bulk.valid) return 1;
    }

    const result consume = run_consume(transfers);
    emit("consume", 1, transfers, consume);
    return consume.valid ? 0 : 1;
}
