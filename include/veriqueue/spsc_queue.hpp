#pragma once

#include "veriqueue/detail/config.hpp"
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

namespace veriqueue {

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4324) // Cache-line alignas intentionally adds tail padding.
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

    struct alignas(control_alignment) producer_state final {
        Index local_tail{0};
        Index cached_head{0};
        std::atomic<Index> published_tail{0};
    };

    struct alignas(control_alignment) consumer_state final {
        Index local_head{0};
        Index cached_tail{0};
        std::atomic<Index> published_head{0};
    };

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
        Index head = consumer_.local_head;
        const Index tail = producer_.local_tail;
        while (head != tail) {
            std::destroy_at(slot_for(head).live_ptr());
            ++head;
        }
    }

    template <class... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] bool try_emplace(Args&&... args) {
        Index tail = producer_.local_tail;

        if (distance(tail, producer_.cached_head) == capacity_index) {
            producer_.cached_head =
                consumer_.published_head.load(std::memory_order_acquire);
            if (distance(tail, producer_.cached_head) == capacity_index) {
                return false;
            }
        }

        std::construct_at(slot_for(tail).storage_ptr(), std::forward<Args>(args)...);
        ++tail;
        producer_.local_tail = tail;
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

        Index tail = producer_.local_tail;
        Index used = distance(tail, producer_.cached_head);
        if (used == capacity_index) {
            producer_.cached_head =
                consumer_.published_head.load(std::memory_order_acquire);
            used = distance(tail, producer_.cached_head);
            if (used == capacity_index) return 0;
        }

        const Index available = static_cast<Index>(capacity_index - used);
        const std::size_t count =
            (std::min)(values.size(), static_cast<std::size_t>(available));

        for (std::size_t i = 0; i < count; ++i) {
            std::construct_at(slot_for(tail).storage_ptr(), values[i]);
            ++tail;
        }

        producer_.local_tail = tail;
        producer_.published_tail.store(tail, std::memory_order_release);
        return count;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        Index head = consumer_.local_head;

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
        consumer_.local_head = head;
        consumer_.published_head.store(head, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t try_pop_bulk(std::span<T> output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        if (output.empty()) return 0;

        Index head = consumer_.local_head;
        Index available = distance(consumer_.cached_tail, head);
        if (available == 0) {
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

        consumer_.local_head = head;
        consumer_.published_head.store(head, std::memory_order_release);
        return count;
    }

    template <class F>
        requires std::is_nothrow_invocable_v<F, T&>
    [[nodiscard]] bool try_consume(F&& consumer) noexcept {
        Index head = consumer_.local_head;

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
        consumer_.local_head = head;
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
        return {
            producer_.local_tail,
            producer_.cached_head,
            consumer_.local_head,
            consumer_.cached_tail,
            producer_.published_tail.load(std::memory_order_relaxed),
            consumer_.published_head.load(std::memory_order_relaxed)};
    }
#endif

private:
    [[nodiscard]] static constexpr Index distance(Index newer, Index older) noexcept {
        return static_cast<Index>(newer - older);
    }

    [[nodiscard]] detail::slot<T>& slot_for(Index logical_index) noexcept {
        return slots_[static_cast<std::size_t>(logical_index & mask_index)];
    }

    [[nodiscard]] const detail::slot<T>& slot_for(Index logical_index) const noexcept {
        return slots_[static_cast<std::size_t>(logical_index & mask_index)];
    }

    producer_state producer_{};
    consumer_state consumer_{};
    alignas(storage_alignment) std::array<detail::slot<T>, Capacity> slots_;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace veriqueue
