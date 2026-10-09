#pragma once

#include "veriqueue/detail/config.hpp"
#include "veriqueue/detail/slot.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace veriqueue {

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4324) // Cache-line alignas intentionally adds tail padding.
#endif

// The controlled hot-path ablation showed broad scalar wins on tested AArch64
// GitHub runners, while an unconditional switch materially regressed GCC/x86-64
// cells. Keep the proven legacy owner-local cursor on non-ARM64 targets and use
// the published atomic itself as the owner cursor on ARM64. Peer synchronization
// remains acquire/release on both paths.
//
// VERIQUEUE_FORCE_SINGLE_OWNER_CURSOR and VERIQUEUE_FORCE_DUAL_OWNER_CURSOR are
// verification-only compile switches used to exercise either state machine on a
// host architecture without changing the default production selection.
#if defined(VERIQUEUE_FORCE_SINGLE_OWNER_CURSOR) && defined(VERIQUEUE_FORCE_DUAL_OWNER_CURSOR)
#error "Only one VeriQueue owner-cursor strategy may be forced"
#endif

#if defined(VERIQUEUE_FORCE_SINGLE_OWNER_CURSOR)
#define VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR 1
#elif defined(VERIQUEUE_FORCE_DUAL_OWNER_CURSOR)
#define VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR 0
#elif defined(__aarch64__) || defined(_M_ARM64)
#define VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR 1
#else
#define VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR 0
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define VERIQUEUE_DETAIL_ARM64_STORAGE_STRIPE 1
#else
#define VERIQUEUE_DETAIL_ARM64_STORAGE_STRIPE 0
#endif

#if defined(__aarch64__) && defined(__GNUC__) && !defined(__clang__)
#define VERIQUEUE_DETAIL_GCC_ARM64 1
#else
#define VERIQUEUE_DETAIL_GCC_ARM64 0
#endif

template <
    class T,
    std::size_t Capacity,
    std::size_t CacheLine = detail::default_cache_line,
    class Index = std::size_t>
class spsc_queue final {
    static_assert(Capacity >= 1, "Capacity must be at least one");
    static_assert(detail::is_power_of_two(Capacity), "Capacity must be a power of two");
    static_assert(std::is_unsigned_v<Index>, "Index must be an unsigned integer type");
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2,
                  "Capacity must be <= max(Index)/2 for modular-distance correctness");
    static_assert(std::atomic<Index>::is_always_lock_free,
                  "The selected Index atomic must be always lock-free");
    static_assert(detail::is_power_of_two(CacheLine), "CacheLine must be a power of two");
    static_assert(CacheLine >= alignof(std::atomic<Index>),
                  "CacheLine must satisfy atomic alignment");
    static_assert(std::is_nothrow_destructible_v<T>,
                  "T must be nothrow destructible because consuming paths and queue destruction are noexcept");

#ifdef VERIQUEUE_DISABLE_PADDING
    static constexpr std::size_t control_alignment = alignof(std::atomic<Index>);
#else
    static constexpr std::size_t control_alignment = CacheLine;
