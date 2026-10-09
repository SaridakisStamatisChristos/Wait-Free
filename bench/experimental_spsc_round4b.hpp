#pragma once

#include "experimental_spsc_round4.hpp"
#include "veriqueue/detail/slot.hpp"

#include <cstddef>

namespace vqbench::experimental::round4b {

// Surgical follow-up to round 4: retain the production sequential slot mapping
// for every slot size except exactly 16 bytes, where round 4 found a cross-
// compiler ARM64 signal for an 8-way stripe. Object footprint is unchanged.
template <class T>
inline constexpr std::size_t selective_stripe =
    sizeof(veriqueue::detail::slot<T>) == 16 ? 8 : 1;

template <class T, std::size_t Capacity>
using selective16_stripe8 =
    vqbench::experimental::round4::striped_queue<T, Capacity, selective_stripe<T>>;

} // namespace vqbench::experimental::round4b
