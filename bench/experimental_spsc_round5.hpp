#pragma once

#include "experimental_spsc_round4.hpp"
#include "veriqueue/detail/config.hpp"
#include "veriqueue/detail/slot.hpp"
#include "veriqueue/spsc_queue.hpp"

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

namespace vqbench::experimental::round5 {

#if defined(__aarch64__) || defined(_M_ARM64)
inline constexpr bool target_aarch64 = true;
#else
inline constexpr bool target_aarch64 = false;
#endif

template <class T>
inline constexpr std::size_t architecture_selective_stripe =
    target_aarch64 && sizeof(veriqueue::detail::slot<T>) == 16 ? 8 : 1;

#if defined(VERIQUEUE_FORCE_SINGLE_OWNER_CURSOR) && defined(VERIQUEUE_FORCE_DUAL_OWNER_CURSOR)
#error "Only one VeriQueue owner-cursor strategy may be forced"
#endif

#if defined(VERIQUEUE_FORCE_SINGLE_OWNER_CURSOR)
#define VQBENCH_R5_SINGLE_OWNER_CURSOR 1
#elif defined(VERIQUEUE_FORCE_DUAL_OWNER_CURSOR)
#define VQBENCH_R5_SINGLE_OWNER_CURSOR 0
#elif defined(__aarch64__) || defined(_M_ARM64)
#define VQBENCH_R5_SINGLE_OWNER_CURSOR 1
#else
#define VQBENCH_R5_SINGLE_OWNER_CURSOR 0
#endif

// Production-shaped striped specialization. It is instantiated only for the
// one evidence-backed case (AArch64 + exactly-16-byte slots). All unaffected
// specializations alias the actual production veriqueue::spsc_queue type below,
// removing class-template/code-layout drift from control cells entirely.
template <
    class T,
    std::size_t Capacity,
    std::size_t CacheLine = veriqueue::detail::default_cache_line,
    class Index = std::size_t>
class architecture_selective_queue final {
    static_assert(Capacity >= 1);
    static_assert(veriqueue::detail::is_power_of_two(Capacity));
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2);
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(veriqueue::detail::is_power_of_two(CacheLine));
    static_assert(CacheLine >= alignof(std::atomic<Index>));
    static_assert(std::is_nothrow_destructible_v<T>);

#ifdef VERIQUEUE_DISABLE_PADDING
    static constexpr std::size_t control_alignment = alignof(std::atomic<Index>);
#else
    static constexpr std::size_t control_alignment = CacheLine;
#endif

    static constexpr std::size_t storage_alignment =
        control_alignment < alignof(veriqueue::detail::slot<T>)
            ? alignof(veriqueue::detail::slot<T>)
            : control_alignment;

    static constexpr Index capacity_index = static_cast<Index>(Capacity);
    static constexpr Index mask_index = static_cast<Index>(Capacity - 1);

    struct alignas(control_alignment) producer_state final {
#if VQBENCH_R5_SINGLE_OWNER_CURSOR
        std::atomic<Index> published_tail{0};
        Index cached_head{0};
#else
        Index local_tail{0};
        Index cached_head{0};
        std::atomic<Index> published_tail{0};
#endif
    };

    struct alignas(control_alignment) consumer_state final {
#if VQBENCH_R5_SINGLE_OWNER_CURSOR
        std::atomic<Index> published_head{0};
        Index cached_tail{0};
#else
        Index local_head{0};
        Index cached_tail{0};
        std::atomic<Index> published_head{0};
#endif
    };

public:
    using value_type = T;
    using index_type = Index;
    static constexpr std::size_t static_capacity = Capacity;

    architecture_selective_queue() noexcept = default;
    architecture_selective_queue(const architecture_selective_queue&) = delete;
    architecture_selective_queue& operator=(const architecture_selective_queue&) = delete;
    architecture_selective_queue(architecture_selective_queue&&) = delete;
    architecture_selective_queue& operator=(architecture_selective_queue&&) = delete;

