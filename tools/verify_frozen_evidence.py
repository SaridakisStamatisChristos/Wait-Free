#!/usr/bin/env python3
"""Verify a frozen VeriQueue GitHub evidence campaign."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import sys

SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _safe_relative(value: str) -> pathlib.PurePosixPath:
    path = pathlib.PurePosixPath(value)
    if path.is_absolute() or not path.parts:
        raise ValueError(f"unsafe checksum path: {value!r}")
    if any(part in {"", ".", ".."} for part in path.parts):
        raise ValueError(f"unsafe checksum path: {value!r}")
    return path


def read_checksums(path: pathlib.Path) -> dict[str, str]:
    entries: dict[str, str] = {}
    for line_number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not raw:
            continue
        if "  " not in raw:
            raise ValueError(f"invalid checksum line {line_number}")
        digest, relative = raw.split("  ", 1)
        if not SHA256_RE.fullmatch(digest):
            raise ValueError(f"invalid SHA-256 on line {line_number}")
        normalized = _safe_relative(relative).as_posix()
        if normalized in entries:
            raise ValueError(f"duplicate checksum entry: {normalized}")
        entries[normalized] = digest
    if not entries:
        raise ValueError("checksum ledger is empty")
    return entries


def verify_campaign(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    manifest_path = root / "manifest.json"
    checksum_path = root / "SHA256SUMS.txt"
    if not manifest_path.is_file():
        return ["manifest.json is missing"]
    if not checksum_path.is_file():
        return ["SHA256SUMS.txt is missing"]

    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return [f"manifest.json is invalid: {exc}"]

    if manifest.get("schema_version") != 1:
        errors.append("unsupported or missing manifest schema_version")
    source_commit = manifest.get("source_commit")
    if not isinstance(source_commit, str) or not re.fullmatch(r"[0-9a-f]{40}", source_commit):
        errors.append("manifest source_commit is not a 40-character lowercase Git SHA")
    if root.name != source_commit:
        errors.append("campaign directory name does not match manifest source_commit")
    campaign_date = manifest.get("campaign_date")
    if not isinstance(campaign_date, str) or not re.fullmatch(r"\d{4}-\d{2}-\d{2}", campaign_date):
        errors.append("manifest campaign_date is invalid")
    if root.parent.name != campaign_date:
        errors.append("campaign parent directory does not match manifest campaign_date")

    run_ids = manifest.get("source_run_ids")
    if not isinstance(run_ids, list) or not run_ids or not all(isinstance(item, int) for item in run_ids):
        errors.append("manifest source_run_ids must be a non-empty integer list")

    try:
        checksums = read_checksums(checksum_path)
    except (OSError, ValueError) as exc:
        return errors + [f"checksum ledger is invalid: {exc}"]

    actual_files = {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_file() and path != checksum_path
    }
    expected_files = set(checksums)
    missing = sorted(expected_files - actual_files)
    extra = sorted(actual_files - expected_files)
    if missing:
        errors.append(f"checksum ledger references missing files: {missing}")
    if extra:
        errors.append(f"files missing from checksum ledger: {extra}")

    for relative, expected in sorted(checksums.items()):
        path = root / pathlib.Path(*pathlib.PurePosixPath(relative).parts)
        if path.is_file():
            actual = sha256_file(path)
            if actual != expected:
                errors.append(f"SHA-256 mismatch: {relative}")

    manifest_files = manifest.get("files")
    if not isinstance(manifest_files, list):
        errors.append("manifest files field is not a list")
    else:
        seen: set[str] = set()
        for entry in manifest_files:
            if not isinstance(entry, dict):
                errors.append("manifest files contains a non-object entry")
                continue
            relative = entry.get("path")
            digest = entry.get("sha256")
            if not isinstance(relative, str) or not isinstance(digest, str):
                errors.append("manifest file entry lacks path/sha256")
                continue
            try:
                normalized = _safe_relative(relative).as_posix()
            except ValueError as exc:
                errors.append(str(exc))
                continue
            if normalized in seen:
                errors.append(f"duplicate manifest file provenance: {normalized}")
                continue
            seen.add(normalized)
            ledger_digest = checksums.get(normalized)
            if ledger_digest is None:
                errors.append(f"manifest payload missing from checksum ledger: {normalized}")
            elif digest != ledger_digest:
                errors.append(f"manifest/ledger digest disagreement: {normalized}")
            if not isinstance(entry.get("source_run_id"), int):
                errors.append(f"manifest provenance lacks source_run_id: {normalized}")
            if not isinstance(entry.get("source_artifact_id"), int):
                errors.append(f"manifest provenance lacks source_artifact_id: {normalized}")

    source_runs = manifest.get("source_runs")
    if not isinstance(source_runs, list) or not source_runs:
        errors.append("manifest source_runs is empty or invalid")
    else:
        for run in source_runs:
            if not isinstance(run, dict):
                errors.append("manifest source_runs contains a non-object entry")
                continue
            if run.get("conclusion") != "success":
                errors.append(f"source run is not successful: {run.get('id')}")
            if run.get("head_sha") != source_commit:
                errors.append(f"source run commit mismatch: {run.get('id')}")
            artifacts = run.get("artifacts")
            if not isinstance(artifacts, list) or not artifacts:
                errors.append(f"source run has no artifact metadata: {run.get('id')}")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign_root", type=pathlib.Path)
    args = parser.parse_args()

    errors = verify_campaign(args.campaign_root)
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print(f"frozen evidence verification: PASS ({args.campaign_root})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
