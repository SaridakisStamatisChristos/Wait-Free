#include "experimental_spsc_round4.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

template <std::size_t Bytes>
struct payload final {
    std::array<std::uint64_t, Bytes / sizeof(std::uint64_t)> words{};
};

template <std::size_t Stripe>
[[nodiscard]] bool mapping_is_permutation() {
    std::array<bool, 64> seen{};
    for (std::size_t logical = 0; logical < 64; ++logical) {
        const auto physical = vqbench::experimental::round4::striped_index<64, Stripe>(logical);
        if (physical >= 64 || seen[physical]) return false;
        seen[physical] = true;
    }
    for (const bool value : seen) {
        if (!value) return false;
    }
    return true;
}

using production8 = veriqueue::spsc_queue<payload<8>, 1024>;
using production16 = veriqueue::spsc_queue<payload<16>, 1024>;

using stripe2_8 = vqbench::experimental::round4::stripe2<payload<8>, 1024>;
using stripe4_8 = vqbench::experimental::round4::stripe4<payload<8>, 1024>;
using stripe8_8 = vqbench::experimental::round4::stripe8<payload<8>, 1024>;
using stripe16_8 = vqbench::experimental::round4::stripe16<payload<8>, 1024>;
using adaptive8 = vqbench::experimental::round4::adaptive_full<payload<8>, 1024>;
using adaptive16 = vqbench::experimental::round4::adaptive_full<payload<16>, 1024>;

static_assert(sizeof(stripe2_8) == sizeof(production8));
static_assert(sizeof(stripe4_8) == sizeof(production8));
static_assert(sizeof(stripe8_8) == sizeof(production8));
static_assert(sizeof(stripe16_8) == sizeof(production8));
static_assert(sizeof(adaptive8) == sizeof(production8));
static_assert(sizeof(adaptive16) == sizeof(production16));
static_assert(alignof(stripe8_8) == alignof(production8));
static_assert(alignof(adaptive16) == alignof(production16));

} // namespace

int main() {
    if (!mapping_is_permutation<2>()) return 2;
    if (!mapping_is_permutation<4>()) return 4;
    if (!mapping_is_permutation<8>()) return 8;
    if (!mapping_is_permutation<16>()) return 16;
    return 0;
}
