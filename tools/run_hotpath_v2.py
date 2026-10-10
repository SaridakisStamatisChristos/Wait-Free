#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import pathlib
import random
import subprocess
import sys
from typing import Iterable

from topology_matrix import representative_pairs

PRODUCTION = "production"
DEFAULT_VARIANTS = [
    PRODUCTION,
    "single_owner_cursor",
    "single_owner_cached_limit",
    "single_owner_cached_limit_unlikely",
    "guard64",
    "guard128",
    "guard64_skew32",
    "uint32_cursor",
    "split_atomic_owner",
    "split_atomic_cached_limit",
    "split_local_owner",
    "split_local_cached_limit",
    "split_atomic_owner_128",
    "single_owner_u32",
    "single_owner_narrow",
]
VALID_CAPACITIES = (2, 64, 256, 1024, 65536)
VALID_PAYLOADS = (8, 16, 64, 256)


def parse_csv_ints(text: str, allowed: Iterable[int], name: str) -> list[int]:
    allowed_set = set(allowed)
    values = [int(part) for part in text.split(",") if part]
    if not values or any(value not in allowed_set for value in values):
        raise argparse.ArgumentTypeError(f"{name} must be a comma-separated subset of {sorted(allowed_set)}")
    if len(values) != len(set(values)):
        raise argparse.ArgumentTypeError(f"{name} must not contain duplicates")
    return values


def select_pairs(policy: str) -> list[dict[str, object]]:
    pairs = representative_pairs()
    if not pairs:
        raise SystemExit("no CPU pair could be discovered")
    if policy == "default":
        return [pairs[0]]

    by_label = {str(item["label"]): item for item in pairs}
    selected: list[dict[str, object]] = []
    if "smt_siblings" in by_label:
        selected.append(by_label["smt_siblings"])

    for label in ("same_llc", "shared_l2", "same_package", "different_cpus"):
        item = by_label.get(label)
        if item is not None and item not in selected:
            selected.append(item)
            break

    if not selected:
        selected.append(pairs[0])
    elif len(selected) == 1:
        for item in pairs:
            if item not in selected:
                selected.append(item)
                break
    return selected[:2]


def run_one(program: pathlib.Path, variant: str, capacity: int, payload: int,
            transfers: int, mode: str, batch: int, pair: dict[str, object]) -> dict[str, object]:
    env = os.environ.copy()
    env["VERIQUEUE_PRODUCER_CPU"] = str(pair["producer_cpu"])
    env["VERIQUEUE_CONSUMER_CPU"] = str(pair["consumer_cpu"])
    env["VERIQUEUE_TOPOLOGY_LABEL"] = str(pair["label"])
    completed = subprocess.run(
        [str(program), variant, str(capacity), str(payload), str(transfers), mode, str(batch)],
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
        raise SystemExit(f"expected one JSON line from {variant}, got {len(lines)}")
    record = json.loads(lines[0])
    if not record.get("valid", False):
        raise SystemExit(f"invalid hot-path run: {record}")
    if record.get("pinning_requested") and not record.get("affinity_valid", False):
        raise SystemExit(f"requested affinity failed: {record}")
    return record


def main() -> None:
    parser = argparse.ArgumentParser(description="Run deterministic paired VeriQueue hot-path v2 campaigns")
    parser.add_argument("--program", type=pathlib.Path, default=pathlib.Path("build/bench_hotpath_v2"))
    parser.add_argument("--capacities", default="1024")
    parser.add_argument("--payloads", default="8,16")
    parser.add_argument("--mode", choices=("scalar", "bulk"), default="scalar")
    parser.add_argument("--batch", type=int, default=16)
    parser.add_argument("--transfers", type=int, default=200_000)
    parser.add_argument("--warmups", type=int, default=5)
    parser.add_argument("--repetitions", type=int, default=30)
    parser.add_argument("--seed", type=int, default=20261010)
    parser.add_argument("--variants", nargs="+", default=DEFAULT_VARIANTS, choices=DEFAULT_VARIANTS)
    parser.add_argument("--topology-policy", choices=("default", "x64-diagnostic"), default="default")
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    capacities = parse_csv_ints(args.capacities, VALID_CAPACITIES, "capacities")
    payloads = parse_csv_ints(args.payloads, VALID_PAYLOADS, "payloads")
    if args.transfers <= 0 or args.warmups < 0 or args.repetitions <= 0:
        parser.error("transfers/repetitions must be positive and warmups non-negative")
    if args.mode == "bulk" and not 1 <= args.batch <= 64:
        parser.error("--batch must be in [1,64] for bulk mode")
    if args.mode == "scalar":
        args.batch = 1
    if len(set(args.variants)) != len(args.variants):
        parser.error("--variants must not contain duplicates")
    if PRODUCTION not in args.variants:
        parser.error("--variants must include production for paired analysis")

    pairs = select_pairs(args.topology_policy)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    rng = random.Random(args.seed)
    campaign_index = 0

    with args.output.open("w", encoding="utf-8") as output:
        for pair_index, pair in enumerate(pairs):
            for capacity in capacities:
                for payload in payloads:
                    for round_index in range(args.warmups + args.repetitions):
                        warmup = round_index < args.warmups
                        repetition = round_index if warmup else round_index - args.warmups
                        round_seed = rng.getrandbits(64)
                        order = list(args.variants)
                        random.Random(round_seed).shuffle(order)
                        for order_index, variant in enumerate(order):
                            record = run_one(
                                args.program, variant, capacity, payload, args.transfers,
                                args.mode, args.batch, pair,
                            )
                            record.update(
                                {
                                    "campaign_seed": args.seed,
                                    "campaign_index": campaign_index,
                                    "pair_index": pair_index,
                                    "round_seed": round_seed,
                                    "warmup": warmup,
                                    "repetition": repetition,
                                    "order_index": order_index,
                                }
                            )
                            output.write(json.dumps(record, sort_keys=True) + "\n")
                            output.flush()
                            campaign_index += 1

    manifest = {
        "program": str(args.program),
        "capacities": capacities,
        "payloads": payloads,
        "mode": args.mode,
        "batch": args.batch,
        "transfers": args.transfers,
        "warmups": args.warmups,
        "repetitions": args.repetitions,
        "seed": args.seed,
        "variants": args.variants,
        "topology_policy": args.topology_policy,
        "cpu_pairs": pairs,
        "raw_output": str(args.output),
    }
    args.output.with_suffix(".manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
