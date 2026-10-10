#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <cstdint>

namespace {

using queue_type = veriqueue::spsc_queue<std::uint64_t, 4>;

#if defined(VERIQUEUE_FORCE_SPLIT_CONTROL)
inline constexpr bool expected_split_control = true;
#elif defined(VERIQUEUE_FORCE_COLOCATED_CONTROL)
inline constexpr bool expected_split_control = false;
#elif defined(__x86_64__) && !defined(_MSC_VER) && \
    (defined(__GNUC__) || defined(__clang__))
inline constexpr bool expected_split_control = true;
#else
inline constexpr bool expected_split_control = false;
#endif

#if defined(VERIQUEUE_FORCE_CACHED_FULL_LIMIT)
inline constexpr bool expected_cached_full_limit = true;
#elif defined(VERIQUEUE_FORCE_CACHED_HEAD_DISTANCE)
inline constexpr bool expected_cached_full_limit = false;
#elif expected_split_control && defined(__x86_64__) && defined(__GNUC__) && \
    !defined(__clang__) && !defined(_MSC_VER)
inline constexpr bool expected_cached_full_limit = true;
#else
inline constexpr bool expected_cached_full_limit = false;
#endif

static_assert(queue_type::testing_split_control() == expected_split_control);
static_assert(queue_type::testing_cached_full_limit() == expected_cached_full_limit);

} // namespace

int main() {
    vqtest::run("selected policy preserves full/refresh semantics", [] {
        queue_type q;
        VQ_CHECK(q.try_push(10));
        VQ_CHECK(q.try_push(11));
        VQ_CHECK(q.try_push(12));
        VQ_CHECK(q.try_push(13));
        VQ_CHECK(!q.try_push(14));

        auto snapshot = q.testing_snapshot();
        VQ_CHECK(snapshot.producer_local_tail == 4);
        VQ_CHECK(snapshot.published_tail == 4);
        VQ_CHECK(snapshot.producer_cached_head == 0);

        std::uint64_t value = 0;
        VQ_CHECK(q.try_pop(value));
        VQ_CHECK(value == 10);
        VQ_CHECK(q.try_push(14));

        snapshot = q.testing_snapshot();
        VQ_CHECK(snapshot.producer_local_tail == 5);
        VQ_CHECK(snapshot.published_tail == 5);
        VQ_CHECK(snapshot.producer_cached_head == 1);

        for (std::uint64_t expected = 11; expected <= 14; ++expected) {
            VQ_CHECK(q.try_pop(value));
            VQ_CHECK(value == expected);
        }
        VQ_CHECK(q.empty());
    });
}