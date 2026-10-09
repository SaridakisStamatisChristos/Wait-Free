#!/usr/bin/env python3
"""Freeze GitHub Actions artifacts into a permanent, verifiable campaign tree."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import stat
import sys
import tempfile
import time
import urllib.error
import urllib.request
import zipfile

API_VERSION = "2022-11-28"
MAX_FILE_BYTES = 95 * 1024 * 1024
MAX_CAMPAIGN_BYTES = 2 * 1024 * 1024 * 1024


def _slug(value: str) -> str:
    value = value.strip().lower()
    value = re.sub(r"[^a-z0-9._-]+", "-", value)
    value = re.sub(r"-+", "-", value).strip("-.")
    if not value:
        raise ValueError("value cannot be normalized to an empty slug")
    return value


def artifact_destination(name: str) -> pathlib.PurePosixPath:
    for prefix in ("hosted-performance-", "portability-"):
        if name.startswith(prefix):
            return pathlib.PurePosixPath(_slug(name[len(prefix) :]))

    known = {
        "relacy-evidence": "verification/relacy",
        "mutation-evidence": "verification/mutation",
        "linearizability-evidence": "verification/linearizability",
        "fuzz-evidence": "verification/fuzz",
        "sanitizer-evidence": "verification/sanitizers",
    }
    if name in known:
        return pathlib.PurePosixPath(known[name])
    return pathlib.PurePosixPath("artifacts") / _slug(name)


def artifact_run_destination(name: str, run_id: int) -> pathlib.PurePosixPath:
    return artifact_destination(name) / f"run-{run_id}"


def _request(url: str, token: str, *, binary: bool) -> bytes | dict:
    headers = {
        "Accept": "application/vnd.github+json",
        "Authorization": f"Bearer {token}",
        "X-GitHub-Api-Version": API_VERSION,
        "User-Agent": "veriqueue-evidence-freezer",
    }
    request = urllib.request.Request(url, headers=headers)
    last_error: Exception | None = None
    for attempt in range(4):
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                payload = response.read()
                if binary:
                    return payload
                return json.loads(payload.decode("utf-8"))
        except urllib.error.HTTPError as exc:
            last_error = exc
            if exc.code not in {429, 500, 502, 503, 504} or attempt == 3:
                raise
        except urllib.error.URLError as exc:
            last_error = exc
            if attempt == 3:
                raise
        time.sleep(2**attempt)
    raise RuntimeError(f"request failed: {last_error}")


def _api_json(repository: str, suffix: str, token: str) -> dict:
    url = f"https://api.github.com/repos/{repository}{suffix}"
    result = _request(url, token, binary=False)
    if not isinstance(result, dict):
        raise TypeError(f"expected JSON object from {url}")
    return result


def _api_bytes(url: str, token: str) -> bytes:
    result = _request(url, token, binary=True)
    if not isinstance(result, bytes):
        raise TypeError(f"expected binary response from {url}")
    return result


def _list_artifacts(repository: str, run_id: int, token: str) -> list[dict]:
    artifacts: list[dict] = []
    page = 1
    while True:
        payload = _api_json(
            repository,
            f"/actions/runs/{run_id}/artifacts?per_page=100&page={page}",
            token,
        )
        batch = payload.get("artifacts", [])
        if not isinstance(batch, list):
            raise ValueError("GitHub artifact response has invalid shape")
        artifacts.extend(batch)
        total = int(payload.get("total_count", len(artifacts)))
        if not batch or len(artifacts) >= total:
            return artifacts
        page += 1


def _safe_member_path(name: str) -> pathlib.PurePosixPath:
    normalized = name.replace("\\", "/")
    path = pathlib.PurePosixPath(normalized)
    if path.is_absolute() or not path.parts:
        raise ValueError(f"unsafe archive member path: {name!r}")
    if any(part in {"", ".", ".."} for part in path.parts):
        raise ValueError(f"unsafe archive member path: {name!r}")
    if ":" in path.parts[0]:
        raise ValueError(f"unsafe archive member path: {name!r}")
    return path


def safe_extract_zip(
    archive_path: pathlib.Path,
    destination: pathlib.Path,
    *,
    max_file_bytes: int = MAX_FILE_BYTES,
    max_total_bytes: int = MAX_CAMPAIGN_BYTES,
) -> list[pathlib.Path]:
    destination.mkdir(parents=True, exist_ok=True)
    extracted: list[pathlib.Path] = []
    seen: set[pathlib.PurePosixPath] = set()
    total = 0

    with zipfile.ZipFile(archive_path) as archive:
        for info in archive.infolist():
            member = _safe_member_path(info.filename)
            if member in seen:
                raise ValueError(f"duplicate archive member: {member}")
            seen.add(member)

            unix_mode = (info.external_attr >> 16) & 0xFFFF
            if unix_mode and stat.S_ISLNK(unix_mode):
                raise ValueError(f"symbolic links are not accepted: {member}")
            if info.is_dir():
                (destination / pathlib.Path(*member.parts)).mkdir(parents=True, exist_ok=True)
                continue
            if info.file_size > max_file_bytes:
                raise ValueError(f"artifact file exceeds repository-safe limit: {member}")
            total += info.file_size
            if total > max_total_bytes:
                raise ValueError("artifact extraction exceeds per-artifact size limit")

            target = destination / pathlib.Path(*member.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(info) as source, target.open("wb") as sink:
                shutil.copyfileobj(source, sink)
            extracted.append(target)
    return extracted


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _tree_size(root: pathlib.Path) -> int:
    return sum(path.stat().st_size for path in root.rglob("*") if path.is_file())


def _selected_run_metadata(run: dict) -> dict:
    keys = (
        "id",
        "name",
        "workflow_id",
        "event",
        "head_branch",
        "head_sha",
        "run_number",
        "run_attempt",
        "status",
        "conclusion",
        "html_url",
        "created_at",
        "updated_at",
    )
    return {key: run.get(key) for key in keys}


def _artifact_metadata(artifact: dict, destination: str, archive_sha256: str) -> dict:
    keys = (
        "id",
        "name",
        "size_in_bytes",
        "expired",
        "created_at",
        "expires_at",
        "updated_at",
        "digest",
    )
    result = {key: artifact.get(key) for key in keys}
    result["destination"] = destination
    result["downloaded_archive_sha256"] = archive_sha256
    return result


def _parse_key_value_file(path: pathlib.Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "=" not in raw:
            continue
        key, value = raw.split("=", 1)
        key = key.strip()
        if key:
            result[key] = value.strip()
    return result


def _benchmark_context(root: pathlib.Path) -> dict:
    contexts: dict[str, dict] = {}
    for raw_path in sorted(root.rglob("raw-compare.jsonl")):
        relative = raw_path.relative_to(root)
        key_parts = list(relative.parts[:-1])
        key = "/".join(key_parts)
        records = []
        for line in raw_path.read_text(encoding="utf-8").splitlines():
            if line.strip():
                records.append(json.loads(line))
        if not records:
            continue

        environments = []
        seen_environments = set()
        for record in records:
            environment = record.get("environment")
            if isinstance(environment, dict):
                encoded = json.dumps(environment, sort_keys=True)
                if encoded not in seen_environments:
                    seen_environments.add(encoded)
                    environments.append(environment)

        context: dict[str, object] = {
            "raw_file": relative.as_posix(),
            "record_count": len(records),
            "campaign_seeds": sorted({record.get("campaign_seed") for record in records}),
            "capacities": sorted({record.get("capacity") for record in records}),
            "payload_bytes": sorted({record.get("payload_bytes") for record in records}),
            "transfers": sorted({record.get("transfers") for record in records}),
            "implementations": sorted({str(record.get("implementation")) for record in records}),
            "topologies": sorted({str(record.get("topology")) for record in records}),
            "environments": environments,
            "comparator_revisions": {
                "boost_version": records[0].get("boost_version"),
                "rigtorp_commit": records[0].get("rigtorp_commit"),
                "moodycamel_commit": records[0].get("moodycamel_commit"),
                "drogalis_commit": records[0].get("drogalis_commit"),
            },
            "stdlib": None,
            "compiler_flags": None,
            "runner_image": None,
        }

        run_contexts = list(raw_path.parent.rglob("run-context.txt"))
        if run_contexts:
            run_context = _parse_key_value_file(run_contexts[0])
            context["run_context_file"] = run_contexts[0].relative_to(root).as_posix()
            context["runner_image"] = {
                "os": run_context.get("runner_image_os"),
                "version": run_context.get("runner_image_version"),
            }
            context["stdlib"] = run_context.get("stdlib")
            context["compiler_flags"] = run_context.get("compiler_flags")

        compile_commands = list(raw_path.parent.rglob("compile_commands.json"))
        if compile_commands:
            context["compile_commands_file"] = compile_commands[0].relative_to(root).as_posix()

        protocol_files = list(raw_path.parent.rglob("protocol.json"))
        if protocol_files:
            context["protocol_file"] = protocol_files[0].relative_to(root).as_posix()

        contexts[key] = context
    return contexts


def _write_checksums(root: pathlib.Path) -> None:
    checksum_path = root / "SHA256SUMS.txt"
    files = sorted(path for path in root.rglob("*") if path.is_file() and path != checksum_path)
    lines = [f"{sha256_file(path)}  {path.relative_to(root).as_posix()}" for path in files]
    checksum_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def freeze_campaign(
    repository: str,
    run_ids: list[int],
    token: str,
    output_base: pathlib.Path,
    campaign_date: str | None,
) -> pathlib.Path:
    if not run_ids:
        raise ValueError("at least one run ID is required")

    runs = [_api_json(repository, f"/actions/runs/{run_id}", token) for run_id in run_ids]
    for run in runs:
        if run.get("conclusion") != "success":
            raise ValueError(f"run {run.get('id')} is not successful: {run.get('conclusion')}")
        run_repo = ((run.get("repository") or {}).get("full_name"))
        if run_repo and run_repo.lower() != repository.lower():
            raise ValueError(f"run {run.get('id')} belongs to {run_repo}, not {repository}")

    commits = {str(run.get("head_sha", "")) for run in runs}
    if "" in commits or len(commits) != 1:
        raise ValueError(f"source runs must share exactly one head SHA, got {sorted(commits)}")
    source_sha = next(iter(commits))

    if campaign_date is None:
        dates = sorted(str(run.get("created_at", ""))[:10] for run in runs)
        campaign_date = dates[0]
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", campaign_date or ""):
        raise ValueError("campaign date must use YYYY-MM-DD")

    campaign_root = output_base / campaign_date / source_sha
    if campaign_root.exists() and any(campaign_root.iterdir()):
        raise FileExistsError(f"campaign destination already exists: {campaign_root}")
    campaign_root.mkdir(parents=True, exist_ok=True)

    source_runs: list[dict] = []
    claimed_destinations: dict[str, str] = {}
    file_provenance: list[dict] = []

    try:
        for run in runs:
            run_id = int(run["id"])
            run_record = _selected_run_metadata(run)
            run_record["artifacts"] = []
            artifacts = _list_artifacts(repository, run_id, token)
            if not artifacts:
                raise ValueError(f"run {run_id} has no artifacts to freeze")

            for artifact in artifacts:
                if artifact.get("expired"):
                    raise ValueError(f"artifact {artifact.get('name')} from run {run_id} is expired")
                name = str(artifact.get("name", ""))
                destination_rel = artifact_run_destination(name, run_id)
                destination_key = destination_rel.as_posix()
                owner = claimed_destinations.get(destination_key)
                if owner is not None:
                    raise ValueError(
                        f"artifact destination collision: {name!r} and {owner!r} -> {destination_key}"
                    )
                claimed_destinations[destination_key] = name

                archive_url = str(artifact.get("archive_download_url", ""))
                if not archive_url:
                    raise ValueError(f"artifact {name!r} has no archive download URL")
                archive_bytes = _api_bytes(archive_url, token)
                archive_hash = hashlib.sha256(archive_bytes).hexdigest()
                expected_digest = artifact.get("digest")
                if isinstance(expected_digest, str) and expected_digest.startswith("sha256:"):
                    if archive_hash != expected_digest.removeprefix("sha256:"):
                        raise ValueError(f"artifact archive digest mismatch: {name}")

                destination = campaign_root / pathlib.Path(*destination_rel.parts)
                with tempfile.NamedTemporaryFile(suffix=".zip", delete=False) as temporary:
                    temporary.write(archive_bytes)
                    archive_path = pathlib.Path(temporary.name)
                try:
                    extracted = safe_extract_zip(archive_path, destination)
                finally:
                    archive_path.unlink(missing_ok=True)

                if _tree_size(campaign_root) > MAX_CAMPAIGN_BYTES:
                    raise ValueError("frozen campaign exceeds campaign size limit")

                for path in extracted:
                    file_provenance.append(
                        {
                            "path": path.relative_to(campaign_root).as_posix(),
                            "sha256": sha256_file(path),
                            "size_bytes": path.stat().st_size,
                            "source_run_id": run_id,
                            "source_artifact_id": artifact.get("id"),
                            "source_artifact_name": name,
                        }
                    )

                run_record["artifacts"].append(
                    _artifact_metadata(artifact, destination_key, archive_hash)
                )
            source_runs.append(run_record)

        metadata_dir = campaign_root / "source-runs"
        metadata_dir.mkdir(parents=True, exist_ok=True)
        for run in source_runs:
            workflow_slug = _slug(str(run.get("name") or "workflow"))
            path = metadata_dir / f"{workflow_slug}-{run['id']}.json"
            path.write_text(json.dumps(run, indent=2, sort_keys=True) + "\n", encoding="utf-8")

        benchmark_context = _benchmark_context(campaign_root)
        manifest = {
            "schema_version": 1,
            "repository": repository,
            "campaign_date": campaign_date,
            "veriqueue_commit": source_sha,
            "workflow_source_commit": source_sha,
            "source_commit": source_sha,
            "source_run_ids": run_ids,
            "source_runs": source_runs,
            "benchmark_context": benchmark_context,
            "files": sorted(file_provenance, key=lambda item: item["path"]),
            "hash_ledger": "SHA256SUMS.txt",
            "notes": [
                "All payload files were downloaded from GitHub Actions artifacts.",
                "Repeated workflow runs are retained independently under run-<id> directories.",
                "Hardware/compiler fields are extracted only when the source artifacts contain them.",
                "Unknown environment facts remain null; the freezer never fabricates missing metadata.",
                "SHA256SUMS.txt covers extracted payloads plus manifest/source-run metadata.",
            ],
        }
        (campaign_root / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        _write_checksums(campaign_root)
        return campaign_root
    except Exception:
        shutil.rmtree(campaign_root, ignore_errors=True)
        raise


def _parse_run_ids(value: str) -> list[int]:
    result: list[int] = []
    seen: set[int] = set()
    for token in value.split(","):
        token = token.strip()
        if not token:
            continue
        run_id = int(token)
        if run_id <= 0:
            raise ValueError("run IDs must be positive")
        if run_id not in seen:
            seen.add(run_id)
            result.append(run_id)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", required=True, help="owner/repository")
    parser.add_argument("--run-ids", required=True, help="comma-separated successful Actions run IDs")
    parser.add_argument("--output-base", type=pathlib.Path, default=pathlib.Path("evidence/campaigns"))
    parser.add_argument("--campaign-date", help="optional YYYY-MM-DD override")
    parser.add_argument("--token", default=os.environ.get("GITHUB_TOKEN"))
    parser.add_argument("--result-json", type=pathlib.Path)
    args = parser.parse_args()

    if not args.token:
        parser.error("GitHub token required via --token or GITHUB_TOKEN")

    try:
        run_ids = _parse_run_ids(args.run_ids)
        root = freeze_campaign(
            args.repository,
            run_ids,
            args.token,
            args.output_base,
            args.campaign_date,
        )
    except Exception as exc:
        print(f"evidence freeze failed: {exc}", file=sys.stderr)
        return 1

    result = {
        "campaign_root": root.as_posix(),
        "campaign_date": root.parent.name,
        "source_commit": root.name,
        "run_ids": run_ids,
    }
    print(json.dumps(result, sort_keys=True))
    if args.result_json:
        args.result_json.parent.mkdir(parents=True, exist_ok=True)
        args.result_json.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
