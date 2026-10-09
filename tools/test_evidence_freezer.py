#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import importlib.util
import json
import pathlib
import stat
import tempfile
import unittest
import zipfile

TOOLS = pathlib.Path(__file__).resolve().parent


def load_module(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, TOOLS / filename)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {filename}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


freezer = load_module("freeze_github_evidence", "freeze_github_evidence.py")
verifier = load_module("verify_frozen_evidence", "verify_frozen_evidence.py")


class EvidenceFreezerTests(unittest.TestCase):
    def test_artifact_destination_normalizes_platform_evidence(self):
        self.assertEqual(
            freezer.artifact_destination("hosted-performance-linux-x64-gcc").as_posix(),
            "linux-x64-gcc",
        )
        self.assertEqual(
            freezer.artifact_destination("portability-windows-arm64-msvc").as_posix(),
            "windows-arm64-msvc",
        )
        self.assertEqual(
            freezer.artifact_destination("mutation-evidence").as_posix(),
            "verification/mutation",
        )

    def test_safe_extract_accepts_regular_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            archive = root / "artifact.zip"
            with zipfile.ZipFile(archive, "w") as handle:
                handle.writestr("nested/raw.jsonl", "{}\n")
            destination = root / "out"
            extracted = freezer.safe_extract_zip(archive, destination)
            self.assertEqual(len(extracted), 1)
            self.assertEqual((destination / "nested/raw.jsonl").read_text(), "{}\n")

    def test_safe_extract_rejects_path_traversal(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            archive = root / "artifact.zip"
            with zipfile.ZipFile(archive, "w") as handle:
                handle.writestr("../escape.txt", "bad")
            with self.assertRaises(ValueError):
                freezer.safe_extract_zip(archive, root / "out")

    def test_safe_extract_rejects_symlink(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            archive = root / "artifact.zip"
            info = zipfile.ZipInfo("link")
            info.create_system = 3
            info.external_attr = (stat.S_IFLNK | 0o777) << 16
            with zipfile.ZipFile(archive, "w") as handle:
                handle.writestr(info, "target")
            with self.assertRaises(ValueError):
                freezer.safe_extract_zip(archive, root / "out")

    def test_verifier_detects_tampering(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = pathlib.Path(temporary)
            sha = "a" * 40
            root = base / "2026-10-09" / sha
            payload = root / "linux-x64-gcc" / "raw.jsonl"
            payload.parent.mkdir(parents=True)
            payload.write_text("{}\n", encoding="utf-8")

            run = {
                "id": 123,
                "name": "Hosted Performance Matrix",
                "head_sha": sha,
                "conclusion": "success",
                "artifacts": [{"id": 456, "name": "hosted-performance-linux-x64-gcc"}],
            }
            source_path = root / "source-runs" / "hosted-performance-matrix-123.json"
            source_path.parent.mkdir(parents=True)
            source_path.write_text(json.dumps(run) + "\n", encoding="utf-8")

            payload_hash = hashlib.sha256(payload.read_bytes()).hexdigest()
            manifest = {
                "schema_version": 1,
                "repository": "owner/repo",
                "campaign_date": "2026-10-09",
                "source_commit": sha,
                "source_run_ids": [123],
                "source_runs": [run],
                "files": [
                    {
                        "path": "linux-x64-gcc/raw.jsonl",
                        "sha256": payload_hash,
                        "size_bytes": payload.stat().st_size,
                        "source_run_id": 123,
                        "source_artifact_id": 456,
                        "source_artifact_name": "hosted-performance-linux-x64-gcc",
                    }
                ],
            }
            (root / "manifest.json").write_text(
                json.dumps(manifest, indent=2) + "\n",
                encoding="utf-8",
            )
            freezer._write_checksums(root)

            self.assertEqual(verifier.verify_campaign(root), [])
            payload.write_text("tampered\n", encoding="utf-8")
            errors = verifier.verify_campaign(root)
            self.assertTrue(any("SHA-256 mismatch" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
