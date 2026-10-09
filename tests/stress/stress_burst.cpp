#include "stress_common.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <thread>

int main() {
    vqtest::run("burst perturbation", [] {
        thread_local std::uint32_t p = 0;
        thread_local std::uint32_t c = 0;
        const auto res = vqstress::run(
            400'000,
            [&] { if ((++p & 63U) == 0U) std::this_thread::yield(); },
            [&] { if ((++c & 7U) == 0U) std::this_thread::yield(); });
        VQ_CHECK(res.ok);
        VQ_CHECK(res.consumed == 400'000);
    });
}
