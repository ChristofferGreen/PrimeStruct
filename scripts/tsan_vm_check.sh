#!/usr/bin/env bash
# Runs the concurrent corpus (tests/vm_threads_corpus.txt, TODO-5900) against a ThreadSanitizer build of the VM.
#
# 1. Builds `primevm` and `primec` with -fsanitize=thread in build-tsan/ (the build is as heavy as a release build:
#    run it alone, never next to another build or test run, and under the repo's heavy-command lock).
# 2. Runs each corpus entry of the list with the release doctest binaries from build-release/ (they only start
#    ./primevm and ./primec of the working directory, which is build-tsan/), PRIMESTRUCT_VM_THREADS_CORPUS=<repeats>:
#    the eager schedule once, then `--vm-threads` <repeats> times, each run printing what the eager run printed.
# 3. A ThreadSanitizer report (TSAN_OPTIONS log_path files, or exit code 66) or a failed doctest fails the script.
#
# The doctest binaries must exist: run `./scripts/compile.sh --release --skip-tests` first.
#
# Usage: scripts/tsan_vm_check.sh [--repeats N] [--build-dir build-tsan] [--release-dir build-release]
#                                 [--skip-build] [--also-runtime]
#   --repeats N      threads runs per program (default 5: ThreadSanitizer runs 5 to 15 times slower; the ctest
#                    entries run 20)
#   --skip-build     reuse an existing build-tsan/
#   --also-runtime   also build PrimeStruct_vm_byte_tests with ThreadSanitizer and run its vm_threads suites
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-tsan"
RELEASE_DIR="$ROOT_DIR/build-release"
CORPUS_LIST="$ROOT_DIR/tests/vm_threads_corpus.txt"
REPEATS=5
SKIP_BUILD=0
ALSO_RUNTIME=0

usage() {
  echo "Usage: scripts/tsan_vm_check.sh [--repeats N] [--build-dir <dir>] [--release-dir <dir>] [--skip-build] [--also-runtime]" >&2
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repeats)
      [[ $# -ge 2 ]] || { usage; exit 2; }
      REPEATS="$2"
      shift 2
      ;;
    --build-dir)
      [[ $# -ge 2 ]] || { usage; exit 2; }
      BUILD_DIR="$2"
      shift 2
      ;;
    --release-dir)
      [[ $# -ge 2 ]] || { usage; exit 2; }
      RELEASE_DIR="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --also-runtime)
      ALSO_RUNTIME=1
      shift
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

if [[ ! "$REPEATS" =~ ^[1-9][0-9]*$ ]]; then
  echo "tsan_vm_check: --repeats needs a positive number" >&2
  exit 2
fi

detect_jobs() {
  local jobs=""
  if command -v getconf >/dev/null 2>&1; then
    jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
  fi
  if [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
    jobs=4
  fi
  printf '%s\n' "$jobs"
}

if [[ "$(uname -s)" == "Darwin" && "${PRIMESTRUCT_TSAN_USE_ENV_COMPILER:-0}" != "1" ]]; then
  export CXX="clang++"
fi
: "${CXX:=c++}"

targets=(primevm primec)
if [[ "$ALSO_RUNTIME" -eq 1 ]]; then
  targets+=(PrimeStruct_vm_byte_tests)
fi

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cached_cxx="$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' "$BUILD_DIR/CMakeCache.txt" | head -n 1)"
    if [[ -n "$cached_cxx" && "$cached_cxx" != *"$CXX" ]]; then
      rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"
    fi
  fi
  # PRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE is the project's switch for -fsanitize=thread on every target (and it
  # fails configuration with the exact compiler error when the toolchain cannot link a ThreadSanitizer binary).
  # mimalloc is off so that ThreadSanitizer sees the system allocator.
  cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_CXX_COMPILER="$CXX" \
    -DPRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE=ON \
    -DPRIMESTRUCT_USE_MIMALLOC=OFF
  nice -n 10 cmake --build "$BUILD_DIR" --parallel "$(detect_jobs)" --target "${targets[@]}"
fi

for binary in primevm primec; do
  if [[ ! -x "$BUILD_DIR/$binary" ]]; then
    echo "tsan_vm_check: $BUILD_DIR/$binary is missing (build it, or drop --skip-build)" >&2
    exit 2
  fi
done

LOG_DIR="$BUILD_DIR/tsan-logs"
rm -rf "$LOG_DIR"
mkdir -p "$LOG_DIR"
export TSAN_OPTIONS="halt_on_error=0 exitcode=66 second_deadlock_stack=1 log_path=$LOG_DIR/report"
export PRIMESTRUCT_VM_THREADS_CORPUS="$REPEATS"

failures=0
runs=0

# Runs a doctest binary from the ThreadSanitizer build directory (the release doctest binaries only start the
# ./primevm and ./primec of their working directory).
run_corpus() {
  local label="$1"
  local binary="$2"
  shift 2
  if [[ ! -x "$binary" ]]; then
    echo "tsan_vm_check: $binary is missing (run ./scripts/compile.sh --release --skip-tests, or drop --skip-build)" >&2
    exit 2
  fi
  local after status
  runs=$((runs + 1))
  if (cd "$BUILD_DIR" && "$binary" "$@" --order-by=file) >"$LOG_DIR/$runs.out" 2>&1; then
    status=0
  else
    status=$?
  fi
  after="$(find "$LOG_DIR" -type f -name 'report*' | wc -l | tr -d ' ')"
  if [[ "$status" -ne 0 ]]; then
    echo "FAIL $label (exit $status); output in $LOG_DIR/$runs.out" >&2
    tail -n 20 "$LOG_DIR/$runs.out" >&2
    failures=$((failures + 1))
  elif [[ "$after" -gt 0 ]]; then
    echo "FAIL $label (ThreadSanitizer report)" >&2
    failures=$((failures + 1))
  else
    echo "ok   $label"
  fi
}

while IFS='|' read -r kind suite source extra; do
  kind="${kind%%#*}"
  kind="$(echo "$kind" | tr -d '[:space:]')"
  [[ -n "$kind" ]] || continue
  suite="$(echo "$suite" | sed -e 's/^ *//' -e 's/ *$//')"
  case "$kind" in
    matrix)
      run_corpus "$suite" "$RELEASE_DIR/PrimeStruct_program_matrix_tests" "--test-suite=$suite"
      ;;
    cases)
      extra="$(echo "$extra" | sed -e 's/^ *//' -e 's/ *$//')"
      run_corpus "$extra" "$RELEASE_DIR/PrimeStruct_compile_run_tests" "--test-suite=$suite" "--test-case=$extra"
      ;;
    *)
      ;;
  esac
done <"$CORPUS_LIST"

if [[ "$ALSO_RUNTIME" -eq 1 ]]; then
  for suite in vm_threads vm_threads_sync vm_threads_host heap_arenas tasks atomics mutexes conditions; do
    suite="primestruct.runtime.$suite"
    run_corpus "$suite" "$BUILD_DIR/PrimeStruct_vm_byte_tests" "--test-suite=$suite"
  done
fi

if find "$LOG_DIR" -type f -name 'report*' | grep -q .; then
  echo "tsan_vm_check: ThreadSanitizer reports in $LOG_DIR" >&2
  failures=$((failures + 1))
fi

if [[ "$failures" -ne 0 ]]; then
  echo "tsan_vm_check: $failures of $runs corpus runs failed" >&2
  exit 1
fi
echo "tsan_vm_check: $runs corpus runs, $REPEATS threads runs per program, no ThreadSanitizer report"
