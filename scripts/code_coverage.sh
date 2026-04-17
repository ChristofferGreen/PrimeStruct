#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
REPORT_DIR="$BUILD_DIR/coverage"
PROFILE_DIR="$REPORT_DIR/profraw"
TEXT_REPORT="$REPORT_DIR/coverage.txt"
HTML_REPORT="$REPORT_DIR/html"
PROFDATA="$REPORT_DIR/PrimeStruct.profdata"

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

find_llvm_tool() {
  local tool_name="$1"

  if command -v "$tool_name" >/dev/null 2>&1; then
    command -v "$tool_name"
    return
  fi

  if command -v xcrun >/dev/null 2>&1; then
    xcrun --find "$tool_name" 2>/dev/null || true
    return
  fi

  printf '\n'
}

if [[ $# -ne 0 ]]; then
  echo "Usage: ./scripts/code_coverage.sh" >&2
  exit 2
fi

if ! command -v clang++ >/dev/null 2>&1; then
  echo "[code_coverage.sh] ERROR: clang++ not found in PATH." >&2
  exit 2
fi

LLVM_PROFDATA_BIN="$(find_llvm_tool llvm-profdata)"
LLVM_COV_BIN="$(find_llvm_tool llvm-cov)"

if [[ -z "$LLVM_PROFDATA_BIN" || ! -x "$LLVM_PROFDATA_BIN" ]]; then
  echo "[code_coverage.sh] ERROR: llvm-profdata not found." >&2
  exit 2
fi

if [[ -z "$LLVM_COV_BIN" || ! -x "$LLVM_COV_BIN" ]]; then
  echo "[code_coverage.sh] ERROR: llvm-cov not found." >&2
  exit 2
fi

JOBS="$(detect_jobs)"
rm -rf "$BUILD_DIR"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS:-} -fprofile-instr-generate -fcoverage-mapping" \
  -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS:-} -fprofile-instr-generate"

cmake --build "$BUILD_DIR" --parallel "$JOBS"

rm -rf "$REPORT_DIR"
mkdir -p "$PROFILE_DIR"

CTEST_JOBS="${CTEST_PARALLEL_LEVEL:-11}"
LLVM_PROFILE_FILE="$PROFILE_DIR/%p-%m.profraw" \
  ctest --test-dir "$BUILD_DIR" --output-on-failure --parallel "$CTEST_JOBS"

if ! find "$PROFILE_DIR" -name '*.profraw' -print -quit | grep -q .; then
  echo "[code_coverage.sh] ERROR: no coverage profiles were produced." >&2
  exit 2
fi

"$LLVM_PROFDATA_BIN" merge -sparse "$PROFILE_DIR"/*.profraw -o "$PROFDATA"

bins=(
  "$BUILD_DIR/PrimeStruct_misc_tests"
  "$BUILD_DIR/PrimeStruct_backend_tests"
  "$BUILD_DIR/PrimeStruct_semantics_tests"
  "$BUILD_DIR/PrimeStruct_text_filter_tests"
  "$BUILD_DIR/PrimeStruct_parser_tests"
)

for bin in "${bins[@]}"; do
  if [[ ! -x "$bin" ]]; then
    echo "[code_coverage.sh] ERROR: coverage binary not found: $bin" >&2
    exit 2
  fi
done

coverage_ignore_regex="${PRIMESTRUCT_COVERAGE_IGNORE_REGEX:-.*/(third_party|tests|examples|workspaces|out|tools)/?}"
coverage_args=()
if [[ -n "$coverage_ignore_regex" ]]; then
  coverage_args+=(--ignore-filename-regex "$coverage_ignore_regex")
fi

"$LLVM_COV_BIN" report "${coverage_args[@]}" "${bins[@]}" -instr-profile "$PROFDATA" > "$TEXT_REPORT"

"$LLVM_COV_BIN" show "${coverage_args[@]}" "${bins[@]}" -instr-profile "$PROFDATA" \
  -format=html -output-dir "$HTML_REPORT" -show-instantiations -show-line-counts-or-regions

coverage_summary="$(python3 - "$TEXT_REPORT" <<'PY'
import pathlib
import sys

report_path = pathlib.Path(sys.argv[1])
for raw_line in report_path.read_text().splitlines():
    if not raw_line.startswith("TOTAL"):
        continue

    parts = raw_line.split()
    if len(parts) < 10:
        raise SystemExit(f"[code_coverage.sh] ERROR: unexpected TOTAL row: {raw_line}")

    functions_total = int(parts[4])
    functions_missed = int(parts[5])
    functions_cover = parts[6]
    lines_total = int(parts[7])
    lines_missed = int(parts[8])
    lines_cover = parts[9]

    print(
        f"Total function coverage: {functions_cover} "
        f"({functions_total - functions_missed}/{functions_total} functions)"
    )
    print(
        f"Total line coverage: {lines_cover} "
        f"({lines_total - lines_missed}/{lines_total} lines)"
    )
    break
else:
    raise SystemExit("[code_coverage.sh] ERROR: TOTAL row not found in coverage report")
PY
)"

echo "PrimeStruct coverage summary"
printf '%s\n' "$coverage_summary"
echo "Text report: $TEXT_REPORT"
echo "HTML report: $HTML_REPORT"
echo "Profile data: $PROFDATA"
