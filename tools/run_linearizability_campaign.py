#!/usr/bin/env python3
import argparse
import json
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_GENERATOR = ROOT / "build" / "lin" / "history_generator"
DEFAULT_CHECKER = ROOT / "build" / "lin" / "history_checker"
CORPUS = ROOT / "verify" / "histories" / "corpus"
EVIDENCE = ROOT / "evidence" / "linearizability"


def parse_int_list(text: str) -> list[int]:
    return [int(item) for item in text.split(",") if item]


def parse_str_list(text: str) -> list[str]:
    return [item for item in text.split(",") if item]


parser = argparse.ArgumentParser(description="Run deterministic Porcupine linearizability campaign")
parser.add_argument("--generator", type=pathlib.Path, default=DEFAULT_GENERATOR)
parser.add_argument("--checker", type=pathlib.Path, default=DEFAULT_CHECKER)
parser.add_argument("--capacities", default="1,2,4,8")
parser.add_argument("--profiles", default="uniform,burst,producer-heavy,consumer-heavy")
parser.add_argument("--seeds", type=int, default=8)
parser.add_argument("--ops-per-side", type=int, default=8)
args = parser.parse_args()

capacities = parse_int_list(args.capacities)
profiles = parse_str_list(args.profiles)
if args.seeds < 1 or args.ops_per_side < 1:
    parser.error("--seeds and --ops-per-side must be positive")

CORPUS.mkdir(parents=True, exist_ok=True)
EVIDENCE.mkdir(parents=True, exist_ok=True)
summary_log = EVIDENCE / "summary.log"
summary_json = EVIDENCE / "summary.json"

results: list[dict[str, object]] = []
counts = {"PASS": 0, "FAIL": 0, "UNKNOWN": 0, "ERROR": 0}
stop = False

with summary_log.open("w") as log:
    for capacity in capacities:
        if stop:
            break
        for profile in profiles:
            if stop:
                break
            for seed in range(1, args.seeds + 1):
                stem = f"c{capacity}-{profile}-o{args.ops_per_side}-s{seed}"
                history = CORPUS / f"{stem}.json"
                visualization = EVIDENCE / f"{stem}.html"

                generated = subprocess.run([
                    str(args.generator),
                    str(history),
                    str(seed),
                    str(capacity),
                    str(args.ops_per_side),
                    profile,
                ], text=True, capture_output=True)
                log.write(generated.stdout)
                log.write(generated.stderr)
                if generated.returncode != 0:
                    status = "ERROR"
                    checker_output = "history generation failed"
                else:
                    checked = subprocess.run([
                        str(args.checker), str(history)
                    ], text=True, capture_output=True)
                    checker_output = checked.stdout + checked.stderr
                    log.write(checker_output)
                    if "PASS " in checker_output and checked.returncode == 0:
                        status = "PASS"
                    elif "FAIL " in checker_output:
                        status = "FAIL"
                    elif "UNKNOWN " in checker_output:
                        status = "UNKNOWN"
                    else:
                        status = "ERROR"

                counts[status] += 1
                result = {
                    "capacity": capacity,
                    "profile": profile,
                    "seed": seed,
                    "ops_per_side": args.ops_per_side,
                    "status": status,
                    "history": str(history.relative_to(ROOT)),
                    "replay": f"{args.checker} {history}",
                }
                results.append(result)
                print(f"{status} capacity={capacity} profile={profile} seed={seed} ops_per_side={args.ops_per_side}")

                if status in {"FAIL", "UNKNOWN"}:
                    # Generate a visualization only for a non-PASS result. This keeps
                    # the ordinary campaign compact while preserving rich diagnostics.
                    visualized = subprocess.run([
                        str(args.checker), str(history), str(visualization)
                    ], text=True, capture_output=True)
                    log.write(visualized.stdout)
                    log.write(visualized.stderr)
                    if visualization.exists():
                        result["visualization"] = str(visualization.relative_to(ROOT))

                if status == "FAIL":
                    preserved = EVIDENCE / "counterexamples"
                    preserved.mkdir(parents=True, exist_ok=True)
                    copied = preserved / history.name
                    shutil.copy2(history, copied)
                    shrink = subprocess.run([
                        sys.executable,
                        str(ROOT / "tools" / "shrink_history.py"),
                        str(copied),
                        str(args.checker),
                    ], text=True, capture_output=True)
                    log.write(shrink.stdout)
                    log.write(shrink.stderr)
                    result["shrink_returncode"] = shrink.returncode
                    minimized = copied.with_name(copied.stem + ".min.json")
                    if minimized.exists():
                        result["minimized_history"] = str(minimized.relative_to(ROOT))
                    stop = True
                    break

                if status == "UNKNOWN":
                    preserved = EVIDENCE / "unknown"
                    preserved.mkdir(parents=True, exist_ok=True)
                    copied = preserved / history.name
                    shutil.copy2(history, copied)
                    result["preserved_history"] = str(copied.relative_to(ROOT))
                    stop = True
                    break

                if status == "ERROR":
                    stop = True
                    break

payload = {
    "matrix": {
        "capacities": capacities,
        "profiles": profiles,
        "seeds": args.seeds,
        "ops_per_side": args.ops_per_side,
    },
    "counts": counts,
    "results": results,
}
summary_json.write_text(json.dumps(payload, indent=2) + "\n")

if counts["FAIL"] or counts["UNKNOWN"] or counts["ERROR"]:
    sys.exit(1)
