#!/usr/bin/env python3
"""Delta-debug a failing history while preserving the checker's failure class.

Usage: shrink_history.py failing.json checker-command [checker-args...]
The checker command must return non-zero for the source history. Candidate reductions
are accepted only when they produce the same exit code, so an Illegal counterexample
cannot silently shrink into an Unknown timeout (or vice versa).
"""
import copy
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

if len(sys.argv) < 3:
    raise SystemExit(__doc__)

source = Path(sys.argv[1])
checker = sys.argv[2:]
data = json.loads(source.read_text())
ops = data["operations"]

initial = subprocess.run(
    checker + [str(source)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
)
if initial.returncode == 0:
    raise SystemExit("source history does not reproduce a checker failure")
target_returncode = initial.returncode


def preserves_failure(candidate):
    payload = copy.deepcopy(data)
    payload["operations"] = candidate
    name = ""
    try:
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as handle:
            json.dump(payload, handle)
            name = handle.name
        result = subprocess.run(
            checker + [name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        return result.returncode == target_returncode
    finally:
        if name:
            try:
                os.unlink(name)
            except FileNotFoundError:
                pass


n = 2
while len(ops) >= 2:
    size = max(1, len(ops) // n)
    reduced = False
    for start in range(0, len(ops), size):
        candidate = ops[:start] + ops[start + size :]
        if candidate and preserves_failure(candidate):
            ops = candidate
            n = max(2, n - 1)
            reduced = True
            break
    if not reduced:
        if n >= len(ops):
            break
        n = min(len(ops), n * 2)

data["operations"] = ops
out = source.with_name(source.stem + ".min.json")
out.write_text(json.dumps(data, indent=2) + "\n")
print(out)
