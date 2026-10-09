#!/usr/bin/env bash
set -euo pipefail
binary="${1:-build/release/bench_throughput}"
output="${2:-evidence/benchmarks/perf-stat.txt}"
mkdir -p "$(dirname "$output")"
if ! command -v perf >/dev/null 2>&1; then
  echo "perf unavailable on this host" | tee "$output"
  exit 0
fi
set +e
perf stat \
  -e cycles,instructions,branches,branch-misses,cache-references,cache-misses,context-switches,cpu-migrations \
  "$binary" >/dev/null 2>"$output"
status=$?
set -e
if [[ $status -ne 0 ]]; then
  echo "perf counters unavailable or restricted; status=$status" >> "$output"
fi
cat "$output"
