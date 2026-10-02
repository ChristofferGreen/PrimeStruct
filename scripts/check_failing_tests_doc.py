#!/usr/bin/env python3
"""Keep docs/failing_tests.md to current failures only (TODO-5367).

Checks:
  * the file stays short (history belongs in docs/todo_archive/);
  * every bullet under `## Open Failures` names an exact ctest test
    (``- `PrimeStruct_...`: note``);
  * each listed test is registered and currently fails - a listed test that
    passes is a stale entry and fails this check.

Usage: check_failing_tests_doc.py --build-dir build-release [--root DIR] [--ctest ctest]
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

MAX_LINES = 150
BULLET = re.compile(r"^- `([^`]+)`")


def open_failures(text: str) -> list[str]:
    match = re.search(r"^## Open Failures\s*\n(.*?)(?=^## |\Z)", text, re.MULTILINE | re.DOTALL)
    if not match:
        return []
    return [m.group(1) for line in match.group(1).split("\n") if (m := BULLET.match(line))]


def check(text: str, run_test) -> list[str]:
    """`run_test(name) -> (returncode, output)`; injectable for tests."""
    problems = []
    lines = text.count("\n") + 1
    if lines > MAX_LINES:
        problems.append(f"docs/failing_tests.md has {lines} lines (max {MAX_LINES}); move history to docs/todo_archive/")
    if "## Open Failures" not in text:
        problems.append("missing `## Open Failures` section")
    for name in open_failures(text):
        code, output = run_test(name)
        if "No tests were found" in output:
            problems.append(f"{name}: not a registered ctest test")
        elif code == 0:
            problems.append(f"{name}: passes now; remove it from Open Failures")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--ctest", default="ctest")
    args = parser.parse_args()

    def run_test(name: str) -> tuple[int, str]:
        result = subprocess.run(
            [args.ctest, "--test-dir", str(args.build_dir), "-R", f"^{re.escape(name)}$", "--no-tests=error"],
            text=True, capture_output=True, check=False,
        )
        return result.returncode, result.stdout + result.stderr

    text = (args.root / "docs" / "failing_tests.md").read_text(encoding="utf-8")
    problems = check(text, run_test)
    if problems:
        print("failing_tests.md check failed:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print("failing_tests.md lists only current failures")
    return 0


if __name__ == "__main__":
    sys.exit(main())
