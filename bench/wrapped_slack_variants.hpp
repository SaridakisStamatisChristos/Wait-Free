#pragma once

#include "veriqueue/detail/slot.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>

namespace vqbench::experimental::wrapped_slack {

inline constexpr std::size_t cache_line = 64;

template <class T, std::size_t Capacity, class Index = std::size_t>
class raw_queue final {
    static_assert(Capacity >= 1);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity < (std::numeric_limits<Index>::max)());
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_nothrow_destructible_v<T>);

    static constexpr Index physical_capacity = static_cast<Index>(Capacity + 1);

    struct alignas(cache_line) writer_state final {
        std::atomic<Index> write{0};
        Index read_cache{0};
    };

    struct alignas(cache_line) reader_state final {
        std::atomic<Index> read{0};
        Index write_cache{0};
    };

public:
    raw_queue() noexcept = default;
    raw_queue(const raw_queue&) = delete;
    raw_queue& operator=(const raw_queue&) = delete;

    ~raw_queue() noexcept {
        Index read = reader_.read.load(std::memory_order_relaxed);
        const Index write = writer_.write.load(std::memory_order_relaxed);
        while (read != write) {
            std::destroy_at(slots_[static_cast<std::size_t>(read)].live_ptr());
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
        std::construct_at(
            slots_[static_cast<std::size_t>(write)].storage_ptr(), value);
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
        T* const source =
            slots_[static_cast<std::size_t>(read)].live_ptr();
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

    writer_state writer_{};
    reader_state reader_{};
    alignas(cache_line)
        std::array<veriqueue::detail::slot<T>, Capacity + 1> slots_{};
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

    struct alignas(cache_line) writer_state final {
        std::atomic<Index> write{0};
        Index read_cache{0};
    };

    struct alignas(cache_line) reader_state final {
        std::atomic<Index> read{0};
        Index write_cache{0};
    };

public:
    typed_trivial_queue() noexcept = default;
    typed_trivial_queue(const typed_trivial_queue&) = delete;
    typed_trivial_queue& operator=(const typed_trivial_queue&) = delete;

    [[nodiscard]] bool try_push(const T& value) noexcept {
        const Index write = writer_.write.load(std::memory_order_relaxed);
        const Index next_write = next(write);
        if (next_write == writer_.read_cache) {
            writer_.read_cache = reader_.read.load(std::memory_order_acquire);
            if (next_write == writer_.read_cache) return false;
        }
        slots_[static_cast<std::size_t>(write)] = value;
        writer_.write.store(next_write, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept {
        const Index read = reader_.read.load(std::memory_order_relaxed);
        if (read == reader_.write_cache) {
            reader_.write_cache = writer_.write.load(std::memory_order_acquire);
            if (read == reader_.write_cache) return false;
        }
        output = slots_[static_cast<std::size_t>(read)];
        reader_.read.store(next(read), std::memory_order_release);
        return true;
    }

private:
    [[nodiscard]] static constexpr Index next(Index value) noexcept {
        ++value;
        return value == physical_capacity ? Index{0} : value;
    }

    writer_state writer_{};
    reader_state reader_{};
    alignas(cache_line) std::array<T, Capacity + 1> slots_{};
};

} // namespace vqbench::experimental::wrapped_slack
