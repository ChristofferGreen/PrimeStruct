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
        "experimental-map-path",
        "experimental PrimeStruct map backing path",
        re.compile(r'/?std/collections/experimental_map(?:/|")'),
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
        "map-type-text",
        "PrimeStruct Map<K, V> surface type spelling",
        re.compile(r"\bMap<"),
    ),
]


# TODO-4472 installs a decaying inventory gate before TODO-4464 broadens this
# coverage into the final zero C++ map-surface audit. Entries are maximum
# allowed counts, so deleting residue does not require touching the audit;
# adding new backing residue does.
ALLOWED_MAX_COUNTS: dict[tuple[str, str], int] = {
    ("src/IrPrinterHelpers.cpp", "experimental-map-path"): 4,
    ("src/IrPrinterHelpers.cpp", "map-backing-type-symbol"): 2,
    ("src/emitter/EmitterBuiltinCallPathHelpers.cpp", "experimental-map-path"): 1,
    ("src/emitter/EmitterHelpersTypes.cpp", "experimental-map-path"): 4,
    ("src/emitter/EmitterHelpersTypes.cpp", "map-backing-type-symbol"): 2,
    ("src/ir_lowerer/IrLowererAccessTargetResolution.cpp", "experimental-map-path"): 7,
    ("src/ir_lowerer/IrLowererAccessTargetResolution.cpp", "map-backing-type-symbol"): 2,
    ("src/ir_lowerer/IrLowererBindingTypeHelpers.cpp", "experimental-map-path"): 4,
    ("src/ir_lowerer/IrLowererBindingTypeHelpers.cpp", "map-backing-type-symbol"): 2,
    ("src/ir_lowerer/IrLowererCallHelpers.cpp", "experimental-map-path"): 4,
    ("src/ir_lowerer/IrLowererCallHelpers.cpp", "map-backing-type-symbol"): 2,
    ("src/ir_lowerer/IrLowererInlineCallContextHelpers.cpp", "experimental-map-path"): 2,
    ("src/ir_lowerer/IrLowererInlineCallContextHelpers.cpp", "map-backing-type-symbol"): 1,
    ("src/ir_lowerer/IrLowererInlineNativeCallDispatch.cpp", "experimental-map-path"): 5,
    ("src/ir_lowerer/IrLowererInlineNativeCallDispatch.cpp", "map-backing-type-symbol"): 3,
    ("src/ir_lowerer/IrLowererInlinePackedArgs.cpp", "experimental-map-path"): 2,
    ("src/ir_lowerer/IrLowererInlinePackedArgs.cpp", "map-backing-type-symbol"): 1,
    ("src/ir_lowerer/IrLowererInlineParamHelpers.cpp", "experimental-map-path"): 2,
    ("src/ir_lowerer/IrLowererInlineParamHelpers.cpp", "map-backing-type-symbol"): 1,
    (
        "src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h",
        "entry-backing-type-symbol",
    ): 1,
    (
        "src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h",
        "experimental-map-path",
    ): 4,
    (
        "src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h",
        "map-backing-type-symbol",
    ): 1,
    ("src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h", "experimental-map-path"): 15,
    ("src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h", "map-backing-type-symbol"): 7,
    ("src/ir_lowerer/IrLowererLowerEmitExprTryHelpers.h", "experimental-map-path"): 1,
    ("src/ir_lowerer/IrLowererLowerEmitExprTryHelpers.h", "map-backing-type-symbol"): 1,
    (
        "src/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.cpp",
        "experimental-map-path",
    ): 2,
    (
        "src/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.cpp",
        "map-backing-type-symbol",
    ): 1,
    (
        "src/ir_lowerer/IrLowererLowerInferenceDispatchSetup.cpp",
        "experimental-map-path",
    ): 2,
    (
        "src/ir_lowerer/IrLowererLowerInferenceDispatchSetup.cpp",
        "map-backing-type-symbol",
    ): 1,
    (
        "src/ir_lowerer/IrLowererLowerInferenceFallbackSetup.cpp",
        "experimental-map-path",
    ): 2,
    (
        "src/ir_lowerer/IrLowererLowerInferenceFallbackSetup.cpp",
        "map-backing-type-symbol",
    ): 1,
    ("src/ir_lowerer/IrLowererLowerInlineCalls.h", "experimental-map-path"): 4,
    ("src/ir_lowerer/IrLowererLowerInlineCalls.h", "map-backing-type-symbol"): 1,
    ("src/ir_lowerer/IrLowererLowerStatementsExpr.h", "experimental-map-path"): 2,
    ("src/ir_lowerer/IrLowererLowerStatementsExpr.h", "map-backing-type-symbol"): 1,
    (
        "src/ir_lowerer/IrLowererOperatorCollectionMutationHelpers.cpp",
        "experimental-map-path",
    ): 2,
    (
        "src/ir_lowerer/IrLowererOperatorCollectionMutationHelpers.cpp",
        "map-backing-type-symbol",
    ): 1,
    ("src/ir_lowerer/IrLowererPackedResultHelpers.cpp", "experimental-map-path"): 1,
    ("src/ir_lowerer/IrLowererResultMetadataHelpers.cpp", "experimental-map-path"): 2,
    ("src/ir_lowerer/IrLowererResultMetadataHelpers.cpp", "map-backing-type-symbol"): 1,
    ("src/ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp", "experimental-map-path"): 1,
    ("src/ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp", "map-backing-type-symbol"): 1,
    (
        "src/ir_lowerer/IrLowererSetupTypeDeclaredCollectionInference.cpp",
        "experimental-map-path",
    ): 4,
    (
        "src/ir_lowerer/IrLowererSetupTypeDeclaredCollectionInference.cpp",
        "map-backing-type-symbol",
    ): 2,
    (
        "src/ir_lowerer/IrLowererSetupTypeMethodCallResolution.cpp",
        "experimental-map-path",
    ): 1,
    (
        "src/ir_lowerer/IrLowererSetupTypeMethodTargetHelpers.cpp",
        "experimental-map-path",
    ): 2,
    ("src/ir_lowerer/IrLowererSetupTypeMethodTargetHelpers.cpp", "map-type-text"): 2,
    ("src/ir_lowerer/IrLowererStatementBindingHelpers.cpp", "experimental-map-path"): 3,
    ("src/ir_lowerer/IrLowererStatementBindingHelpers.cpp", "map-backing-type-symbol"): 3,
    (
        "src/ir_lowerer/IrLowererStatementBindingTypeMetadata.cpp",
        "experimental-map-path",
    ): 2,
    (
        "src/ir_lowerer/IrLowererStatementBindingTypeMetadata.cpp",
        "map-backing-type-symbol",
    ): 2,
    ("src/ir_lowerer/IrLowererStatementCallEmission.cpp", "experimental-map-path"): 7,
    ("src/ir_lowerer/IrLowererStatementCallEmission.cpp", "map-backing-type-symbol"): 1,
    ("src/ir_lowerer/IrLowererStructFieldBindingHelpers.cpp", "experimental-map-path"): 2,
    ("src/ir_lowerer/IrLowererStructFieldBindingHelpers.cpp", "map-backing-type-symbol"): 2,
    ("src/ir_lowerer/IrLowererStructLayoutHelpers.cpp", "experimental-map-path"): 1,
    ("src/ir_lowerer/IrLowererStructLayoutHelpers.cpp", "map-backing-type-symbol"): 1,
    ("src/ir_lowerer/IrLowererStructSlotLayoutHelpers.cpp", "experimental-map-path"): 8,
    ("src/ir_lowerer/IrLowererStructSlotLayoutHelpers.cpp", "map-backing-type-symbol"): 6,
    ("src/ir_lowerer/IrLowererStructTypeHelpers.cpp", "experimental-map-path"): 1,
    ("src/ir_lowerer/IrLowererStructTypeHelpers.cpp", "map-backing-type-symbol"): 1,
    ("src/ir_lowerer/IrLowererUninitializedStructInference.cpp", "experimental-map-path"): 2,
    ("src/ir_lowerer/IrLowererUninitializedStructInference.cpp", "map-backing-type-symbol"): 2,
    ("src/semantics/MapConstructorHelpers.h", "experimental-map-path"): 5,
    ("src/semantics/SemanticPublicationBuilders.cpp", "experimental-map-path"): 2,
    ("src/semantics/SemanticsBindingTypeHelpers.cpp", "experimental-map-path"): 6,
    ("src/semantics/SemanticsBindingTypeHelpers.cpp", "map-backing-type-symbol"): 2,
    ("src/semantics/SemanticsBuiltinPathHelpers.cpp", "experimental-map-path"): 1,
    ("src/semantics/SemanticsValidate.cpp", "experimental-map-path"): 2,
    ("src/semantics/SemanticsValidate.cpp", "map-type-text"): 1,
    ("src/semantics/SemanticsValidatorBuildDirectCallBinding.cpp", "experimental-map-path"): 1,
    (
        "src/semantics/SemanticsValidatorBuildDirectCallBinding.cpp",
        "map-backing-type-symbol",
    ): 1,
    ("src/semantics/SemanticsValidatorBuildInitializerInference.cpp", "experimental-map-path"): 2,
    (
        "src/semantics/SemanticsValidatorBuildInitializerInference.cpp",
        "map-backing-type-symbol",
    ): 2,
    (
        "src/semantics/SemanticsValidatorBuildInitializerInferenceCalls.cpp",
        "experimental-map-path",
    ): 2,
    (
        "src/semantics/SemanticsValidatorBuildInitializerInferenceCalls.cpp",
        "map-backing-type-symbol",
    ): 2,
    ("src/semantics/SemanticsValidatorBuildParameters.cpp", "experimental-map-path"): 2,
    ("src/semantics/SemanticsValidatorBuildParameters.cpp", "map-backing-type-symbol"): 1,
    (
        "src/semantics/SemanticsValidatorCollectionHelperRewrites.cpp",
        "experimental-map-path",
    ): 2,
    ("src/semantics/SemanticsValidatorExprArgumentValidation.cpp", "experimental-map-path"): 3,
    (
        "src/semantics/SemanticsValidatorExprArgumentValidationCollections.cpp",
        "experimental-map-path",
    ): 1,
    (
        "src/semantics/SemanticsValidatorExprArgumentValidationCollections.cpp",
        "map-backing-type-symbol",
    ): 1,
    ("src/semantics/SemanticsValidatorExprCallResolution.cpp", "experimental-map-path"): 2,
    (
        "src/semantics/SemanticsValidatorExprCollectionAccessValidation.cpp",
        "experimental-map-path",
    ): 2,
    (
        "src/semantics/SemanticsValidatorExprCountCapacityMapBuiltins.cpp",
        "experimental-map-path",
    ): 2,
    (
        "src/semantics/SemanticsValidatorExprLateMapAccessBuiltins.cpp",
        "experimental-map-path",
    ): 3,
    (
        "src/semantics/SemanticsValidatorExprLateMapAccessBuiltins.cpp",
        "map-backing-type-symbol",
    ): 1,
    (
        "src/semantics/SemanticsValidatorExprMethodTargetResolution.cpp",
        "experimental-map-path",
    ): 3,
    (
        "src/semantics/SemanticsValidatorExprMethodTargetResolution.cpp",
        "map-backing-type-symbol",
    ): 2,
    (
        "src/semantics/SemanticsValidatorExprPreDispatchDirectCalls.cpp",
        "experimental-map-path",
    ): 2,
    ("src/semantics/SemanticsValidatorExprReceiverPaths.cpp", "experimental-map-path"): 2,
    ("src/semantics/SemanticsValidatorExprReceiverPaths.cpp", "map-backing-type-symbol"): 2,
    (
        "src/semantics/SemanticsValidatorInferCollectionCompatibility.cpp",
        "experimental-map-path",
    ): 8,
    (
        "src/semantics/SemanticsValidatorInferCollectionCompatibility.cpp",
        "map-backing-type-symbol",
    ): 2,
    (
        "src/semantics/SemanticsValidatorInferCollectionCompatibilityInternal.h",
        "experimental-map-path",
    ): 1,
    (
        "src/semantics/SemanticsValidatorInferCollectionReturnInference.cpp",
        "experimental-map-path",
    ): 3,
    (
        "src/semantics/SemanticsValidatorInferCollectionReturnInference.cpp",
        "map-backing-type-symbol",
    ): 2,
    ("src/semantics/SemanticsValidatorInferCollections.cpp", "experimental-map-path"): 2,
    ("src/semantics/SemanticsValidatorInferCollections.cpp", "map-backing-type-symbol"): 1,
    ("src/semantics/SemanticsValidatorInferStructReturn.cpp", "experimental-map-path"): 1,
    ("src/semantics/SemanticsValidatorInferStructReturn.cpp", "map-backing-type-symbol"): 1,
    (
        "src/semantics/SemanticsValidatorInferStructReturnHelpers.cpp",
        "experimental-map-path",
    ): 1,
    (
        "src/semantics/SemanticsValidatorInferStructReturnHelpers.cpp",
        "map-backing-type-symbol",
    ): 1,
    ("src/semantics/SemanticsValidatorInferTargetResolution.cpp", "experimental-map-path"): 1,
    ("src/semantics/SemanticsValidatorPassesStructLayouts.cpp", "experimental-map-path"): 2,
    ("src/semantics/SemanticsValidatorResultHelpers.cpp", "experimental-map-path"): 1,
    ("src/semantics/SemanticsValidatorResultHelpers.cpp", "map-backing-type-symbol"): 1,
    ("src/semantics/SemanticsValidatorStatement.cpp", "experimental-map-path"): 2,
    ("src/semantics/SemanticsValidatorStatement.cpp", "map-backing-type-symbol"): 2,
    ("src/semantics/SemanticsValidatorStatementContainerHelpers.cpp", "experimental-map-path"): 2,
    ("src/semantics/SemanticsValidatorStatementPrintability.cpp", "experimental-map-path"): 2,
    ("src/semantics/SemanticsValidatorStatementReturns.cpp", "experimental-map-path"): 1,
    ("src/semantics/SemanticsValidatorStatementReturns.cpp", "map-backing-type-symbol"): 1,
    ("src/semantics/TemplateMonomorphBindingCallInference.h", "experimental-map-path"): 6,
    (
        "src/semantics/TemplateMonomorphCollectionCompatibilityPaths.h",
        "experimental-map-path",
    ): 2,
    (
        "src/semantics/TemplateMonomorphCollectionCompatibilityPaths.h",
        "map-backing-type-symbol",
    ): 2,
    ("src/semantics/TemplateMonomorphCoreUtilities.h", "experimental-map-path"): 2,
    (
        "src/semantics/TemplateMonomorphExperimentalCollectionConstructorRewrites.h",
        "entry-backing-type-symbol",
    ): 1,
    (
        "src/semantics/TemplateMonomorphExperimentalCollectionConstructorRewrites.h",
        "experimental-map-path",
    ): 4,
    (
        "src/semantics/TemplateMonomorphExperimentalCollectionReceiverResolution.h",
        "experimental-map-path",
    ): 4,
    (
        "src/semantics/TemplateMonomorphExperimentalCollectionReceiverResolution.h",
        "map-backing-type-symbol",
    ): 1,
    ("src/semantics/TemplateMonomorphExperimentalCollectionTypeHelpers.h", "experimental-map-path"): 2,
    ("src/semantics/TemplateMonomorphExpressionRewrite.h", "experimental-map-path"): 4,
    ("src/semantics/TemplateMonomorphFallbackTypeInference.h", "experimental-map-path"): 9,
    ("src/semantics/TemplateMonomorphFallbackTypeInference.h", "map-backing-type-symbol"): 2,
    ("src/semantics/TemplateMonomorphFallbackTypeInference.h", "map-type-text"): 5,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Check production C++ for PrimeStruct map backing traces. "
            "Current classified experimental_map/Map__ residue is allowed "
            "only up to the checked-in inventory; any new file or count "
            "increase fails. Tests, docs, stdlib .prime files, ordinary C++ "
            "std::map usage, generic mapping names, and source-map "
            "infrastructure are not scanned or matched."
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


def collect_violations(
    observed: dict[tuple[str, str], int],
) -> list[tuple[str, str, int, int]]:
    violations: list[tuple[str, str, int, int]] = []
    for (path, pattern_id), count in sorted(observed.items()):
        allowed = ALLOWED_MAX_COUNTS.get((path, pattern_id), 0)
        if count > allowed:
            violations.append((path, pattern_id, count, allowed))
    return violations


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    observed = collect_counts(root)
    violations = collect_violations(observed)

    if violations:
        print("Map backing trace check failed:", file=sys.stderr)
        for path, pattern_id, count, allowed in violations:
            print(
                f"  - {path}: pattern {pattern_id} has {count} traces "
                f"(allowed {allowed})",
                file=sys.stderr,
            )
        return 1

    observed_total = sum(observed.values())
    allowed_total = sum(ALLOWED_MAX_COUNTS.values())
    print(
        "Map backing trace check passed. "
        f"{observed_total} production traces observed "
        f"within {allowed_total} allowed inventory slots."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