    ~architecture_selective_queue() noexcept {
#if VQBENCH_R5_SINGLE_OWNER_CURSOR
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
#if VQBENCH_R5_SINGLE_OWNER_CURSOR
        Index tail = producer_.published_tail.load(std::memory_order_relaxed);
#else
        Index tail = producer_.local_tail;
#endif
        if (distance(tail, producer_.cached_head) == capacity_index) {
            producer_.cached_head = consumer_.published_head.load(std::memory_order_acquire);
            if (distance(tail, producer_.cached_head) == capacity_index) return false;
        }
        std::construct_at(slot_for(tail).storage_ptr(), std::forward<Args>(args)...);
        ++tail;
#if !VQBENCH_R5_SINGLE_OWNER_CURSOR
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
#if VQBENCH_R5_SINGLE_OWNER_CURSOR
        Index tail = producer_.published_tail.load(std::memory_order_relaxed);
#else
        Index tail = producer_.local_tail;
#endif
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
#if !VQBENCH_R5_SINGLE_OWNER_CURSOR
        producer_.local_tail = tail;
#endif
        producer_.published_tail.store(tail, std::memory_order_release);
        return count;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
#if VQBENCH_R5_SINGLE_OWNER_CURSOR
        Index head = consumer_.published_head.load(std::memory_order_relaxed);
#else
        Index head = consumer_.local_head;
#endif
        if (head == consumer_.cached_tail) {
            consumer_.cached_tail = producer_.published_tail.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) return false;
        }
        T* const source = slot_for(head).live_ptr();
        output = std::move(*source);
        std::destroy_at(source);
        ++head;
#if !VQBENCH_R5_SINGLE_OWNER_CURSOR
        consumer_.local_head = head;
#endif
        consumer_.published_head.store(head, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t try_pop_bulk(std::span<T> output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        if (output.empty()) return 0;
#if VQBENCH_R5_SINGLE_OWNER_CURSOR
        Index head = consumer_.published_head.load(std::memory_order_relaxed);
#else
        Index head = consumer_.local_head;
#endif
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
#if !VQBENCH_R5_SINGLE_OWNER_CURSOR
        consumer_.local_head = head;
#endif
        consumer_.published_head.store(head, std::memory_order_release);
        return count;
    }

    template <class F>
        requires std::is_nothrow_invocable_v<F, T&>
    [[nodiscard]] bool try_consume(F&& consumer) noexcept {
#if VQBENCH_R5_SINGLE_OWNER_CURSOR
        Index head = consumer_.published_head.load(std::memory_order_relaxed);
#else
        Index head = consumer_.local_head;
#endif
        if (head == consumer_.cached_tail) {
            consumer_.cached_tail = producer_.published_tail.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) return false;
        }
        T* const source = slot_for(head).live_ptr();
        std::invoke(std::forward<F>(consumer), *source);
        std::destroy_at(source);
        ++head;
#if !VQBENCH_R5_SINGLE_OWNER_CURSOR
        consumer_.local_head = head;
#endif
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
        const auto physical = vqbench::experimental::round4::striped_index<Capacity, 8>(
            static_cast<std::size_t>(logical_index));
        return slots_[physical];
    }

    producer_state producer_{};
    consumer_state consumer_{};
    alignas(storage_alignment) std::array<veriqueue::detail::slot<T>, Capacity> slots_;
};

// Unaffected cases are literally the production type. Only the selected ARM64
// 16-byte specialization uses the striped experimental implementation.
template <class T, std::size_t Capacity>
using architecture_selective = std::conditional_t<
    architecture_selective_stripe<T> == 8,
    architecture_selective_queue<T, Capacity>,
    veriqueue::spsc_queue<T, Capacity>>;

#undef VQBENCH_R5_SINGLE_OWNER_CURSOR

} // namespace vqbench::experimental::round5
