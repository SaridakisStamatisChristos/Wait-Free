#pragma once

#include "veriqueue/detail/slot.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace vqbench::experimental {

inline constexpr std::size_t cache_line = 64;

// Experimental ablation: the single writer for each published cursor also uses
// that atomic cursor as its owner-local cursor. Owner reads are relaxed; peer
// observations remain acquire and publication remains release. This removes the
// duplicate owner-local cursor store without changing the SPSC protocol.
template <class T, std::size_t Capacity, class Index = std::size_t>
class single_owner_cursor_queue final {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2);
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_nothrow_destructible_v<T>);

    static constexpr Index capacity_index = static_cast<Index>(Capacity);
    static constexpr Index mask_index = static_cast<Index>(Capacity - 1);

    struct alignas(cache_line) producer_state final {
        std::atomic<Index> tail{0};
        Index cached_head{0};
    };

    struct alignas(cache_line) consumer_state final {
        std::atomic<Index> head{0};
        Index cached_tail{0};
    };

public:
    single_owner_cursor_queue() noexcept = default;
    single_owner_cursor_queue(const single_owner_cursor_queue&) = delete;
    single_owner_cursor_queue& operator=(const single_owner_cursor_queue&) = delete;

    ~single_owner_cursor_queue() noexcept {
        // Contract is external quiescence, exactly as for production. Once no
        // producer/consumer operation overlaps destruction, relaxed owner-cursor
        // loads are sufficient because destruction is not a synchronization edge.
        Index head = consumer_.head.load(std::memory_order_relaxed);
        const Index tail = producer_.tail.load(std::memory_order_relaxed);
        while (head != tail) {
            std::destroy_at(slot_for(head).live_ptr());
            ++head;
        }
    }

    template <class... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] bool try_emplace(Args&&... args) {
        Index tail = producer_.tail.load(std::memory_order_relaxed);
        if (distance(tail, producer_.cached_head) == capacity_index) {
            producer_.cached_head = consumer_.head.load(std::memory_order_acquire);
            if (distance(tail, producer_.cached_head) == capacity_index) return false;
        }

        std::construct_at(slot_for(tail).storage_ptr(), std::forward<Args>(args)...);
        ++tail;
        producer_.tail.store(tail, std::memory_order_release);
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

        Index tail = producer_.tail.load(std::memory_order_relaxed);
        Index used = distance(tail, producer_.cached_head);
        Index available = static_cast<Index>(capacity_index - used);
        const std::size_t target = (std::min)(values.size(), Capacity);

        if (static_cast<std::size_t>(available) < target) {
            producer_.cached_head = consumer_.head.load(std::memory_order_acquire);
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
        producer_.tail.store(tail, std::memory_order_release);
        return count;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        Index head = consumer_.head.load(std::memory_order_relaxed);
        if (head == consumer_.cached_tail) {
            consumer_.cached_tail = producer_.tail.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) return false;
        }

        T* const source = slot_for(head).live_ptr();
        output = std::move(*source);
        std::destroy_at(source);
        ++head;
        consumer_.head.store(head, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t try_pop_bulk(std::span<T> output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        if (output.empty()) return 0;

        Index head = consumer_.head.load(std::memory_order_relaxed);
        Index available = distance(consumer_.cached_tail, head);
        const std::size_t target = (std::min)(output.size(), Capacity);

        if (static_cast<std::size_t>(available) < target) {
            consumer_.cached_tail = producer_.tail.load(std::memory_order_acquire);
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
        consumer_.head.store(head, std::memory_order_release);
        return count;
    }

    template <class F>
        requires std::is_nothrow_invocable_v<F, T&>
    [[nodiscard]] bool try_consume(F&& consumer) noexcept {
        Index head = consumer_.head.load(std::memory_order_relaxed);
        if (head == consumer_.cached_tail) {
            consumer_.cached_tail = producer_.tail.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) return false;
        }

        T* const source = slot_for(head).live_ptr();
        std::invoke(std::forward<F>(consumer), *source);
        std::destroy_at(source);
        ++head;
        consumer_.head.store(head, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        const Index head = consumer_.head.load(std::memory_order_acquire);
        const Index tail = producer_.tail.load(std::memory_order_acquire);
        return head == tail;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }

    [[nodiscard]] std::size_t size_approx() const noexcept {
        const Index head = consumer_.head.load(std::memory_order_acquire);
        const Index tail = producer_.tail.load(std::memory_order_acquire);
        const Index observed = distance(tail, head);
        return observed > capacity_index ? Capacity : static_cast<std::size_t>(observed);
    }

private:
    [[nodiscard]] static constexpr Index distance(Index newer, Index older) noexcept {
        return static_cast<Index>(newer - older);
    }

    [[nodiscard]] veriqueue::detail::slot<T>& slot_for(Index logical_index) noexcept {
        return slots_[static_cast<std::size_t>(logical_index & mask_index)];
    }

    producer_state producer_{};
    consumer_state consumer_{};
    alignas(cache_line) std::array<veriqueue::detail::slot<T>, Capacity> slots_{};
};

template <class T, std::size_t Capacity, class Index = std::size_t>
class cached_limit_queue final {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2);
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_nothrow_destructible_v<T>);

    static constexpr Index capacity_index = static_cast<Index>(Capacity);
    static constexpr Index mask_index = static_cast<Index>(Capacity - 1);

    struct alignas(cache_line) producer_state final {
        Index local_tail{0};
        Index cached_full_limit{capacity_index};
        std::atomic<Index> published_tail{0};
    };

    struct alignas(cache_line) consumer_state final {
        Index local_head{0};
        Index cached_tail{0};
        std::atomic<Index> published_head{0};
    };

public:
    cached_limit_queue() noexcept = default;
    cached_limit_queue(const cached_limit_queue&) = delete;
    cached_limit_queue& operator=(const cached_limit_queue&) = delete;

    ~cached_limit_queue() noexcept {
        Index head = consumer_.local_head;
        const Index tail = producer_.local_tail;
        while (head != tail) {
            std::destroy_at(slot_for(head).live_ptr());
            ++head;
        }
    }

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        Index tail = producer_.local_tail;
        if (tail == producer_.cached_full_limit) {
            const Index head = consumer_.published_head.load(std::memory_order_acquire);
            producer_.cached_full_limit = static_cast<Index>(head + capacity_index);
            if (tail == producer_.cached_full_limit) return false;
        }
        std::construct_at(slot_for(tail).storage_ptr(), value);
        ++tail;
        producer_.local_tail = tail;
        producer_.published_tail.store(tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        Index head = consumer_.local_head;
        if (head == consumer_.cached_tail) {
            consumer_.cached_tail = producer_.published_tail.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) return false;
        }
        T* const source = slot_for(head).live_ptr();
        output = std::move(*source);
        std::destroy_at(source);
        ++head;
        consumer_.local_head = head;
        consumer_.published_head.store(head, std::memory_order_release);
        return true;
    }

