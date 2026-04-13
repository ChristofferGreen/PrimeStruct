#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from pathlib import Path


SUITE_BEGIN_RE = re.compile(r'^\s*TEST_SUITE_BEGIN\("([^"]+)"\);', re.MULTILINE)
TEST_CASE_RE = re.compile(r'^\s*TEST_CASE\(', re.MULTILINE)
NUMERIC_SUFFIX_RE = re.compile(r'.*_[0-9]+$')
TEST_SOURCE_SUFFIXES = {'.cpp', '.h'}

EXPECTED_FILE_SUITES = {
    'tests/unit/test_compile_run_vm_bounds.cpp': 'primestruct.compile.run.vm.bounds',
    'tests/unit/test_compile_run_vm_maps.cpp': 'primestruct.compile.run.vm.maps',
    'tests/unit/test_compile_run_vm_math.cpp': 'primestruct.compile.run.vm.math',
    'tests/unit/test_compile_run_vm_outputs.cpp': 'primestruct.compile.run.vm.outputs',
    'tests/unit/test_ir_pipeline_backends.cpp': 'primestruct.ir.pipeline.backends.core',
    'tests/unit/test_ir_pipeline_backends_graph_pilot.cpp': 'primestruct.ir.pipeline.backends.core',
    'tests/unit/test_ir_pipeline_backends_glsl.cpp': 'primestruct.ir.pipeline.backends.glsl',
    'tests/unit/test_ir_pipeline_backends_cpp_vm.cpp': 'primestruct.ir.pipeline.backends.cpp_vm',
    'tests/unit/test_ir_pipeline_backends_registry.cpp': 'primestruct.ir.pipeline.backends.registry',
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            'Check PrimeStruct test file naming guardrails: numeric suffix bans, '
            'duplicate suite declarations, and file->suite alignment checks.'
        )
    )
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1], help='repository root')
    return parser.parse_args()


def normalize_path(path: Path) -> str:
    return path.as_posix()


def iter_test_sources(root: Path) -> list[Path]:
    unit_dir = root / 'tests' / 'unit'
    if not unit_dir.exists():
        return []

    sources: list[Path] = []
    for path in sorted(unit_dir.iterdir()):
        if not path.is_file():
            continue
        if not path.name.startswith('test_'):
            continue
        if path.suffix not in TEST_SOURCE_SUFFIXES:
            continue
        sources.append(path)
    return sources


def extract_suite_begins(path: Path) -> list[str]:
    return SUITE_BEGIN_RE.findall(path.read_text(encoding='utf-8'))


def has_test_case(path: Path) -> bool:
    return bool(TEST_CASE_RE.search(path.read_text(encoding='utf-8')))


def check_numeric_suffixes(test_sources: list[Path], root: Path, violations: list[str]) -> None:
    for path in test_sources:
        if NUMERIC_SUFFIX_RE.fullmatch(path.stem):
            rel_path = normalize_path(path.relative_to(root))
            violations.append(
                f'{rel_path}: numeric-only suffix test filenames are not allowed; use semantic topic names'
            )


def check_duplicate_suite_begins(test_sources: list[Path], root: Path, violations: list[str]) -> None:
    for path in test_sources:
        suites = extract_suite_begins(path)
        if not suites:
            continue

        suite_counts = Counter(suites)
        duplicate_suites = sorted(suite for suite, count in suite_counts.items() if count > 1)
        for suite in duplicate_suites:
            rel_path = normalize_path(path.relative_to(root))
            violations.append(
                f'{rel_path}: duplicate TEST_SUITE_BEGIN("{suite}") declarations found; keep one declaration per suite per file'
            )


def check_expected_file_suite_alignment(root: Path, violations: list[str]) -> None:
    for rel_path, expected_suite in sorted(EXPECTED_FILE_SUITES.items()):
        path = root / rel_path
        if not path.exists():
            violations.append(f'{rel_path}: expected file is missing for suite-alignment guardrail')
            continue

        suites = extract_suite_begins(path)
        if expected_suite not in suites:
            suites_text = ', '.join(f'"{suite}"' for suite in suites) if suites else '<none>'
            violations.append(
                f'{rel_path}: expected TEST_SUITE_BEGIN("{expected_suite}") but found {suites_text}'
            )


def check_vm_collections_splitout(root: Path, violations: list[str]) -> None:
    expected_suite = 'primestruct.compile.run.vm.collections'
    unit_dir = root / 'tests' / 'unit'
    wrapper = unit_dir / 'test_compile_run_vm_collections.cpp'
    if not wrapper.exists():
        violations.append('tests/unit/test_compile_run_vm_collections.cpp: expected helper wrapper file is missing')
        return

    wrapper_rel = normalize_path(wrapper.relative_to(root))
    wrapper_suites = extract_suite_begins(wrapper)
    if wrapper_suites:
        suites_text = ', '.join(f'"{suite}"' for suite in wrapper_suites)
        violations.append(
            f'{wrapper_rel}: expected helper-only wrapper with no TEST_SUITE_BEGIN declarations; found {suites_text}'
        )
    if has_test_case(wrapper):
        violations.append(
            f'{wrapper_rel}: expected helper-only wrapper with no TEST_CASE definitions after split-out'
        )

    shard_paths = sorted(unit_dir.glob('test_compile_run_vm_collections_*.cpp'))
    if not shard_paths:
        violations.append(
            'tests/unit/test_compile_run_vm_collections_*.cpp: expected at least one split-out collections shard'
        )
        return

    for shard in shard_paths:
        shard_rel = normalize_path(shard.relative_to(root))
        suites = extract_suite_begins(shard)
        if expected_suite not in suites:
            suites_text = ', '.join(f'"{suite}"' for suite in suites) if suites else '<none>'
            violations.append(
                f'{shard_rel}: expected TEST_SUITE_BEGIN("{expected_suite}") in split-out collections shard but found {suites_text}'
            )
        if not has_test_case(shard):
            violations.append(
                f'{shard_rel}: split-out collections shard should contain TEST_CASE definitions'
            )

    for vm_source in sorted(unit_dir.glob('test_compile_run_vm_*.cpp')):
        if vm_source == wrapper:
            continue
        if vm_source.name.startswith('test_compile_run_vm_collections_'):
            continue
        suites = extract_suite_begins(vm_source)
        if expected_suite in suites:
            rel_path = normalize_path(vm_source.relative_to(root))
            violations.append(
                f'{rel_path}: non-collections vm source must not declare TEST_SUITE_BEGIN("{expected_suite}")'
            )


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    test_sources = iter_test_sources(root)

    violations: list[str] = []
    check_numeric_suffixes(test_sources, root, violations)
    check_duplicate_suite_begins(test_sources, root, violations)
    check_expected_file_suite_alignment(root, violations)
    check_vm_collections_splitout(root, violations)

    if violations:
        print('Test naming guardrail check failed:', file=sys.stderr)
        for violation in violations:
            print(f'  - {violation}', file=sys.stderr)
        return 1

    print(
        'Test naming guardrail check passed. '
        f'Validated {len(test_sources)} test sources and {len(EXPECTED_FILE_SUITES)} explicit file/suite mappings.'
    )
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
