#pragma once

#include <cstddef>

namespace veriqueue::detail {

inline constexpr std::size_t default_cache_line = 64;

constexpr bool is_power_of_two(std::size_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

} // namespace veriqueue::detail
