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


# TODO-4473 installs a decaying full-inventory gate before TODO-4464 tightens
# map-surface production C++ to zero tolerance. Entries are maximum allowed
# counts, so deleting residue does not require touching the audit; adding new
# map-specific production C++ residue does.
ALLOWED_MAX_FILE_COUNTS: dict[str, int] = {
    "include/primec/StdlibSurfaceRegistry.h": 2,
    "src/IrPrinterHelpers.cpp": 6,
    "src/StdlibSurfaceRegistry.cpp": 12,
    "src/emitter/EmitterBuiltinCallPathHelpers.cpp": 27,
    "src/emitter/EmitterBuiltinMethodResolutionHelpers.cpp": 8,
    "src/emitter/EmitterBuiltinMethodResolutionMetadataHelpers.cpp": 26,
    "src/emitter/EmitterBuiltinMethodResolutionTypeInferenceHelpers.cpp": 10,
    "src/emitter/EmitterEmitSetupReturnInferenceCollections.h": 31,
    "src/emitter/EmitterExprCollectionTypeHelpers.h": 10,
    "src/emitter/EmitterHelpersTypes.cpp": 10,
    "src/ir_lowerer/IrLowererAccessLoadHelpers.cpp": 2,
    "src/ir_lowerer/IrLowererAccessTargetResolution.cpp": 37,
    "src/ir_lowerer/IrLowererBindingTypeHelpers.cpp": 8,
    "src/ir_lowerer/IrLowererBuiltinNameHelpers.cpp": 9,
    "src/ir_lowerer/IrLowererCallHelpers.cpp": 28,
    "src/ir_lowerer/IrLowererCallResolution.cpp": 33,
    "src/ir_lowerer/IrLowererCountAccessHelpers.cpp": 4,
    "src/ir_lowerer/IrLowererFlowControlHelpers.cpp": 2,
    "src/ir_lowerer/IrLowererHelpers.cpp": 5,
    "src/ir_lowerer/IrLowererInlineCallContextHelpers.cpp": 10,
    "src/ir_lowerer/IrLowererInlineNativeCallDispatch.cpp": 39,
    "src/ir_lowerer/IrLowererInlinePackedArgs.cpp": 6,
    "src/ir_lowerer/IrLowererInlineParamHelpers.cpp": 6,
    "src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h": 17,
    "src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h": 49,
    "src/ir_lowerer/IrLowererLowerEmitExprTryHelpers.h": 2,
    "src/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.cpp": 8,
    "src/ir_lowerer/IrLowererLowerInferenceDispatchSetup.cpp": 6,
    "src/ir_lowerer/IrLowererLowerInferenceFallbackSetup.cpp": 3,
    "src/ir_lowerer/IrLowererLowerInlineCalls.h": 15,
    "src/ir_lowerer/IrLowererLowerStatementsBindings.h": 4,
    "src/ir_lowerer/IrLowererLowerStatementsExpr.h": 18,
    "src/ir_lowerer/IrLowererNativeTailDispatch.cpp": 14,
    "src/ir_lowerer/IrLowererOperatorCollectionMutationHelpers.cpp": 3,
    "src/ir_lowerer/IrLowererPackedResultHelpers.cpp": 3,
    "src/ir_lowerer/IrLowererResultMetadataHelpers.cpp": 8,
    "src/ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp": 20,
    "src/ir_lowerer/IrLowererSetupTypeDeclaredCollectionInference.cpp": 11,
    "src/ir_lowerer/IrLowererSetupTypeMethodCallResolution.cpp": 26,
    "src/ir_lowerer/IrLowererSetupTypeMethodTargetHelpers.cpp": 13,
    "src/ir_lowerer/IrLowererStatementBindingHelpers.cpp": 6,
    "src/ir_lowerer/IrLowererStatementBindingTypeMetadata.cpp": 4,
    "src/ir_lowerer/IrLowererStatementCallEmission.cpp": 21,
    "src/ir_lowerer/IrLowererStructFieldBindingHelpers.cpp": 4,
    "src/ir_lowerer/IrLowererStructLayoutHelpers.cpp": 2,
    "src/ir_lowerer/IrLowererStructReturnPathHelpers.cpp": 9,
    "src/ir_lowerer/IrLowererStructSlotLayoutHelpers.cpp": 16,
    "src/ir_lowerer/IrLowererStructTypeHelpers.cpp": 2,
    "src/ir_lowerer/IrLowererUninitializedStructInference.cpp": 9,
    "src/semantics/MapConstructorHelpers.h": 27,
    "src/semantics/SemanticPublicationBuilders.cpp": 6,
    "src/semantics/SemanticsBindingTypeHelpers.cpp": 14,
    "src/semantics/SemanticsBuiltinPathHelpers.cpp": 14,
    "src/semantics/SemanticsCallPathHelpers.cpp": 4,
    "src/semantics/SemanticsValidate.cpp": 34,
    "src/semantics/SemanticsValidator.cpp": 6,
    "src/semantics/SemanticsValidatorBuildCallResolution.cpp": 3,
    "src/semantics/SemanticsValidatorBuildInitializerInference.cpp": 9,
    "src/semantics/SemanticsValidatorBuildInitializerInferenceCalls.cpp": 8,
    "src/semantics/SemanticsValidatorBuildParameters.cpp": 14,
    "src/semantics/SemanticsValidatorCollectionHelperRewrites.cpp": 10,
    "src/semantics/SemanticsValidatorEffectFreeCollections.cpp": 6,
    "src/semantics/SemanticsValidatorExprArgumentValidation.cpp": 9,
    "src/semantics/SemanticsValidatorExprArgumentValidationCollections.cpp": 15,
    "src/semantics/SemanticsValidatorExprCallResolution.cpp": 13,
    "src/semantics/SemanticsValidatorExprCollectionAccess.cpp": 87,
    "src/semantics/SemanticsValidatorExprCollectionAccessValidation.cpp": 33,
    "src/semantics/SemanticsValidatorExprCollectionCountCapacity.cpp": 15,
    "src/semantics/SemanticsValidatorExprCollectionDispatchSetup.cpp": 8,
    "src/semantics/SemanticsValidatorExprCountCapacityMapBuiltins.cpp": 52,
    "src/semantics/SemanticsValidatorExprLateMapAccessBuiltins.cpp": 41,
    "src/semantics/SemanticsValidatorExprMapSoaBuiltins.cpp": 8,
    "src/semantics/SemanticsValidatorExprMethodResolution.cpp": 15,
    "src/semantics/SemanticsValidatorExprMethodTargetResolution.cpp": 68,
    "src/semantics/SemanticsValidatorExprPointerLike.cpp": 8,
    "src/semantics/SemanticsValidatorExprPreDispatchDirectCalls.cpp": 28,
    "src/semantics/SemanticsValidatorExprReceiverPaths.cpp": 4,
    "src/semantics/SemanticsValidatorExprScalarPointerMemory.cpp": 6,
    "src/semantics/SemanticsValidatorExprTry.cpp": 11,
    "src/semantics/SemanticsValidatorExprVectorHelpers.cpp": 4,
    "src/semantics/SemanticsValidatorInferCollectionBufferAndMapResolvers.cpp": 10,
    "src/semantics/SemanticsValidatorInferCollectionCompatibility.cpp": 35,
    "src/semantics/SemanticsValidatorInferCollectionCompatibilityInternal.h": 13,
    "src/semantics/SemanticsValidatorInferCollectionCountCapacity.cpp": 6,
    "src/semantics/SemanticsValidatorInferCollectionDirectCountCapacity.cpp": 4,
    "src/semantics/SemanticsValidatorInferCollectionDispatch.cpp": 20,
    "src/semantics/SemanticsValidatorInferCollectionDispatchSetup.cpp": 20,
    "src/semantics/SemanticsValidatorInferCollectionReturnInference.cpp": 6,
    "src/semantics/SemanticsValidatorInferCollections.cpp": 3,
    "src/semantics/SemanticsValidatorInferDefinition.cpp": 11,
    "src/semantics/SemanticsValidatorInferLateFallbackBuiltins.cpp": 20,
    "src/semantics/SemanticsValidatorInferMethodResolution.cpp": 13,
    "src/semantics/SemanticsValidatorInferMethodResolutionHelpers.cpp": 3,
    "src/semantics/SemanticsValidatorInferPreDispatchCalls.cpp": 29,
    "src/semantics/SemanticsValidatorInferStructReturn.cpp": 9,
    "src/semantics/SemanticsValidatorInferStructReturnHelpers.cpp": 6,
    "src/semantics/SemanticsValidatorPassesDiagnostics.cpp": 4,
    "src/semantics/SemanticsValidatorResultHelpers.cpp": 5,
    "src/semantics/SemanticsValidatorSnapshots.cpp": 2,
    "src/semantics/SemanticsValidatorStatement.cpp": 4,
    "src/semantics/SemanticsValidatorStatementPrintability.cpp": 6,
    "src/semantics/SemanticsValidatorStatementReturns.cpp": 7,
    "src/semantics/TemplateMonomorphBindingCallInference.h": 6,
    "src/semantics/TemplateMonomorphCollectionCompatibilityPaths.h": 9,
    "src/semantics/TemplateMonomorphCoreUtilities.h": 10,
    "src/semantics/TemplateMonomorphExperimentalCollectionConstructorRewrites.h": 6,
    "src/semantics/TemplateMonomorphExperimentalCollectionReceiverResolution.h": 27,
    "src/semantics/TemplateMonomorphExperimentalCollectionTypeHelpers.h": 1,
    "src/semantics/TemplateMonomorphExpressionRewrite.h": 32,
    "src/semantics/TemplateMonomorphFallbackTypeInference.h": 16,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Check production C++ for PrimeStruct map-surface traces. "
            "Current map-specific residue is allowed only up to the "
            "checked-in inventory; any new file or count increase fails. "
            "Tests, docs, stdlib .prime files, ordinary C++ std::map usage, "
            "generic mapping names, and source-map infrastructure are not "
            "scanned or matched."
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
        for line in text.splitlines():
            for pattern in TRACE_PATTERNS:
                matches = pattern.regex.findall(line)
                if matches:
                    key = (rel_path, pattern.id)
                    counts[key] = counts.get(key, 0) + len(matches)
    return counts


def collect_file_counts(observed: dict[tuple[str, str], int]) -> dict[str, int]:
    file_counts: dict[str, int] = {}
    for (path, _pattern_id), count in observed.items():
        file_counts[path] = file_counts.get(path, 0) + count
    return file_counts


def collect_violations(observed: dict[tuple[str, str], int]) -> list[tuple[str, int, int]]:
    violations: list[tuple[str, int, int]] = []
    for path, count in sorted(collect_file_counts(observed).items()):
        allowed = ALLOWED_MAX_FILE_COUNTS.get(path, 0)
        if count > allowed:
            violations.append((path, count, allowed))
    return violations


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    observed = collect_counts(root)
    violations = collect_violations(observed)

    if violations:
        print("Map surface trace inventory check failed:", file=sys.stderr)
        for path, count, allowed in violations:
            print(
                f"  - {path}: has {count} map-surface traces (allowed {allowed})",
                file=sys.stderr,
            )
        return 1

    observed_total = sum(observed.values())
    allowed_total = sum(ALLOWED_MAX_FILE_COUNTS.values())
    print(
        "Map surface trace inventory check passed. "
        f"{observed_total} production traces observed "
        f"within {allowed_total} allowed inventory slots."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
