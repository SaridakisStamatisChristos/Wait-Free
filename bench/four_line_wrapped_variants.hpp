#pragma once

#include "veriqueue/detail/slot.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>

namespace vqbench::experimental::four_line_wrapped {

inline constexpr std::size_t cache_line = 64;

template <class T, std::size_t Capacity, bool FourLine, bool HeapStorage,
          class Index = std::size_t>
class raw_queue final {
    static_assert(Capacity >= 1);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity < (std::numeric_limits<Index>::max)());
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_nothrow_destructible_v<T>);

    using slot_type = veriqueue::detail::slot<T>;
    static constexpr Index physical_capacity = static_cast<Index>(Capacity + 1);
    static constexpr std::size_t padding = ((cache_line - 1) / sizeof(slot_type)) + 1;
    static constexpr std::size_t heap_count = Capacity + 1 + 2 * padding;

    struct grouped_writer final {
        alignas(cache_line) std::atomic<Index> cursor{0};
        Index peer_cache{0};
    };

    struct grouped_reader final {
        alignas(cache_line) std::atomic<Index> cursor{0};
        Index peer_cache{0};
    };

    struct four_line_control final {
        alignas(cache_line) std::atomic<Index> write{0};
        alignas(cache_line) Index read_cache{0};
        alignas(cache_line) std::atomic<Index> read{0};
        alignas(cache_line) Index write_cache{0};
    };

public:
    raw_queue() {
        if constexpr (HeapStorage) {
            heap_slots_ = std::make_unique<slot_type[]>(heap_count);
        }
    }

    raw_queue(const raw_queue&) = delete;
    raw_queue& operator=(const raw_queue&) = delete;

    ~raw_queue() noexcept {
        Index read = read_cursor().load(std::memory_order_relaxed);
        const Index write = write_cursor().load(std::memory_order_relaxed);
        while (read != write) {
            std::destroy_at(slot_for(read).live_ptr());
            read = next(read);
        }
    }

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        const Index write = write_cursor().load(std::memory_order_relaxed);
        const Index next_write = next(write);
        if (next_write == writer_peer_cache()) {
            writer_peer_cache() = read_cursor().load(std::memory_order_acquire);
            if (next_write == writer_peer_cache()) return false;
        }
        std::construct_at(slot_for(write).storage_ptr(), value);
        write_cursor().store(next_write, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        const Index read = read_cursor().load(std::memory_order_relaxed);
        if (read == reader_peer_cache()) {
            reader_peer_cache() = write_cursor().load(std::memory_order_acquire);
            if (read == reader_peer_cache()) return false;
        }
        T* const source = slot_for(read).live_ptr();
        output = std::move(*source);
        std::destroy_at(source);
        read_cursor().store(next(read), std::memory_order_release);
        return true;
    }

private:
    [[nodiscard]] static constexpr Index next(Index value) noexcept {
        ++value;
        return value == physical_capacity ? Index{0} : value;
    }

    [[nodiscard]] std::atomic<Index>& write_cursor() noexcept {
        if constexpr (FourLine) return control_.write;
        else return writer_.cursor;
    }

    [[nodiscard]] std::atomic<Index>& read_cursor() noexcept {
        if constexpr (FourLine) return control_.read;
        else return reader_.cursor;
    }

    [[nodiscard]] Index& writer_peer_cache() noexcept {
        if constexpr (FourLine) return control_.read_cache;
        else return writer_.peer_cache;
    }

    [[nodiscard]] Index& reader_peer_cache() noexcept {
        if constexpr (FourLine) return control_.write_cache;
        else return reader_.peer_cache;
    }

    [[nodiscard]] slot_type& slot_for(Index index) noexcept {
        if constexpr (HeapStorage) {
            return heap_slots_[padding + static_cast<std::size_t>(index)];
        } else {
            return inline_slots_[static_cast<std::size_t>(index)];
        }
    }

    grouped_writer writer_{};
    grouped_reader reader_{};
    four_line_control control_{};
    alignas(cache_line) std::array<slot_type, Capacity + 1> inline_slots_{};
    std::unique_ptr<slot_type[]> heap_slots_{};
};

template <class T, std::size_t Capacity>
using grouped_inline = raw_queue<T, Capacity, false, false>;

template <class T, std::size_t Capacity>
using four_line_inline = raw_queue<T, Capacity, true, false>;

template <class T, std::size_t Capacity>
using four_line_heap = raw_queue<T, Capacity, true, true>;

} // namespace vqbench::experimental::four_line_wrapped
