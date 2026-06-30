#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


SCANNED_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx"}
SCANNED_ROOTS = ("include", "src")
IGNORED_PREFIXES = (
    "include/primec/testing/",
)


@dataclass(frozen=True)
class TracePattern:
    id: str
    category: str
    description: str
    regex: re.Pattern[str]


@dataclass(frozen=True)
class Trace:
    rel_path: str
    line_number: int
    pattern_id: str
    category: str
    text: str
    count: int


TRACE_PATTERNS = [
    TracePattern(
        "vector-bridge-key",
        "stdlib-bridge-key",
        "PrimeStruct vector stdlib bridge key",
        re.compile(r"\bcollections\.vector_(?:helpers|constructors)\b"),
    ),
    TracePattern(
        "map-bridge-key",
        "stdlib-bridge-key",
        "PrimeStruct map stdlib bridge key",
        re.compile(r"\bcollections\.map_(?:helpers|constructors)\b"),
    ),
    TracePattern(
        "vector-registry-id",
        "stdlib-registry-id",
        "PrimeStruct vector stdlib surface registry id",
        re.compile(r"\b(?:StdlibSurfaceId::)?CollectionsVector[A-Za-z0-9_]*\b"),
    ),
    TracePattern(
        "map-registry-id",
        "stdlib-registry-id",
        "PrimeStruct map stdlib surface registry id",
        re.compile(r"\b(?:StdlibSurfaceId::)?CollectionsMap[A-Za-z0-9_]*\b"),
    ),
    TracePattern(
        "vector-statement-helper-api",
        "vector-statement-helper",
        "compiler-owned vector statement helper API",
        re.compile(
            r"\b(?:tryEmitVectorStatementHelper|validateVectorStatementHelper"
            r"(?:Receiver|Target)?|resolveExperimentalVectorMutatorAliasName|"
            r"isStdCollectionsVectorWrapperMutatorAliasPath)\b"
        ),
    ),
    TracePattern(
        "vector-helper-classifier-api",
        "vector-helper-classifier",
        "compiler-owned vector helper name classifier",
        re.compile(
            r"\b(?:isVectorBuiltinName|vectorReceiverHasVisibleCanonicalHelper|"
            r"resolveExperimentalVectorReadHelperAliasName)\b"
        ),
    ),
    TracePattern(
        "vector-helper-path-builder",
        "vector-helper-classifier",
        "compiler-owned vector helper path builder",
        re.compile(r"\b(?:canonicalCollectionMemberPath|collectionMemberPath)\(\s*\"vector\""),
    ),
    TracePattern(
        "vector-literal-path",
        "vector-literal-path",
        "compiler-owned vector literal diagnostics or lowering",
        re.compile(r"\b(?:VectorLiteral|vector literal|vector-literal)\b"),
    ),
    TracePattern(
        "vector-backing-classifier-api",
        "vector-backing-classifier",
        "compiler-owned vector backing/value classifier",
        re.compile(
            r"\b(?:isExperimentalVector[A-Za-z0-9_]*|"
            r"resolveExperimentalVectorValueTarget|resolveExperimentalVectorTarget|"
            r"resolvesExperimentalVector[A-Za-z0-9_]*|"
            r"extractExperimentalVector[A-Za-z0-9_]*|"
            r"extractVectorValueTypeFromTypeText|isVectorValue(?:Local)?|"
            r"usesBuiltinVectorValueStorage|specializedExperimentalVectorStructPathForElementType)\b"
        ),
    ),
    TracePattern(
        "vector-backing-path-builder",
        "vector-backing-classifier",
        "compiler-owned vector backing path builder",
        re.compile(
            r"\b(?:experimentalCollectionTypePath|experimentalCollectionMemberRoot|"
            r"normalizeBuiltinCollectionStructPath)\(\s*\"vector\""
        ),
    ),
    TracePattern(
        "map-literal-path",
        "map-literal-path",
        "compiler-owned map literal diagnostics or lowering",
        re.compile(r"\b(?:MapLiteral|map literal|map-literal|collectMapLiteralEntries|isMapLiteralAssignPair)\b"),
    ),
    TracePattern(
        "map-helper-classifier-api",
        "map-helper-classifier",
        "compiler-owned map helper/access classifier",
        re.compile(
            r"\b(?:isKeyValueBuiltinName|isKeyValue(?:AccessName|HelperMethod|ImportAlias[A-Za-z0-9_]*))\b"
        ),
    ),
    TracePattern(
        "map-helper-path-builder",
        "map-helper-classifier",
        "compiler-owned map helper path builder",
        re.compile(r"\b(?:canonicalCollectionMemberPath|collectionMemberPath)\(\s*\"map\""),
    ),
    TracePattern(
        "map-backing-classifier-api",
        "map-backing-classifier",
        "compiler-owned map backing/value classifier",
        re.compile(
            r"\b(?:isExperimentalMapStructTypePath|resolveExperimentalMapValueTarget|"
            r"resolveExperimentalMapTarget|inferPublishedExperimentalMapStructPathFromConstructorPath|"
            r"resolveSpecializedExperimentalMapStructPath(?:ForBindingType)?|"
            r"isMapCollectionTypeName(?:Local)?|isMapConstructorDirectTargetPath|"
            r"isMapValue|MapValue)\b"
        ),
    ),
    TracePattern(
        "map-backing-path-builder",
        "map-backing-classifier",
        "compiler-owned map backing path builder",
        re.compile(r"\b(?:experimentalCollectionTypePath|normalizeBuiltinCollectionStructPath)\(\s*\"map\""),
    ),
]

