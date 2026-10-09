#include "experimental_spsc_round3.hpp"
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
using state128_queue = vqbench::experimental::round3::state128<probe_payload, 1024>;
using state256_queue = vqbench::experimental::round3::state256<probe_payload, 1024>;
using consumer_first64_queue =
    vqbench::experimental::round3::consumer_first64<probe_payload, 1024>;
using consumer_first128_queue =
    vqbench::experimental::round3::consumer_first128<probe_payload, 1024>;
using precompute64_queue =
    vqbench::experimental::round3::precompute64<probe_payload, 1024>;
using precompute128_queue =
    vqbench::experimental::round3::precompute128<probe_payload, 1024>;
using consumer_first_precompute128_queue =
    vqbench::experimental::round3::consumer_first_precompute128<probe_payload, 1024>;

#define DEFINE_PROBE(prefix, queue_type)                                      \
    extern "C" bool prefix##_push(queue_type& q, const probe_payload& value) { \
        return q.try_push(value);                                              \
    }                                                                          \
    extern "C" bool prefix##_pop(queue_type& q, probe_payload& value) {       \
        return q.try_pop(value);                                               \
    }

DEFINE_PROBE(vq_r3_production, production_queue)
DEFINE_PROBE(vq_r3_state128, state128_queue)
DEFINE_PROBE(vq_r3_state256, state256_queue)
DEFINE_PROBE(vq_r3_consumer_first64, consumer_first64_queue)
DEFINE_PROBE(vq_r3_consumer_first128, consumer_first128_queue)
DEFINE_PROBE(vq_r3_precompute64, precompute64_queue)
DEFINE_PROBE(vq_r3_precompute128, precompute128_queue)
DEFINE_PROBE(vq_r3_consumer_first_precompute128, consumer_first_precompute128_queue)

#undef DEFINE_PROBE
