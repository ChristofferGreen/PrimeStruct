#!/usr/bin/env python3
"""Check clang-format only on lines changed since a base ref.

Usage: scripts/check_format.py [--base REF] [--root DIR]

Compares the working tree with the merge-base of REF (default origin/master, then
master) and runs clang-format restricted to the changed line ranges of each changed
C++ file, so existing code is never reported. Exits 0 when clang-format would not
change any touched line, 1 otherwise, and skips with a message when clang-format or
the base ref is unavailable.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

SUFFIXES = {".cpp", ".h"}
HUNK = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")


def git(root: Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], cwd=root, capture_output=True, text=True, check=False)


def resolve_base(root: Path, requested: str | None) -> str | None:
    for ref in ([requested] if requested else ["origin/master", "master"]):
        if git(root, "rev-parse", "--verify", "--quiet", ref).returncode == 0:
            merge_base = git(root, "merge-base", ref, "HEAD")
            return merge_base.stdout.strip() if merge_base.returncode == 0 else ref
    return None


def changed_ranges(diff_text: str) -> dict[str, list[tuple[int, int]]]:
    ranges: dict[str, list[tuple[int, int]]] = {}
    current = None
    for line in diff_text.splitlines():
        if line.startswith("+++ b/"):
            current = line[6:]
        elif line.startswith("+++ "):
            current = None
        elif current and (match := HUNK.match(line)):
            start = int(match.group(1))
            count = int(match.group(2)) if match.group(2) is not None else 1
            if count > 0 and Path(current).suffix in SUFFIXES:
                ranges.setdefault(current, []).append((start, start + count - 1))
    return ranges


def format_problems(root: Path, path: str, spans: list[tuple[int, int]]) -> list[str]:
    command = ["clang-format", "--style=file", "--dry-run"]
    for first, last in spans:
        command.append(f"--lines={first}:{last}")
    command.append(path)
    result = subprocess.run(command, cwd=root, capture_output=True, text=True, check=False)
    problems = []
    for line in result.stderr.splitlines():
        match = re.match(r"^(.*?):(\d+):\d+: (?:warning|error): (.*)$", line)
        if match:
            problems.append(f"{match.group(1)}:{match.group(2)}: {match.group(3)}")
    if result.returncode != 0 and not problems:
        problems.append(f"{path}: clang-format failed: {result.stderr.strip()}")
    return sorted(set(problems))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()
    if shutil.which("clang-format") is None:
        print("check_format: clang-format not found; skipping")
        return 0
    base = resolve_base(root, args.base)
    if base is None:
        print("check_format: no base ref found; skipping")
        return 0
    diff = git(root, "diff", "-U0", "--no-color", base, "--")
    problems: list[str] = []
    for path, spans in sorted(changed_ranges(diff.stdout).items()):
        if (root / path).is_file():
            problems.extend(format_problems(root, path, spans))
    if problems:
        print("check_format failed (run clang-format on the touched lines):")
        for problem in problems[:100]:
            print(f"  {problem}")
        return 1
    print("check_format passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
