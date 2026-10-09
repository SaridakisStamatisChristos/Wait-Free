#include "stress_common.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <thread>

namespace {
void tiny_jitter(std::uint64_t& state) {
    state ^= state << 13U;
    state ^= state >> 7U;
    state ^= state << 17U;
    const auto spins = static_cast<unsigned>(state & 0x3FU);
    for (unsigned i = 0; i < spins; ++i) {
        std::atomic_signal_fence(std::memory_order_seq_cst);
    }
    if ((state & 0x3FFU) == 0U) {
        std::this_thread::yield();
    }
}
}

int main() {
    vqtest::run("randomized schedule jitter", [] {
        thread_local std::uint64_t p = 0x123456789ABCDEF0ULL;
        thread_local std::uint64_t c = 0x0FEDCBA987654321ULL;
        const auto res = vqstress::run(300'000, [&] { tiny_jitter(p); }, [&] { tiny_jitter(c); });
        VQ_CHECK(res.ok);
        VQ_CHECK(res.consumed == 300'000);
    });
}
