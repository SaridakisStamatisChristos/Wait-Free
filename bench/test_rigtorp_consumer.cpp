#include "rigtorp_codegen_variants.hpp"

#include <atomic>
#include <cstddef>
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
          std::size_t ControlSpan = 256, bool CachePeer = true, bool CacheOnProgress = false, bool SlotStorage = false>
void model() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        std::uint64_t, Capacity, std::allocator<std::uint64_t>,
        Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage>;
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
          bool CachePeer = true, bool CacheOnProgress = false, bool SlotStorage = false>
void lifetime() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        tracked, 2, std::allocator<tracked>, Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage>;
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
          bool CachePeer = true, bool CacheOnProgress = false, bool SlotStorage = false>
void exception() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        throwing, 2, std::allocator<throwing>, Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage>;
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
          bool CachePeer = true, bool CacheOnProgress = false, bool SlotStorage = false>
void concurrent() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        std::uint64_t, 64, std::allocator<std::uint64_t>, Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage>;
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
          bool CachePeer = true, bool CacheOnProgress = false, bool SlotStorage = false>
void suite() {
    model<Split, Grouped, 1, ControlSpan, CachePeer, CacheOnProgress, SlotStorage>();
    model<Split, Grouped, 2, ControlSpan, CachePeer, CacheOnProgress, SlotStorage>();
    model<Split, Grouped, 4, ControlSpan, CachePeer, CacheOnProgress, SlotStorage>();
    model<Split, Grouped, 64, ControlSpan, CachePeer, CacheOnProgress, SlotStorage>();
    model<Split, Grouped, 1024, ControlSpan, CachePeer, CacheOnProgress, SlotStorage>();
    model<Split, Grouped, 65536, ControlSpan, CachePeer, CacheOnProgress, SlotStorage>();
    lifetime<Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage>();
    exception<Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage>();
    concurrent<Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage>();
}

struct allocation_record {
    static inline int allocations = 0;
    static inline int deallocations = 0;
    static inline std::size_t bytes = 0;
    static inline std::size_t alignment = 0;
};

template <class T>
struct recording_allocator {
    using value_type = T;
    [[nodiscard]] T* allocate(std::size_t count) {
        ++allocation_record::allocations;
        allocation_record::bytes = count * sizeof(T);
        allocation_record::alignment = alignof(T);
        return std::allocator<T>{}.allocate(count);
    }
    void deallocate(T* pointer, std::size_t count) noexcept {
        ++allocation_record::deallocations;
        std::allocator<T>{}.deallocate(pointer, count);
    }
};

struct alignas(128) nondefault {
    static inline int alive = 0;
    static inline int copies = 0;
    std::uint64_t value;
    nondefault() = delete;
    explicit nondefault(std::uint64_t v) : value(v) { ++alive; }
    nondefault(const nondefault& other) : value(other.value) {
        check(reinterpret_cast<std::uintptr_t>(this) % alignof(nondefault) == 0);
        ++alive;
        ++copies;
    }
    nondefault& operator=(nondefault&& other) noexcept {
        value = other.value;
        return *this;
    }
    ~nondefault() noexcept { --alive; }
};

template <bool Managed>
void allocation_and_reuse() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        nondefault, 2, recording_allocator<nondefault>, true, true, 256, true, true, Managed>;
    allocation_record::allocations = 0;
    allocation_record::deallocations = 0;
    check(nondefault::alive == 0);
    {
        nondefault in(0), out(UINT64_MAX);
        {
            queue q;
            check(nondefault::alive == 2); // No persistent/default-constructed T.
            check(allocation_record::allocations == 1);
            check(allocation_record::bytes == (3 + 2 * 2) * sizeof(nondefault));
            check(allocation_record::alignment == alignof(nondefault));
            for (std::uint64_t i = 0; i < 1000; ++i) {
                in.value = i;
                check(q.try_push(in) && q.try_push(in));
                check(nondefault::alive == 4);
                const int copies = nondefault::copies;
                check(!q.try_push(in) && nondefault::copies == copies);
                check(q.try_pop(out) && out.value == i);
                check(q.try_pop(out) && out.value == i);
                check(nondefault::alive == 2);
                out.value = UINT64_MAX;
                check(!q.try_pop(out) && out.value == UINT64_MAX);
            }
            check(q.try_push(in) && q.try_push(in)); // Cleanup of live leftovers.
        }
        check(nondefault::alive == 2);
        check(allocation_record::deallocations == 1);
    }
    check(nondefault::alive == 0);
}

void managed_storage_contract() {
    allocation_and_reuse<false>();
    allocation_and_reuse<true>();
    using namespace vqbench::experimental::rigtorp_codegen;
    static_assert(sizeof(dynamic_managed_queue<nondefault, 2>) ==
                  sizeof(dynamic_progress_queue<nondefault, 2>));
    static_assert(alignof(dynamic_managed_queue<nondefault, 2>) ==
                  alignof(dynamic_progress_queue<nondefault, 2>));
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
    static_assert(sizeof(dynamic_managed_queue<std::uint64_t, 64>) ==
                  sizeof(dynamic_progress_queue<std::uint64_t, 64>));
    static_assert(alignof(dynamic_managed_queue<std::uint64_t, 64>) ==
                  alignof(dynamic_progress_queue<std::uint64_t, 64>));
    suite<true, true, 256, true, true, true>();
    suite<false, true, 256, true, true, true>();
    managed_storage_contract();
}
