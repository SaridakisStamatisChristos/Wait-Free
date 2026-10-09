#include "bench_common.hpp"

#include <array>
#include <cstdint>

int main() {
    const unsigned n = vqbench::hardware_threads();
    const unsigned p = 0;
    const unsigned c = n > 1 ? 1U : 0U;
    constexpr std::uint64_t transfers = 5'000'000;
    const double ops = vqbench::throughput<std::uint64_t, 1024>(transfers, p, c);
    std::cout << "{\"benchmark\":\"throughput\",\"payload_bytes\":8,\"capacity\":1024,"
              << "\"producer_cpu\":" << p << ",\"consumer_cpu\":" << c
              << ",\"transfers\":" << transfers << ",\"transfers_per_second\":" << ops
              << ",\"environment\":" << vqbench::environment_json() << "}\n";
}
