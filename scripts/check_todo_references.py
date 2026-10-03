#!/usr/bin/env python3
"""Audit TODO-id citations in src/ and include/ comments (TODO-5396).

Every `TODO-NNNN` cited in production code must exist in docs/todo.md (open
work) or under docs/todo_archive/ (finished work), so a citation is always a
resolvable pointer. Citations of finished ids are history; their count may only
shrink (BASELINE_CLOSED_CITATIONS), which pushes comments toward plain
explanations instead of ticket numbers.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

SCAN_DIRS = ("src", "include")
BASELINE_CLOSED_CITATIONS = 315
ID_RE = re.compile(r"TODO-(\d+)")


def known_ids(root: Path) -> tuple[set[str], set[str]]:
    open_ids = set(ID_RE.findall((root / "docs" / "todo.md").read_text(encoding="utf-8")))
    archived: set[str] = set()
    for path in sorted((root / "docs" / "todo_archive").glob("*.md")):
        archived |= set(ID_RE.findall(path.read_text(encoding="utf-8", errors="replace")))
    return open_ids, archived


def scan(root: Path, open_ids: set[str], archived: set[str]) -> tuple[list[str], int]:
    dangling: list[str] = []
    closed = 0
    for scan_dir in SCAN_DIRS:
        for path in sorted((root / scan_dir).rglob("*")):
            if path.suffix not in {".cpp", ".h"} or not path.is_file():
                continue
            for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                for ident in ID_RE.findall(line):
                    if ident in open_ids:
                        continue
                    if ident in archived:
                        closed += 1
                    else:
                        dangling.append(f"{path.relative_to(root).as_posix()}:{number}: TODO-{ident} is not in docs/todo.md or docs/todo_archive/")
    return dangling, closed


def evaluate(dangling: list[str], closed: int) -> list[str]:
    problems = list(dangling)
    if closed > BASELINE_CLOSED_CITATIONS:
        problems.append(f"{closed} citations of finished TODO ids exceed the baseline of {BASELINE_CLOSED_CITATIONS}; "
                        "explain the behavior in the comment instead of citing a ticket")
    elif closed < BASELINE_CLOSED_CITATIONS:
        problems.append(f"only {closed} citations of finished TODO ids remain; lower BASELINE_CLOSED_CITATIONS "
                        f"from {BASELINE_CLOSED_CITATIONS} to {closed}")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    root = parser.parse_args().root.resolve()
    open_ids, archived = known_ids(root)
    dangling, closed = scan(root, open_ids, archived)
    problems = evaluate(dangling, closed)
    if problems:
        print("todo reference audit failed:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print(f"todo reference audit passed ({closed} citations of finished ids, baseline {BASELINE_CLOSED_CITATIONS})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
