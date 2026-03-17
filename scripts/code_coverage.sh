#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
REPORT_DIR="$BUILD_DIR/coverage"
TEXT_REPORT="$REPORT_DIR/coverage.txt"
HTML_REPORT="$REPORT_DIR/html"
PROFDATA="$REPORT_DIR/PrimeStruct.profdata"

"$ROOT_DIR/scripts/compile.sh" --clean --coverage "$@"

if [[ ! -f "$TEXT_REPORT" ]]; then
  echo "[code_coverage.sh] ERROR: coverage text report not found: $TEXT_REPORT" >&2
  exit 2
fi

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