PATTERN_BY_ID = {pattern.id: pattern for pattern in TRACE_PATTERNS}

_EXEMPT_MARKERS = (
    "map-vector-compiler-knowledge: exempt",
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
            "Inventory PrimeStruct map/vector compiler knowledge in tracked "
            "production C++ under include/ and src/. The default mode reports "
            "the current nonzero inventory and exits successfully. "
            "--enforce-zero turns the inventory into a failing zero audit."
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
        help="fail when any map/vector compiler-knowledge trace remains",
    )
    parser.add_argument(
        "--require-zero-category",
        action="append",
        default=[],
        metavar="CATEGORY",
        help="fail when a specific inventory category has any trace",
    )
    return parser.parse_args()


def normalize_path(path: Path) -> str:
    return path.as_posix()


def is_repo_root(root: Path) -> bool:
    try:
        result = subprocess.run(
            ["git", "-C", str(root), "rev-parse", "--show-toplevel"],
            text=True,
            capture_output=True,
            check=False,
            timeout=10,
        )
    except (OSError, subprocess.TimeoutExpired):
        return False
    if result.returncode != 0:
        return False
    return Path(result.stdout.strip()).resolve() == root.resolve()


def should_scan_rel_path(rel_path: str, suffix: str) -> bool:
    if suffix not in SCANNED_SUFFIXES:
        return False
    if any(rel_path.startswith(prefix) for prefix in IGNORED_PREFIXES):
        return False
    return rel_path.startswith("include/") or rel_path.startswith("src/")


def tracked_sources(root: Path) -> list[Path]:
    if not is_repo_root(root):
        return []
    try:
        result = subprocess.run(
            ["git", "-C", str(root), "ls-files", "--", *SCANNED_ROOTS],
            text=True,
            capture_output=True,
            check=False,
            timeout=10,
        )
    except (OSError, subprocess.TimeoutExpired):
        return []
    if result.returncode != 0:
        return []

    sources: list[Path] = []
    for raw_path in result.stdout.splitlines():
        path = root / raw_path
        if path.is_file() and should_scan_rel_path(raw_path, path.suffix):
            sources.append(path)
    return sorted(sources)


def recursive_sources(root: Path) -> list[Path]:
    sources: list[Path] = []
    for scan_root_name in SCANNED_ROOTS:
        scan_root = root / scan_root_name
        if not scan_root.exists():
            continue
        for path in scan_root.rglob("*"):
            if not path.is_file():
                continue
            rel_path = normalize_path(path.relative_to(root))
            if should_scan_rel_path(rel_path, path.suffix):
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
                            category=pattern.category,
                            text=line.strip(),
                            count=count,
                        )
                    )
    return traces


def collect_category_counts(traces: list[Trace]) -> dict[str, int]:
    counts: dict[str, int] = {}
    for trace in traces:
        counts[trace.category] = counts.get(trace.category, 0) + trace.count
    return counts


def collect_path_pattern_counts(traces: list[Trace]) -> dict[tuple[str, str], int]:
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


def format_inventory(traces: list[Trace]) -> list[str]:
    if not traces:
        return ["Map/vector compiler-knowledge inventory: 0 traces observed."]

    lines = ["Map/vector compiler-knowledge inventory:"]
    total = sum(trace.count for trace in traces)
    lines.append(f"  total traces: {total}")
    category_counts = collect_category_counts(traces)
    for category, count in sorted(category_counts.items()):
        lines.append(f"  category {category}: {count}")

    path_pattern_counts = collect_path_pattern_counts(traces)
    examples = collect_examples(traces)
    for (rel_path, pattern_id), count in sorted(path_pattern_counts.items()):
        pattern = PATTERN_BY_ID[pattern_id]
        lines.append(
            f"  - {rel_path}: category {pattern.category}, pattern {pattern_id} "
            f"has {count} traces [{pattern.description}]"
        )
        example = examples[(rel_path, pattern_id)]
        lines.append(f"    first: {example.rel_path}:{example.line_number}: {example.text}")
    return lines


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    traces = collect_traces(root)
    lines = format_inventory(traces)
    category_counts = collect_category_counts(traces)
    forbidden_categories = {
        category for category in args.require_zero_category
        if category_counts.get(category, 0) != 0
    }

    if args.enforce_zero and traces:
        for line in lines:
            print(line, file=sys.stderr)
        print(
            "Map/vector compiler-knowledge zero audit failed: "
            f"{sum(trace.count for trace in traces)} traces remain.",
            file=sys.stderr,
        )
        return 1
    if forbidden_categories:
        for line in lines:
            print(line, file=sys.stderr)
        categories = ", ".join(sorted(forbidden_categories))
        print(
            "Map/vector compiler-knowledge category zero audit failed: "
            f"{categories}",
            file=sys.stderr,
        )
        return 1

    for line in lines:
        print(line)
    if args.enforce_zero:
        print("Map/vector compiler-knowledge zero audit passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
