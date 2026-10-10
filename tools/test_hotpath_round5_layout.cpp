#include "experimental_spsc_round5.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

template <std::size_t Bytes>
struct payload final {
    std::array<std::uint64_t, Bytes / sizeof(std::uint64_t)> words{};
};

using prod8 = veriqueue::spsc_queue<payload<8>, 1024>;
using prod16 = veriqueue::spsc_queue<payload<16>, 1024>;
using prod64 = veriqueue::spsc_queue<payload<64>, 1024>;
using prod256 = veriqueue::spsc_queue<payload<256>, 1024>;
using cand8 = vqbench::experimental::round5::architecture_selective<payload<8>, 1024>;
using cand16 = vqbench::experimental::round5::architecture_selective<payload<16>, 1024>;
using cand64 = vqbench::experimental::round5::architecture_selective<payload<64>, 1024>;
using cand256 = vqbench::experimental::round5::architecture_selective<payload<256>, 1024>;

static_assert(sizeof(prod8) == sizeof(cand8));
static_assert(sizeof(prod16) == sizeof(cand16));
static_assert(sizeof(prod64) == sizeof(cand64));
static_assert(sizeof(prod256) == sizeof(cand256));
static_assert(alignof(prod8) == alignof(cand8));
static_assert(alignof(prod16) == alignof(cand16));
static_assert(alignof(prod64) == alignof(cand64));
static_assert(alignof(prod256) == alignof(cand256));

#if defined(__aarch64__) || defined(_M_ARM64)
static_assert(vqbench::experimental::round5::architecture_selective_stripe<payload<8>> == 1);
static_assert(vqbench::experimental::round5::architecture_selective_stripe<payload<16>> == 8);
static_assert(vqbench::experimental::round5::architecture_selective_stripe<payload<64>> == 1);
static_assert(vqbench::experimental::round5::architecture_selective_stripe<payload<256>> == 1);
#else
static_assert(vqbench::experimental::round5::architecture_selective_stripe<payload<8>> == 1);
static_assert(vqbench::experimental::round5::architecture_selective_stripe<payload<16>> == 1);
static_assert(vqbench::experimental::round5::architecture_selective_stripe<payload<64>> == 1);
static_assert(vqbench::experimental::round5::architecture_selective_stripe<payload<256>> == 1);
#endif

} // namespace

int main() { return 0; }
