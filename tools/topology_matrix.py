#!/usr/bin/env python3
"""Select representative CPU pairs for SPSC topology-sensitive benchmarks.

The script uses Linux sysfs when available and falls back to the first two allowed
CPUs. It emits one representative pair for each topology class that exists on the
current host; absence of a class is data, not an error.
"""
from __future__ import annotations

import json
import os
from pathlib import Path


def allowed_cpus() -> list[int]:
    try:
        return sorted(os.sched_getaffinity(0))
    except (AttributeError, OSError):
        n = os.cpu_count() or 1
        return list(range(n))


def read_int(path: Path) -> int | None:
    try:
        return int(path.read_text().strip())
    except (OSError, ValueError):
        return None


def parse_cpu_list(text: str) -> set[int]:
    cpus: set[int] = set()
    for part in text.strip().split(','):
        if not part:
            continue
        if '-' in part:
            lo, hi = part.split('-', 1)
            cpus.update(range(int(lo), int(hi) + 1))
        else:
            cpus.add(int(part))
    return cpus


def cache_groups(cpu: int) -> dict[int, frozenset[int]]:
    root = Path(f'/sys/devices/system/cpu/cpu{cpu}/cache')
    groups: dict[int, frozenset[int]] = {}
    if not root.exists():
        return groups
    for index in root.glob('index*'):
        level = read_int(index / 'level')
        if level is None:
            continue
        try:
            shared = parse_cpu_list((index / 'shared_cpu_list').read_text())
        except OSError:
            continue
        current = groups.get(level, frozenset())
        if len(shared) > len(current):
            groups[level] = frozenset(shared)
    return groups


def cpu_info(cpu: int) -> dict[str, object]:
    topology = Path(f'/sys/devices/system/cpu/cpu{cpu}/topology')
    return {
        'cpu': cpu,
        'package': read_int(topology / 'physical_package_id'),
        'die': read_int(topology / 'die_id'),
        'core': read_int(topology / 'core_id'),
        'caches': cache_groups(cpu),
    }


def shares_level(a: dict[str, object], b_cpu: int, level: int) -> bool:
    caches = a['caches']
    assert isinstance(caches, dict)
    group = caches.get(level)
    return group is not None and b_cpu in group


def classify(a: dict[str, object], b: dict[str, object]) -> str:
    bcpu = int(b['cpu'])
    same_package = a['package'] is not None and a['package'] == b['package']
    same_core = same_package and a['core'] is not None and a['core'] == b['core']
    if same_core:
        return 'smt_siblings'
    if shares_level(a, bcpu, 2):
        return 'shared_l2'
    levels = sorted((a['caches']).keys()) if isinstance(a['caches'], dict) else []
    llc = max((level for level in levels if level >= 3), default=None)
    if llc is not None and shares_level(a, bcpu, llc):
        return 'same_llc'
    if same_package and a['die'] is not None and b['die'] is not None and a['die'] != b['die']:
        return 'cross_die_or_ccd'
    if same_package:
        return 'same_package'
    if a['package'] is not None and b['package'] is not None and a['package'] != b['package']:
        return 'cross_socket_or_numa'
    return 'different_cpus'


def representative_pairs() -> list[dict[str, object]]:
    cpus = allowed_cpus()
    if not cpus:
        return []
    if len(cpus) == 1:
        return [{'label': 'single_cpu_fallback', 'producer_cpu': cpus[0], 'consumer_cpu': cpus[0]}]
    infos = {cpu: cpu_info(cpu) for cpu in cpus}
    chosen: dict[str, tuple[int, int]] = {}
    for i, a_cpu in enumerate(cpus):
        for b_cpu in cpus[i + 1:]:
            label = classify(infos[a_cpu], infos[b_cpu])
            chosen.setdefault(label, (a_cpu, b_cpu))
    priority = [
        'smt_siblings', 'shared_l2', 'same_llc', 'cross_die_or_ccd',
        'same_package', 'cross_socket_or_numa', 'different_cpus'
    ]
    result = []
    for label in priority:
        if label in chosen:
            a, b = chosen[label]
            result.append({'label': label, 'producer_cpu': a, 'consumer_cpu': b})
    if not result:
        result.append({'label': 'default_pair', 'producer_cpu': cpus[0], 'consumer_cpu': cpus[1]})
    return result


if __name__ == '__main__':
    print(json.dumps(representative_pairs(), indent=2))
