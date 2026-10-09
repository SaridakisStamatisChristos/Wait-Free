#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import pathlib
import random
import statistics
from collections import Counter, defaultdict
from typing import Any

PRODUCTION = "production"
LOWER_WIN_THRESHOLD = 1.03
UPPER_LOSS_THRESHOLD = 0.97
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
    rng = random.Random(seed)
    n = len(values)
    medians: list[float] = []
    for _ in range(BOOTSTRAP_SAMPLES):
        medians.append(statistics.median(values[rng.randrange(n)] for _ in range(n)))
    return percentile(medians, 0.025), percentile(medians, 0.975)


def classify(lower: float, upper: float) -> str:
    if lower > LOWER_WIN_THRESHOLD:
        return "WIN"
    if upper < UPPER_LOSS_THRESHOLD:
        return "LOSS"
    return "TIE/INCONCLUSIVE"


def main() -> None:
    parser = argparse.ArgumentParser(description="Analyze paired VeriQueue hot-path v2 results")
    parser.add_argument("input", nargs="+", type=pathlib.Path)
    parser.add_argument("--json-output", type=pathlib.Path, required=True)
    parser.add_argument("--markdown-output", type=pathlib.Path, required=True)
    parser.add_argument("--bootstrap-seed", type=int, default=20261010)
    parser.add_argument("--require-repetitions", type=int, default=30)
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

    cells: dict[tuple[Any, ...], dict[int, dict[str, float]]] = defaultdict(lambda: defaultdict(dict))
    for record in records:
        environment = record.get("environment") or {}
        key = (
            int(record["capacity"]),
            int(record["payload_bytes"]),
            str(record.get("mode", "scalar")),
            int(record.get("batch_size", 1)),
            str(record.get("topology", "unknown")),
            int(record.get("producer_cpu", -1)),
            int(record.get("consumer_cpu", -1)),
            str(environment.get("architecture", "unknown")),
            str(environment.get("compiler", "unknown")),
        )
        repetition = int(record["repetition"])
        implementation = str(record["implementation"])
        if implementation in cells[key][repetition]:
            raise SystemExit(f"duplicate result for {key=} {repetition=} {implementation=}")
        cells[key][repetition][implementation] = float(record["transfers_per_second"])

    comparisons: list[dict[str, Any]] = []
    variants = sorted({str(record["implementation"]) for record in records if record["implementation"] != PRODUCTION})
    for cell_index, (key, repetitions) in enumerate(sorted(cells.items())):
        (capacity, payload_bytes, mode, batch_size, topology, producer_cpu,
         consumer_cpu, architecture, compiler) = key
        for variant_index, variant in enumerate(variants):
            ratios: list[float] = []
            missing: list[int] = []
            for repetition, group in sorted(repetitions.items()):
                if PRODUCTION not in group or variant not in group:
                    missing.append(repetition)
                    continue
                if group[PRODUCTION] <= 0.0:
                    raise SystemExit(f"non-positive production throughput in {key=} {repetition=}")
                ratios.append(group[variant] / group[PRODUCTION])
            if missing:
                raise SystemExit(f"unpaired repetitions for {variant} in {key}: {missing}")
            if len(ratios) != args.require_repetitions:
                raise SystemExit(
                    f"expected {args.require_repetitions} paired repetitions for {variant} in {key}, got {len(ratios)}"
                )

            ci_low, ci_high = bootstrap_median_ci(
                ratios,
                args.bootstrap_seed ^ (cell_index * 0x9E3779B1) ^ (variant_index * 0x85EBCA6B),
            )
            mean = statistics.mean(ratios)
            comparisons.append(
                {
                    "variant": variant,
                    "capacity": capacity,
                    "payload_bytes": payload_bytes,
                    "mode": mode,
                    "batch_size": batch_size,
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
                    "priority_cell": capacity == 1024 and payload_bytes in (8, 16) and mode == "scalar",
                }
            )

    counts: dict[str, dict[str, int]] = {}
    priority: dict[str, dict[str, str]] = defaultdict(dict)
    for variant in variants:
        verdicts = Counter(
            item["classification"] for item in comparisons if item["variant"] == variant
        )
        counts[variant] = {
            "WIN": verdicts["WIN"],
            "LOSS": verdicts["LOSS"],
            "TIE/INCONCLUSIVE": verdicts["TIE/INCONCLUSIVE"],
        }
        for item in comparisons:
            if item["variant"] == variant and item["priority_cell"]:
                key = f"{item['architecture']}|{item['compiler']}|{item['topology']}|1024x{item['payload_bytes']}"
                priority[variant][key] = item["classification"]

    payload = {
        "method": "paired_candidate_over_production_throughput_ratio",
        "bootstrap_samples": BOOTSTRAP_SAMPLES,
        "bootstrap_seed": args.bootstrap_seed,
        "thresholds": {
            "win_ci_lower_gt": LOWER_WIN_THRESHOLD,
            "loss_ci_upper_lt": UPPER_LOSS_THRESHOLD,
        },
        "counts": counts,
        "priority_cells": priority,
        "comparisons": comparisons,
    }
    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    lines = [
        "# VeriQueue hot-path v2 paired benchmark summary",
        "",
        "Each ratio is `candidate throughput / production VeriQueue throughput` within the same repetition.",
        "WIN requires bootstrap 95% CI lower bound > 1.03; LOSS requires CI upper bound < 0.97; otherwise TIE/INCONCLUSIVE.",
        "Affinity-invalid measurements fail closed. Topologies, compilers, architectures, scalar/bulk modes, and batch sizes are never pooled.",
        "",
        "## Verdict counts",
        "",
        "| Variant | WIN | TIE/INCONCLUSIVE | LOSS |",
        "|---|---:|---:|---:|",
    ]
    for variant in variants:
        item = counts[variant]
        lines.append(f"| {variant} | {item['WIN']} | {item['TIE/INCONCLUSIVE']} | {item['LOSS']} |")

    lines.extend(
        [
            "",
            "## Per-cell paired statistics",
            "",
            "| Variant | Arch | Compiler | Mode | Batch | Capacity | Payload | Topology | n | Median | IQR | p05 | p95 | CV | 95% CI | Verdict |",
            "|---|---|---|---|---:|---:|---:|---|---:|---:|---|---:|---:|---:|---|---|",
        ]
    )
    for item in comparisons:
        lines.append(
            f"| {item['variant']} | {item['architecture']} | {item['compiler']} | {item['mode']} | "
            f"{item['batch_size']} | {item['capacity']} | {item['payload_bytes']} | {item['topology']} | "
            f"{item['n']} | {item['median_ratio']:.4f} | [{item['iqr_low']:.4f}, {item['iqr_high']:.4f}] | "
            f"{item['p05']:.4f} | {item['p95']:.4f} | {item['cv']:.4f} | "
            f"[{item['ci95_low']:.4f}, {item['ci95_high']:.4f}] | {item['classification']} |"
        )
    args.markdown_output.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
