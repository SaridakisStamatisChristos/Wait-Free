#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"
#include "../../bench/pr30_queue_control.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace {

template <class Snapshot>
auto fields(const Snapshot& state) {
    return std::array{
        state.producer_local_tail, state.producer_cached_head,
        state.consumer_local_head, state.consumer_cached_tail,
        state.published_tail, state.published_head};
}

template <std::size_t Capacity>
void mixed_replay() {
    using candidate = veriqueue::spsc_queue<std::uint64_t, Capacity, 64, std::uint8_t>;
    using control = vqbench::pr30_control::spsc_queue<std::uint64_t, Capacity, 64, std::uint8_t>;
    static_assert(std::is_nothrow_default_constructible_v<candidate>);
    static_assert(sizeof(candidate) == sizeof(control));
    static_assert(alignof(candidate) == alignof(control));
    static_assert(candidate::static_capacity == Capacity);
    candidate a;
    control b;
    std::uint64_t seed = 0x97a105ULL;
    std::uint64_t next = 1;
    for (unsigned step = 0; step < 20000; ++step) {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        switch (seed % 5) {
        case 0: {
            const auto value = next++;
            VQ_CHECK(a.try_emplace(value) == b.try_emplace(value));
            break;
        }
        case 1: {
            std::uint64_t x = 0xfeed, y = x;
            VQ_CHECK(a.try_pop(x) == b.try_pop(y));
            VQ_CHECK(x == y);
            break;
        }
        case 2: {
            bool called_a = false, called_b = false;
            std::uint64_t x = 0xfeed, y = x;
            const bool result_a = a.try_consume([&](std::uint64_t& value) noexcept {
                called_a = true;
                x = value;
            });
            const bool result_b = b.try_consume([&](std::uint64_t& value) noexcept {
                called_b = true;
                y = value;
            });
            VQ_CHECK(result_a == result_b && called_a == called_b && x == y);
            break;
        }
        case 3: {
            const std::array<std::uint64_t, 3> input{next, next + 1, next + 2};
            next += input.size();
            VQ_CHECK(a.try_push_bulk(std::span<const std::uint64_t>{input}) ==
                     b.try_push_bulk(std::span<const std::uint64_t>{input}));
            break;
        }
        default: {
            std::array<std::uint64_t, 3> x{0xfeed, 0xfeed, 0xfeed}, y = x;
            VQ_CHECK(a.try_pop_bulk(std::span<std::uint64_t>{x}) ==
                     b.try_pop_bulk(std::span<std::uint64_t>{y}));
            VQ_CHECK(x == y);
            break;
        }
        }
        VQ_CHECK(fields(a.testing_snapshot()) == fields(b.testing_snapshot()));
        VQ_CHECK(a.empty() == b.empty());
        VQ_CHECK(a.size_approx() == b.size_approx());
        VQ_CHECK(a.capacity() == b.capacity());
    }
}

} // namespace

int main() {
    vqtest::run("scalar/bulk/consume PR30 state equivalence with uint8 rollover", [] {
        mixed_replay<1>();
        mixed_replay<2>();
        mixed_replay<4>();
        mixed_replay<64>();
    });
}
