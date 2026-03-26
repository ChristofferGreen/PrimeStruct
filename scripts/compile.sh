#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
BUILD_TYPE="Debug"
FAST_MODE=0
FAST_TEST_THRESHOLD_SECONDS=10
SKIP_TESTS=0

usage() {
  echo "Usage: ./scripts/compile.sh [--release] [--fast] [--skip-tests]" >&2
}

detect_jobs() {
  local jobs=""

  if command -v getconf >/dev/null 2>&1; then
    jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
  fi

  if [[ -z "$jobs" ]] && command -v nproc >/dev/null 2>&1; then
    jobs="$(nproc 2>/dev/null || true)"
  fi

  if [[ -z "$jobs" ]] && command -v sysctl >/dev/null 2>&1; then
    jobs="$(sysctl -n hw.ncpu 2>/dev/null || true)"
  fi

  if [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
    jobs=4
  fi

  printf '%s\n' "$jobs"
}

build_fast_exclude_file() {
  local build_dir="$1"
  local threshold_seconds="$2"
  local output_file="$3"
  local cost_file="$build_dir/Testing/Temporary/CTestCostData.txt"

  if [[ ! -f "$cost_file" ]]; then
    return 1
  fi

  awk -v threshold="$threshold_seconds" '
    NF >= 3 && ($3 + 0) > threshold {
      print $1
    }
  ' "$cost_file" > "$output_file"
  return 0
}

for arg in "$@"; do
  case "$arg" in
    --release)
      BUILD_DIR="$ROOT_DIR/build-release"
      BUILD_TYPE="Release"
      ;;
    --fast)
      FAST_MODE=1
      ;;
    --skip-tests)
      SKIP_TESTS=1
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

JOBS="$(detect_jobs)"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build "$BUILD_DIR" --parallel "$JOBS"

if [[ "$SKIP_TESTS" -eq 1 ]]; then
  echo "Skipping tests."
  exit 0
fi

ctest_args=(
  --test-dir "$BUILD_DIR"
  --output-on-failure
  --parallel "$JOBS"
)

if [[ "$FAST_MODE" -eq 1 ]]; then
  FAST_EXCLUDE_FILE="$(mktemp "${TMPDIR:-/tmp}/primestruct-fast-tests.XXXXXX")"
  trap 'rm -f "$FAST_EXCLUDE_FILE"' EXIT

  if build_fast_exclude_file "$BUILD_DIR" "$FAST_TEST_THRESHOLD_SECONDS" "$FAST_EXCLUDE_FILE"; then
    excluded_count="$(wc -l < "$FAST_EXCLUDE_FILE" | tr -d '[:space:]')"
    if [[ "$excluded_count" -gt 0 ]]; then
      echo "Fast mode: excluding $excluded_count tests with historical runtime > ${FAST_TEST_THRESHOLD_SECONDS}s."
      ctest_args+=(--exclude-from-file "$FAST_EXCLUDE_FILE")
    else
      echo "Fast mode: no historical tests exceed ${FAST_TEST_THRESHOLD_SECONDS}s."
    fi
  else
    echo "Fast mode: no CTest timing data found in $BUILD_DIR/Testing/Temporary/CTestCostData.txt; running full suite." >&2
  fi
fi

ctest "${ctest_args[@]}"
