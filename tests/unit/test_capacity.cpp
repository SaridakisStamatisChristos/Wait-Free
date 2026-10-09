#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <cstddef>

template <std::size_t Capacity>
void boundary_case() {
    veriqueue::spsc_queue<std::size_t, Capacity> q;
    for (std::size_t i = 0; i < Capacity; ++i) {
        VQ_CHECK(q.try_push(i));
    }
    VQ_CHECK(!q.try_push(Capacity));
    std::size_t out = 0;
    VQ_CHECK(q.try_pop(out));
    VQ_CHECK(out == 0);
    VQ_CHECK(q.try_push(Capacity));
    for (std::size_t expected = 1; expected <= Capacity; ++expected) {
        VQ_CHECK(q.try_pop(out));
        VQ_CHECK(out == expected);
    }
}

int main() {
    vqtest::run("capacity 2", [] { boundary_case<2>(); });
    vqtest::run("capacity 64", [] { boundary_case<64>(); });
    vqtest::run("capacity 1024", [] { boundary_case<1024>(); });
}
