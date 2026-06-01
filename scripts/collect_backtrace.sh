#!/usr/bin/env bash
# Helper: run a representative failing doctest under gdb and save backtrace
set -euo pipefail

if [ -z "${1-}" ]; then
  echo "Usage: $0 <test-range-args>"
  echo "Example: $0 --test-suite=primestruct.ir.pipeline.serialization --order-by=file --first=1 --last=4"
  exit 2
fi

OUT_FILE="build-release/backtrace_full.txt"
mkdir -p "$(dirname "$OUT_FILE")"

# locate the test binary (prefer build-release, then build-debug, then build-asan)
TEST_BIN=""
for candidate in build-release/PrimeStruct_backend_ir_tests build-debug/PrimeStruct_backend_ir_tests build-asan/PrimeStruct_backend_ir_tests ./PrimeStruct_backend_ir_tests; do
  if [ -x "$candidate" ]; then
    TEST_BIN="$candidate"
    break
  fi
done

if [ -z "$TEST_BIN" ]; then
  echo "Error: cannot find PrimeStruct_backend_ir_tests binary in expected build locations." > "$OUT_FILE"
  echo "Checked: build-release/, build-debug/, build-asan/, and ./" >> "$OUT_FILE"
  echo "Please run this script from the repo root after building the release/debug/asan target." >> "$OUT_FILE"
  echo "Wrote diagnostics to $OUT_FILE"
  exit 1
fi

echo "Running: $TEST_BIN $*" > "$OUT_FILE"
gdb --batch --ex "set pagination off" --ex "run" --ex "bt full" --args "$TEST_BIN" "$@" >> "$OUT_FILE" 2>&1 || true

echo "Saved backtrace to $OUT_FILE"
