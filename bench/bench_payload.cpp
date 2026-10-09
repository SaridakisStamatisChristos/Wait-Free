#include "bench_common.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

struct p16 { std::array<std::byte, 16> x{}; };
struct p64 { std::array<std::byte, 64> x{}; };
struct p256 { std::array<std::byte, 256> x{}; };

template <class T>
void run(const char* name) {
    const unsigned n = vqbench::hardware_threads();
    const double ops = vqbench::throughput<T, 1024>(2'000'000, 0, n > 1 ? 1U : 0U);
    std::cout << "{\"payload\":\"" << name << "\",\"bytes\":" << sizeof(T)
              << ",\"transfers_per_second\":" << ops << "}\n";
}

int main() {
    run<std::uint64_t>("u64");
    run<p16>("16B");
    run<p64>("64B");
    run<p256>("256B");
}