#endif

    static constexpr std::size_t storage_alignment =
        control_alignment < alignof(detail::slot<T>) ? alignof(detail::slot<T>) : control_alignment;

    static constexpr Index capacity_index = static_cast<Index>(Capacity);
    static constexpr Index mask_index = static_cast<Index>(Capacity - 1);

    // Evidence-backed storage optimization: on AArch64, exactly-16-byte slots
    // benefit from spreading the low three logical-index bits across eight distant
    // storage regions. Capacity remains unchanged and the mapping is a permutation,
    // so queue semantics, object footprint, and synchronization are unaffected.
    // Capacities below eight retain the sequential mapping because an 8-way stripe
    // cannot be represented without changing storage size.
    static constexpr bool use_arm64_16b_storage_stripe =
        VERIQUEUE_DETAIL_ARM64_STORAGE_STRIPE &&
        sizeof(detail::slot<T>) == 16 &&
        Capacity >= 8;

    // Experimental attribution-backed hybrid: GCC/AArch64 8-byte queues at capacity 256
    // keep PR21's single-owner scalar cursor, but consecutive bulk calls use an owner-local
    // shadow cursor with the same field ordering as the pre-PR21 queue. A successful scalar
    // operation invalidates bulk mode; the next bulk call resynchronizes from the published
    // cursor. Non-target state types are unchanged.
    static constexpr bool use_gcc_arm64_8b_bulk_hybrid =
        VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR &&
        VERIQUEUE_DETAIL_GCC_ARM64 &&
        sizeof(detail::slot<T>) == 8 &&
        Capacity == 256;

    struct alignas(control_alignment) single_owner_producer_state final {
        std::atomic<Index> published_tail{0};
        Index cached_head{0};
    };

    struct alignas(control_alignment) single_owner_consumer_state final {
        std::atomic<Index> published_head{0};
        Index cached_tail{0};
    };

    struct alignas(control_alignment) hybrid_producer_state final {
        Index local_tail{0};
        Index cached_head{0};
        std::atomic<Index> published_tail{0};
        bool bulk_active{false};
    };

    struct alignas(control_alignment) hybrid_consumer_state final {
        Index local_head{0};
        Index cached_tail{0};
        std::atomic<Index> published_head{0};
        bool bulk_active{false};
    };

    struct alignas(control_alignment) dual_owner_producer_state final {
        Index local_tail{0};
        Index cached_head{0};
        std::atomic<Index> published_tail{0};
    };

    struct alignas(control_alignment) dual_owner_consumer_state final {
        Index local_head{0};
        Index cached_tail{0};
        std::atomic<Index> published_head{0};
    };

#if VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR
    using producer_state = std::conditional_t<
        use_gcc_arm64_8b_bulk_hybrid, hybrid_producer_state, single_owner_producer_state>;
    using consumer_state = std::conditional_t<
        use_gcc_arm64_8b_bulk_hybrid, hybrid_consumer_state, single_owner_consumer_state>;
#else
    using producer_state = dual_owner_producer_state;
    using consumer_state = dual_owner_consumer_state;
#endif

