#include "bench_common.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {
std::uint64_t transfer_count() noexcept {
    constexpr std::uint64_t fallback = 250'000;
    const char* raw = std::getenv("VERIQUEUE_TRANSFERS");
    if (raw == nullptr) {
        return fallback;
    }
    std::uint64_t value = fallback;
    const std::string_view text(raw);
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && value > 0
               ? value
               : fallback;
}

template <std::size_t Capacity>
void emit_case(std::string_view pattern, double rate, std::uint64_t transfers,
               vqbench::cpu_pair cpus) {
    std::cout << "{\"benchmark\":\"throughput\",\"pattern\":\"" << pattern
              << "\",\"payload_bytes\":8,\"capacity\":" << Capacity
              << ",\"producer_cpu\":" << cpus.producer << ",\"consumer_cpu\":"
              << cpus.consumer << ",\"topology\":\"" << vqbench::topology_label()
              << "\",\"transfers\":" << transfers << ",\"transfers_per_second\":"
              << rate << ",\"environment\":" << vqbench::environment_json() << "}\n";
}

template <std::size_t Capacity>
void run_capacity(std::uint64_t transfers, vqbench::cpu_pair cpus) {
    emit_case<Capacity>("steady",
                        vqbench::throughput<std::uint64_t, Capacity>(transfers, cpus.producer,
                                                                    cpus.consumer),
                        transfers, cpus);
    emit_case<Capacity>("producer_dominant",
                        vqbench::throughput<std::uint64_t, Capacity>(
                            transfers, cpus.producer, cpus.consumer, {0, 64}),
                        transfers, cpus);
    emit_case<Capacity>("consumer_dominant",
                        vqbench::throughput<std::uint64_t, Capacity>(
                            transfers, cpus.producer, cpus.consumer, {64, 0}),
                        transfers, cpus);
    const auto capacity = static_cast<std::uint64_t>(Capacity);
    const auto fill_drain_transfers = std::max<std::uint64_t>(1, transfers / capacity) * capacity;
    emit_case<Capacity>("fill_drain",
                        vqbench::fill_drain_throughput<std::uint64_t, Capacity>(
                            transfers, cpus.producer, cpus.consumer),
                        fill_drain_transfers, cpus);
}
} // namespace

int main() {
    const auto cpus = vqbench::selected_cpu_pair();
    const auto transfers = transfer_count();
    run_capacity<1>(transfers, cpus);
    run_capacity<2>(transfers, cpus);
    run_capacity<64>(transfers, cpus);
    run_capacity<256>(transfers, cpus);
    run_capacity<1024>(transfers, cpus);
    run_capacity<65536>(transfers, cpus);
}
