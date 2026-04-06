#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_DIRS=("src" "include" "tests")

count_files() {
  local dir="$1"
  rg --files "$ROOT_DIR/$dir" | wc -l | awk '{ print $1 }'
}

count_lines() {
  local dir="$1"
  local total=0
  local file
  while IFS= read -r file; do
    total=$((total + $(wc -l < "$file")))
  done < <(rg --files "$ROOT_DIR/$dir")
  echo "$total"
}

total_files=0
total_lines=0

echo "PrimeStruct lines of code"
for dir in "${TARGET_DIRS[@]}"; do
  dir_files="$(count_files "$dir")"
  dir_lines="$(count_lines "$dir")"
  total_files=$((total_files + dir_files))
  total_lines=$((total_lines + dir_lines))
  echo "$dir: $dir_lines lines across $dir_files files"
done

echo "total: $total_lines lines across $total_files files"
