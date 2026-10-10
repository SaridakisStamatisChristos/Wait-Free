#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import random
import subprocess
import sys
from typing import Any

DEFAULT_IMPLEMENTATIONS = (
    "veriqueue",
    "rigtorp",
    "boost_lockfree",
    "moodycamel",
    "drogalis",
)


def parse_csv(value: str) -> list[str]:
    items = [item.strip() for item in value.split(",") if item.strip()]
    if not items:
        raise ValueError("list cannot be empty")
    return items


def parse_int_csv(value: str) -> list[int]:
    return [int(item) for item in parse_csv(value)]


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run_one(
    program: pathlib.Path,
    implementation: str,
    capacity: int,
    payload: int,
    transfers: int,
) -> dict[str, Any]:
    command = [
        str(program),
        implementation,
        str(capacity),
        str(payload),
        str(transfers),
    ]
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.stderr:
        sys.stderr.write(completed.stderr)
    if completed.returncode != 0:
        sys.stderr.write(completed.stdout)
        raise RuntimeError(
            f"benchmark failed with exit code {completed.returncode}: {' '.join(command)}"
        )

    lines = [line for line in completed.stdout.splitlines() if line.strip()]
    if len(lines) != 1:
        raise RuntimeError(
            f"expected exactly one JSON result from {program}, got {len(lines)} lines"
        )
    record = json.loads(lines[0])
    if record.get("benchmark") != "baseline_compare_v2":
        raise RuntimeError(f"unexpected benchmark identity: {record}")
    if record.get("implementation") != implementation:
        raise RuntimeError(f"implementation mismatch: expected={implementation} record={record}")
    if not record.get("valid", False) or not record.get("affinity_valid", False):
        raise RuntimeError(f"correctness/affinity-invalid result: {record}")
    if int(record.get("capacity", -1)) != capacity:
        raise RuntimeError(f"capacity mismatch: expected={capacity} record={record}")
    if int(record.get("payload_bytes", -1)) != payload:
        raise RuntimeError(f"payload mismatch: expected={payload} record={record}")
    if int(record.get("transfers", -1)) != transfers:
        raise RuntimeError(f"transfer-count mismatch: expected={transfers} record={record}")
    if float(record.get("transfers_per_second", 0.0)) <= 0.0:
        raise RuntimeError(f"non-positive throughput: {record}")
    return record


def assert_round_context(records: dict[str, dict[str, Any]]) -> None:
    if not records:
        raise RuntimeError("empty comparison round")
    keys = (
        "capacity",
        "payload_bytes",
        "topology",
        "producer_cpu",
        "consumer_cpu",
        "transfers",
    )
    first_name = next(iter(records))
    reference = records[first_name]
    for implementation, record in records.items():
        mismatches = {
            key: (reference.get(key), record.get(key))
            for key in keys
            if reference.get(key) != record.get(key)
        }
        if mismatches:
            raise RuntimeError(
                f"round context mismatch reference={first_name} implementation={implementation}: "
                f"{mismatches}"
            )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run randomized paired multi-implementation SPSC comparison campaign"
    )
    parser.add_argument("--program", type=pathlib.Path, required=True)
    parser.add_argument("--capacity", type=int, required=True)
    parser.add_argument("--payloads", default="8,16,64,256")
    parser.add_argument(
        "--implementations", default=",".join(DEFAULT_IMPLEMENTATIONS)
    )
    parser.add_argument("--transfers", type=int, default=1_000_000)
    parser.add_argument("--warmups", type=int, default=5)
    parser.add_argument("--repetitions", type=int, default=50)
    parser.add_argument("--seed", type=int, default=20261010)
    parser.add_argument("--lane", required=True)
    parser.add_argument("--architecture", required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    if not args.program.is_file():
        parser.error(f"benchmark executable does not exist: {args.program}")
    if args.capacity <= 0 or args.transfers <= 0 or args.repetitions <= 0:
        parser.error("capacity/transfers/repetitions must be positive")
    if args.warmups < 0:
        parser.error("warmups must be non-negative")

    try:
        payloads = parse_int_csv(args.payloads)
        implementations = parse_csv(args.implementations)
    except ValueError as exc:
        parser.error(str(exc))

    if len(set(implementations)) != len(implementations):
        parser.error("implementations must be unique")
    if "veriqueue" not in implementations:
        parser.error("implementations must include veriqueue")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    master_rng = random.Random(args.seed)
    campaign_index = 0
    meta = {
        "record_type": "meta",
        "schema": "veriqueue_cross_algorithm_campaign_v1",
        "lane": args.lane,
        "architecture": args.architecture,
        "compiler": args.compiler,
        "source_commit": args.source_commit,
        "program_sha256": sha256_file(args.program),
        "capacity": args.capacity,
        "payloads": payloads,
        "implementations": implementations,
        "transfers": args.transfers,
        "warmups": args.warmups,
        "repetitions": args.repetitions,
        "seed": args.seed,
        "ordering": "randomized_per_round",
    }

    try:
        with args.output.open("w", encoding="utf-8") as output:
            output.write(json.dumps(meta, sort_keys=True) + "\n")
            output.flush()
            for payload in payloads:
                total_rounds = args.warmups + args.repetitions
                for round_index in range(total_rounds):
                    warmup = round_index < args.warmups
                    repetition = round_index if warmup else round_index - args.warmups
                    round_seed = master_rng.getrandbits(64)
                    order = list(implementations)
                    random.Random(round_seed).shuffle(order)
                    round_records: dict[str, dict[str, Any]] = {}

                    for order_index, implementation in enumerate(order):
                        record = run_one(
                            args.program,
                            implementation,
                            args.capacity,
                            payload,
                            args.transfers,
                        )
                        record.update(
                            {
                                "comparison_schema": "veriqueue_cross_algorithm_campaign_v1",
                                "comparison_lane": args.lane,
                                "comparison_architecture": args.architecture,
                                "comparison_compiler": args.compiler,
                                "comparison_source_commit": args.source_commit,
                                "campaign_seed": args.seed,
                                "round_seed": round_seed,
                                "warmup": warmup,
                                "repetition": repetition,
                                "order_index": order_index,
                                "campaign_index": campaign_index,
                            }
                        )
                        round_records[implementation] = record
                        campaign_index += 1

                    assert_round_context(round_records)
                    for implementation in order:
                        output.write(
                            json.dumps(round_records[implementation], sort_keys=True) + "\n"
                        )
                    output.flush()
    except (OSError, RuntimeError, json.JSONDecodeError) as exc:
        print(f"comparator campaign failed: {exc}", file=sys.stderr)
        return 1

    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
