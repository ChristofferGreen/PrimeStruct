#!/usr/bin/env python3
"""Fail when collection helper spellings are hard-coded outside their owners (TODO-5350).

The helper spellings the compiler matches on - borrowed `_ref` helper names,
rooted same-namespace paths (`/vector/...`, `/soa/...`, `/map/...`, `/array/...`,
`/string/...`), and canonical `/std/collections/...` member paths - are owned by
`include/primec/support/CollectionHelperNames.h`. Every other file must use those
constants. Allowed owners that legitimately carry the literals:

  - the constants header itself,
  - the stdlib surface registry (the data the table is derived from),
  - the compat-spelling classifier.

Directories not yet migrated are ratcheted per file (RATCHET below): a file may not
gain literals, and the entries only ever shrink. `src/semantics` is fully migrated
and must stay at zero.

Usage: python3 scripts/check_collection_helper_literals.py [--root ROOT]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

LITERAL = re.compile(
    r'"((?:/(?:std/collections/[A-Za-z_]+(?:/[A-Za-z_]*)?/?|vector/?|soa/?|map/?|array/?|string/?)[A-Za-z_]*)'
    r'|(?:count|get|ref|at_unsafe|at|to_aos|tryAt|contains|insert)_ref)"'
)
SCANNED_SUFFIXES = {".h", ".hpp", ".cpp", ".cc"}
SCANNED_ROOTS = ("src", "include")

OWNERS = {
    "include/primec/support/CollectionHelperNames.h",
    "src/support/StdlibSurfaceRegistry.cpp",
    "src/support/StdlibSurfaceTables.h",
    "src/support/CollectionSpellingClassifier.cpp",
}

# Remaining literals in directories that have not been migrated yet. Counts are
# code occurrences (comments ignored) per file; they may only go down.
RATCHET: dict[str, int] = {}


def code_part(line: str) -> str:
    """The line without a trailing // comment (string-aware)."""
    in_string = False
    i = 0
    while i < len(line):
        c = line[i]
        if in_string:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_string = False
        elif c == '"':
            in_string = True
        elif line.startswith("//", i):
            return line[:i]
        i += 1
    return line


def count_literals(path: Path) -> int:
    count = 0
    in_block = False
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        stripped = line.lstrip()
        if in_block:
            if "*/" in line:
                in_block = False
            continue
        if stripped.startswith("/*") and "*/" not in stripped:
            in_block = True
            continue
        if stripped.startswith("//") or re.match(r"\*(\s|/|$)", stripped) or 'R"(' in line:
            continue
        count += len(LITERAL.findall(code_part(line)))
    return count


def scan(root: Path) -> dict[str, int]:
    found: dict[str, int] = {}
    for top in SCANNED_ROOTS:
        base = root / top
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix not in SCANNED_SUFFIXES or not path.is_file():
                continue
            rel = path.relative_to(root).as_posix()
            if rel in OWNERS:
                continue
            n = count_literals(path)
            if n:
                found[rel] = n
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()

    failures: list[str] = []
    for rel, n in scan(root).items():
        allowed = RATCHET.get(rel, 0)
        if n > allowed:
            failures.append(
                f"{rel}: {n} hard-coded collection helper literal(s), allowed {allowed}; "
                "use primec::collection_helpers constants (include/primec/support/CollectionHelperNames.h)"
            )
    if failures:
        print("collection helper literal check failed:", file=sys.stderr)
        for failure in failures:
            print("  " + failure, file=sys.stderr)
        return 1
    print("collection helper literal check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
