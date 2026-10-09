#pragma once

#include "experimental_spsc_round4.hpp"
#include "veriqueue/detail/slot.hpp"

#include <cstddef>

namespace vqbench::experimental::round5 {

#if defined(__aarch64__) || defined(_M_ARM64)
inline constexpr bool target_aarch64 = true;
#else
inline constexpr bool target_aarch64 = false;
#endif

// Promotion-shaped experimental selector derived from round 4C evidence:
// use the 8-way storage permutation only on AArch64 and only when the slot is
// exactly 16 bytes. Every other architecture / slot size retains the normal
// sequential physical slot order (Stripe == 1).
template <class T>
inline constexpr std::size_t architecture_selective_stripe =
    target_aarch64 && sizeof(veriqueue::detail::slot<T>) == 16 ? 8 : 1;

template <class T, std::size_t Capacity>
using architecture_selective =
    vqbench::experimental::round4::striped_queue<
        T, Capacity, architecture_selective_stripe<T>>;

} // namespace vqbench::experimental::round5
