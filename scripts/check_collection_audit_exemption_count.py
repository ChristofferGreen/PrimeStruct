#!/usr/bin/env python3
"""Ratchet the number of files carrying a "*-surface-audit: exempt" marker.

The vector/soa/map surface-trace checkers (check_vector_surface_traces.py,
check_soa_surface_trace_inventory.py, check_map_surface_strict_audit.py, and
friends) let a file opt out of trace scanning by carrying one of a handful
of "<name>-surface-audit: exempt" markers in its first 10 lines. That
per-file opt-out is meant to shrink over time as Collection decoupling work
migrates files off the legacy collection surface, not to grow unnoticed as
new code is added.

This script counts every file under include/ and src/ (matching the same
SCANNED_SUFFIXES and "first 10 lines" convention as the sibling checkers)
that carries any recognized exemption marker, and fails the count exceeds a
hardcoded baseline recorded when this ratchet was introduced. It does not
enforce that the count go down -- that is handled per-leaf, elsewhere, by
removing markers as files are migrated -- it only prevents the exemption
count from silently growing.

Usage:
    python3 scripts/check_collection_audit_exemption_count.py [--root ROOT]
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


SCANNED_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx"}

# Every "<name>-surface-audit: exempt" marker recognized by any of the
# per-collection surface-trace checkers in this directory as of this
# script's introduction. Kept as a single shared list here so this ratchet
# counts a file exempted by ANY collection-surface audit, not just one.
_EXEMPT_MARKERS = (
    "vector-surface-audit: exempt",
    "soa-surface-audit: exempt",
    "map-surface-audit: exempt",
    "collection-surface-audit: exempt",
)

# Real measured count of exempt files under include/ and src/ as of
# 2026-08-21, when this ratchet was introduced. NOTE: the TODO-4704 scope
# text's "115 files as of 2026-07-06" figure is stale -- extensive
# collection-decoupling cleanup landed between 2026-07-06 and 2026-08-21
# that added and removed exempt markers across many files, so the baseline
# recorded here (126) is the actual re-measured count, not the stale one.
BASELINE_EXEMPT_FILE_COUNT = 126


def _is_exempt(text: str) -> bool:
    for line in text.splitlines()[:10]:
        if any(marker in line for marker in _EXEMPT_MARKERS):
            return True
    return False


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Ratchet the number of include/src files carrying a "
            "'*-surface-audit: exempt' marker. Fails if the count exceeds "
            "the recorded baseline; never requires it to shrink (that is "
            "handled per-leaf elsewhere)."
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


def collect_exempt_files(root: Path) -> list[str]:
    exempt: list[str] = []
    for path in iter_sources(root):
        rel_path = normalize_path(path.relative_to(root))
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            raise SystemExit(f"Unable to read {rel_path} as UTF-8: {exc}") from exc
        if _is_exempt(text):
            exempt.append(rel_path)
    return exempt


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    exempt_files = collect_exempt_files(root)
    count = len(exempt_files)

    if count > BASELINE_EXEMPT_FILE_COUNT:
        print(
            "Collection audit exemption-count ratchet failed: "
            f"{count} files carry a '*-surface-audit: exempt' marker, "
            f"exceeding the baseline of {BASELINE_EXEMPT_FILE_COUNT}.",
            file=sys.stderr,
        )
        print(
            "Either remove the marker from a migrated file, or if this "
            "growth is an intentional, reviewed exemption, raise "
            "BASELINE_EXEMPT_FILE_COUNT in "
            "scripts/check_collection_audit_exemption_count.py to match.",
            file=sys.stderr,
        )
        new_files = sorted(exempt_files)
        if len(new_files) <= 200:
            print("Exempt files:", file=sys.stderr)
            for rel_path in new_files:
                print(f"  - {rel_path}", file=sys.stderr)
        return 1

    print(
        "Collection audit exemption-count ratchet passed: "
        f"{count} exempt file(s) (baseline {BASELINE_EXEMPT_FILE_COUNT})."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
