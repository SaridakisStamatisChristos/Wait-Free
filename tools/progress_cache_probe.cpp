#include "veriqueue/spsc_queue.hpp"
#include "pr30_queue_control.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

template <std::size_t Bytes>
struct payload final {
    std::uint64_t sequence;
    std::array<std::byte, Bytes - 8> data;
};

template <>
struct payload<8> final { std::uint64_t sequence; };

extern "C" bool candidate_push_c2_p8(veriqueue::spsc_queue<payload<8>, 2>* q, const payload<8>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c2_p8(veriqueue::spsc_queue<payload<8>, 2>* q, payload<8>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c2_p8(vqbench::pr30_control::spsc_queue<payload<8>, 2>* q, const payload<8>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c2_p8(vqbench::pr30_control::spsc_queue<payload<8>, 2>* q, payload<8>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c2_p16(veriqueue::spsc_queue<payload<16>, 2>* q, const payload<16>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c2_p16(veriqueue::spsc_queue<payload<16>, 2>* q, payload<16>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c2_p16(vqbench::pr30_control::spsc_queue<payload<16>, 2>* q, const payload<16>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c2_p16(vqbench::pr30_control::spsc_queue<payload<16>, 2>* q, payload<16>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c2_p64(veriqueue::spsc_queue<payload<64>, 2>* q, const payload<64>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c2_p64(veriqueue::spsc_queue<payload<64>, 2>* q, payload<64>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c2_p64(vqbench::pr30_control::spsc_queue<payload<64>, 2>* q, const payload<64>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c2_p64(vqbench::pr30_control::spsc_queue<payload<64>, 2>* q, payload<64>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c2_p256(veriqueue::spsc_queue<payload<256>, 2>* q, const payload<256>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c2_p256(veriqueue::spsc_queue<payload<256>, 2>* q, payload<256>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c2_p256(vqbench::pr30_control::spsc_queue<payload<256>, 2>* q, const payload<256>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c2_p256(vqbench::pr30_control::spsc_queue<payload<256>, 2>* q, payload<256>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c64_p8(veriqueue::spsc_queue<payload<8>, 64>* q, const payload<8>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c64_p8(veriqueue::spsc_queue<payload<8>, 64>* q, payload<8>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c64_p8(vqbench::pr30_control::spsc_queue<payload<8>, 64>* q, const payload<8>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c64_p8(vqbench::pr30_control::spsc_queue<payload<8>, 64>* q, payload<8>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c64_p16(veriqueue::spsc_queue<payload<16>, 64>* q, const payload<16>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c64_p16(veriqueue::spsc_queue<payload<16>, 64>* q, payload<16>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c64_p16(vqbench::pr30_control::spsc_queue<payload<16>, 64>* q, const payload<16>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c64_p16(vqbench::pr30_control::spsc_queue<payload<16>, 64>* q, payload<16>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c64_p64(veriqueue::spsc_queue<payload<64>, 64>* q, const payload<64>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c64_p64(veriqueue::spsc_queue<payload<64>, 64>* q, payload<64>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c64_p64(vqbench::pr30_control::spsc_queue<payload<64>, 64>* q, const payload<64>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c64_p64(vqbench::pr30_control::spsc_queue<payload<64>, 64>* q, payload<64>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c64_p256(veriqueue::spsc_queue<payload<256>, 64>* q, const payload<256>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c64_p256(veriqueue::spsc_queue<payload<256>, 64>* q, payload<256>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c64_p256(vqbench::pr30_control::spsc_queue<payload<256>, 64>* q, const payload<256>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c64_p256(vqbench::pr30_control::spsc_queue<payload<256>, 64>* q, payload<256>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c256_p8(veriqueue::spsc_queue<payload<8>, 256>* q, const payload<8>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c256_p8(veriqueue::spsc_queue<payload<8>, 256>* q, payload<8>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c256_p8(vqbench::pr30_control::spsc_queue<payload<8>, 256>* q, const payload<8>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c256_p8(vqbench::pr30_control::spsc_queue<payload<8>, 256>* q, payload<8>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c256_p16(veriqueue::spsc_queue<payload<16>, 256>* q, const payload<16>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c256_p16(veriqueue::spsc_queue<payload<16>, 256>* q, payload<16>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c256_p16(vqbench::pr30_control::spsc_queue<payload<16>, 256>* q, const payload<16>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c256_p16(vqbench::pr30_control::spsc_queue<payload<16>, 256>* q, payload<16>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c256_p64(veriqueue::spsc_queue<payload<64>, 256>* q, const payload<64>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c256_p64(veriqueue::spsc_queue<payload<64>, 256>* q, payload<64>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c256_p64(vqbench::pr30_control::spsc_queue<payload<64>, 256>* q, const payload<64>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c256_p64(vqbench::pr30_control::spsc_queue<payload<64>, 256>* q, payload<64>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c256_p256(veriqueue::spsc_queue<payload<256>, 256>* q, const payload<256>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c256_p256(veriqueue::spsc_queue<payload<256>, 256>* q, payload<256>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c256_p256(vqbench::pr30_control::spsc_queue<payload<256>, 256>* q, const payload<256>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c256_p256(vqbench::pr30_control::spsc_queue<payload<256>, 256>* q, payload<256>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c1024_p8(veriqueue::spsc_queue<payload<8>, 1024>* q, const payload<8>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c1024_p8(veriqueue::spsc_queue<payload<8>, 1024>* q, payload<8>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c1024_p8(vqbench::pr30_control::spsc_queue<payload<8>, 1024>* q, const payload<8>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c1024_p8(vqbench::pr30_control::spsc_queue<payload<8>, 1024>* q, payload<8>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c1024_p16(veriqueue::spsc_queue<payload<16>, 1024>* q, const payload<16>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c1024_p16(veriqueue::spsc_queue<payload<16>, 1024>* q, payload<16>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c1024_p16(vqbench::pr30_control::spsc_queue<payload<16>, 1024>* q, const payload<16>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c1024_p16(vqbench::pr30_control::spsc_queue<payload<16>, 1024>* q, payload<16>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c1024_p64(veriqueue::spsc_queue<payload<64>, 1024>* q, const payload<64>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c1024_p64(veriqueue::spsc_queue<payload<64>, 1024>* q, payload<64>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c1024_p64(vqbench::pr30_control::spsc_queue<payload<64>, 1024>* q, const payload<64>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c1024_p64(vqbench::pr30_control::spsc_queue<payload<64>, 1024>* q, payload<64>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c1024_p256(veriqueue::spsc_queue<payload<256>, 1024>* q, const payload<256>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c1024_p256(veriqueue::spsc_queue<payload<256>, 1024>* q, payload<256>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c1024_p256(vqbench::pr30_control::spsc_queue<payload<256>, 1024>* q, const payload<256>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c1024_p256(vqbench::pr30_control::spsc_queue<payload<256>, 1024>* q, payload<256>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c65536_p8(veriqueue::spsc_queue<payload<8>, 65536>* q, const payload<8>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c65536_p8(veriqueue::spsc_queue<payload<8>, 65536>* q, payload<8>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c65536_p8(vqbench::pr30_control::spsc_queue<payload<8>, 65536>* q, const payload<8>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c65536_p8(vqbench::pr30_control::spsc_queue<payload<8>, 65536>* q, payload<8>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c65536_p16(veriqueue::spsc_queue<payload<16>, 65536>* q, const payload<16>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c65536_p16(veriqueue::spsc_queue<payload<16>, 65536>* q, payload<16>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c65536_p16(vqbench::pr30_control::spsc_queue<payload<16>, 65536>* q, const payload<16>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c65536_p16(vqbench::pr30_control::spsc_queue<payload<16>, 65536>* q, payload<16>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c65536_p64(veriqueue::spsc_queue<payload<64>, 65536>* q, const payload<64>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c65536_p64(veriqueue::spsc_queue<payload<64>, 65536>* q, payload<64>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c65536_p64(vqbench::pr30_control::spsc_queue<payload<64>, 65536>* q, const payload<64>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c65536_p64(vqbench::pr30_control::spsc_queue<payload<64>, 65536>* q, payload<64>* out) {
    return q->try_pop(*out);
}
extern "C" bool candidate_push_c65536_p256(veriqueue::spsc_queue<payload<256>, 65536>* q, const payload<256>* value) {
    return q->try_push(*value);
}
extern "C" bool candidate_pop_c65536_p256(veriqueue::spsc_queue<payload<256>, 65536>* q, payload<256>* out) {
    return q->try_pop(*out);
}
extern "C" bool control_push_c65536_p256(vqbench::pr30_control::spsc_queue<payload<256>, 65536>* q, const payload<256>* value) {
    return q->try_push(*value);
}
extern "C" bool control_pop_c65536_p256(vqbench::pr30_control::spsc_queue<payload<256>, 65536>* q, payload<256>* out) {
    return q->try_pop(*out);
}
