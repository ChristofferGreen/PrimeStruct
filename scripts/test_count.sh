#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-release"
MODE="auto"

usage() {
  cat <<'EOF'
Usage: ./scripts/test_count.sh [--build-dir <dir>] [--source-only | --binary-only]

Counts PrimeStruct test cases.
- Source count: counts TEST_CASE(...) macro definitions under tests/.
- Binary count: sums doctest --count output across PrimeStruct_*_tests binaries.

Defaults:
- build dir: build-release
- mode: auto (prints source count and binary count when binaries exist)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      if [[ $# -lt 2 ]]; then
        echo "error: --build-dir requires a value" >&2
        exit 1
      fi
      if [[ "$2" = /* ]]; then
        BUILD_DIR="$2"
      else
        BUILD_DIR="$ROOT_DIR/$2"
      fi
      shift 2
      ;;
    --source-only)
      MODE="source"
      shift
      ;;
    --binary-only)
      MODE="binary"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

count_source_tests() {
  rg --pcre2 -U -o '\bTEST_CASE\s*\(' "$ROOT_DIR/tests" | wc -l | awk '{print $1}'
}

count_binary_tests() {
  local total=0
  local found=0
  local binary count
  while IFS= read -r binary; do
    found=1
    count="$("$binary" --count | awk -F': ' '/test cases/{print $2; exit}')"
    if [[ -z "${count:-}" ]]; then
      echo "error: failed to parse doctest --count output from $binary" >&2
      exit 1
    fi
    printf '%s: %s\n' "$(basename "$binary")" "$count"
    total=$((total + count))
  done < <(find "$BUILD_DIR" -maxdepth 1 -type f -name 'PrimeStruct_*_tests' -perm -u+x | sort)

  if [[ "$found" -eq 0 ]]; then
    return 1
  fi

  echo "$total"
}

echo "PrimeStruct test count"
echo "root: $ROOT_DIR"

source_count=""
if [[ "$MODE" != "binary" ]]; then
  source_count="$(count_source_tests)"
  echo "source-defined TEST_CASE macros: $source_count"
fi

if [[ "$MODE" != "source" ]]; then
  echo "registered test cases from doctest binaries in: $BUILD_DIR"
  if binary_lines="$(count_binary_tests)"; then
    binary_count="$(echo "$binary_lines" | tail -n 1)"
    echo "$binary_lines" | sed '$d' | sed 's/^/  /'
    echo "binary-registered test cases: $binary_count"
    if [[ -n "$source_count" ]]; then
      echo "source-vs-binary delta: $((source_count - binary_count))"
    fi
  else
    echo "binary-registered test cases: unavailable (no PrimeStruct_*_tests executables found)"
    if [[ "$MODE" = "binary" ]]; then
      exit 1
    fi
  fi
fi
