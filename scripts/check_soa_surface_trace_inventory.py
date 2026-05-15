#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


SCANNED_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx"}


@dataclass(frozen=True)
class TracePattern:
    id: str
    description: str
    regex: re.Pattern[str]


TRACE_PATTERNS = [
    TracePattern(
        "canonical-soa-path",
        "canonical PrimeStruct soa helper path",
        re.compile(r'/?std/collections/soa(?:/|")'),
    ),
    TracePattern(
        "legacy-soa-vector-path",
        "retired PrimeStruct soa_vector helper path",
        re.compile(r'/?std/collections/soa_vector(?:/|")'),
    ),
    TracePattern(
        "experimental-soa-vector-path",
        "experimental PrimeStruct SoA backing path",
        re.compile(r'/?std/collections/experimental_soa_vector(?:/|")'),
    ),
    TracePattern(
        "root-soa-vector-path",
        "rooted retired PrimeStruct soa_vector helper path",
        re.compile(r"(?<![A-Za-z0-9_/])/?soa_vector/"),
    ),
    TracePattern(
        "soa-vector-token",
        "retired PrimeStruct soa_vector token",
        re.compile(r"(?<![/A-Za-z0-9_])soa_vector\b(?!/)"),
    ),
    TracePattern(
        "experimental-soa-vector-token",
        "experimental PrimeStruct SoA token",
        re.compile(r"\bexperimental_soa_vector[A-Za-z0-9_]*\b"),
    ),
    TracePattern(
        "soa-vector-type-symbol",
        "PrimeStruct SoaVector backing type symbol",
        re.compile(r"\bSoaVector(?:__)?\b"),
    ),
    TracePattern(
        "soa-vector-helper-symbol",
        "PrimeStruct soaVector helper symbol",
        re.compile(r"\bsoaVector[A-Za-z0-9_]*\b"),
    ),
    TracePattern(
        "soa-conversion-helper",
        "PrimeStruct SoA public conversion helper",
        re.compile(r"\b(?:from_aos|to_aos)\b"),
    ),
    TracePattern(
        "soa-surface-id",
        "PrimeStruct stdlib SoA surface registry id",
        re.compile(r"\bCollectionsSoa[A-Za-z0-9_]*\b"),
    ),
]

PATTERN_BY_ID = {pattern.id: pattern for pattern in TRACE_PATTERNS}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Check production C++ for PrimeStruct SoA public-surface traces. "
            "Any public collection trace in include/ or src/ fails. Generic "
            "SoA substrate terms such as SoaColumn, "
            "SoaFieldView, SoaSchema, field-layout helpers, borrow and "
            "invalidation machinery, and allocation primitives are not "
            "matched by this public-surface zero audit."
        )
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root",
    )
    parser.add_argument(
        "--print-inventory",
        action="store_true",
        help="print observed public-surface traces and exit",
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


def print_inventory(observed: dict[tuple[str, str], int]) -> None:
    for (rel_path, pattern_id), count in sorted(observed.items()):
        print(f"{rel_path}\t{pattern_id}\t{count}")


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    observed = collect_counts(root)

    if args.print_inventory:
        print_inventory(observed)
        return 0

    if observed:
        print("SoA surface trace zero audit failed:", file=sys.stderr)
        for (rel_path, pattern_id), count in sorted(observed.items()):
            description = PATTERN_BY_ID[pattern_id].description
            print(
                f"  - {rel_path}: pattern {pattern_id} has {count} traces "
                f"[{description}]",
                file=sys.stderr,
            )
        return 1

    observed_total = sum(observed.values())
    print(
        "SoA surface trace zero audit passed. "
        f"{observed_total} production traces observed."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
