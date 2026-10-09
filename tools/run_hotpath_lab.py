#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import pathlib
import random
import subprocess
import sys

VARIANTS = ["veriqueue", "cached_limit", "split_control", "split_cached_limit"]


def main() -> None:
    parser = argparse.ArgumentParser(description="Run paired hot-path experimental variants")
    parser.add_argument("--program", type=pathlib.Path, default=pathlib.Path("build/bench/bench_hotpath_variants"))
    parser.add_argument("--capacity", type=int, default=1024)
    parser.add_argument("--transfers", type=int, default=500_000)
    parser.add_argument("--warmups", type=int, default=3)
    parser.add_argument("--repetitions", type=int, default=20)
    parser.add_argument("--seed", type=int, default=20261010)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    if args.transfers <= 0 or args.warmups < 0 or args.repetitions <= 0:
        parser.error("transfers/repetitions must be positive and warmups non-negative")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    rng = random.Random(args.seed)
    campaign_index = 0

    with args.output.open("w", encoding="utf-8") as output:
        for round_index in range(args.warmups + args.repetitions):
            warmup = round_index < args.warmups
            repetition = round_index if warmup else round_index - args.warmups
            round_seed = rng.getrandbits(64)
            order = list(VARIANTS)
            random.Random(round_seed).shuffle(order)
            for order_index, variant in enumerate(order):
                completed = subprocess.run(
                    [str(args.program), variant, str(args.capacity), str(args.transfers)],
                    text=True,
                    capture_output=True,
                )
                if completed.stderr:
                    sys.stderr.write(completed.stderr)
                if completed.returncode != 0:
                    sys.stderr.write(completed.stdout)
                    raise SystemExit(completed.returncode)
                lines = [line for line in completed.stdout.splitlines() if line.strip()]
                if len(lines) != 1:
                    raise SystemExit(f"expected one JSON line from {variant}, got {len(lines)}")
                record = json.loads(lines[0])
                if not record.get("valid", False):
                    raise SystemExit(f"invalid hot-path run: {record}")
                record.update(
                    {
                        "campaign_seed": args.seed,
                        "campaign_index": campaign_index,
                        "round_seed": round_seed,
                        "warmup": warmup,
                        "repetition": repetition,
                        "order_index": order_index,
                    }
                )
                output.write(json.dumps(record, sort_keys=True) + "\n")
                output.flush()
                campaign_index += 1


if __name__ == "__main__":
    main()
