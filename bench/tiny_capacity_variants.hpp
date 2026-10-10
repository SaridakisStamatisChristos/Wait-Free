#pragma once

#include "veriqueue/detail/slot.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>

namespace vqbench::experimental::tiny_capacity {

inline constexpr std::size_t cache_line = 64;

template <class T>
struct alignas(cache_line) padded_slot final {
    veriqueue::detail::slot<T> slot{};
};

// Exact-capacity SPSC candidate for very small rings. Control state follows the
// proven x64 split-control layout, while each logical slot receives its own cache
// line. This tests whether cap-2 losses are dominated by producer/consumer
// ping-pong on a shared data cache line rather than cursor protocol overhead.
template <class T, std::size_t Capacity, bool CachedLimit, class Index = std::size_t>
class split_padded_slot_queue final {
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

    struct alignas(cache_line) published_cursor final {
        std::atomic<Index> value{0};
    };

    struct alignas(cache_line) consumer_private final {
        Index local_head{0};
        Index cached_tail{0};
    };

public:
    split_padded_slot_queue() noexcept = default;
    split_padded_slot_queue(const split_padded_slot_queue&) = delete;
    split_padded_slot_queue& operator=(const split_padded_slot_queue&) = delete;

    ~split_padded_slot_queue() noexcept {
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
        return slots_[static_cast<std::size_t>(logical_index & mask_index)].slot;
    }

    producer_private producer_{};
    published_cursor published_tail_{};
    consumer_private consumer_{};
    published_cursor published_head_{};
    std::array<padded_slot<T>, Capacity> slots_{};
};

template <class T, std::size_t Capacity>
using split_padded = split_padded_slot_queue<T, Capacity, false>;

template <class T, std::size_t Capacity>
using split_padded_cached = split_padded_slot_queue<T, Capacity, true>;

} // namespace vqbench::experimental::tiny_capacity
