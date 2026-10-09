#pragma once

#include "experimental_spsc_variants.hpp"
#include "veriqueue/detail/slot.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace vqbench::experimental::v2 {

inline constexpr std::size_t cache_line = 64;

template <std::size_t Bytes>
struct padding_block final {
    std::array<std::byte, Bytes> bytes{};
};

template <>
struct padding_block<0> final {};

// The storage region itself starts on a cache-line boundary. Padding is placed
// inside that region, before the slot array, so 64/128-byte guards move cache-set
// placement while a 96-byte guard produces the requested 64+32 phase skew for
// normally aligned payloads. The compiler may add only the extra padding required
// to preserve alignof(slot<T>) for over-aligned T.
template <class T, std::size_t Capacity, std::size_t PaddingBytes>
struct alignas(cache_line) slot_storage final {
    [[no_unique_address]] padding_block<PaddingBytes> padding{};
    std::array<veriqueue::detail::slot<T>, Capacity> slots{};
};

// Experimental only. This combines the already-qualified ARM64 single-owner
// cursor ablation with a cached logical full limit. The producer therefore avoids
// the modular subtraction on the successful scalar fast path. Peer observation
// remains acquire and publication remains release.
template <class T, std::size_t Capacity, std::size_t PaddingBytes = 0,
          bool MarkSlowPathUnlikely = false, class Index = std::size_t>
class single_owner_cached_limit_queue final {
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
        Index cached_full_limit{capacity_index};
    };

    struct alignas(cache_line) consumer_state final {
        std::atomic<Index> head{0};
        Index cached_tail{0};
    };

public:
    single_owner_cached_limit_queue() noexcept = default;
    single_owner_cached_limit_queue(const single_owner_cached_limit_queue&) = delete;
    single_owner_cached_limit_queue& operator=(const single_owner_cached_limit_queue&) = delete;

    ~single_owner_cached_limit_queue() noexcept {
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
        if constexpr (MarkSlowPathUnlikely) {
            if (tail == producer_.cached_full_limit) [[unlikely]] {
                if (!refresh_full_limit(tail)) return false;
            }
        } else {
            if (tail == producer_.cached_full_limit) {
                if (!refresh_full_limit(tail)) return false;
            }
        }

        std::construct_at(slot_for(tail).storage_ptr(), value);
        ++tail;
        producer_.tail.store(tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_push(T&& value)
        requires std::is_move_constructible_v<T>
    {
        Index tail = producer_.tail.load(std::memory_order_relaxed);
        if constexpr (MarkSlowPathUnlikely) {
            if (tail == producer_.cached_full_limit) [[unlikely]] {
                if (!refresh_full_limit(tail)) return false;
            }
        } else {
            if (tail == producer_.cached_full_limit) {
                if (!refresh_full_limit(tail)) return false;
            }
        }

        std::construct_at(slot_for(tail).storage_ptr(), std::move(value));
        ++tail;
        producer_.tail.store(tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t try_push_bulk(std::span<const T> values) noexcept
        requires std::is_nothrow_copy_constructible_v<T>
    {
        if (values.empty()) return 0;

        Index tail = producer_.tail.load(std::memory_order_relaxed);
        Index available = distance(producer_.cached_full_limit, tail);
        const std::size_t target = (std::min)(values.size(), Capacity);

        if (static_cast<std::size_t>(available) < target) {
            const Index head = consumer_.head.load(std::memory_order_acquire);
            producer_.cached_full_limit = static_cast<Index>(head + capacity_index);
            available = distance(producer_.cached_full_limit, tail);
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
        if constexpr (MarkSlowPathUnlikely) {
            if (head == consumer_.cached_tail) [[unlikely]] {
                consumer_.cached_tail = producer_.tail.load(std::memory_order_acquire);
                if (head == consumer_.cached_tail) return false;
            }
        } else {
            if (head == consumer_.cached_tail) {
                consumer_.cached_tail = producer_.tail.load(std::memory_order_acquire);
                if (head == consumer_.cached_tail) return false;
            }
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

private:
    [[nodiscard]] bool refresh_full_limit(Index tail) noexcept {
        const Index head = consumer_.head.load(std::memory_order_acquire);
        producer_.cached_full_limit = static_cast<Index>(head + capacity_index);
        return tail != producer_.cached_full_limit;
    }

    [[nodiscard]] static constexpr Index distance(Index newer, Index older) noexcept {
        return static_cast<Index>(newer - older);
    }

    [[nodiscard]] veriqueue::detail::slot<T>& slot_for(Index logical_index) noexcept {
        return storage_.slots[static_cast<std::size_t>(logical_index & mask_index)];
    }

    producer_state producer_{};
    consumer_state consumer_{};
    slot_storage<T, Capacity, PaddingBytes> storage_{};
};

// Canonical aliases used by the v2 benchmark dispatcher. Keeping the aliases in
// one place makes the experimental dimensions explicit and prevents accidental
// production selection.
template <class T, std::size_t Capacity>
using cached_limit = single_owner_cached_limit_queue<T, Capacity, 0, false, std::size_t>;

template <class T, std::size_t Capacity>
using cached_limit_unlikely = single_owner_cached_limit_queue<T, Capacity, 0, true, std::size_t>;

template <class T, std::size_t Capacity>
using guard64 = single_owner_cached_limit_queue<T, Capacity, 64, false, std::size_t>;

template <class T, std::size_t Capacity>
using guard128 = single_owner_cached_limit_queue<T, Capacity, 128, false, std::size_t>;

template <class T, std::size_t Capacity>
using guard64_skew32 = single_owner_cached_limit_queue<T, Capacity, 96, false, std::size_t>;

template <class T, std::size_t Capacity>
using uint32_cursor = single_owner_cached_limit_queue<T, Capacity, 0, false, std::uint32_t>;

} // namespace vqbench::experimental::v2
