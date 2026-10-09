#include "veriqueue/spsc_queue.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    veriqueue::spsc_queue<std::uint8_t, 8, 64, std::uint8_t> q;
    std::deque<std::uint8_t> model;

    for (std::size_t i = 0; i < size; ++i) {
        const std::uint8_t opcode = static_cast<std::uint8_t>(data[i] % 5U);
        const std::uint8_t base = static_cast<std::uint8_t>(data[i] >> 3U);

        if (opcode == 0U) {
            const bool actual = q.try_push(base);
            const bool expected = model.size() < 8;
            if (expected) model.push_back(base);
            if (actual != expected) std::abort();
        } else if (opcode == 1U) {
            std::uint8_t out = 0;
            const bool actual = q.try_pop(out);
            const bool expected = !model.empty();
            if (actual != expected) std::abort();
            if (actual) {
                if (out != model.front()) std::abort();
                model.pop_front();
            }
        } else if (opcode == 2U) {
            std::array<std::uint8_t, 4> input{};
            const std::size_t wanted = static_cast<std::size_t>((data[i] >> 5U) + 1U);
            for (std::size_t j = 0; j < wanted; ++j) {
                input[j] = static_cast<std::uint8_t>(base + static_cast<std::uint8_t>(j));
            }
            const std::size_t expected = (std::min)(wanted, 8U - model.size());
            const std::size_t actual =
                q.try_push_bulk(std::span<const std::uint8_t>{input}.first(wanted));
            if (actual != expected) std::abort();
            for (std::size_t j = 0; j < expected; ++j) model.push_back(input[j]);
        } else if (opcode == 3U) {
            std::array<std::uint8_t, 4> output{};
            const std::size_t wanted = static_cast<std::size_t>((data[i] >> 5U) + 1U);
            const std::size_t expected = (std::min)(wanted, model.size());
            const std::size_t actual =
                q.try_pop_bulk(std::span<std::uint8_t>{output}.first(wanted));
            if (actual != expected) std::abort();
            for (std::size_t j = 0; j < actual; ++j) {
                if (output[j] != model.front()) std::abort();
                model.pop_front();
            }
        } else {
            std::uint8_t observed = 0;
            const bool actual = q.try_consume([&](std::uint8_t& value) noexcept { observed = value; });
            const bool expected = !model.empty();
            if (actual != expected) std::abort();
            if (actual) {
                if (observed != model.front()) std::abort();
                model.pop_front();
            }
        }

        if (q.size_approx() != model.size()) std::abort();
    }
    return 0;
}
