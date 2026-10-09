#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <cstdint>
#include <type_traits>

template <class Index, std::size_t Capacity>
void run_wraps(std::size_t operations) {
    veriqueue::spsc_queue<std::uint32_t, Capacity, 64, Index> q;
    std::uint32_t out = 0;
    for (std::size_t i = 0; i < operations; ++i) {
        const auto value = static_cast<std::uint32_t>(i);
        VQ_CHECK(q.try_push(value));
        VQ_CHECK(q.try_pop(out));
        VQ_CHECK(out == value);
    }
    VQ_CHECK(q.empty());
    const auto snapshot = q.testing_snapshot();
    VQ_CHECK(snapshot.published_tail == snapshot.published_head);
}

int main() {
    vqtest::run("uint8 capacity 1", [] { run_wraps<std::uint8_t, 1>(50'000); });
    vqtest::run("uint8 capacity 2", [] { run_wraps<std::uint8_t, 2>(50'000); });
    vqtest::run("uint8 capacity 4", [] { run_wraps<std::uint8_t, 4>(50'000); });
    vqtest::run("uint8 capacity 8", [] { run_wraps<std::uint8_t, 8>(50'000); });
    vqtest::run("uint8 capacity 16", [] { run_wraps<std::uint8_t, 16>(50'000); });
    vqtest::run("uint16 capacity 64", [] { run_wraps<std::uint16_t, 64>(250'000); });
}
