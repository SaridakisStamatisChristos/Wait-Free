#pragma once

#include "veriqueue/spsc_queue.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace vqstress {

struct result final {
    std::uint64_t produced{0};
    std::uint64_t consumed{0};
    bool ok{true};
};

template <class PerturbProducer, class PerturbConsumer>
result run(std::uint64_t count, PerturbProducer producer_perturb, PerturbConsumer consumer_perturb) {
    veriqueue::spsc_queue<std::uint64_t, 1024> q;
    std::atomic<bool> failed{false};
    std::atomic<std::uint64_t> produced{0};
    std::atomic<std::uint64_t> consumed{0};

    std::thread producer([&] {
        for (std::uint64_t value = 1; value <= count;) {
            if (q.try_push(value)) {
                produced.store(value, std::memory_order_relaxed);
                ++value;
            } else {
                producer_perturb();
            }
        }
    });

    std::thread consumer([&] {
        for (std::uint64_t expected = 1; expected <= count;) {
            std::uint64_t out = 0;
            if (q.try_pop(out)) {
                if (out != expected) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }
                consumed.store(expected, std::memory_order_relaxed);
                ++expected;
            } else {
                consumer_perturb();
            }
        }
    });

    producer.join();
    consumer.join();
    return {produced.load(), consumed.load(), !failed.load()};
}

} // namespace vqstress
