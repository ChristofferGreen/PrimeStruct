#!/usr/bin/env python3
"""Guard the concurrent corpus list tests/vm_threads_corpus.txt (TODO-5900).

Every test source under tests/unit that runs a program on a task schedule (`--vm-schedule-seed`, `--vm-threads`,
`vmThreadsConfig`, `vmTaskScheduleFlags`) must be named in the corpus list, either as part of the corpus that the
`PrimeStruct_vm_threads_corpus_*` ctest entries run repeatedly under `--vm-threads`, as a file that already repeats
under threads in its own entry, or as an exclusion with a reason. A new concurrent test therefore cannot be left out:
this check fails until the file is listed.

List format, one entry per line, fields separated by `|`, `#` starts a comment:
  matrix|<doctest suite>|<source file>|<n>        program-matrix suite in corpus mode, <n> threads runs per ctest entry
  cases|<doctest suite>|<source file>|<case name> doctest case run in corpus mode (CMake adds one entry per file)
  repeated|<doctest suite>|<source file>          already repeats under threads in its own ctest entry
  exclude|<source file>|<reason>                  mentions a marker but is not a concurrent program run

Usage: check_vm_threads_corpus.py [--root .] [--list tests/vm_threads_corpus.txt]
Exit status 0 when clean, 1 when findings were reported.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True

MARKERS = re.compile(r"vm-schedule-seed|--vm-threads|vmThreadsConfig|vmTaskScheduleFlags")
KINDS = {"matrix": 4, "cases": 4, "repeated": 3, "exclude": 3}


def parse_list(text: str) -> tuple[list[tuple[str, ...]], list[str]]:
    entries: list[tuple[str, ...]] = []
    problems: list[str] = []
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        fields = tuple(field.strip() for field in line.split("|"))
        kind = fields[0]
        if kind not in KINDS:
            problems.append(f"line {number}: unknown kind '{kind}'")
        elif len(fields) != KINDS[kind] or not all(fields):
            problems.append(f"line {number}: '{kind}' takes {KINDS[kind]} non-empty fields, got {len(fields)}")
        else:
            entries.append(fields)
    return entries, problems


def concurrent_sources(root: Path) -> list[str]:
    found = []
    for path in sorted((root / "tests" / "unit").rglob("*.cpp")):
        if MARKERS.search(path.read_text(errors="replace")):
            found.append(path.relative_to(root).as_posix())
    return found


def check(root: Path, list_text: str, cmake_text: str) -> list[str]:
    entries, problems = parse_list(list_text)
    listed: dict[str, str] = {}
    for fields in entries:
        kind = fields[0]
        source = fields[1] if kind == "exclude" else fields[2]
        if source in listed and (kind != "cases" or listed[source] != "cases"):
            problems.append(f"{source}: listed twice ({listed[source]} and {kind})")
        listed[source] = kind
        path = root / source
        if not path.is_file():
            problems.append(f"{source}: no such file")
            continue
        text = path.read_text(errors="replace")
        if kind == "matrix" and not fields[3].isdigit():
            problems.append(f"{source}: the runs per entry '{fields[3]}' is not a number")
        if kind in ("matrix", "repeated") and f'TEST_SUITE_BEGIN("{fields[1]}")' not in text:
            problems.append(f"{source}: does not begin doctest suite {fields[1]}")
        if kind == "matrix":
            if "vmThreadsConfig" not in text:
                problems.append(f"{source}: a matrix corpus file must run the threads config (vmThreadsConfig)")
            short = fields[1].removeprefix("primestruct.program_matrix.")
            if not re.search(rf"(?<![\w.]){re.escape(short)}=\d+", cmake_text):
                problems.append(f"{source}: CMakeLists.txt has no case count for program matrix suite {short}")
        if kind == "cases":
            if f'TEST_CASE("{fields[3]}")' not in text:
                problems.append(f"{source}: no TEST_CASE named '{fields[3]}'")
            if "vmTaskScheduleFlags" not in text:
                problems.append(f"{source}: a cases corpus file must use vmTaskScheduleFlags")
    for source in concurrent_sources(root):
        if source not in listed:
            problems.append(
                f"{source}: runs a task schedule but is not in tests/vm_threads_corpus.txt "
                "(add it as matrix, cases or repeated, or exclude it with a reason)"
            )
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--list", type=Path, default=None)
    args = parser.parse_args()
    root = args.root.resolve()
    list_path = args.list or root / "tests" / "vm_threads_corpus.txt"
    problems = check(root, list_path.read_text(), (root / "CMakeLists.txt").read_text())
    for problem in problems:
        print(f"vm threads corpus: {problem}", file=sys.stderr)
    if problems:
        return 1
    print("vm threads corpus: every concurrent test source is listed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
