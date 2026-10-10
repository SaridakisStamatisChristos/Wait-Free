#pragma once

#include "veriqueue/detail/slot.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <memory>
#include <type_traits>
#include <utility>

namespace vqbench::experimental::rigtorp_codegen {

inline constexpr std::size_t arm_destructive_span = 256;

// Lab-only stateless allocator: allocation alignment is the sole policy change.
// Rebinding to slot<T> preserves payload alignment and the existing element count.
template <class T>
struct aligned_buffer_allocator {
    using value_type = T;
    using is_always_equal = std::true_type;
    static constexpr std::size_t alignment = alignof(T) > 256 ? alignof(T) : 256;

    aligned_buffer_allocator() noexcept = default;
    template <class U>
    aligned_buffer_allocator(const aligned_buffer_allocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t count) {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }
        return static_cast<T*>(::operator new(count * sizeof(T), std::align_val_t{alignment}));
    }
    void deallocate(T* pointer, std::size_t) noexcept {
        ::operator delete(pointer, std::align_val_t{alignment});
    }
    template <class U>
    bool operator==(const aligned_buffer_allocator<U>&) const noexcept { return true; }
};

template <
    class T,
    std::size_t Capacity,
    class Allocator = std::allocator<T>,
    bool SplitConsumer = false,
    bool GroupOwnerCache = false,
    std::size_t ControlSpan = arm_destructive_span,
    bool CachePeer = true,
    bool CacheOnProgress = false,
    bool SlotStorage = false,
    bool InlineStorage = false>
class dynamic_raw_queue final {
    static_assert(Capacity >= 1);
    static_assert(std::is_nothrow_destructible_v<T>);
    static_assert(!CacheOnProgress || CachePeer);
    static_assert(ControlSpan == arm_destructive_span ||
                  (ControlSpan == 64 && GroupOwnerCache));

    static_assert(!InlineStorage || SlotStorage);

    using managed_slot = veriqueue::detail::slot<T>;
    using storage_type = std::conditional_t<SlotStorage, managed_slot, T>;
    using storage_allocator = std::conditional_t<
        SlotStorage,
        typename std::allocator_traits<Allocator>::template rebind_alloc<storage_type>,
        Allocator>;
    static_assert(sizeof(managed_slot) == sizeof(T));
    static_assert(alignof(managed_slot) == alignof(T));
    static_assert(std::is_trivially_default_constructible_v<managed_slot>);
    static_assert(std::is_trivially_destructible_v<managed_slot>);

    struct no_reserved_bytes {};
    // Keep the standard-allocator object's size/alignment and allocation class
    // unchanged while moving only the reader control. These bytes are not slots.
    using reserved_bytes = std::conditional_t<
        ControlSpan == 64, std::array<std::byte, 256>, no_reserved_bytes>;

    static constexpr std::size_t padding =
        ((arm_destructive_span - 1) / sizeof(T)) + 1;
    static constexpr std::size_t owner_cache_alignment =
        GroupOwnerCache ? alignof(std::size_t) : arm_destructive_span;

    struct no_inline_storage {};
    using inline_storage_type = std::conditional_t<
        InlineStorage, std::array<managed_slot, Capacity + 1 + 2 * padding>, no_inline_storage>;
    static_assert(!InlineStorage || std::is_nothrow_default_constructible_v<storage_allocator>);

public:
    dynamic_raw_queue() noexcept(InlineStorage) : capacity_(Capacity + 1), slots_(nullptr) {
        if constexpr (InlineStorage) {
            // The member array is default-initialized, never value-initialized.
            // Its slot/byte lifetimes begin without constructing T or zeroing.
            slots_ = inline_slots_.data();
        } else {
            slots_ = std::allocator_traits<storage_allocator>::allocate(
                allocator_, capacity_ + 2 * padding);
            if constexpr (SlotStorage) {
                // Begin the byte-buffer lifetimes without constructing T or
                // zeroing storage. Construction and live access stay separate.
                std::uninitialized_default_construct_n(slots_, capacity_ + 2 * padding);
            }
        }
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
        if constexpr (!InlineStorage) {
            if constexpr (SlotStorage) {
                std::destroy_n(slots_, capacity_ + 2 * padding);
            }
            std::allocator_traits<storage_allocator>::deallocate(
                allocator_, slots_, capacity_ + 2 * padding);
        }
        // Inline slot lifetimes end automatically with the member array.
    }

