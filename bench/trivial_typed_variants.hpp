#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>

namespace vqbench::experimental::trivial_typed {

template <class T>
concept typed_payload =
    std::is_trivially_copyable_v<T> &&
    std::is_trivially_destructible_v<T> &&
    std::is_default_constructible_v<T> &&
    std::is_copy_assignable_v<T>;

template <typed_payload T, std::size_t Capacity, class Index = std::size_t>
class monotonic_seq_queue final {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2);
    static_assert(std::atomic<Index>::is_always_lock_free);

    static constexpr Index capacity_index = static_cast<Index>(Capacity);
    static constexpr Index mask_index = static_cast<Index>(Capacity - 1);

    struct alignas(64) producer_state final {
        std::atomic<Index> tail{0};
        Index cached_head{0};
    };

    struct alignas(64) consumer_state final {
        std::atomic<Index> head{0};
        Index cached_tail{0};
    };

public:
    monotonic_seq_queue() noexcept = default;
    monotonic_seq_queue(const monotonic_seq_queue&) = delete;
    monotonic_seq_queue& operator=(const monotonic_seq_queue&) = delete;

    [[nodiscard]] bool try_push(const T& value) noexcept {
        Index tail = producer_.tail.load(std::memory_order_relaxed);
        if (distance(tail, producer_.cached_head) == capacity_index) {
            producer_.cached_head = consumer_.head.load(std::memory_order_acquire);
            if (distance(tail, producer_.cached_head) == capacity_index) return false;
        }
        slots_[static_cast<std::size_t>(tail & mask_index)] = value;
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
        output = slots_[static_cast<std::size_t>(head & mask_index)];
        ++head;
        consumer_.head.store(head, std::memory_order_release);
        return true;
    }

private:
    [[nodiscard]] static constexpr Index distance(Index newer, Index older) noexcept {
        return static_cast<Index>(newer - older);
    }

    producer_state producer_{};
    consumer_state consumer_{};
    alignas(64) std::array<T, Capacity> slots_{};
};

template <typed_payload T, std::size_t Capacity, class Index = std::size_t>
class cached_limit_seq_queue final {
    static_assert(Capacity >= 1);
    static_assert((Capacity & (Capacity - 1)) == 0);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity <= (std::numeric_limits<Index>::max)() / 2);
    static_assert(std::atomic<Index>::is_always_lock_free);

    static constexpr Index capacity_index = static_cast<Index>(Capacity);
    static constexpr Index mask_index = static_cast<Index>(Capacity - 1);

    struct alignas(64) producer_state final {
        std::atomic<Index> tail{0};
        Index cached_full_limit{capacity_index};
    };

    struct alignas(64) consumer_state final {
        std::atomic<Index> head{0};
        Index cached_tail{0};
    };

public:
    cached_limit_seq_queue() noexcept = default;
    cached_limit_seq_queue(const cached_limit_seq_queue&) = delete;
    cached_limit_seq_queue& operator=(const cached_limit_seq_queue&) = delete;

    [[nodiscard]] bool try_push(const T& value) noexcept {
        Index tail = producer_.tail.load(std::memory_order_relaxed);
        if (tail == producer_.cached_full_limit) {
            const Index head = consumer_.head.load(std::memory_order_acquire);
            producer_.cached_full_limit = static_cast<Index>(head + capacity_index);
            if (tail == producer_.cached_full_limit) return false;
        }
        slots_[static_cast<std::size_t>(tail & mask_index)] = value;
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
        output = slots_[static_cast<std::size_t>(head & mask_index)];
        ++head;
        consumer_.head.store(head, std::memory_order_release);
        return true;
    }

private:
    producer_state producer_{};
    consumer_state consumer_{};
    alignas(64) std::array<T, Capacity> slots_{};
};

template <
    typed_payload T,
    std::size_t Capacity,
    std::size_t Separation = 64,
    bool HeapStorage = false,
    class Index = std::size_t>
class wrapped_queue final {
    static_assert(Capacity >= 1);
    static_assert(Separation >= alignof(std::atomic<Index>));
    static_assert((Separation & (Separation - 1)) == 0);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(Capacity < (std::numeric_limits<Index>::max)());
    static_assert(std::atomic<Index>::is_always_lock_free);

    static constexpr Index physical_capacity = static_cast<Index>(Capacity + 1);
    static constexpr std::size_t padding =
        ((Separation - 1) / sizeof(T)) + 1;
    static constexpr std::size_t allocation_count =
        Capacity + 1 + 2 * padding;

    struct alignas(Separation) writer_state final {
        std::atomic<Index> write{0};
        Index read_cache{0};
    };

    struct alignas(Separation) reader_state final {
        std::atomic<Index> read{0};
        Index write_cache{0};
    };

public:
    wrapped_queue() {
        if constexpr (HeapStorage) {
            heap_slots_ = std::make_unique<T[]>(allocation_count);
        }
    }

    wrapped_queue(const wrapped_queue&) = delete;
    wrapped_queue& operator=(const wrapped_queue&) = delete;

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
        if constexpr (HeapStorage) {
            return heap_slots_[padding + static_cast<std::size_t>(index)];
        } else {
            return inline_slots_[static_cast<std::size_t>(index)];
        }
    }

    writer_state writer_{};
    reader_state reader_{};
    std::array<T, HeapStorage ? 1 : Capacity + 1> inline_slots_{};
    std::unique_ptr<T[]> heap_slots_{};
};

template <typed_payload T, std::size_t Capacity>
using wrapped64_inline = wrapped_queue<T, Capacity, 64, false>;

template <typed_payload T, std::size_t Capacity>
using wrapped256_heap = wrapped_queue<T, Capacity, 256, true>;

} // namespace vqbench::experimental::trivial_typed
