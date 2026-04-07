#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_DIRS=("$ROOT_DIR/src" "$ROOT_DIR/include" "$ROOT_DIR/tests")
TOP_N=10

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  echo "Usage: ./scripts/top_lines_of_code.sh [top_n]"
  echo
  echo "Shows the files with the most lines in src/, include/, and tests/."
  echo "Defaults to top 10."
  exit 0
fi

if [[ $# -gt 0 ]]; then
  if ! [[ "$1" =~ ^[0-9]+$ ]] || [[ "$1" -le 0 ]]; then
    echo "error: top_n must be a positive integer" >&2
    exit 1
  fi
  TOP_N="$1"
fi

echo "PrimeStruct top $TOP_N files by lines of code (src/include/tests)"
printf "%8s  %s\n" "lines" "file"

while IFS= read -r file; do
  line_count="$(wc -l < "$file")"
  printf "%s\t%s\n" "$line_count" "${file#"$ROOT_DIR"/}"
done < <(rg --files "${TARGET_DIRS[@]}") \
  | sort -t $'\t' -k1,1nr -k2,2 \
  | awk -F $'\t' -v top="$TOP_N" 'NR <= top { printf "%8d  %s\n", $1, $2 }'