    [[nodiscard]] bool try_push(const T& value)
        requires std::is_copy_constructible_v<T>
    {
        const auto write = write_.load(std::memory_order_relaxed);
        const auto next_write = next(write);
        if constexpr (CachePeer) {
            if (next_write == read_cache_) {
                const auto observed = read_.load(std::memory_order_acquire);
                if constexpr (CacheOnProgress) {
                    if (next_write == observed) return false;
                    read_cache_ = observed;
                } else {
                    read_cache_ = observed;
                    if (next_write == read_cache_) return false;
                }
            }
        } else {
            if (next_write == read_.load(std::memory_order_acquire)) return false;
        }
        ::new (static_cast<void*>(storage_ptr(write))) T(value);
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
            if constexpr (CachePeer) {
                if (read == write_cache_) {
                    const auto observed = write_.load(std::memory_order_acquire);
                    if constexpr (CacheOnProgress) {
                        if (read == observed) return false;
                        write_cache_ = observed;
                    } else {
                        write_cache_ = observed;
                        if (read == write_cache_) return false;
                    }
                }
            } else {
                if (read == write_.load(std::memory_order_acquire)) return false;
            }
            T* const source = slot_ptr(read);
            output = std::move(*source);
            std::destroy_at(source);
            read_.store(next(read), std::memory_order_release);
            return true;
        }
    }

    // Observe allocation and first usable slot only outside benchmark timing.
    [[nodiscard]] std::array<std::size_t, 6> buffer_offsets() const noexcept {
        const auto base = reinterpret_cast<std::uintptr_t>(slots_);
        const auto first = reinterpret_cast<std::uintptr_t>(slots_ + padding);
        return {base % 64, base % 128, base % 256, first % 64, first % 128, first % 256};
    }

    [[nodiscard]] static constexpr std::array<std::size_t, 7> layout_offsets() noexcept {
        return {offsetof(dynamic_raw_queue, capacity_), offsetof(dynamic_raw_queue, slots_),
                offsetof(dynamic_raw_queue, write_), offsetof(dynamic_raw_queue, read_cache_),
                offsetof(dynamic_raw_queue, read_), offsetof(dynamic_raw_queue, write_cache_),
                offsetof(dynamic_raw_queue, inline_slots_)};
    }

private:
    // The split consumer copies before reloading its owner cursor. Peer refresh
    // uses the same compile-time policy as the fused consumer above.
    [[nodiscard]] T* front() noexcept {
        const auto read = read_.load(std::memory_order_relaxed);
        if constexpr (CachePeer) {
            if (read == write_cache_) {
                const auto observed = write_.load(std::memory_order_acquire);
                if constexpr (CacheOnProgress) {
                    if (read == observed) return nullptr;
                    write_cache_ = observed;
                } else {
                    write_cache_ = observed;
                    if (read == write_cache_) return nullptr;
                }
            }
        } else {
            if (read == write_.load(std::memory_order_acquire)) return nullptr;
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
        if constexpr (SlotStorage) {
            return slots_[padding + index].live_ptr();
        } else {
            return slots_ + padding + index;
        }
    }

    [[nodiscard]] T* storage_ptr(std::size_t index) noexcept {
        if constexpr (SlotStorage) {
            return slots_[padding + index].storage_ptr();
        } else {
            return slots_ + padding + index;
        }
    }

    std::size_t capacity_;
    storage_type* slots_;
    [[no_unique_address]] storage_allocator allocator_{};
    alignas(arm_destructive_span) std::atomic<std::size_t> write_{0};
    alignas(owner_cache_alignment) std::size_t read_cache_{0};
    alignas(ControlSpan) std::atomic<std::size_t> read_{0};
    alignas(owner_cache_alignment) std::size_t write_cache_{0};
    [[no_unique_address]] reserved_bytes reserved_;
    // All existing control offsets remain fixed. Padding precedes/follows the
    // usable physical ring exactly as in the heap control.
    alignas(InlineStorage ? arm_destructive_span : 1)
    [[no_unique_address]] inline_storage_type inline_slots_;
};

template <class T, std::size_t Capacity>
using inline_managed_queue =
    dynamic_raw_queue<T, Capacity, std::allocator<T>, true, true, 256, true, true, true, true>;

template <class T, std::size_t Capacity>
using dynamic_aligned_queue =
    dynamic_raw_queue<T, Capacity, aligned_buffer_allocator<T>, true, true, 256, true, true, true>;

template <class T, std::size_t Capacity>
using dynamic_managed_queue = dynamic_raw_queue<T, Capacity, std::allocator<T>, true, true, 256, true, true, true>;

template <class T, std::size_t Capacity>
using dynamic_progress_queue = dynamic_raw_queue<T, Capacity, std::allocator<T>, true, true, 256, true, true>;

template <class T, std::size_t Capacity>
using dynamic_direct_queue = dynamic_raw_queue<T, Capacity, std::allocator<T>, true, true, 256, false>;

template <class T, std::size_t Capacity>
using dynamic_ctrl64_queue = dynamic_raw_queue<T, Capacity, std::allocator<T>, true, true, 64>;

template <class T, std::size_t Capacity>
using dynamic_combined_queue = dynamic_raw_queue<T, Capacity, std::allocator<T>, true, true>;

template <class T, std::size_t Capacity>
using dynamic_grouped_queue = dynamic_raw_queue<T, Capacity, std::allocator<T>, false, true>;

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


