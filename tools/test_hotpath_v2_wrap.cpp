#include "experimental_spsc_v2.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>

namespace {

using queue_type = vqbench::experimental::v2::single_owner_cached_limit_queue<
    std::uint64_t, 4, 0, false, std::uint32_t>;

struct alignas(128) over_aligned final {
    std::uint64_t value{0};
};

using over_aligned_storage =
    vqbench::experimental::v2::slot_storage<over_aligned, 4, 96>;
using over_aligned_queue =
    vqbench::experimental::v2::single_owner_cached_limit_queue<over_aligned, 4, 96>;

static_assert(alignof(over_aligned_storage) >= alignof(over_aligned));
static_assert(alignof(over_aligned_queue) >= alignof(over_aligned));

[[nodiscard]] bool fail(std::string_view where) {
    std::cerr << "hotpath-v2 wrap verification failed at: " << where << '\n';
    return false;
}

[[nodiscard]] bool scalar_wrap() {
    constexpr std::uint32_t start = (std::numeric_limits<std::uint32_t>::max)() - 2U;
    queue_type q{start};

    if (!q.try_push(std::uint64_t{11})) return fail("scalar push 11");
    if (!q.try_push(std::uint64_t{12})) return fail("scalar push 12");
    if (!q.try_push(std::uint64_t{13})) return fail("scalar push 13 across wrap");
    if (!q.try_push(std::uint64_t{14})) return fail("scalar push 14 after wrap");
    if (q.try_push(std::uint64_t{15})) return fail("scalar capacity overflow accepted");

    std::uint64_t value = 0;
    for (const std::uint64_t expected : {11U, 12U, 13U, 14U}) {
        if (!q.try_pop(value)) return fail("scalar pop unexpectedly empty");
        if (value != expected) return fail("scalar FIFO value mismatch");
    }
    if (q.try_pop(value)) return fail("scalar empty pop accepted");

    if (!q.try_push(std::uint64_t{21})) return fail("scalar post-wrap refresh push");
    if (!q.try_pop(value)) return fail("scalar post-wrap refresh pop");
    if (value != 21U) return fail("scalar post-wrap refresh value mismatch");
    if (q.try_pop(value)) return fail("scalar final empty pop accepted");
    return true;
}

[[nodiscard]] bool bulk_wrap() {
    constexpr std::uint32_t start = (std::numeric_limits<std::uint32_t>::max)() - 1U;
    queue_type q{start};

    const std::array<std::uint64_t, 4> input{31U, 32U, 33U, 34U};
    if (q.try_push_bulk(std::span<const std::uint64_t>{input}) != input.size()) {
        return fail("bulk push across wrap count");
    }

    const std::array<std::uint64_t, 1> extra{35U};
    if (q.try_push_bulk(std::span<const std::uint64_t>{extra}) != 0U) {
        return fail("bulk capacity overflow accepted");
    }

    std::array<std::uint64_t, 4> output{};
    if (q.try_pop_bulk(std::span<std::uint64_t>{output}) != output.size()) {
        return fail("bulk pop across wrap count");
    }
    if (output != input) return fail("bulk FIFO value mismatch");

    std::array<std::uint64_t, 1> empty_probe{};
    if (q.try_pop_bulk(std::span<std::uint64_t>{empty_probe}) != 0U) {
        return fail("bulk empty pop accepted");
    }
    return true;
}

[[nodiscard]] bool over_aligned_layout() {
    over_aligned_queue q{};
    const over_aligned input{0x123456789abcdef0ULL};
    if (!q.try_push(input)) return fail("over-aligned push");
    over_aligned output{};
    if (!q.try_pop(output)) return fail("over-aligned pop");
    if (output.value != input.value) return fail("over-aligned value mismatch");
    return true;
}

} // namespace

int main() {
    if (!scalar_wrap()) return 11;
    if (!bulk_wrap()) return 12;
    if (!over_aligned_layout()) return 13;
    return 0;
}
