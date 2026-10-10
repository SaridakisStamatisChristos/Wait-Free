#pragma once

#include "four_line_wrapped_variants.hpp"
#include "veriqueue/detail/slot.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>

namespace vqbench::experimental::synthesized_arm {

inline constexpr std::size_t cache_line = 64;

template <class T, std::size_t Capacity, class Index = std::size_t>
class wrapped_raw_queue final {
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
    wrapped_raw_queue() noexcept = default;
    wrapped_raw_queue(const wrapped_raw_queue&) = delete;
    wrapped_raw_queue& operator=(const wrapped_raw_queue&) = delete;

    ~wrapped_raw_queue() noexcept {
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

    [[nodiscard]] veriqueue::detail::slot<T>& slot_for(Index index) noexcept {
        return slots_[static_cast<std::size_t>(index)];
    }

    writer_state writer_{};
    reader_state reader_{};
    alignas(cache_line)
        std::array<veriqueue::detail::slot<T>, Capacity + 1> slots_{};
};

template <class T, std::size_t Capacity, class Index = std::size_t>
class isolated_cached_limit_queue final {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2);
    static_assert(std::atomic<Index>::is_always_lock_free);
    static_assert(std::is_nothrow_destructible_v<T>);

    static constexpr Index capacity_index = static_cast<Index>(Capacity);

    struct alignas(cache_line) producer_private final {
        Index cached_full_limit{capacity_index};
    };

    struct alignas(cache_line) published_cursor final {
        std::atomic<Index> value{0};
    };

    struct alignas(cache_line) consumer_private final {
        Index cached_tail{0};
    };

public:
    isolated_cached_limit_queue() noexcept = default;
    isolated_cached_limit_queue(const isolated_cached_limit_queue&) = delete;
    isolated_cached_limit_queue& operator=(const isolated_cached_limit_queue&) = delete;

    ~isolated_cached_limit_queue() noexcept {
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
        if (tail == producer_.cached_full_limit) {
            const Index head = published_head_.value.load(std::memory_order_acquire);
            producer_.cached_full_limit = static_cast<Index>(head + capacity_index);
            if (tail == producer_.cached_full_limit) return false;
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
            consumer_.cached_tail =
                published_tail_.value.load(std::memory_order_acquire);
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
    [[nodiscard]] veriqueue::detail::slot<T>& slot_for(Index logical) noexcept {
        const auto bounded =
            static_cast<std::size_t>(logical) & (Capacity - 1);
        return slots_[bounded];
    }

    producer_private producer_{};
    published_cursor published_tail_{};
    consumer_private consumer_{};
    published_cursor published_head_{};
    alignas(cache_line)
        std::array<veriqueue::detail::slot<T>, Capacity> slots_{};
};

template <
    class T,
    std::size_t Capacity,
    std::size_t TileSlots,
    class Index = std::size_t>
class local_stripe_queue final {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    static_assert(TileSlots >= 8 && (TileSlots & (TileSlots - 1)) == 0);
    static_assert((TileSlots % 8) == 0);
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
    local_stripe_queue() noexcept = default;
    local_stripe_queue(const local_stripe_queue&) = delete;
    local_stripe_queue& operator=(const local_stripe_queue&) = delete;

    ~local_stripe_queue() noexcept {
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

private:
    [[nodiscard]] static constexpr std::size_t slot_index(Index logical) noexcept {
        const std::size_t bounded =
            static_cast<std::size_t>(logical) & (Capacity - 1);
        if constexpr (sizeof(veriqueue::detail::slot<T>) <= 16 &&
                      Capacity >= TileSlots) {
            constexpr std::size_t lanes = 8;
            constexpr std::size_t lane_span = TileSlots / lanes;
            const std::size_t tile_base = bounded & ~(TileSlots - 1);
            const std::size_t local = bounded & (TileSlots - 1);
            const std::size_t lane = local & (lanes - 1);
            const std::size_t ordinal = local >> 3;
            return tile_base + lane * lane_span + ordinal;
        }
        return bounded;
    }

    [[nodiscard]] veriqueue::detail::slot<T>& slot_for(Index logical) noexcept {
        return slots_[slot_index(logical)];
    }

    producer_state producer_{};
    consumer_state consumer_{};
    alignas(cache_line)
        std::array<veriqueue::detail::slot<T>, Capacity> slots_{};
};

template <class T, std::size_t Capacity>
struct selected_queue final {
private:
    using production = veriqueue::spsc_queue<T, Capacity>;
    using wrapped = wrapped_raw_queue<T, Capacity>;
    using isolated = isolated_cached_limit_queue<T, Capacity>;
    using four_line =
        vqbench::experimental::four_line_wrapped::four_line_inline<T, Capacity>;
    using tile128 = local_stripe_queue<T, Capacity, 128>;
    using tile256 = local_stripe_queue<T, Capacity, 256>;

#if defined(__aarch64__) || defined(_M_ARM64)
#if defined(__clang__)
    static constexpr bool clang_keep_production =
        Capacity == 256 && sizeof(T) == 8;
    using impl = std::conditional_t<clang_keep_production, production, four_line>;
#elif defined(__GNUC__)
    static constexpr bool use_wrapped =
        (Capacity == 64 && sizeof(T) == 16) ||
        (Capacity == 256 && (sizeof(T) == 8 || sizeof(T) == 16));
    static constexpr bool use_isolated =
        Capacity == 256 && sizeof(T) == 256;
    static constexpr bool use_four_line =
        Capacity == 1024 && (sizeof(T) == 8 || sizeof(T) == 16);
    static constexpr bool use_tile128 =
        Capacity >= 65536 && sizeof(T) == 16;
    static constexpr bool use_tile256 =
        Capacity >= 65536 && (sizeof(T) == 8 || sizeof(T) == 256);
    using impl = std::conditional_t<
        use_wrapped,
        wrapped,
        std::conditional_t<
            use_isolated,
            isolated,
            std::conditional_t<
                use_four_line,
                four_line,
                std::conditional_t<
                    use_tile128,
                    tile128,
                    std::conditional_t<use_tile256, tile256, production>>>>>;
#else
    using impl = production;
#endif
#else
    using impl = production;
#endif

public:
    selected_queue() noexcept(std::is_nothrow_default_constructible_v<impl>) = default;
    selected_queue(const selected_queue&) = delete;
    selected_queue& operator=(const selected_queue&) = delete;

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        return queue_.try_push(value);
    }

    [[nodiscard]] bool try_pop(T& output) noexcept
        requires std::is_nothrow_move_assignable_v<T>
    {
        return queue_.try_pop(output);
    }

private:
    impl queue_{};
};

} // namespace vqbench::experimental::synthesized_arm
