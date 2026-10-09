#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import importlib.util
import json
import pathlib
import tempfile
import unittest

TOOLS = pathlib.Path(__file__).resolve().parent


def load_generator():
    path = TOOLS / "generate_performance_scoreboard.py"
    spec = importlib.util.spec_from_file_location("generate_performance_scoreboard", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load scoreboard generator")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


scoreboard = load_generator()


def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_checksums(root: pathlib.Path) -> None:
    ledger = root / "SHA256SUMS.txt"
    lines = []
    for path in sorted(p for p in root.rglob("*") if p.is_file() and p != ledger):
        lines.append(f"{sha256(path)}  {path.relative_to(root).as_posix()}")
    ledger.write_text("\n".join(lines) + "\n", encoding="utf-8")


def make_campaign(base: pathlib.Path, classification: str = "LOSS") -> pathlib.Path:
    commit = "a" * 40
    root = base / "2026-10-09" / commit
    summary = root / "linux-x64-gcc" / "run-123" / "summary.json"
    summary.parent.mkdir(parents=True)

    if classification == "LOSS":
        ci_low, ci_high, median = 0.80, 0.90, 0.85
    elif classification == "WIN":
        ci_low, ci_high, median = 1.10, 1.20, 1.15
    else:
        ci_low, ci_high, median = 0.99, 1.02, 1.00

    payload = {
        "method": "paired_veriqueue_throughput_ratio",
        "bootstrap_samples": 10000,
        "bootstrap_seed": 20261009,
        "thresholds": {
            "win_ci_lower_gt": 1.03,
            "loss_ci_upper_lt": 0.97,
        },
        "comparisons": [
            {
                "capacity": 1024,
                "payload_bytes": 8,
                "topology": "smt-siblings",
                "producer_cpu": 0,
                "consumer_cpu": 1,
                "competitor": "rigtorp",
                "n": 30,
                "median_ratio": median,
                "iqr_low": median - 0.01,
                "iqr_high": median + 0.01,
                "p05": median - 0.02,
                "p95": median + 0.02,
                "cv": 0.02,
                "ci95_low": ci_low,
                "ci95_high": ci_high,
                "classification": classification,
                "thresholds": {
                    "win_ci_lower_gt": 1.03,
                    "loss_ci_upper_lt": 0.97,
                },
            }
        ],
    }
    summary.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    run = {
        "id": 123,
        "name": "Hosted Performance Matrix",
        "head_sha": commit,
        "conclusion": "success",
        "artifacts": [{"id": 456, "name": "hosted-performance-linux-x64-gcc"}],
    }
    source_run = root / "source-runs" / "hosted-performance-matrix-123.json"
    source_run.parent.mkdir(parents=True)
    source_run.write_text(json.dumps(run, indent=2) + "\n", encoding="utf-8")

    manifest = {
        "schema_version": 1,
        "repository": "owner/repo",
        "campaign_date": "2026-10-09",
        "source_commit": commit,
        "source_run_ids": [123],
        "source_runs": [run],
        "files": [
            {
                "path": summary.relative_to(root).as_posix(),
                "sha256": sha256(summary),
                "size_bytes": summary.stat().st_size,
                "source_run_id": 123,
                "source_artifact_id": 456,
                "source_artifact_name": "hosted-performance-linux-x64-gcc",
            }
        ],
    }
    (root / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    write_checksums(root)
    return root


class PerformanceScoreboardTests(unittest.TestCase):
    def test_no_frozen_evidence_is_explicit(self):
        with tempfile.TemporaryDirectory() as temporary:
            rendered = scoreboard.render_scoreboard(pathlib.Path(temporary))
            self.assertIn("No frozen paired benchmark campaign is committed yet.", rendered)
            self.assertIn("no frozen evidence", rendered.lower())

    def test_loss_cell_is_never_hidden(self):
        with tempfile.TemporaryDirectory() as temporary:
            evidence = pathlib.Path(temporary) / "campaigns"
            make_campaign(evidence, "LOSS")
            rendered = scoreboard.render_scoreboard(evidence)
            self.assertIn("| linux-x64-gcc | rigtorp | 1 | 0 | 0 | 1 |", rendered)
            self.assertIn("## Confirmed loss cells", rendered)
            self.assertIn("smt-siblings", rendered)
            self.assertIn("[0.8000, 0.9000]", rendered)

    def test_stored_verdict_must_match_predeclared_thresholds(self):
        with tempfile.TemporaryDirectory() as temporary:
            evidence = pathlib.Path(temporary) / "campaigns"
            root = make_campaign(evidence, "WIN")
            summary = root / "linux-x64-gcc" / "run-123" / "summary.json"
            payload = json.loads(summary.read_text(encoding="utf-8"))
            payload["comparisons"][0]["classification"] = "LOSS"
            summary.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

            manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
            manifest["files"][0]["sha256"] = sha256(summary)
            manifest["files"][0]["size_bytes"] = summary.stat().st_size
            (root / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
            write_checksums(root)

            with self.assertRaisesRegex(ValueError, "verdict mismatch"):
                scoreboard.render_scoreboard(evidence)


if __name__ == "__main__":
    unittest.main()
