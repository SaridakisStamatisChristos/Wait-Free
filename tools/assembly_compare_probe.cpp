#include "experimental_spsc_variants.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <cstdint>

using production_queue = veriqueue::spsc_queue<std::uint64_t, 1024>;
using variant_queue = vqbench::experimental::single_owner_cursor_queue<std::uint64_t, 1024>;

extern "C" bool veriqueue_production_push(production_queue& q, std::uint64_t value) {
    return q.try_push(value);
}

extern "C" bool veriqueue_production_pop(production_queue& q, std::uint64_t& value) {
    return q.try_pop(value);
}

extern "C" bool veriqueue_single_owner_push(variant_queue& q, std::uint64_t value) {
    return q.try_push(value);
}

extern "C" bool veriqueue_single_owner_pop(variant_queue& q, std::uint64_t& value) {
    return q.try_pop(value);
}
