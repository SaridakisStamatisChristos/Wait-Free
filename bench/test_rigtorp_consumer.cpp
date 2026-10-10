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
          std::size_t ControlSpan = 256, bool CachePeer = true, bool CacheOnProgress = false, bool SlotStorage = false,
          bool InlineStorage = false, template <class> class Allocator = std::allocator>
void model() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        std::uint64_t, Capacity, Allocator<std::uint64_t>,
        Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage, InlineStorage>;
    auto owner = std::make_unique<queue>();
    queue& q = *owner;
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
          bool CachePeer = true, bool CacheOnProgress = false, bool SlotStorage = false,
          bool InlineStorage = false, template <class> class Allocator = std::allocator>
void lifetime() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        tracked, 2, Allocator<tracked>,
        Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage, InlineStorage>;
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
          bool CachePeer = true, bool CacheOnProgress = false, bool SlotStorage = false,
          bool InlineStorage = false, template <class> class Allocator = std::allocator>
void exception() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        throwing, 2, Allocator<throwing>,
        Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage, InlineStorage>;
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
          bool CachePeer = true, bool CacheOnProgress = false, bool SlotStorage = false,
          bool InlineStorage = false, template <class> class Allocator = std::allocator>
void concurrent() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        std::uint64_t, 64, Allocator<std::uint64_t>,
        Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage, InlineStorage>;
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
          bool CachePeer = true, bool CacheOnProgress = false, bool SlotStorage = false,
          bool InlineStorage = false, template <class> class Allocator = std::allocator>
void suite() {
    model<Split, Grouped, 1, ControlSpan, CachePeer, CacheOnProgress, SlotStorage, InlineStorage, Allocator>();
    model<Split, Grouped, 2, ControlSpan, CachePeer, CacheOnProgress, SlotStorage, InlineStorage, Allocator>();
    model<Split, Grouped, 4, ControlSpan, CachePeer, CacheOnProgress, SlotStorage, InlineStorage, Allocator>();
    model<Split, Grouped, 64, ControlSpan, CachePeer, CacheOnProgress, SlotStorage, InlineStorage, Allocator>();
    model<Split, Grouped, 1024, ControlSpan, CachePeer, CacheOnProgress, SlotStorage, InlineStorage, Allocator>();
    model<Split, Grouped, 65536, ControlSpan, CachePeer, CacheOnProgress, SlotStorage, InlineStorage, Allocator>();
    lifetime<Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage, InlineStorage, Allocator>();
    exception<Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage, InlineStorage, Allocator>();
    concurrent<Split, Grouped, ControlSpan, CachePeer, CacheOnProgress, SlotStorage, InlineStorage, Allocator>();
}

struct allocation_record {
    static inline int allocations = 0;
    static inline int deallocations = 0;
    static inline std::size_t bytes = 0;
    static inline std::size_t alignment = 0;
};

template <class T, bool Aligned = false>
struct recording_allocator {
    using value_type = T;
    template <class U>
    struct rebind { using other = recording_allocator<U, Aligned>; };
    [[nodiscard]] T* allocate(std::size_t count) {
        ++allocation_record::allocations;
        allocation_record::bytes = count * sizeof(T);
        using aligned = vqbench::experimental::rigtorp_codegen::aligned_buffer_allocator<T>;
        allocation_record::alignment = Aligned ? aligned::alignment : alignof(T);
        if constexpr (Aligned) return aligned{}.allocate(count);
        else return std::allocator<T>{}.allocate(count);
    }
    void deallocate(T* pointer, std::size_t count) noexcept {
        ++allocation_record::deallocations;
        using aligned = vqbench::experimental::rigtorp_codegen::aligned_buffer_allocator<T>;
        if constexpr (Aligned) aligned{}.deallocate(pointer, count);
        else std::allocator<T>{}.deallocate(pointer, count);
    }
};

