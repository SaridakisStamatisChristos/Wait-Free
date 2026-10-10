#pragma once

#include "veriqueue/detail/slot.hpp"

#include <array>
#include <bit>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>

namespace vqbench::experimental::policy {

inline constexpr std::size_t cache_line = 64;

enum class storage_mapping {
    sequential,
    global_stripe_16b,
    tiled_stripe_16b,
};

template <class Slot, std::size_t Capacity, storage_mapping Mapping, class Index>
[[nodiscard]] constexpr std::size_t physical_slot_index(Index logical_index) noexcept {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    const std::size_t bounded = static_cast<std::size_t>(logical_index) & (Capacity - 1);

    if constexpr (Mapping == storage_mapping::global_stripe_16b && sizeof(Slot) == 16 && Capacity >= 8) {
        constexpr unsigned stripe_bits = 3;
        constexpr unsigned capacity_bits = std::countr_zero(Capacity);
        constexpr std::size_t stripe_mask = 7;
        const std::size_t low = bounded & stripe_mask;
        const std::size_t high = bounded >> stripe_bits;
        return (low << (capacity_bits - stripe_bits)) | high;
    } else if constexpr (Mapping == storage_mapping::tiled_stripe_16b && sizeof(Slot) == 16 && Capacity >= 16) {
        // A 16-slot / 256-byte local transpose. Consecutive logical 16-byte
        // elements land one cache line apart, but the complete tile remains local.
        constexpr std::size_t tile_slots = 16;
        constexpr std::size_t lanes = cache_line / sizeof(Slot); // four 16-byte slots
        const std::size_t tile_base = bounded & ~(tile_slots - 1);
        const std::size_t local = bounded & (tile_slots - 1);
        return tile_base + ((local % lanes) * lanes) + (local / lanes);
    } else {
        return bounded;
    }
}

template <class T, std::size_t Capacity, storage_mapping Mapping, class Index = std::size_t>
class raw_cached_limit_queue final {
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
    raw_cached_limit_queue() noexcept = default;
    raw_cached_limit_queue(const raw_cached_limit_queue&) = delete;
    raw_cached_limit_queue& operator=(const raw_cached_limit_queue&) = delete;

    ~raw_cached_limit_queue() noexcept {
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
        return physical_slot_index<veriqueue::detail::slot<T>, Capacity, Mapping>(logical_index);
    }

private:
    [[nodiscard]] veriqueue::detail::slot<T>& slot_for(Index logical_index) noexcept {
        return slots_[testing_slot_index(logical_index)];
    }

    producer_state producer_{};
    consumer_state consumer_{};
    alignas(cache_line) std::array<veriqueue::detail::slot<T>, Capacity> slots_{};
};

template <class T, std::size_t Capacity, storage_mapping Mapping, class Index = std::size_t>
class typed_trivial_cached_limit_queue final {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2);
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_trivially_destructible_v<T>);
    static_assert(std::is_default_constructible_v<T>);
    static_assert(std::is_copy_assignable_v<T>);

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
    typed_trivial_cached_limit_queue() noexcept = default;
    typed_trivial_cached_limit_queue(const typed_trivial_cached_limit_queue&) = delete;
    typed_trivial_cached_limit_queue& operator=(const typed_trivial_cached_limit_queue&) = delete;

    [[nodiscard]] bool try_push(const T& value) noexcept {
        Index tail = producer_.tail.load(std::memory_order_relaxed);
        if (tail == producer_.cached_full_limit) {
            const Index head = consumer_.head.load(std::memory_order_acquire);
            producer_.cached_full_limit = static_cast<Index>(head + capacity_index);
            if (tail == producer_.cached_full_limit) return false;
        }
        slot_for(tail) = value;
        ++tail;
        producer_.tail.store(tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept {
        Index head = consumer_.head.load(std::memory_order_relaxed);
        if (head == consumer_.cached_tail) {
            consumer_.cached_tail = producer_.tail.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) return false;
        }
        output = slot_for(head);
        ++head;
        consumer_.head.store(head, std::memory_order_release);
        return true;
    }

    [[nodiscard]] static constexpr std::size_t testing_slot_index(Index logical_index) noexcept {
        return physical_slot_index<T, Capacity, Mapping>(logical_index);
    }

private:
    [[nodiscard]] T& slot_for(Index logical_index) noexcept {
        return slots_[testing_slot_index(logical_index)];
    }

    producer_state producer_{};
    consumer_state consumer_{};
    alignas(cache_line) std::array<T, Capacity> slots_{};
};

