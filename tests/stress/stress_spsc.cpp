#include "stress_common.hpp"
#include "test_support.hpp"

#include <atomic>
#include <cstdint>
#include <thread>

int main() {
    vqtest::run("one million ordered transfers", [] {
        const auto res = vqstress::run(1'000'000, [] {}, [] {});
        VQ_CHECK(res.ok);
        VQ_CHECK(res.produced == 1'000'000);
        VQ_CHECK(res.consumed == 1'000'000);
    });

    vqtest::run("advisory size remains physically bounded under wraparound", [] {
        using queue_type = veriqueue::spsc_queue<std::uint64_t, 8, 64, std::uint8_t>;
        queue_type q;

        constexpr std::uint64_t transfers = 250'000;
        std::atomic<bool> producer_done{false};
        std::atomic<bool> consumer_done{false};
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
            producer_done.store(true, std::memory_order_release);
        });

        std::thread consumer([&] {
            for (std::uint64_t expected = 1;
                 expected <= transfers && !failed.load(std::memory_order_relaxed);) {
                std::uint64_t out = 0;
                if (q.try_pop(out)) {
                    if (out != expected) {
                        failed.store(true, std::memory_order_relaxed);
                        break;
                    }
                    ++expected;
                } else {
                    std::this_thread::yield();
                }
            }
            consumer_done.store(true, std::memory_order_release);
        });

        while (!producer_done.load(std::memory_order_acquire) ||
               !consumer_done.load(std::memory_order_acquire)) {
            if (q.size_approx() > q.capacity()) {
                failed.store(true, std::memory_order_relaxed);
                break;
            }
        }

        producer.join();
        consumer.join();

        VQ_CHECK(!failed.load(std::memory_order_relaxed));
        VQ_CHECK(q.size_approx() <= q.capacity());
        VQ_CHECK(q.empty());
    });
}
