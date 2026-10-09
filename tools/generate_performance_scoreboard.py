#!/usr/bin/env python3
"""Generate the VeriQueue performance scoreboard from frozen campaign evidence only."""

from __future__ import annotations

import argparse
import importlib.util
import json
import pathlib
import statistics
import sys
from collections import defaultdict
from typing import Any

WIN_THRESHOLD = 1.03
LOSS_THRESHOLD = 0.97
EXPECTED_METHOD = "paired_veriqueue_throughput_ratio"


def _load_verifier():
    path = pathlib.Path(__file__).resolve().with_name("verify_frozen_evidence.py")
    spec = importlib.util.spec_from_file_location("verify_frozen_evidence", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load frozen-evidence verifier")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


VERIFIER = _load_verifier()


def classify(ci_low: float, ci_high: float) -> str:
    if ci_low > WIN_THRESHOLD:
        return "WIN"
    if ci_high < LOSS_THRESHOLD:
        return "LOSS"
    return "TIE/INCONCLUSIVE"


def _validate_thresholds(payload: dict[str, Any], source: pathlib.Path) -> None:
    thresholds = payload.get("thresholds")
    if not isinstance(thresholds, dict):
        raise ValueError(f"{source}: missing thresholds")
    if float(thresholds.get("win_ci_lower_gt", -1.0)) != WIN_THRESHOLD:
        raise ValueError(f"{source}: unexpected WIN threshold")
    if float(thresholds.get("loss_ci_upper_lt", -1.0)) != LOSS_THRESHOLD:
        raise ValueError(f"{source}: unexpected LOSS threshold")


def _run_id_from_path(path: pathlib.PurePath) -> int | None:
    for part in path.parts:
        if part.startswith("run-") and part[4:].isdigit():
            return int(part[4:])
    return None


def _load_rows(evidence_root: pathlib.Path) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    rows: list[dict[str, Any]] = []
    campaigns: list[dict[str, Any]] = []

    manifests = sorted(evidence_root.glob("*/*/manifest.json")) if evidence_root.exists() else []
    for manifest_path in manifests:
        campaign_root = manifest_path.parent
        errors = VERIFIER.verify_campaign(campaign_root)
        if errors:
            joined = "; ".join(errors)
            raise ValueError(f"invalid frozen campaign {campaign_root}: {joined}")

        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        campaign_date = str(manifest["campaign_date"])
        source_commit = str(manifest["source_commit"])
        summary_count = 0
        cell_count = 0

        for summary_path in sorted(campaign_root.rglob("summary.json")):
            payload = json.loads(summary_path.read_text(encoding="utf-8"))
            if payload.get("method") != EXPECTED_METHOD:
                continue
            _validate_thresholds(payload, summary_path)

            relative = summary_path.relative_to(campaign_root)
            if not relative.parts or relative.parts[0] in {"verification", "source-runs", "artifacts"}:
                raise ValueError(f"{summary_path}: benchmark summary is not under a platform directory")
            platform = relative.parts[0]
            run_id = _run_id_from_path(relative)
            comparisons = payload.get("comparisons")
            if not isinstance(comparisons, list):
                raise ValueError(f"{summary_path}: comparisons is not a list")

            summary_count += 1
            for item in comparisons:
                if not isinstance(item, dict):
                    raise ValueError(f"{summary_path}: comparison entry is not an object")
                _validate_thresholds(item, summary_path)
                ci_low = float(item["ci95_low"])
                ci_high = float(item["ci95_high"])
                verdict = str(item["classification"])
                expected = classify(ci_low, ci_high)
                if verdict != expected:
                    raise ValueError(
                        f"{summary_path}: verdict mismatch for {item.get('competitor')}: "
                        f"stored={verdict} expected={expected}"
                    )
                row = {
                    "campaign_date": campaign_date,
                    "source_commit": source_commit,
                    "platform": platform,
                    "run_id": run_id,
                    "evidence_path": summary_path.as_posix(),
                    "capacity": int(item["capacity"]),
                    "payload_bytes": int(item["payload_bytes"]),
                    "topology": str(item["topology"]),
                    "competitor": str(item["competitor"]),
                    "n": int(item["n"]),
                    "median_ratio": float(item["median_ratio"]),
                    "ci95_low": ci_low,
                    "ci95_high": ci_high,
                    "classification": verdict,
                }
                rows.append(row)
                cell_count += 1

        campaigns.append(
            {
                "campaign_date": campaign_date,
                "source_commit": source_commit,
                "summary_count": summary_count,
                "cell_count": cell_count,
            }
        )

    return rows, campaigns


def _aggregate(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, str], list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[(row["platform"], row["competitor"])].append(row)

    result = []
    for (platform, competitor), items in sorted(grouped.items()):
        counts = defaultdict(int)
        ratios = []
        for item in items:
            counts[item["classification"]] += 1
            ratios.append(float(item["median_ratio"]))
        result.append(
            {
                "platform": platform,
                "competitor": competitor,
                "cells": len(items),
                "wins": counts["WIN"],
                "ties": counts["TIE/INCONCLUSIVE"],
                "losses": counts["LOSS"],
                "median_of_cell_medians": statistics.median(ratios),
                "min_cell_median": min(ratios),
                "max_cell_median": max(ratios),
            }
        )
    return result


def render_scoreboard(evidence_root: pathlib.Path) -> str:
    rows, campaigns = _load_rows(evidence_root)
    lines = [
        "# VeriQueue Performance Scoreboard",
        "",
        "> Generated from checksum-valid frozen GitHub campaign evidence. Do not edit benchmark numbers by hand.",
        "",
        "Ratio is `VeriQueue throughput / competitor throughput` within the same measured repetition. "
        "The predeclared verdict rule is: **WIN** when the bootstrap 95% CI lower bound is `> 1.03`; "
        "**LOSS** when the upper bound is `< 0.97`; otherwise **TIE/INCONCLUSIVE**.",
        "",
        "This scoreboard summarizes only committed evidence under `evidence/campaigns/`. "
        "Absence of a platform or cell means **no frozen evidence**, not a win.",
        "",
    ]

    benchmark_campaigns = [campaign for campaign in campaigns if campaign["summary_count"] > 0]
    if not rows:
        lines.extend(
            [
                "## Current status",
                "",
                "No frozen paired benchmark campaign is committed yet.",
                "",
                "The scoreboard will populate automatically when an evidence-freeze PR commits paired benchmark summaries.",
                "",
            ]
        )
        return "\n".join(lines)

    lines.extend(
        [
            "## Frozen campaign inventory",
            "",
            "| Date | Source commit | Benchmark summaries | Comparison cells |",
            "|---|---|---:|---:|",
        ]
    )
    for campaign in benchmark_campaigns:
        lines.append(
            f"| {campaign['campaign_date']} | `{campaign['source_commit'][:12]}` | "
            f"{campaign['summary_count']} | {campaign['cell_count']} |"
        )

    lines.extend(
        [
            "",
            "## Platform / competitor scorecard",
            "",
            "The median column is a descriptive median of already-paired cell medians; it is not a replacement "
            "for the per-cell confidence-interval verdicts.",
            "",
            "| Platform | Competitor | Cells | WIN | TIE/INCONCLUSIVE | LOSS | Median cell ratio | Range |",
            "|---|---|---:|---:|---:|---:|---:|---|",
        ]
    )
    for item in _aggregate(rows):
        lines.append(
            f"| {item['platform']} | {item['competitor']} | {item['cells']} | {item['wins']} | "
            f"{item['ties']} | {item['losses']} | {item['median_of_cell_medians']:.4f} | "
            f"[{item['min_cell_median']:.4f}, {item['max_cell_median']:.4f}] |"
        )

    losses = sorted(
        (row for row in rows if row["classification"] == "LOSS"),
        key=lambda row: (
            row["platform"],
            row["competitor"],
            row["capacity"],
            row["payload_bytes"],
            row["topology"],
            row["run_id"] or -1,
        ),
    )
    lines.extend(["", "## Confirmed loss cells", ""])
    if not losses:
        lines.extend(["No frozen cell is currently classified LOSS.", ""])
    else:
        lines.extend(
            [
                "| Platform | Run | Capacity | Payload | Topology | Competitor | Median ratio | 95% CI | Evidence |",
                "|---|---:|---:|---:|---|---|---:|---|---|",
            ]
        )
        for row in losses:
            run = row["run_id"] if row["run_id"] is not None else "?"
            lines.append(
                f"| {row['platform']} | {run} | {row['capacity']} | {row['payload_bytes']} | "
                f"{row['topology']} | {row['competitor']} | {row['median_ratio']:.4f} | "
                f"[{row['ci95_low']:.4f}, {row['ci95_high']:.4f}] | `{row['evidence_path']}` |"
            )
        lines.append("")

    lines.extend(
        [
            "## Claim boundary",
            "",
            "The scoreboard supports bounded statements about the frozen GitHub-hosted matrix only. "
            "It does **not** support claims that VeriQueue is universally fastest, fastest on every topology, "
            "or superior outside the measured compiler/architecture/workload cells.",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence-root", type=pathlib.Path, default=pathlib.Path("evidence/campaigns"))
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("docs/PERFORMANCE_SCOREBOARD.md"))
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    try:
        rendered = render_scoreboard(args.evidence_root)
    except Exception as exc:
        print(f"scoreboard generation failed: {exc}", file=sys.stderr)
        return 1

    if args.check:
        if not args.output.exists():
            print(f"scoreboard is missing: {args.output}", file=sys.stderr)
            return 1
        current = args.output.read_text(encoding="utf-8")
        if current != rendered:
            print("performance scoreboard is stale; regenerate it", file=sys.stderr)
            return 1
        print("performance scoreboard: UP TO DATE")
        return 0

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(rendered, encoding="utf-8")
    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