template <class T>
struct byte_slot final {
    alignas(T) std::array<std::byte, sizeof(T)> bytes{};
};

template <class T, std::size_t Capacity, storage_mapping Mapping, class Index = std::size_t>
class memcpy_trivial_cached_limit_queue final {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2);
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_trivially_destructible_v<T>);

    static constexpr Index capacity_index = static_cast<Index>(Capacity);
    using slot_type = byte_slot<T>;

    struct alignas(cache_line) producer_state final {
        std::atomic<Index> tail{0};
        Index cached_full_limit{capacity_index};
    };

    struct alignas(cache_line) consumer_state final {
        std::atomic<Index> head{0};
        Index cached_tail{0};
    };

public:
    memcpy_trivial_cached_limit_queue() noexcept = default;
    memcpy_trivial_cached_limit_queue(const memcpy_trivial_cached_limit_queue&) = delete;
    memcpy_trivial_cached_limit_queue& operator=(const memcpy_trivial_cached_limit_queue&) = delete;

    [[nodiscard]] bool try_push(const T& value) noexcept {
        Index tail = producer_.tail.load(std::memory_order_relaxed);
        if (tail == producer_.cached_full_limit) {
            const Index head = consumer_.head.load(std::memory_order_acquire);
            producer_.cached_full_limit = static_cast<Index>(head + capacity_index);
            if (tail == producer_.cached_full_limit) return false;
        }
        std::memcpy(slot_for(tail).bytes.data(), std::addressof(value), sizeof(T));
        ++tail;
        producer_.tail.store(tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept {
        Index head = consumer_.head.load(std::memory_order_relaxed);
        if (head == consumer_.cached_tail) {
            consumer_.cached_tail = producer_.tail.load(std::memory_order_acquire);
            if (head == consumer_.cached_tail) return false;
        }
        std::memcpy(std::addressof(output), slot_for(head).bytes.data(), sizeof(T));
        ++head;
        consumer_.head.store(head, std::memory_order_release);
        return true;
    }

    [[nodiscard]] static constexpr std::size_t testing_slot_index(Index logical_index) noexcept {
        return physical_slot_index<slot_type, Capacity, Mapping>(logical_index);
    }

private:
    [[nodiscard]] slot_type& slot_for(Index logical_index) noexcept {
        return slots_[testing_slot_index(logical_index)];
    }

    producer_state producer_{};
    consumer_state consumer_{};
    alignas(cache_line) std::array<slot_type, Capacity> slots_{};
};

template <class T, std::size_t Capacity>
using raw_cached_seq = raw_cached_limit_queue<T, Capacity, storage_mapping::sequential>;
template <class T, std::size_t Capacity>
using raw_cached_global = raw_cached_limit_queue<T, Capacity, storage_mapping::global_stripe_16b>;
template <class T, std::size_t Capacity>
using raw_cached_tiled = raw_cached_limit_queue<T, Capacity, storage_mapping::tiled_stripe_16b>;
template <class T, std::size_t Capacity>
using typed_cached_seq = typed_trivial_cached_limit_queue<T, Capacity, storage_mapping::sequential>;
template <class T, std::size_t Capacity>
using typed_cached_tiled = typed_trivial_cached_limit_queue<T, Capacity, storage_mapping::tiled_stripe_16b>;
template <class T, std::size_t Capacity>
using memcpy_cached_seq = memcpy_trivial_cached_limit_queue<T, Capacity, storage_mapping::sequential>;
template <class T, std::size_t Capacity>
using memcpy_cached_tiled = memcpy_trivial_cached_limit_queue<T, Capacity, storage_mapping::tiled_stripe_16b>;

} // namespace vqbench::experimental::policy
