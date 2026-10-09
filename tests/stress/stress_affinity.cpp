#include "affinity_support.hpp"
#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <atomic>
#include <cstdint>
#include <thread>

int main() {
    vqtest::run("pinned producer/consumer ordered transfer", [] {
        constexpr std::uint64_t count = 250'000;
        veriqueue::spsc_queue<std::uint64_t, 1024> q;
        const auto [producer_cpu, consumer_cpu] = vqstress::affinity_pair();
        std::atomic<bool> failed{false};
        std::atomic<bool> producer_pinned{false};
        std::atomic<bool> consumer_pinned{false};

        std::thread producer([&] {
            producer_pinned.store(vqstress::pin_current_thread(producer_cpu),
                                  std::memory_order_relaxed);
            for (std::uint64_t value = 1; value <= count;) {
                if (q.try_push(value)) {
                    ++value;
                } else {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&] {
            consumer_pinned.store(vqstress::pin_current_thread(consumer_cpu),
                                  std::memory_order_relaxed);
            for (std::uint64_t expected = 1; expected <= count;) {
                std::uint64_t out = 0;
                if (q.try_pop(out)) {
                    if (out != expected) {
                        failed.store(true, std::memory_order_relaxed);
                        return;
                    }
                    ++expected;
                } else {
                    std::this_thread::yield();
                }
            }
        });

        producer.join();
        consumer.join();
        VQ_CHECK(!failed.load());
#if defined(__linux__)
        VQ_CHECK(producer_pinned.load());
        VQ_CHECK(consumer_pinned.load());
#endif
    });
}
