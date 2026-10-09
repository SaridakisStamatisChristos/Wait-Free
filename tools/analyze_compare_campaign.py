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

BASELINE = "veriqueue"
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
        sample = [values[rng.randrange(n)] for _ in range(n)]
        medians.append(statistics.median(sample))
    return percentile(medians, 0.025), percentile(medians, 0.975)


def classify(lower: float, upper: float) -> str:
    if lower > LOWER_WIN_THRESHOLD:
        return "WIN"
    if upper < UPPER_LOSS_THRESHOLD:
        return "LOSS"
    return "TIE/INCONCLUSIVE"


def main() -> None:
    parser = argparse.ArgumentParser(description="Analyze paired GitHub-hosted comparisons")
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("--json-output", type=pathlib.Path, required=True)
    parser.add_argument("--markdown-output", type=pathlib.Path, required=True)
    parser.add_argument("--bootstrap-seed", type=int, default=20261009)
    args = parser.parse_args()

    records: list[dict[str, Any]] = []
    with args.input.open(encoding="utf-8") as handle:
        for line in handle:
            record = json.loads(line)
            if record.get("warmup"):
                continue
            if not record.get("valid", False):
                raise SystemExit(f"invalid record in measured set: {record}")
            records.append(record)

    cells: dict[tuple[int, int, str, int, int], dict[int, dict[str, float]]] = defaultdict(
        lambda: defaultdict(dict)
    )
    for record in records:
        key = (
            int(record["capacity"]),
            int(record["payload_bytes"]),
            str(record.get("topology", "unknown")),
            int(record.get("producer_cpu", -1)),
            int(record.get("consumer_cpu", -1)),
        )
        repetition = int(record["repetition"])
        implementation = str(record["implementation"])
        rate = float(record["transfers_per_second"])
        if implementation in cells[key][repetition]:
            raise SystemExit(f"duplicate result for {key=} {repetition=} {implementation=}")
        cells[key][repetition][implementation] = rate

    comparisons: list[dict[str, Any]] = []
    for cell_index, (key, repetitions) in enumerate(sorted(cells.items())):
        capacity, payload_bytes, topology, producer_cpu, consumer_cpu = key
        implementations = sorted(
            {implementation for per_rep in repetitions.values() for implementation in per_rep}
        )
        if BASELINE not in implementations:
            raise SystemExit(f"missing {BASELINE} in cell {key}")

        for competitor in implementations:
            if competitor == BASELINE:
                continue
            ratios: list[float] = []
            missing: list[int] = []
            for repetition, per_rep in sorted(repetitions.items()):
                if BASELINE not in per_rep or competitor not in per_rep:
                    missing.append(repetition)
                    continue
                competitor_rate = per_rep[competitor]
                if competitor_rate <= 0:
                    raise SystemExit(f"non-positive competitor rate in {key=} {repetition=}")
                ratios.append(per_rep[BASELINE] / competitor_rate)
            if missing:
                raise SystemExit(f"unpaired repetitions for {competitor} in {key}: {missing}")
            if not ratios:
                continue

            ci_low, ci_high = bootstrap_median_ci(
                ratios,
                args.bootstrap_seed ^ (cell_index * 0x9E3779B1) ^ sum(ord(c) for c in competitor),
            )
            mean = statistics.mean(ratios)
            cv = statistics.pstdev(ratios) / mean if mean else 0.0
            result = {
                "capacity": capacity,
                "payload_bytes": payload_bytes,
                "topology": topology,
                "producer_cpu": producer_cpu,
                "consumer_cpu": consumer_cpu,
                "competitor": competitor,
                "n": len(ratios),
                "median_ratio": statistics.median(ratios),
                "iqr_low": percentile(ratios, 0.25),
                "iqr_high": percentile(ratios, 0.75),
                "p05": percentile(ratios, 0.05),
                "p95": percentile(ratios, 0.95),
                "cv": cv,
                "ci95_low": ci_low,
                "ci95_high": ci_high,
                "classification": classify(ci_low, ci_high),
                "thresholds": {
                    "win_ci_lower_gt": LOWER_WIN_THRESHOLD,
                    "loss_ci_upper_lt": UPPER_LOSS_THRESHOLD,
                },
            }
            comparisons.append(result)

    payload = {
        "method": "paired_veriqueue_throughput_ratio",
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
    args.json_output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")

    lines = [
        "# Paired comparative benchmark summary",
        "",
        "Ratio is `VeriQueue throughput / competitor throughput` within the same measured repetition.",
        "Classification is predeclared: WIN when bootstrap 95% CI lower bound > 1.03; "
        "LOSS when CI upper bound < 0.97; otherwise TIE/INCONCLUSIVE.",
        "",
        "| Capacity | Payload | Topology | Competitor | n | Median ratio | IQR | p05 | p95 | CV | 95% CI | Verdict |",
        "|---:|---:|---|---|---:|---:|---|---:|---:|---:|---|---|",
    ]
    for item in comparisons:
        lines.append(
            f"| {item['capacity']} | {item['payload_bytes']} | {item['topology']} | "
            f"{item['competitor']} | {item['n']} | {item['median_ratio']:.4f} | "
            f"[{item['iqr_low']:.4f}, {item['iqr_high']:.4f}] | {item['p05']:.4f} | "
            f"{item['p95']:.4f} | {item['cv']:.4f} | "
            f"[{item['ci95_low']:.4f}, {item['ci95_high']:.4f}] | {item['classification']} |"
        )
    args.markdown_output.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
