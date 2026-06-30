#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
BUILD_TYPE="Debug"
SKIP_TESTS=0
FAILING_TESTS_DOC="$ROOT_DIR/docs/failing_tests.md"
FAILING_TESTS_START_MARKER="<!-- compile.sh:failing-tests:start -->"
FAILING_TESTS_END_MARKER="<!-- compile.sh:failing-tests:end -->"

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

relative_path() {
  local path="$1"

  if [[ "$path" == "$ROOT_DIR" ]]; then
    printf '.\n'
  elif [[ "$path" == "$ROOT_DIR/"* ]]; then
    printf '%s\n' "${path#"$ROOT_DIR/"}"
  else
    printf '%s\n' "$path"
  fi
}

replace_managed_failing_tests_block() {
  local replacement_file="$1"
  local tmp_file="$BUILD_DIR/failing_tests.md.tmp.$$"

  mkdir -p "$BUILD_DIR"
  awk \
    -v start="$FAILING_TESTS_START_MARKER" \
    -v end="$FAILING_TESTS_END_MARKER" \
    -v replacementFile="$replacement_file" '
      BEGIN {
        while ((getline line < replacementFile) > 0) {
          replacement[++replacementCount] = line
        }
        close(replacementFile)
      }
      $0 == start {
        sawStart = 1
        inManagedBlock = 1
        print
        for (i = 1; i <= replacementCount; ++i) {
          print replacement[i]
        }
        next
      }
      $0 == end {
        sawEnd = 1
        inManagedBlock = 0
        print
        next
      }
      !inManagedBlock {
        print
      }
      END {
        if (!sawStart || !sawEnd) {
          exit 3
        }
      }
    ' "$FAILING_TESTS_DOC" > "$tmp_file"

  mv "$tmp_file" "$FAILING_TESTS_DOC"
}

update_failing_tests_doc() {
  local ctest_status="$1"
  local ctest_jobs="$2"
  local failure_log="$BUILD_DIR/Testing/Temporary/LastTestsFailed.log"
  local last_test_log="$BUILD_DIR/Testing/Temporary/LastTest.log"
  local replacement_file="$BUILD_DIR/failing_tests.generated.$$"
  local stamp build_dir_rel command_line

  mkdir -p "$BUILD_DIR"
  stamp="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
  build_dir_rel="$(relative_path "$BUILD_DIR")"
  command_line="ctest --test-dir $build_dir_rel --output-on-failure --parallel $ctest_jobs"

  {
    printf -- '- Last updated: `%s`\n' "$stamp"
    printf -- '- Build type: `%s`\n' "$BUILD_TYPE"
    printf -- '- Build dir: `%s`\n' "$build_dir_rel"
    printf -- '- Command: `%s`\n' "$command_line"

    if [[ "$ctest_status" -eq 0 ]]; then
      printf -- "- Result: no failing CTest cases.\n"
    elif [[ -s "$failure_log" ]]; then
      printf -- '- Result: `ctest` failed with status `%s`.\n' "$ctest_status"
      printf -- "- Failing CTest cases:\n"
      while IFS= read -r failure; do
        [[ -z "$failure" ]] && continue
        if [[ "$failure" == *:* ]]; then
          printf -- '  - `%s`: `%s`\n' "${failure%%:*}" "${failure#*:}"
        else
          printf -- '  - `%s`\n' "$failure"
        fi
      done < "$failure_log"
    else
      printf -- '- Result: `ctest` failed with status `%s`, but no failure entries were found.\n' "$ctest_status"
      printf -- '- Check `%s` for details.\n' "$(relative_path "$last_test_log")"
    fi
  } > "$replacement_file"

  replace_managed_failing_tests_block "$replacement_file"
  rm -f "$replacement_file"
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

DEFAULT_CTEST_JOBS=$(detect_jobs)
CTEST_JOBS="${CTEST_PARALLEL_LEVEL:-$DEFAULT_CTEST_JOBS}"
set +e
ctest --test-dir "$BUILD_DIR" --output-on-failure --parallel "$CTEST_JOBS"
CTEST_STATUS=$?
set -e

if ! update_failing_tests_doc "$CTEST_STATUS" "$CTEST_JOBS"; then
  echo "Failed to update docs/failing_tests.md." >&2
  if [[ "$CTEST_STATUS" -eq 0 ]]; then
    exit 1
  fi
fi

exit "$CTEST_STATUS"
