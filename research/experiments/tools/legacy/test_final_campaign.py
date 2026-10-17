#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import pathlib
import unittest

TOOLS = pathlib.Path(__file__).resolve().parent


def load_validator():
    path = TOOLS / "validate_final_campaign.py"
    spec = importlib.util.spec_from_file_location("validate_final_campaign", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load final campaign validator")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


validator = load_validator()


def protocol(run_index: int):
    seed = 20261009 + run_index
    return {
        "mode": "full",
        "run_index": run_index,
        "capacities": [2, 64, 256, 1024, 65536],
        "payload_bytes": [8, 16, 64, 256],
        "transfers": 100000,
        "warmups": 5,
        "repetitions": 30,
        "campaign_seed": seed,
        "bootstrap_seed": seed,
        "win_ci_lower_gt": 1.03,
        "loss_ci_upper_lt": 0.97,
    }


def build_fixture(performance_count: int = 3, performance_event: str = "workflow_dispatch"):
    repo = "owner/repo"
    sha = "a" * 40
    runs = []
    artifacts = {}
    protocols = {}
    next_id = 100

    for name in sorted(validator.REQUIRED_QUALIFICATION):
        run = {
            "id": next_id,
            "name": name,
            "head_sha": sha,
            "conclusion": "success",
            "event": "pull_request",
            "repository": {"full_name": repo},
        }
        runs.append(run)
        artifacts[next_id] = [{"id": next_id + 1000, "name": f"{name}-evidence", "expired": False}]
        next_id += 1

    for run_index in range(1, performance_count + 1):
        run = {
            "id": next_id,
            "name": validator.PERFORMANCE_WORKFLOW,
            "head_sha": sha,
            "head_branch": (
                f"final-campaign/run-{run_index}" if performance_event == "create" else "main"
            ),
            "conclusion": "success",
            "event": performance_event,
            "repository": {"full_name": repo},
        }
        runs.append(run)
        artifacts[next_id] = [
            {"id": next_id * 10 + offset, "name": name, "expired": False}
            for offset, name in enumerate(sorted(validator.EXPECTED_PERFORMANCE_ARTIFACTS))
        ]
        protocols[next_id] = [protocol(run_index) for _ in validator.EXPECTED_PERFORMANCE_ARTIFACTS]
        next_id += 1
    return repo, runs, artifacts, protocols


class FinalCampaignTests(unittest.TestCase):
    def test_valid_three_run_campaign_passes(self):
        repo, runs, artifacts, protocols = build_fixture(3)
        report = validator.validate_campaign_metadata(repo, runs, artifacts, protocols)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual([item["run_index"] for item in report["performance_runs"]], [1, 2, 3])

    def test_valid_create_triggered_campaign_passes(self):
        repo, runs, artifacts, protocols = build_fixture(3, "create")
        report = validator.validate_campaign_metadata(repo, runs, artifacts, protocols)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual([item["event"] for item in report["performance_runs"]], ["create"] * 3)

    def test_requires_every_qualification_class(self):
        repo, runs, artifacts, protocols = build_fixture(3)
        removed = next(run for run in runs if run["name"] == "Sanitizers")
        runs.remove(removed)
        artifacts.pop(removed["id"])
        with self.assertRaisesRegex(ValueError, "missing required qualification"):
            validator.validate_campaign_metadata(repo, runs, artifacts, protocols)

    def test_rejects_mixed_source_sha(self):
        repo, runs, artifacts, protocols = build_fixture(3)
        runs[-1]["head_sha"] = "b" * 40
        with self.assertRaisesRegex(ValueError, "share one source SHA"):
            validator.validate_campaign_metadata(repo, runs, artifacts, protocols)

    def test_rejects_fewer_than_three_performance_runs(self):
        repo, runs, artifacts, protocols = build_fixture(2)
        with self.assertRaisesRegex(ValueError, "requires 3-5"):
            validator.validate_campaign_metadata(repo, runs, artifacts, protocols)

    def test_rejects_incomplete_platform_artifacts(self):
        repo, runs, artifacts, protocols = build_fixture(3)
        performance = next(run for run in runs if run["name"] == validator.PERFORMANCE_WORKFLOW)
        artifacts[performance["id"]].pop()
        with self.assertRaisesRegex(ValueError, "missing platform artifacts"):
            validator.validate_campaign_metadata(repo, runs, artifacts, protocols)

    def test_rejects_duplicate_run_index(self):
        repo, runs, artifacts, protocols = build_fixture(3)
        performance_runs = [run for run in runs if run["name"] == validator.PERFORMANCE_WORKFLOW]
        protocols[performance_runs[1]["id"]] = [protocol(1) for _ in validator.EXPECTED_PERFORMANCE_ARTIFACTS]
        with self.assertRaisesRegex(ValueError, "duplicate final performance run_index"):
            validator.validate_campaign_metadata(repo, runs, artifacts, protocols)

    def test_rejects_smoke_protocol(self):
        bad = protocol(1)
        bad["mode"] = "pull-request-smoke"
        with self.assertRaisesRegex(ValueError, "not full mode"):
            validator.validate_protocol(bad)

    def test_rejects_posthoc_threshold_change(self):
        bad = protocol(1)
        bad["win_ci_lower_gt"] = 1.01
        with self.assertRaisesRegex(ValueError, "WIN threshold"):
            validator.validate_protocol(bad)

    def test_rejects_create_trigger_with_wrong_branch(self):
        repo, runs, artifacts, protocols = build_fixture(3, "create")
        performance = next(run for run in runs if run["name"] == validator.PERFORMANCE_WORKFLOW)
        performance["head_branch"] = "final-campaign/run-5"
        with self.assertRaisesRegex(ValueError, "must originate"):
            validator.validate_campaign_metadata(repo, runs, artifacts, protocols)

    def test_rejects_unapproved_full_event(self):
        repo, runs, artifacts, protocols = build_fixture(3)
        performance = next(run for run in runs if run["name"] == validator.PERFORMANCE_WORKFLOW)
        performance["event"] = "push"
        with self.assertRaisesRegex(ValueError, "unsupported full-campaign event"):
            validator.validate_campaign_metadata(repo, runs, artifacts, protocols)


if __name__ == "__main__":
    unittest.main()
