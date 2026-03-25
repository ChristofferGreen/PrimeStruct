#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
BUILD_TYPE="Debug"
RUN_TESTS=1
CONFIGURE_ONLY=0
CLEAN=0
COVERAGE=0
RUN_BENCHMARK=0
RUN_WASM_RUNTIME_CHECKS=0
RUN_BENCHMARK_REGRESSION=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --release)
      BUILD_DIR="$ROOT_DIR/build-release"
      BUILD_TYPE="Release"
      shift
      ;;
    --coverage)
      COVERAGE=1
      shift
      ;;
    --configure)
      CONFIGURE_ONLY=1
      shift
      ;;
    --clean)
      CLEAN=1
      shift
      ;;
    --skip-tests)
      RUN_TESTS=0
      shift
      ;;
    --benchmark)
      RUN_BENCHMARK=1
      shift
      ;;
    --benchmark-regression)
      RUN_BENCHMARK=1
      RUN_BENCHMARK_REGRESSION=1
      shift
      ;;
    --wasm-runtime-checks)
      RUN_WASM_RUNTIME_CHECKS=1
      shift
      ;;
    *)
      echo "Unknown arg: $1" >&2
      exit 2
      ;;
  esac
 done

COVERAGE_CMAKE_ARGS=()

if [[ -n "${PRIMESTRUCT_BUILD_JOBS:-}" ]]; then
  BUILD_JOBS="$PRIMESTRUCT_BUILD_JOBS"
elif command -v nproc >/dev/null 2>&1; then
  BUILD_JOBS="$(nproc)"
elif command -v sysctl >/dev/null 2>&1; then
  BUILD_JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
else
  BUILD_JOBS=4
fi
TEST_JOBS="${PRIMESTRUCT_TEST_JOBS:-11}"
TEST_BATCH_SIZE="${PRIMESTRUCT_TEST_BATCH_SIZE:-40}"
CTEST_JSON_CACHE=""
RUN_IN_BUILD_DIR_STATUS=0
TEST_LOCK_DIR="${TMPDIR:-/tmp}/primestruct-compile-test-lock"
TEST_LOCK_WAIT_SECONDS="${PRIMESTRUCT_TEST_LOCK_WAIT_SECONDS:-600}"
TEST_LOCK_HELD=0
FOREIGN_TEST_WAIT_SECONDS="${PRIMESTRUCT_FOREIGN_TEST_WAIT_SECONDS:-600}"
DIRECT_RERUN_RETRIES="${PRIMESTRUCT_DIRECT_RERUN_RETRIES:-2}"
BUILD_RETRIES="${PRIMESTRUCT_BUILD_RETRIES:-2}"
KILL_FOREIGN_TEST_PROCESSES="${PRIMESTRUCT_KILL_FOREIGN_TEST_PROCESSES:-0}"
TEST_DRIVER="${PRIMESTRUCT_TEST_DRIVER:-direct}"

print_command() {
  local prefix="$1"
  shift
  printf '%s' "$prefix"
  printf '%q ' "$@"
  printf '\n'
}

run_in_build_dir_capture_status() {
  local llvm_profile_file="$1"
  shift

  RUN_IN_BUILD_DIR_STATUS=0
  set +e
  if [[ -n "$llvm_profile_file" ]]; then
    (cd "$BUILD_DIR" && LLVM_PROFILE_FILE="$llvm_profile_file" "$@")
    RUN_IN_BUILD_DIR_STATUS=$?
  else
    (cd "$BUILD_DIR" && "$@")
    RUN_IN_BUILD_DIR_STATUS=$?
  fi
  set -e
}

run_build_capture_status() {
  BUILD_STATUS=0
  set +e
  cmake --build "$BUILD_DIR" --parallel "$BUILD_JOBS"
  BUILD_STATUS=$?
  set -e
}

release_test_lock() {
  if [[ "$TEST_LOCK_HELD" -eq 1 ]]; then
    rm -rf "$TEST_LOCK_DIR"
    TEST_LOCK_HELD=0
  fi
}

