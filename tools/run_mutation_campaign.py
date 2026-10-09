#!/usr/bin/env python3
import json
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD_ROOT = ROOT / "build" / "mutation"
EVIDENCE = ROOT / "evidence" / "relacy" / "mutation-results.json"

results = []
for mutant in range(1, 9):
    build = BUILD_ROOT / f"mutant-{mutant}"
    shutil.rmtree(build, ignore_errors=True)
    configure = [
        "cmake", "-S", str(ROOT), "-B", str(build), "-G", "Ninja",
        "-DVERIQUEUE_BUILD_TESTS=OFF",
        "-DVERIQUEUE_BUILD_STRESS=OFF",
        "-DVERIQUEUE_BUILD_RELACY=ON",
        f"-DVERIQUEUE_RELACY_MUTATION_ID={mutant}",
        f"-DFETCHCONTENT_BASE_DIR={BUILD_ROOT / '_deps'}",
    ]
    subprocess.run(configure, check=True)
    subprocess.run(["cmake", "--build", str(build), "--target", "mutation_relacy", "-j2"], check=True)
    cp = subprocess.run([str(build / "mutation_relacy")], text=True, capture_output=True)
    killed = cp.returncode != 0
    results.append({"mutant": mutant, "killed": killed, "returncode": cp.returncode, "stdout": cp.stdout[-4000:], "stderr": cp.stderr[-4000:]})
    print(f"MUT-{mutant:02d}: {'KILLED' if killed else 'SURVIVED'}")

EVIDENCE.parent.mkdir(parents=True, exist_ok=True)
EVIDENCE.write_text(json.dumps({"results": results, "kill_rate": sum(r['killed'] for r in results) / len(results)}, indent=2) + "\n")
if not all(r["killed"] for r in results):
    sys.exit(1)
