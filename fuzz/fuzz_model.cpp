#include "veriqueue/spsc_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    veriqueue::spsc_queue<std::uint8_t, 8, 64, std::uint8_t> q;
    std::deque<std::uint8_t> model;
    for (std::size_t i = 0; i < size; ++i) {
        if ((data[i] & 1U) == 0U) {
            const auto value = static_cast<std::uint8_t>(data[i] >> 1U);
            const bool actual = q.try_push(value);
            const bool expected = model.size() < 8;
            if (expected) model.push_back(value);
            if (actual != expected) std::abort();
        } else {
            std::uint8_t out = 0;
            const bool actual = q.try_pop(out);
            const bool expected = !model.empty();
            if (actual != expected) std::abort();
            if (actual) {
                if (out != model.front()) std::abort();
                model.pop_front();
            }
        }
        if (q.size_approx() != model.size()) std::abort();
    }
    return 0;
}
