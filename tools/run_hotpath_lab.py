#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import pathlib
import random
import subprocess
import sys

DEFAULT_VARIANTS = ["veriqueue", "single_owner_cursor"]
ALL_VARIANTS = [
    "veriqueue",
    "single_owner_cursor",
    "cached_limit",
    "split_control",
    "split_cached_limit",
]


def main() -> None:
    parser = argparse.ArgumentParser(description="Run paired hot-path experimental variants")
    parser.add_argument("--program", type=pathlib.Path, default=pathlib.Path("build/bench/bench_hotpath_variants"))
    parser.add_argument("--capacity", type=int, default=1024)
    parser.add_argument("--payload", type=int, default=8, choices=(8, 16, 64, 256))
    parser.add_argument("--transfers", type=int, default=500_000)
    parser.add_argument("--warmups", type=int, default=5)
    parser.add_argument("--repetitions", type=int, default=30)
    parser.add_argument("--seed", type=int, default=20261010)
    parser.add_argument("--variants", nargs="+", default=DEFAULT_VARIANTS, choices=ALL_VARIANTS)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    if args.transfers <= 0 or args.warmups < 0 or args.repetitions <= 0:
        parser.error("transfers/repetitions must be positive and warmups non-negative")
    if len(set(args.variants)) != len(args.variants):
        parser.error("--variants must not contain duplicates")
    if "veriqueue" not in args.variants:
        parser.error("--variants must include veriqueue for paired analysis")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    rng = random.Random(args.seed)
    campaign_index = 0

    with args.output.open("w", encoding="utf-8") as output:
        for round_index in range(args.warmups + args.repetitions):
            warmup = round_index < args.warmups
            repetition = round_index if warmup else round_index - args.warmups
            round_seed = rng.getrandbits(64)
            order = list(args.variants)
            random.Random(round_seed).shuffle(order)
            for order_index, variant in enumerate(order):
                completed = subprocess.run(
                    [
                        str(args.program),
                        variant,
                        str(args.capacity),
                        str(args.payload),
                        str(args.transfers),
                    ],
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
                if record.get("pinning_requested") and not record.get("affinity_valid", False):
                    raise SystemExit(f"requested affinity failed: {record}")
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
