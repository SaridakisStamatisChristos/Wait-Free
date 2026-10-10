#pragma once

#include "veriqueue/detail/slot.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>

namespace vqbench::experimental::clean_four_line {

template <
    class T,
    std::size_t Capacity,
    std::size_t Separation,
    class Index = std::size_t>
class inline_queue final {
    static_assert(Capacity >= 1);
    static_assert(Separation >= alignof(std::atomic<Index>));
    static_assert((Separation & (Separation - 1)) == 0);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity < (std::numeric_limits<Index>::max)());
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_nothrow_destructible_v<T>);

    using slot_type = veriqueue::detail::slot<T>;
    static constexpr Index physical_capacity = static_cast<Index>(Capacity + 1);

public:
    inline_queue() noexcept = default;
    inline_queue(const inline_queue&) = delete;
    inline_queue& operator=(const inline_queue&) = delete;

    ~inline_queue() noexcept {
        Index read = read_.load(std::memory_order_relaxed);
        const Index write = write_.load(std::memory_order_relaxed);
        while (read != write) {
            std::destroy_at(slot_for(read).live_ptr());
            read = next(read);
        }
    }

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        const Index write = write_.load(std::memory_order_relaxed);
        const Index next_write = next(write);
        if (next_write == read_cache_) {
            read_cache_ = read_.load(std::memory_order_acquire);
            if (next_write == read_cache_) return false;
        }
        std::construct_at(slot_for(write).storage_ptr(), value);
        write_.store(next_write, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        const Index read = read_.load(std::memory_order_relaxed);
        if (read == write_cache_) {
            write_cache_ = write_.load(std::memory_order_acquire);
            if (read == write_cache_) return false;
        }
        T* const source = slot_for(read).live_ptr();
        output = std::move(*source);
        std::destroy_at(source);
        read_.store(next(read), std::memory_order_release);
        return true;
    }

private:
    [[nodiscard]] static constexpr Index next(Index value) noexcept {
        ++value;
        return value == physical_capacity ? Index{0} : value;
    }

    [[nodiscard]] slot_type& slot_for(Index index) noexcept {
        return slots_[static_cast<std::size_t>(index)];
    }

    alignas(Separation) std::atomic<Index> write_{0};
    alignas(Separation) Index read_cache_{0};
    alignas(Separation) std::atomic<Index> read_{0};
    alignas(Separation) Index write_cache_{0};
    alignas(Separation) std::array<slot_type, Capacity + 1> slots_{};
};

template <
    class T,
    std::size_t Capacity,
    std::size_t Separation,
    class Index = std::size_t>
class heap_queue final {
    static_assert(Capacity >= 1);
    static_assert(Separation >= alignof(std::atomic<Index>));
    static_assert((Separation & (Separation - 1)) == 0);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity < (std::numeric_limits<Index>::max)());
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_nothrow_destructible_v<T>);

    using slot_type = veriqueue::detail::slot<T>;
    static constexpr Index physical_capacity = static_cast<Index>(Capacity + 1);
    static constexpr std::size_t padding =
        ((Separation - 1) / sizeof(slot_type)) + 1;
    static constexpr std::size_t allocation_count =
        Capacity + 1 + 2 * padding;

public:
    heap_queue() : slots_(std::make_unique<slot_type[]>(allocation_count)) {}
    heap_queue(const heap_queue&) = delete;
    heap_queue& operator=(const heap_queue&) = delete;

    ~heap_queue() noexcept {
        Index read = read_.load(std::memory_order_relaxed);
        const Index write = write_.load(std::memory_order_relaxed);
        while (read != write) {
            std::destroy_at(slot_for(read).live_ptr());
            read = next(read);
        }
    }

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        const Index write = write_.load(std::memory_order_relaxed);
        const Index next_write = next(write);
        if (next_write == read_cache_) {
            read_cache_ = read_.load(std::memory_order_acquire);
            if (next_write == read_cache_) return false;
        }
        std::construct_at(slot_for(write).storage_ptr(), value);
        write_.store(next_write, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        const Index read = read_.load(std::memory_order_relaxed);
        if (read == write_cache_) {
            write_cache_ = write_.load(std::memory_order_acquire);
            if (read == write_cache_) return false;
        }
        T* const source = slot_for(read).live_ptr();
        output = std::move(*source);
        std::destroy_at(source);
        read_.store(next(read), std::memory_order_release);
        return true;
    }

private:
    [[nodiscard]] static constexpr Index next(Index value) noexcept {
        ++value;
        return value == physical_capacity ? Index{0} : value;
    }

    [[nodiscard]] slot_type& slot_for(Index index) noexcept {
        return slots_[padding + static_cast<std::size_t>(index)];
    }

    std::unique_ptr<slot_type[]> slots_;
    alignas(Separation) std::atomic<Index> write_{0};
    alignas(Separation) Index read_cache_{0};
    alignas(Separation) std::atomic<Index> read_{0};
    alignas(Separation) Index write_cache_{0};
};

} // namespace vqbench::experimental::clean_four_line
