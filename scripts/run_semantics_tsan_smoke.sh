#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-tsan-semantics"
BUILD_TYPE="RelWithDebInfo"
SKIP_TESTS=0

usage() {
  echo "Usage: ./scripts/run_semantics_tsan_smoke.sh [--build-dir <dir>] [--build-type <type>] [--skip-tests]" >&2
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      if [[ $# -lt 2 ]]; then
        usage
        exit 2
      fi
      BUILD_DIR="$2"
      shift 2
      ;;
    --build-type)
      if [[ $# -lt 2 ]]; then
        usage
        exit 2
      fi
      BUILD_TYPE="$2"
      shift 2
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

export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
export CTEST_PARALLEL_LEVEL="${CTEST_PARALLEL_LEVEL:-1}"

if [[ "$(uname -s)" == "Darwin" ]]; then
  if [[ "${PRIMESTRUCT_TSAN_USE_ENV_COMPILER:-0}" != "1" ]]; then
    export CXX="clang++"
  fi
fi

: "${CXX:=c++}"

if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  cached_cxx="$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' "$BUILD_DIR/CMakeCache.txt" | head -n 1)"
  if [[ -n "${CXX:-}" && -n "$cached_cxx" && "$cached_cxx" != *"$CXX" ]]; then
    rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"
  fi
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DPRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE=ON

cmake --build "$BUILD_DIR" --target PrimeStruct_semantics_tsan_smoke

if [[ "$SKIP_TESTS" -eq 0 ]]; then
  ctest --test-dir "$BUILD_DIR" --output-on-failure --parallel 1 -R '^PrimeStruct_semantics_tsan_smoke$'
fi
