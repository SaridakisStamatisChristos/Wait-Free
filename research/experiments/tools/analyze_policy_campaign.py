#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import pathlib
import statistics
import sys
from collections import defaultdict
from typing import Any

from analyze_comparator_campaign import (
    bootstrap_geomean_ci,
    bootstrap_median_ci,
    cell_key,
    classify,
    geometric_mean,
    load_records,
)

DEFAULT_EXTERNALS = ("rigtorp", "boost_lockfree", "moodycamel", "drogalis")


def parse_csv(value: str) -> tuple[str, ...]:
    values = tuple(item.strip() for item in value.split(",") if item.strip())
    if not values:
        raise ValueError("list cannot be empty")
    if len(set(values)) != len(values):
        raise ValueError("list values must be unique")
    return values


def analyze(
    records: list[dict[str, Any]],
    candidates: tuple[str, ...],
    externals: tuple[str, ...],
    bootstrap_seed: int,
    bootstrap_samples: int,
) -> dict[str, Any]:
    expected = set(candidates) | set(externals)
    if set(candidates) & set(externals):
        raise ValueError("candidate and external sets must be disjoint")
    if "veriqueue" not in candidates:
        raise ValueError("candidates must include veriqueue as the production control")

    cells: dict[tuple[Any, ...], dict[int, dict[str, float]]] = defaultdict(lambda: defaultdict(dict))
    source_commits: set[str] = set()
    version_fingerprints: dict[str, set[str]] = defaultdict(set)

    for record in records:
        if record.get("warmup"):
            continue
        if record.get("comparison_schema") != "veriqueue_cross_algorithm_campaign_v1":
            raise ValueError(f"unexpected comparison schema: {record}")
        if not record.get("valid", False) or not record.get("affinity_valid", False):
            raise ValueError(f"invalid measured record: {record}")
        implementation = str(record["implementation"])
        if implementation not in expected:
            raise ValueError(f"unexpected implementation: {implementation}")
        rate = float(record["transfers_per_second"])
        if rate <= 0.0:
            raise ValueError(f"non-positive throughput: {record}")
        source_commits.add(str(record["comparison_source_commit"]))
        key = cell_key(record)
        repetition = int(record["repetition"])
        if implementation in cells[key][repetition]:
            raise ValueError(f"duplicate implementation={implementation} cell={key} repetition={repetition}")
        cells[key][repetition][implementation] = rate
        for field in ("rigtorp_commit", "moodycamel_commit", "drogalis_commit", "boost_version"):
            if field in record:
                version_fingerprints[field].add(str(record[field]))

    if len(source_commits) != 1:
        raise ValueError(f"campaign mixes source commits: {sorted(source_commits)}")
    for field, values in version_fingerprints.items():
        if len(values) != 1:
            raise ValueError(f"campaign mixes baseline versions for {field}: {sorted(values)}")

    rows: list[dict[str, Any]] = []
    for cell_index, (key, repetitions) in enumerate(sorted(cells.items())):
        lane, architecture, compiler, capacity, payload, topology, producer_cpu, consumer_cpu = key
        for repetition, rates in sorted(repetitions.items()):
            if set(rates) != expected:
                raise ValueError(
                    f"incomplete repetition cell={key} repetition={repetition}: "
                    f"have={sorted(rates)} expected={sorted(expected)}"
                )

        medians = {
            impl: statistics.median(rep[impl] for rep in repetitions.values())
            for impl in expected
        }
        best_external = max(externals, key=lambda name: medians[name])

        for candidate_index, candidate in enumerate(candidates):
            ordered_repetitions = [rep for _, rep in sorted(repetitions.items())]
            best_ratios = [rep[candidate] / rep[best_external] for rep in ordered_repetitions]
            control_ratios = [rep[candidate] / rep["veriqueue"] for rep in ordered_repetitions]
            best_seed = bootstrap_seed ^ (cell_index * 0x9E3779B1) ^ (candidate_index * 0x85EBCA77)
            control_seed = best_seed ^ 0xC2B2AE3D
            best_low, best_high = bootstrap_median_ci(best_ratios, best_seed, bootstrap_samples)
            control_low, control_high = bootstrap_median_ci(control_ratios, control_seed, bootstrap_samples)
            rows.append(
                {
                    "lane": lane,
                    "architecture": architecture,
                    "compiler": compiler,
                    "capacity": capacity,
                    "payload_bytes": payload,
                    "topology": topology,
                    "producer_cpu": producer_cpu,
                    "consumer_cpu": consumer_cpu,
                    "candidate": candidate,
                    "best_external": best_external,
                    "n": len(best_ratios),
                    "candidate_median_tps": medians[candidate],
                    "best_external_median_tps": medians[best_external],
                    "production_median_tps": medians["veriqueue"],
                    "vs_best_external_median_ratio": statistics.median(best_ratios),
                    "vs_best_external_ci95_low": best_low,
                    "vs_best_external_ci95_high": best_high,
                    "vs_best_external_classification": classify(best_low, best_high),
                    "vs_production_median_ratio": statistics.median(control_ratios),
                    "vs_production_ci95_low": control_low,
                    "vs_production_ci95_high": control_high,
                    "vs_production_classification": classify(control_low, control_high),
                }
            )

    def summarize(candidate: str, selected: list[dict[str, Any]], salt: int) -> dict[str, Any]:
        best_values = [float(row["vs_best_external_median_ratio"]) for row in selected]
        prod_values = [float(row["vs_production_median_ratio"]) for row in selected]
        best_low, best_high = bootstrap_geomean_ci(best_values, bootstrap_seed ^ salt, bootstrap_samples)
        prod_low, prod_high = bootstrap_geomean_ci(
            prod_values, bootstrap_seed ^ salt ^ 0x27D4EB2D, bootstrap_samples
        )
        return {
            "candidate": candidate,
            "cells": len(selected),
            "vs_best_external_geomean": geometric_mean(best_values),
            "vs_best_external_ci95_low": best_low,
            "vs_best_external_ci95_high": best_high,
            "vs_best_external_wins": sum(r["vs_best_external_classification"] == "WIN" for r in selected),
            "vs_best_external_ties": sum(r["vs_best_external_classification"] == "TIE" for r in selected),
            "vs_best_external_losses": sum(r["vs_best_external_classification"] == "LOSS" for r in selected),
            "vs_production_geomean": geometric_mean(prod_values),
            "vs_production_ci95_low": prod_low,
            "vs_production_ci95_high": prod_high,
            "vs_production_wins": sum(r["vs_production_classification"] == "WIN" for r in selected),
            "vs_production_ties": sum(r["vs_production_classification"] == "TIE" for r in selected),
            "vs_production_losses": sum(r["vs_production_classification"] == "LOSS" for r in selected),
        }

    overall: list[dict[str, Any]] = []
    by_lane: list[dict[str, Any]] = []
    lanes = sorted({str(row["lane"]) for row in rows})
    for candidate_index, candidate in enumerate(candidates):
        selected = [row for row in rows if row["candidate"] == candidate]
        overall.append(summarize(candidate, selected, 0x100000 + candidate_index))
        for lane_index, lane in enumerate(lanes):
            lane_rows = [row for row in selected if row["lane"] == lane]
            if lane_rows:
                by_lane.append(
                    {
                        "lane": lane,
                        **summarize(
                            candidate,
                            lane_rows,
                            0x200000 + candidate_index * 100 + lane_index,
                        ),
                    }
                )

    return {
        "schema": "veriqueue_policy_campaign_analysis_v1",
        "source_commit": next(iter(source_commits)),
        "candidates": list(candidates),
        "externals": list(externals),
        "baseline_versions": {
            field: next(iter(values)) for field, values in sorted(version_fingerprints.items())
        },
        "bootstrap_seed": bootstrap_seed,
        "bootstrap_samples": bootstrap_samples,
        "overall": overall,
        "by_lane": by_lane,
        "cells": rows,
    }


