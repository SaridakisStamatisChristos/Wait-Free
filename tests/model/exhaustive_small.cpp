#include "queue_model.hpp"
#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

struct op final { bool push; std::uint32_t value; };

template <std::size_t Capacity>
void replay(const std::vector<op>& ops) {
    veriqueue::spsc_queue<std::uint32_t, Capacity, 64, std::uint8_t> q;
    vqmodel::bounded_fifo<std::uint32_t> model(Capacity);
    for (const auto& operation : ops) {
        if (operation.push) {
            VQ_CHECK(q.try_push(operation.value) == model.push(operation.value));
        } else {
            std::uint32_t out = 0;
            const bool actual = q.try_pop(out);
            const auto expected = model.pop();
            VQ_CHECK(actual == expected.has_value());
            if (actual) {
                VQ_CHECK(out == *expected);
            }
        }
    }
}

template <std::size_t Capacity>
void enumerate(std::vector<op>& ops, std::size_t depth, std::uint32_t next) {
    if (depth == 0) {
        replay<Capacity>(ops);
        return;
    }
    ops.push_back({false, 0});
    enumerate<Capacity>(ops, depth - 1, next);
    ops.back() = {true, next};
    enumerate<Capacity>(ops, depth - 1, next + 1);
    ops.pop_back();
}

int main() {
    vqtest::run("exhaustive capacity 1 depth 12", [] {
        std::vector<op> ops;
        enumerate<1>(ops, 12, 1);
    });
    vqtest::run("exhaustive capacity 2 depth 12", [] {
        std::vector<op> ops;
        enumerate<2>(ops, 12, 1);
    });
}
