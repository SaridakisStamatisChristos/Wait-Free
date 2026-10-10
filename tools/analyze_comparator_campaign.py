#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import math
import pathlib
import random
import statistics
import sys
from collections import defaultdict
from typing import Any, Iterable

DEFAULT_IMPLEMENTATIONS = (
    "veriqueue",
    "rigtorp",
    "boost_lockfree",
    "moodycamel",
    "drogalis",
)
DEFAULT_BOOTSTRAP_SAMPLES = 10_000


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


def geometric_mean(values: Iterable[float]) -> float:
    data = list(values)
    if not data or any(value <= 0.0 for value in data):
        raise ValueError("geometric mean requires positive data")
    return math.exp(sum(math.log(value) for value in data) / len(data))


def bootstrap_median_ci(
    values: list[float], seed: int, samples: int
) -> tuple[float, float]:
    if not values:
        raise ValueError("bootstrap requires data")
    rng = random.Random(seed)
    n = len(values)
    estimates = []
    for _ in range(samples):
        sample = [values[rng.randrange(n)] for _ in range(n)]
        estimates.append(statistics.median(sample))
    return percentile(estimates, 0.025), percentile(estimates, 0.975)


def bootstrap_geomean_ci(
    values: list[float], seed: int, samples: int
) -> tuple[float, float]:
    if not values:
        raise ValueError("bootstrap requires data")
    rng = random.Random(seed)
    n = len(values)
    estimates = []
    for _ in range(samples):
        sample = [values[rng.randrange(n)] for _ in range(n)]
        estimates.append(geometric_mean(sample))
    return percentile(estimates, 0.025), percentile(estimates, 0.975)


def classify(ci_low: float, ci_high: float) -> str:
    if ci_low > 1.0:
        return "WIN"
    if ci_high < 1.0:
        return "LOSS"
    return "TIE"


def expand_inputs(paths: list[pathlib.Path]) -> list[pathlib.Path]:
    expanded: list[pathlib.Path] = []
    for path in paths:
        if path.is_dir():
            expanded.extend(sorted(path.rglob("*.jsonl")))
        elif path.is_file():
            expanded.append(path)
        else:
            raise ValueError(f"input path does not exist: {path}")
    unique: list[pathlib.Path] = []
    seen: set[pathlib.Path] = set()
    for path in expanded:
        resolved = path.resolve()
        if resolved not in seen:
            seen.add(resolved)
            unique.append(path)
    if not unique:
        raise ValueError("no JSONL inputs found")
    return unique


def load_records(paths: list[pathlib.Path]) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    metadata: list[dict[str, Any]] = []
    records: list[dict[str, Any]] = []
    for path in expand_inputs(paths):
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if not line.strip():
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError(f"invalid JSON in {path}:{line_number}: {exc}") from exc
            if record.get("record_type") == "meta":
                metadata.append(record)
            else:
                records.append(record)
    if not records:
        raise ValueError("no benchmark records found")
    return metadata, records


def cell_key(record: dict[str, Any]) -> tuple[str, str, str, int, int, str, int, int]:
    return (
        str(record["comparison_lane"]),
        str(record["comparison_architecture"]),
        str(record["comparison_compiler"]),
        int(record["capacity"]),
        int(record["payload_bytes"]),
        str(record.get("topology", "unknown")),
        int(record.get("producer_cpu", -1)),
        int(record.get("consumer_cpu", -1)),
    )


