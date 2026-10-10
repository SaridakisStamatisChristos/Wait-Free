#pragma once

#include "veriqueue/detail/slot.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace vqbench::experimental::rigtorp_codegen {

inline constexpr std::size_t arm_destructive_span = 256;

template <class T, std::size_t Capacity, class Allocator = std::allocator<T>, bool SplitConsumer = false>
class dynamic_raw_queue final {
    static_assert(Capacity >= 1);
    static_assert(std::is_nothrow_destructible_v<T>);

    static constexpr std::size_t padding =
        ((arm_destructive_span - 1) / sizeof(T)) + 1;

public:
    dynamic_raw_queue() : capacity_(Capacity + 1), slots_(nullptr) {
        slots_ = std::allocator_traits<Allocator>::allocate(
            allocator_, capacity_ + 2 * padding);
    }

    dynamic_raw_queue(const dynamic_raw_queue&) = delete;
    dynamic_raw_queue& operator=(const dynamic_raw_queue&) = delete;

    ~dynamic_raw_queue() noexcept {
        auto read = read_.load(std::memory_order_relaxed);
        const auto write = write_.load(std::memory_order_relaxed);
        while (read != write) {
            std::destroy_at(slot_ptr(read));
            read = next(read);
        }
        std::allocator_traits<Allocator>::deallocate(
            allocator_, slots_, capacity_ + 2 * padding);
    }

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        const auto write = write_.load(std::memory_order_relaxed);
        const auto next_write = next(write);
        if (next_write == read_cache_) {
            read_cache_ = read_.load(std::memory_order_acquire);
            if (next_write == read_cache_) return false;
        }
        ::new (static_cast<void*>(slot_ptr(write))) T(value);
        write_.store(next_write, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        if constexpr (SplitConsumer) {
            T* const source = front();
            if (source == nullptr) return false;
            output = std::move(*source);
            pop_front();
            return true;
        } else {
            const auto read = read_.load(std::memory_order_relaxed);
            if (read == write_cache_) {
                write_cache_ = write_.load(std::memory_order_acquire);
                if (read == write_cache_) return false;
            }
            T* const source = slot_ptr(read);
            output = std::move(*source);
            std::destroy_at(source);
            read_.store(next(read), std::memory_order_release);
            return true;
        }
    }

private:
    // Experimental consumer API shape only. The layout, producer, allocation,
    // physical capacity and acquire/release protocol match dynamic_raw.
    [[nodiscard]] T* front() noexcept {
        const auto read = read_.load(std::memory_order_relaxed);
        if (read == write_cache_) {
            write_cache_ = write_.load(std::memory_order_acquire);
            if (read == write_cache_) return nullptr;
        }
        return slot_ptr(read);
    }

    void pop_front() noexcept {
        const auto read = read_.load(std::memory_order_relaxed);
        std::destroy_at(slot_ptr(read));
        read_.store(next(read), std::memory_order_release);
    }

    [[nodiscard]] std::size_t next(std::size_t value) const noexcept {
        ++value;
        return value == capacity_ ? 0 : value;
    }

    [[nodiscard]] T* slot_ptr(std::size_t index) noexcept {
        return slots_ + padding + index;
    }

    std::size_t capacity_;
    T* slots_;
    [[no_unique_address]] Allocator allocator_{};
    alignas(arm_destructive_span) std::atomic<std::size_t> write_{0};
    alignas(arm_destructive_span) std::size_t read_cache_{0};
    alignas(arm_destructive_span) std::atomic<std::size_t> read_{0};
    alignas(arm_destructive_span) std::size_t write_cache_{0};
};

template <class T, std::size_t Capacity>
using dynamic_split_queue = dynamic_raw_queue<T, Capacity, std::allocator<T>, true>;

template <class T, std::size_t Capacity, class Allocator = std::allocator<T>>
class static_raw_queue final {
    static_assert(Capacity >= 1);
    static_assert(std::is_nothrow_destructible_v<T>);

    static constexpr std::size_t physical_capacity = Capacity + 1;
    static constexpr std::size_t padding =
        ((arm_destructive_span - 1) / sizeof(T)) + 1;
    static constexpr std::size_t allocation_count =
        physical_capacity + 2 * padding;

public:
    static_raw_queue()
        : slots_(std::allocator_traits<Allocator>::allocate(
              allocator_, allocation_count)) {}

