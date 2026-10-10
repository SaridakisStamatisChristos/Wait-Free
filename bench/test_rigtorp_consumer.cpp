#include "rigtorp_codegen_variants.hpp"

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <random>
#include <stdexcept>
#include <thread>

namespace {
void check(bool condition) {
    if (!condition) throw std::runtime_error("dynamic consumer contract failure");
}

template <bool Split, bool Grouped, std::size_t Capacity,
          std::size_t ControlSpan = 256, bool CachePeer = true, bool CacheOnProgress = false>
void model() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        std::uint64_t, Capacity, std::allocator<std::uint64_t>,
        Split, Grouped, ControlSpan, CachePeer, CacheOnProgress>;
    queue q;
    std::deque<std::uint64_t> expected;
    std::mt19937_64 rng(20261043 + Capacity);
    for (std::uint64_t i = 0; i < 100000; ++i) {
        if ((rng() & 1U) != 0) {
            const bool ok = q.try_push(i);
            check(ok == (expected.size() < Capacity));
            if (ok) expected.push_back(i);
        } else {
            std::uint64_t out = UINT64_MAX;
            const bool ok = q.try_pop(out);
            check(ok == !expected.empty());
            if (ok) {
                check(out == expected.front());
                expected.pop_front();
            } else {
                check(out == UINT64_MAX);
            }
        }
    }
    while (!expected.empty()) {
        std::uint64_t out = 0;
        check(q.try_pop(out) && out == expected.front());
        expected.pop_front();
    }
    for (std::uint64_t i = 0; i < Capacity; ++i) check(q.try_push(i));
    check(!q.try_push(UINT64_MAX)); // One slack slot never becomes usable.
    for (std::uint64_t i = 0; i < Capacity; ++i) {
        std::uint64_t out = UINT64_MAX;
        check(q.try_pop(out) && out == i);
    }
}

struct tracked {
    static inline int alive = 0;
    std::uint64_t value = 0;
    explicit tracked(std::uint64_t v = 0) : value(v) { ++alive; }
    tracked(const tracked& other) : value(other.value) { ++alive; }
    tracked& operator=(tracked&& other) noexcept {
        value = other.value;
        return *this;
    }
    ~tracked() noexcept { --alive; }
};

template <bool Split, bool Grouped, std::size_t ControlSpan = 256,
          bool CachePeer = true, bool CacheOnProgress = false>
void lifetime() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        tracked, 2, std::allocator<tracked>, Split, Grouped, ControlSpan, CachePeer, CacheOnProgress>;
    check(tracked::alive == 0);
    {
        tracked in(42), out;
        {
            queue q;
            check(q.try_push(in));
            check(tracked::alive == 3);
            check(q.try_pop(out) && out.value == 42);
            check(tracked::alive == 2);
            check(q.try_push(in) && q.try_push(in));
            check(!q.try_push(in));
        }
        check(tracked::alive == 2); // Destruction cleans queued objects.
    }
    check(tracked::alive == 0);
}

struct throwing {
    static inline bool fail = false;
    std::uint64_t value = 42;
    throwing() = default;
    throwing(const throwing& other) : value(other.value) {
        if (fail) throw std::runtime_error("expected copy failure");
    }
    throwing& operator=(throwing&&) noexcept = default;
};

template <bool Split, bool Grouped, std::size_t ControlSpan = 256,
          bool CachePeer = true, bool CacheOnProgress = false>
void exception() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        throwing, 2, std::allocator<throwing>, Split, Grouped, ControlSpan, CachePeer, CacheOnProgress>;
    queue q;
    throwing in, out;
    throwing::fail = true;
    bool threw = false;
    try { static_cast<void>(q.try_push(in)); }
    catch (const std::runtime_error&) { threw = true; }
    throwing::fail = false;
    check(threw && !q.try_pop(out));
    check(q.try_push(in) && q.try_pop(out) && out.value == 42);
}

