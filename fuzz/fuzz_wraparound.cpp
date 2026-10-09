#include "veriqueue/spsc_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    veriqueue::spsc_queue<std::uint8_t, 4, 64, std::uint8_t> q;
    std::deque<std::uint8_t> model;
    for (std::size_t round = 0; round < 16; ++round) {
        for (std::size_t i = 0; i < size; ++i) {
            if ((data[i] & 1U) == 0U) {
                const bool a = q.try_push(data[i]);
                const bool b = model.size() < 4;
                if (b) model.push_back(data[i]);
                if (a != b) std::abort();
            } else {
                std::uint8_t out = 0;
                const bool a = q.try_pop(out);
                const bool b = !model.empty();
                if (a != b) std::abort();
                if (a) {
                    if (out != model.front()) std::abort();
                    model.pop_front();
                }
            }
        }
    }
    return 0;
}
