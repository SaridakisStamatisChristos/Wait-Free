#include "experimental_policy_variants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace {
struct payload16 final { std::array<std::uint64_t, 2> words{}; };
struct payload8 final { std::uint64_t word{}; };
} // namespace

int main() {
    using namespace vqbench::experimental::policy;
    using global = raw_cached_limit_queue<payload16, 64, storage_mapping::global_stripe_16b>;
    using tiled = raw_cached_limit_queue<payload16, 64, storage_mapping::tiled_stripe_16b>;
    using tiled8 = raw_cached_limit_queue<payload8, 64, storage_mapping::tiled_stripe_16b>;

    constexpr std::array<std::size_t, 8> expected_global{0, 8, 16, 24, 32, 40, 48, 56};
    for (std::size_t i = 0; i < expected_global.size(); ++i) {
        if (global::testing_slot_index(i) != expected_global[i]) return 1;
    }

    constexpr std::array<std::size_t, 16> expected_tiled{
        0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
    std::array<bool, 64> seen{};
    for (std::size_t i = 0; i < 64; ++i) {
        const auto mapped = tiled::testing_slot_index(i);
        if (mapped >= 64 || seen[mapped]) return 2;
        seen[mapped] = true;
        if (i < expected_tiled.size() && mapped != expected_tiled[i]) return 3;
    }

    for (std::size_t i = 0; i < 64; ++i) {
        if (tiled8::testing_slot_index(i) != i) return 4;
    }

    std::cout << "policy layout checks passed\n";
    return 0;
}