    static_raw_queue(const static_raw_queue&) = delete;
    static_raw_queue& operator=(const static_raw_queue&) = delete;

    ~static_raw_queue() noexcept {
        auto read = read_.load(std::memory_order_relaxed);
        const auto write = write_.load(std::memory_order_relaxed);
        while (read != write) {
            std::destroy_at(slot_ptr(read));
            read = next(read);
        }
        std::allocator_traits<Allocator>::deallocate(
            allocator_, slots_, allocation_count);
    }

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        const auto write = write_.load(std::memory_order_relaxed);
        const auto next_write = next(write);
        if (next_write == read_cache_) {
            read_cache_ = read_.load(std::memory_order_acquire);
            if (next_write == read_cache_) return false;
        }
        ::new (static_cast<void*>(slot_ptr(write))) T(value);
        write_.store(next_write, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        const auto read = read_.load(std::memory_order_relaxed);
        if (read == write_cache_) {
            write_cache_ = write_.load(std::memory_order_acquire);
            if (read == write_cache_) return false;
        }
        T* const source = slot_ptr(read);
        output = std::move(*source);
        std::destroy_at(source);
        read_.store(next(read), std::memory_order_release);
        return true;
    }

private:
    [[nodiscard]] static constexpr std::size_t next(std::size_t value) noexcept {
        ++value;
        return value == physical_capacity ? 0 : value;
    }

    [[nodiscard]] T* slot_ptr(std::size_t index) noexcept {
        return slots_ + padding + index;
    }

    [[no_unique_address]] Allocator allocator_{};
    T* slots_;
    alignas(arm_destructive_span) std::atomic<std::size_t> write_{0};
    alignas(arm_destructive_span) std::size_t read_cache_{0};
    alignas(arm_destructive_span) std::atomic<std::size_t> read_{0};
    alignas(arm_destructive_span) std::size_t write_cache_{0};
};

template <class T, std::size_t Capacity>
class static_slot_queue final {
    static_assert(Capacity >= 1);
    static_assert(std::is_nothrow_destructible_v<T>);

    using slot_type = veriqueue::detail::slot<T>;
    using allocator_type = std::allocator<slot_type>;
    static constexpr std::size_t physical_capacity = Capacity + 1;
    static constexpr std::size_t padding =
        ((arm_destructive_span - 1) / sizeof(slot_type)) + 1;
    static constexpr std::size_t allocation_count =
        physical_capacity + 2 * padding;

public:
    static_slot_queue()
        : slots_(std::allocator_traits<allocator_type>::allocate(
              allocator_, allocation_count)) {}

    static_slot_queue(const static_slot_queue&) = delete;
    static_slot_queue& operator=(const static_slot_queue&) = delete;

    ~static_slot_queue() noexcept {
        auto read = read_.load(std::memory_order_relaxed);
        const auto write = write_.load(std::memory_order_relaxed);
        while (read != write) {
            std::destroy_at(slot_for(read).live_ptr());
            read = next(read);
        }
        std::allocator_traits<allocator_type>::deallocate(
            allocator_, slots_, allocation_count);
    }

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        const auto write = write_.load(std::memory_order_relaxed);
        const auto next_write = next(write);
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
        const auto read = read_.load(std::memory_order_relaxed);
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
    [[nodiscard]] static constexpr std::size_t next(std::size_t value) noexcept {
        ++value;
        return value == physical_capacity ? 0 : value;
    }

    [[nodiscard]] slot_type& slot_for(std::size_t index) noexcept {
        return slots_[padding + index];
    }

    [[no_unique_address]] allocator_type allocator_{};
    slot_type* slots_;
    alignas(arm_destructive_span) std::atomic<std::size_t> write_{0};
    alignas(arm_destructive_span) std::size_t read_cache_{0};
    alignas(arm_destructive_span) std::atomic<std::size_t> read_{0};
    alignas(arm_destructive_span) std::size_t write_cache_{0};
};

} // namespace vqbench::experimental::rigtorp_codegen