public:
    using value_type = T;
    using index_type = Index;
    static constexpr std::size_t static_capacity = Capacity;

    spsc_queue() noexcept = default;
    spsc_queue(const spsc_queue&) = delete;
    spsc_queue& operator=(const spsc_queue&) = delete;
    spsc_queue(spsc_queue&&) = delete;
    spsc_queue& operator=(spsc_queue&&) = delete;

    ~spsc_queue() noexcept {
        // Contract: external quiescence. No producer/consumer operation may overlap destruction.
        // In the single-owner strategy the owner cursor is the published atomic itself. Once
        // externally quiescent, relaxed loads are sufficient because destruction is not a
        // synchronization edge.
#if VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR
        Index head = consumer_.published_head.load(std::memory_order_relaxed);
        const Index tail = producer_.published_tail.load(std::memory_order_relaxed);
#else
        Index head = consumer_.local_head;
        const Index tail = producer_.local_tail;
#endif
        while (head != tail) {
            std::destroy_at(slot_for(head).live_ptr());
            ++head;
        }
    }

    template <class... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] bool try_emplace(Args&&... args) {
#if VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR
        Index tail = producer_.published_tail.load(std::memory_order_relaxed);
#else
        Index tail = producer_.local_tail;
#endif

        if (distance(tail, producer_.cached_head) == capacity_index) {
            producer_.cached_head =
                consumer_.published_head.load(std::memory_order_acquire);
            if (distance(tail, producer_.cached_head) == capacity_index) {
                return false;
            }
        }

        std::construct_at(slot_for(tail).storage_ptr(), std::forward<Args>(args)...);
        ++tail;
#if VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR
        if constexpr (use_gcc_arm64_8b_bulk_hybrid) {
            if (producer_.bulk_active) [[unlikely]] producer_.bulk_active = false;
        }
#else
        producer_.local_tail = tail;
#endif
        producer_.published_tail.store(tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        return try_emplace(value);
    }

    [[nodiscard]] bool try_push(T&& value)
        requires std::is_move_constructible_v<T>
    {
        return try_emplace(std::move(value));
    }

    [[nodiscard]] std::size_t try_push_bulk(std::span<const T> values) noexcept
        requires std::is_nothrow_copy_constructible_v<T>
    {
        if (values.empty()) return 0;

#if VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR
        Index tail;
        if constexpr (use_gcc_arm64_8b_bulk_hybrid) {
            if (!producer_.bulk_active) [[unlikely]] {
                producer_.local_tail =
                    producer_.published_tail.load(std::memory_order_relaxed);
                producer_.bulk_active = true;
            }
            tail = producer_.local_tail;
        } else {
            tail = producer_.published_tail.load(std::memory_order_relaxed);
        }
#else
        Index tail = producer_.local_tail;
#endif
        Index used = distance(tail, producer_.cached_head);
        Index available = static_cast<Index>(capacity_index - used);
        const std::size_t target = (std::min)(values.size(), Capacity);

        if (static_cast<std::size_t>(available) < target) {
            producer_.cached_head =
                consumer_.published_head.load(std::memory_order_acquire);
            used = distance(tail, producer_.cached_head);
            available = static_cast<Index>(capacity_index - used);
            if (available == 0) return 0;
        }

        const std::size_t count =
            (std::min)(values.size(), static_cast<std::size_t>(available));

        for (std::size_t i = 0; i < count; ++i) {
            std::construct_at(slot_for(tail).storage_ptr(), values[i]);
            ++tail;
        }

#if VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR
        if constexpr (use_gcc_arm64_8b_bulk_hybrid) producer_.local_tail = tail;
#else
        producer_.local_tail = tail;
#endif
        producer_.published_tail.store(tail, std::memory_order_release);
        return count;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
#if VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR
        Index head = consumer_.published_head.load(std::memory_order_relaxed);
#else
        Index head = consumer_.local_head;
#endif

        if (head == consumer_.cached_tail) {
            consumer_.cached_tail =
                producer_.published_tail.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) {
                return false;
            }
        }

        T* const source = slot_for(head).live_ptr();
        output = std::move(*source);
        std::destroy_at(source);
        ++head;
#if VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR
        if constexpr (use_gcc_arm64_8b_bulk_hybrid) {
            if (consumer_.bulk_active) [[unlikely]] consumer_.bulk_active = false;
        }
#else
        consumer_.local_head = head;
#endif
        consumer_.published_head.store(head, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t try_pop_bulk(std::span<T> output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        if (output.empty()) return 0;

#if VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR
        Index head;
        if constexpr (use_gcc_arm64_8b_bulk_hybrid) {
            if (!consumer_.bulk_active) [[unlikely]] {
                consumer_.local_head =
                    consumer_.published_head.load(std::memory_order_relaxed);
                consumer_.bulk_active = true;
            }
            head = consumer_.local_head;
        } else {
            head = consumer_.published_head.load(std::memory_order_relaxed);
        }
#else
        Index head = consumer_.local_head;
#endif
        Index available = distance(consumer_.cached_tail, head);
        const std::size_t target = (std::min)(output.size(), Capacity);

        if (static_cast<std::size_t>(available) < target) {
            consumer_.cached_tail =
                producer_.published_tail.load(std::memory_order_acquire);
            available = distance(consumer_.cached_tail, head);
            if (available == 0) return 0;
        }

        const std::size_t count =
            (std::min)(output.size(), static_cast<std::size_t>(available));
        for (std::size_t i = 0; i < count; ++i) {
            T* const source = slot_for(head).live_ptr();
            output[i] = std::move(*source);
            std::destroy_at(source);
            ++head;
        }

#if VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR
        if constexpr (use_gcc_arm64_8b_bulk_hybrid) consumer_.local_head = head;
#else
        consumer_.local_head = head;
#endif
        consumer_.published_head.store(head, std::memory_order_release);
        return count;
    }

    template <class F>
        requires std::is_nothrow_invocable_v<F, T&>
    [[nodiscard]] bool try_consume(F&& consumer) noexcept {
#if VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR
        Index head = consumer_.published_head.load(std::memory_order_relaxed);
#else
        Index head = consumer_.local_head;
#endif

        if (head == consumer_.cached_tail) {
            consumer_.cached_tail =
                producer_.published_tail.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) {
                return false;
            }
        }

        T* const source = slot_for(head).live_ptr();
        std::invoke(std::forward<F>(consumer), *source);
        std::destroy_at(source);
        ++head;
#if VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR
        if constexpr (use_gcc_arm64_8b_bulk_hybrid) {
            if (consumer_.bulk_active) [[unlikely]] consumer_.bulk_active = false;
        }
#else
        consumer_.local_head = head;
#endif
        consumer_.published_head.store(head, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        // Advisory concurrent observation: the independently loaded cursors need not
        // belong to one linearizable queue state.
        const Index head = consumer_.published_head.load(std::memory_order_acquire);
        const Index tail = producer_.published_tail.load(std::memory_order_acquire);
        return head == tail;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

    [[nodiscard]] std::size_t size_approx() const noexcept {
        // The two cursors are observed independently. Under concurrency they may come
        // from different logical instants, and unsigned modular subtraction can then
        // produce a value outside the physical queue range. Preserve the deliberately
        // advisory semantics while guaranteeing the useful physical bound [0, Capacity].
        const Index head = consumer_.published_head.load(std::memory_order_acquire);
        const Index tail = producer_.published_tail.load(std::memory_order_acquire);
        const Index observed = distance(tail, head);
        return observed > capacity_index ? Capacity : static_cast<std::size_t>(observed);
    }

#ifdef VERIQUEUE_TESTING
    struct test_snapshot final {
        Index producer_local_tail;
        Index producer_cached_head;
        Index consumer_local_head;
        Index consumer_cached_tail;
        Index published_tail;
        Index published_head;
    };

    [[nodiscard]] test_snapshot testing_snapshot() const noexcept {
#if VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR
        const Index tail = producer_.published_tail.load(std::memory_order_relaxed);
        const Index head = consumer_.published_head.load(std::memory_order_relaxed);
        return {
            tail,
            producer_.cached_head,
            head,
            consumer_.cached_tail,
            tail,
            head};
#else
        return {
            producer_.local_tail,
            producer_.cached_head,
            consumer_.local_head,
            consumer_.cached_tail,
            producer_.published_tail.load(std::memory_order_relaxed),
            consumer_.published_head.load(std::memory_order_relaxed)};
#endif
    }

    [[nodiscard]] static constexpr bool testing_storage_striped() noexcept {
        return use_arm64_16b_storage_stripe;
    }

    [[nodiscard]] static constexpr std::size_t testing_slot_index(Index logical_index) noexcept {
        return physical_slot_index(logical_index);
    }
#endif

private:
    [[nodiscard]] static constexpr Index distance(Index newer, Index older) noexcept {
        return static_cast<Index>(newer - older);
    }

    [[nodiscard]] static constexpr std::size_t physical_slot_index(Index logical_index) noexcept {
        const std::size_t bounded = static_cast<std::size_t>(logical_index & mask_index);
        if constexpr (use_arm64_16b_storage_stripe) {
            constexpr unsigned stripe_bits = 3;
            constexpr unsigned capacity_bits = std::countr_zero(Capacity);
            constexpr std::size_t stripe_mask = 7;
            const std::size_t low = bounded & stripe_mask;
            const std::size_t high = bounded >> stripe_bits;
            return (low << (capacity_bits - stripe_bits)) | high;
        } else {
            return bounded;
        }
    }

    [[nodiscard]] detail::slot<T>& slot_for(Index logical_index) noexcept {
        return slots_[physical_slot_index(logical_index)];
    }

    [[nodiscard]] const detail::slot<T>& slot_for(Index logical_index) const noexcept {
        return slots_[physical_slot_index(logical_index)];
    }

    producer_state producer_{};
    consumer_state consumer_{};
    alignas(storage_alignment) std::array<detail::slot<T>, Capacity> slots_;
};

#undef VERIQUEUE_DETAIL_GCC_ARM64
#undef VERIQUEUE_DETAIL_ARM64_STORAGE_STRIPE
#undef VERIQUEUE_DETAIL_ARM64_SINGLE_OWNER_CURSOR

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace veriqueue
