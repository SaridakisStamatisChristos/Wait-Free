#include "veriqueue/spsc_queue.hpp"
#include <cstdint>

extern "C" bool veriqueue_probe_push(veriqueue::spsc_queue<std::uint64_t, 1024>& q,
                                      std::uint64_t value) {
    return q.try_push(value);
}

extern "C" bool veriqueue_probe_pop(veriqueue::spsc_queue<std::uint64_t, 1024>& q,
                                     std::uint64_t& value) {
    return q.try_pop(value);
}
