#include <relacy/relacy_std.hpp>

#include <cstddef>

#ifndef VERIQUEUE_RELACY_MUTATION_ID
#define VERIQUEUE_RELACY_MUTATION_ID 0
#endif

namespace {

constexpr int mutation_id = VERIQUEUE_RELACY_MUTATION_ID;
constexpr std::memory_order tail_store_order =
    mutation_id == 1 ? std::memory_order_relaxed : std::memory_order_release;
constexpr std::memory_order tail_load_order =
    mutation_id == 2 ? std::memory_order_relaxed : std::memory_order_acquire;
constexpr std::memory_order head_store_order =
    mutation_id == 3 ? std::memory_order_relaxed : std::memory_order_release;
constexpr std::memory_order head_load_order =
    mutation_id == 4 ? std::memory_order_relaxed : std::memory_order_acquire;

template <std::size_t Capacity>
class mutant_queue final {
public:
    bool push(int value) {
        const unsigned old_tail = producer_tail_($);
        unsigned tail = old_tail;

        if constexpr (mutation_id != 22) {
            unsigned full_limit = static_cast<unsigned>(Capacity);
            if constexpr (mutation_id == 8) {
                full_limit = static_cast<unsigned>(Capacity + 1);
            } else if constexpr (mutation_id == 9) {
                full_limit = static_cast<unsigned>(Capacity - 1);
            }

            if (tail - producer_cached_head_($) == full_limit) {
                if constexpr (mutation_id != 10) {
                    producer_cached_head_($) = head_.load(head_load_order, $);
                }
                if (tail - producer_cached_head_($) == full_limit) return false;
            }
        }

        const unsigned mask = mutation_id == 7
                                  ? 0U
                                  : static_cast<unsigned>(Capacity - 1);
        unsigned slot = tail & mask;
        if constexpr (mutation_id == 14) {
            slot = (slot + 1U) & static_cast<unsigned>(Capacity - 1);
        }

        const int stored_value = mutation_id == 18 ? value + 1000 : value;

        if constexpr (mutation_id == 12) {
            tail += 2U;
        } else {
            ++tail;
        }

        unsigned published_tail = tail;
        if constexpr (mutation_id == 16) {
            ++published_tail;
        } else if constexpr (mutation_id == 24) {
            published_tail = old_tail;
        }

        if constexpr (mutation_id == 5) {
            if constexpr (mutation_id != 19) {
                producer_tail_($) = tail;
            }
            tail_.store(published_tail, tail_store_order, $);
            slots_[slot]($) = stored_value;
        } else {
            slots_[slot]($) = stored_value;
            if constexpr (mutation_id != 19) {
                producer_tail_($) = tail;
            }
            tail_.store(published_tail, tail_store_order, $);
        }
        return true;
    }

    unsigned push_bulk(const int* values, unsigned requested) {
        if (requested == 0U) return 0U;

        const unsigned old_tail = producer_tail_($);
        unsigned tail = old_tail;
        unsigned used = tail - producer_cached_head_($);
        if (used == Capacity) {
            producer_cached_head_($) = head_.load(head_load_order, $);
            used = tail - producer_cached_head_($);
            if (used == Capacity && mutation_id != 29) return 0U;
        }

        const unsigned available = static_cast<unsigned>(Capacity) - used;
        unsigned accepted = requested < available ? requested : available;
        if constexpr (mutation_id == 29) {
            if (requested > accepted) ++accepted;
        }

        unsigned final_tail = tail + accepted;
        unsigned published_tail = final_tail;
        if constexpr (mutation_id == 27) {
            if (accepted > 1U) published_tail = old_tail + 1U;
        }

        if constexpr (mutation_id == 25) {
            producer_tail_($) = final_tail;
            tail_.store(published_tail, tail_store_order, $);
        }

        for (unsigned i = 0; i < accepted; ++i) {
            unsigned slot = tail & static_cast<unsigned>(Capacity - 1);
            if constexpr (mutation_id == 30) {
                if (i == 1U) slot = (slot + 1U) & static_cast<unsigned>(Capacity - 1);
            }
            slots_[slot]($) = values[i];
            ++tail;
        }

        if constexpr (mutation_id != 25) {
            producer_tail_($) = final_tail;
            tail_.store(published_tail, tail_store_order, $);
        }
        return accepted;
    }

