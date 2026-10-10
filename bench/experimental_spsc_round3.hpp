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

namespace vqbench::experimental::round3 {

inline constexpr std::size_t cache_line = 64;

template <class T>
inline constexpr std::size_t storage_alignment =
    cache_line > alignof(veriqueue::detail::slot<T>)
        ? cache_line
        : alignof(veriqueue::detail::slot<T>);

template <class Index, std::size_t Stride>
struct alignas(Stride) producer_state final {
    std::atomic<Index> published_tail{0};
    Index cached_head{0};
};

template <class Index, std::size_t Stride>
struct alignas(Stride) consumer_state final {
    std::atomic<Index> published_head{0};
    Index cached_tail{0};
};

template <class Index, std::size_t Stride, bool ConsumerFirst>
struct control_layout;

template <class Index, std::size_t Stride>
struct control_layout<Index, Stride, false> final {
    producer_state<Index, Stride> producer{};
    consumer_state<Index, Stride> consumer{};
};

template <class Index, std::size_t Stride>
struct control_layout<Index, Stride, true> final {
    consumer_state<Index, Stride> consumer{};
    producer_state<Index, Stride> producer{};
};

// Round-3 layout/scheduling ablation. The queue protocol is intentionally the
// same as production ARM64 single-owner mode: relaxed owner cursor reads,
// acquire peer refreshes, release publications, cached peer cursors, and the
// same unsigned modular-distance full/empty rules.
//
// Only three dimensions vary:
//   * control-state stride (64/128/256 B),
//   * producer-first vs consumer-first control placement,
//   * source-order computation of the slot address before the full/empty test.
//
// PrecomputeSlot forms an in-bounds storage address only. It never launders or
// dereferences a possibly non-live T until after the queue-state test succeeds.
template <class T, std::size_t Capacity, std::size_t ControlStride = cache_line,
          bool ConsumerFirst = false, bool PrecomputeSlot = false,
          class Index = std::size_t>
class layout_queue final {
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

public:
    layout_queue() noexcept = default;
    layout_queue(const layout_queue&) = delete;
    layout_queue& operator=(const layout_queue&) = delete;

    ~layout_queue() noexcept {
        Index head = controls_.consumer.published_head.load(std::memory_order_relaxed);
        const Index tail = controls_.producer.published_tail.load(std::memory_order_relaxed);
        while (head != tail) {
            std::destroy_at(slot_for(head).live_ptr());
            ++head;
        }
    }

