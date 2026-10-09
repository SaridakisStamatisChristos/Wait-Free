#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace {

struct alignas(128) over_aligned final {
    static inline bool misaligned = false;

    std::uint64_t value{0};
    std::array<std::byte, 120> padding{};

    explicit over_aligned(std::uint64_t v = 0) noexcept : value(v) {
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

} // namespace

int main() {
    static_assert(alignof(veriqueue::spsc_queue<over_aligned, 8>) >= alignof(over_aligned));

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
