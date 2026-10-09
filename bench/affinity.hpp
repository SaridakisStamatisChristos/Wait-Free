#pragma once

#include <cstddef>
#include <thread>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace vqbench {

inline bool pin_current_thread(unsigned cpu) noexcept {
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
#else
    (void)cpu;
    return false;
#endif
}

inline unsigned hardware_threads() noexcept {
    const auto n = std::thread::hardware_concurrency();
    return n == 0 ? 1U : n;
}

} // namespace vqbench
