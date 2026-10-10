#pragma once

#include "experimental_spsc_variants.hpp"
#include "veriqueue/detail/slot.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace vqbench::experimental::round2 {

inline constexpr std::size_t cache_line = 64;

template <class T>
inline constexpr std::size_t storage_alignment =
    cache_line > alignof(veriqueue::detail::slot<T>)
        ? cache_line
        : alignof(veriqueue::detail::slot<T>);

// Round-2 coherence-layout experiment.
//
// The published cursors live on cache lines distinct from producer/consumer
// private state. This lets us test whether peer acquire loads of the published
// cursor unnecessarily disturb the owner's cached peer cursor / local cursor.
// OwnerUsesPublishedAtomic=true keeps the ARM64 production algorithm (the owner
// reloads its own published atomic relaxed). false restores a plain owner-local
// cursor while retaining split publication. CachedLimit switches only the
// producer full test from modular distance to an absolute logical full limit.
// ControlStride=128 tests adjacent-line/pre-fetch isolation without changing the
// synchronization protocol.
template <class T, std::size_t Capacity, bool OwnerUsesPublishedAtomic,
          bool CachedLimit, std::size_t ControlStride = cache_line,
          class Index = std::size_t>
class split_coherence_queue final {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    static_assert((ControlStride & (ControlStride - 1)) == 0);
    static_assert(ControlStride >= alignof(std::atomic<Index>));
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2);
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_nothrow_destructible_v<T>);

    static constexpr Index capacity_index = static_cast<Index>(Capacity);
    static constexpr Index mask_index = static_cast<Index>(Capacity - 1);

    struct alignas(ControlStride) producer_private final {
        Index local_tail{0};
        Index cached_remote{0};
    };

    struct alignas(ControlStride) consumer_private final {
        Index local_head{0};
        Index cached_tail{0};
    };

    struct alignas(ControlStride) published_cursor final {
        std::atomic<Index> value{0};
    };

public:
    explicit split_coherence_queue(Index initial_cursor = 0) noexcept {
        producer_.local_tail = initial_cursor;
        producer_.cached_remote = CachedLimit
            ? static_cast<Index>(initial_cursor + capacity_index)
            : initial_cursor;
        consumer_.local_head = initial_cursor;
        consumer_.cached_tail = initial_cursor;
        published_tail_.value.store(initial_cursor, std::memory_order_relaxed);
        published_head_.value.store(initial_cursor, std::memory_order_relaxed);
    }

    split_coherence_queue(const split_coherence_queue&) = delete;
    split_coherence_queue& operator=(const split_coherence_queue&) = delete;

    ~split_coherence_queue() noexcept {
        Index head = owner_head();
        const Index tail = owner_tail();
        while (head != tail) {
            std::destroy_at(slot_for(head).live_ptr());
            ++head;
        }
    }

    template <class... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] bool try_emplace(Args&&... args) {
        Index tail = owner_tail();
        if constexpr (CachedLimit) {
            if (tail == producer_.cached_remote) {
                const Index head = published_head_.value.load(std::memory_order_acquire);
                producer_.cached_remote = static_cast<Index>(head + capacity_index);
                if (tail == producer_.cached_remote) return false;
            }
        } else {
            if (distance(tail, producer_.cached_remote) == capacity_index) {
                producer_.cached_remote = published_head_.value.load(std::memory_order_acquire);
                if (distance(tail, producer_.cached_remote) == capacity_index) return false;
            }
        }

        std::construct_at(slot_for(tail).storage_ptr(), std::forward<Args>(args)...);
        ++tail;
        publish_tail(tail);
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

        Index tail = owner_tail();
        Index available = producer_available(tail);
        const std::size_t target = (std::min)(values.size(), Capacity);
        if (static_cast<std::size_t>(available) < target) {
            const Index head = published_head_.value.load(std::memory_order_acquire);
            if constexpr (CachedLimit) {
                producer_.cached_remote = static_cast<Index>(head + capacity_index);
            } else {
                producer_.cached_remote = head;
            }
            available = producer_available(tail);
            if (available == 0) return 0;
        }

        const std::size_t count =
            (std::min)(values.size(), static_cast<std::size_t>(available));
        for (std::size_t i = 0; i < count; ++i) {
            std::construct_at(slot_for(tail).storage_ptr(), values[i]);
            ++tail;
        }
        publish_tail(tail);
        return count;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        Index head = owner_head();
        if (head == consumer_.cached_tail) {
            consumer_.cached_tail = published_tail_.value.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) return false;
        }

        T* const source = slot_for(head).live_ptr();
        output = std::move(*source);
        std::destroy_at(source);
        ++head;
        publish_head(head);
        return true;
    }

    [[nodiscard]] std::size_t try_pop_bulk(std::span<T> output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        if (output.empty()) return 0;

        Index head = owner_head();
        Index available = distance(consumer_.cached_tail, head);
        const std::size_t target = (std::min)(output.size(), Capacity);
        if (static_cast<std::size_t>(available) < target) {
            consumer_.cached_tail = published_tail_.value.load(std::memory_order_acquire);
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
        publish_head(head);
        return count;
    }

    template <class F>
        requires std::is_nothrow_invocable_v<F, T&>
    [[nodiscard]] bool try_consume(F&& consumer) noexcept {
        Index head = owner_head();
        if (head == consumer_.cached_tail) {
            consumer_.cached_tail = published_tail_.value.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) return false;
        }
        T* const source = slot_for(head).live_ptr();
        std::invoke(std::forward<F>(consumer), *source);
        std::destroy_at(source);
        ++head;
        publish_head(head);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        const Index head = published_head_.value.load(std::memory_order_acquire);
        const Index tail = published_tail_.value.load(std::memory_order_acquire);
        return head == tail;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }

    [[nodiscard]] std::size_t size_approx() const noexcept {
        const Index head = published_head_.value.load(std::memory_order_acquire);
        const Index tail = published_tail_.value.load(std::memory_order_acquire);
        const Index observed = distance(tail, head);
        return observed > capacity_index ? Capacity : static_cast<std::size_t>(observed);
    }

