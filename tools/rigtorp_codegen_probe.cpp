#include "rigtorp_codegen_variants.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <rigtorp/SPSCQueue.h>

#include <array>
#include <cstddef>
#include <cstdint>

template <std::size_t Bytes>
struct probe_payload final {
    std::array<std::uint64_t, Bytes / 8> words{};
};

#if defined(_MSC_VER)
#define VQ_NOINLINE __declspec(noinline)
#else
#define VQ_NOINLINE __attribute__((noinline))
#endif

#define VQ_PROBE(NAME, CAP, BYTES, TYPE) \
    using NAME##_c##CAP##_p##BYTES = TYPE<probe_payload<BYTES>, CAP>; \
    extern "C" VQ_NOINLINE bool NAME##_push_c##CAP##_p##BYTES( \
        NAME##_c##CAP##_p##BYTES& q, const probe_payload<BYTES>& value) { \
        return q.try_push(value); \
    } \
    extern "C" VQ_NOINLINE bool NAME##_pop_c##CAP##_p##BYTES( \
        NAME##_c##CAP##_p##BYTES& q, probe_payload<BYTES>& value) { \
        return q.try_pop(value); \
    }

#define VQ_UPSTREAM(CAP, BYTES) \
    extern "C" VQ_NOINLINE bool upstream_push_c##CAP##_p##BYTES( \
        rigtorp::SPSCQueue<probe_payload<BYTES>>& q, const probe_payload<BYTES>& value) { \
        return q.try_push(value); \
    } \
    extern "C" VQ_NOINLINE bool upstream_pop_c##CAP##_p##BYTES( \
        rigtorp::SPSCQueue<probe_payload<BYTES>>& q, probe_payload<BYTES>& value) { \
        auto* source = q.front(); \
        if (source == nullptr) return false; \
        value = *source; \
        q.pop(); \
        return true; \
    }

#define VQ_CASE(CAP, BYTES) \
    VQ_PROBE(production, CAP, BYTES, veriqueue::spsc_queue) \
    VQ_PROBE(dynamic_raw, CAP, BYTES, vqbench::experimental::rigtorp_codegen::dynamic_raw_queue) \
    VQ_PROBE(dynamic_ctrl64, CAP, BYTES, vqbench::experimental::rigtorp_codegen::dynamic_ctrl64_queue) \
    VQ_PROBE(dynamic_combined, CAP, BYTES, vqbench::experimental::rigtorp_codegen::dynamic_combined_queue) \
    VQ_PROBE(dynamic_grouped, CAP, BYTES, vqbench::experimental::rigtorp_codegen::dynamic_grouped_queue) \
    VQ_PROBE(dynamic_split, CAP, BYTES, vqbench::experimental::rigtorp_codegen::dynamic_split_queue) \
    VQ_PROBE(static_raw, CAP, BYTES, vqbench::experimental::rigtorp_codegen::static_raw_queue) \
    VQ_PROBE(static_slot, CAP, BYTES, vqbench::experimental::rigtorp_codegen::static_slot_queue) \
    VQ_UPSTREAM(CAP, BYTES)

VQ_CASE(2, 8)
VQ_CASE(2, 16)
VQ_CASE(2, 64)
VQ_CASE(2, 256)
VQ_CASE(64, 8)
VQ_CASE(64, 16)
VQ_CASE(64, 64)
VQ_CASE(64, 256)
VQ_CASE(256, 8)
VQ_CASE(256, 16)
VQ_CASE(256, 64)
VQ_CASE(256, 256)
VQ_CASE(1024, 8)
VQ_CASE(1024, 16)
VQ_CASE(1024, 64)
VQ_CASE(1024, 256)
VQ_CASE(65536, 8)
VQ_CASE(65536, 16)
VQ_CASE(65536, 64)
VQ_CASE(65536, 256)

#undef VQ_CASE
#undef VQ_UPSTREAM
#undef VQ_PROBE
#undef VQ_NOINLINE

int main() { return 0; }

