#!/usr/bin/env python3
"""Measure duplicated code in a test directory (TODO-5356).

Counts 6-line windows (whitespace-normalized, at least 120 characters) that
occur more than once and reports the excess, the share of all windows, and the
most repeated windows with the number of files that contain them.

  python3 scripts/measure_test_duplication.py tests/unit/ir_pipeline/validation
"""

from __future__ import annotations

import collections
import re
import sys
from pathlib import Path

WINDOW = 6
MIN_CHARS = 120


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "tests/unit/ir_pipeline/validation")
    counts: collections.Counter[str] = collections.Counter()
    where: dict[str, set[str]] = collections.defaultdict(set)
    total_lines = windows = files = 0
    for path in sorted(root.rglob("*")):
        if path.suffix not in {".cpp", ".h"} or not path.is_file():
            continue
        files += 1
        lines = [re.sub(r"\s+", " ", l.strip()) for l in path.read_text(errors="replace").splitlines()]
        total_lines += len(lines)
        for i in range(len(lines) - WINDOW + 1):
            block = " | ".join(lines[i:i + WINDOW])
            if len(block) < MIN_CHARS:
                continue
            windows += 1
            counts[block] += 1
            where[block].add(path.name)
    excess = sum(c - 1 for c in counts.values() if c > 1)
    print(f"{files} files, {total_lines} lines, {windows} windows; duplicated excess {excess} "
          f"({100.0 * excess / max(windows, 1):.1f}%)")
    for block, c in counts.most_common(10):
        print(f"{c:5d}x in {len(where[block]):3d} files: {block[:140]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
