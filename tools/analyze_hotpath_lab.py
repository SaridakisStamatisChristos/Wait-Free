#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import pathlib
import random
import statistics
from collections import defaultdict
from typing import Any

PRODUCTION = "veriqueue"
VARIANT = "single_owner_cursor"
LOWER_WIN_THRESHOLD = 1.03
UPPER_LOSS_THRESHOLD = 0.97


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


def bootstrap_median_ci(values: list[float], seed: int, samples: int = 10_000) -> tuple[float, float]:
    rng = random.Random(seed)
    n = len(values)
    medians: list[float] = []
    for _ in range(samples):
        medians.append(statistics.median(values[rng.randrange(n)] for _ in range(n)))
    return percentile(medians, 0.025), percentile(medians, 0.975)


def classify(lower: float, upper: float) -> str:
    if lower > LOWER_WIN_THRESHOLD:
        return "WIN"
    if upper < UPPER_LOSS_THRESHOLD:
        return "LOSS"
    return "TIE/INCONCLUSIVE"


def main() -> None:
    parser = argparse.ArgumentParser(description="Analyze single-owner-cursor paired hot-path results")
    parser.add_argument("input", nargs="+", type=pathlib.Path)
    parser.add_argument("--json-output", type=pathlib.Path, required=True)
    parser.add_argument("--markdown-output", type=pathlib.Path, required=True)
    parser.add_argument("--bootstrap-seed", type=int, default=20261010)
    args = parser.parse_args()

    records: list[dict[str, Any]] = []
    for path in args.input:
        with path.open(encoding="utf-8") as handle:
            for line in handle:
                record = json.loads(line)
                if record.get("warmup"):
                    continue
                if not record.get("valid", False):
                    raise SystemExit(f"invalid measured record: {record}")
                if record.get("pinning_requested") and not record.get("affinity_valid", False):
                    raise SystemExit(f"affinity-invalid measured record: {record}")
                records.append(record)

    cells: dict[tuple[int, int, str, int, int, str, str], dict[int, dict[str, float]]] = defaultdict(
        lambda: defaultdict(dict)
    )
    for record in records:
        environment = record.get("environment") or {}
        key = (
            int(record["capacity"]),
            int(record["payload_bytes"]),
            str(record.get("topology", "unknown")),
            int(record.get("producer_cpu", -1)),
            int(record.get("consumer_cpu", -1)),
            str(environment.get("architecture", "unknown")),
            str(environment.get("compiler", "unknown")),
        )
        repetition = int(record["repetition"])
        implementation = str(record["implementation"])
        if implementation not in (PRODUCTION, VARIANT):
            continue
        if implementation in cells[key][repetition]:
            raise SystemExit(f"duplicate result for {key=} {repetition=} {implementation=}")
        cells[key][repetition][implementation] = float(record["transfers_per_second"])

    comparisons: list[dict[str, Any]] = []
    for cell_index, (key, repetitions) in enumerate(sorted(cells.items())):
        capacity, payload_bytes, topology, producer_cpu, consumer_cpu, architecture, compiler = key
        ratios: list[float] = []
        missing: list[int] = []
        for repetition, pair in sorted(repetitions.items()):
            if PRODUCTION not in pair or VARIANT not in pair:
                missing.append(repetition)
                continue
            if pair[PRODUCTION] <= 0.0:
                raise SystemExit(f"non-positive production throughput in {key=} {repetition=}")
            ratios.append(pair[VARIANT] / pair[PRODUCTION])
        if missing:
            raise SystemExit(f"unpaired repetitions in {key}: {missing}")
        if not ratios:
            continue

        ci_low, ci_high = bootstrap_median_ci(
            ratios, args.bootstrap_seed ^ (cell_index * 0x9E3779B1)
        )
        mean = statistics.mean(ratios)
        comparisons.append(
            {
                "capacity": capacity,
                "payload_bytes": payload_bytes,
                "topology": topology,
                "producer_cpu": producer_cpu,
                "consumer_cpu": consumer_cpu,
                "architecture": architecture,
                "compiler": compiler,
                "n": len(ratios),
                "median_ratio": statistics.median(ratios),
                "iqr_low": percentile(ratios, 0.25),
                "iqr_high": percentile(ratios, 0.75),
                "p05": percentile(ratios, 0.05),
                "p95": percentile(ratios, 0.95),
                "cv": statistics.pstdev(ratios) / mean if mean else 0.0,
                "ci95_low": ci_low,
                "ci95_high": ci_high,
                "classification": classify(ci_low, ci_high),
            }
        )

    payload = {
        "method": "paired_single_owner_cursor_over_production_throughput_ratio",
        "bootstrap_samples": 10_000,
        "bootstrap_seed": args.bootstrap_seed,
        "thresholds": {
            "win_ci_lower_gt": LOWER_WIN_THRESHOLD,
            "loss_ci_upper_lt": UPPER_LOSS_THRESHOLD,
        },
        "comparisons": comparisons,
    }
    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    lines = [
        "# Single-owner-cursor paired benchmark summary",
        "",
        "Ratio is `single_owner_cursor throughput / production VeriQueue throughput` within the same repetition.",
        "WIN requires bootstrap 95% CI lower bound > 1.03; LOSS requires CI upper bound < 0.97; otherwise TIE/INCONCLUSIVE.",
        "Affinity-invalid measurements are rejected before analysis.",
        "",
        "| Arch | Compiler | Capacity | Payload | Topology | n | Median ratio | IQR | p05 | p95 | CV | 95% CI | Verdict |",
        "|---|---|---:|---:|---|---:|---:|---|---:|---:|---:|---|---|",
    ]
    for item in comparisons:
        lines.append(
            f"| {item['architecture']} | {item['compiler']} | {item['capacity']} | "
            f"{item['payload_bytes']} | {item['topology']} | {item['n']} | "
            f"{item['median_ratio']:.4f} | [{item['iqr_low']:.4f}, {item['iqr_high']:.4f}] | "
            f"{item['p05']:.4f} | {item['p95']:.4f} | {item['cv']:.4f} | "
            f"[{item['ci95_low']:.4f}, {item['ci95_high']:.4f}] | {item['classification']} |"
        )
    args.markdown_output.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