acquire_test_lock() {
  if [[ "$TEST_LOCK_HELD" -eq 1 ]]; then
    return
  fi

  local owner_file="$TEST_LOCK_DIR/owner"
  local waited=0
  while ! mkdir "$TEST_LOCK_DIR" 2>/dev/null; do
    if [[ -f "$owner_file" ]]; then
      local owner
      owner="$(cat "$owner_file" 2>/dev/null || true)"
      if [[ -n "$owner" ]]; then
        local owner_pid
        owner_pid="${owner%% *}"
        if [[ -n "$owner_pid" ]] && ! kill -0 "$owner_pid" 2>/dev/null; then
          echo "[compile.sh] Removing stale test lock from $owner" >&2
          rm -rf "$TEST_LOCK_DIR"
          continue
        fi
        echo "[compile.sh] Waiting for test lock held by $owner" >&2
      fi
    else
      echo "[compile.sh] Waiting for test lock at $TEST_LOCK_DIR" >&2
    fi

    if [[ "$waited" -ge "$TEST_LOCK_WAIT_SECONDS" ]]; then
      echo "[compile.sh] ERROR: timed out waiting for test lock at $TEST_LOCK_DIR" >&2
      return 1
    fi
    sleep 1
    waited=$((waited + 1))
  done

  printf '%s %s %s\n' "$$" "$ROOT_DIR" "$(date '+%Y-%m-%d %H:%M:%S %Z')" > "$owner_file"
  TEST_LOCK_HELD=1
}

wait_for_foreign_test_processes() {
  local waited=0
  local process_pattern='bash ./scripts/compile.sh|PrimeStruct/.*/compile.sh|ctest --output-on-failure|PrimeStruct_.*tests'

  while true; do
    local -a foreign_owners=()
    local pid

    while IFS= read -r pid; do
      [[ -z "$pid" ]] && continue
      [[ "$pid" == "$$" ]] && continue

      local state=""
      state="$(ps -o state= -p "$pid" 2>/dev/null | tr -d '[:space:]')"
      if [[ -n "$state" && "$state" == Z* ]]; then
        continue
      fi

      local cwd=""
      cwd="$(lsof -a -d cwd -p "$pid" -Fn 2>/dev/null | sed -n 's/^n//p' | head -n 1)"
      if [[ -n "$cwd" ]]; then
        if [[ "$cwd" == "$ROOT_DIR" || "$cwd" == "$ROOT_DIR/"* ]]; then
          continue
        fi
        foreign_owners+=("$pid:$cwd")
      fi
    done < <(pgrep -f "$process_pattern" || true)

    if [[ "${#foreign_owners[@]}" -eq 0 ]]; then
      return
    fi

    if [[ "$KILL_FOREIGN_TEST_PROCESSES" == "1" ]]; then
      printf '[compile.sh] WARN: killing foreign PrimeStruct test processes: %s\n' "${foreign_owners[*]}" >&2
      local owner
      for owner in "${foreign_owners[@]}"; do
        local foreign_pid="${owner%%:*}"
        kill -9 "$foreign_pid" 2>/dev/null || true
      done
      sleep 1
      continue
    fi

    printf '[compile.sh] Waiting for foreign PrimeStruct test processes: %s\n' "${foreign_owners[*]}" >&2
    if [[ "$waited" -ge "$FOREIGN_TEST_WAIT_SECONDS" ]]; then
      echo "[compile.sh] ERROR: timed out waiting for foreign PrimeStruct test processes to finish." >&2
      return 1
    fi

    sleep 1
    waited=$((waited + 1))
  done
}

ctest_json_cache_is_valid() {
  local cache_path="$1"

  if [[ ! -s "$cache_path" ]]; then
    return 1
  fi

  python3 - "$cache_path" <<'PY' >/dev/null 2>&1
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    json.load(handle)
PY
}

ensure_ctest_json_cache() {
  if [[ -z "$CTEST_JSON_CACHE" ]]; then
    CTEST_JSON_CACHE="$BUILD_DIR/Testing/Temporary/ctest_tests.json"
  fi

  if ctest_json_cache_is_valid "$CTEST_JSON_CACHE"; then
    return
  fi

  mkdir -p "$(dirname "$CTEST_JSON_CACHE")"
  local temp_cache="${CTEST_JSON_CACHE}.tmp"
  rm -f "$CTEST_JSON_CACHE" "$temp_cache"
  (cd "$BUILD_DIR" && ctest --show-only=json-v1 > "$temp_cache")
  mv "$temp_cache" "$CTEST_JSON_CACHE"

  if ! ctest_json_cache_is_valid "$CTEST_JSON_CACHE"; then
    echo "[compile.sh] ERROR: failed to build a valid CTest JSON cache." >&2
    return 1
  fi
}

