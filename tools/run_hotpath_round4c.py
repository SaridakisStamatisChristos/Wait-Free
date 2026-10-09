#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import pathlib
import subprocess
import sys

from topology_matrix import representative_pairs


def select_pair() -> dict[str, object]:
    pairs = representative_pairs()
    if not pairs:
        raise SystemExit("no CPU pair could be discovered")
    return pairs[0]


def run_block(program: pathlib.Path, payload: int, transfers: int,
              pair: dict[str, object]) -> dict[str, object]:
    env = os.environ.copy()
    env["VERIQUEUE_PRODUCER_CPU"] = str(pair["producer_cpu"])
    env["VERIQUEUE_CONSUMER_CPU"] = str(pair["consumer_cpu"])
    env["VERIQUEUE_TOPOLOGY_LABEL"] = str(pair["label"])
    completed = subprocess.run(
        [str(program), str(payload), str(transfers)],
        text=True,
        capture_output=True,
        env=env,
    )
    if completed.stderr:
        sys.stderr.write(completed.stderr)
    if completed.returncode != 0:
        sys.stderr.write(completed.stdout)
        raise SystemExit(completed.returncode)
    lines = [line for line in completed.stdout.splitlines() if line.strip()]
    if len(lines) != 1:
        raise SystemExit(f"expected one JSON line, got {len(lines)}")
    record = json.loads(lines[0])
    if not record.get("valid", False):
        raise SystemExit(f"invalid ABBA block: {record}")
    return record


def main() -> None:
    parser = argparse.ArgumentParser(description="Run long ABBA paired blocks for round 4C")
    parser.add_argument("--program", type=pathlib.Path,
                        default=pathlib.Path("build/bench_hotpath_round4c"))
    parser.add_argument("--payload", type=int, choices=(8, 16), required=True)
    parser.add_argument("--transfers", type=int, default=5_000_000)
    parser.add_argument("--warmups", type=int, default=3)
    parser.add_argument("--repetitions", type=int, default=30)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    if args.transfers <= 0 or args.warmups < 0 or args.repetitions <= 0:
        parser.error("invalid run counts")

    pair = select_pair()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as output:
        for block_index in range(args.warmups + args.repetitions):
            warmup = block_index < args.warmups
            repetition = block_index if warmup else block_index - args.warmups
            block = run_block(args.program, args.payload, args.transfers, pair)
            common = {
                "benchmark": "hotpath_round4c_abba",
                "mode": "scalar",
                "batch_size": 1,
                "payload_bytes": args.payload,
                "capacity": 1024,
                "producer_cpu": int(pair["producer_cpu"]),
                "consumer_cpu": int(pair["consumer_cpu"]),
                "topology": str(pair["label"]),
                "valid": True,
                "pinning_requested": True,
                "affinity_valid": True,
                "warmup": warmup,
                "repetition": repetition,
                "block_index": block_index,
                "transfers_per_leg": args.transfers,
                "block_paired_ratio": float(block["paired_ratio"]),
                "environment": block["environment"],
            }
            production = dict(common)
            production.update(
                implementation="production",
                transfers_per_second=float(block["production_tps"]),
            )
            candidate = dict(common)
            candidate.update(
                implementation="selective16_stripe8",
                transfers_per_second=float(block["candidate_tps"]),
            )
            output.write(json.dumps(production, sort_keys=True) + "\n")
            output.write(json.dumps(candidate, sort_keys=True) + "\n")
            output.flush()

    args.output.with_suffix(".manifest.json").write_text(
        json.dumps(
            {
                "design": "ABBA: production,candidate,candidate,production within each process",
                "capacity": 1024,
                "payload": args.payload,
                "transfers_per_leg": args.transfers,
                "warmups": args.warmups,
                "repetitions": args.repetitions,
                "cpu_pair": pair,
                "raw_output": str(args.output),
            }, indent=2, sort_keys=True
        ) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
