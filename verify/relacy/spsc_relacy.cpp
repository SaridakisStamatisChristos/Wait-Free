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
            producer_cached_head_($) = head_.load(HeadLoad, $);
            if (tail - producer_cached_head_($) == Capacity) return false;
        }
        slots_[tail & (Capacity - 1)]($) = value;
        ++tail;
        producer_tail_($) = tail;
        tail_.store(tail, TailStore, $);
        return true;
    }

    bool pop(int& value) {
        unsigned head = consumer_head_($);
        if (head == consumer_cached_tail_($)) {
            consumer_cached_tail_($) = tail_.load(TailLoad, $);
            if (head == consumer_cached_tail_($)) return false;
        }
        value = slots_[head & (Capacity - 1)]($);
        ++head;
        consumer_head_($) = head;
        head_.store(head, HeadStore, $);
        return true;
    }

    unsigned final_size() {
        return producer_tail_($) - consumer_head_($);
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
    static constexpr int operation_count = 5;

    queue<Capacity> q;
    bool pushed[operation_count]{};
    int popped_values[operation_count]{};
    int push_count{0};
    int pop_count{0};
    int failed_push_count{0};
    int failed_pop_count{0};

    void before() {
        for (int i = 0; i < operation_count; ++i) {
            pushed[i] = false;
            popped_values[i] = 0;
        }
        push_count = 0;
        pop_count = 0;
        failed_push_count = 0;
        failed_pop_count = 0;
    }

    void thread(unsigned index) {
        if (index == 0) {
            for (int value = 1; value <= operation_count; ++value) {
                const bool ok = q.push(value);
                pushed[value - 1] = ok;
                if (ok) {
                    ++push_count;
                } else {
                    ++failed_push_count;
                }
            }
        } else {
            // More attempts than values deliberately exercise both successful and
            // empty observations without adding a retry loop to the modeled queue.
            for (int attempt = 0; attempt < operation_count + static_cast<int>(Capacity); ++attempt) {
                int value = 0;
                if (q.pop(value)) {
                    RL_ASSERT(pop_count < operation_count);
                    popped_values[pop_count] = value;
                    ++pop_count;
                } else {
                    ++failed_pop_count;
                }
            }
        }
    }

    void after() {
        RL_ASSERT(push_count + failed_push_count == operation_count);
        RL_ASSERT(pop_count + failed_pop_count == operation_count + static_cast<int>(Capacity));
        RL_ASSERT(pop_count <= push_count);

        // Successful pops must be exactly the FIFO prefix of successful pushes.
        // Failed pushes are absent from the abstract queue, so skip them when
        // reconstructing the expected sequence.
        int expected_index = 0;
        for (int i = 0; i < pop_count; ++i) {
            while (expected_index < operation_count && !pushed[expected_index]) {
                ++expected_index;
            }
            RL_ASSERT(expected_index < operation_count);
            RL_ASSERT(popped_values[i] == expected_index + 1);
            ++expected_index;
        }

        const unsigned logical_size = q.final_size();
        RL_ASSERT(logical_size <= Capacity);
        RL_ASSERT(logical_size == static_cast<unsigned>(push_count - pop_count));
    }
};

} // namespace

int main() {
    const bool one = rl::simulate<correct_test<1>>();
    const bool two = rl::simulate<correct_test<2>>();
    const bool four = rl::simulate<correct_test<4>>();
    return (one && two && four) ? 0 : 1;
}
