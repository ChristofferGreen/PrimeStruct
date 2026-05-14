#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


SCANNED_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx"}
SCANNED_SUBDIRS = (Path("src/semantics"),)


@dataclass(frozen=True)
class TracePattern:
    id: str
    description: str
    regex: re.Pattern[str]


TRACE_PATTERNS = [
    TracePattern(
        "semantic-map-wrapper-symbol",
        "retired PrimeStruct semantic map wrapper helper symbol",
        re.compile(
            r"\bmap(?:Count|Contains|TryAt|At|AtUnsafe|Insert)(?:Ref)?\b"
        ),
    ),
    TracePattern(
        "semantic-map-wrapper-path",
        "retired PrimeStruct semantic map wrapper helper path",
        re.compile(
            r"/(?:std/collections/(?:experimental_map/)?)?"
            r"map(?:Count|Contains|TryAt|At|AtUnsafe|Insert)(?:Ref)?\b"
        ),
    ),
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Check semantic production C++ for retired PrimeStruct "
            "map wrapper helper traces. Canonical /std/collections/map/* "
            "helpers and ordinary std::map usage are allowed."
        )
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root",
    )
    return parser.parse_args()


def normalize_path(path: Path) -> str:
    return path.as_posix()


def iter_sources(root: Path) -> list[Path]:
    sources: list[Path] = []
    for rel_scan_root in SCANNED_SUBDIRS:
        scan_root = root / rel_scan_root
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


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    observed = collect_counts(root)

    if observed:
        print("Semantic map wrapper trace check failed:", file=sys.stderr)
        for (path, pattern_id), count in sorted(observed.items()):
            print(f"  - {path}: pattern {pattern_id} has {count} traces", file=sys.stderr)
        return 1

    observed_total = sum(observed.values())
    print(
        "Semantic map wrapper trace check passed. "
        f"{observed_total} retired semantic traces observed."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
