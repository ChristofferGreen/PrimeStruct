#!/usr/bin/env bash
# Startup time and memory of a PrimeStruct native UI executable: runs it N times in the AppKit
# backend's snapshot mode (the first window is laid out and drawn into a PNG, then the app quits),
# which is "time to first drawn window", and reports the mean wall time and the peak resident set.
#
# Usage: scripts/measure_app_startup.sh <executable> [runs] [file-to-open]
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <executable> [runs] [file-to-open]" >&2
  exit 64
fi
EXE="$1"
RUNS="${2:-10}"
FILE="${3:-}"
SNAPSHOT="$(mktemp -t primestruct_startup).png"
LOG="$(mktemp -t primestruct_startup_log)"
trap 'rm -f "$SNAPSHOT" "$LOG"' EXIT

ARGS=()
if [[ -n "$FILE" ]]; then
  # The backend opens the file itself, so the same call works for a bytecode runner (whose first
  # argument is a program) and for a compiled editor.
  export PRIMESTRUCT_UI_OPEN="$FILE"
fi

total=0
peak=0
for ((i = 0; i < RUNS; i++)); do
  start=$(python3 -c 'import time; print(time.perf_counter_ns())')
  PRIMESTRUCT_UI_SNAPSHOT="$SNAPSHOT" /usr/bin/time -l "$EXE" >/dev/null 2>"$LOG"
  end=$(python3 -c 'import time; print(time.perf_counter_ns())')
  total=$((total + (end - start)))
  rss=$(awk '/maximum resident set size/ {print $1}' "$LOG")
  if [[ -n "$rss" && "$rss" -gt "$peak" ]]; then
    peak="$rss"
  fi
done
echo "runs=$RUNS mean_ms=$((total / RUNS / 1000000)) peak_rss_kb=$((peak / 1024))"
