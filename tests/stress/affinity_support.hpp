#pragma once

#include <thread>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace vqstress {

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
    if (cpu >= static_cast<unsigned>(CPU_SETSIZE)) {
        return false;
    }
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(static_cast<int>(cpu), &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
#else
    (void)cpu;
    return true;
#endif
}

inline std::pair<unsigned, unsigned> affinity_pair() {
    const auto cpus = allowed_cpus();
    return {cpus.front(), cpus.size() > 1 ? cpus[1] : cpus.front()};
}

} // namespace vqstress
