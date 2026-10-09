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

    unsigned push_bulk(const int* values, unsigned requested) {
        if (requested == 0U) return 0U;

        unsigned tail = producer_tail_($);
        unsigned used = tail - producer_cached_head_($);
        if (used == Capacity) {
            producer_cached_head_($) = head_.load(HeadLoad, $);
            used = tail - producer_cached_head_($);
            if (used == Capacity) return 0U;
        }

        const unsigned available = static_cast<unsigned>(Capacity) - used;
        const unsigned accepted = requested < available ? requested : available;
        for (unsigned i = 0; i < accepted; ++i) {
            slots_[tail & (Capacity - 1)]($) = values[i];
            ++tail;
        }
        producer_tail_($) = tail;
        tail_.store(tail, TailStore, $);
        return accepted;
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

    unsigned pop_bulk(int* values, unsigned requested) {
        if (requested == 0U) return 0U;

        unsigned head = consumer_head_($);
        unsigned available = consumer_cached_tail_($) - head;
        if (available == 0U) {
            consumer_cached_tail_($) = tail_.load(TailLoad, $);
            available = consumer_cached_tail_($) - head;
            if (available == 0U) return 0U;
        }

        const unsigned accepted = requested < available ? requested : available;
        for (unsigned i = 0; i < accepted; ++i) {
            values[i] = slots_[head & (Capacity - 1)]($);
            ++head;
        }
        consumer_head_($) = head;
        head_.store(head, HeadStore, $);
        return accepted;
    }

    bool consume(int& value) {
        return pop(value);
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

template <std::size_t Capacity>
struct bulk_correct_test : rl::test_suite<bulk_correct_test<Capacity>, 2> {
    static constexpr int value_count = 6;

    queue<Capacity> q;
    bool pushed[value_count]{};
    int consumed_values[value_count]{};
    int push_count{0};
    int consume_count{0};

    void before() {
        for (int i = 0; i < value_count; ++i) {
            pushed[i] = false;
            consumed_values[i] = 0;
        }
        push_count = 0;
        consume_count = 0;
    }

    void thread(unsigned index) {
        if (index == 0) {
            for (int first = 1; first <= value_count; first += 2) {
                const int values[2] = {first, first + 1};
                const unsigned accepted = q.push_bulk(values, 2U);
                RL_ASSERT(accepted <= 2U);
                for (unsigned i = 0; i < accepted; ++i) {
                    pushed[(first - 1) + static_cast<int>(i)] = true;
                    ++push_count;
                }
            }
        } else {
            for (int attempt = 0; attempt < value_count + static_cast<int>(Capacity); ++attempt) {
                if ((attempt & 1) == 0) {
                    int values[2] = {0, 0};
                    const unsigned count = q.pop_bulk(values, 2U);
                    for (unsigned i = 0; i < count; ++i) {
                        RL_ASSERT(consume_count < value_count);
                        consumed_values[consume_count++] = values[i];
                    }
                } else {
                    int value = 0;
                    if (q.consume(value)) {
                        RL_ASSERT(consume_count < value_count);
                        consumed_values[consume_count++] = value;
                    }
                }
            }
        }
    }

    void after() {
        RL_ASSERT(consume_count <= push_count);
        int expected_index = 0;
        for (int i = 0; i < consume_count; ++i) {
            while (expected_index < value_count && !pushed[expected_index]) {
                ++expected_index;
            }
            RL_ASSERT(expected_index < value_count);
            RL_ASSERT(consumed_values[i] == expected_index + 1);
            ++expected_index;
        }

        const unsigned logical_size = q.final_size();
        RL_ASSERT(logical_size <= Capacity);
        RL_ASSERT(logical_size == static_cast<unsigned>(push_count - consume_count));
    }
};

} // namespace

int main() {
    const bool scalar_one = rl::simulate<correct_test<1>>();
    const bool scalar_two = rl::simulate<correct_test<2>>();
    const bool scalar_four = rl::simulate<correct_test<4>>();
    const bool bulk_one = rl::simulate<bulk_correct_test<1>>();
    const bool bulk_two = rl::simulate<bulk_correct_test<2>>();
    const bool bulk_four = rl::simulate<bulk_correct_test<4>>();
    return (scalar_one && scalar_two && scalar_four && bulk_one && bulk_two && bulk_four) ? 0 : 1;
}
