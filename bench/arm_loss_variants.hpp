#pragma once

#include "veriqueue/detail/slot.hpp"

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>

namespace vqbench::experimental::arm_loss {

inline constexpr std::size_t cache_line = 64;

enum class local_stripe_tile : std::size_t {
    tile32 = 32,
    tile64 = 64,
    tile128 = 128,
    tile256 = 256,
};

template <class Slot, std::size_t Capacity, local_stripe_tile Tile, class Index>
[[nodiscard]] constexpr std::size_t local_eight_lane_index(Index logical_index) noexcept {
    static_assert(Capacity >= 1 && (Capacity & (Capacity - 1)) == 0);
    constexpr std::size_t tile_slots = static_cast<std::size_t>(Tile);
    static_assert((tile_slots & (tile_slots - 1)) == 0);
    static_assert(tile_slots >= 8);
    static_assert((tile_slots % 8) == 0);

    const std::size_t bounded = static_cast<std::size_t>(logical_index) & (Capacity - 1);

    // Keep larger-than-16-byte payloads sequential. Their elements already occupy
    // at least one cache line and were not part of the ARM small-slot loss cluster.
    if constexpr (sizeof(Slot) <= 16 && Capacity >= tile_slots) {
        constexpr std::size_t lanes = 8;
        constexpr std::size_t lane_span = tile_slots / lanes;
        const std::size_t tile_base = bounded & ~(tile_slots - 1);
        const std::size_t local = bounded & (tile_slots - 1);
        const std::size_t lane = local & (lanes - 1);
        const std::size_t ordinal = local >> 3;
        return tile_base + lane * lane_span + ordinal;
    } else {
        return bounded;
    }
}

// ARM forensic candidate. It keeps the production single-owner publication
// protocol and exact fixed-capacity semantics, but replaces producer distance
// arithmetic with a cached full-limit equality and uses a local eight-lane
// storage permutation for small slots. The local permutation preserves the
// eight-operation cache-line separation that motivated the proven global 16-B
// stripe while bounding the working-set jump to one small tile.
template <class T, std::size_t Capacity, local_stripe_tile Tile, class Index = std::size_t>
class cached_local_stripe_queue final {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2);
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_nothrow_destructible_v<T>);

    static constexpr Index capacity_index = static_cast<Index>(Capacity);

    struct alignas(cache_line) producer_state final {
        std::atomic<Index> tail{0};
        Index cached_full_limit{capacity_index};
    };

    struct alignas(cache_line) consumer_state final {
        std::atomic<Index> head{0};
        Index cached_tail{0};
    };

public:
    cached_local_stripe_queue() noexcept = default;
    cached_local_stripe_queue(const cached_local_stripe_queue&) = delete;
    cached_local_stripe_queue& operator=(const cached_local_stripe_queue&) = delete;

    ~cached_local_stripe_queue() noexcept {
        Index head = consumer_.head.load(std::memory_order_relaxed);
        const Index tail = producer_.tail.load(std::memory_order_relaxed);
        while (head != tail) {
            std::destroy_at(slot_for(head).live_ptr());
            ++head;
        }
    }

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        Index tail = producer_.tail.load(std::memory_order_relaxed);
        if (tail == producer_.cached_full_limit) {
            const Index head = consumer_.head.load(std::memory_order_acquire);
            producer_.cached_full_limit = static_cast<Index>(head + capacity_index);
            if (tail == producer_.cached_full_limit) return false;
        }

        std::construct_at(slot_for(tail).storage_ptr(), value);
        ++tail;
        producer_.tail.store(tail, std::memory_order_release);
        return true;
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

    [[nodiscard]] static constexpr std::size_t testing_slot_index(Index logical_index) noexcept {
        return local_eight_lane_index<veriqueue::detail::slot<T>, Capacity, Tile>(logical_index);
    }

private:
    [[nodiscard]] veriqueue::detail::slot<T>& slot_for(Index logical_index) noexcept {
        return slots_[testing_slot_index(logical_index)];
    }

    producer_state producer_{};
    consumer_state consumer_{};
    alignas(cache_line) std::array<veriqueue::detail::slot<T>, Capacity> slots_{};
};

template <class T, std::size_t Capacity>
using tile32_l8 = cached_local_stripe_queue<T, Capacity, local_stripe_tile::tile32>;

template <class T, std::size_t Capacity>
using tile64_l8 = cached_local_stripe_queue<T, Capacity, local_stripe_tile::tile64>;

template <class T, std::size_t Capacity>
using tile128_l8 = cached_local_stripe_queue<T, Capacity, local_stripe_tile::tile128>;

template <class T, std::size_t Capacity>
using tile256_l8 = cached_local_stripe_queue<T, Capacity, local_stripe_tile::tile256>;

} // namespace vqbench::experimental::arm_loss
