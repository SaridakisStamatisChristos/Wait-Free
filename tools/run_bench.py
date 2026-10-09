#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import pathlib
import platform
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]


def topology_pairs(mode: str) -> list[dict[str, object]]:
    if mode == 'default':
        return [{'label': 'default', 'producer_cpu': None, 'consumer_cpu': None}]
    cp = subprocess.run(
        [sys.executable, str(ROOT / 'tools' / 'topology_matrix.py')],
        check=True,
        text=True,
        capture_output=True,
    )
    pairs = json.loads(cp.stdout)
    return pairs or [{'label': 'default', 'producer_cpu': None, 'consumer_cpu': None}]


def run_program(exe: pathlib.Path, env: dict[str, str]) -> list[dict[str, object]]:
    cp = subprocess.run([str(exe)], check=True, text=True, capture_output=True, env=env)
    records: list[dict[str, object]] = []
    for line in cp.stdout.splitlines():
        if line.strip():
            records.append(json.loads(line))
    return records


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', default='build/release')
    parser.add_argument('--warmups', type=int, default=5)
    parser.add_argument('--repetitions', type=int, default=20)
    parser.add_argument('--topology', choices=['auto', 'default'], default='auto')
    parser.add_argument('--output', default='evidence/benchmarks/raw.jsonl')
    parser.add_argument('--programs', nargs='+', default=None)
    args = parser.parse_args()

    output = pathlib.Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    if args.programs is None:
        programs = ['bench_throughput', 'bench_latency', 'bench_bursts', 'bench_payload']
        if (pathlib.Path(args.build) / 'bench_compare').exists():
            programs.append('bench_compare')
    else:
        programs = args.programs
    pairs = topology_pairs(args.topology)
    meta = {
        'timestamp_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
        'platform': platform.platform(),
        'python': platform.python_version(),
        'warmups': args.warmups,
        'repetitions': args.repetitions,
        'topology_pairs': pairs,
    }

    with output.open('w') as handle:
        handle.write(json.dumps({'meta': meta}, sort_keys=True) + '\n')
        for pair in pairs:
            env = os.environ.copy()
            env['VERIQUEUE_TOPOLOGY_LABEL'] = str(pair['label'])
            if pair['producer_cpu'] is not None:
                env['VERIQUEUE_PRODUCER_CPU'] = str(pair['producer_cpu'])
            if pair['consumer_cpu'] is not None:
                env['VERIQUEUE_CONSUMER_CPU'] = str(pair['consumer_cpu'])

            for _ in range(args.warmups):
                for program in programs:
                    run_program(pathlib.Path(args.build) / program, env)

            for repetition in range(args.repetitions):
                for program in programs:
                    for record in run_program(pathlib.Path(args.build) / program, env):
                        record['repetition'] = repetition
                        handle.write(json.dumps(record, sort_keys=True) + '\n')
    print(output)


if __name__ == '__main__':
    main()
