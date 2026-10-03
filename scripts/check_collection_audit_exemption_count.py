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

sys.path.insert(0, str(Path(__file__).resolve().parent))
import surface_audit_exemptions  # noqa: E402


SCANNED_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx"}

# Every "<name>-surface-audit: exempt" marker recognized by any of the
# per-collection surface-trace checkers in this directory as of this
# script's introduction. Kept as a single shared list here so this ratchet
# counts a file exempted by ANY collection-surface audit, not just one.
# Real measured count of exempt files under include/ and src/ as of
# 2026-08-21, when this ratchet was introduced. NOTE: the TODO-4704 scope
# text's "115 files as of 2026-07-06" figure is stale -- extensive
# collection-decoupling cleanup landed between 2026-07-06 and 2026-08-21
# that added and removed exempt markers across many files, so the baseline
# recorded here (126) is the actual re-measured count, not the stale one.
#
# Raised 126 -> 134 on 2026-09-19: TODO-5293 ("merge the semantics-stage
# and ir_lowerer-stage getBuiltinArrayAccessName implementations behind a
# shared classifier") and TODO-5294 ("receiver-target resolution
# consolidation"), both closed (see docs/todo_finished.md), split several
# already-exempt files (e.g. the old resolveMethodTarget seams) into new,
# more focused files -- each new file inherited the same pre-existing
# exempt status from the code it was extracted from rather than adding new
# hardcoded-collection-surface debt. Verified via `git log --diff-filter=A`
# that every file pushing the count from 126 to 134
# (SemanticsValidatorMethodTarget{ArgsPack,KeyValue,ResolutionDetail,
# String,StructSum,Vector}Resolvers.cpp,
# src/support/BuiltinArrayAccessNameClassifier.cpp,
# src/support/ReceiverElementFamilyClassifier.cpp) was added by a TODO-5293/
# TODO-5294 commit, not by unrelated new code.
# TODO-5350: moving the helper spellings into include/primec/support/
# CollectionHelperNames.h made 52 files literal-free, so their exemption markers
# were dropped and the baseline ratcheted down from 134 to 83.
# TODO-5384: splitting SemanticsValidatorExprMethodTargetResolution.cpp left one
# unit (the resolve step, which spells the "to_aos" helper literal) that still
# needs its marker next to the original one: 83 -> 84.
# TODO-5384: splitting already-exempt semantics files into smaller units left
# eight units that still spell collection helper literals and need the marker
# (the other split units carry none): 84 -> 92.
# TODO-5385: decomposing the giant validateExpr / rewriteExpr functions into phase
# units left four more units that spell collection helper literals: 92 -> 96.
# TODO-5419: the nested rewriteExprPhase6 split added two more such units: 96 -> 98.
# TODO-5403: splitting IrLowererCountAccessHelpers.cpp added one unit that spells both literals: 98 -> 99.
# TODO-5403: the file-local helper header split out of IrLowererSetupTypeMethodCallResolution.cpp: 99 -> 100.
BASELINE_EXEMPT_FILE_COUNT = 100


def _is_exempt(root: Path, rel_path: str, text: str) -> bool:
    return surface_audit_exemptions.is_exempt(root, rel_path, text, ['map', 'soa', 'vector'])


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
        if _is_exempt(root, rel_path, text):
            exempt.append(rel_path)
    return exempt


def collect_inline_markers(root: Path) -> list[str]:
    inline: list[str] = []
    for path in iter_sources(root):
        text = path.read_text(encoding="utf-8", errors="replace")
        if surface_audit_exemptions.inline_markers(text):
            inline.append(normalize_path(path.relative_to(root)))
    return inline


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    if surface_audit_exemptions.has_data_file(root):
        inline = collect_inline_markers(root)
        if inline:
            print("Surface-audit exemptions live in scripts/surface_audit_exemptions.txt (TODO-5417); "
                  "remove the in-file marker from:")
            for rel in inline:
                print(f"  - {rel}")
            return 1
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