def analyze_records(
    records: list[dict[str, Any]],
    bootstrap_seed: int,
    bootstrap_samples: int,
    expected_implementations: tuple[str, ...] = DEFAULT_IMPLEMENTATIONS,
) -> dict[str, Any]:
    cells: dict[
        tuple[str, str, str, int, int, str, int, int],
        dict[int, dict[str, float]],
    ] = defaultdict(lambda: defaultdict(dict))
    source_commits: set[str] = set()
    version_fingerprints: dict[str, set[str]] = defaultdict(set)

    for record in records:
        if record.get("warmup"):
            continue
        if record.get("comparison_schema") != "veriqueue_cross_algorithm_campaign_v1":
            raise ValueError(f"unexpected comparison schema: {record}")
        if not record.get("valid", False) or not record.get("affinity_valid", False):
            raise ValueError(f"invalid measured record: {record}")
        source_commits.add(str(record["comparison_source_commit"]))
        implementation = str(record["implementation"])
        if implementation not in expected_implementations:
            raise ValueError(f"unexpected implementation: {implementation}")
        rate = float(record["transfers_per_second"])
        if rate <= 0.0:
            raise ValueError(f"non-positive throughput: {record}")
        key = cell_key(record)
        repetition = int(record["repetition"])
        if implementation in cells[key][repetition]:
            raise ValueError(
                f"duplicate implementation={implementation} cell={key} repetition={repetition}"
            )
        cells[key][repetition][implementation] = rate
        for field in ("rigtorp_commit", "moodycamel_commit", "drogalis_commit", "boost_version"):
            if field in record:
                version_fingerprints[field].add(str(record[field]))

    if len(source_commits) != 1:
        raise ValueError(f"campaign mixes source commits: {sorted(source_commits)}")
    for field, values in version_fingerprints.items():
        if len(values) != 1:
            raise ValueError(f"campaign mixes baseline versions for {field}: {sorted(values)}")

    pairwise: list[dict[str, Any]] = []
    best_external_rows: list[dict[str, Any]] = []
    competitors = tuple(i for i in expected_implementations if i != "veriqueue")

    for cell_index, (key, repetitions) in enumerate(sorted(cells.items())):
        lane, architecture, compiler, capacity, payload, topology, producer_cpu, consumer_cpu = key
        expected_set = set(expected_implementations)
        for repetition, implementation_rates in sorted(repetitions.items()):
            if set(implementation_rates) != expected_set:
                raise ValueError(
                    f"incomplete repetition cell={key} repetition={repetition}: "
                    f"have={sorted(implementation_rates)} expected={sorted(expected_set)}"
                )

        medians = {
            implementation: statistics.median(
                [rep[implementation] for rep in repetitions.values()]
            )
            for implementation in expected_implementations
        }

        cell_rows: list[dict[str, Any]] = []
        for competitor_index, competitor in enumerate(competitors):
            ratios = [
                rep["veriqueue"] / rep[competitor]
                for _, rep in sorted(repetitions.items())
            ]
            seed = (
                bootstrap_seed
                ^ (cell_index * 0x9E3779B1)
                ^ (competitor_index * 0x85EBCA77)
            )
            ci_low, ci_high = bootstrap_median_ci(ratios, seed, bootstrap_samples)
            median_ratio = statistics.median(ratios)
            mean_ratio = statistics.mean(ratios)
            row = {
                "lane": lane,
                "architecture": architecture,
                "compiler": compiler,
                "capacity": capacity,
                "payload_bytes": payload,
                "topology": topology,
                "producer_cpu": producer_cpu,
                "consumer_cpu": consumer_cpu,
                "competitor": competitor,
                "n": len(ratios),
                "veriqueue_median_tps": medians["veriqueue"],
                "competitor_median_tps": medians[competitor],
                "median_ratio": median_ratio,
                "iqr_low": percentile(ratios, 0.25),
                "iqr_high": percentile(ratios, 0.75),
                "p05": percentile(ratios, 0.05),
                "p95": percentile(ratios, 0.95),
                "cv": statistics.pstdev(ratios) / mean_ratio if mean_ratio else 0.0,
                "ci95_low": ci_low,
                "ci95_high": ci_high,
                "classification": classify(ci_low, ci_high),
            }
            pairwise.append(row)
            cell_rows.append(row)

        best_competitor = max(competitors, key=lambda name: medians[name])
        best_row = next(row for row in cell_rows if row["competitor"] == best_competitor)
        best_external_rows.append({**best_row, "best_competitor": best_competitor})

    def summarize(rows: list[dict[str, Any]], seed_salt: int) -> dict[str, Any]:
        ratios = [float(row["median_ratio"]) for row in rows]
        ci_low, ci_high = bootstrap_geomean_ci(
            ratios, bootstrap_seed ^ seed_salt, bootstrap_samples
        )
        return {
            "cells": len(rows),
            "geometric_mean_ratio": geometric_mean(ratios),
            "cell_bootstrap_ci95_low": ci_low,
            "cell_bootstrap_ci95_high": ci_high,
            "wins": sum(row["classification"] == "WIN" for row in rows),
            "ties": sum(row["classification"] == "TIE" for row in rows),
            "losses": sum(row["classification"] == "LOSS" for row in rows),
        }

    overall_by_competitor: dict[str, dict[str, Any]] = {}
    lane_by_competitor: list[dict[str, Any]] = []
    lanes = sorted({str(row["lane"]) for row in pairwise})
    for competitor_index, competitor in enumerate(competitors):
        comp_rows = [row for row in pairwise if row["competitor"] == competitor]
        overall_by_competitor[competitor] = summarize(
            comp_rows, 0x100000 + competitor_index
        )
        for lane_index, lane in enumerate(lanes):
            lane_rows = [row for row in comp_rows if row["lane"] == lane]
            if lane_rows:
                lane_by_competitor.append(
                    {
                        "lane": lane,
                        "competitor": competitor,
                        **summarize(
                            lane_rows,
                            0x200000 + competitor_index * 100 + lane_index,
                        ),
                    }
                )

    overall_best = summarize(best_external_rows, 0x300000)
    lane_best: list[dict[str, Any]] = []
    for lane_index, lane in enumerate(lanes):
        lane_rows = [row for row in best_external_rows if row["lane"] == lane]
        if lane_rows:
            lane_best.append(
                {
                    "lane": lane,
                    **summarize(lane_rows, 0x400000 + lane_index),
                }
            )

    return {
        "schema": "veriqueue_cross_algorithm_analysis_v1",
        "source_commit": next(iter(source_commits)),
        "baseline_versions": {
            field: next(iter(values)) for field, values in sorted(version_fingerprints.items())
        },
        "bootstrap_samples": bootstrap_samples,
        "bootstrap_seed": bootstrap_seed,
        "cell_classification": {
            "WIN": "bootstrap 95% CI for paired median VeriQueue/competitor ratio is entirely above 1.0",
            "TIE": "bootstrap 95% CI overlaps 1.0",
            "LOSS": "bootstrap 95% CI is entirely below 1.0",
        },
        "pairwise_cells": pairwise,
        "best_external_cells": best_external_rows,
        "overall_by_competitor": overall_by_competitor,
        "lane_by_competitor": lane_by_competitor,
        "overall_vs_best_external": overall_best,
        "lane_vs_best_external": lane_best,
    }


