#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <thread>

int main() {
    vqtest::run("concurrent bulk batches preserve exact FIFO", [] {
        constexpr std::uint64_t transfers = 250'000;
        veriqueue::spsc_queue<std::uint64_t, 256> q;
        std::atomic<bool> failed{false};

        std::thread producer([&] {
            std::uint64_t next = 1;
            std::array<std::uint64_t, 17> batch{};
            while (next <= transfers && !failed.load(std::memory_order_relaxed)) {
                const std::size_t wanted = static_cast<std::size_t>((next % batch.size()) + 1U);
                const std::size_t remaining =
                    static_cast<std::size_t>(transfers - next + 1U);
                const std::size_t count = (std::min)(wanted, remaining);
                for (std::size_t i = 0; i < count; ++i) batch[i] = next + i;
                const std::size_t pushed =
                    q.try_push_bulk(std::span<const std::uint64_t>{batch}.first(count));
                if (pushed == 0) {
                    std::this_thread::yield();
                } else {
                    next += pushed;
                }
            }
        });

        std::thread consumer([&] {
            std::uint64_t expected = 1;
            std::array<std::uint64_t, 19> batch{};
            while (expected <= transfers && !failed.load(std::memory_order_relaxed)) {
                const std::size_t popped = q.try_pop_bulk(std::span<std::uint64_t>{batch});
                if (popped == 0) {
                    std::this_thread::yield();
                    continue;
                }
                for (std::size_t i = 0; i < popped; ++i) {
                    if (batch[i] != expected) {
                        failed.store(true, std::memory_order_relaxed);
                        return;
                    }
                    ++expected;
                }
            }
        });

        producer.join();
        consumer.join();
        VQ_CHECK(!failed.load(std::memory_order_relaxed));
        VQ_CHECK(q.empty());
    });

    vqtest::run("concurrent try_consume preserves exact FIFO", [] {
        constexpr std::uint64_t transfers = 200'000;
        veriqueue::spsc_queue<std::uint64_t, 64> q;
        std::atomic<bool> failed{false};

        std::thread producer([&] {
            for (std::uint64_t value = 1;
                 value <= transfers && !failed.load(std::memory_order_relaxed);) {
                if (q.try_push(value)) {
                    ++value;
                } else {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&] {
            std::uint64_t expected = 1;
            while (expected <= transfers && !failed.load(std::memory_order_relaxed)) {
                const bool consumed = q.try_consume([&](std::uint64_t& value) noexcept {
                    if (value != expected) {
                        failed.store(true, std::memory_order_relaxed);
                    } else {
                        ++expected;
                    }
                });
                if (!consumed) std::this_thread::yield();
            }
        });

        producer.join();
        consumer.join();
        VQ_CHECK(!failed.load(std::memory_order_relaxed));
        VQ_CHECK(q.empty());
    });
}
