#!/usr/bin/env python3
"""Separate process-level perf diagnostic. Its rates/counters never enter the ARM40 score."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import random
import shutil
import subprocess

EVENTS = ('cycles:u', 'instructions:u', 'branches:u', 'branch-misses:u',
          'cache-references:u', 'cache-misses:u')
IMPLEMENTATIONS = ('raw_dynslot', 'heap_common', 'align_common')


def parse_counts(text):
    counts = {}
    for line in text.splitlines():
        fields = [x.strip() for x in line.split(';')]
        if len(fields) < 5 or fields[2] not in EVENTS:
            continue
        try:
            value, running = float(fields[0]), float(fields[4].rstrip('%'))
            runtime = float(fields[3])
        except ValueError:
            continue  # Unsupported/not-counted remains missing, never zero.
        if (not all(math.isfinite(x) for x in (value, running, runtime)) or
                value < 0 or running < 90 or running > 100 or runtime <= 0):
            continue
        if fields[2] in counts:
            raise ValueError('duplicate perf event')
        counts[fields[2]] = dict(count=value, percent_running=running, runtime=runtime)
    return counts


def find_perf():
    # Use a real packaged binary rather than a wrapper requiring an exact kernel package.
    candidates = sorted(Path('/usr/lib/linux-tools').glob('*/perf'), reverse=True)
    candidates += sorted(Path('/usr/lib').glob('linux-tools-*/perf'), reverse=True)
    direct = shutil.which('perf')
    if direct:
        candidates.append(Path(direct))
    return next((str(p) for p in candidates if p.is_file() and os.access(p, os.X_OK)), None)


def capability(returncode, text):
    counts = parse_counts(text)
    ready = returncode == 0 and set(counts) == set(EVENTS)
    ready = ready and counts.get('cycles:u', {}).get('count', 0) > 0
    ready = ready and counts.get('instructions:u', {}).get('count', 0) > 0
    return ready, counts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--program', type=Path, required=True)
    parser.add_argument('--capacity', type=int, required=True)
    parser.add_argument('--compiler', choices=('gcc', 'clang'), required=True)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    program = args.program.resolve()
    seed = 2026101023 + (10000 if args.compiler == 'gcc' else 20000) + args.capacity
    meta = dict(schema='veriqueue_placement_perf_v1', source_commit=args.source_commit,
                compiler=args.compiler, capacity=args.capacity, seed=seed, events=list(EVENTS),
                program_sha256=hashlib.sha256(program.read_bytes()).hexdigest(),
                scoring_evidence=False, measurement_boundary='whole process including construction, threads, JSON',
                transfers=1000000, warmups=2, repetitions=10, payloads=[8, 16, 64, 256],
                implementations=list(IMPLEMENTATIONS))
    perf = find_perf()
    meta['perf'] = perf
    if not perf:
        meta.update(status='unavailable', reason='No executable perf installed; no counters inferred.')
        (args.output / 'profile.json').write_text(json.dumps(meta, indent=2) + '\n')
        print(meta['reason'])
        return 0
    env = dict(os.environ, LC_ALL='C')
    def measure(label, payload, transfers, stem):
        csv = args.output / f'{stem}.csv'
        command = [perf, 'stat', '--no-big-num', '-x', ';', '-o', str(csv),
                   '-e', ','.join(EVENTS), '--', str(program), label,
                   str(args.capacity), str(payload), str(transfers)]
        r = subprocess.run(command, text=True, capture_output=True, env=env)
        (args.output / f'{stem}.stdout').write_text(r.stdout)
        (args.output / f'{stem}.stderr').write_text(r.stderr)
        text = csv.read_text() if csv.exists() else ''
        ready, counts = capability(r.returncode, text)
        return r, ready, counts
    probe, ready, counts = measure('heap_common', 16, 10000, 'probe')
    meta.update(probe_exit=probe.returncode, probe_counts=counts)
    settings = Path('/proc/sys/kernel/perf_event_paranoid')
    meta['perf_event_paranoid'] = settings.read_text().strip() if settings.exists() else None
    if not ready:
        meta.update(status='unavailable', reason='Preset PMU events unavailable, restricted or <90% running; '
                    'raw probe retained, no counter zero-fill or scored rerun.')
        (args.output / 'profile.json').write_text(json.dumps(meta, indent=2) + '\n')
        print(meta['reason'])
        return 0
    records = []
    rng = random.Random(seed)
    for payload in (8, 16, 64, 256):
        for ri in range(12):
            order = list(IMPLEMENTATIONS)
            round_seed = rng.getrandbits(64)
            random.Random(round_seed).shuffle(order)
            for oi, label in enumerate(order):
                stem = f'p{payload}-r{ri}-o{oi}-{label}'
                r, supported, counts = measure(label, payload, 1000000, stem)
                benchmark = None
                if r.returncode == 0:
                    try:
                        benchmark = json.loads(r.stdout)
                    except json.JSONDecodeError:
                        pass
                valid = isinstance(benchmark, dict) and benchmark.get('valid') is True
                if valid:
                    valid = (benchmark.get('affinity_valid') is True and
                             all(benchmark.get(k) == 1000000 for k in ('produced', 'consumed', 'transfers')) and
                             benchmark.get('checksum') == (1000000 * 999999 // 2 * 0x9e3779b185ebca87) % (1 << 64))
                records.append(dict(implementation=label, payload_bytes=payload, warmup=ri < 2,
                                    repetition=ri if ri < 2 else ri - 2, round_seed=round_seed, order_index=oi,
                                    counter_valid=supported, benchmark_valid=valid, counts=counts,
                                    benchmark=benchmark, exit_code=r.returncode, raw_prefix=stem))
    meta.update(status='collected' if all(r['counter_valid'] and r['benchmark_valid'] for r in records) else 'partial',
                records=records)
    (args.output / 'profile.json').write_text(json.dumps(meta, indent=2) + '\n')
    print('PMU profile:', meta['status'], len(records), 'unscored records')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
