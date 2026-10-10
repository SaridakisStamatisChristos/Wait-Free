#include "rigtorp_codegen_variants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>

template <std::size_t Bytes>
struct layout_payload {
    std::uint64_t sequence;
    std::array<std::byte, Bytes - 8> bytes;
};
template <>
struct layout_payload<8> { std::uint64_t sequence; };

template <std::size_t Capacity, std::size_t Bytes>
void emit_layout() {
    using namespace vqbench::experimental::rigtorp_codegen;
    using heap = dynamic_managed_queue<layout_payload<Bytes>, Capacity>;
    using inlined = inline_managed_queue<layout_payload<Bytes>, Capacity>;
    static_assert(sizeof(layout_payload<Bytes>) == Bytes);
    static_assert(std::is_standard_layout_v<heap> && std::is_standard_layout_v<inlined>);
    constexpr auto h = heap::layout_offsets();
    constexpr auto i = inlined::layout_offsets();
    static_assert(h[0] == 0 && h[1] == 8 && h[2] == 256 && h[3] == 264 && h[4] == 512 && h[5] == 520);
    static_assert(sizeof(heap) == 768 && alignof(heap) == 256);
    static_assert(i[6] == 768 && alignof(inlined) == 256);
    for (std::size_t n = 0; n < 6; ++n) {
        if (h[n] != i[n]) throw std::runtime_error("control offset changed");
    }
    constexpr auto padding = ((256 - 1) / Bytes) + 1;
    constexpr auto first_live = i[6] + padding * Bytes;
    static_assert(first_live >= i[5] + 256);
    std::cout << "{\"capacity\":" << Capacity << ",\"payload_bytes\":" << Bytes
              << ",\"heap_size\":" << sizeof(heap) << ",\"inline_size\":" << sizeof(inlined)
              << ",\"alignment\":" << alignof(inlined) << ",\"control_offsets\":[";
    for (std::size_t n = 0; n < 6; ++n) std::cout << (n == 0 ? "" : ",") << i[n];
    std::cout << "],\"inline_array_offset\":" << i[6]
              << ",\"first_payload_offset\":" << first_live << "}\n";
}

template <std::size_t Capacity>
void emit_capacity() {
    emit_layout<Capacity, 8>();
    emit_layout<Capacity, 16>();
    emit_layout<Capacity, 64>();
    emit_layout<Capacity, 256>();
}

int main() {
    emit_capacity<2>();
    emit_capacity<64>();
    emit_capacity<256>();
    emit_capacity<1024>();
    emit_capacity<65536>();
}
