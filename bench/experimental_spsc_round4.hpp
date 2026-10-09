#pragma once

#include "veriqueue/detail/slot.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace vqbench::experimental::round4 {

inline constexpr std::size_t cache_line = 64;

template <class T>
inline constexpr std::size_t storage_alignment =
    cache_line > alignof(veriqueue::detail::slot<T>)
        ? cache_line
        : alignof(veriqueue::detail::slot<T>);

// Bit-rotation / stripe mapping over the existing power-of-two slot array.
// Stripe=1 is the normal sequential layout. Stripe>1 moves the low log2(Stripe)
// logical-index bits to the high end of the physical slot index, so consecutive
// logical items are distributed across distant regions while the array size and
// object footprint remain unchanged.
template <std::size_t Capacity, std::size_t Stripe>
[[nodiscard]] constexpr std::size_t striped_index(std::size_t logical) noexcept {
    static_assert(Capacity >= 1 && (Capacity & (Capacity - 1)) == 0);
    static_assert(Stripe >= 1 && (Stripe & (Stripe - 1)) == 0);
    static_assert(Stripe <= Capacity);
    if constexpr (Stripe == 1) {
        return logical & (Capacity - 1);
    } else {
        constexpr unsigned stripe_bits = std::countr_zero(Stripe);
        constexpr unsigned capacity_bits = std::countr_zero(Capacity);
        constexpr std::size_t stripe_mask = Stripe - 1;
        const std::size_t bounded = logical & (Capacity - 1);
        const std::size_t low = bounded & stripe_mask;
        const std::size_t high = bounded >> stripe_bits;
        return (low << (capacity_bits - stripe_bits)) | high;
    }
}

template <class T, std::size_t Capacity, std::size_t Stripe,
          class Index = std::size_t>
class striped_queue final {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    static_assert(Stripe >= 1 && (Stripe & (Stripe - 1)) == 0);
    static_assert(Stripe <= Capacity);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2);
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_nothrow_destructible_v<T>);

    static constexpr Index capacity_index = static_cast<Index>(Capacity);

    struct alignas(cache_line) producer_state final {
        std::atomic<Index> published_tail{0};
        Index cached_head{0};
    };

    struct alignas(cache_line) consumer_state final {
        std::atomic<Index> published_head{0};
        Index cached_tail{0};
    };

public:
    striped_queue() noexcept = default;
    striped_queue(const striped_queue&) = delete;
    striped_queue& operator=(const striped_queue&) = delete;

    ~striped_queue() noexcept {
        Index head = consumer_.published_head.load(std::memory_order_relaxed);
        const Index tail = producer_.published_tail.load(std::memory_order_relaxed);
        while (head != tail) {
            std::destroy_at(slot_for(head).live_ptr());
            ++head;
        }
    }

    template <class... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] bool try_emplace(Args&&... args) {
        Index tail = producer_.published_tail.load(std::memory_order_relaxed);
        if (distance(tail, producer_.cached_head) == capacity_index) {
            producer_.cached_head = consumer_.published_head.load(std::memory_order_acquire);
            if (distance(tail, producer_.cached_head) == capacity_index) return false;
        }
        std::construct_at(slot_for(tail).storage_ptr(), std::forward<Args>(args)...);
        ++tail;
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
        Index tail = producer_.published_tail.load(std::memory_order_relaxed);
        Index used = distance(tail, producer_.cached_head);
        Index available = static_cast<Index>(capacity_index - used);
        const std::size_t target = (std::min)(values.size(), Capacity);
        if (static_cast<std::size_t>(available) < target) {
            producer_.cached_head = consumer_.published_head.load(std::memory_order_acquire);
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
        producer_.published_tail.store(tail, std::memory_order_release);
        return count;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        Index head = consumer_.published_head.load(std::memory_order_relaxed);
        if (head == consumer_.cached_tail) {
            consumer_.cached_tail = producer_.published_tail.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) return false;
        }
        T* const source = slot_for(head).live_ptr();
        output = std::move(*source);
        std::destroy_at(source);
        ++head;
        consumer_.published_head.store(head, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t try_pop_bulk(std::span<T> output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        if (output.empty()) return 0;
        Index head = consumer_.published_head.load(std::memory_order_relaxed);
        Index available = distance(consumer_.cached_tail, head);
        const std::size_t target = (std::min)(output.size(), Capacity);
        if (static_cast<std::size_t>(available) < target) {
            consumer_.cached_tail = producer_.published_tail.load(std::memory_order_acquire);
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
        consumer_.published_head.store(head, std::memory_order_release);
        return count;
    }

    template <class F>
        requires std::is_nothrow_invocable_v<F, T&>
    [[nodiscard]] bool try_consume(F&& consumer) noexcept {
        Index head = consumer_.published_head.load(std::memory_order_relaxed);
        if (head == consumer_.cached_tail) {
            consumer_.cached_tail = producer_.published_tail.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) return false;
        }
        T* const source = slot_for(head).live_ptr();
        std::invoke(std::forward<F>(consumer), *source);
        std::destroy_at(source);
        ++head;
        consumer_.published_head.store(head, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        const Index head = consumer_.published_head.load(std::memory_order_acquire);
        const Index tail = producer_.published_tail.load(std::memory_order_acquire);
        return head == tail;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }

    [[nodiscard]] std::size_t size_approx() const noexcept {
        const Index head = consumer_.published_head.load(std::memory_order_acquire);
        const Index tail = producer_.published_tail.load(std::memory_order_acquire);
        const Index observed = distance(tail, head);
        return observed > capacity_index ? Capacity : static_cast<std::size_t>(observed);
    }

private:
    [[nodiscard]] static constexpr Index distance(Index newer, Index older) noexcept {
        return static_cast<Index>(newer - older);
    }

    [[nodiscard]] veriqueue::detail::slot<T>& slot_for(Index logical_index) noexcept {
        const auto physical = striped_index<Capacity, Stripe>(
            static_cast<std::size_t>(logical_index));
        return slots_[physical];
    }

    producer_state producer_{};
    consumer_state consumer_{};
    alignas(storage_alignment<T>) std::array<veriqueue::detail::slot<T>, Capacity> slots_{};
};

template <class T>
inline constexpr std::size_t adaptive_full_stripe =
    sizeof(veriqueue::detail::slot<T>) <= 8 ? 8 :
    sizeof(veriqueue::detail::slot<T>) <= 16 ? 4 :
    sizeof(veriqueue::detail::slot<T>) <= 32 ? 2 : 1;

template <class T>
inline constexpr std::size_t adaptive_half_stripe =
    sizeof(veriqueue::detail::slot<T>) <= 8 ? 4 :
    sizeof(veriqueue::detail::slot<T>) <= 16 ? 2 : 1;

template <class T, std::size_t Capacity>
using stripe2 = striped_queue<T, Capacity, 2>;

template <class T, std::size_t Capacity>
using stripe4 = striped_queue<T, Capacity, 4>;

template <class T, std::size_t Capacity>
using stripe8 = striped_queue<T, Capacity, 8>;

template <class T, std::size_t Capacity>
using stripe16 = striped_queue<T, Capacity, 16>;

template <class T, std::size_t Capacity>
using adaptive_full = striped_queue<T, Capacity, adaptive_full_stripe<T>>;

template <class T, std::size_t Capacity>
using adaptive_half = striped_queue<T, Capacity, adaptive_half_stripe<T>>;

} // namespace vqbench::experimental::round4