template <std::size_t Alignment>
struct alignas(Alignment) nondefault_value {
    static inline int alive = 0;
    static inline int copies = 0;
    std::uint64_t value;
    nondefault_value() = delete;
    explicit nondefault_value(std::uint64_t v) : value(v) { ++alive; }
    nondefault_value(const nondefault_value& other) : value(other.value) {
        check(reinterpret_cast<std::uintptr_t>(this) % alignof(nondefault_value) == 0);
        ++alive;
        ++copies;
    }
    nondefault_value& operator=(nondefault_value&& other) noexcept {
        value = other.value;
        return *this;
    }
    ~nondefault_value() noexcept { --alive; }
};

using nondefault = nondefault_value<128>;

template <bool Managed, bool Inline = false, bool Split = true, bool Aligned = false,
          class Value = nondefault>
void allocation_and_reuse() {
    using queue = vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<
        Value, 2, recording_allocator<Value, Aligned>, Split, true, 256, true, true, Managed, Inline>;
    allocation_record::allocations = 0;
    allocation_record::deallocations = 0;
    check(Value::alive == 0);
    {
        Value in(0), out(UINT64_MAX);
        {
            queue q;
            check(Value::alive == 2); // No persistent/default-constructed T.
            check(allocation_record::allocations == (Inline ? 0 : 1));
            if constexpr (!Inline) {
                check(allocation_record::bytes == (3 + 2 * (((256 - 1) / sizeof(Value)) + 1)) * sizeof(Value));
                check(allocation_record::alignment == (Aligned ?
                    vqbench::experimental::rigtorp_codegen::aligned_buffer_allocator<Value>::alignment :
                    alignof(Value)));
                if constexpr (Aligned) {
                    check(q.buffer_offsets() == std::array<std::size_t, 6>{});
                }
            }
            for (std::uint64_t i = 0; i < 1000; ++i) {
                in.value = i;
                check(q.try_push(in) && q.try_push(in));
                check(Value::alive == 4);
                const int copies = Value::copies;
                check(!q.try_push(in) && Value::copies == copies);
                check(q.try_pop(out) && out.value == i);
                check(q.try_pop(out) && out.value == i);
                check(Value::alive == 2);
                out.value = UINT64_MAX;
                check(!q.try_pop(out) && out.value == UINT64_MAX);
            }
            check(q.try_push(in) && q.try_push(in)); // Cleanup of live leftovers.
        }
        check(Value::alive == 2);
        check(allocation_record::deallocations == (Inline ? 0 : 1));
    }
    check(Value::alive == 0);
}

void managed_storage_contract() {
    allocation_and_reuse<false>();
    allocation_and_reuse<true>();
    allocation_and_reuse<true, true, true>();
    allocation_and_reuse<true, true, false>();
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
    suite<true, true, 256, true, true, true, true>();
    suite<false, true, 256, true, true, true, true>();
    static_assert(std::is_nothrow_default_constructible_v<inline_managed_queue<nondefault, 2>>);
    static_assert(!std::is_nothrow_default_constructible_v<dynamic_managed_queue<nondefault, 2>>);
    managed_storage_contract();
    suite<true, true, 256, true, true, true, false, aligned_buffer_allocator>();
    suite<false, true, 256, true, true, true, false, aligned_buffer_allocator>();
    allocation_and_reuse<true, false, true, true>();
    allocation_and_reuse<true, false, false, true>();
    allocation_and_reuse<true, false, true, true, nondefault_value<512>>();
    allocation_and_reuse<true, false, false, true, nondefault_value<512>>();
    static_assert(std::is_empty_v<aligned_buffer_allocator<std::uint64_t>>);
    static_assert(!std::is_nothrow_default_constructible_v<dynamic_aligned_queue<nondefault, 2>>);
    bool overflow_threw = false;
    try {
        static_cast<void>(aligned_buffer_allocator<nondefault>{}.allocate(
            std::numeric_limits<std::size_t>::max()));
    } catch (const std::bad_array_new_length&) { overflow_threw = true; }
    check(overflow_threw);
}

