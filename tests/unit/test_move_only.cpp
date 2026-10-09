#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <cstddef>
#include <memory>

struct no_default final {
    int value;
    no_default() = delete;
    explicit no_default(int v) noexcept : value(v) {}
    no_default(no_default&&) noexcept = default;
    no_default& operator=(no_default&&) noexcept = default;
    no_default(const no_default&) = delete;
    no_default& operator=(const no_default&) = delete;
};

struct alignas(128) aligned_payload final {
    std::byte bytes[128]{};
    aligned_payload() noexcept = default;
    aligned_payload(aligned_payload&&) noexcept = default;
    aligned_payload& operator=(aligned_payload&&) noexcept = default;
};

int main() {
    vqtest::run("move only", [] {
        veriqueue::spsc_queue<std::unique_ptr<int>, 4> q;
        VQ_CHECK(q.try_push(std::make_unique<int>(42)));
        std::unique_ptr<int> out;
        VQ_CHECK(q.try_pop(out));
        VQ_CHECK(out && *out == 42);
    });

    vqtest::run("non default constructible", [] {
        veriqueue::spsc_queue<no_default, 2> q;
        VQ_CHECK(q.try_emplace(77));
        no_default out{0};
        VQ_CHECK(q.try_pop(out));
        VQ_CHECK(out.value == 77);
    });

    vqtest::run("large alignment", [] {
        veriqueue::spsc_queue<aligned_payload, 2> q;
        VQ_CHECK(q.try_emplace());
        aligned_payload out;
        VQ_CHECK(q.try_pop(out));
    });
}
