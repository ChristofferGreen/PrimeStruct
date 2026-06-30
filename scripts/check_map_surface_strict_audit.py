#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


SCANNED_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx"}


@dataclass(frozen=True)
class TracePattern:
    id: str
    description: str
    regex: re.Pattern[str]


@dataclass(frozen=True)
class Trace:
    rel_path: str
    line_number: int
    pattern_id: str
    text: str
    count: int


TRACE_PATTERNS = [
    TracePattern(
        "canonical-map-path",
        "canonical PrimeStruct map helper path",
        re.compile(r'/?std/collections/map(?:/|")'),
    ),
    TracePattern(
        "experimental-map-path",
        "experimental PrimeStruct map backing path",
        re.compile(r'/?std/collections/experimental_map(?:/|")'),
    ),
    TracePattern(
        "root-map-helper-path",
        "rooted PrimeStruct map helper path",
        re.compile(r'(?<![A-Za-z0-9_/])/?map/'),
    ),
    TracePattern(
        "map-helper-symbol",
        "PrimeStruct map helper symbol",
        re.compile(
            r"\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|"
            r"Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|"
            r"TryAt)(?:Ref)?\b"
        ),
    ),
    TracePattern(
        "map-backing-type-symbol",
        "PrimeStruct Map__ backing type symbol",
        re.compile(r"\bMap__"),
    ),
    TracePattern(
        "entry-backing-type-symbol",
        "PrimeStruct Entry__ backing type symbol",
        re.compile(r"\bEntry__"),
    ),
    TracePattern(
        "map-surface-id",
        "PrimeStruct stdlib map surface registry id",
        re.compile(r"\bCollectionsMap[A-Za-z0-9_]*\b"),
    ),
    TracePattern(
        "map-type-text",
        "PrimeStruct Map<K, V> surface type spelling",
        re.compile(r"\bMap<"),
    ),
]

PATTERN_BY_ID = {pattern.id: pattern for pattern in TRACE_PATTERNS}

# Counts are maxima. The final map-surface gate allows no production C++ traces.
CURRENT_ALLOWED_COUNTS: dict[tuple[str, str], int] = {}

_EXEMPT_MARKERS = (
    "map-surface-audit: exempt",
    "collection-surface-audit: exempt",
)


def _is_exempt(text: str) -> bool:
    for line in text.splitlines()[:10]:
        if any(marker in line for marker in _EXEMPT_MARKERS):
            return True
    return False


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Check tracked production C++ for PrimeStruct map surface traces. "
            "The default and --enforce-zero modes both fail on every "
            "PrimeStruct-map trace. Ordinary C++ std::map usage, generic "
            "mapping words, "
            "tests, docs, stdlib .prime files, and generated source-lock "
            "fixtures are not matched or scanned."
        )
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root",
    )
    parser.add_argument(
        "--enforce-zero",
        action="store_true",
        help="fail on every observed map surface trace",
    )
    parser.add_argument(
        "--print-inventory",
        action="store_true",
        help="print observed map surface traces and exit",
    )
    return parser.parse_args()


def normalize_path(path: Path) -> str:
    return path.as_posix()


def tracked_sources(root: Path) -> list[Path]:
    try:
        result = subprocess.run(
            ["git", "-C", str(root), "ls-files", "--", "include", "src"],
            text=True,
            capture_output=True,
            check=False,
            timeout=10,
        )
    except (OSError, subprocess.TimeoutExpired):
        return []

    if result.returncode != 0:
        return []

    sources = []
    for raw_path in result.stdout.splitlines():
        path = root / raw_path
        if path.is_file() and path.suffix in SCANNED_SUFFIXES:
            sources.append(path)
    return sorted(sources)


def recursive_sources(root: Path) -> list[Path]:
    sources: list[Path] = []
    for scan_root in (root / "include", root / "src"):
        if not scan_root.exists():
            continue
        for path in scan_root.rglob("*"):
            if path.is_file() and path.suffix in SCANNED_SUFFIXES:
                sources.append(path)
    return sorted(sources)


def iter_sources(root: Path) -> list[Path]:
    sources = tracked_sources(root)
    if sources:
        return sources
    return recursive_sources(root)


def collect_traces(root: Path) -> list[Trace]:
    traces: list[Trace] = []
    for path in iter_sources(root):
        rel_path = normalize_path(path.relative_to(root))
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            raise SystemExit(f"Unable to read {rel_path} as UTF-8: {exc}") from exc
        if _is_exempt(text):
            continue
        for line_number, line in enumerate(text.splitlines(), start=1):
            for pattern in TRACE_PATTERNS:
                count = len(pattern.regex.findall(line))
                if count:
                    traces.append(
                        Trace(
                            rel_path=rel_path,
                            line_number=line_number,
                            pattern_id=pattern.id,
                            text=line.strip(),
                            count=count,
                        )
                    )
    return traces


def collect_counts(traces: list[Trace]) -> dict[tuple[str, str], int]:
    counts: dict[tuple[str, str], int] = {}
    for trace in traces:
        key = (trace.rel_path, trace.pattern_id)
        counts[key] = counts.get(key, 0) + trace.count
    return counts


def collect_examples(traces: list[Trace]) -> dict[tuple[str, str], Trace]:
    examples: dict[tuple[str, str], Trace] = {}
    for trace in traces:
        examples.setdefault((trace.rel_path, trace.pattern_id), trace)
    return examples


def allowed_counts(enforce_zero: bool) -> dict[tuple[str, str], int]:
    if enforce_zero:
        return {}
    return CURRENT_ALLOWED_COUNTS


def collect_violations(
    observed: dict[tuple[str, str], int],
    allowed: dict[tuple[str, str], int],
) -> list[tuple[str, str, int, int]]:
    violations: list[tuple[str, str, int, int]] = []
    for (path, pattern_id), count in sorted(observed.items()):
        allowed_count = allowed.get((path, pattern_id), 0)
        if count > allowed_count:
            violations.append((path, pattern_id, count, allowed_count))
    return violations


def print_inventory(observed: dict[tuple[str, str], int]) -> None:
    for (rel_path, pattern_id), count in sorted(observed.items()):
        print(f"{rel_path}\t{pattern_id}\t{count}")


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    traces = collect_traces(root)
    observed = collect_counts(traces)

    if args.print_inventory:
        print_inventory(observed)
        return 0

    allowed = allowed_counts(args.enforce_zero)
    violations = collect_violations(observed, allowed)

    if violations:
        examples = collect_examples(traces)
        print("Map surface strict audit failed:", file=sys.stderr)
        for path, pattern_id, count, allowed_count in violations:
            description = PATTERN_BY_ID[pattern_id].description
            print(
                f"  - {path}: pattern {pattern_id} has {count} traces "
                f"(allowed {allowed_count}) [{description}]",
                file=sys.stderr,
            )
            example = examples.get((path, pattern_id))
            if example is not None:
                print(
                    f"    first: {example.rel_path}:{example.line_number}: "
                    f"{example.text}",
                    file=sys.stderr,
                )
        return 1

    observed_total = sum(observed.values())
    if args.enforce_zero:
        print(
            "Map surface strict audit passed. "
            f"{observed_total} production traces observed under zero tolerance."
        )
    else:
        print(
            "Map surface strict audit passed. "
            f"{observed_total} production traces observed."
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
