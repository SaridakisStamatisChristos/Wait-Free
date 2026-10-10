#pragma once

#include "veriqueue/detail/slot.hpp"

#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>

namespace vqbench::experimental::wrapped_slack {

inline constexpr std::size_t cache_line = 64;

template <class Element>
inline constexpr std::size_t padding_elements =
    ((cache_line - 1) / sizeof(Element)) + 1;

template <class T, std::size_t Capacity, class Index = std::size_t>
class raw_queue final {
    static_assert(Capacity >= 1);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity < (std::numeric_limits<Index>::max)());
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_nothrow_destructible_v<T>);

    using slot_type = veriqueue::detail::slot<T>;
    static constexpr Index physical_capacity = static_cast<Index>(Capacity + 1);
    static constexpr std::size_t padding = padding_elements<slot_type>;
    static constexpr std::size_t storage_count = Capacity + 1 + 2 * padding;

    struct alignas(cache_line) writer_state final {
        std::atomic<Index> write{0};
        Index read_cache{0};
    };

    struct alignas(cache_line) reader_state final {
        std::atomic<Index> read{0};
        Index write_cache{0};
    };

public:
    raw_queue()
        : slots_(std::make_unique<slot_type[]>(storage_count)) {}
    raw_queue(const raw_queue&) = delete;
    raw_queue& operator=(const raw_queue&) = delete;

    ~raw_queue() noexcept {
        Index read = reader_.read.load(std::memory_order_relaxed);
        const Index write = writer_.write.load(std::memory_order_relaxed);
        while (read != write) {
            std::destroy_at(slot_for(read).live_ptr());
            read = next(read);
        }
    }

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        const Index write = writer_.write.load(std::memory_order_relaxed);
        const Index next_write = next(write);
        if (next_write == writer_.read_cache) {
            writer_.read_cache = reader_.read.load(std::memory_order_acquire);
            if (next_write == writer_.read_cache) return false;
        }
        std::construct_at(slot_for(write).storage_ptr(), value);
        writer_.write.store(next_write, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        const Index read = reader_.read.load(std::memory_order_relaxed);
        if (read == reader_.write_cache) {
            reader_.write_cache = writer_.write.load(std::memory_order_acquire);
            if (read == reader_.write_cache) return false;
        }
        T* const source = slot_for(read).live_ptr();
        output = std::move(*source);
        std::destroy_at(source);
        reader_.read.store(next(read), std::memory_order_release);
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

    writer_state writer_{};
    reader_state reader_{};
    std::unique_ptr<slot_type[]> slots_;
};

template <class T, std::size_t Capacity, class Index = std::size_t>
class typed_trivial_queue final {
    static_assert(Capacity >= 1);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity < (std::numeric_limits<Index>::max)());
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_trivially_destructible_v<T>);
    static_assert(std::is_default_constructible_v<T>);
    static_assert(std::is_copy_assignable_v<T>);

    static constexpr Index physical_capacity = static_cast<Index>(Capacity + 1);
    static constexpr std::size_t padding = padding_elements<T>;
    static constexpr std::size_t storage_count = Capacity + 1 + 2 * padding;

    struct alignas(cache_line) writer_state final {
        std::atomic<Index> write{0};
        Index read_cache{0};
    };

    struct alignas(cache_line) reader_state final {
        std::atomic<Index> read{0};
        Index write_cache{0};
    };

public:
    typed_trivial_queue()
        : slots_(std::make_unique<T[]>(storage_count)) {}
    typed_trivial_queue(const typed_trivial_queue&) = delete;
    typed_trivial_queue& operator=(const typed_trivial_queue&) = delete;

    [[nodiscard]] bool try_push(const T& value) noexcept {
        const Index write = writer_.write.load(std::memory_order_relaxed);
        const Index next_write = next(write);
        if (next_write == writer_.read_cache) {
            writer_.read_cache = reader_.read.load(std::memory_order_acquire);
            if (next_write == writer_.read_cache) return false;
        }
        slot_for(write) = value;
        writer_.write.store(next_write, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept {
        const Index read = reader_.read.load(std::memory_order_relaxed);
        if (read == reader_.write_cache) {
            reader_.write_cache = writer_.write.load(std::memory_order_acquire);
            if (read == reader_.write_cache) return false;
        }
        output = slot_for(read);
        reader_.read.store(next(read), std::memory_order_release);
        return true;
    }

private:
    [[nodiscard]] static constexpr Index next(Index value) noexcept {
        ++value;
        return value == physical_capacity ? Index{0} : value;
    }

    [[nodiscard]] T& slot_for(Index index) noexcept {
        return slots_[padding + static_cast<std::size_t>(index)];
    }

    writer_state writer_{};
    reader_state reader_{};
    std::unique_ptr<T[]> slots_;
};

} // namespace vqbench::experimental::wrapped_slack
