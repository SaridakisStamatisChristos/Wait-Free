#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import pathlib
import random
import subprocess
import sys
from typing import Any


def parse_int_list(value: str) -> list[int]:
    result = [int(part) for part in value.split(",") if part]
    if not result:
        raise ValueError("integer list cannot be empty")
    return result


def run_one(program: pathlib.Path, capacity: int, payload: int, transfers: int) -> dict[str, Any]:
    command = [str(program), "veriqueue", str(capacity), str(payload), str(transfers)]
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.stderr:
        sys.stderr.write(completed.stderr)
    if completed.returncode != 0:
        sys.stderr.write(completed.stdout)
        raise RuntimeError(f"benchmark failed with exit code {completed.returncode}: {' '.join(command)}")

    lines = [line for line in completed.stdout.splitlines() if line.strip()]
    if len(lines) != 1:
        raise RuntimeError(f"expected exactly one JSON result from {program}, got {len(lines)} lines")
    record = json.loads(lines[0])
    if not record.get("valid", False):
        raise RuntimeError(f"correctness-invalid benchmark result from {program}: {record}")
    if float(record.get("transfers_per_second", 0.0)) <= 0.0:
        raise RuntimeError(f"non-positive benchmark rate from {program}: {record}")
    if int(record.get("capacity", -1)) != capacity or int(record.get("payload_bytes", -1)) != payload:
        raise RuntimeError(f"benchmark result cell mismatch from {program}: {record}")
    if int(record.get("transfers", -1)) != transfers:
        raise RuntimeError(f"benchmark transfer-count mismatch from {program}: {record}")
    return record


def assert_pair_context(candidate: dict[str, Any], reference: dict[str, Any]) -> None:
    keys = ("capacity", "payload_bytes", "topology", "producer_cpu", "consumer_cpu", "transfers")
    mismatches = {key: (candidate.get(key), reference.get(key)) for key in keys if candidate.get(key) != reference.get(key)}
    if mismatches:
        raise RuntimeError(f"candidate/reference context mismatch: {mismatches}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run paired candidate/reference VeriQueue regression campaign")
    parser.add_argument("--candidate", type=pathlib.Path, required=True)
    parser.add_argument("--reference", type=pathlib.Path, required=True)
    parser.add_argument("--capacities", default="64,1024")
    parser.add_argument("--payloads", default="8,64")
    parser.add_argument("--transfers", type=int, default=100_000)
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--repetitions", type=int, default=15)
    parser.add_argument("--seed", type=int, default=20261009)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    if args.transfers <= 0 or args.warmups < 0 or args.repetitions <= 0:
        parser.error("transfers/repetitions must be positive and warmups non-negative")

    try:
        capacities = parse_int_list(args.capacities)
        payloads = parse_int_list(args.payloads)
    except ValueError as exc:
        parser.error(str(exc))

    for program in (args.candidate, args.reference):
        if not program.is_file():
            parser.error(f"benchmark executable does not exist: {program}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    master_rng = random.Random(args.seed)
    campaign_index = 0

    try:
        with args.output.open("w", encoding="utf-8") as output:
            for capacity in capacities:
                for payload in payloads:
                    total_rounds = args.warmups + args.repetitions
                    for round_index in range(total_rounds):
                        warmup = round_index < args.warmups
                        repetition = round_index if warmup else round_index - args.warmups
                        pair_seed = master_rng.getrandbits(64)
                        order = ["candidate", "reference"]
                        random.Random(pair_seed).shuffle(order)
                        pair_records: dict[str, dict[str, Any]] = {}

                        for order_index, role in enumerate(order):
                            program = args.candidate if role == "candidate" else args.reference
                            record = run_one(program, capacity, payload, args.transfers)
                            record.update(
                                {
                                    "regression_role": role,
                                    "regression_seed": args.seed,
                                    "pair_seed": pair_seed,
                                    "warmup": warmup,
                                    "repetition": repetition,
                                    "order_index": order_index,
                                    "campaign_index": campaign_index,
                                }
                            )
                            pair_records[role] = record
                            campaign_index += 1

                        assert_pair_context(pair_records["candidate"], pair_records["reference"])
                        for role in order:
                            output.write(json.dumps(pair_records[role], sort_keys=True) + "\n")
                            output.flush()
    except (OSError, RuntimeError, json.JSONDecodeError) as exc:
        print(f"regression campaign failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
