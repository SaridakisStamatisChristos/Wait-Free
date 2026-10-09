#include "veriqueue/spsc_queue.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

struct payload final {
    static inline std::atomic<int> live{0};
    int value{0};
    explicit payload(int v) noexcept : value(v) { ++live; }
    payload(payload&& other) noexcept : value(other.value) { ++live; }
    payload& operator=(payload&& other) noexcept { value = other.value; return *this; }
    payload(const payload&) = delete;
    payload& operator=(const payload&) = delete;
    ~payload() noexcept { --live; }
};

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    payload::live = 0;
    {
        veriqueue::spsc_queue<payload, 4> q;
        payload out{0};
        for (std::size_t i = 0; i < size; ++i) {
            if ((data[i] & 1U) == 0U) {
                static_cast<void>(q.try_emplace(static_cast<int>(data[i])));
            } else {
                static_cast<void>(q.try_pop(out));
            }
        }
    }
    if (payload::live.load() != 0) std::abort();
    return 0;
}
