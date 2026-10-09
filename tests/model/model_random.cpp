#include "queue_model.hpp"
#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <cstdint>
#include <random>

int main() {
    vqtest::run("random model equivalence", [] {
        constexpr std::size_t capacity = 8;
        veriqueue::spsc_queue<std::uint32_t, capacity, 64, std::uint8_t> q;
        vqmodel::bounded_fifo<std::uint32_t> model(capacity);
        std::mt19937_64 rng(0xA11CE5EEDULL);
        std::uint32_t next_value = 1;

        for (std::size_t i = 0; i < 1'000'000; ++i) {
            const bool push = (rng() & 1U) == 0U;
            if (push) {
                const bool a = q.try_push(next_value);
                const bool b = model.push(next_value);
                VQ_CHECK(a == b);
                if (a) {
                    ++next_value;
                }
            } else {
                std::uint32_t out = 0;
                const bool a = q.try_pop(out);
                const auto b = model.pop();
                VQ_CHECK(a == b.has_value());
                if (a) {
                    VQ_CHECK(out == *b);
                }
            }
            VQ_CHECK(q.size_approx() == model.size());
        }
    });
}
