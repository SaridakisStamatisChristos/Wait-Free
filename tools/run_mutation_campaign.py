#!/usr/bin/env python3
import csv
import json
import pathlib
import shutil
import subprocess
import sys
from collections import defaultdict

ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD_ROOT = ROOT / "build" / "mutation"
BUILD = BUILD_ROOT / "campaign"
EVIDENCE_DIR = ROOT / "evidence" / "relacy"
JSON_EVIDENCE = EVIDENCE_DIR / "mutation-results.json"
CSV_EVIDENCE = EVIDENCE_DIR / "mutation-matrix.csv"
MD_EVIDENCE = EVIDENCE_DIR / "mutation-summary.md"

MUTANTS = [
    (1, "tail-release-to-relaxed", "memory-order"),
    (2, "tail-acquire-to-relaxed", "memory-order"),
    (3, "head-release-to-relaxed", "memory-order"),
    (4, "head-acquire-to-relaxed", "memory-order"),
    (5, "publish-tail-before-slot-write", "publication-order"),
    (6, "publish-head-before-slot-read", "publication-order"),
    (7, "collapse-slot-mask-to-zero", "slot-mapping"),
    (8, "full-limit-capacity-plus-one", "capacity"),
    (9, "full-limit-capacity-minus-one", "capacity"),
    (10, "never-refresh-producer-cached-head", "cache-refresh"),
    (11, "never-refresh-consumer-cached-tail", "cache-refresh"),
    (12, "producer-cursor-double-increment", "cursor"),
    (13, "consumer-cursor-double-increment", "cursor"),
    (14, "producer-slot-plus-one", "slot-mapping"),
    (15, "consumer-slot-plus-one", "slot-mapping"),
    (16, "publish-tail-one-ahead", "publication-value"),
    (17, "publish-head-one-ahead", "publication-value"),
    (18, "corrupt-stored-value", "data-integrity"),
    (19, "suppress-producer-local-tail-update", "cursor"),
    (20, "suppress-consumer-local-head-update", "cursor"),
    (21, "invert-consumer-refresh-predicate", "cache-refresh"),
    (22, "omit-full-check", "capacity"),
    (23, "publish-old-head", "publication-value"),
    (24, "publish-old-tail", "publication-value"),
    (25, "bulk-publish-tail-before-slot-writes", "bulk-publication-order"),
    (26, "bulk-publish-head-before-slot-reads", "bulk-publication-order"),
    (27, "bulk-publish-only-first-tail-advance", "bulk-publication-value"),
    (28, "bulk-publish-only-first-head-advance", "bulk-publication-value"),
    (29, "bulk-overaccept-capacity-by-one", "bulk-capacity"),
    (30, "bulk-second-element-wrong-slot", "bulk-slot-mapping"),
    (31, "consume-publish-head-before-read", "consume-publication-order"),
    (32, "consume-read-next-slot", "consume-slot-mapping"),
]


def configure_build_run(mutant_id: int) -> subprocess.CompletedProcess[str]:
    configure = [
        "cmake", "-S", str(ROOT), "-B", str(BUILD), "-G", "Ninja",
        "-DVERIQUEUE_BUILD_TESTS=OFF",
        "-DVERIQUEUE_BUILD_STRESS=OFF",
        "-DVERIQUEUE_BUILD_RELACY=ON",
        f"-DVERIQUEUE_RELACY_MUTATION_ID={mutant_id}",
        f"-DFETCHCONTENT_BASE_DIR={BUILD_ROOT / '_deps'}",
    ]
    subprocess.run(configure, check=True)
    subprocess.run(["cmake", "--build", str(BUILD), "--target", "mutation_relacy", "-j2"], check=True)
    return subprocess.run([str(BUILD / "mutation_relacy")], text=True, capture_output=True)


shutil.rmtree(BUILD_ROOT, ignore_errors=True)
EVIDENCE_DIR.mkdir(parents=True, exist_ok=True)

baseline = configure_build_run(0)
if baseline.returncode != 0:
    JSON_EVIDENCE.write_text(json.dumps({
        "baseline_ok": False,
        "baseline_returncode": baseline.returncode,
        "baseline_stdout": baseline.stdout[-4000:],
        "baseline_stderr": baseline.stderr[-4000:],
        "results": [],
    }, indent=2) + "\n")
    print("MUT-00 baseline oracle failed; campaign is invalid", file=sys.stderr)
    sys.exit(1)

results = []
for mutant_id, name, fault_class in MUTANTS:
    cp = configure_build_run(mutant_id)
    killed = cp.returncode != 0
    result = {
        "mutant": mutant_id,
        "name": name,
        "fault_class": fault_class,
        "killed": killed,
        "returncode": cp.returncode,
        "stdout": cp.stdout[-4000:],
        "stderr": cp.stderr[-4000:],
    }
    results.append(result)
    print(f"MUT-{mutant_id:02d} [{fault_class}] {name}: {'KILLED' if killed else 'SURVIVED'}")

class_stats: dict[str, dict[str, int]] = defaultdict(lambda: {"total": 0, "killed": 0})
for result in results:
    stats = class_stats[result["fault_class"]]
    stats["total"] += 1
    stats["killed"] += int(result["killed"])

killed_count = sum(int(result["killed"]) for result in results)
payload = {
    "baseline_ok": True,
    "mutant_count": len(results),
    "killed_count": killed_count,
    "kill_rate": killed_count / len(results),
    "class_stats": dict(sorted(class_stats.items())),
    "results": results,
}
JSON_EVIDENCE.write_text(json.dumps(payload, indent=2) + "\n")

with CSV_EVIDENCE.open("w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow(["mutant", "name", "fault_class", "killed", "returncode"])
    for result in results:
        writer.writerow([
            f"MUT-{result['mutant']:02d}",
            result["name"],
            result["fault_class"],
            str(result["killed"]).lower(),
            result["returncode"],
        ])

lines = [
    "# Mutation campaign summary",
    "",
    f"Baseline oracle: **PASS**",
    f"Overall: **{killed_count}/{len(results)} killed ({100.0 * killed_count / len(results):.1f}%)**",
    "",
    "## Fault-class coverage",
    "",
    "| Fault class | Killed | Total |",
    "|---|---:|---:|",
]
for fault_class, stats in sorted(class_stats.items()):
    lines.append(f"| {fault_class} | {stats['killed']} | {stats['total']} |")
lines.extend([
    "",
    "## Mutants",
    "",
    "| ID | Fault class | Mutation | Result |",
    "|---|---|---|---|",
])
for result in results:
    lines.append(
        f"| MUT-{result['mutant']:02d} | {result['fault_class']} | "
        f"{result['name']} | {'KILLED' if result['killed'] else 'SURVIVED'} |"
    )
MD_EVIDENCE.write_text("\n".join(lines) + "\n")

if not all(result["killed"] for result in results):
    sys.exit(1)
