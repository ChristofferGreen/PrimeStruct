# Code Reuse Plan

Status: Active
Created: 2026-11-12

This plan records an audit of duplicated code in `src/` (and, briefly,
`stdlib/`), the reasons the duplication exists, and the work that removes it.
The work items are TODO-5959 through TODO-5977 in `docs/todo.md`. Each task
block names the section of this plan it comes from. The findings below were
measured on master at `bff054ac0` (2026-11-12); re-check line numbers before
editing, since other work lands on master every day.

## 1. Summary

The duplication is real, but it is not general carelessness. A few
architectural decisions make every compiler layer rebuild the same pieces, and
in several places the copies already disagree.

- A clone scan over `src/` (851 files, 219,849 non-blank, non-comment lines)
  finds 13.1% of lines inside a repeated 8-line block, and 17.2% when
  identifiers and literals are ignored (10-line blocks).
- 808 top-level function names are defined in two or more files.
- Per directory (exact / identifier-normalized): semantics 14.6% / 17.9%,
  ir_lowerer 14.2% / 19.8%, emitter 18.8% / 21.6%, parser 24.8% / 23.7%,
  ast 37.7% / 45.6%, ir 12.3% / 15.7%, wasm_emitter 16.8% / 19.8%,
  glsl_emitter 3.8% / 27.5%, native_emitter 10.4% / 11.6%, runtime 6.3% / 10.3%,
  support 2.7% / 7.1%, ui 0.2% / 0.9%.
- `stdlib/` is 17.7% exact, almost all of it in
  `stdlib/std/collections/soa_storage/` (section 12).

Root causes, most important first:

1. Two dead code generators are still compiled (section 2).
2. Types and call paths travel as strings, so every layer re-parses them
   (section 3).
3. Builtin-name recognition is re-implemented per layer and the copies
   disagree (section 4).
4. The shared classifiers in `src/support` were only half adopted (section 5).
5. The 1,200-line file cap is met by cutting functions into numbered phases
   instead of extracting components (section 6).
6. Copy, then drift, inside semantics (sections 7 and 8).
7. Parallel code that should be table-driven (sections 9, 10 and 11).
8. Small utilities with several copies (section 13) and an opcode table
   without category data (section 14).

How the audit was done: a window-hash clone detector (exact lines, and lines
with identifiers, strings and numbers replaced by placeholders), a scan of
top-level function names defined in more than one file, and a reading of the
flagged code. Items marked "verified" were read side by side; items marked
"reported" come from the scan and a reading by a helper agent and should be
confirmed by whoever picks them up.

## 2. Dead AST emitters (TODO-5959)

Verified. `src/emitter` (the AST to C++ emitter, 116 files, about 14,200
lines) and `src/glsl_emitter` (the AST to GLSL emitter, 8 files, about 2,900
lines) are compiled into the codegen library (`PRIMESTRUCT_CODEGEN_SOURCES`
in `CMakeLists.txt`, about lines 607-664) but no production path reaches them.

- `resolveIrBackendEmitKind` (`src/support/EmitKind.cpp:45`) rewrites
  `cpp`→`cpp-ir`, `exe`→`exe-ir`, `optcpp`→`optcpp-ir`,
  `optexe`→`optexe-ir`, `glsl`→`glsl-ir`, `spirv`→`spirv-ir`.
- `src/ir/IrBackends.cpp` instantiates only `IrToCppEmitter`,
  `IrToOptCppEmitter`, `IrToGlslEmitter`, `WasmEmitter` and `NativeEmitter`.
- Nothing outside `src/emitter` and `src/glsl_emitter` includes their headers
  (`include/primec/backend/Emitter.h`, `include/primec/backend/GlslEmitter.h`).
- The September archive (`docs/todo_archive/2026-09.md`, around line 14209)
  already says the emitter helper "is not in the shipped compiler" and that its
  pinned tests go "if the legacy `src/emitter` AST emitter is ever deleted as a
  compatibility subsystem".

Test dependents:

- `include/primec/testing/EmitterHelpers.h` exposes emitter internals to tests.
  33 test files include it and 14 use `primec::emitter::` names, most often
  `BindingInfo` (127 uses), `ReturnKind` (90), `resolveMethodCallPath` (76),
  `isSimpleCallName` (33) and the `runEmitterExprControl*Step` functions.
- Most of them are in `tests/unit/compile_run/emitters/` (27 files). Those
  files mix cases that compile through `primec` (keep them) with cases that call
  emitter internals (delete them).
- The `tests/unit/ir_pipeline/validation/test_ir_pipeline_validation_emitter_*`
  files test single emitter Step functions.