template <bool Split, bool Grouped, std::size_t ControlSpan = 256,
          bool CachePeer = true, bool CacheOnProgress = false>
void concurrent() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        std::uint64_t, 64, std::allocator<std::uint64_t>, Split, Grouped, ControlSpan, CachePeer, CacheOnProgress>;
    queue q;
    std::atomic<bool> failed{false};
    std::thread producer([&] {
        for (std::uint64_t i = 0; i < 200000; ++i) {
            while (!q.try_push(i)) {}
        }
    });
    std::thread consumer([&] {
        for (std::uint64_t i = 0; i < 200000; ++i) {
            std::uint64_t out = UINT64_MAX;
            while (!q.try_pop(out)) {}
            if (out != i) failed.store(true, std::memory_order_relaxed);
        }
    });
    producer.join();
    consumer.join();
    check(!failed.load(std::memory_order_relaxed));
}

template <bool Split, bool Grouped, std::size_t ControlSpan = 256,
          bool CachePeer = true, bool CacheOnProgress = false>
void suite() {
    model<Split, Grouped, 1, ControlSpan, CachePeer, CacheOnProgress>();
    model<Split, Grouped, 2, ControlSpan, CachePeer, CacheOnProgress>();
    model<Split, Grouped, 4, ControlSpan, CachePeer, CacheOnProgress>();
    model<Split, Grouped, 64, ControlSpan, CachePeer, CacheOnProgress>();
    model<Split, Grouped, 1024, ControlSpan, CachePeer, CacheOnProgress>();
    model<Split, Grouped, 65536, ControlSpan, CachePeer, CacheOnProgress>();
    lifetime<Split, Grouped, ControlSpan, CachePeer, CacheOnProgress>();
    exception<Split, Grouped, ControlSpan, CachePeer, CacheOnProgress>();
    concurrent<Split, Grouped, ControlSpan, CachePeer, CacheOnProgress>();
}
} // namespace

int main() {
    using namespace vqbench::experimental::rigtorp_codegen;
    static_assert(sizeof(dynamic_raw_queue<std::uint64_t, 64>) ==
                  sizeof(dynamic_split_queue<std::uint64_t, 64>));
    static_assert(alignof(dynamic_raw_queue<std::uint64_t, 64>) ==
                  alignof(dynamic_split_queue<std::uint64_t, 64>));
    static_assert(sizeof(dynamic_grouped_queue<std::uint64_t, 64>) <
                  sizeof(dynamic_raw_queue<std::uint64_t, 64>));
    static_assert(alignof(dynamic_grouped_queue<std::uint64_t, 64>) == 256);
    suite<false, false>();
    suite<true, false>();
    suite<false, true>();
    suite<true, true>();
    static_assert(sizeof(dynamic_ctrl64_queue<std::uint64_t, 64>) ==
                  sizeof(dynamic_combined_queue<std::uint64_t, 64>));
    static_assert(alignof(dynamic_ctrl64_queue<std::uint64_t, 64>) ==
                  alignof(dynamic_combined_queue<std::uint64_t, 64>));
    suite<true, true, 64>();
    static_assert(sizeof(dynamic_direct_queue<std::uint64_t, 64>) ==
                  sizeof(dynamic_combined_queue<std::uint64_t, 64>));
    static_assert(alignof(dynamic_direct_queue<std::uint64_t, 64>) ==
                  alignof(dynamic_combined_queue<std::uint64_t, 64>));
    suite<true, true, 256, false>();
    suite<false, true, 256, false>();
    static_assert(sizeof(dynamic_progress_queue<std::uint64_t, 64>) ==
                  sizeof(dynamic_combined_queue<std::uint64_t, 64>));
    static_assert(alignof(dynamic_progress_queue<std::uint64_t, 64>) ==
                  alignof(dynamic_combined_queue<std::uint64_t, 64>));
    suite<true, true, 256, true, true>();
    suite<false, true, 256, true, true>();
}
