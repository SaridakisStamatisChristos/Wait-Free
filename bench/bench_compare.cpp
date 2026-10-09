#include "affinity.hpp"
#include "environment.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <boost/lockfree/spsc_queue.hpp>
#include <boost/version.hpp>
#include <rigtorp/SPSCQueue.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <thread>

namespace {

template <class Push, class Pop>
double run_pair(Push&& push, Pop&& pop, std::uint64_t transfers, unsigned pcpu, unsigned ccpu) {
    std::atomic<bool> start{false};
    std::thread producer([&] {
        static_cast<void>(vqbench::pin_current_thread(pcpu));
        while (!start.load(std::memory_order_acquire)) {}
        for (std::uint64_t i = 0; i < transfers;) {
            if (push(i)) ++i;
        }
    });
    std::thread consumer([&] {
        static_cast<void>(vqbench::pin_current_thread(ccpu));
        std::uint64_t out = 0;
        while (!start.load(std::memory_order_acquire)) {}
        for (std::uint64_t i = 0; i < transfers;) {
            if (pop(out)) ++i;
        }
    });
    const auto begin = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto end = std::chrono::steady_clock::now();
    return static_cast<double>(transfers) / std::chrono::duration<double>(end - begin).count();
}

void emit(std::string_view name, double rate, std::uint64_t transfers, vqbench::cpu_pair cpus) {
    std::cout << "{\"benchmark\":\"baseline_compare\",\"implementation\":\"" << name
              << "\",\"payload_bytes\":8,\"capacity\":1024,\"producer_cpu\":" << cpus.producer
              << ",\"consumer_cpu\":" << cpus.consumer << ",\"topology\":\""
              << vqbench::topology_label() << "\",\"transfers\":" << transfers
              << ",\"transfers_per_second\":" << rate
              << ",\"boost_version\":" << BOOST_VERSION
              << ",\"rigtorp_commit\":\"59a6a938513ea5004817383711ed35d32385d3ee\""
              << ",\"environment\":"
              << vqbench::environment_json() << "}\n";
}

} // namespace

int main() {
    constexpr std::uint64_t transfers = 5'000'000;
    const auto cpus = vqbench::selected_cpu_pair();

    {
        veriqueue::spsc_queue<std::uint64_t, 1024> q;
        const double rate = run_pair(
            [&](std::uint64_t v) { return q.try_push(v); },
            [&](std::uint64_t& out) { return q.try_pop(out); }, transfers, cpus.producer, cpus.consumer);
        emit("veriqueue", rate, transfers, cpus);
    }
    {
        rigtorp::SPSCQueue<std::uint64_t> q(1024);
        const double rate = run_pair(
            [&](std::uint64_t v) { return q.try_push(v); },
            [&](std::uint64_t& out) {
                auto* ptr = q.front();
                if (ptr == nullptr) return false;
                out = *ptr;
                q.pop();
                return true;
            }, transfers, cpus.producer, cpus.consumer);
        emit("rigtorp", rate, transfers, cpus);
    }
    {
        boost::lockfree::spsc_queue<std::uint64_t, boost::lockfree::capacity<1024>> q;
        const double rate = run_pair(
            [&](std::uint64_t v) { return q.push(v); },
            [&](std::uint64_t& out) { return q.pop(out); }, transfers, cpus.producer, cpus.consumer);
        emit("boost_lockfree", rate, transfers, cpus);
    }
}
