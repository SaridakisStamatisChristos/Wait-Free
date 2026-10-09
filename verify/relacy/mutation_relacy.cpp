#include <relacy/relacy_std.hpp>

#include <cstddef>

#ifndef VERIQUEUE_RELACY_MUTATION_ID
#define VERIQUEUE_RELACY_MUTATION_ID 0
#endif

namespace {

constexpr std::memory_order tail_store_order =
    VERIQUEUE_RELACY_MUTATION_ID == 1 ? std::memory_order_relaxed : std::memory_order_release;
constexpr std::memory_order tail_load_order =
    VERIQUEUE_RELACY_MUTATION_ID == 2 ? std::memory_order_relaxed : std::memory_order_acquire;
constexpr std::memory_order head_store_order =
    VERIQUEUE_RELACY_MUTATION_ID == 3 ? std::memory_order_relaxed : std::memory_order_release;
constexpr std::memory_order head_load_order =
    VERIQUEUE_RELACY_MUTATION_ID == 4 ? std::memory_order_relaxed : std::memory_order_acquire;

template <std::size_t Capacity>
class mutant_queue final {
public:
    bool push(int value) {
        unsigned tail = producer_tail_($);
        const unsigned full_limit =
            VERIQUEUE_RELACY_MUTATION_ID == 8 ? static_cast<unsigned>(Capacity + 1) : static_cast<unsigned>(Capacity);
        if (tail - producer_cached_head_($) == full_limit) {
            producer_cached_head_($) = head_.load(head_load_order);
            if (tail - producer_cached_head_($) == full_limit) return false;
        }

        const unsigned mask = VERIQUEUE_RELACY_MUTATION_ID == 7
                                  ? 0U
                                  : static_cast<unsigned>(Capacity - 1);
        const unsigned slot = tail & mask;
        ++tail;

        if constexpr (VERIQUEUE_RELACY_MUTATION_ID == 5) {
            producer_tail_($) = tail;
            tail_.store(tail, tail_store_order);
            slots_[slot]($) = value;
        } else {
            slots_[slot]($) = value;
            producer_tail_($) = tail;
            tail_.store(tail, tail_store_order);
        }
        return true;
    }

    bool pop(int& value) {
        unsigned head = consumer_head_($);
        if (head == consumer_cached_tail_($)) {
            consumer_cached_tail_($) = tail_.load(tail_load_order);
            if (head == consumer_cached_tail_($)) return false;
        }

        const unsigned mask = VERIQUEUE_RELACY_MUTATION_ID == 7
                                  ? 0U
                                  : static_cast<unsigned>(Capacity - 1);
        const unsigned slot = head & mask;
        ++head;
        if constexpr (VERIQUEUE_RELACY_MUTATION_ID == 6) {
            consumer_head_($) = head;
            head_.store(head, head_store_order);
            value = slots_[slot]($);
        } else {
            value = slots_[slot]($);
            consumer_head_($) = head;
            head_.store(head, head_store_order);
        }
        return true;
    }

private:
    VAR_T(unsigned) producer_tail_{0};
    VAR_T(unsigned) producer_cached_head_{0};
    std::atomic<unsigned> tail_{0};
    VAR_T(unsigned) consumer_head_{0};
    VAR_T(unsigned) consumer_cached_tail_{0};
    std::atomic<unsigned> head_{0};
    VAR_T(int) slots_[Capacity]{};
};

struct mutation_test : rl::test_suite<mutation_test, 2> {
    mutant_queue<2> q;
    VAR_T(int) last{0};

    void thread(unsigned index) {
        if (index == 0) {
            static_cast<void>(q.push(1));
            static_cast<void>(q.push(2));
            static_cast<void>(q.push(3));
        } else {
            int value = 0;
            for (int i = 0; i < 4; ++i) {
                if (q.pop(value)) {
                    RL_ASSERT(value > last($));
                    last($) = value;
                }
            }
        }
    }
};

} // namespace

int main() {
    const bool matches_expected_success = rl::simulate<mutation_test>();
    return matches_expected_success ? 0 : 1;
}
