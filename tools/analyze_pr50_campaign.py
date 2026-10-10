#!/usr/bin/env python3
"""Keep the original external scoreboard and a separate paired PR30 diagnostic."""
from __future__ import annotations

import argparse
import json
import pathlib
import statistics
from collections import Counter, defaultdict

from analyze_comparator_campaign import (
    DEFAULT_IMPLEMENTATIONS, analyze_records, bootstrap_geomean_ci,
    bootstrap_median_ci, cell_key, classify, geometric_mean, load_records,
    markdown_report, write_csv,
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    args = parser.parse_args()
    metadata, records = load_records(sorted(args.input.glob('*.jsonl')))
    expected = set(DEFAULT_IMPLEMENTATIONS) | {'pr30_control'}
    pairs = defaultdict(lambda: defaultdict(dict))
    for record in records:
        if record.get('warmup'):
            continue
        if not record.get('valid') or not record.get('affinity_valid'):
            raise ValueError('Invalid campaign record')
        if record['produced'] != 1000000 or record['consumed'] != 1000000:
            raise ValueError('Wrong transfer count')
        key, rep, impl = cell_key(record), record['repetition'], record['implementation']
        if impl in pairs[key][rep]:
            raise ValueError('Duplicate paired record')
        pairs[key][rep][impl] = float(record['transfers_per_second'])
    if len(metadata) != 20 or len(pairs) != 80:
        raise ValueError('Require the original 20 shards / 80 cells')
    for rounds in pairs.values():
        if set(rounds) != set(range(50)) or any(set(r) != expected for r in rounds.values()):
            raise ValueError('Require 50 complete six-way paired repetitions')
    if len({r['comparison_source_commit'] for r in records}) != 1:
        raise ValueError('Mixed source heads')
    # The frozen PR30 queue is a same-run control, never an external competitor.
    result = analyze_records([r for r in records if r['implementation'] != 'pr30_control'],
                             20261010, 10000)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    (args.output / 'summary.md').write_text(markdown_report(result))
    (args.output / 'headline.md').write_text(markdown_report(result, headline_only=True))
    write_csv(args.output / 'cells.csv', result['pairwise_cells'])
    rows = []
    for index, (key, rounds) in enumerate(sorted(pairs.items())):
        ratios = [r['veriqueue'] / r['pr30_control'] for _, r in sorted(rounds.items())]
        low, high = bootstrap_median_ci(ratios, 20261050 ^ (index * 0x9E3779B1), 20000)
        rows.append(dict(lane=key[0], capacity=key[3], payload_bytes=key[4], n=50,
                         ratio=statistics.median(ratios), low=low, high=high,
                         classification=classify(low, high)))
    summaries = []
    for lane in ['all'] + sorted({r['lane'] for r in rows}):
        selected = [r for r in rows if lane == 'all' or r['lane'] == lane]
        values = [r['ratio'] for r in selected]
        low, high = bootstrap_geomean_ci(values, 20261050, 20000)
        summaries.append(dict(lane=lane, cells=len(selected), ratio=geometric_mean(values),
                              low=low, high=high, counts=dict(Counter(r['classification'] for r in selected))))
    diagnostic = dict(source_commit=result['source_commit'], frozen_control_commit=
                      '60ab1cedb6b79777549888d10fe4a51484f3b159', summaries=summaries, cells=rows)
    (args.output / 'pr30-paired-diagnostic.json').write_text(json.dumps(diagnostic, indent=2) + '\n')
    print(markdown_report(result, headline_only=True))
    print(json.dumps(summaries, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
