#!/usr/bin/env python3
"""Prespecified runtime slot representation comparison; production score is separate."""
from __future__ import annotations

import argparse
import json
import math
import pathlib
import statistics
from collections import Counter, defaultdict

from analyze_comparator_campaign import (
    bootstrap_geomean_ci, bootstrap_median_ci, cell_key, classify,
    geometric_mean, load_records,
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    args = parser.parse_args()
    metadata, records = load_records(sorted(args.input.glob('*.jsonl')))
    expected = {'veriqueue', 'dynamic_raw', 'raw_progress', 'raw_dynslot',
                'rigtorp', 'boost_lockfree', 'moodycamel', 'drogalis'}
    pairs = defaultdict(lambda: defaultdict(dict))
    warmups = defaultdict(lambda: defaultdict(set))
    pins = {'rigtorp_commit': '59a6a938513ea5004817383711ed35d32385d3ee',
            'moodycamel_commit': '6867b56452352acf077fccd5f6cc7e3a8cfde0fb',
            'drogalis_commit': 'c959bd4f7204dd73c4e75a2b00c3459c3a5e9a96',
            'boost_version': 108300}
    for record in records:
        if record.get('comparison_schema') != 'veriqueue_cross_algorithm_campaign_v1':
            raise ValueError('Unexpected campaign schema')
        if not record.get('valid') or not record.get('affinity_valid'):
            raise ValueError('Invalid campaign record')
        if any(record.get(k) != 1000000 for k in ('produced', 'consumed', 'transfers')):
            raise ValueError('Wrong transfer count')
        if any(record.get(k) != v for k, v in pins.items()):
            raise ValueError('Unexpected external comparator pin')
        key, rep, impl = cell_key(record), int(record['repetition']), record['implementation']
        if impl not in expected:
            raise ValueError('Unexpected implementation')
        if record.get('warmup'):
            if rep in warmups[key][impl]:
                raise ValueError('Duplicate warmup')
            warmups[key][impl].add(rep)
            continue
        rate = float(record['transfers_per_second'])
        if not math.isfinite(rate) or not rate > 0.0 or impl in pairs[key][rep]:
            raise ValueError('Invalid or duplicate paired rate')
        pairs[key][rep][impl] = rate
    expected_cells = {(f'arm64-{c}',  n, p) for c in ('gcc', 'clang')
                      for n in (2, 64, 256, 1024, 65536) for p in (8, 16, 64, 256)}
    if len(metadata) != 10 or len(pairs) != 40:
        raise ValueError('Require 10 shards / 40 ARM cells')
    if {(k[0], k[3], k[4]) for k in pairs} != expected_cells:
        raise ValueError('Wrong ARM capacity/payload matrix')
    for key, rounds in pairs.items():
        if key[1] != 'arm64' or key[0] != f'arm64-{key[2]}':
            raise ValueError('Architecture/compiler mismatch')
        if set(rounds) != set(range(50)) or any(set(r) != expected for r in rounds.values()):
            raise ValueError('Require 50 complete eight-way paired repetitions')
        if set(warmups[key]) != expected or any(v != set(range(5)) for v in warmups[key].values()):
            raise ValueError('Require five warmups per implementation and cell')
    sources = {r['comparison_source_commit'] for r in records}
    if len(sources) != 1:
        raise ValueError('Mixed source heads')
    rows = []
    for index, (key, rounds) in enumerate(sorted(pairs.items())):
        ratios = [r['raw_dynslot'] / r['raw_progress'] for _, r in sorted(rounds.items())]
        low, high = bootstrap_median_ci(ratios, 20261051 ^ (index * 0x9E3779B1), 20000)
        rows.append(dict(lane=key[0], capacity=key[3], payload_bytes=key[4], n=50,
                         ratio=statistics.median(ratios), low=low, high=high,
                         classification=classify(low, high)))
    summaries = []
    for index, lane in enumerate(['all', 'arm64-clang', 'arm64-gcc']):
        selected = [r for r in rows if lane == 'all' or r['lane'] == lane]
        values = [r['ratio'] for r in selected]
        low, high = bootstrap_geomean_ci(values, 20261051 ^ index, 20000)
        summaries.append(dict(lane=lane, cells=len(selected), ratio=geometric_mean(values),
                              low=low, high=high, counts=dict(Counter(r['classification'] for r in selected))))
    result = dict(source_commit=next(iter(sources)), candidate='raw_dynslot',
                  control='raw_progress', bootstrap_samples=20000,
                  summaries=summaries, cells=rows,
                  interpretation='TIE means unresolved difference, not proven equivalence.')
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(summaries, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
