#!/usr/bin/env python3
"""Generate the alignment lab comparator without changing production benchmark source."""
from pathlib import Path
import sys

source = '#include "rigtorp_codegen_variants.hpp"\n' + Path('bench/bench_compare.cpp').read_text()
source = source.replace('    double rate{0.0};',
    '    bool alignment_observed{false};\n    std::array<std::size_t, 6> buffer_mods{};\n    double rate{0.0};')
# Preserve the original positional initializer for the seven run_pair results.
source = source.replace('    return {\n        valid ?', '    return {\n        false, {},\n        valid ?')
needle = '    if (implementation == "rigtorp") {'
insert = ''
for label, alias in [('raw_dynslot', 'dynamic_managed_queue'),
                     ('aligned_dynslot', 'dynamic_aligned_queue'),
                     ('inline_dynslot', 'inline_managed_queue')]:
    insert += f'''    if (implementation == "{label}") {{
        using queue = vqbench::experimental::rigtorp_codegen::{alias}<Payload, Capacity>;
        auto q = std::make_unique<queue>();
        const auto mods = q->buffer_offsets();
        auto result = run_pair<sizeof(Payload)>(
            [&](const Payload& value) {{ return q->try_push(value); }},
            [&](Payload& out) {{ return q->try_pop(out); }},
            transfers, cpus.producer, cpus.consumer);
        result.alignment_observed = true;
        result.buffer_mods = mods;
        return result;
    }}
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
])
assert source.count(needle) == 1
source = source.replace(needle, insert + '\n' + needle)
output = Path(sys.argv[1])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(source)