private:
    [[nodiscard]] Index owner_tail() const noexcept {
        if constexpr (OwnerUsesPublishedAtomic) {
            return published_tail_.value.load(std::memory_order_relaxed);
        } else {
            return producer_.local_tail;
        }
    }

    [[nodiscard]] Index owner_head() const noexcept {
        if constexpr (OwnerUsesPublishedAtomic) {
            return published_head_.value.load(std::memory_order_relaxed);
        } else {
            return consumer_.local_head;
        }
    }

    void publish_tail(Index tail) noexcept {
        if constexpr (!OwnerUsesPublishedAtomic) producer_.local_tail = tail;
        published_tail_.value.store(tail, std::memory_order_release);
    }

    void publish_head(Index head) noexcept {
        if constexpr (!OwnerUsesPublishedAtomic) consumer_.local_head = head;
        published_head_.value.store(head, std::memory_order_release);
    }

    [[nodiscard]] Index producer_available(Index tail) const noexcept {
        if constexpr (CachedLimit) {
            return distance(producer_.cached_remote, tail);
        } else {
            const Index used = distance(tail, producer_.cached_remote);
            return static_cast<Index>(capacity_index - used);
        }
    }

    [[nodiscard]] static constexpr Index distance(Index newer, Index older) noexcept {
        return static_cast<Index>(newer - older);
    }

    [[nodiscard]] veriqueue::detail::slot<T>& slot_for(Index logical_index) noexcept {
        return slots_[static_cast<std::size_t>(logical_index & mask_index)];
    }

    producer_private producer_{};
    published_cursor published_tail_{};
    consumer_private consumer_{};
    published_cursor published_head_{};
    alignas(storage_alignment<T>) std::array<veriqueue::detail::slot<T>, Capacity> slots_{};
};

template <class T, std::size_t Capacity>
using split_atomic_owner =
    split_coherence_queue<T, Capacity, true, false, 64, std::size_t>;

template <class T, std::size_t Capacity>
using split_atomic_cached_limit =
    split_coherence_queue<T, Capacity, true, true, 64, std::size_t>;

template <class T, std::size_t Capacity>
using split_local_owner =
    split_coherence_queue<T, Capacity, false, false, 64, std::size_t>;

template <class T, std::size_t Capacity>
using split_local_cached_limit =
    split_coherence_queue<T, Capacity, false, true, 64, std::size_t>;

template <class T, std::size_t Capacity>
using split_atomic_owner_128 =
    split_coherence_queue<T, Capacity, true, false, 128, std::size_t>;

template <class T, std::size_t Capacity>
using single_owner_u32 =
    vqbench::experimental::single_owner_cursor_queue<T, Capacity, std::uint32_t>;

template <std::size_t Capacity>
using narrow_index = std::conditional_t<
    (Capacity <= (std::numeric_limits<std::uint16_t>::max)() / 2),
    std::uint16_t,
    std::uint32_t>;

template <class T, std::size_t Capacity>
using single_owner_narrow =
    vqbench::experimental::single_owner_cursor_queue<T, Capacity, narrow_index<Capacity>>;

} // namespace vqbench::experimental::round2
