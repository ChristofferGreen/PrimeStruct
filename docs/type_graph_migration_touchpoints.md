# Type-Graph Migration Touchpoints

This note records the concrete first migration surfaces selected from the live
Group 11 queue in `docs/todo.md`, including the immediate validator/lowerer
files to touch for each slice.

## 1) Direct-call/callee non-template inference island

Selected island:
- Local `auto` initializer direct-call binding inference (non-method call path)
  that still resolves/falls back through call-shape and return-shape probes.

Validator touchpoints:
- `src/semantics/SemanticsValidatorBuildInitializerInferenceCalls.cpp`
  - `inferCallInitializerBinding(...)`
  - `inferResolvedDirectCallBindingType(...)`
  - `inferDeclaredDirectCallBinding(...)`
- `src/semantics/SemanticsValidatorBuildInitializerInference.cpp`
  - `inferBindingTypeFromInitializer(...)` graph lookup gate and call fallback.

Lowerer touchpoints:
- `src/ir_lowerer/IrLowererLowerInferenceCallReturnSetup.cpp`
  - `runLowerInferenceExprKindCallReturnSetup(...)`
- `src/ir_lowerer/IrLowererLowerInferenceDispatchSetup.cpp`
  - call dispatch path that applies `inferCallExprDirectReturnKind(...)` before
    fallback stages.

## 2) Receiver/method-call non-template inference island

Selected island:
- Method-target/type inference for receiver-driven collection access and
  value-method forwarding (`at`/`at_unsafe` + map value follow-through).

Validator touchpoints:
- `src/semantics/SemanticsValidatorExprMethodTargetResolution.cpp`
  - receiver access + method-target forwarding path (collection access blocks).
- `src/semantics/SemanticsValidatorInferMethodResolution.cpp`
  - receiver query-type based method resolution helpers.

Lowerer touchpoints:
- `src/ir_lowerer/IrLowererSetupTypeMethodCallResolution.cpp`
  - method-call receiver probing and target resolution from expression shape.
- `src/ir_lowerer/IrLowererLowerInferenceCallReturnSetup.cpp`
  - method-call return-kind inference branch.

## 3) Collection helper/access fallback non-template inference island

Selected island:
- Bare helper/access rewrite and fallback path for vector/map helper calls.

Validator touchpoints:
- `src/semantics/SemanticsValidatorExprDirectCollectionFallbacks.cpp`
  - `validateExprDirectCollectionFallbacks(...)`
- `src/semantics/SemanticsValidatorBuildInitializerInferenceCalls.cpp`
  - collection helper/access binding inference and canonical fallback probes.

Lowerer touchpoints:
- `src/ir_lowerer/IrLowererLowerInferenceFallbackSetup.cpp`
  - collection-access fallback kind inference.
- `src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h`
  - collection helper/access emit-time resolution helpers.

## 4) Shared dependency state between CT-eval and graph validation

Selected dependency state:
- Query-call type state (`queryTypeText`, receiver binding type, result shape).

Shared-state publication/consumption touchpoints:
- `include/primec/SemanticProduct.h`
  - `SemanticProgramQueryFact`.
- `src/semantics/SemanticsValidatorInferGraph.cpp`
  - graph local-`auto` query snapshot capture (`inferQuerySnapshotData(...)`).
- `src/semantics/SemanticsValidatorInferCollectionReturnInference.cpp`
  - `inferQueryExprTypeText(...)` query-type consumption path.

## 5) CT-eval-only dependency state that needs adapter

Selected CT-eval-only state:
- Templated fallback type inference used during monomorph/CT-style probing.

Adapter seam touchpoints:
- `src/semantics/TemplateMonomorphFallbackTypeInference.h`
  - `inferExprTypeTextForTemplatedVectorFallback(...)`
  - `inferDefinitionReturnBindingForTemplatedFallback(...)`
- `src/semantics/TemplateMonomorphImplicitTemplateInference.h`
  - `inferImplicitTemplateArgs(...)` fallback usage.

## 6) Explicit template-argument inference surface

Selected surface:
- Explicit template arguments on call/transform rewrite during monomorph type
  resolution.

Touchpoints:
- `src/semantics/TemplateMonomorphTypeResolution.h`
  - `resolveTypeString(...)` and `rewriteTransforms(...)` explicit-arg path.

## 7) Implicit template-inference surface

Selected surface:
- Implicit template argument synthesis from call arguments and inferred binding
  types.

Touchpoints:
- `src/semantics/TemplateMonomorphImplicitTemplateInference.h`
  - `inferImplicitTemplateArgs(...)`.

## 8) Revalidation/monomorph follow-up for selected template surfaces

Selected follow-up:
- Re-run template monomorphization as part of graph-preparation and route
  migrated template inference outputs through that single staged entry.

Touchpoints:
- `src/semantics/TypeResolutionGraphPreparation.cpp`
  - `prepareProgramForTypeResolutionAnalysis(...)` (`monomorphizeTemplates(...)`).
- `src/semantics/TemplateMonomorph.cpp`
  - specialization/instantiation entrypoints and cache usage.

## 9) Omitted-envelope graph family

Selected family:
- Omitted struct initializer envelopes.

Touchpoints:
- `src/semantics/SemanticsValidatorBuildStructFields.cpp`
  - omitted-envelope field binding resolution path.
- `src/semantics/SemanticsValidatorBuildUtility.cpp`
  - envelope value extraction helper.

## 10) Local-`auto` graph initializer-family surface

Selected surface:
- `if`-branch local `auto` bindings.

Touchpoints:
- `src/semantics/SemanticsValidatorInferGraph.cpp`
  - local-`auto` capture and branch/control-flow traversal (`isIfCall(...)`).
- `src/semantics/SemanticsValidatorBuildInitializerInference.cpp`
  - binding inference entry that consumes graph local-`auto` state.

## 11) Local-`auto` graph control-flow join surface

Selected surface:
- `if` join return inference.

Touchpoints:
- `src/semantics/SemanticsValidatorStatementReturns.cpp`
  - branch return/join compatibility checks.
- `src/semantics/SemanticsValidatorInferCollectionReturnInference.cpp`
  - `if` branch type-text merge in query inference.
