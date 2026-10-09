#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import pathlib
import random
import statistics
import sys
from collections import defaultdict
from typing import Any

FAIL_MEDIAN_LT = 0.95
FAIL_CI_UPPER_LT = 0.98
WARN_MEDIAN_LT = 0.98
BOOTSTRAP_SAMPLES = 10_000


def percentile(values: list[float], p: float) -> float:
    ordered = sorted(values)
    if not ordered:
        raise ValueError("percentile requires data")
    if len(ordered) == 1:
        return ordered[0]
    position = p * (len(ordered) - 1)
    lo = math.floor(position)
    hi = math.ceil(position)
    if lo == hi:
        return ordered[lo]
    fraction = position - lo
    return ordered[lo] * (1.0 - fraction) + ordered[hi] * fraction


def bootstrap_median_ci(values: list[float], seed: int) -> tuple[float, float]:
    if not values:
        raise ValueError("bootstrap requires data")
    rng = random.Random(seed)
    n = len(values)
    medians = []
    for _ in range(BOOTSTRAP_SAMPLES):
        sample = [values[rng.randrange(n)] for _ in range(n)]
        medians.append(statistics.median(sample))
    return percentile(medians, 0.025), percentile(medians, 0.975)


def classify(median_ratio: float, ci_high: float) -> str:
    if median_ratio < FAIL_MEDIAN_LT and ci_high < FAIL_CI_UPPER_LT:
        return "FAIL"
    if median_ratio < WARN_MEDIAN_LT:
        return "WARN"
    return "PASS"


def analyze_records(records: list[dict[str, Any]], bootstrap_seed: int) -> list[dict[str, Any]]:
    cells: dict[tuple[int, int, str, int, int], dict[int, dict[str, float]]] = defaultdict(
        lambda: defaultdict(dict)
    )

    for record in records:
        if record.get("warmup"):
            continue
        if not record.get("valid", False):
            raise ValueError(f"correctness-invalid measured record: {record}")
        role = str(record.get("regression_role", ""))
        if role not in {"candidate", "reference"}:
            raise ValueError(f"invalid regression role: {role!r}")
        key = (
            int(record["capacity"]),
            int(record["payload_bytes"]),
            str(record.get("topology", "unknown")),
            int(record.get("producer_cpu", -1)),
            int(record.get("consumer_cpu", -1)),
        )
        repetition = int(record["repetition"])
        if role in cells[key][repetition]:
            raise ValueError(f"duplicate {role} result for cell={key} repetition={repetition}")
        rate = float(record["transfers_per_second"])
        if rate <= 0.0:
            raise ValueError(f"non-positive rate for cell={key} repetition={repetition}")
        cells[key][repetition][role] = rate

    results: list[dict[str, Any]] = []
    for cell_index, (key, repetitions) in enumerate(sorted(cells.items())):
        capacity, payload_bytes, topology, producer_cpu, consumer_cpu = key
        ratios: list[float] = []
        for repetition, pair in sorted(repetitions.items()):
            if set(pair) != {"candidate", "reference"}:
                raise ValueError(f"unpaired repetition for cell={key} repetition={repetition}: {sorted(pair)}")
            ratios.append(pair["candidate"] / pair["reference"])

        ci_low, ci_high = bootstrap_median_ci(
            ratios,
            bootstrap_seed ^ (cell_index * 0x9E3779B1),
        )
        median_ratio = statistics.median(ratios)
        mean = statistics.mean(ratios)
        cv = statistics.pstdev(ratios) / mean if mean else 0.0
        results.append(
            {
                "capacity": capacity,
                "payload_bytes": payload_bytes,
                "topology": topology,
                "producer_cpu": producer_cpu,
                "consumer_cpu": consumer_cpu,
                "n": len(ratios),
                "median_ratio": median_ratio,
                "iqr_low": percentile(ratios, 0.25),
                "iqr_high": percentile(ratios, 0.75),
                "p05": percentile(ratios, 0.05),
                "p95": percentile(ratios, 0.95),
                "cv": cv,
                "ci95_low": ci_low,
                "ci95_high": ci_high,
                "classification": classify(median_ratio, ci_high),
            }
        )
    if not results:
        raise ValueError("no measured regression cells found")
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description="Analyze paired candidate/reference performance regression campaign")
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("--json-output", type=pathlib.Path, required=True)
    parser.add_argument("--markdown-output", type=pathlib.Path, required=True)
    parser.add_argument("--bootstrap-seed", type=int, default=20261009)
    args = parser.parse_args()

    try:
        records = [
            json.loads(line)
            for line in args.input.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        results = analyze_records(records, args.bootstrap_seed)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"regression analysis failed: {exc}", file=sys.stderr)
        return 2

    payload = {
        "method": "paired_candidate_reference_throughput_ratio",
        "ratio": "candidate/reference",
        "bootstrap_samples": BOOTSTRAP_SAMPLES,
        "bootstrap_seed": args.bootstrap_seed,
        "policy": {
            "fail_median_lt": FAIL_MEDIAN_LT,
            "fail_ci95_upper_lt": FAIL_CI_UPPER_LT,
            "warn_median_lt": WARN_MEDIAN_LT,
        },
        "cells": results,
    }
    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    lines = [
        "# Paired performance regression report",
        "",
        "Ratio is `candidate throughput / reference throughput` within the same runner and repetition.",
        "Hard FAIL is predeclared as median < 0.95 AND bootstrap 95% CI upper bound < 0.98. "
        "WARN is median < 0.98 when the hard-fail rule is not met.",
        "",
        "| Capacity | Payload | Topology | n | Median | IQR | p05 | p95 | CV | 95% CI | Verdict |",
        "|---:|---:|---|---:|---:|---|---:|---:|---:|---|---|",
    ]
    for item in results:
        lines.append(
            f"| {item['capacity']} | {item['payload_bytes']} | {item['topology']} | {item['n']} | "
            f"{item['median_ratio']:.4f} | [{item['iqr_low']:.4f}, {item['iqr_high']:.4f}] | "
            f"{item['p05']:.4f} | {item['p95']:.4f} | {item['cv']:.4f} | "
            f"[{item['ci95_low']:.4f}, {item['ci95_high']:.4f}] | {item['classification']} |"
        )
    args.markdown_output.write_text("\n".join(lines) + "\n", encoding="utf-8")

    warnings = [item for item in results if item["classification"] == "WARN"]
    failures = [item for item in results if item["classification"] == "FAIL"]
    for item in warnings:
        print(
            f"::warning::performance warning capacity={item['capacity']} payload={item['payload_bytes']} "
            f"median={item['median_ratio']:.4f} ci95=[{item['ci95_low']:.4f},{item['ci95_high']:.4f}]"
        )
    for item in failures:
        print(
            f"::error::performance regression capacity={item['capacity']} payload={item['payload_bytes']} "
            f"median={item['median_ratio']:.4f} ci95=[{item['ci95_low']:.4f},{item['ci95_high']:.4f}]"
        )
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