    bool pop(int& value) {
        const unsigned old_head = consumer_head_($);
        unsigned head = old_head;

        const bool should_refresh = mutation_id == 21
                                        ? head != consumer_cached_tail_($)
                                        : head == consumer_cached_tail_($);
        if (should_refresh) {
            if constexpr (mutation_id != 11) {
                consumer_cached_tail_($) = tail_.load(tail_load_order, $);
            }
            if (head == consumer_cached_tail_($)) return false;
        }

        const unsigned mask = mutation_id == 7
                                  ? 0U
                                  : static_cast<unsigned>(Capacity - 1);
        unsigned slot = head & mask;
        if constexpr (mutation_id == 15) {
            slot = (slot + 1U) & static_cast<unsigned>(Capacity - 1);
        }

        if constexpr (mutation_id == 13) {
            head += 2U;
        } else {
            ++head;
        }

        unsigned published_head = head;
        if constexpr (mutation_id == 17) {
            ++published_head;
        } else if constexpr (mutation_id == 23) {
            published_head = old_head;
        }

        if constexpr (mutation_id == 6) {
            if constexpr (mutation_id != 20) {
                consumer_head_($) = head;
            }
            head_.store(published_head, head_store_order, $);
            value = slots_[slot]($);
        } else {
            value = slots_[slot]($);
            if constexpr (mutation_id != 20) {
                consumer_head_($) = head;
            }
            head_.store(published_head, head_store_order, $);
        }
        return true;
    }

    unsigned pop_bulk(int* values, unsigned requested) {
        if (requested == 0U) return 0U;

        const unsigned old_head = consumer_head_($);
        unsigned head = old_head;
        unsigned available = consumer_cached_tail_($) - head;
        if (available == 0U) {
            consumer_cached_tail_($) = tail_.load(tail_load_order, $);
            available = consumer_cached_tail_($) - head;
            if (available == 0U) return 0U;
        }

        const unsigned accepted = requested < available ? requested : available;
        const unsigned final_head = head + accepted;
        unsigned published_head = final_head;
        if constexpr (mutation_id == 28) {
            if (accepted > 1U) published_head = old_head + 1U;
        }

        if constexpr (mutation_id == 26) {
            consumer_head_($) = final_head;
            head_.store(published_head, head_store_order, $);
        }

        for (unsigned i = 0; i < accepted; ++i) {
            values[i] = slots_[head & static_cast<unsigned>(Capacity - 1)]($);
            ++head;
        }

        if constexpr (mutation_id != 26) {
            consumer_head_($) = final_head;
            head_.store(published_head, head_store_order, $);
        }
        return accepted;
    }

