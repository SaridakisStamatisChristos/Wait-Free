#!/usr/bin/env python3
"""Prespecified common-code aligned / ordinary heap ARM40 diagnostic and integrity gate."""
from __future__ import annotations

import argparse
import json
import math
import pathlib
import random
import statistics
from collections import Counter, defaultdict

from analyze_comparator_campaign import (
    bootstrap_geomean_ci, bootstrap_median_ci, cell_key, classify,
    geometric_mean, load_records,
)

IMPLEMENTATIONS = ('veriqueue', 'raw_dynslot', 'heap_common', 'align_common',
                   'rigtorp', 'boost_lockfree', 'moodycamel', 'drogalis')
PINS = {'rigtorp_commit': '59a6a938513ea5004817383711ed35d32385d3ee',
        'moodycamel_commit': '6867b56452352acf077fccd5f6cc7e3a8cfde0fb',
        'drogalis_commit': 'c959bd4f7204dd73c4e75a2b00c3459c3a5e9a96',
        'boost_version': 108300}
EXPECTED_SHARDS = {(c, n) for c in ('gcc', 'clang') for n in (2, 64, 256, 1024, 65536)}


def campaign_seed(compiler, capacity):
    return 2026101021 + (1 if compiler == 'gcc' else 2) * 10000 + capacity


