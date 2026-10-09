#!/usr/bin/env bash
set -euo pipefail
printf 'date_utc=%s\n' "$(date -u +%FT%TZ)"
printf 'uname=%s\n' "$(uname -a)"
command -v lscpu >/dev/null && lscpu
command -v numactl >/dev/null && numactl --hardware || true
for f in /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor /sys/devices/system/cpu/smt/control; do
  [[ -r "$f" ]] && printf '%s=%s\n' "$f" "$(cat "$f")"
done
