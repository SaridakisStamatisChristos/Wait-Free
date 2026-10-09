#include <relacy/relacy_std.hpp>

#include <cstddef>
#include <cstdint>

namespace {

template <std::size_t Capacity, std::memory_order TailStore = std::memory_order_release,
          std::memory_order TailLoad = std::memory_order_acquire,
          std::memory_order HeadStore = std::memory_order_release,
          std::memory_order HeadLoad = std::memory_order_acquire>
class queue final {
public:
    bool push(int value) {
        unsigned tail = producer_tail_($);
        if (tail - producer_cached_head_($) == Capacity) {
            producer_cached_head_($) = head_.load(HeadLoad);
            if (tail - producer_cached_head_($) == Capacity) return false;
        }
        slots_[tail & (Capacity - 1)]($) = value;
        ++tail;
        producer_tail_($) = tail;
        tail_.store(tail, TailStore);
        return true;
    }

    bool pop(int& value) {
        unsigned head = consumer_head_($);
        if (head == consumer_cached_tail_($)) {
            consumer_cached_tail_($) = tail_.load(TailLoad);
            if (head == consumer_cached_tail_($)) return false;
        }
        value = slots_[head & (Capacity - 1)]($);
        ++head;
        consumer_head_($) = head;
        head_.store(head, HeadStore);
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

template <std::size_t Capacity>
struct correct_test : rl::test_suite<correct_test<Capacity>, 2> {
    queue<Capacity> q;
    VAR_T(int) popped{0};

    void thread(unsigned index) {
        if (index == 0) {
            static_cast<void>(q.push(1));
            static_cast<void>(q.push(2));
            static_cast<void>(q.push(3));
        } else {
            int last = 0;
            int value = 0;
            for (int i = 0; i < 3; ++i) {
                if (q.pop(value)) {
                    RL_ASSERT(value > last);
                    last = value;
                    popped($) += 1;
                }
            }
        }
    }
};

} // namespace

int main() {
    const bool one = rl::simulate<correct_test<1>>();
    const bool two = rl::simulate<correct_test<2>>();
    return (one && two) ? 0 : 1;
}