collect_failed_ctest_indices() {
  local start="$1"
  local end="$2"
  local failed_log="$BUILD_DIR/Testing/Temporary/LastTestsFailed.log"

  if [[ ! -f "$failed_log" ]]; then
    return
  fi

  while IFS= read -r line; do
    local index="${line%%:*}"
    if [[ "$index" =~ ^[0-9]+$ ]] && [[ "$index" -ge "$start" ]] && [[ "$index" -le "$end" ]]; then
      printf '%s\n' "$index"
    fi
  done < "$failed_log"
}

run_ctest_indices_direct() {
  local llvm_profile_file="$1"
  shift

  if [[ "$#" -eq 0 ]]; then
    echo "[compile.sh] ERROR: no test indices provided for direct rerun." >&2
    return 1
  fi

  ensure_ctest_json_cache

  local indices_csv
  indices_csv="$(IFS=,; printf '%s' "$*")"

  PRIMESTRUCT_BUILD_DIR="$BUILD_DIR" \
  PRIMESTRUCT_CTEST_JSON="$CTEST_JSON_CACHE" \
  PRIMESTRUCT_LLVM_PROFILE_FILE="$llvm_profile_file" \
  PRIMESTRUCT_CTEST_INDICES="$indices_csv" \
  PRIMESTRUCT_DIRECT_RERUN_RETRIES="$DIRECT_RERUN_RETRIES" \
  PRIMESTRUCT_ROOT_DIR="$ROOT_DIR" \
  PRIMESTRUCT_FOREIGN_TEST_WAIT_SECONDS="$FOREIGN_TEST_WAIT_SECONDS" \
  PRIMESTRUCT_KILL_FOREIGN_TEST_PROCESSES="$KILL_FOREIGN_TEST_PROCESSES" \
    python3 - <<'PY'
import json
import os
import re
import signal
import shlex
import subprocess
import sys
import time

build_dir = os.environ["PRIMESTRUCT_BUILD_DIR"]
json_path = os.environ["PRIMESTRUCT_CTEST_JSON"]
llvm_profile_file = os.environ.get("PRIMESTRUCT_LLVM_PROFILE_FILE", "")
max_retries = int(os.environ.get("PRIMESTRUCT_DIRECT_RERUN_RETRIES", "0"))
root_dir = os.environ["PRIMESTRUCT_ROOT_DIR"]
foreign_wait_seconds = int(os.environ.get("PRIMESTRUCT_FOREIGN_TEST_WAIT_SECONDS", "600"))
kill_foreign_test_processes = os.environ.get("PRIMESTRUCT_KILL_FOREIGN_TEST_PROCESSES", "0") == "1"
indices = []
for raw_value in os.environ["PRIMESTRUCT_CTEST_INDICES"].split(","):
    raw_value = raw_value.strip()
    if not raw_value:
        continue
    index = int(raw_value)
    if index not in indices:
        indices.append(index)

with open(json_path, "r", encoding="utf-8") as handle:
    tests = json.load(handle)["tests"]

selected = []
for index in indices:
    if index < 1 or index > len(tests):
        print(f"[compile.sh] ERROR: direct rerun index {index} is out of range 1-{len(tests)}.", file=sys.stderr)
        sys.exit(1)
    selected.append((index, tests[index - 1]))

def wait_for_foreign_test_processes() -> None:
    waited = 0
    while True:
        foreign = list_foreign_test_processes()
        if not foreign:
            return
        if kill_foreign_test_processes:
            joined = " ".join(f"{pid}:{cwd}" for pid, cwd in foreign)
            print(
                f"[compile.sh] WARN: killing foreign PrimeStruct test processes: {joined}",
                file=sys.stderr,
                flush=True,
            )
            for pid, _cwd in foreign:
                try:
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            time.sleep(1)
            continue
        if waited >= foreign_wait_seconds:
            joined = " ".join(f"{pid}:{cwd}" for pid, cwd in foreign)
            print(
                f"[compile.sh] ERROR: timed out waiting for foreign PrimeStruct test processes: {joined}",
                file=sys.stderr,
                flush=True,
            )
            sys.exit(1)
        joined = " ".join(f"{pid}:{cwd}" for pid, cwd in foreign)
        print(f"[compile.sh] Waiting for foreign PrimeStruct test processes: {joined}", file=sys.stderr, flush=True)
        time.sleep(1)
        waited += 1

def list_foreign_test_processes():
    pattern = r"bash ./scripts/compile.sh|PrimeStruct/.*/compile.sh|ctest --output-on-failure|PrimeStruct_.*tests"
    pgrep = subprocess.run(["pgrep", "-f", pattern], capture_output=True, text=True, check=False)
    foreign = []
    for raw_pid in pgrep.stdout.splitlines():
        raw_pid = raw_pid.strip()
        if not raw_pid:
            continue
        pid = int(raw_pid)
        if pid == os.getpid():
            continue
        state = subprocess.run(
            ["ps", "-o", "state=", "-p", str(pid)],
            capture_output=True,
            text=True,
            check=False,
        ).stdout.strip()
        if state.startswith("Z"):
            continue
        lsof = subprocess.run(
            ["lsof", "-a", "-d", "cwd", "-p", str(pid), "-Fn"],
            capture_output=True,
            text=True,
            check=False,
        )
        cwd = ""
        for line in lsof.stdout.splitlines():
            if line.startswith("n"):
                cwd = line[1:]
                break
        if not cwd:
            continue
        if cwd == root_dir or cwd.startswith(root_dir + os.sep):
            continue
        foreign.append((pid, cwd))
    return foreign

for index, test in selected:
    properties = {prop["name"]: prop["value"] for prop in test.get("properties", [])}
    cwd = properties.get("WORKING_DIRECTORY", build_dir)
    timeout = properties.get("TIMEOUT")
    timeout_seconds = float(timeout) if timeout is not None else None
    command = test["command"]

    env = os.environ.copy()
    if llvm_profile_file:
        env["LLVM_PROFILE_FILE"] = llvm_profile_file
    else:
        env.pop("LLVM_PROFILE_FILE", None)

    attempts = 0
    while True:
        wait_for_foreign_test_processes()
        print(
            f"[compile.sh] Direct rerun [{index}/{len(tests)}]"
            f" attempt {attempts + 1}/{max_retries + 1}: "
            + " ".join(shlex.quote(part) for part in command),
            flush=True,
        )

        try:
            completed = subprocess.run(
                command,
                cwd=cwd,
                env=env,
                timeout=timeout_seconds,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
        except subprocess.TimeoutExpired:
            print(
                f"[compile.sh] ERROR: direct rerun timed out for {test['name']} after {timeout_seconds} seconds.",
                file=sys.stderr,
                flush=True,
            )
            sys.exit(1)

        output = completed.stdout or ""
        if output:
            print(output, end="", flush=True)

        if completed.returncode == 0:
            break

        crash_matches_signal = bool(re.search(r"test case CRASHED:\s+SIG[A-Z]+", output))
        foreign_after_failure = list_foreign_test_processes()
        should_retry = (
            attempts < max_retries
            and (
                completed.returncode < 0
                or crash_matches_signal
                or bool(foreign_after_failure)
            )
        )

        if should_retry:
            attempts += 1
            if completed.returncode < 0:
                reason = f"exited via signal {-completed.returncode}"
            elif crash_matches_signal:
                reason = "reported a doctest signal crash"
            else:
                joined = " ".join(f"{pid}:{cwd}" for pid, cwd in foreign_after_failure)
                reason = f"saw foreign PrimeStruct test processes after failure ({joined})"
            print(
                f"[compile.sh] WARN: direct rerun for {test['name']} {reason}; retrying.",
                file=sys.stderr,
                flush=True,
            )
            continue

        exit_code = completed.returncode if completed.returncode > 0 else 1
        print(
            f"[compile.sh] ERROR: direct rerun failed for {test['name']} with exit {completed.returncode}.",
            file=sys.stderr,
            flush=True,
        )
        sys.exit(exit_code)
PY
}

run_ctest_range_direct() {
  local start="$1"
  local end="$2"
  local llvm_profile_file="$3"
  local -a indices=()
  local index
  for ((index = start; index <= end; index++)); do
    indices+=("$index")
  done
  run_ctest_indices_direct "$llvm_profile_file" "${indices[@]}"
}

run_ctest_batches() {
  local prefix="$1"
  local llvm_profile_file="$2"
  shift 2

  local -a base_cmd=("$@")
  local batch_size="$TEST_BATCH_SIZE"
  local driver="$TEST_DRIVER"

  local total_tests
  total_tests="$(cd "$BUILD_DIR" && ctest -N | awk -F': ' '/Total Tests:/ {print $2}')"
  if [[ -z "$total_tests" ]]; then
    echo "[compile.sh] ERROR: unable to determine CTest test count." >&2
    return 1
  fi

  if [[ "$driver" != "ctest" ]]; then
    if [[ "$driver" != "direct" ]]; then
      echo "[compile.sh] ERROR: unsupported test driver '$driver'." >&2
      return 1
    fi

    local direct_prefix="${prefix} (direct)"
    if [[ "$batch_size" -le 0 || "$total_tests" -le "$batch_size" ]]; then
      print_command "$direct_prefix: " "ctest-json-direct" "1-$total_tests"
      run_ctest_range_direct 1 "$total_tests" "$llvm_profile_file"
      return
    fi

    local direct_start=1
    while [[ "$direct_start" -le "$total_tests" ]]; do
      local direct_end=$((direct_start + batch_size - 1))
      if [[ "$direct_end" -gt "$total_tests" ]]; then
        direct_end="$total_tests"
      fi
      print_command "$direct_prefix [$direct_start-$direct_end/$total_tests]: " "ctest-json-direct" "$direct_start-$direct_end"
      run_ctest_range_direct "$direct_start" "$direct_end" "$llvm_profile_file"
      direct_start=$((direct_end + 1))
    done
    return
  fi

  if [[ "$batch_size" -le 0 || "$total_tests" -le "$batch_size" ]]; then
    wait_for_foreign_test_processes
    if [[ -n "$llvm_profile_file" ]]; then
      print_command "$prefix" LLVM_PROFILE_FILE="$llvm_profile_file" "${base_cmd[@]}"
      run_in_build_dir_capture_status "$llvm_profile_file" "${base_cmd[@]}"
      if [[ "$RUN_IN_BUILD_DIR_STATUS" -ne 0 ]]; then
        mapfile -t failed_indices < <(collect_failed_ctest_indices 1 "$total_tests")
        if [[ "${#failed_indices[@]}" -gt 0 ]]; then
          echo "[compile.sh] WARN: ctest failed; rerunning failed tests directly." >&2
          run_ctest_indices_direct "$llvm_profile_file" "${failed_indices[@]}"
        else
          echo "[compile.sh] WARN: ctest failed; rerunning tests directly." >&2
          run_ctest_range_direct 1 "$total_tests" "$llvm_profile_file"
        fi
      fi
    else
      print_command "$prefix" "${base_cmd[@]}"
      run_in_build_dir_capture_status "" "${base_cmd[@]}"
      if [[ "$RUN_IN_BUILD_DIR_STATUS" -ne 0 ]]; then
        mapfile -t failed_indices < <(collect_failed_ctest_indices 1 "$total_tests")
        if [[ "${#failed_indices[@]}" -gt 0 ]]; then
          echo "[compile.sh] WARN: ctest failed; rerunning failed tests directly." >&2
          run_ctest_indices_direct "" "${failed_indices[@]}"
        else
          echo "[compile.sh] WARN: ctest failed; rerunning tests directly." >&2
          run_ctest_range_direct 1 "$total_tests" ""
        fi
      fi
    fi
    return
  fi

  local start=1
  while [[ "$start" -le "$total_tests" ]]; do
    local end=$((start + batch_size - 1))
    if [[ "$end" -gt "$total_tests" ]]; then
      end="$total_tests"
    fi
    local -a chunk_cmd=("${base_cmd[@]}" -I "$start","$end")
    wait_for_foreign_test_processes
    if [[ -n "$llvm_profile_file" ]]; then
      print_command "$prefix [$start-$end/$total_tests]: " LLVM_PROFILE_FILE="$llvm_profile_file" "${chunk_cmd[@]}"
      run_in_build_dir_capture_status "$llvm_profile_file" "${chunk_cmd[@]}"
      if [[ "$RUN_IN_BUILD_DIR_STATUS" -ne 0 ]]; then
        mapfile -t failed_indices < <(collect_failed_ctest_indices "$start" "$end")
        if [[ "${#failed_indices[@]}" -gt 0 ]]; then
          echo "[compile.sh] WARN: ctest failed for batch $start-$end; rerunning failed tests directly." >&2
          run_ctest_indices_direct "$llvm_profile_file" "${failed_indices[@]}"
        else
          echo "[compile.sh] WARN: ctest failed for batch $start-$end; rerunning batch directly." >&2
          run_ctest_range_direct "$start" "$end" "$llvm_profile_file"
        fi
      fi
    else
      print_command "$prefix [$start-$end/$total_tests]: " "${chunk_cmd[@]}"
      run_in_build_dir_capture_status "" "${chunk_cmd[@]}"
      if [[ "$RUN_IN_BUILD_DIR_STATUS" -ne 0 ]]; then
        mapfile -t failed_indices < <(collect_failed_ctest_indices "$start" "$end")
        if [[ "${#failed_indices[@]}" -gt 0 ]]; then
          echo "[compile.sh] WARN: ctest failed for batch $start-$end; rerunning failed tests directly." >&2
          run_ctest_indices_direct "" "${failed_indices[@]}"
        else
          echo "[compile.sh] WARN: ctest failed for batch $start-$end; rerunning batch directly." >&2
          run_ctest_range_direct "$start" "$end" ""
        fi
      fi
    fi
    start=$((end + 1))
  done
}

if [[ $COVERAGE -eq 1 ]]; then
  if [[ $RUN_TESTS -eq 0 ]]; then
    echo "[compile.sh] ERROR: --coverage requires tests; remove --skip-tests." >&2
    exit 2
  fi
  if ! command -v clang++ >/dev/null 2>&1; then
    echo "[compile.sh] ERROR: --coverage requires clang++ in PATH." >&2
    exit 2
  fi
  COVERAGE_CMAKE_ARGS+=("-DCMAKE_CXX_COMPILER=clang++")
  COVERAGE_CXX_FLAGS="${CXXFLAGS:-} -fprofile-instr-generate -fcoverage-mapping"
  COVERAGE_LINK_FLAGS="${LDFLAGS:-} -fprofile-instr-generate"
  COVERAGE_CMAKE_ARGS+=("-DCMAKE_CXX_FLAGS=${COVERAGE_CXX_FLAGS}")
  COVERAGE_CMAKE_ARGS+=("-DCMAKE_EXE_LINKER_FLAGS=${COVERAGE_LINK_FLAGS}")
fi

if [[ $CLEAN -eq 1 ]]; then
  rm -rf "$BUILD_DIR"
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  "${COVERAGE_CMAKE_ARGS[@]}"

if [[ $CONFIGURE_ONLY -eq 1 ]]; then
  exit 0
fi

BUILD_STATUS=0
build_attempt=0
while true; do
  run_build_capture_status
  if [[ "$BUILD_STATUS" -eq 0 ]]; then
    break
  fi

  if [[ "$BUILD_STATUS" -eq 137 || "$BUILD_STATUS" -eq 143 ]] && [[ "$build_attempt" -lt "$BUILD_RETRIES" ]]; then
    build_attempt=$((build_attempt + 1))
    echo "[compile.sh] WARN: build exited with status $BUILD_STATUS; retrying ($build_attempt/$BUILD_RETRIES)." >&2
    continue
  fi

  exit "$BUILD_STATUS"
done

if [[ $RUN_TESTS -eq 1 ]]; then
  trap release_test_lock EXIT
  acquire_test_lock

  if [[ $COVERAGE -eq 1 ]]; then
    LLVM_PROFDATA="${LLVM_PROFDATA:-llvm-profdata}"
    LLVM_COV="${LLVM_COV:-llvm-cov}"
    if ! command -v "$LLVM_PROFDATA" >/dev/null 2>&1; then
      if command -v xcrun >/dev/null 2>&1; then
        LLVM_PROFDATA="$(xcrun --find llvm-profdata 2>/dev/null || true)"
      fi
    fi
    if ! command -v "$LLVM_COV" >/dev/null 2>&1; then
      if command -v xcrun >/dev/null 2>&1; then
        LLVM_COV="$(xcrun --find llvm-cov 2>/dev/null || true)"
      fi
    fi
    if [[ -z "$LLVM_PROFDATA" || ! -x "$LLVM_PROFDATA" ]]; then
      echo "[compile.sh] ERROR: llvm-profdata not found; install LLVM tools or set LLVM_PROFDATA." >&2
      exit 2
    fi
    if [[ -z "$LLVM_COV" || ! -x "$LLVM_COV" ]]; then
      echo "[compile.sh] ERROR: llvm-cov not found; install LLVM tools or set LLVM_COV." >&2
      exit 2
    fi

    report_dir="$BUILD_DIR/coverage"
    profile_dir="$report_dir/profraw"
    rm -rf "$report_dir"
    mkdir -p "$profile_dir"

    test_cmd=(ctest --output-on-failure --parallel "$TEST_JOBS")
    run_ctest_batches "[compile.sh] Running tests" "$profile_dir/%p-%m.profraw" "${test_cmd[@]}"

    if ! compgen -G "$profile_dir/*.profraw" >/dev/null; then
      echo "[compile.sh] WARN: no .profraw files produced; coverage report skipped." >&2
      exit 0
    fi

    profdata="$report_dir/PrimeStruct.profdata"
    "$LLVM_PROFDATA" merge -sparse "$profile_dir"/*.profraw -o "$profdata"

    bins=(
      "$BUILD_DIR/PrimeStruct_misc_tests"
      "$BUILD_DIR/PrimeStruct_backend_tests"
      "$BUILD_DIR/PrimeStruct_semantics_tests"
      "$BUILD_DIR/PrimeStruct_text_filter_tests"
      "$BUILD_DIR/PrimeStruct_parser_tests"
    )
    for bin in "${bins[@]}"; do
      if [[ ! -x "$bin" ]]; then
        echo "[compile.sh] ERROR: coverage binary not found: $bin" >&2
        exit 2
      fi
    done

    coverage_ignore_regex="${PRIMESTRUCT_COVERAGE_IGNORE_REGEX:-.*/(third_party|tests|examples|workspaces|out|tools)/?}"
    if [[ -n "$coverage_ignore_regex" ]]; then
      echo "[compile.sh] Coverage: ignoring files matching regex: $coverage_ignore_regex"
      coverage_args=(--ignore-filename-regex "$coverage_ignore_regex")
    else
      coverage_args=()
    fi

    text_report="$report_dir/coverage.txt"
    echo "[compile.sh] Generating coverage report -> $text_report"
    "$LLVM_COV" report "${coverage_args[@]}" "${bins[@]}" -instr-profile "$profdata" > "$text_report"

    echo "[compile.sh] Generating HTML coverage -> $report_dir/html"
    "$LLVM_COV" show "${coverage_args[@]}" "${bins[@]}" -instr-profile "$profdata" \
      -format=html -output-dir "$report_dir/html" -show-instantiations -show-line-counts-or-regions
  else
    test_cmd=(ctest --output-on-failure --parallel "$TEST_JOBS")
    run_ctest_batches "[compile.sh] Running tests" "" "${test_cmd[@]}"
  fi
fi

if [[ $RUN_WASM_RUNTIME_CHECKS -eq 1 ]]; then
  "$ROOT_DIR/scripts/run_wasm_runtime_checks.sh" --build-dir "$BUILD_DIR" --primec "$BUILD_DIR/primec"
fi

if [[ $RUN_BENCHMARK -eq 1 ]]; then
  if [[ "$BUILD_TYPE" != "Release" ]]; then
    echo "[compile.sh] WARN: benchmarks run in $BUILD_TYPE mode; consider --release." >&2
  fi
  if [[ $RUN_BENCHMARK_REGRESSION -eq 1 ]]; then
    BENCH_REPORT_PATH="$BUILD_DIR/benchmarks/benchmark_report.json"
    BENCH_BASELINE_PATH="$ROOT_DIR/benchmarks/benchmark_baseline.json"
    "$ROOT_DIR/scripts/benchmark.sh" --build-dir "$BUILD_DIR" \
      --report-json "$BENCH_REPORT_PATH" \
      --baseline-json "$BENCH_BASELINE_PATH"
  else
    "$ROOT_DIR/scripts/benchmark.sh" --build-dir "$BUILD_DIR"
  fi
fi
