#!/usr/bin/env bash
set -euo pipefail
lab_build="$1"
mkdir -p "$lab_build"
python tools/generate_counter_diagnostic.py "$lab_build/comparator.cpp"
lab_flags=(-std=c++20 -O3 -DNDEBUG -pthread
  -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wold-style-cast -Wcast-align -Wundef -Werror
  -Iinclude -Ibench
  -isystem "$lab_build/_deps/rigtorp_spsc-src/include"
  -isystem "$lab_build/_deps/moodycamel_readerwriterqueue-src"
  -isystem "$lab_build/_deps/drogalis_spsc-src/include")
if [[ "${COUNTER_SANITIZERS:-0}" == 1 ]]; then
  "$CXX" "${lab_flags[@]}" -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$lab_build/comparator.cpp" bench/environment.cpp -o "$lab_build/comparator-sanitized"
  exit 0
fi
"$CXX" "${lab_flags[@]}" "$lab_build/comparator.cpp" bench/environment.cpp -o "$lab_build/comparator"
"$CXX" "${lab_flags[@]}" -S "$lab_build/comparator.cpp" -o "$lab_build/comparator.s"
nm -n -C "$lab_build/comparator" > "$lab_build/linked-symbols.txt"
python tools/audit_counter_code.py "$lab_build/comparator.s" "$lab_build/linked-symbols.txt" "$lab_build/common-code.json"
