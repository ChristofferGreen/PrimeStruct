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

_EXEMPT_MARKERS = (
    "vector-surface-audit: exempt",
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
            "Check production C++ for PrimeStruct vector-surface traces. "
            "Any trace in include/ or src/ fails the check; tests, docs, "
            "stdlib .prime files, and ordinary C++ std::vector uses are not "
            "scanned."
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
        if _is_exempt(text):
            continue
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
        print("Vector surface trace check failed:", file=sys.stderr)
        for (path, pattern_id), count in sorted(observed.items()):
            print(f"  - {path}: pattern {pattern_id} has {count} traces", file=sys.stderr)
        return 1

    observed_total = sum(observed.values())
    print(
        "Vector surface trace check passed. "
        f"{observed_total} production traces observed."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