    bool consume(int& value) {
        const unsigned old_head = consumer_head_($);
        unsigned head = old_head;
        if (head == consumer_cached_tail_($)) {
            consumer_cached_tail_($) = tail_.load(tail_load_order, $);
            if (head == consumer_cached_tail_($)) return false;
        }

        unsigned slot = head & static_cast<unsigned>(Capacity - 1);
        if constexpr (mutation_id == 32) {
            slot = (slot + 1U) & static_cast<unsigned>(Capacity - 1);
        }
        ++head;

        if constexpr (mutation_id == 31) {
            consumer_head_($) = head;
            head_.store(head, head_store_order, $);
            value = slots_[slot]($);
        } else {
            value = slots_[slot]($);
            consumer_head_($) = head;
            head_.store(head, head_store_order, $);
        }
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

struct deterministic_contract_test : rl::test_suite<deterministic_contract_test, 1> {
    mutant_queue<2> q;

    void thread(unsigned) {
        int value = 0;

        RL_ASSERT(q.push(1));
        RL_ASSERT(q.push(2));
        RL_ASSERT(!q.push(3));

        RL_ASSERT(q.pop(value));
        RL_ASSERT(value == 1);

        RL_ASSERT(q.push(3));
        RL_ASSERT(!q.push(4));

        RL_ASSERT(q.pop(value));
        RL_ASSERT(value == 2);
        RL_ASSERT(q.pop(value));
        RL_ASSERT(value == 3);
        RL_ASSERT(!q.pop(value));
        RL_ASSERT(q.final_size() == 0U);
    }
};

struct deterministic_bulk_contract_test : rl::test_suite<deterministic_bulk_contract_test, 1> {
    mutant_queue<2> q;

    void thread(unsigned) {
        const int first[3] = {1, 2, 3};
        RL_ASSERT(q.push_bulk(first, 3U) == 2U);

        int out[2] = {0, 0};
        RL_ASSERT(q.pop_bulk(out, 2U) == 2U);
        RL_ASSERT(out[0] == 1);
        RL_ASSERT(out[1] == 2);
        RL_ASSERT(q.final_size() == 0U);

        const int second[2] = {3, 4};
        RL_ASSERT(q.push_bulk(second, 2U) == 2U);
        int value = 0;
        RL_ASSERT(q.consume(value));
        RL_ASSERT(value == 3);
        RL_ASSERT(q.pop(value));
        RL_ASSERT(value == 4);
        RL_ASSERT(!q.consume(value));
        RL_ASSERT(q.final_size() == 0U);
    }
};

struct concurrent_semantic_test : rl::test_suite<concurrent_semantic_test, 2> {
    static constexpr int operation_count = 5;

    mutant_queue<2> q;
    bool pushed[operation_count]{};
    int popped_values[operation_count]{};
    int push_count{0};
    int pop_count{0};

    void before() {
        for (int i = 0; i < operation_count; ++i) {
            pushed[i] = false;
            popped_values[i] = 0;
        }
        push_count = 0;
        pop_count = 0;
    }

    void thread(unsigned index) {
        if (index == 0) {
            for (int value = 1; value <= operation_count; ++value) {
                if (q.push(value)) {
                    pushed[value - 1] = true;
                    ++push_count;
                }
            }
        } else {
            for (int attempt = 0; attempt < operation_count + 2; ++attempt) {
                int value = 0;
                if (q.pop(value)) {
                    RL_ASSERT(pop_count < operation_count);
                    popped_values[pop_count] = value;
                    ++pop_count;
                }
            }
        }
    }

    void after() {
        RL_ASSERT(pop_count <= push_count);

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
        RL_ASSERT(logical_size <= 2U);
        RL_ASSERT(logical_size == static_cast<unsigned>(push_count - pop_count));
    }
};

struct concurrent_bulk_semantic_test : rl::test_suite<concurrent_bulk_semantic_test, 2> {
    static constexpr int value_count = 6;

    mutant_queue<2> q;
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
            for (int attempt = 0; attempt < value_count + 2; ++attempt) {
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
        RL_ASSERT(logical_size <= 2U);
        RL_ASSERT(logical_size == static_cast<unsigned>(push_count - consume_count));
    }
};

} // namespace

int main() {
    const bool contract_ok = rl::simulate<deterministic_contract_test>();
    const bool bulk_contract_ok = rl::simulate<deterministic_bulk_contract_test>();
    const bool concurrent_ok = rl::simulate<concurrent_semantic_test>();
    const bool concurrent_bulk_ok = rl::simulate<concurrent_bulk_semantic_test>();
    return (contract_ok && bulk_contract_ok && concurrent_ok && concurrent_bulk_ok) ? 0 : 1;
}
