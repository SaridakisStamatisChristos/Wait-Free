#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import pathlib
import random
import subprocess
import sys

IMPLEMENTATIONS = [
    "veriqueue",
    "rigtorp",
    "boost_lockfree",
    "moodycamel",
    "drogalis",
]


def parse_int_list(value: str) -> list[int]:
    return [int(part) for part in value.split(",") if part]


def main() -> None:
    parser = argparse.ArgumentParser(description="Run paired same-runner SPSC comparisons")
    parser.add_argument("--program", type=pathlib.Path, default=pathlib.Path("build/bench/bench_compare"))
    parser.add_argument("--capacities", default="64,1024")
    parser.add_argument("--payloads", default="8,64")
    parser.add_argument("--transfers", type=int, default=100_000)
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--repetitions", type=int, default=10)
    parser.add_argument("--seed", type=int, default=20261009)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    if args.transfers <= 0 or args.warmups < 0 or args.repetitions <= 0:
        parser.error("transfers/repetitions must be positive and warmups non-negative")

    capacities = parse_int_list(args.capacities)
    payloads = parse_int_list(args.payloads)
    args.output.parent.mkdir(parents=True, exist_ok=True)

    master_rng = random.Random(args.seed)
    campaign_index = 0

    with args.output.open("w", encoding="utf-8") as output:
        for capacity in capacities:
            for payload in payloads:
                total_rounds = args.warmups + args.repetitions
                for round_index in range(total_rounds):
                    warmup = round_index < args.warmups
                    repetition = round_index - args.warmups if not warmup else round_index
                    order = list(IMPLEMENTATIONS)
                    round_seed = master_rng.getrandbits(64)
                    random.Random(round_seed).shuffle(order)

                    for order_index, implementation in enumerate(order):
                        command = [
                            str(args.program),
                            implementation,
                            str(capacity),
                            str(payload),
                            str(args.transfers),
                        ]
                        completed = subprocess.run(command, text=True, capture_output=True)
                        if completed.stderr:
                            sys.stderr.write(completed.stderr)
                        if completed.returncode != 0:
                            sys.stderr.write(
                                f"benchmark failed: implementation={implementation} "
                                f"capacity={capacity} payload={payload} "
                                f"returncode={completed.returncode}\n"
                            )
                            sys.stderr.write(completed.stdout)
                            raise SystemExit(completed.returncode)

                        lines = [line for line in completed.stdout.splitlines() if line.strip()]
                        if len(lines) != 1:
                            raise SystemExit(
                                f"expected one JSON result from {implementation}, got {len(lines)} lines"
                            )
                        record = json.loads(lines[0])
                        if not record.get("valid", False):
                            raise SystemExit(f"invalid comparative run: {record}")

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
