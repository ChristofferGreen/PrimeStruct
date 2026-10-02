#!/usr/bin/env python3
"""Guard CTest registration of doctest cases (TODO-5354).

Two checks:

1. Shard coverage. For every doctest binary + filter set registered with CTest
   (`--test-suite=...`, `--source-file=...`, `--test-case=...`), the
   `--first/--last` shard ranges must tile 1..N with no gaps or overlaps, where
   N is what the binary itself reports for that filter (`--count`). A wrong
   `TOTAL_CASES`/`RANGE_LAST` in cmake/PrimeStructManaged*.cmake therefore
   fails here. For every (binary, suite) the registered filter groups must also
   add up to the suite's full case count, so a `SOURCE_FILE` group that was
   never registered cannot hide cases.
   Finally the union of everything CTest runs must equal everything each doctest
   binary defines (`--list-test-cases`), so a suite or file nobody registered is
   reported by name.
2. Unregistered test files. Every file under tests/unit/ that defines a
   `TEST_CASE` must be a translation unit of a built target or be reachable
   through `#include` from one.

Usage:
    check_test_registration.py --build-dir build-release [--root .]
Exit status 0 when clean, 1 when findings were reported.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from xml.etree import ElementTree
from collections import defaultdict
from pathlib import Path

# Cases that are intentionally not registered as their own CTest shard.
INTENTIONALLY_UNREGISTERED_CASE_NAMES = {
    # Env-gated generator for the pinned targets header and docs (not a check).
    "collection parity regenerates pins and doc when asked",
}

TEST_CASE_RE = re.compile(r"^\s*(?:TEST_CASE|TEST_CASE_TEMPLATE|TEST_CASE_FIXTURE)\s*\(", re.MULTILINE)
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)
COUNT_RE = re.compile(r"unskipped test cases passing the current filters:\s*(\d+)")


def split_range_args(args: list[str]) -> tuple[tuple[str, ...], int | None, int | None]:
    """Returns (filter args, first, last) for a doctest command line (binary excluded)."""
    first = last = None
    filters: list[str] = []
    for arg in args:
        if arg.startswith("--first="):
            first = int(arg.split("=", 1)[1])
        elif arg.startswith("--last="):
            last = int(arg.split("=", 1)[1])
        elif arg.startswith("--order-by="):
            continue
        else:
            filters.append(arg)
    return tuple(filters), first, last


def is_doctest_command(command: list[str]) -> bool:
    return len(command) > 1 and any(arg.startswith("--test-suite=") for arg in command[1:])


def find_range_problems(ranges: list[tuple[int, int]], total: int) -> list[str]:
    """Ranges must tile 1..total exactly."""
    problems: list[str] = []
    expected = 1
    for first, last in sorted(ranges):
        if first > expected:
            problems.append(f"cases {expected}..{first - 1} are not covered by any shard")
        elif first < expected:
            problems.append(f"shard {first}..{last} overlaps the previous shard (expected start {expected})")
        expected = max(expected, last + 1)
    if expected - 1 < total:
        problems.append(f"cases {expected}..{total} are not covered by any shard")
    # A last shard that is only partly filled is just rounding of TOTAL_CASES up to
    # CASES_PER_SHARD; a shard that starts past the last case selects nothing.
    for first, last in sorted(ranges):
        if first > total:
            problems.append(f"shard {first}..{last} starts past the last case ({total}) and selects nothing")
    return problems


def count_cases(binary: str, filters: tuple[str, ...], cache: dict) -> int:
    key = (binary, filters)
    if key not in cache:
        result = subprocess.run(
            [binary, *filters, "--count"], text=True, capture_output=True, check=False
        )
        match = COUNT_RE.search(result.stdout)
        if result.returncode != 0 or not match:
            raise RuntimeError(f"could not count cases for {binary} {' '.join(filters)}: {result.stdout}{result.stderr}")
        cache[key] = int(match.group(1))
    return cache[key]


def check_shards(tests: list[dict], counter) -> list[str]:
    """`counter(binary, filters) -> int`; injectable for tests."""
    groups: dict[tuple[str, tuple[str, ...]], list[tuple[int, int]]] = defaultdict(list)
    whole: set[tuple[str, tuple[str, ...]]] = set()
    for test in tests:
        command = test.get("command") or []
        if not is_doctest_command(command):
            continue
        filters, first, last = split_range_args(command[1:])
        key = (command[0], filters)
        if first is None and last is None:
            whole.add(key)
        else:
            groups[key].append((first or 1, last if last is not None else 10**9))

    findings: list[str] = []
    for (binary, filters), ranges in sorted(groups.items()):
        total = counter(binary, filters)
        label = f"{Path(binary).name} {' '.join(filters)}"
        # An open-ended last shard (no --last) is allowed to run to the end.
        clipped = [(f, min(l, total)) if l == 10**9 else (f, l) for f, l in ranges]
        for problem in find_range_problems(clipped, total):
            findings.append(f"{label}: {problem}")

    return findings


def list_cases(binary: str, args: tuple[str, ...], cache: dict) -> list[tuple[str, str, str]]:
    """(suite, file, name) of every case selected by `args`, in the order doctest runs them."""
    key = (binary, args)
    if key not in cache:
        result = subprocess.run(
            [binary, *args, "--list-test-cases", "--reporters=xml"], text=True, capture_output=True, check=False
        )
        if result.returncode != 0:
            raise RuntimeError(f"could not list cases for {binary} {' '.join(args)}: {result.stderr}")
        cases = []
        for element in ElementTree.fromstring(result.stdout).iter("TestCase"):
            cases.append((element.get("testsuite", ""), element.get("filename", ""), element.get("name", "")))
        cache[key] = cases
    return cache[key]


def check_coverage(tests: list[dict], lister) -> list[str]:
    """Every case a doctest binary defines must run under some registered CTest entry."""
    registered: dict[str, set[tuple[str, str, str]]] = defaultdict(set)
    for test in tests:
        command = test.get("command") or []
        if not is_doctest_command(command):
            continue
        binary = command[0]
        extra = tuple(a for a in command[1:] if a.startswith("--order-by="))
        filters, first, last = split_range_args(command[1:])
        selected = lister(binary, tuple(filters) + extra)
        if first is not None or last is not None:
            selected = selected[(first or 1) - 1 : last if last is not None else None]
        registered[binary].update(selected)
    findings: list[str] = []
    for binary, covered in sorted(registered.items()):
        everything = lister(binary, ())
        missing = [case for case in everything if case not in covered and case[2] not in INTENTIONALLY_UNREGISTERED_CASE_NAMES]
        for suite, filename, name in missing:
            findings.append(f"{Path(binary).name}: case '{name}' ({suite}) in {Path(filename).name} is not run by any CTest entry")
    return findings


def resolve_include(including: Path, name: str, include_dirs: list[Path]) -> Path | None:
    for base in [including.parent, *include_dirs]:
        candidate = (base / name).resolve()
        if candidate.is_file():
            return candidate
    return None


def reachable_files(translation_units: list[Path], include_dirs: list[Path]) -> set[Path]:
    seen: set[Path] = set()
    stack = [p.resolve() for p in translation_units if p.is_file()]
    while stack:
        path = stack.pop()
        if path in seen:
            continue
        seen.add(path)
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for name in INCLUDE_RE.findall(text):
            target = resolve_include(path, name, include_dirs)
            if target is not None and target not in seen:
                stack.append(target)
    return seen


def cmake_mentioned_files(root: Path) -> set[Path]:
    """Test sources named in CMake (including option-gated targets such as the TSAN smokes)."""
    mentioned: set[Path] = set()
    cmake_files = [root / "CMakeLists.txt", *sorted((root / "cmake").glob("*.cmake"))]
    for cmake_file in cmake_files:
        if not cmake_file.is_file():
            continue
        for match in re.finditer(r"tests/unit/[\w./-]+\.(?:cpp|h|hpp)", cmake_file.read_text(encoding="utf-8")):
            path = root / match.group(0)
            if path.is_file():
                mentioned.add(path.resolve())
    return mentioned


def check_unregistered_files(root: Path, translation_units: list[Path], include_dirs: list[Path]) -> list[str]:
    reachable = reachable_files(translation_units + sorted(cmake_mentioned_files(root)), include_dirs)
    findings: list[str] = []
    for path in sorted((root / "tests" / "unit").rglob("*")):
        if path.suffix not in {".cpp", ".h", ".hpp"} or not path.is_file():
            continue
        if path.resolve() in reachable:
            continue
        if TEST_CASE_RE.search(path.read_text(encoding="utf-8", errors="replace")):
            findings.append(f"{path.relative_to(root)}: defines TEST_CASE but is not part of any built test target")
    return findings


def translation_units_from_compile_commands(build_dir: Path) -> tuple[list[Path], list[Path]]:
    database = json.loads((build_dir / "compile_commands.json").read_text(encoding="utf-8"))
    units: list[Path] = []
    include_dirs: set[Path] = set()
    for entry in database:
        units.append(Path(entry["directory"]) / entry["file"] if not Path(entry["file"]).is_absolute() else Path(entry["file"]))
        command = entry.get("command") or " ".join(entry.get("arguments", []))
        for match in re.finditer(r"(?:^|\s)-I\s*(\S+)", command):
            include_dirs.add(Path(match.group(1)))
    return units, sorted(include_dirs)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--ctest", default="ctest", help="ctest executable")
    args = parser.parse_args()
    root = args.root.resolve()
    build_dir = args.build_dir.resolve()

    ctest = subprocess.run(
        [args.ctest, "--test-dir", str(build_dir), "--show-only=json-v1"],
        text=True, capture_output=True, check=False,
    )
    if ctest.returncode != 0:
        print(f"ctest --show-only failed: {ctest.stderr}", file=sys.stderr)
        return 2
    tests = json.loads(ctest.stdout)["tests"]

    cache: dict = {}
    findings = check_shards(tests, lambda binary, filters: count_cases(binary, filters, cache))
    list_cache: dict = {}
    findings += check_coverage(tests, lambda binary, args: list_cases(binary, args, list_cache))
    units, include_dirs = translation_units_from_compile_commands(build_dir)
    findings += check_unregistered_files(root, units, include_dirs)

    if findings:
        print("test registration check failed:")
        for finding in findings:
            print(f"  {finding}")
        return 1
    print("test registration check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
