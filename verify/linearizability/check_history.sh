#!/usr/bin/env bash
set -euo pipefail
history="$(realpath "$1")"
visualization="${2:-}"
cd "$(dirname "$0")"
if [[ -n "$visualization" ]]; then
  visualization="$(realpath -m "$visualization")"
  exec go run . "$history" "$visualization"
fi
exec go run . "$history"
