#!/usr/bin/env python3
"""Synthetic integrity falsification only; these rates are never benchmark evidence."""
import copy
import random

from analyze_comparator_campaign import classify
from analyze_storage_location import IMPLEMENTATIONS, PINS, campaign_seed, validate


def fixture():
    metadata, records = [], []
    checksum = (1000000 * 999999 // 2 * 0x9e3779b185ebca87) % (1 << 64)
    for compiler in ('gcc', 'clang'):
        for capacity in (2, 64, 256, 1024, 65536):
            seed = campaign_seed(compiler, capacity)
            metadata.append(dict(schema='veriqueue_cross_algorithm_campaign_v1', compiler=compiler,
                                 capacity=capacity, source_commit='fixture', architecture='arm64',
                                 lane=f'arm64-{compiler}', payloads=[8, 16, 64, 256],
                                 implementations=list(IMPLEMENTATIONS), transfers=1000000, warmups=5,
                                 repetitions=50, seed=seed, ordering='randomized_per_round',
                                 program_sha256='0' * 64))
            rng = random.Random(seed)
            for pi, payload in enumerate((8, 16, 64, 256)):
                for ri in range(55):
                    round_seed = rng.getrandbits(64)
                    order = list(IMPLEMENTATIONS)
                    random.Random(round_seed).shuffle(order)
                    for oi, impl in enumerate(order):
                        records.append(dict(comparison_schema='veriqueue_cross_algorithm_campaign_v1',
                                            benchmark='baseline_compare_v2', comparison_source_commit='fixture',
                                            comparison_lane=f'arm64-{compiler}', comparison_architecture='arm64',
                                            comparison_compiler=compiler, capacity=capacity, payload_bytes=payload,
                                            topology='synthetic', producer_cpu=0, consumer_cpu=1,
                                            produced=1000000, consumed=1000000, transfers=1000000,
                                            checksum=checksum, valid=True, affinity_valid=True,
                                            pinning_requested=True, producer_pinned=True, consumer_pinned=True,
                                            campaign_seed=seed, warmup=ri < 5, repetition=ri if ri < 5 else ri - 5,
                                            implementation=impl, transfers_per_second=1.0,
                                            round_seed=round_seed, order_index=oi,
                                            campaign_index=(pi * 55 + ri) * 8 + oi, **PINS))
    return metadata, records


def reject(metadata, records):
    try:
        validate(metadata, records, 'fixture')
    except ValueError:
        return
    raise AssertionError('corrupted evidence accepted')


def main():
    metadata, records = fixture()
    pairs = validate(metadata, records, 'fixture')
    assert len(pairs) == 40 and all(len(r) == 50 for r in pairs.values())
    reject(metadata, records[:-1])
    reject(metadata, records[1:] + [records[1]])
    bad_meta = copy.deepcopy(metadata)
    bad_meta[0]['repetitions'] = 49
    reject(bad_meta, records)
    for field, value in [('comparison_source_commit', 'mixed'), ('checksum', 0), ('boost_version', 1),
                         ('producer_pinned', False), ('transfers', 999999), ('round_seed', 0),
                         ('campaign_seed', 0), ('order_index', 9), ('campaign_index', -1),
                         ('transfers_per_second', float('nan')), ('repetition', 5), ('warmup', False)]:
        changed = [dict(records[0], **{field: value})] + records[1:]
        reject(metadata, changed)
    assert classify(0.9637, 0.9995296) == 'LOSS'
    assert classify(1.0, 1.1) == 'TIE' and classify(0.9, 1.0) == 'TIE'
    print('Storage-location integrity falsification passed; synthetic fixture is not performance evidence.')


if __name__ == '__main__':
    main()
