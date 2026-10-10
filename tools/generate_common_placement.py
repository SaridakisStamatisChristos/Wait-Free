#!/usr/bin/env python3
"""Generate the common-code placement lab comparator without changing production benchmark source."""
from pathlib import Path
import sys

source = '#include "rigtorp_codegen_variants.hpp"\n' + Path('bench/bench_compare.cpp').read_text()
source = source.replace('    double rate{0.0};',
    '    bool alignment_observed{false};\n    std::array<std::size_t, 6> buffer_mods{};\n    std::array<std::uint64_t, 7> placement{};\n    double rate{0.0};')
# Preserve the original positional initializer for the seven run_pair results.
source = source.replace('    return {\n        valid ?', '    return {\n        false, {}, {},\n        valid ?')
needle = '    if (implementation == "rigtorp") {'
insert = r'''    if (implementation == "raw_dynslot") {
        using queue = vqbench::experimental::rigtorp_codegen::dynamic_managed_queue<Payload, Capacity>;
        auto q = std::make_unique<queue>();
        const auto mods = q->buffer_offsets();
        const auto placement = q->placement_offsets();
        auto result = run_pair<sizeof(Payload)>(
            [&](const Payload& value) { return q->try_push(value); },
            [&](Payload& out) { return q->try_pop(out); },
            transfers, cpus.producer, cpus.consumer);
        result.alignment_observed = true;
        result.buffer_mods = mods;
        result.placement = placement;
        return result;
    }
    if (implementation == "heap_common" || implementation == "align_common") {
        using queue = vqbench::experimental::rigtorp_codegen::dynamic_common_queue<Payload, Capacity>;
        using allocator = vqbench::experimental::rigtorp_codegen::runtime_buffer_allocator<Payload>;
        const bool aligned = implementation == "align_common";
        auto q = std::make_unique<queue>(allocator{aligned});
        const auto mods = q->buffer_offsets();
        const auto placement = q->placement_offsets();
        auto result = run_pair<sizeof(Payload)>(
            [&](const Payload& value) { return q->try_push(value); },
            [&](Payload& out) { return q->try_pop(out); },
            transfers, cpus.producer, cpus.consumer);
        result.alignment_observed = true;
        result.buffer_mods = mods;
        result.placement = placement;
        return result;
    }
'''
assert source.count(needle) == 1
source = source.replace(needle, insert + needle)
needle = '              << ",\\"boost_version\\":" << BOOST_VERSION'
insert = '\n'.join([
    '              << ",\\"buffer_alignment_observed\\":" << (result.alignment_observed ? "true" : "false")',
    '              << ",\\"buffer_mods_64_128_256\\":["',
    '              << result.buffer_mods[0] << "," << result.buffer_mods[1] << "," << result.buffer_mods[2]',
    '              << "," << result.buffer_mods[3] << "," << result.buffer_mods[4] << "," << result.buffer_mods[5]',
    '              << "]"',
    '              << ",\\"placement_mod4096_distance\\":["',
    '              << result.placement[0] << "," << result.placement[1] << "," << result.placement[2]',
    '              << "," << result.placement[3] << "," << result.placement[4] << "," << result.placement[5]',
    '              << "," << result.placement[6] << "]"',
])
assert source.count(needle) == 1
source = source.replace(needle, insert + '\n' + needle)
original = Path('bench/bench_compare.cpp').read_text()
a = '    std::atomic<bool> start{false};'
b = '    const auto end = std::chrono::steady_clock::now();'
assert source[source.index(a):source.index(b) + len(b)] == original[original.index(a):original.index(b) + len(b)]
output = Path(sys.argv[1])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(source)