def write_csv(path: pathlib.Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "lane",
        "architecture",
        "compiler",
        "capacity",
        "payload_bytes",
        "competitor",
        "n",
        "veriqueue_median_tps",
        "competitor_median_tps",
        "median_ratio",
        "ci95_low",
        "ci95_high",
        "classification",
        "cv",
    ]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def markdown_report(result: dict[str, Any], headline_only: bool = False) -> str:
    lines = [
        "# VeriQueue cross-algorithm benchmark",
        "",
        f"Source commit: `{result['source_commit']}`",
        "",
        "Per-cell verdicts use paired repetitions with randomized implementation order. "
        "WIN means the bootstrap 95% CI for `VeriQueue / competitor` is entirely above 1.0; "
        "LOSS means entirely below 1.0; otherwise the cell is a TIE.",
        "",
        "Scope: common-denominator scalar SPSC push/pop throughput. Capacities are the nominal "
        "requested capacities; implementations may use different internal storage/capacity semantics. "
        "Native bulk APIs are intentionally excluded from this cross-implementation ranking because "
        "equivalent bulk interfaces are not available across every comparator.",
        "",
        "## Overall vs each comparator",
        "",
        "| Comparator | Cells | Geomean ratio | Cell-bootstrap 95% CI | W | T | L |",
        "|---|---:|---:|---|---:|---:|---:|",
    ]
    for competitor, summary in result["overall_by_competitor"].items():
        lines.append(
            f"| {competitor} | {summary['cells']} | {summary['geometric_mean_ratio']:.4f}x | "
            f"[{summary['cell_bootstrap_ci95_low']:.4f}, {summary['cell_bootstrap_ci95_high']:.4f}] | "
            f"{summary['wins']} | {summary['ties']} | {summary['losses']} |"
        )

    best = result["overall_vs_best_external"]
    lines.extend(
        [
            "",
            "## Against the fastest external comparator in each cell",
            "",
            f"Overall geomean: **{best['geometric_mean_ratio']:.4f}x** "
            f"(cell-bootstrap 95% CI [{best['cell_bootstrap_ci95_low']:.4f}, "
            f"{best['cell_bootstrap_ci95_high']:.4f}]); "
            f"**{best['wins']} wins / {best['ties']} ties / {best['losses']} losses**.",
            "",
            "| Lane | Cells | Geomean ratio | Cell-bootstrap 95% CI | W | T | L |",
            "|---|---:|---:|---|---:|---:|---:|",
        ]
    )
    for summary in result["lane_vs_best_external"]:
        lines.append(
            f"| {summary['lane']} | {summary['cells']} | {summary['geometric_mean_ratio']:.4f}x | "
            f"[{summary['cell_bootstrap_ci95_low']:.4f}, {summary['cell_bootstrap_ci95_high']:.4f}] | "
            f"{summary['wins']} | {summary['ties']} | {summary['losses']} |"
        )

    if headline_only:
        return "\n".join(lines) + "\n"

    lines.extend(
        [
            "",
            "## Lane summary by comparator",
            "",
            "| Lane | Comparator | Cells | Geomean ratio | 95% CI | W | T | L |",
            "|---|---|---:|---:|---|---:|---:|---:|",
        ]
    )
    for summary in result["lane_by_competitor"]:
        lines.append(
            f"| {summary['lane']} | {summary['competitor']} | {summary['cells']} | "
            f"{summary['geometric_mean_ratio']:.4f}x | "
            f"[{summary['cell_bootstrap_ci95_low']:.4f}, {summary['cell_bootstrap_ci95_high']:.4f}] | "
            f"{summary['wins']} | {summary['ties']} | {summary['losses']} |"
        )

    lines.extend(
        [
            "",
            "## Cell-level comparison",
            "",
            "| Lane | Capacity | Payload | Comparator | VQ median/s | Comparator median/s | Ratio | 95% CI | Verdict |",
            "|---|---:|---:|---|---:|---:|---:|---|---|",
        ]
    )
    for row in result["pairwise_cells"]:
        lines.append(
            f"| {row['lane']} | {row['capacity']} | {row['payload_bytes']} | {row['competitor']} | "
            f"{row['veriqueue_median_tps']:.0f} | {row['competitor_median_tps']:.0f} | "
            f"{row['median_ratio']:.4f}x | [{row['ci95_low']:.4f}, {row['ci95_high']:.4f}] | "
            f"{row['classification']} |"
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Analyze randomized multi-implementation SPSC comparison campaign"
    )
    parser.add_argument("inputs", nargs="+", type=pathlib.Path)
    parser.add_argument("--json-output", type=pathlib.Path, required=True)
    parser.add_argument("--markdown-output", type=pathlib.Path, required=True)
    parser.add_argument("--headline-output", type=pathlib.Path, required=True)
    parser.add_argument("--csv-output", type=pathlib.Path, required=True)
    parser.add_argument("--bootstrap-seed", type=int, default=20261010)
    parser.add_argument("--bootstrap-samples", type=int, default=DEFAULT_BOOTSTRAP_SAMPLES)
    args = parser.parse_args()

    if args.bootstrap_samples <= 0:
        parser.error("bootstrap-samples must be positive")

    try:
        metadata, records = load_records(args.inputs)
        result = analyze_records(records, args.bootstrap_seed, args.bootstrap_samples)
        if metadata:
            result["campaign_metadata"] = metadata
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"comparator analysis failed: {exc}", file=sys.stderr)
        return 2

    for path in (args.json_output, args.markdown_output, args.headline_output, args.csv_output):
        path.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    args.markdown_output.write_text(markdown_report(result), encoding="utf-8")
    args.headline_output.write_text(
        markdown_report(result, headline_only=True), encoding="utf-8"
    )
    write_csv(args.csv_output, result["pairwise_cells"])
    print(args.headline_output.read_text(encoding="utf-8"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