- `tests/unit/ir_pipeline/backends/test_ir_pipeline_backends_architecture.h`
  asserts library structure and will need updating.
- `docs/source_lock_inventory.md` has an entry for
  `test_ir_pipeline_validation_emitter_expr_source_delegation_stays_stable.cpp`.

Duplication carried by the dead code: each of the two emitters has its own
copy of the builtin-name classifiers, the type-text parsers and the collection
spelling predicates (sections 3 to 5). `src/emitter` also has 41 `*Step.cpp`
files of about 30 lines each. The `IfBlock*Step` and `BuiltinBlock*Step`
families are identical after renaming (BindingAuto, BindingExplicit,
BindingFallback, BindingQualifiers, EarlyReturn and Statement differ by 0 lines;
FinalValue by 1; BindingPrelude by 6). Deleting the directory removes all of
this, so nothing in it should be refactored first.

## 3. Types as strings (TODO-5961, TODO-5976)

Verified. There is no structured type object. A type is a `std::string`
("type text") in every layer, and the semantic product publishes about 45
`*TypeText*` string fields (`include/primec/frontend/SemanticProduct.h`).
`include/primec/ast/Ast.h` stores types as `Transform{name, templateArgs,
templateArgDetails}`. `include/primec/support/CanonicalReceiverType.h` is the
only structured piece: a receiver-only struct with one producer (the lowerer's
`resolveReceiverType`).

Copies of the parsers:

