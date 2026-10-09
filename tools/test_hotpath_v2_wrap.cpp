#include "experimental_spsc_v2.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <span>

namespace {

using queue_type = vqbench::experimental::v2::single_owner_cached_limit_queue<
    std::uint64_t, 4, 0, false, std::uint32_t>;

[[nodiscard]] bool scalar_wrap() {
    constexpr std::uint32_t start = (std::numeric_limits<std::uint32_t>::max)() - 2U;
    queue_type q{start};

    if (!q.try_push(11U)) return false;
    if (!q.try_push(12U)) return false;
    if (!q.try_push(13U)) return false; // tail wraps UINT32_MAX -> 0
    if (!q.try_push(14U)) return false;
    if (q.try_push(15U)) return false;  // exact capacity remains four

    std::uint64_t value = 0;
    for (const std::uint64_t expected : {11U, 12U, 13U, 14U}) {
        if (!q.try_pop(value) || value != expected) return false;
    }
    if (q.try_pop(value)) return false;

    // Exercise another producer refresh after both owner cursors crossed wrap.
    if (!q.try_push(21U)) return false;
    if (!q.try_pop(value) || value != 21U) return false;
    return !q.try_pop(value);
}

[[nodiscard]] bool bulk_wrap() {
    constexpr std::uint32_t start = (std::numeric_limits<std::uint32_t>::max)() - 1U;
    queue_type q{start};

    const std::array<std::uint64_t, 4> input{31U, 32U, 33U, 34U};
    if (q.try_push_bulk(std::span<const std::uint64_t>{input}) != input.size()) return false;

    const std::array<std::uint64_t, 1> extra{35U};
    if (q.try_push_bulk(std::span<const std::uint64_t>{extra}) != 0U) return false;

    std::array<std::uint64_t, 4> output{};
    if (q.try_pop_bulk(std::span<std::uint64_t>{output}) != output.size()) return false;
    if (output != input) return false;

    std::array<std::uint64_t, 1> empty_probe{};
    return q.try_pop_bulk(std::span<std::uint64_t>{empty_probe}) == 0U;
}

} // namespace

int main() {
    return scalar_wrap() && bulk_wrap() ? 0 : 1;
}
