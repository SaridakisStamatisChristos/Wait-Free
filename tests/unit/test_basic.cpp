#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <cstdint>

int main() {
    vqtest::run("default construction and FIFO", [] {
        veriqueue::spsc_queue<int, 4> q;
        VQ_CHECK(q.empty());
        VQ_CHECK(q.capacity() == 4);
        VQ_CHECK(q.size_approx() == 0);
        VQ_CHECK(q.try_push(1));
        VQ_CHECK(q.try_push(2));
        VQ_CHECK(q.try_push(3));
        VQ_CHECK(q.try_push(4));
        VQ_CHECK(!q.try_push(5));
        VQ_CHECK(q.size_approx() == 4);

        int out = 0;
        for (int expected = 1; expected <= 4; ++expected) {
            VQ_CHECK(q.try_pop(out));
            VQ_CHECK(out == expected);
        }
        VQ_CHECK(!q.try_pop(out));
        VQ_CHECK(q.empty());
    });

    vqtest::run("capacity one", [] {
        veriqueue::spsc_queue<std::uint64_t, 1> q;
        VQ_CHECK(q.try_push(7));
        VQ_CHECK(!q.try_push(8));
        std::uint64_t out = 0;
        VQ_CHECK(q.try_pop(out));
        VQ_CHECK(out == 7);
        VQ_CHECK(q.try_push(9));
        VQ_CHECK(q.try_pop(out));
        VQ_CHECK(out == 9);
    });
}