| Function | Definitions | Call sites |
| --- | --- | --- |
| `splitTemplateTypeName` | `src/semantics/SemanticsBindingTypeHelpers.cpp:605` (wrapper over `splitTemplateTypeNameUncached` at :567, with a cache in `typeNameCaches()`), `src/ir_lowerer/IrLowererHelpers.cpp:550`, `src/emitter/EmitterHelpersTypes.cpp:219`, `src/ir/IrPrinterHelpers.cpp:21` | semantics 341, lowerer 163, emitter 28, IR printer 4 |
| `splitTopLevelTemplateArgs` | `src/semantics/SemanticsBindingTypeHelpers.cpp:587`, `src/semantics/SemanticPublicationBuildersSupport.cpp:249`, `src/emitter/EmitterHelpersTypes.cpp:249`, `src/ir/IrPrinterHelpers.cpp:51` | semantics 220, emitter 20, IR printer 4 |
| `trimTemplateTypeText` | `src/ir_lowerer/IrLowererTemplateTypeParseHelpers.cpp:13` | about 600 in the lowerer |
| `returnKindForTypeName` | `src/semantics/SemanticsBindingTypeHelpers.cpp:673`, `src/emitter/EmitterHelpersTypes.cpp:603`, `src/ir/IrPrinterHelpers.cpp:413` | |
| `valueKindFromTypeName` | `src/ir_lowerer/IrLowererSetupTypeHelpers.cpp:28` (returns `LocalInfo::ValueKind`; the lowerer's fourth variant of the above) | about 288 |
| `normalizeBindingTypeName` | semantics, plus an emitter copy | semantics 1104, emitter 87 |

Divergence (verified):

- The emitter and IR printer copies of `splitTemplateTypeName` are identical:
  they match `<` and `>` by depth and clear `base` on failure.
- The lowerer copy is the same algorithm but returns early on empty text and
  writes `base` only on success.
- The semantics copy is a different algorithm: the first `<`, then `rfind('>')`
  which must be the last character, with no depth matching. On failure it sets
  `base = text`. For `A<B>C<D>` it returns base `A` and argument `B>C<D`; the
  other copies reject the text.

The lowerer still re-derives types from text. It does consume the semantic
product (360 `SemanticProgram*` references in 113 of 179 lowerer files), but
the inference files (`IrLowererLowerInference*`, about 5,400 lines) and the
type setup files (`IrLowererSetupType*`, about 5,700 lines) re-split type text
163 times. `docs/spec/semantics-lowering-boundary.md` says production lowering
should consume the product without AST re-derivation; the remaining fallbacks
are marked at `src/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.cpp:446`
("structural-only fallbacks") and
`src/ir_lowerer/IrLowererLowerStatementsCallsStage.cpp:314` ("re-derive it
here"). Lowerer-specific data such as `LocalInfo` (slot and kind) is
legitimate; re-parsing type text is the duplicate part.

## 4. Builtin and call-name classification (TODO-5962)

Verified. Each layer has its own classifiers for builtin math names and
control-flow calls:

| Function | Definitions |
| --- | --- |
| `parseMathName` | `src/semantics/SemanticsBuiltinPathHelpersShared.h:44`, `src/ir_lowerer/IrLowererBuiltinNameHelpers.cpp:206`, `src/ir_lowerer/IrLowererSetupMathHelpers.cpp:16` (different signature, a `hasMathImport` flag), `src/emitter/EmitterBuiltinCallPathHelpers.cpp:822` |
| `getBuiltinMinMaxName` | `src/semantics/SemanticsBuiltinPathHelpers.cpp:336`, `src/ir_lowerer/IrLowererBuiltinNameHelpers.cpp:266`, `src/emitter/EmitterBuiltinCallPathHelpers.cpp:860`, `src/ir/IrPrinterHelpers.cpp:238` |
| `getBuiltinAbsSignName` | semantics :343, lowerer :318, emitter :867, IR printer :256 |
| `getBuiltinSaturateName` | semantics :350, lowerer :322, emitter :874, IR printer :274 |
| `getBuiltinConvertName` | `src/semantics/SemanticsBuiltinPathHelpersRewrites.cpp:176`, lowerer :342, emitter :946, IR printer :292 |
| `getBuiltinClampName` | semantics :329, lowerer :261, IR printer :220 |
| `isSimpleCallName` | `src/semantics/SemanticsCallPathHelpers.cpp:123`, `src/semantics/SemanticsValidatorStatementBindingsPhasesBReferencePhasesA.cpp:459`, `src/ir_lowerer/IrLowererHelpers.cpp:296` and :589, `src/emitter/EmitterBuiltinCallPathHelpers.cpp:639`, `src/ir/IrPrinterHelpers.cpp:165`, `src/glsl_emitter/GlslEmitterHelpers.cpp:81` |
| `isReturnCall` | `src/semantics/SemanticsCallPathHelpers.cpp:160`, `src/ir_lowerer/IrLowererHelpers.cpp:413`, `src/emitter/EmitterHelpersBuiltins.cpp:249`, `src/ir/IrPrinterHelpers.cpp:151`, `src/ast/AstPrinter.cpp:17` |
| `isIfCall`, `isLoopCall`, `isWhileCall`, `isForCall`, `isRepeatCall` | 3 copies each: `SemanticsCallPathHelpers.cpp`, `IrLowererHelpers.cpp`, and the IR printer or the emitter |
| `trimLeadingSlash` | `src/semantics/SemanticsCallPathHelpers.cpp:15`, three more in `src/semantics/SemanticsValidatorInferCollectionCompatibilityInternal.h` (:366, :422, :435), `src/ir_lowerer/IrLowererCallHelpers.cpp:46`, `src/emitter/EmitterHelpersBuiltins.cpp:13`, `src/emitter/EmitterBuiltinMethodResolutionMetadataHelpers.cpp:21` |

`include/primec/ast/AstCallPathHelpers.h` (`normalizedCallName`) exists as a
shared header but is rarely used.

Divergence (verified for `parseMathName` and `getBuiltinMinMaxName`):

- Semantics reads `expr.name`, does no SoA-alias normalization, and rejects any
  path with a `/` other than `std/math/...`.
- The emitter reads `resolveExprPath(expr)` and calls
  `normalizeInternalSoaStorageBuiltinAlias` first.
- The lowerer normalizes the SoA alias and, for a path with a leading slash and
  more slashes, keeps only the last segment: `/foo/bar/min` classifies as
  `min`, which semantics and the emitter reject.
- The IR printer rejects any name containing `/`, so `/std/math/min` is not
  recognized there.
- The lowerer uses a table (`classifyMathName(expr, out, allowBare, {"min",
  "max"})`); the others repeat a three-line body per function.

A probe on 2026-11-12 did not turn this into a miscompile: a program defining
`/mylib/min` (returns `a + b`) and `/mylib/abs` (returns `a + 100`) and
returning `/mylib/min(3, 4) + /mylib/abs(1)` exits 108 on `--emit=vm` and
`--emit=native`, as it should. The lowerer apparently resolves user
definitions before it classifies. The divergence is latent, but any new call
site that classifies first would miscompile.

## 5. Collection spelling predicates (TODO-5963, TODO-5964)

Verified in part. `src/support/CollectionSpellingClassifier.cpp` (235 lines)
holds the authoritative removed-name sets (for example
`classifierRemovedVectorCompatibilityHelper`, lines 52-58). Adoption:

| Shared unit | semantics | ir_lowerer | emitter |
| --- | --- | --- | --- |
| `CollectionSpellingClassifier` | 53 files | 2 files (`IrLowererSetupTypeCollectionHelpers.cpp`, `IrLowererHelpers.cpp`) | 0 |
| `BuiltinArrayAccessNameClassifier` | 4 files | 1 file | 0 |
| `ReceiverElementFamilyClassifier` | 6 files | via `CanonicalReceiverType` | 0 |
| `StdlibSurfaceRegistry` | 92 files | 19 files | 4 files |

Even adopters keep wrappers: `src/semantics/SemanticsBuiltinPathHelpersShared.h:27`
re-wraps the classifier, and the lowerer wrapper at
`src/ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp:393` adds its own
suffix stripping.

Divergence (verified): the list `count`, `capacity`, `at`, `at_unsafe`,
`push`, `pop`, `reserve`, `clear`, `remove_at`, `remove_swap` exists three
times: in the classifier, hard-coded in the lowerer's
`allowsArrayVectorCompatibilitySuffix`
(`src/ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp:220`), and
registry-driven in the emitter
(`src/emitter/EmitterBuiltinCallPathHelpers.cpp:206`), which returns `true`
when registry metadata is missing.

Copies by name (from the duplicate-definition scan):

- Lowerer: `isCollectionVectorRecordPath` (5 copies),
  `experimentalCollectionMemberRoot` (5), `resolveScopedCallPath` (6, header-
  and file-local), `stdCollectionsRoot` (6, across layers),
  `allowsArrayVectorCompatibilitySuffix` (5), `isRemovedVectorCompatibilityHelper`
  (5, including `TemplateMonomorphCollectionCompatibilityPaths.cpp`),
  `experimentalCollectionTypePath`, `collectionHelperPathCandidates`.
- `stripGeneratedHelperSuffix` (6): `src/ir_lowerer/IrLowererCountAccessInternal.h:132`,
  `src/ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp:18`,
  `src/semantics/SemanticsValidatorExprArgumentValidationCollections.cpp:15`,
  `src/semantics/SemanticsValidatorBuildTransforms.cpp:32`, and two in the emitter.
- Semantics: `isSpecializedExperimentalKeyValueBackingPath` is defined in 8
  files (`SemanticsValidatorBuildInitializerInferenceCalls.cpp`,
  `SemanticsValidatorBuildInitializerInferenceHelpers.h`,
  `SemanticsValidatorExprReceiverPaths.cpp`,
  `SemanticsValidatorInferCollectionCompatibilityHelpers.h`,
  `SemanticsValidatorInferStructReturn.cpp`,
  `SemanticsValidatorResultHelpers.cpp`, `SemanticsValidatorStatement.cpp`,
  `SemanticsValidatorStatementReturnsHelpers.h`) with 5 distinct bodies
  (groups of 3, 2, 1, 1, 1). Also `canonicalKeyValueHelperPathLocal` (7),
  `resolveCanonicalKeyValueHelperNameFromSpelling` (6),
  `isCanonicalKeyValueAccessHelperName` (5),
  `keyValueConstructorSurfaceMetadataLocal` and
  `keyValueHelperSurfaceMetadataLocal` (4 layers each).
- The helper-path matching code (canonical root, then the helper token after
  it) is copied between `src/semantics/SemanticsCallPathHelpers.cpp:38` and
  `src/emitter/EmitterHelpersBuiltins.cpp:33`; only the handling of a trailing
  member path containing `/` differs.

## 6. Phase splitting under the file cap (TODO-5966, TODO-5967, TODO-5968)

Verified. `scripts/check_source_file_sizes.py` caps `.cpp`/`.h` files under
`src/` and `include/` at 1,200 lines (TODO-5353). The cap is fine. The way
files were brought under it is the problem: long functions were cut into
numbered phases.

- Files: `SemanticsValidatorStatementBindingsPhasesA/B/C.cpp`,
  `SemanticsValidatorStatementBindingsPhasesBReferencePhasesA/B.cpp`,
  `TemplateMonomorphExpressionRewritePhasesA/B/C/D.cpp`,
  `TemplateMonomorphExpressionRewritePhasesCInnerPhasesA/B.cpp` (with
  `TemplateMonomorphExpressionRewriteInnerState.h`),
  `SemanticsValidatorExprCallPhasesA/B.cpp`. Similar "Continued" and
  "TailDispatch" fragments exist in the lowerer
  (`IrLowererLowerStatementsBindingsContinued.h`,
  `IrLowererLowerStatementsExprContinued.h`,
  `IrLowererLowerEmitExprContinued.h`, `IrLowererLowerEmitExprTailDispatch*.h`).
- Each file holds numbered functions: 16 `rewriteExprReference` phases, 16
  `rewriteExpr` phases, 8 `validateExprCall` phases, 7 `validateBinding`
  phases, 6 `validateBindingReference` phases. Each takes about 10 parameters
  and returns `PhaseStatus`.
- Shared state lives in structs such as
  `SemanticsValidatorStatementBindingsReferenceState.h`, with about 15
  `std::function` members that used to be lambdas local to one function (its
  header says "split out of validateBindingPhase5").
- Each phase starts by re-binding that state, for example
  `[[maybe_unused]] auto &failBindingDiagnostic = st.failBindingDiagnostic;`.
  There are 484 `[[maybe_unused]]` lines in the `*Phases*.cpp` files.
- Each phase file repeats a 30 to 40 line include block.

Cost: phase N depends on state that phases 1 to N-1 filled in, every call to
the old lambdas goes through `std::function`, and no phase can be read or
tested alone. The fix is to turn the named lambdas into real helpers (free
functions or small classes with explicit inputs) and express each validator
or rewriter as an ordered list of small rules. The guidance for future splits
belongs in `AGENTS.md` and the docstring of `check_source_file_sizes.py`: a
file over the cap gets a component extracted, never a phase split.

## 7. Execution vs Definition intra-body diagnostics (TODO-5960)

Verified. `SemanticsValidator::collectExecutionIntraBodyCallDiagnostics`
(`src/semantics/SemanticsValidatorExecutionDiagnostics.cpp`, 490 lines,
called from `SemanticsValidatorPassesExecutions.cpp:151`) and
`collectDefinitionIntraBodyCallDiagnostics`
(`src/semantics/SemanticsValidatorPassesDiagnostics.cpp:77`, 528 lines, called
from `SemanticsValidatorPassesDefinitions.cpp:514`) are 75 to 90% the same
code, including file-local copies of `isBuiltinCollectionHelperName`,
`isRootedKeyValueCountShadowPath`, `isFlowEffectDiagnosticMessage` and
`isLoopBlockEnvelope`. The intended differences are the parameter setup
(`executionParams`), the related-span label (`"execution: "` vs
`"definition: "`) and the source line and column (`exec` vs `def`).

The copies have drifted:

- The Definition copy's `isBuiltinCollectionHelperName` (line 14) accepts
  `insert` by string compare and also `isGetHelperName` and `isRefHelperName`;
  the Execution copy (line 13) uses `isInsertHelperName` and has neither get nor
  ref.
- The Definition copy's control-flow check (line 116) includes `isPickCall`;
  the Execution copy does not.
- Only the Definition copy has `isCanonicalKeyValueAccessHelperPath` (line 54).
- Their TODO-4809 comments differ.

Whether the Execution copy's missing `pick`, `get` and `ref` handling is a bug
or an intended difference is not documented. Write a repro for each before
merging.

## 8. Reflection generated-helper AST builders (TODO-5965)

Reported. `SemanticsValidateReflectionGeneratedHelpers{,CloneDebug,Compare,
Serialization,State,Validate}.cpp` (175 to 652 lines each, split by feature)
each re-define the same anonymous-namespace AST builders:
`appendPublicVisibility` (5 copies), `makeFieldAccessExpr` (5),
`makeTypeBinding` (5), and `makeI32LiteralExpr`, `makeU64LiteralExpr`,
`makeBinaryCallExpr`, `makeNameExpr`, `makeStringLiteralExpr`,
`makeReturnStatementExpr` (2 to 4 each). Hashing the bodies shows 2 to 3
distinct variants of `makeFieldAccessExpr` and `makeTypeBinding`, so they have
already forked. `makeI32LiteralExpr` (4 copies) and `makeNameExpr` (about 3)
also exist outside the reflection files. Estimated saving: 60 to 100 lines per
file.

## 9. Float math lowering (TODO-5969)

Reported, sizes verified. The six files
`src/ir_lowerer/IrLowererOperator{AngleTrig,ArcHyperbolic,ClampMinMaxTrig,
ExpLogHyperbolic,PowAbsSign,SaturateRoundingRoots}Helpers.cpp` (437, 210, 503,
701, 526, 488 lines; 2,865 in total) hand-emit IR sequences, for example a
Taylor-series loop for `exp` near the top of
`IrLowererOperatorExpLogHyperbolicHelpers.cpp`. Identifier-normalized
duplication is 65 to 80% per file. Repeated pieces:

- the "requires exactly one argument" and "requires float argument" checks
  (22 and 14 occurrences);
- F32/F64 opcode selection such as `(argKind == Float64) ? IrOpcode::AddF64 :
  IrOpcode::AddF32`, about 50 times;
- a `pushFloatConst` lambda, defined about 16 times;
- store/load temp-local boilerplate.

Fix: a float opcode set selected once by kind (add, sub, mul, div, compare,
convert, push-constant), shared argument checks, `emitLoadLocal` /
`emitStoreLocal` helpers, and table-driven unary float ops (series and
constants as data).

## 10. Semantic-product fact plumbing (TODO-5970, TODO-5971)

Reported. `SemanticsValidator::rebindMergedWorkerPublicationFactSemanticNodeIds`
(`src/semantics/SemanticsValidatorSnapshotsPublicationFacts.cpp:140` to about
510) repeats one sequence per fact kind (direct call, method call, bridge,
callable summary, binding, return, query, try, on-error and more): define a
key lambda, `pruneReplacedEntriesBySnapshotKey`,
`appendMissingEntriesBySnapshotKey`, `stable_sort` with a hand-written
comparator, `rebindSemanticNodeIdsBySnapshotKey`. That is about 12 copies in
one 370-line function. `SemanticsValidatorSnapshotsFacts.cpp` has about 12
`*FactSnapshotForSemanticProduct` functions that each build, key and sort a
vector; `SemanticsValidatorSnapshotLocals.cpp` is the query/call/try variant
of the same shape. A per-fact-kind traits struct (key function, ordering
tuple) turns these into one template.

`src/frontend/SemanticProductLookups.cpp` (730 lines, 82%
identifier-normalized self-duplication) has about 20
`semanticProgramLookupPublished*By*` functions of one shape (guard on id 0 or
`InvalidSymbolId`, find in an index map, `lookupPublishedSemanticEntryByIndex`),
about 14 `semanticProgram*Fact*` accessors and about 12 `*View` functions. A
template over (index-map member, fact-vector member) shrinks it to roughly a
quarter.

## 11. Lowerer helpers copied per translation unit (TODO-5972) and SoA rewrites (TODO-5973)

Reported, partly verified.

Lowerer:

- `IrLowererLowerInferenceBaseKindInternal.h` (940 lines) and
  `IrLowererLowerInferenceDispatchSetupFileLocal.h` (569 lines) are each
  included by one `.cpp` (`...BaseKindHelpers.cpp`, `...DispatchSetup.cpp`).
  They hold the same inline helpers, renamed with a `BaseSetup` or
  `DispatchSetup` prefix to avoid clashes, for example
  `isBaseSetupFileErrorTypeText` / `isDispatchSetupFileErrorTypeText` and
  `inferBaseSetupSemanticFileHandleMethodKind` /
  `inferDispatchSetupSemanticFileHandleMethodKind`. They have started to
  drift: the Dispatch copy says "KeyValue" where the Base copy still says
  "Map", and only Dispatch has `inferDispatchSetupNameValueKindFromTypeText`.
- `resultErrorTypesMatch` is identical in
  `src/ir_lowerer/IrLowererPackedResultHelpers.cpp:68` and
  `src/ir_lowerer/IrLowererResultWhyHelpers.cpp:31` (the two files share 73
  exact 8-line windows).
- `IrLowererInlineParamHelpers.cpp` and `IrLowererInlinePackedArgs.cpp` both
  define `isPublishedKeyValueConstructorExpr`, `extractParameterTypeName`
  (identical) and `rewritePublishedKeyValueConstructorExpr` (about 23
  differing diff lines); `rewritePackedResultKeyValueConstructorExpr` in
  `IrLowererPackedResultHelpers.cpp` is a third near-copy.
- `IrLowererInlineParamHelpers.cpp` has four `emitInlineDefinitionCallParameters`
  overloads that forward 20+ positional `std::function` parameters to each
  other (one inserts `{}` for a single argument, around line 457). A context
  struct replaces them.
- The lowerer statement fragments `IrLowererLowerStatementsExprContinued.h`
  and `IrLowererLowerEmitExprResultHelpers.h` are among the most
  self-duplicated files (36 and 71 shared windows).

SoA rewrites (semantics):

- `SemanticsValidateExperimentalSoaFieldViewRewrites.cpp` (rewrites index
  expressions), `...HelperRewrites.cpp` (helper expressions) and
  `...CarrierRewrites.cpp`, and the
  `SemanticsValidateBuiltinSoaRewrites{,AccessCountRewrites,MutatorRewrites}.cpp`
  family each have a recursive expression walker and a statement walker,
  threaded with the same 8 to 9 context arguments (`bindings`, `allBindings`,
  `soaCollectionReturnDefinitions`, `specializedSoaVectorElementTypes`,
  `structFieldNames`, `structPaths`, `visibleSoaFieldHelpers`, the namespace).
- The receiver-resolution preamble (for example `tryReceiverBinding`) repeats.
- After renaming, the FieldView and HelperRewrites walkers differ in about 120
  of 200 lines: the same structure with different rewrite logic in the middle.
- `SemanticsValidateBuiltinSoaRewritesAccessCountRewrites.cpp` is the most
  duplicated file in the tree (67% exact, 75% identifier-normalized), which
  points to a table (name, arity, replacement path).

## 12. Stdlib: arity-unrolled SoA storage (no TODO)

`stdlib/std/collections/soa_storage/mutation.prime` (83% duplicated) and
`access_write.prime` (82%) write out functions such as
`soaColumns16Push<T0, ..., T15>` once per column count, with one parameter and
one `Reference<SoaColumn<Tn>>` binding per column. This comes from the
language having no variadic generics, not from code style; it shrinks only
when the language gains them. `stdlib/std/gfx/experimental.prime` and
`gfx.prime` share 73 windows; the experimental file is migration code by the
style boundary in `AGENTS.md`.

## 13. Small shared utilities (TODO-5974)

Verified unless noted.

- `jsonEscape`: `src/bin/primevm_main.cpp:29`, `src/runtime/VmDebugAdapter.cpp:11`,
  `src/runtime/VmDebugDapProtocol.cpp:311` (declared in `VmDebugDapProtocol.h:15`),
  `src/support/Diagnostics.cpp:9`. Same escaping; the last builds `\u00XX`
  with an `ostringstream`.
- C string literal escapers, three variants:
  - `escapeCppStringLiteral` (`src/backend/IrToCppEmitter.cpp:19`, used for
    `cpp-ir`/`exe-ir`) escapes only `\\`, `"`, `\n`, `\r`, `\t` and copies every
    other byte raw, including NUL, `?` and bytes of 0x7F and above.
  - `quote` (`src/backend/IrToOptCppEmitter.cpp:43`) escapes `"` and `\\` and
    writes everything outside 0x20-0x7E, and `?`, as a three-digit octal escape.
  - `quote` (`src/ir/IrModulePrinter.cpp:21`) escapes `\n`, `\t`, `\r`, `"`,
    `\\` and (reported) writes `\xNN` for other bytes.
  Not tested whether the `cpp-ir` variant produces wrong C++ for any string.
- `fnv1a64` (4, reported): `src/frontend/StdlibSymbolManifest.cpp:42`,
  `src/semantics/SemanticsValidatorResultHelpers.cpp:43`,
  `src/semantics/TemplateMonomorphSetupUtilities.cpp:13`,
  `src/support/TempPaths.cpp:32`. Same algorithm; `std::string` vs
  `std::string_view` signatures.
- `isAsciiDigit` and `isAsciiAlpha` (4 each): `src/frontend/Lexer.cpp:46-50`,
  `src/frontend/ImportResolverHelpers.cpp:30-34`, `src/parser/ParserHelpers.cpp:17-21`,
  `src/semantics/SemanticsHelpersValidation.cpp:10-14`.
- `isHexDigitChar` (3): `src/frontend/Lexer.cpp:42`, `src/parser/ParserHelpers.cpp:191`,
  `src/text_filter/TextFilterHelpers.cpp:38`.
- `trim` / `trimAscii` / `trimWhitespace` (3 each, including
  `src/frontend/ImportResolverHelpers.cpp`, `src/ui/Markdown.cpp`,
  `src/ui/MarkdownTable.cpp`).
- Parser: `isIgnorableToken` (5 files), `isArgumentLabelValueStart` (3).
- Wasm: `appendI32MemArg` in `src/wasm_emitter/WasmEmitter.cpp:91`,
  `WasmEmitterIo.cpp:46`, `WasmEmitterWasiHelpers.cpp:41`.

## 14. IR opcode metadata (TODO-5975)

Reported. About 20 independent `switch` statements over `IrOpcode` exist
(approximate `case IrOpcode::` counts: `IrToOptCppEmitter.cpp` 155,
`IrPureSemantics.h` 139, `IrCfg.cpp` 131, `NativeEmitterFunctionEmit.cpp` 130,
`NativeJitOpcodes.h` 115, `IrToCppEmitterInstructionEmitter.cpp` 115,
`NativeEmitterInternalsArm64RegAlloc.h` 111, `IrToGlslEmitterFunctionEmitter.cpp`
109, `WasmEmitter.cpp` 105, `IrVirtualRegisterScheduler.cpp` 93,
`NativeEmitterInternalsX64RegAlloc.h` 85, `NativeEmitterPromotion.h` 83,
`NativeEmitterRegAlloc.cpp` 77). The per-target emit switches are legitimate.

Already shared and good: `include/primec/ir/IrOpcodeTable.h` (name and
validator flags per opcode), the stack-effect table in `src/ir/IrCfg.cpp:42`
(used by validation, virtual-register lowering, local SROA and constant
folding), and `IrPureSemantics.h` (`evalPureOpcode`, `IrPureOpcodeArityTable`;
used by both VM kernels, the kernel boundary, the native JIT host, `optcpp` and
constant folding).

Missing: the table has no category (pure, memory, control, I/O) and no result
kind, so passes re-derive them: `isReturnOpcode` (`src/ir/IrCfg.cpp:9`,
`src/ir/IrInliner.cpp:12`, `src/runtime/VmFastKernelPrepare.cpp:12`),
`isConstantPush` (`src/ir/optimizer/IrPassCfgSimplify.cpp:9`,
`src/ir/optimizer/IrPassConstFold.cpp:17`, `src/runtime/VmFastKernelPrepare.cpp:82`),
`arithmeticKind` (`VmFastKernelPrepare.cpp:53`), `pureOpcodeUnderflowMessage`
(`src/runtime/VmKernelBoundary.cpp:31`), the latency switch in
`IrVirtualRegisterScheduler.cpp`, and the whitelist switches in
`NativeEmitterPromotion.h:54` and `NativeJitOpcodes.h:23`.

## 15. Diagnostics helpers (TODO-5977)

Reported. `failExprDiagnostic` is a proper member
(`src/semantics/SemanticsValidatorPrivateCore.h:399`, used from 63 files), but
there are also 81 local `auto fail*Diagnostic = [&]` lambdas
(`failExprRootDiagnostic` x9, `failStatementDiagnostic` x4, and two each of
`failPassesEffects*`, `failNamedArgument*`, `failInferDefinition*`,
`failContainerHelper*`), with no shared builder or sink, so each decides its
own span, label and related-span logic. Not checked whether they differ in
behavior.

## 16. What is fine as it is

- Native x64 and arm64: `NativeEmitterFunctionEmit.cpp` is a template over the
  target emitter (`kIsArm64`), so the main 130-case opcode switch is shared,
  and `NativeEmitterRegAlloc.cpp` holds target-independent liveness and
  coloring. The per-target files (`X64RegAlloc.h` 1,125 lines,
  `Arm64RegAlloc.h` 1,186) have parallel operand-juggling logic (commutation,
  immediate placement), which an estimated 10 to 15% of could share, but most
  of the rest is encodings and ABI. Not worth a merge now.
- `IrToCpp` (a stack machine in C++, about 1,850 lines) and `IrToOptCpp` (a
  register-style emitter, 1,040 lines plus an 856-line runtime header) share no
  code. Merging them would change the default `cpp` output, so that is a
  product decision, not a cleanup.
- `src/ui` has almost no duplication (0.2%). `NativeUiHeadless*` is a test
  double for `NativeUiAppKit.mm`, not a copy.

## 17. Working these items

The TODOs are meant to be worked two at a time by agents in their own git
worktrees while another worker uses the same queue. These rules keep them out
of each other's way.

Claiming (also Operating Rule 18 in `docs/todo.md`):

1. Before starting, fetch origin/master. Pick a `ready` leaf whose block has no
   `claimed_by` line.
2. Change its `- [ ]` to `- [~]` and add
   `claimed_by: <worker-name> (<branch or worktree>), <YYYY-MM-DD>` under
   `status`. Commit that change alone ("Claim TODO-NNNN") and push it to
   master before writing any code.
3. If the push is rejected, fetch and rebase. If the leaf now shows another
   worker's claim, drop yours and pick a different leaf.
4. When abandoning a leaf, remove the `claimed_by` line, set `- [ ]` again and
   push. When finishing, the block moves to `docs/todo_finished.md` as usual.

Concurrency:

- Two leaves may run at the same time only if their task blocks do not name
  the same files. Each block lists its surface and the leaves it must not run
  alongside.
- Heavy commands (`./scripts/compile.sh`, `cmake --build`, `ctest`, any
  `PrimeStruct_*_tests` binary) still run one at a time on the machine
  (`AGENTS.md`, Hard RAM safety rule). Agents in worktrees of one clone take a
  lock around each heavy command: `mkdir` of
  `$(git rev-parse --git-common-dir)/primestruct-heavy.lock` (atomic; retry
  every 30 seconds until it succeeds), write the owner and start time into a
  file inside it, and `rm -rf` it when the command ends, also on failure.
  Reading, editing and committing need no lock.
- A worktree keeps its own `build-release/`. Reusing the worktree for the next
  leaf (reset to the fresh origin/master) keeps builds incremental; remove it
  with `git worktree remove` when the agent is done.
