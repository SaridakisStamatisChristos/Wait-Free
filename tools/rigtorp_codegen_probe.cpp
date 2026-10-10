#include "rigtorp_codegen_variants.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <rigtorp/SPSCQueue.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

struct payload16 final {
    std::array<std::uint64_t, 2> words{};
};

#if defined(_MSC_VER)
#define VQ_NOINLINE __declspec(noinline)
#else
#define VQ_NOINLINE __attribute__((noinline))
#endif

using production = veriqueue::spsc_queue<payload16, 1024>;
using dynamic_raw =
    vqbench::experimental::rigtorp_codegen::dynamic_raw_queue<payload16, 1024>;
using static_raw =
    vqbench::experimental::rigtorp_codegen::static_raw_queue<payload16, 1024>;
using static_slot =
    vqbench::experimental::rigtorp_codegen::static_slot_queue<payload16, 1024>;
using upstream = rigtorp::SPSCQueue<payload16>;

extern "C" VQ_NOINLINE bool production_push(
    production& q,
    const payload16& value) {
    return q.try_push(value);
}

extern "C" VQ_NOINLINE bool production_pop(
    production& q,
    payload16& value) {
    return q.try_pop(value);
}

extern "C" VQ_NOINLINE bool dynamic_raw_push(
    dynamic_raw& q,
    const payload16& value) {
    return q.try_push(value);
}

extern "C" VQ_NOINLINE bool dynamic_raw_pop(
    dynamic_raw& q,
    payload16& value) {
    return q.try_pop(value);
}

extern "C" VQ_NOINLINE bool static_raw_push(
    static_raw& q,
    const payload16& value) {
    return q.try_push(value);
}

extern "C" VQ_NOINLINE bool static_raw_pop(
    static_raw& q,
    payload16& value) {
    return q.try_pop(value);
}

extern "C" VQ_NOINLINE bool static_slot_push(
    static_slot& q,
    const payload16& value) {
    return q.try_push(value);
}

extern "C" VQ_NOINLINE bool static_slot_pop(
    static_slot& q,
    payload16& value) {
    return q.try_pop(value);
}

extern "C" VQ_NOINLINE bool upstream_push(
    upstream& q,
    const payload16& value) {
    return q.try_push(value);
}

extern "C" VQ_NOINLINE bool upstream_pop(
    upstream& q,
    payload16& value) {
    auto* source = q.front();
    if (source == nullptr) return false;
    value = *source;
    q.pop();
    return true;
}

#undef VQ_NOINLINE

} // namespace

int main() { return 0; }
