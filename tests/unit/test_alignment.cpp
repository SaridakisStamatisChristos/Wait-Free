#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

struct alignas(128) over_aligned final {
    static inline bool misaligned = false;

    std::uint64_t value{0};
    std::array<std::byte, 120> padding{};

    over_aligned() noexcept {
        check_alignment();
    }

    explicit over_aligned(std::uint64_t v) noexcept : value(v) {
        check_alignment();
    }

    over_aligned(const over_aligned& other) noexcept : value(other.value) {
        check_alignment();
    }

    over_aligned(over_aligned&& other) noexcept : value(other.value) {
        check_alignment();
    }

    over_aligned& operator=(over_aligned&& other) noexcept {
        check_alignment();
        value = other.value;
        return *this;
    }

    over_aligned& operator=(const over_aligned&) = delete;

private:
    void check_alignment() noexcept {
        const auto address = reinterpret_cast<std::uintptr_t>(this);
        if ((address % alignof(over_aligned)) != 0U) misaligned = true;
    }
};

struct payload8 final {
    std::uint64_t a{0};
};

struct payload16 final {
    std::uint64_t a{0};
    std::uint64_t b{0};
};

static_assert(sizeof(payload8) == 8);
static_assert(sizeof(payload16) == 16);

} // namespace

int main() {
    static_assert(alignof(veriqueue::spsc_queue<over_aligned, 8>) >= alignof(over_aligned));

    using queue8 = veriqueue::spsc_queue<payload8, 64>;
    using queue16 = veriqueue::spsc_queue<payload16, 64>;
    using tiny_queue16 = veriqueue::spsc_queue<payload16, 4>;

#if defined(__aarch64__) || defined(_M_ARM64)
    static_assert(queue16::testing_storage_striped());
#else
    static_assert(!queue16::testing_storage_striped());
#endif
    static_assert(!queue8::testing_storage_striped());
    static_assert(!tiny_queue16::testing_storage_striped());

#if !defined(VERIQUEUE_DISABLE_PADDING) && \
    (defined(__x86_64__) || defined(_M_X64))
    static_assert(queue8::testing_split_control());
    static_assert(queue16::testing_split_control());
#else
    static_assert(!queue8::testing_split_control());
    static_assert(!queue16::testing_split_control());
#endif

    vqtest::run("storage mapping selection and permutation", [] {
        std::array<bool, 64> seen{};
        for (std::size_t logical = 0; logical < 64; ++logical) {
            const std::size_t physical = queue16::testing_slot_index(logical);
            VQ_CHECK(physical < 64);
            VQ_CHECK(!seen[physical]);
            seen[physical] = true;
            VQ_CHECK(queue16::testing_slot_index(logical + 64) == physical);
        }
        for (const bool value : seen) VQ_CHECK(value);

#if defined(__aarch64__) || defined(_M_ARM64)
        constexpr std::array<std::size_t, 8> expected_first{
            0, 8, 16, 24, 32, 40, 48, 56};
        for (std::size_t i = 0; i < expected_first.size(); ++i) {
            VQ_CHECK(queue16::testing_slot_index(i) == expected_first[i]);
        }
        VQ_CHECK(queue16::testing_slot_index(8) == 1);
#else
        for (std::size_t i = 0; i < 64; ++i) {
            VQ_CHECK(queue16::testing_slot_index(i) == i);
        }
#endif

        for (std::size_t i = 0; i < 64; ++i) {
            VQ_CHECK(queue8::testing_slot_index(i) == i);
        }
        for (std::size_t i = 0; i < 4; ++i) {
            VQ_CHECK(tiny_queue16::testing_slot_index(i) == i);
        }
    });

    vqtest::run("over-aligned scalar storage", [] {
        over_aligned::misaligned = false;
        veriqueue::spsc_queue<over_aligned, 8> q;
        VQ_CHECK(q.try_emplace(11));
        VQ_CHECK(q.try_emplace(12));

        over_aligned output{};
        VQ_CHECK(q.try_pop(output));
        VQ_CHECK(output.value == 11);
        VQ_CHECK(q.try_pop(output));
        VQ_CHECK(output.value == 12);
        VQ_CHECK(!over_aligned::misaligned);
    });

    vqtest::run("over-aligned bulk storage", [] {
        over_aligned::misaligned = false;
        veriqueue::spsc_queue<over_aligned, 8> q;
        std::array<over_aligned, 3> input{over_aligned{21}, over_aligned{22}, over_aligned{23}};
        std::array<over_aligned, 3> output{};

        VQ_CHECK(q.try_push_bulk(std::span<const over_aligned>{input}) == 3);
        VQ_CHECK(q.try_pop_bulk(std::span<over_aligned>{output}) == 3);
        VQ_CHECK(output[0].value == 21);
        VQ_CHECK(output[1].value == 22);
        VQ_CHECK(output[2].value == 23);
        VQ_CHECK(!over_aligned::misaligned);
    });
}
