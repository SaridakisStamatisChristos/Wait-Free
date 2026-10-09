#!/usr/bin/env bash
set -euo pipefail
cxx="${CXX:-c++}"
out="${1:-evidence/benchmarks/assembly.s}"
mkdir -p "$(dirname "$out")"
"$cxx" -std=c++23 -O3 -S -Iinclude tools/assembly_probe.cpp -o "$out"
if grep -Eiq '\b(pthread_|malloc|calloc|realloc|free|mutex|cmpxchg|lock[[:space:]])' "$out"; then
  echo "unexpected hot-path primitive found in assembly" >&2
  exit 1
fi
printf 'assembly audit: PASS (%s)\n' "$out"
