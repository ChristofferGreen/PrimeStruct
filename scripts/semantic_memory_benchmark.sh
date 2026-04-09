#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRIMEC_PATH="${PRIMEC_PATH:-$ROOT_DIR/build-release/primec}"
RUNS="${SEMANTIC_MEMORY_RUNS:-3}"
REPORT_JSON="${SEMANTIC_MEMORY_REPORT_JSON:-$ROOT_DIR/build-release/benchmarks/semantic_memory_report.json}"

exec python3 "$ROOT_DIR/scripts/semantic_memory_benchmark.py" \
  --repo-root "$ROOT_DIR" \
  --primec "$PRIMEC_PATH" \
  --runs "$RUNS" \
  --report-json "$REPORT_JSON" \
  "$@"
