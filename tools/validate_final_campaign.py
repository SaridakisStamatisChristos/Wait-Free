#!/usr/bin/env python3
from __future__ import annotations

import argparse
import importlib.util
import io
import json
import pathlib
import sys
import zipfile
from typing import Any

TOOLS = pathlib.Path(__file__).resolve().parent


def load_freezer():
    path = TOOLS / "freeze_github_evidence.py"
    spec = importlib.util.spec_from_file_location("freeze_github_evidence", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load GitHub evidence freezer")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


FREEZER = load_freezer()

REQUIRED_QUALIFICATION = {
    "CI",
    "Sanitizers",
    "Fuzz Smoke",
    "Model Check",
    "Portability Qualification",
    "Performance Regression Gate",
}
PERFORMANCE_WORKFLOW = "Final Performance Campaign"
EXPECTED_PERFORMANCE_ARTIFACTS = {
    "hosted-performance-linux-x64-gcc",
    "hosted-performance-linux-x64-clang",
    "hosted-performance-linux-arm64-gcc",
    "hosted-performance-linux-arm64-clang",
}
EXPECTED_CAPACITIES = [2, 64, 256, 1024, 65536]
EXPECTED_PAYLOADS = [8, 16, 64, 256]


def parse_run_ids(value: str) -> list[int]:
    run_ids: list[int] = []
    seen: set[int] = set()
    for part in value.split(","):
        part = part.strip()
        if not part:
            continue
        run_id = int(part)
        if run_id <= 0:
            raise ValueError("run IDs must be positive")
        if run_id in seen:
            raise ValueError(f"duplicate run ID: {run_id}")
        seen.add(run_id)
        run_ids.append(run_id)
    if not run_ids:
        raise ValueError("at least one run ID is required")
    return run_ids


def validate_protocol(protocol: dict[str, Any]) -> int:
    if protocol.get("mode") != "full":
        raise ValueError(f"final performance artifact is not full mode: {protocol.get('mode')!r}")
    if protocol.get("capacities") != EXPECTED_CAPACITIES:
        raise ValueError(f"unexpected final capacities: {protocol.get('capacities')}")
    if protocol.get("payload_bytes") != EXPECTED_PAYLOADS:
        raise ValueError(f"unexpected final payload matrix: {protocol.get('payload_bytes')}")
    if int(protocol.get("transfers", -1)) != 100_000:
        raise ValueError(f"unexpected final transfer count: {protocol.get('transfers')}")
    if int(protocol.get("warmups", -1)) != 5:
        raise ValueError(f"unexpected final warmup count: {protocol.get('warmups')}")
    if int(protocol.get("repetitions", -1)) != 30:
        raise ValueError(f"unexpected final measured repetition count: {protocol.get('repetitions')}")
    if float(protocol.get("win_ci_lower_gt", -1.0)) != 1.03:
        raise ValueError("unexpected final WIN threshold")
    if float(protocol.get("loss_ci_upper_lt", -1.0)) != 0.97:
        raise ValueError("unexpected final LOSS threshold")

    run_index = int(protocol.get("run_index", -1))
    if run_index not in {1, 2, 3, 4, 5}:
        raise ValueError(f"invalid final run_index: {run_index}")
    expected_seed = 20261009 + run_index
    if int(protocol.get("campaign_seed", -1)) != expected_seed:
        raise ValueError(f"unexpected campaign seed for run_index={run_index}")
    if int(protocol.get("bootstrap_seed", -1)) != expected_seed:
        raise ValueError(f"unexpected bootstrap seed for run_index={run_index}")
    return run_index


def validate_campaign_metadata(
    repository: str,
    runs: list[dict[str, Any]],
    artifacts_by_run: dict[int, list[dict[str, Any]]],
    protocols_by_run: dict[int, list[dict[str, Any]]],
) -> dict[str, Any]:
    if not runs:
        raise ValueError("no campaign runs supplied")

    run_ids = [int(run["id"]) for run in runs]
    if len(run_ids) != len(set(run_ids)):
        raise ValueError("campaign contains duplicate run metadata")

    source_shas = {str(run.get("head_sha", "")) for run in runs}
    if "" in source_shas or len(source_shas) != 1:
        raise ValueError(f"all final campaign runs must share one source SHA: {sorted(source_shas)}")
    source_sha = next(iter(source_shas))

    workflow_runs: dict[str, list[int]] = {}
    for run in runs:
        run_id = int(run["id"])
        repo_name = ((run.get("repository") or {}).get("full_name"))
        if repo_name and repo_name.lower() != repository.lower():
            raise ValueError(f"run {run_id} belongs to {repo_name}, not {repository}")
        if run.get("conclusion") != "success":
            raise ValueError(f"run {run_id} is not successful: {run.get('conclusion')}")
        artifacts = artifacts_by_run.get(run_id, [])
        if not artifacts:
            raise ValueError(f"run {run_id} has no preserved artifacts")
        if any(artifact.get("expired") for artifact in artifacts):
            raise ValueError(f"run {run_id} contains an expired artifact")
        workflow_runs.setdefault(str(run.get("name", "")), []).append(run_id)

    missing = sorted(REQUIRED_QUALIFICATION - set(workflow_runs))
    if missing:
        raise ValueError(f"missing required qualification workflow classes: {missing}")

    performance_runs = [run for run in runs if run.get("name") == PERFORMANCE_WORKFLOW]
    if not 3 <= len(performance_runs) <= 5:
        raise ValueError(
            f"final campaign requires 3-5 {PERFORMANCE_WORKFLOW!r} runs, got {len(performance_runs)}"
        )

    final_run_indices: set[int] = set()
    performance_report = []
    for run in performance_runs:
        run_id = int(run["id"])
        if run.get("event") != "workflow_dispatch":
            raise ValueError(f"performance run {run_id} is not a workflow_dispatch full campaign")

        names = {str(artifact.get("name", "")) for artifact in artifacts_by_run[run_id]}
        missing_artifacts = sorted(EXPECTED_PERFORMANCE_ARTIFACTS - names)
        if missing_artifacts:
            raise ValueError(f"performance run {run_id} missing platform artifacts: {missing_artifacts}")

        protocols = protocols_by_run.get(run_id, [])
        if len(protocols) != len(EXPECTED_PERFORMANCE_ARTIFACTS):
            raise ValueError(
                f"performance run {run_id} requires one protocol per platform artifact, got {len(protocols)}"
            )
        indices = {validate_protocol(protocol) for protocol in protocols}
        if len(indices) != 1:
            raise ValueError(f"performance run {run_id} has inconsistent run_index metadata: {sorted(indices)}")
        run_index = next(iter(indices))
        if run_index in final_run_indices:
            raise ValueError(f"duplicate final performance run_index: {run_index}")
        final_run_indices.add(run_index)
        performance_report.append(
            {
                "run_id": run_id,
                "run_index": run_index,
                "artifact_names": sorted(EXPECTED_PERFORMANCE_ARTIFACTS),
            }
        )

    return {
        "schema_version": 1,
        "repository": repository,
        "source_commit": source_sha,
        "required_qualification_workflows": sorted(REQUIRED_QUALIFICATION),
        "qualification_runs": {
            name: sorted(workflow_runs[name]) for name in sorted(REQUIRED_QUALIFICATION)
        },
        "performance_workflow": PERFORMANCE_WORKFLOW,
        "performance_runs": sorted(performance_report, key=lambda item: item["run_index"]),
        "selected_run_ids": sorted(run_ids),
        "status": "PASS",
    }


def extract_protocol(archive_bytes: bytes) -> dict[str, Any]:
    with zipfile.ZipFile(io.BytesIO(archive_bytes)) as archive:
        candidates = [
            info for info in archive.infolist()
            if not info.is_dir() and pathlib.PurePosixPath(info.filename).name == "protocol.json"
        ]
        if len(candidates) != 1:
            raise ValueError(f"expected exactly one protocol.json in performance artifact, got {len(candidates)}")
        with archive.open(candidates[0]) as handle:
            return json.loads(handle.read().decode("utf-8"))


def collect_from_github(repository: str, run_ids: list[int], token: str):
    runs = []
    artifacts_by_run: dict[int, list[dict[str, Any]]] = {}
    protocols_by_run: dict[int, list[dict[str, Any]]] = {}

    for run_id in run_ids:
        run = FREEZER._api_json(repository, f"/actions/runs/{run_id}", token)
        runs.append(run)
        artifacts = FREEZER._list_artifacts(repository, run_id, token)
        artifacts_by_run[run_id] = artifacts
        if run.get("name") != PERFORMANCE_WORKFLOW:
            continue

        protocols = []
        artifact_by_name = {str(artifact.get("name", "")): artifact for artifact in artifacts}
        for name in sorted(EXPECTED_PERFORMANCE_ARTIFACTS):
            artifact = artifact_by_name.get(name)
            if artifact is None:
                continue
            archive_url = str(artifact.get("archive_download_url", ""))
            if not archive_url:
                raise ValueError(f"artifact {name} from run {run_id} has no download URL")
            archive_bytes = FREEZER._api_bytes(archive_url, token)
            protocols.append(extract_protocol(archive_bytes))
        protocols_by_run[run_id] = protocols
    return runs, artifacts_by_run, protocols_by_run


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate a complete VeriQueue final GitHub campaign")
    parser.add_argument("--repository", required=True)
    parser.add_argument("--run-ids", required=True)
    parser.add_argument("--token", required=True)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()

    try:
        run_ids = parse_run_ids(args.run_ids)
        runs, artifacts, protocols = collect_from_github(args.repository, run_ids, args.token)
        report = validate_campaign_metadata(args.repository, runs, artifacts, protocols)
    except Exception as exc:
        print(f"final campaign validation failed: {exc}", file=sys.stderr)
        return 1

    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    print(encoded, end="")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
