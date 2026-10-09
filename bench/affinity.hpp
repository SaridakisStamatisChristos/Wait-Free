#pragma once

#include <charconv>
#include <cstdlib>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace vqbench {

struct cpu_pair final {
    unsigned producer;
    unsigned consumer;
};

inline std::vector<unsigned> allowed_cpus() {
    std::vector<unsigned> cpus;
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) == 0) {
        for (unsigned cpu = 0; cpu < static_cast<unsigned>(CPU_SETSIZE); ++cpu) {
            if (CPU_ISSET(static_cast<int>(cpu), &set)) {
                cpus.push_back(cpu);
            }
        }
    }
#endif
    if (cpus.empty()) {
        const auto count = std::thread::hardware_concurrency();
        const unsigned usable = count == 0 ? 1U : count;
        for (unsigned cpu = 0; cpu < usable; ++cpu) {
            cpus.push_back(cpu);
        }
    }
    return cpus;
}

inline bool pin_current_thread(unsigned cpu) noexcept {
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    if (cpu >= static_cast<unsigned>(CPU_SETSIZE)) {
        return false;
    }
    CPU_SET(static_cast<int>(cpu), &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
#else
    (void)cpu;
    return false;
#endif
}

inline unsigned parse_cpu_env(const char* name, unsigned fallback) noexcept {
    const char* raw = std::getenv(name);
    if (raw == nullptr) {
        return fallback;
    }
    unsigned value = fallback;
    const std::string_view text(raw);
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return fallback;
    }
    return value;
}

inline cpu_pair selected_cpu_pair() {
    const auto cpus = allowed_cpus();
    const unsigned producer = cpus.front();
    const unsigned consumer = cpus.size() > 1 ? cpus[1] : cpus.front();
    return {
        parse_cpu_env("VERIQUEUE_PRODUCER_CPU", producer),
        parse_cpu_env("VERIQUEUE_CONSUMER_CPU", consumer),
    };
}

inline std::string_view topology_label() noexcept {
    const char* label = std::getenv("VERIQUEUE_TOPOLOGY_LABEL");
    return label == nullptr ? std::string_view{"unspecified"} : std::string_view{label};
}

inline unsigned hardware_threads() noexcept {
    const auto n = std::thread::hardware_concurrency();
    return n == 0 ? 1U : n;
}

} // namespace vqbench
