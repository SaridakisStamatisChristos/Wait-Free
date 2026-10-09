#pragma once

#include <cstddef>
#include <memory>

namespace veriqueue::detail {

template <class T>
struct slot {
    alignas(T) std::byte bytes[sizeof(T)];

    [[nodiscard]] T* storage_ptr() noexcept {
        return reinterpret_cast<T*>(bytes);
    }

    [[nodiscard]] const T* storage_ptr() const noexcept {
        return reinterpret_cast<const T*>(bytes);
    }

    [[nodiscard]] T* live_ptr() noexcept {
        return std::launder(storage_ptr());
    }

    [[nodiscard]] const T* live_ptr() const noexcept {
        return std::launder(storage_ptr());
    }
};

} // namespace veriqueue::detail
