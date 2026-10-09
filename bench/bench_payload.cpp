#include "bench_common.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

struct p16 final { std::array<std::byte, 16> x{}; };
struct p64 final { std::array<std::byte, 64> x{}; };
struct p256 final { std::array<std::byte, 256> x{}; };

template <class T>
void run(const char* name, vqbench::cpu_pair cpus) {
    constexpr std::uint64_t transfers = 500'000;
    const double rate =
        vqbench::throughput<T, 1024>(transfers, cpus.producer, cpus.consumer);
    std::cout << "{\"benchmark\":\"payload\",\"payload\":\"" << name
              << "\",\"payload_bytes\":" << sizeof(T)
              << ",\"capacity\":1024,\"producer_cpu\":" << cpus.producer
              << ",\"consumer_cpu\":" << cpus.consumer << ",\"topology\":\""
              << vqbench::topology_label() << "\",\"transfers_per_second\":" << rate
              << "}\n";
}

int main() {
    const auto cpus = vqbench::selected_cpu_pair();
    run<std::uint64_t>("u64", cpus);
    run<p16>("16B", cpus);
    run<p64>("64B", cpus);
    run<p256>("256B", cpus);
}
