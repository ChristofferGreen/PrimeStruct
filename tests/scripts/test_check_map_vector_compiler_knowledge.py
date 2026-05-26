#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Self-test the map/vector compiler-knowledge inventory."
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="PrimeStruct repository root",
    )
    return parser.parse_args()


def run_checker(
    repo_root: Path,
    scanned_root: Path,
    *extra_args: str,
) -> subprocess.CompletedProcess[str]:
    checker = repo_root / "scripts" / "check_map_vector_compiler_knowledge.py"
    return subprocess.run(
        [sys.executable, str(checker), "--root", str(scanned_root), *extra_args],
        text=True,
        capture_output=True,
        check=False,
    )


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def assert_contains(haystack: str, needle: str, label: str) -> bool:
    if needle in haystack:
        return True
    print(f"Missing {label}: {needle!r}", file=sys.stderr)
    print("stdout:", haystack, sep="\n", file=sys.stderr)
    return False


def make_scanned_root(repo_root: Path, name: str) -> Path:
    scratch_root = repo_root / ".primec_test_scratch" / "map_vector_compiler_knowledge"
    scanned_root = scratch_root / f"{name}_{os.getpid()}"
    shutil.rmtree(scanned_root, ignore_errors=True)
    (scanned_root / "include").mkdir(parents=True)
    (scanned_root / "src").mkdir()
    return scanned_root


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    scratch_parent = repo_root / ".primec_test_scratch" / "map_vector_compiler_knowledge"
    created_roots: list[Path] = []

    try:
        clean_root = make_scanned_root(repo_root, "clean")
        created_roots.append(clean_root)
        write(
            clean_root / "include" / "AllowedStdContainers.h",
            "#include <map>\n#include <vector>\n"
            "std::map<int, std::vector<int>> values;\n"
            "// source-map and SourceMap terminology is unrelated debug metadata.\n",
        )
        write(
            clean_root / "tests" / "BadTestTrace.cpp",
            'const char *bridge = "collections.vector_helpers";\n',
        )
        write(
            clean_root / "docs" / "bad.md",
            "MapValue and collections.map_helpers are allowed in docs.\n",
        )
        write(
            clean_root / "stdlib" / "std" / "collections" / "map.prime",
            "fn map() -> void {}\n",
        )
        write(
            clean_root / "include" / "primec" / "testing" / "BadTestingTrace.h",
            "bool isMapValue();\n",
        )
        clean = run_checker(repo_root, clean_root)
        if clean.returncode != 0:
            print(clean.stdout, end="")
            print(clean.stderr, end="", file=sys.stderr)
            return 1
        if not assert_contains(clean.stdout, "0 traces observed", "clean inventory"):
            return 1
        clean_map_helper_zero = run_checker(
            repo_root,
            clean_root,
            "--require-zero-category",
            "map-helper-classifier",
        )
        if clean_map_helper_zero.returncode != 0:
            print("Clean inventory should satisfy category-zero mode", file=sys.stderr)
            print(clean_map_helper_zero.stdout, end="")
            print(clean_map_helper_zero.stderr, end="", file=sys.stderr)
            return 1

        bad_root = make_scanned_root(repo_root, "bad")
        created_roots.append(bad_root)
        write(
            bad_root / "src" / "CompilerKnowledge.cpp",
            '\n'.join(
                [
                    'auto vectorMetadata = findStdlibSurfaceMetadataByBridgeKey("collections.vector_helpers");',
                    'auto mapMetadata = findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");',
                    "if (tryEmitVectorStatementHelper(stmt)) { return true; }",
                    'if (isVectorBuiltinName(expr, "count")) { return true; }',
                    'auto vectorPath = collectionMemberPath("vector", helperName);',
                    'error = "vector literal requires heap_alloc effect";',
                    "auto vectorStruct = specializedExperimentalVectorStructPathForElementType(elemType);",
                    'auto vectorStorage = experimentalCollectionTypePath("vector", "Vector");',
                    'if (isMapLiteralAssignPair(arg)) { collectMapLiteralEntries(args); }',
                    'error = "map literal requires exactly two template arguments";',
                    'if (isKeyValueBuiltinName(expr, "count")) { return true; }',
                    'auto mapPath = collectionMemberPath("map", helperName);',
                    "if (isExperimentalMapStructTypePath(typeName) || isMapValue(expr, locals)) { return true; }",
                    'auto mapStruct = experimentalCollectionTypePath("map", "Map");',
                    "auto direct = StdlibSurfaceId::CollectionsMapHelpers;",
                    "auto vectorDirect = StdlibSurfaceId::CollectionsVectorHelperSurface;",
                    'auto backing = std::string("MapValue");',
                ]
            )
            + "\n",
        )
        bad = run_checker(repo_root, bad_root)
        if bad.returncode != 0:
            print("Default inventory mode should allow nonzero traces", file=sys.stderr)
            print(bad.stdout, end="")
            print(bad.stderr, end="", file=sys.stderr)
            return 1
        expected_categories = [
            "category stdlib-bridge-key:",
            "category stdlib-registry-id:",
            "category vector-statement-helper:",
            "category vector-helper-classifier:",
            "category vector-literal-path:",
            "category vector-backing-classifier:",
            "category map-literal-path:",
            "category map-helper-classifier:",
            "category map-backing-classifier:",
        ]
        for category in expected_categories:
            if not assert_contains(bad.stdout, category, category):
                return 1
        expected_patterns = [
            "pattern vector-bridge-key has 1 traces",
            "pattern map-bridge-key has 1 traces",
            "pattern vector-statement-helper-api has 1 traces",
            "pattern vector-helper-classifier-api has 1 traces",
            "pattern vector-helper-path-builder has 1 traces",
            "pattern vector-literal-path has 1 traces",
            "pattern vector-backing-classifier-api has 1 traces",
            "pattern vector-backing-path-builder has 1 traces",
            "pattern map-literal-path has 3 traces",
            "pattern map-helper-classifier-api has 1 traces",
            "pattern map-helper-path-builder has 1 traces",
            "pattern map-backing-classifier-api has 3 traces",
            "pattern map-backing-path-builder has 1 traces",
            "pattern map-registry-id has 1 traces",
            "pattern vector-registry-id has 1 traces",
        ]
        for pattern in expected_patterns:
            if not assert_contains(bad.stdout, pattern, pattern):
                return 1

        category_zero = run_checker(
            repo_root,
            bad_root,
            "--require-zero-category",
            "map-helper-classifier",
        )
        if category_zero.returncode == 0:
            print("Category-zero mode accepted map helper traces", file=sys.stderr)
            print(category_zero.stdout, end="")
            return 1
        if not assert_contains(
            category_zero.stderr,
            "Map/vector compiler-knowledge category zero audit failed: map-helper-classifier",
            "category-zero failure",
        ):
            return 1

        zero = run_checker(repo_root, bad_root, "--enforce-zero")
        if zero.returncode == 0:
            print("Zero mode accepted map/vector compiler knowledge", file=sys.stderr)
            print(zero.stdout, end="")
            return 1
        if not assert_contains(
            zero.stderr,
            "Map/vector compiler-knowledge zero audit failed",
            "zero-mode failure",
        ):
            return 1
    finally:
        for root in created_roots:
            shutil.rmtree(root, ignore_errors=True)
        try:
            scratch_parent.rmdir()
        except OSError:
            pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