    template <class... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] bool try_emplace(Args&&... args) {
        Index tail = controls_.producer.published_tail.load(std::memory_order_relaxed);
        T* target = nullptr;
        if constexpr (PrecomputeSlot) target = slot_for(tail).storage_ptr();

        if (distance(tail, controls_.producer.cached_head) == capacity_index) {
            controls_.producer.cached_head =
                controls_.consumer.published_head.load(std::memory_order_acquire);
            if (distance(tail, controls_.producer.cached_head) == capacity_index) {
                return false;
            }
        }

        if constexpr (!PrecomputeSlot) target = slot_for(tail).storage_ptr();
        std::construct_at(target, std::forward<Args>(args)...);
        ++tail;
        controls_.producer.published_tail.store(tail, std::memory_order_release);
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
        Index tail = controls_.producer.published_tail.load(std::memory_order_relaxed);
        Index used = distance(tail, controls_.producer.cached_head);
        Index available = static_cast<Index>(capacity_index - used);
        const std::size_t target = (std::min)(values.size(), Capacity);
        if (static_cast<std::size_t>(available) < target) {
            controls_.producer.cached_head =
                controls_.consumer.published_head.load(std::memory_order_acquire);
            used = distance(tail, controls_.producer.cached_head);
            available = static_cast<Index>(capacity_index - used);
            if (available == 0) return 0;
        }
        const std::size_t count =
            (std::min)(values.size(), static_cast<std::size_t>(available));
        for (std::size_t i = 0; i < count; ++i) {
            std::construct_at(slot_for(tail).storage_ptr(), values[i]);
            ++tail;
        }
        controls_.producer.published_tail.store(tail, std::memory_order_release);
        return count;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        Index head = controls_.consumer.published_head.load(std::memory_order_relaxed);
        T* storage = nullptr;
        if constexpr (PrecomputeSlot) storage = slot_for(head).storage_ptr();

        if (head == controls_.consumer.cached_tail) {
            controls_.consumer.cached_tail =
                controls_.producer.published_tail.load(std::memory_order_acquire);
            if (head == controls_.consumer.cached_tail) return false;
        }

        T* source = nullptr;
        if constexpr (PrecomputeSlot) {
            source = std::launder(storage);
        } else {
            source = slot_for(head).live_ptr();
        }
        output = std::move(*source);
        std::destroy_at(source);
        ++head;
        controls_.consumer.published_head.store(head, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t try_pop_bulk(std::span<T> output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        if (output.empty()) return 0;
        Index head = controls_.consumer.published_head.load(std::memory_order_relaxed);
        Index available = distance(controls_.consumer.cached_tail, head);
        const std::size_t target = (std::min)(output.size(), Capacity);
        if (static_cast<std::size_t>(available) < target) {
            controls_.consumer.cached_tail =
                controls_.producer.published_tail.load(std::memory_order_acquire);
            available = distance(controls_.consumer.cached_tail, head);
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
        controls_.consumer.published_head.store(head, std::memory_order_release);
        return count;
    }

    template <class F>
        requires std::is_nothrow_invocable_v<F, T&>
    [[nodiscard]] bool try_consume(F&& consumer) noexcept {
        Index head = controls_.consumer.published_head.load(std::memory_order_relaxed);
        T* storage = nullptr;
        if constexpr (PrecomputeSlot) storage = slot_for(head).storage_ptr();
        if (head == controls_.consumer.cached_tail) {
            controls_.consumer.cached_tail =
                controls_.producer.published_tail.load(std::memory_order_acquire);
            if (head == controls_.consumer.cached_tail) return false;
        }
        T* source = nullptr;
        if constexpr (PrecomputeSlot) {
            source = std::launder(storage);
        } else {
            source = slot_for(head).live_ptr();
        }
        std::invoke(std::forward<F>(consumer), *source);
        std::destroy_at(source);
        ++head;
        controls_.consumer.published_head.store(head, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        const Index head = controls_.consumer.published_head.load(std::memory_order_acquire);
        const Index tail = controls_.producer.published_tail.load(std::memory_order_acquire);
        return head == tail;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }

    [[nodiscard]] std::size_t size_approx() const noexcept {
        const Index head = controls_.consumer.published_head.load(std::memory_order_acquire);
        const Index tail = controls_.producer.published_tail.load(std::memory_order_acquire);
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

    control_layout<Index, ControlStride, ConsumerFirst> controls_{};
    alignas(storage_alignment<T>) std::array<veriqueue::detail::slot<T>, Capacity> slots_{};
};

template <class T, std::size_t Capacity>
using state128 = layout_queue<T, Capacity, 128, false, false>;

template <class T, std::size_t Capacity>
using state256 = layout_queue<T, Capacity, 256, false, false>;

template <class T, std::size_t Capacity>
using consumer_first64 = layout_queue<T, Capacity, 64, true, false>;

template <class T, std::size_t Capacity>
using consumer_first128 = layout_queue<T, Capacity, 128, true, false>;

template <class T, std::size_t Capacity>
using precompute64 = layout_queue<T, Capacity, 64, false, true>;

template <class T, std::size_t Capacity>
using precompute128 = layout_queue<T, Capacity, 128, false, true>;

template <class T, std::size_t Capacity>
using consumer_first_precompute128 = layout_queue<T, Capacity, 128, true, true>;

} // namespace vqbench::experimental::round3