private:
    [[nodiscard]] veriqueue::detail::slot<T>& slot_for(Index logical_index) noexcept {
        return slots_[static_cast<std::size_t>(logical_index & mask_index)];
    }

    producer_state producer_{};
    consumer_state consumer_{};
    alignas(cache_line) std::array<veriqueue::detail::slot<T>, Capacity> slots_{};
};

template <class T, std::size_t Capacity, bool CachedLimit, class Index = std::size_t>
class split_control_queue final {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2);
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_nothrow_destructible_v<T>);

    static constexpr Index capacity_index = static_cast<Index>(Capacity);
    static constexpr Index mask_index = static_cast<Index>(Capacity - 1);

    struct alignas(cache_line) producer_private final {
        Index local_tail{0};
        Index cached_remote{CachedLimit ? capacity_index : Index{0}};
    };

    struct alignas(cache_line) consumer_private final {
        Index local_head{0};
        Index cached_tail{0};
    };

    struct alignas(cache_line) published_cursor final {
        std::atomic<Index> value{0};
    };

public:
    split_control_queue() noexcept = default;
    split_control_queue(const split_control_queue&) = delete;
    split_control_queue& operator=(const split_control_queue&) = delete;

    ~split_control_queue() noexcept {
        Index head = consumer_.local_head;
        const Index tail = producer_.local_tail;
        while (head != tail) {
            std::destroy_at(slot_for(head).live_ptr());
            ++head;
        }
    }

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        Index tail = producer_.local_tail;
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

        std::construct_at(slot_for(tail).storage_ptr(), value);
        ++tail;
        producer_.local_tail = tail;
        published_tail_.value.store(tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        Index head = consumer_.local_head;
        if (head == consumer_.cached_tail) {
            consumer_.cached_tail = published_tail_.value.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) return false;
        }
        T* const source = slot_for(head).live_ptr();
        output = std::move(*source);
        std::destroy_at(source);
        ++head;
        consumer_.local_head = head;
        published_head_.value.store(head, std::memory_order_release);
        return true;
    }

private:
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
    alignas(cache_line) std::array<veriqueue::detail::slot<T>, Capacity> slots_{};
};

} // namespace vqbench::experimental
