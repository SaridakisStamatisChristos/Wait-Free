#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

int main() {
    vqtest::run("bulk push and pop preserve exact FIFO across partial batches", [] {
        veriqueue::spsc_queue<int, 4> q;
        const std::array<int, 6> input{1, 2, 3, 4, 5, 6};

        const auto first = q.try_push_bulk(std::span<const int>{input});
        VQ_CHECK(first == 4);
        VQ_CHECK(q.try_push_bulk(std::span<const int>{input}.subspan(4)) == 0);

        std::array<int, 2> first_out{};
        VQ_CHECK(q.try_pop_bulk(std::span<int>{first_out}) == 2);
        VQ_CHECK(first_out[0] == 1);
        VQ_CHECK(first_out[1] == 2);

        VQ_CHECK(q.try_push_bulk(std::span<const int>{input}.subspan(4)) == 2);

        std::array<int, 4> second_out{};
        VQ_CHECK(q.try_pop_bulk(std::span<int>{second_out}) == 4);
        VQ_CHECK((second_out == std::array<int, 4>{3, 4, 5, 6}));
        VQ_CHECK(q.empty());
    });

    vqtest::run("bulk capacity one and empty spans", [] {
        veriqueue::spsc_queue<std::uint64_t, 1> q;
        const std::array<std::uint64_t, 2> input{7, 8};
        VQ_CHECK(q.try_push_bulk(std::span<const std::uint64_t>{}) == 0);
        VQ_CHECK(q.try_push_bulk(std::span<const std::uint64_t>{input}) == 1);
        VQ_CHECK(q.try_push_bulk(std::span<const std::uint64_t>{input}.subspan(1)) == 0);

        std::array<std::uint64_t, 1> out{};
        VQ_CHECK(q.try_pop_bulk(std::span<std::uint64_t>{}) == 0);
        VQ_CHECK(q.try_pop_bulk(std::span<std::uint64_t>{out}) == 1);
        VQ_CHECK(out[0] == 7);
        VQ_CHECK(q.try_pop_bulk(std::span<std::uint64_t>{out}) == 0);
    });

    vqtest::run("bulk operations survive reduced-width rollover", [] {
        using queue_type = veriqueue::spsc_queue<std::uint64_t, 8, 64, std::uint8_t>;
        queue_type q;
        std::uint64_t next = 1;
        for (int round = 0; round < 400; ++round) {
            const std::array<std::uint64_t, 3> input{next, next + 1, next + 2};
            VQ_CHECK(q.try_push_bulk(std::span<const std::uint64_t>{input}) == input.size());
            std::array<std::uint64_t, 3> out{};
            VQ_CHECK(q.try_pop_bulk(std::span<std::uint64_t>{out}) == out.size());
            VQ_CHECK(out == input);
            next += input.size();
        }
        VQ_CHECK(q.empty());
    });

    vqtest::run("try_consume exposes and retires exactly one FIFO element", [] {
        veriqueue::spsc_queue<int, 4> q;
        VQ_CHECK(q.try_push(41));
        VQ_CHECK(q.try_push(42));

        int observed = 0;
        VQ_CHECK(q.try_consume([&](int& value) noexcept {
            observed = value;
            value = -1;
        }));
        VQ_CHECK(observed == 41);

        VQ_CHECK(q.try_consume([&](const int& value) noexcept { observed = value; }));
        VQ_CHECK(observed == 42);
        VQ_CHECK(!q.try_consume([&](int&) noexcept { observed = 999; }));
        VQ_CHECK(observed == 42);
        VQ_CHECK(q.empty());
    });
}
