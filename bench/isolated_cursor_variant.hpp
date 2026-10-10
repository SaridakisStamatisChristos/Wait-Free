#pragma once

#include "veriqueue/detail/slot.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>

namespace vqbench::experimental::isolated_cursor {

inline constexpr std::size_t cache_line = 64;

template <class Slot, std::size_t Capacity, class Index>
[[nodiscard]] constexpr std::size_t production_like_slot_index(Index logical_index) noexcept {
    static_assert(Capacity >= 1 && (Capacity & (Capacity - 1)) == 0);
    const std::size_t bounded = static_cast<std::size_t>(logical_index) & (Capacity - 1);
#if defined(__aarch64__) || defined(_M_ARM64)
    if constexpr (sizeof(Slot) == 16 && Capacity >= 8) {
        constexpr std::size_t lanes = 8;
        constexpr std::size_t lane_span = Capacity / lanes;
        const std::size_t lane = bounded & (lanes - 1);
        const std::size_t ordinal = bounded >> 3;
        return lane * lane_span + ordinal;
    }
#endif
    return bounded;
}

// Hybrid between the production x64 split-control layout and the ARM64
// single-owner cursor protocol. The owner reads/writes the published atomic
// cursor directly, so each successful operation performs one cursor store.
// Cached peer state lives on a separate owner-private cache line, preventing
// peer observation of the published cursor from pulling private cache metadata
// into the coherency traffic.
template <class T, std::size_t Capacity, bool CachedLimit, class Index = std::size_t>
class isolated_published_cursor_queue final {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2);
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_nothrow_destructible_v<T>);

    static constexpr Index capacity_index = static_cast<Index>(Capacity);

    struct alignas(cache_line) producer_private final {
        Index cached_remote{CachedLimit ? capacity_index : Index{0}};
    };

    struct alignas(cache_line) published_cursor final {
        std::atomic<Index> value{0};
    };

    struct alignas(cache_line) consumer_private final {
        Index cached_tail{0};
    };

public:
    isolated_published_cursor_queue() noexcept = default;
    isolated_published_cursor_queue(const isolated_published_cursor_queue&) = delete;
    isolated_published_cursor_queue& operator=(const isolated_published_cursor_queue&) = delete;

    ~isolated_published_cursor_queue() noexcept {
        Index head = published_head_.value.load(std::memory_order_relaxed);
        const Index tail = published_tail_.value.load(std::memory_order_relaxed);
        while (head != tail) {
            std::destroy_at(slot_for(head).live_ptr());
            ++head;
        }
    }

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        Index tail = published_tail_.value.load(std::memory_order_relaxed);
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
        published_tail_.value.store(tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        Index head = published_head_.value.load(std::memory_order_relaxed);
        if (head == consumer_.cached_tail) {
            consumer_.cached_tail = published_tail_.value.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) return false;
        }

        T* const source = slot_for(head).live_ptr();
        output = std::move(*source);
        std::destroy_at(source);
        ++head;
        published_head_.value.store(head, std::memory_order_release);
        return true;
    }

private:
    [[nodiscard]] static constexpr Index distance(Index newer, Index older) noexcept {
        return static_cast<Index>(newer - older);
    }

    [[nodiscard]] veriqueue::detail::slot<T>& slot_for(Index logical_index) noexcept {
        constexpr auto slot_count = Capacity;
        const std::size_t index =
            production_like_slot_index<veriqueue::detail::slot<T>, slot_count>(logical_index);
        return slots_[index];
    }

    producer_private producer_{};
    published_cursor published_tail_{};
    consumer_private consumer_{};
    published_cursor published_head_{};
    alignas(cache_line) std::array<veriqueue::detail::slot<T>, Capacity> slots_{};
};

template <class T, std::size_t Capacity>
using isolated_distance = isolated_published_cursor_queue<T, Capacity, false>;

template <class T, std::size_t Capacity>
using isolated_cached_limit = isolated_published_cursor_queue<T, Capacity, true>;

} // namespace vqbench::experimental::isolated_cursor
