#include "bench_common.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>

int main() {
    const unsigned n = vqbench::hardware_threads();
    for (std::uint64_t batch : {1ULL, 8ULL, 64ULL, 256ULL}) {
        const auto transfers = batch * 20'000ULL;
        const double ops = vqbench::throughput<std::uint64_t, 1024>(transfers, 0, n > 1 ? 1U : 0U);
        std::cout << "{\"benchmark\":\"burst_proxy\",\"batch\":" << batch
                  << ",\"transfers_per_second\":" << ops << "}\n";
    }
}
