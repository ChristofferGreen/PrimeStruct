#!/usr/bin/env python3
"""Fail when a test compares two compiler dumps without normalizing them (TODO-5355).

Dump output can contain wall-clock values (`prepare_ms=91`, `*_over=false`), so a
raw `CHECK(dumpA == dumpB)` is an intermittent failure waiting to happen. In every
test file that mentions `--dump-stage`, a `CHECK(<lhs> == <rhs>)` whose operands
both look like dump/file contents (no string literal, and each side is a
`readFile(...)` call or a variable whose name contains "dump") must wrap both
sides in `stripDumpTimings(...)` from primec/testing/DumpNormalization.h.

Usage: check_dump_comparisons.py [--root DIR]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

MENTIONS_DUMP = re.compile(r"dump[-_]stage")
CHECK_START = re.compile(r"\b(?:CHECK|REQUIRE)(?:_EQ)?\s*\(")
DUMP_NAME = re.compile(r"\b\w*[dD]ump\w*\b")


def statements(text: str):
    for match in CHECK_START.finditer(text):
        depth = 1
        i = match.end()
        in_string = False
        while i < len(text) and depth:
            ch = text[i]
            if in_string:
                if ch == "\\":
                    i += 1
                elif ch == '"':
                    in_string = False
            elif ch == '"':
                in_string = True
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
            i += 1
        yield match.start(), text[match.end() : i - 1]


def split_top_level_equals(expr: str):
    depth = 0
    in_string = False
    i = 0
    while i < len(expr) - 1:
        ch = expr[i]
        if in_string:
            if ch == "\\":
                i += 1
            elif ch == '"':
                in_string = False
        elif ch == '"':
            in_string = True
        elif ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif depth == 0 and expr[i : i + 2] == "==":
            return expr[:i], expr[i + 2 :]
        i += 1
    return None


def looks_like_dump(side: str) -> bool:
    if '"' in side:
        return False
    return "readFile(" in side or DUMP_NAME.search(side) is not None


def check_text(text: str) -> list[int]:
    """Returns 1-based line numbers of raw dump comparisons."""
    if not MENTIONS_DUMP.search(text):
        return []
    lines = []
    for start, body in statements(text):
        parts = split_top_level_equals(body)
        if parts is None:
            continue
        left, right = parts
        if "stripDumpTimings(" in body:
            continue
        if looks_like_dump(left) and looks_like_dump(right):
            lines.append(text.count("\n", 0, start) + 1)
    return lines


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    root = parser.parse_args().root.resolve()
    findings = []
    for path in sorted((root / "tests" / "unit").rglob("*")):
        if path.suffix not in {".cpp", ".h", ".hpp"} or not path.is_file():
            continue
        for line in check_text(path.read_text(encoding="utf-8", errors="replace")):
            findings.append(f"{path.relative_to(root)}:{line}: raw dump comparison; wrap both sides in stripDumpTimings()")
    if findings:
        print("dump comparison check failed:")
        for finding in findings:
            print(f"  {finding}")
        return 1
    print("dump comparison check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
