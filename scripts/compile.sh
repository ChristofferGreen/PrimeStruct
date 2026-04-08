#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
BUILD_TYPE="Debug"
SKIP_TESTS=0
INCLUDE_EXPENSIVE_TESTS=0
EXPENSIVE_RUNTIME_THRESHOLD_SECONDS=3
EXPENSIVE_MEMORY_THRESHOLD_MB=500
DEFAULT_TEST_MEMORY_GUARD_MB=4096

usage() {
  echo "Usage: ./scripts/compile.sh [--release] [--runtime-threshold <seconds>] [--skip-tests] [--include-expensive-tests]" >&2
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

while [[ $# -gt 0 ]]; do
  case "$1" in
    --release)
      BUILD_DIR="$ROOT_DIR/build-release"
      BUILD_TYPE="Release"
      shift
      ;;
    --runtime-threshold)
      if [[ $# -lt 2 || ! "$2" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
        usage
        exit 2
      fi
      EXPENSIVE_RUNTIME_THRESHOLD_SECONDS="$2"
      shift 2
      ;;
    --skip-tests)
      SKIP_TESTS=1
      shift
      ;;
    --include-expensive-tests)
      INCLUDE_EXPENSIVE_TESTS=1
      shift
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

export PRIMESTRUCT_RUN_COMMAND_WRAPPER="$ROOT_DIR/scripts/test_command_wrapper.sh"
if [[ ! -x "$PRIMESTRUCT_RUN_COMMAND_WRAPPER" ]]; then
  echo "error: command wrapper is missing or not executable: $PRIMESTRUCT_RUN_COMMAND_WRAPPER" >&2
  exit 1
fi

export PRIMESTRUCT_EXPENSIVE_MEMORY_THRESHOLD_MB="${PRIMESTRUCT_EXPENSIVE_MEMORY_THRESHOLD_MB:-$EXPENSIVE_MEMORY_THRESHOLD_MB}"
export PRIMESTRUCT_TEST_MEMORY_GUARD_MB="${PRIMESTRUCT_TEST_MEMORY_GUARD_MB:-$DEFAULT_TEST_MEMORY_GUARD_MB}"

python3 "$ROOT_DIR/scripts/manage_expensive_tests.py" run-gate \
  --build-dir "$BUILD_DIR" \
  --include-expensive-tests "$INCLUDE_EXPENSIVE_TESTS" \
  --runtime-threshold "$EXPENSIVE_RUNTIME_THRESHOLD_SECONDS" \
  --memory-threshold "$EXPENSIVE_MEMORY_THRESHOLD_MB"
