#include "experimental_spsc_v2.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#ifndef VQ_PAYLOAD_BYTES
#define VQ_PAYLOAD_BYTES 8
#endif

static_assert(VQ_PAYLOAD_BYTES == 8 || VQ_PAYLOAD_BYTES == 16 ||
              VQ_PAYLOAD_BYTES == 64 || VQ_PAYLOAD_BYTES == 256);

struct probe_payload final {
    std::array<std::uint64_t, VQ_PAYLOAD_BYTES / sizeof(std::uint64_t)> words{};
};

using production_queue = veriqueue::spsc_queue<probe_payload, 1024>;
using single_owner_queue = vqbench::experimental::single_owner_cursor_queue<probe_payload, 1024>;
using cached_queue = vqbench::experimental::v2::cached_limit<probe_payload, 1024>;
using unlikely_queue = vqbench::experimental::v2::cached_limit_unlikely<probe_payload, 1024>;
using guard64_queue = vqbench::experimental::v2::guard64<probe_payload, 1024>;
using guard128_queue = vqbench::experimental::v2::guard128<probe_payload, 1024>;
using skew_queue = vqbench::experimental::v2::guard64_skew32<probe_payload, 1024>;
using u32_queue = vqbench::experimental::v2::uint32_cursor<probe_payload, 1024>;

#define DEFINE_PROBE(prefix, queue_type)                                                       \
    extern "C" bool prefix##_push(queue_type& q, const probe_payload& value) {                 \
        return q.try_push(value);                                                               \
    }                                                                                           \
    extern "C" bool prefix##_pop(queue_type& q, probe_payload& value) {                        \
        return q.try_pop(value);                                                                \
    }

DEFINE_PROBE(vq_production, production_queue)
DEFINE_PROBE(vq_single_owner, single_owner_queue)
DEFINE_PROBE(vq_cached_limit, cached_queue)
DEFINE_PROBE(vq_cached_limit_unlikely, unlikely_queue)
DEFINE_PROBE(vq_guard64, guard64_queue)
DEFINE_PROBE(vq_guard128, guard128_queue)
DEFINE_PROBE(vq_guard64_skew32, skew_queue)
DEFINE_PROBE(vq_uint32_cursor, u32_queue)

#undef DEFINE_PROBE
