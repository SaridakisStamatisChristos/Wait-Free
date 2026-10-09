#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import statistics
from collections import defaultdict
from typing import Any


def percentile(values: list[float], p: float) -> float:
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = p * (len(ordered) - 1)
    lo = math.floor(position)
    hi = math.ceil(position)
    if lo == hi:
        return ordered[lo]
    fraction = position - lo
    return ordered[lo] * (1.0 - fraction) + ordered[hi] * fraction


def group_key(record: dict[str, Any], metric: str) -> tuple[tuple[str, str], ...]:
    dimensions = ['benchmark', 'implementation', 'pattern', 'burst', 'payload', 'payload_bytes',
                  'capacity', 'topology', 'producer_cpu', 'consumer_cpu']
    pairs = [('metric', metric)]
    for dimension in dimensions:
        if dimension in record:
            pairs.append((dimension, str(record[dimension])))
    return tuple(pairs)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('input')
    args = parser.parse_args()
    groups: dict[tuple[tuple[str, str], ...], list[float]] = defaultdict(list)

    with open(args.input, encoding='utf-8') as handle:
        for line in handle:
            record = json.loads(line)
            metric = None
            if 'transfers_per_second' in record:
                metric = 'transfers_per_second'
            elif 'median_ns' in record:
                metric = 'median_ns'
            if metric is not None:
                groups[group_key(record, metric)].append(float(record[metric]))

    print('| Experiment | n | median | IQR | p5 | p95 | CV |')
    print('|---|---:|---:|---:|---:|---:|---:|')
    for key, values in sorted(groups.items()):
        mean = statistics.mean(values)
        stdev = statistics.pstdev(values)
        label = ', '.join(f'{k}={v}' for k, v in key)
        print(
            f'| {label} | {len(values)} | {statistics.median(values):.3f} | '
            f'[{percentile(values, 0.25):.3f}, {percentile(values, 0.75):.3f}] | '
            f'{percentile(values, 0.05):.3f} | {percentile(values, 0.95):.3f} | '
            f'{(stdev / mean if mean else 0.0):.4f} |'
        )


if __name__ == '__main__':
    main()
