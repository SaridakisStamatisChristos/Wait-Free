#include "bench_common.hpp"

#include <cstdint>
#include <iostream>

int main() {
    const auto cpus = vqbench::selected_cpu_pair();
    constexpr std::uint64_t transfers = 500'000;
    for (const std::uint64_t burst : {1ULL, 8ULL, 64ULL, 256ULL}) {
        const double rate = vqbench::throughput<std::uint64_t, 1024>(
            transfers, cpus.producer, cpus.consumer, {burst, burst});
        std::cout << "{\"benchmark\":\"bursts\",\"pattern\":\"burst\",\"burst\":"
                  << burst << ",\"payload_bytes\":8,\"capacity\":1024,\"producer_cpu\":"
                  << cpus.producer << ",\"consumer_cpu\":" << cpus.consumer
                  << ",\"topology\":\"" << vqbench::topology_label()
                  << "\",\"transfers\":" << transfers << ",\"transfers_per_second\":"
                  << rate << "}\n";
    }
}
