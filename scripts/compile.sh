#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
BUILD_TYPE="Debug"
SKIP_TESTS=0

usage() {
  echo "Usage: ./scripts/compile.sh [--release] [--skip-tests]" >&2
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
    --skip-tests)
      SKIP_TESTS=1
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

DEFAULT_CTEST_JOBS=11
CTEST_JOBS="${CTEST_PARALLEL_LEVEL:-$DEFAULT_CTEST_JOBS}"
ctest --test-dir "$BUILD_DIR" --output-on-failure --parallel "$CTEST_JOBS"
