#include "stress_common.hpp"
#include "test_support.hpp"

int main() {
    vqtest::run("one million ordered transfers", [] {
        const auto res = vqstress::run(1'000'000, [] {}, [] {});
        VQ_CHECK(res.ok);
        VQ_CHECK(res.produced == 1'000'000);
        VQ_CHECK(res.consumed == 1'000'000);
    });
}
