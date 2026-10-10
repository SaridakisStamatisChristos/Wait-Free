#include "experimental_policy_variants.hpp"
#include "experimental_spsc_variants.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <boost/lockfree/spsc_queue.hpp>
#include <dro/spsc-queue.hpp>
#include <readerwriterqueue.h>
#include <rigtorp/SPSCQueue.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

template <std::size_t Bytes>
struct payload final {
    std::array<std::uint64_t, Bytes / sizeof(std::uint64_t)> words{};
};

#if defined(_MSC_VER)
#define VQ_NOINLINE __declspec(noinline)
#else
#define VQ_NOINLINE __attribute__((noinline))
#endif

#define DEFINE_SIZE_WRAPPERS(Suffix, Bytes) \
    using payload_##Suffix = payload<Bytes>; \
    using vq_##Suffix = veriqueue::spsc_queue<payload_##Suffix, 1024>; \
    using split_##Suffix = vqbench::experimental::split_control_queue<payload_##Suffix, 1024, false>; \
    using split_cached_##Suffix = vqbench::experimental::split_control_queue<payload_##Suffix, 1024, true>; \
    using raw_seq_##Suffix = vqbench::experimental::policy::raw_cached_seq<payload_##Suffix, 1024>; \
    using raw_tiled_##Suffix = vqbench::experimental::policy::raw_cached_tiled<payload_##Suffix, 1024>; \
    using typed_seq_##Suffix = vqbench::experimental::policy::typed_cached_seq<payload_##Suffix, 1024>; \
    using memcpy_seq_##Suffix = vqbench::experimental::policy::memcpy_cached_seq<payload_##Suffix, 1024>; \
    extern "C" VQ_NOINLINE bool vq_push_##Suffix(vq_##Suffix& q, const payload_##Suffix& v) { return q.try_push(v); } \
    extern "C" VQ_NOINLINE bool vq_pop_##Suffix(vq_##Suffix& q, payload_##Suffix& v) { return q.try_pop(v); } \
    extern "C" VQ_NOINLINE bool split_push_##Suffix(split_##Suffix& q, const payload_##Suffix& v) { return q.try_push(v); } \
    extern "C" VQ_NOINLINE bool split_pop_##Suffix(split_##Suffix& q, payload_##Suffix& v) { return q.try_pop(v); } \
    extern "C" VQ_NOINLINE bool split_cached_push_##Suffix(split_cached_##Suffix& q, const payload_##Suffix& v) { return q.try_push(v); } \
    extern "C" VQ_NOINLINE bool split_cached_pop_##Suffix(split_cached_##Suffix& q, payload_##Suffix& v) { return q.try_pop(v); } \
    extern "C" VQ_NOINLINE bool raw_seq_push_##Suffix(raw_seq_##Suffix& q, const payload_##Suffix& v) { return q.try_push(v); } \
    extern "C" VQ_NOINLINE bool raw_seq_pop_##Suffix(raw_seq_##Suffix& q, payload_##Suffix& v) { return q.try_pop(v); } \
    extern "C" VQ_NOINLINE bool raw_tiled_push_##Suffix(raw_tiled_##Suffix& q, const payload_##Suffix& v) { return q.try_push(v); } \
    extern "C" VQ_NOINLINE bool raw_tiled_pop_##Suffix(raw_tiled_##Suffix& q, payload_##Suffix& v) { return q.try_pop(v); } \
    extern "C" VQ_NOINLINE bool typed_seq_push_##Suffix(typed_seq_##Suffix& q, const payload_##Suffix& v) { return q.try_push(v); } \
    extern "C" VQ_NOINLINE bool typed_seq_pop_##Suffix(typed_seq_##Suffix& q, payload_##Suffix& v) { return q.try_pop(v); } \
    extern "C" VQ_NOINLINE bool memcpy_seq_push_##Suffix(memcpy_seq_##Suffix& q, const payload_##Suffix& v) { return q.try_push(v); } \
    extern "C" VQ_NOINLINE bool memcpy_seq_pop_##Suffix(memcpy_seq_##Suffix& q, payload_##Suffix& v) { return q.try_pop(v); } \
    extern "C" VQ_NOINLINE bool rigtorp_push_##Suffix(rigtorp::SPSCQueue<payload_##Suffix>& q, const payload_##Suffix& v) { return q.try_push(v); } \
    extern "C" VQ_NOINLINE bool rigtorp_pop_##Suffix(rigtorp::SPSCQueue<payload_##Suffix>& q, payload_##Suffix& v) { auto* p = q.front(); if (p == nullptr) return false; v = *p; q.pop(); return true; } \
    extern "C" VQ_NOINLINE bool boost_push_##Suffix(boost::lockfree::spsc_queue<payload_##Suffix>& q, const payload_##Suffix& v) { return q.push(v); } \
    extern "C" VQ_NOINLINE bool boost_pop_##Suffix(boost::lockfree::spsc_queue<payload_##Suffix>& q, payload_##Suffix& v) { return q.pop(v); } \
    extern "C" VQ_NOINLINE bool moody_push_##Suffix(moodycamel::ReaderWriterQueue<payload_##Suffix>& q, const payload_##Suffix& v) { return q.try_enqueue(v); } \
    extern "C" VQ_NOINLINE bool moody_pop_##Suffix(moodycamel::ReaderWriterQueue<payload_##Suffix>& q, payload_##Suffix& v) { return q.try_dequeue(v); } \
    extern "C" VQ_NOINLINE bool drogalis_push_##Suffix(dro::SPSCQueue<payload_##Suffix>& q, const payload_##Suffix& v) { return q.try_push(v); } \
    extern "C" VQ_NOINLINE bool drogalis_pop_##Suffix(dro::SPSCQueue<payload_##Suffix>& q, payload_##Suffix& v) { return q.try_pop(v); }

DEFINE_SIZE_WRAPPERS(p16, 16)
DEFINE_SIZE_WRAPPERS(p64, 64)
DEFINE_SIZE_WRAPPERS(p256, 256)

#undef DEFINE_SIZE_WRAPPERS
#undef VQ_NOINLINE

} // namespace

int main() { return 0; }
