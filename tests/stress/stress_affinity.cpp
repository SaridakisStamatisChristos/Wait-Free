#include "stress_common.hpp"
#include "test_support.hpp"

#include <thread>

int main() {
    vqtest::run("affinity-neutral concurrency smoke", [] {
        const auto res = vqstress::run(
            250'000,
            [] { std::this_thread::yield(); },
            [] { std::this_thread::yield(); });
        VQ_CHECK(res.ok);
        VQ_CHECK(res.consumed == 250'000);
    });
}