def markdown_report(result: dict[str, Any]) -> str:
    lines = [
        "# VeriQueue compile-time policy forensic campaign",
        "",
        f"Source commit: `{result['source_commit']}`",
        "",
        "Each experimental candidate is compared in the same randomized repetition against the "
        "fastest external implementation selected by median throughput for that cell, and against "
        "the production VeriQueue control. This is promotion evidence, not a universal SOTA claim.",
        "",
        "## Candidate summary",
        "",
        "| Candidate | Cells | vs best external | 95% CI | W/T/L | vs production | 95% CI | W/T/L |",
        "|---|---:|---:|---|---|---:|---|---|",
    ]
    for row in sorted(
        result["overall"], key=lambda item: item["vs_best_external_geomean"], reverse=True
    ):
        lines.append(
            f"| {row['candidate']} | {row['cells']} | {row['vs_best_external_geomean']:.4f}x | "
            f"[{row['vs_best_external_ci95_low']:.4f}, {row['vs_best_external_ci95_high']:.4f}] | "
            f"{row['vs_best_external_wins']}/{row['vs_best_external_ties']}/{row['vs_best_external_losses']} | "
            f"{row['vs_production_geomean']:.4f}x | "
            f"[{row['vs_production_ci95_low']:.4f}, {row['vs_production_ci95_high']:.4f}] | "
            f"{row['vs_production_wins']}/{row['vs_production_ties']}/{row['vs_production_losses']} |"
        )
    lines.extend(
        [
            "",
            "## Lane summary",
            "",
            "| Lane | Candidate | vs best external | vs production |",
            "|---|---|---:|---:|",
        ]
    )
    for row in sorted(
        result["by_lane"], key=lambda item: (item["lane"], -item["vs_best_external_geomean"])
    ):
        lines.append(
            f"| {row['lane']} | {row['candidate']} | "
            f"{row['vs_best_external_geomean']:.4f}x | {row['vs_production_geomean']:.4f}x |"
        )
    return "\n".join(lines) + "\n"


def write_csv(path: pathlib.Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = list(rows[0].keys()) if rows else []
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        if fieldnames:
            writer.writeheader()
            writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Analyze compile-time policy candidates against external SPSC baselines"
    )
    parser.add_argument("inputs", nargs="+", type=pathlib.Path)
    parser.add_argument("--candidates", required=True)
    parser.add_argument("--externals", default=",".join(DEFAULT_EXTERNALS))
    parser.add_argument("--json-output", type=pathlib.Path, required=True)
    parser.add_argument("--markdown-output", type=pathlib.Path, required=True)
    parser.add_argument("--csv-output", type=pathlib.Path, required=True)
    parser.add_argument("--bootstrap-seed", type=int, default=20261010)
    parser.add_argument("--bootstrap-samples", type=int, default=10_000)
    args = parser.parse_args()

    try:
        candidates = parse_csv(args.candidates)
        externals = parse_csv(args.externals)
        _, records = load_records(args.inputs)
        result = analyze(records, candidates, externals, args.bootstrap_seed, args.bootstrap_samples)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"policy campaign analysis failed: {exc}", file=sys.stderr)
        return 2

    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    args.markdown_output.write_text(markdown_report(result), encoding="utf-8")
    write_csv(args.csv_output, result["cells"])
    print(args.markdown_output.read_text(encoding="utf-8"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
