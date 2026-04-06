#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
BUILD_TYPE="Debug"
FAST_MODE=0
FAST_TEST_THRESHOLD_SECONDS=10
SKIP_TESTS=0

usage() {
  echo "Usage: ./scripts/compile.sh [--release] [--fast] [--fast-threshold <seconds>] [--skip-tests]" >&2
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

find_fast_cost_file() {
  local build_dir="$1"
  local local_cost_file="$build_dir/Testing/Temporary/CTestCostData.txt"

  if [[ -f "$local_cost_file" ]]; then
    printf '%s\n' "$local_cost_file"
    return 0
  fi

  local build_dir_name
  build_dir_name="$(basename "$build_dir")"

  local main_worktree=""
  if command -v git >/dev/null 2>&1; then
    main_worktree="$(git -C "$ROOT_DIR" worktree list --porcelain 2>/dev/null |
      awk '/^worktree / { print substr($0, 10); exit }')"
  fi

  if [[ -n "$main_worktree" ]]; then
    local fallback_cost_file="$main_worktree/$build_dir_name/Testing/Temporary/CTestCostData.txt"
    if [[ -f "$fallback_cost_file" ]]; then
      printf '%s\n' "$fallback_cost_file"
      return 0
    fi
  fi

  return 1
}

build_fast_exclude_file() {
  local build_dir="$1"
  local threshold_seconds="$2"
  local output_file="$3"
  local cost_file=""
  cost_file="$(find_fast_cost_file "$build_dir")" || {
    return 1
  }

  awk -v threshold="$threshold_seconds" '
    NF >= 3 && ($3 + 0) > threshold {
      print $1
    }
  ' "$cost_file" > "$output_file"

  FAST_COST_FILE="$cost_file"
  return 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --release)
      BUILD_DIR="$ROOT_DIR/build-release"
      BUILD_TYPE="Release"
      shift
      ;;
    --fast)
      FAST_MODE=1
      shift
      ;;
    --fast-threshold)
      if [[ $# -lt 2 || ! "$2" =~ ^[1-9][0-9]*$ ]]; then
        usage
        exit 2
      fi
      FAST_MODE=1
      FAST_TEST_THRESHOLD_SECONDS="$2"
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
  --parallel 5
)

if [[ "$FAST_MODE" -eq 1 ]]; then
  FAST_EXCLUDE_FILE="$(mktemp "${TMPDIR:-/tmp}/primestruct-fast-tests.XXXXXX")"
  trap 'rm -f "$FAST_EXCLUDE_FILE"' EXIT

  if build_fast_exclude_file "$BUILD_DIR" "$FAST_TEST_THRESHOLD_SECONDS" "$FAST_EXCLUDE_FILE"; then
    excluded_count="$(wc -l < "$FAST_EXCLUDE_FILE" | tr -d '[:space:]')"
    if [[ "$FAST_COST_FILE" != "$BUILD_DIR/Testing/Temporary/CTestCostData.txt" ]]; then
      echo "Fast mode: using fallback timing data from $FAST_COST_FILE."
    fi
    if [[ "$excluded_count" -gt 0 ]]; then
      echo "Fast mode: excluding $excluded_count tests with historical runtime > ${FAST_TEST_THRESHOLD_SECONDS}s."
      ctest_args+=(--exclude-from-file "$FAST_EXCLUDE_FILE")
    else
      echo "Fast mode: no historical tests exceed ${FAST_TEST_THRESHOLD_SECONDS}s."
    fi
  else
    echo "Fast mode: no CTest timing data found locally or in the main worktree; running full suite." >&2
  fi
fi

ctest "${ctest_args[@]}"
