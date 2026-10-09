#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <stdexcept>

struct throwing_construct final {
    static inline bool throw_now = false;
    int value{0};

    explicit throwing_construct(int v) {
        if (throw_now) {
            throw std::runtime_error("injected constructor failure");
        }
        value = v;
    }

    throwing_construct(throwing_construct&&) noexcept = default;
    throwing_construct& operator=(throwing_construct&&) noexcept = default;
};

int main() {
    vqtest::run("constructor failure does not publish", [] {
        veriqueue::spsc_queue<throwing_construct, 2> q;
        throwing_construct::throw_now = true;
        bool threw = false;
        try {
            static_cast<void>(q.try_emplace(7));
        } catch (const std::runtime_error&) {
            threw = true;
        }
        VQ_CHECK(threw);
        VQ_CHECK(q.empty());
        VQ_CHECK(q.size_approx() == 0);

        throwing_construct::throw_now = false;
        VQ_CHECK(q.try_emplace(8));
        throwing_construct out{0};
        VQ_CHECK(q.try_pop(out));
        VQ_CHECK(out.value == 8);
    });
}
