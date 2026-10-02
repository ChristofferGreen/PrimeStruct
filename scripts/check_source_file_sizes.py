#!/usr/bin/env python3
"""Keep src/semantics source files small (TODO-5353).

No `.cpp`/`.h` under src/semantics may exceed MAX_LINES unless it is listed in
scripts/source_file_size_allowlist.txt (`<path> <max lines> <reason>` per line).
An allowlisted file may not grow past its recorded limit, and an entry whose
file is back under MAX_LINES (or gone) is stale and must be removed, so the
list only shrinks.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

MAX_LINES = 1200
SCAN_DIR = "src/semantics"
ALLOWLIST = "scripts/source_file_size_allowlist.txt"


def parse_allowlist(text: str) -> dict[str, tuple[int, str]]:
    entries: dict[str, tuple[int, str]] = {}
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 2)
        if len(parts) < 3 or not parts[1].isdigit():
            raise ValueError(f"bad allowlist line (need '<path> <max lines> <reason>'): {raw}")
        entries[parts[0]] = (int(parts[1]), parts[2])
    return entries


def find_violations(sizes: dict[str, int], allowlist: dict[str, tuple[int, str]]) -> list[str]:
    problems: list[str] = []
    for path, lines in sorted(sizes.items()):
        if path in allowlist:
            limit, _ = allowlist[path]
            if lines > limit:
                problems.append(f"{path}: {lines} lines exceeds its allowlisted limit {limit}")
            elif lines <= MAX_LINES:
                problems.append(f"{path}: {lines} lines is back under {MAX_LINES}; remove its allowlist entry")
        elif lines > MAX_LINES:
            problems.append(f"{path}: {lines} lines exceeds {MAX_LINES}; split it or allowlist it with a reason")
    for path in sorted(set(allowlist) - set(sizes)):
        problems.append(f"{path}: allowlisted but missing; remove its allowlist entry")
    return problems


def collect_sizes(root: Path) -> dict[str, int]:
    sizes: dict[str, int] = {}
    for path in sorted((root / SCAN_DIR).rglob("*")):
        if path.suffix in {".cpp", ".h"} and path.is_file():
            sizes[path.relative_to(root).as_posix()] = len(path.read_text(encoding="utf-8", errors="replace").splitlines())
    return sizes


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    root = parser.parse_args().root.resolve()
    allowlist = parse_allowlist((root / ALLOWLIST).read_text(encoding="utf-8"))
    problems = find_violations(collect_sizes(root), allowlist)
    if problems:
        print("source file size check failed:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print(f"source file size check passed ({len(allowlist)} allowlisted files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
