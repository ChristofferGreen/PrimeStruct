#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path


SCHEMA = "primestruct_vector_surface_trace_baseline_v1"
SCANNED_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx"}


@dataclass(frozen=True)
class TracePattern:
    id: str
    description: str
    regex: re.Pattern[str]


TRACE_PATTERNS = [
    TracePattern(
        "canonical-vector-path",
        "canonical PrimeStruct vector helper path",
        re.compile(r'/?std/collections/vector(?:/|")'),
    ),
    TracePattern(
        "experimental-vector-path",
        "experimental PrimeStruct vector helper or storage path",
        re.compile(r'/?std/collections/experimental_vector(?:/|")'),
    ),
    TracePattern(
        "root-vector-helper-path",
        "rooted PrimeStruct vector helper path",
        re.compile(r'(?<![A-Za-z0-9_/])/?vector/'),
    ),
    TracePattern(
        "vector-helper-symbol",
        "PrimeStruct vector helper symbol",
        re.compile(
            r"\bvector(?:Alloc|At|AtUnsafe|Capacity|CheckShape|Clear|Count|"
            r"Destroy|DropRange|Free|InitSlot|Load|New|Pop|Push|RemoveAt|"
            r"RemoveSwap|Reserve|Store)\b"
        ),
    ),
    TracePattern(
        "vector-surface-id",
        "PrimeStruct stdlib vector surface registry id",
        re.compile(r"\bCollectionsVectorHelpers\b"),
    ),
    TracePattern(
        "vector-type-text",
        "PrimeStruct Vector<T> surface type spelling",
        re.compile(r"\bVector<"),
    ),
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Check production C++ for PrimeStruct vector-surface traces. "
            "Existing traces are tracked as a baseline; new or increased "
            "traces fail the check."
        )
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root",
    )
    parser.add_argument(
        "--baseline",
        type=Path,
        default=Path(__file__).resolve().with_name("vector_surface_trace_baseline.json"),
        help="JSON baseline of currently known production vector traces",
    )
    parser.add_argument(
        "--update-baseline",
        action="store_true",
        help="rewrite the baseline to the current observed trace counts",
    )
    return parser.parse_args()


def normalize_path(path: Path) -> str:
    return path.as_posix()


def iter_sources(root: Path) -> list[Path]:
    sources: list[Path] = []
    for scan_root in (root / "include", root / "src"):
        if not scan_root.exists():
            continue
        for path in scan_root.rglob("*"):
            if path.is_file() and path.suffix in SCANNED_SUFFIXES:
                sources.append(path)
    return sorted(sources)


def collect_counts(root: Path) -> dict[tuple[str, str], int]:
    counts: dict[tuple[str, str], int] = {}
    for path in iter_sources(root):
        rel_path = normalize_path(path.relative_to(root))
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            raise SystemExit(f"Unable to read {rel_path} as UTF-8: {exc}") from exc
        for line in text.splitlines():
            for pattern in TRACE_PATTERNS:
                matches = pattern.regex.findall(line)
                if matches:
                    key = (rel_path, pattern.id)
                    counts[key] = counts.get(key, 0) + len(matches)
    return counts


def counts_to_json(counts: dict[tuple[str, str], int]) -> dict:
    descriptions = {
        pattern.id: pattern.description
        for pattern in TRACE_PATTERNS
    }
    entries = [
        {"path": path, "pattern": pattern_id, "count": count}
        for (path, pattern_id), count in sorted(counts.items())
        if count > 0
    ]
    return {
        "schema": SCHEMA,
        "patterns": descriptions,
        "entries": entries,
    }


def load_baseline(path: Path) -> dict[tuple[str, str], int]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise SystemExit(f"Vector trace baseline is missing: {path}") from exc
    except json.JSONDecodeError as exc:
        raise SystemExit(f"Vector trace baseline is invalid JSON: {path}: {exc}") from exc
    if data.get("schema") != SCHEMA:
        raise SystemExit(f"Vector trace baseline has unsupported schema: {data.get('schema')!r}")
    counts: dict[tuple[str, str], int] = {}
    known_patterns = {pattern.id for pattern in TRACE_PATTERNS}
    for index, entry in enumerate(data.get("entries", []), 1):
        path_text = entry.get("path")
        pattern_id = entry.get("pattern")
        count = entry.get("count")
        if not isinstance(path_text, str) or not isinstance(pattern_id, str) or not isinstance(count, int):
            raise SystemExit(f"Vector trace baseline entry {index} is malformed: {entry!r}")
        if pattern_id not in known_patterns:
            raise SystemExit(f"Vector trace baseline entry {index} uses unknown pattern: {pattern_id}")
        if count < 1:
            raise SystemExit(f"Vector trace baseline entry {index} must have a positive count")
        counts[(path_text, pattern_id)] = count
    return counts


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    baseline_path = args.baseline.resolve()
    observed = collect_counts(root)

    if args.update_baseline:
        baseline_path.write_text(
            json.dumps(counts_to_json(observed), indent=2, sort_keys=False) + "\n",
            encoding="utf-8",
        )
        print(f"Updated vector trace baseline: {baseline_path}")
        return 0

    expected = load_baseline(baseline_path)
    violations: list[str] = []
    reductions: list[str] = []

    for key, observed_count in sorted(observed.items()):
        expected_count = expected.get(key, 0)
        if observed_count > expected_count:
            path, pattern_id = key
            violations.append(
                f"{path}: pattern {pattern_id} has {observed_count} traces; "
                f"baseline allows {expected_count}"
            )
        elif observed_count < expected_count:
            path, pattern_id = key
            reductions.append(
                f"{path}: pattern {pattern_id} dropped from {expected_count} to {observed_count}"
            )

    for key, expected_count in sorted(expected.items()):
        if key not in observed:
            path, pattern_id = key
            reductions.append(
                f"{path}: pattern {pattern_id} dropped from {expected_count} to 0"
            )

    if violations:
        print("Vector surface trace check failed:", file=sys.stderr)
        for violation in violations:
            print(f"  - {violation}", file=sys.stderr)
        if reductions:
            print("Observed trace reductions; update the baseline after confirming them:", file=sys.stderr)
            for reduction in reductions:
                print(f"  - {reduction}", file=sys.stderr)
        return 1

    observed_total = sum(observed.values())
    expected_total = sum(expected.values())
    print(
        "Vector surface trace check passed. "
        f"{observed_total} production traces observed; baseline ceiling is {expected_total}."
    )
    if reductions:
        print("Observed trace reductions; update the baseline when the removal is intentional:")
        for reduction in reductions:
            print(f"  - {reduction}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