def validate(metadata, records, source):
    expected = set(IMPLEMENTATIONS)
    if len(metadata) != 10 or len(records) != 17600:
        raise ValueError('Require ten shards and 17,600 records including warmups')
    metas = {}
    for m in metadata:
        k = m['compiler'], m['capacity']
        if k not in EXPECTED_SHARDS or k in metas:
            raise ValueError('Wrong or duplicate shard')
        if (m['schema'] != 'veriqueue_cross_algorithm_campaign_v1' or
                m['source_commit'] != source or m['architecture'] != 'arm64' or
                m['lane'] != f'arm64-{k[0]}' or m['payloads'] != [8, 16, 64, 256] or
                m['implementations'] != list(IMPLEMENTATIONS) or
                m['transfers'] != 1000000 or m['warmups'] != 5 or m['repetitions'] != 50 or
                m['seed'] != campaign_seed(*k) or m['ordering'] != 'randomized_per_round' or
                len(m['program_sha256']) != 64 or
                any(c not in '0123456789abcdef' for c in m['program_sha256'])):
            raise ValueError('Wrong shard protocol/provenance')
        metas[k] = m
    if set(metas) != EXPECTED_SHARDS:
        raise ValueError('Incomplete matrix')
    rounds = defaultdict(dict)
    pairs = defaultdict(dict)
    checksum = (1000000 * 999999 // 2 * 0x9e3779b185ebca87) % (1 << 64)
    for r in records:
        if (r.get('comparison_schema') != 'veriqueue_cross_algorithm_campaign_v1' or
                r.get('benchmark') != 'baseline_compare_v2' or
                r.get('comparison_source_commit') != source or
                any(r.get(k) != 1000000 for k in ('produced', 'consumed', 'transfers')) or
                any(r.get(k) != v for k, v in PINS.items()) or
                r.get('checksum') != checksum or
                any(r.get(k) is not True for k in
                    ('valid', 'affinity_valid', 'pinning_requested', 'producer_pinned', 'consumer_pinned'))):
            raise ValueError('Invalid record/pins/checksum/source')
        internal = r.get('implementation') in ('raw_dynslot', 'heap_common', 'align_common')
        mods = r.get('buffer_mods_64_128_256')
        if (r.get('buffer_alignment_observed') is not internal or not isinstance(mods, list) or
                len(mods) != 6 or any(type(v) is not int or v < 0 or v >= d
                                      for v, d in zip(mods, (64, 128, 256, 64, 128, 256)))):
            raise ValueError('Missing/invalid outside-timing alignment observation')
        if mods != [mods[2] % 64, mods[2] % 128, mods[2],
                    mods[5] % 64, mods[5] % 128, mods[5]]:
            raise ValueError('Inconsistent alignment observation')
        if internal and (mods[:3] != mods[3:] or any(v % 8 for v in mods)):
            raise ValueError('Wrong payload buffer alignment/padding')
        if r.get('implementation') == 'align_common' and mods != [0] * 6:
            raise ValueError('Guaranteed 256-byte buffer alignment failed')
        if not internal and mods != [0] * 6:
            raise ValueError('Unobserved alignment must have zero placeholder')
        place = r.get('placement_mod4096_distance')
        if (not isinstance(place, list) or len(place) != 7 or
                any(type(v) is not int or v < 0 for v in place) or
                any(v >= 4096 for v in place[:5]) or place[6] not in (0, 1)):
            raise ValueError('Invalid placement observation')
        if internal:
            owner, write, read, base, first, distance, after = place
            expected_base = (owner + (distance if after else -distance)) % 4096
            if (owner % 256 or write != (owner + 256) % 4096 or read != (owner + 512) % 4096 or
                    base != expected_base or first != (base + 256) % 4096 or
                    base % 256 != mods[2] or first % 256 != mods[5] or
                    distance < (768 if after else
                                (r['capacity'] + 1 + 2 * ((255 // r['payload_bytes']) + 1)) * r['payload_bytes'])):
                raise ValueError('Inconsistent owner/control/buffer geometry')
        elif place != [0] * 7:
            raise ValueError('Unobserved placement must have zero placeholders')
        key = cell_key(r)
        shard = key[2], key[3]
        if (shard not in metas or key[1] != 'arm64' or key[0] != f'arm64-{key[2]}' or
                key[4] not in (8, 16, 64, 256) or key[6] < 0 or key[7] < 0 or key[6] == key[7] or
                r['campaign_seed'] != campaign_seed(*shard) or type(r['warmup']) is not bool):
            raise ValueError('Wrong cell context')
        rep, impl = r['repetition'], r['implementation']
        if impl not in expected or rep not in range(5 if r['warmup'] else 50):
            raise ValueError('Wrong implementation/repetition')
        rk = key, r['warmup'], rep
        if impl in rounds[rk]:
            raise ValueError('Duplicate record')
        rate = float(r['transfers_per_second'])
        if not math.isfinite(rate) or rate <= 0:
            raise ValueError('Invalid throughput')
        rounds[rk][impl] = r
    # Verify the declared fresh random schedule, order indices and chronology.
    for shard in sorted(EXPECTED_SHARDS):
        rng = random.Random(campaign_seed(*shard))
        for pi, payload in enumerate((8, 16, 64, 256)):
            for ri in range(55):
                warmup, rep = ri < 5, ri if ri < 5 else ri - 5
                matches = [v for (k, w, n), v in rounds.items()
                           if (k[2], k[3]) == shard and k[4] == payload and w == warmup and n == rep]
                if len(matches) != 1 or set(matches[0]) != expected:
                    raise ValueError('Missing/fragmented complete eight-way round')
                group = matches[0]
                seed = rng.getrandbits(64)
                order = list(IMPLEMENTATIONS)
                random.Random(seed).shuffle(order)
                for oi, impl in enumerate(order):
                    r = group[impl]
                    if (r['round_seed'] != seed or r['order_index'] != oi or
                            r['campaign_index'] != (pi * 55 + ri) * 8 + oi):
                        raise ValueError('Randomized schedule/order mismatch')
                if not warmup:
                    key = cell_key(group[order[0]])
                    pairs[key][rep] = {i: float(r['transfers_per_second']) for i, r in group.items()}
    if len(pairs) != 40 or len(rounds) != 2200:
        raise ValueError('Wrong cell/round count')
    return pairs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    parser.add_argument('--source-commit', required=True)
    args = parser.parse_args()
    metadata, records = load_records(sorted(args.input.glob('*.jsonl')))
    pairs = validate(metadata, records, args.source_commit)
    def comparison(candidate, control, seed):
        rows = []
        for index, (key, rounds) in enumerate(sorted(pairs.items())):
            ratios = [r[candidate] / r[control] for _, r in sorted(rounds.items())]
            low, high = bootstrap_median_ci(ratios, seed ^ (index * 0x9E3779B1), 20000)
            rows.append(dict(lane=key[0], capacity=key[3], payload_bytes=key[4], n=50,
                             ratio=statistics.median(ratios), low=low, high=high,
                             classification=classify(low, high)))
        summaries = []
        for index, lane in enumerate(['all', 'arm64-clang', 'arm64-gcc']):
            selected = [r for r in rows if lane == 'all' or r['lane'] == lane]
            values = [r['ratio'] for r in selected]
            low, high = bootstrap_geomean_ci(values, seed ^ index, 20000)
            summaries.append(dict(lane=lane, cells=len(selected), ratio=geometric_mean(values),
                                  low=low, high=high, classification=classify(low, high),
                                  counts=dict(Counter(r['classification'] for r in selected))))
        return dict(candidate=candidate, control=control, bootstrap_seed=seed,
                    summaries=summaries, cells=rows)

    primary = comparison('align_common', 'heap_common', 2026101022)
    calibration = comparison('heap_common', 'raw_dynslot', 2026101022 ^ 0x517CC1B7)
    observations = []
    for compiler in ('gcc', 'clang'):
        for impl in ('raw_dynslot', 'heap_common', 'align_common'):
            chosen = [r for r in records if r['comparison_compiler'] == compiler
                      and r['implementation'] == impl and not r['warmup']]
            histogram = Counter(tuple(r['buffer_mods_64_128_256']) for r in chosen)
            placements = Counter(tuple(r['placement_mod4096_distance']) for r in chosen)
            observations.append(dict(compiler=compiler, implementation=impl, measured_records=len(chosen),
                                     histogram=[dict(mods=list(k), count=v) for k, v in sorted(histogram.items())],
                                     placements=[dict(geometry=list(k), count=v) for k, v in sorted(placements.items())]))
    result = dict(alignment_observations=observations, source_commit=args.source_commit,
                  candidate='align_common', control='heap_common',
                  bootstrap_samples=20000, primary=primary, calibration=calibration,
                  integrity=dict(shards=10, cells=40, records=17600, warmups=5, repetitions=50,
                                 complete_rounds=2200, transfers=1000000, pins=PINS,
                                 random_schedule_verified=True, checksums_verified=True, buffer_alignment_verified=True, placement_geometry_verified=True),
                  interpretation='Diagnostic only; common code address must be independently verified. TIE is not equivalence.')
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(primary=primary['summaries'], calibration=calibration['summaries']), indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
