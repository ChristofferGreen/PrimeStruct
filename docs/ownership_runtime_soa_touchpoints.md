# Ownership/Runtime + SoA Touchpoints

This file inventories the concrete code touchpoints for the live Group 13/14
TODO queues in `docs/todo.md`.

## 1) Vector constructor migration touchpoints

Goal reference: move canonical imported vector constructor usage from fixed-arity
helpers to variadic `vector(values...)`.

- Stdlib canonical constructor routing in `stdlib/std/collections/vector.prime`
  now exposes only variadic `/std/collections/vector/vector<T>([args<T>] values)`;
  the previous arity-specific wrapper overloads were removed.
- Compatibility wrapper helper families still expose fixed-arity constructor
  spellings in `stdlib/std/collections/collections.prime`
  (`vectorSingle`, `vectorPair`, ..., `vectorOct`).
- Experimental constructor compatibility forwarders remain in
  `stdlib/std/collections/experimental_vector.prime`
  (`vectorSingle`, `vectorPair`, ..., `vectorOct`) and currently route to the
  variadic `vector<T>([args<T>] values)`.
- Compiler call-resolution and constructor-path matching still contain explicit
  fixed-arity constructor name lists. Key files:
  - `src/semantics/TemplateMonomorphExperimentalCollectionConstructorPaths.h`
  - `src/semantics/TemplateMonomorphTypeResolution.h`
  - `src/semantics/SemanticsValidatorBuildCallResolution.cpp`
  - `src/semantics/SemanticsValidatorInferCollectionCallResolution.cpp`
  - `src/semantics/SemanticsValidatorExprVectorHelpers.cpp`
  - `src/semantics/SemanticsValidatorStatementVectorResolution.cpp`
  - `src/ir_lowerer/IrLowererSetupTypeDeclaredCollectionInference.cpp`
- Test and fixture sources still use fixed-arity constructor helper names
  heavily (for compatibility coverage), especially:
  - `tests/unit/test_compile_run_vector_conformance_sources.h`
  - `tests/unit/test_compile_run_vector_conformance_namespace_sources.h`
  - `tests/unit/test_compile_run_native_backend_vector_and_experimental_map_variadics.h`
  - `tests/unit/test_compile_run_native_backend_map_and_vector_variadics.h`

## 2) Map constructor migration touchpoints

Goal reference: move canonical imported map constructor usage from fixed-arity
helpers to variadic `map(entry(...), ...)`.

- Stdlib canonical constructor routing in `stdlib/std/collections/map.prime`
  now exposes variadic `/std/collections/map/map<K, V>([args<Entry<K, V>>] entries)`
  plus `/std/collections/map/entry<K, V>(...)`; the previous key/value
  arity-specific canonical overloads were removed.
- Fixed-arity compatibility wrappers still exist in the same file and now route
  through canonical `/std/collections/map/entry<K, V>(...)`:
  `mapSingle`, `mapDouble`, `mapPair`, ..., `mapOct`.
- Experimental fixed-arity compatibility helpers still exist in
  `stdlib/std/collections/experimental_map.prime`
  (`mapSingle`, `mapDouble`, ..., `mapOct`) in addition to the variadic
  `map<K, V>([args<Entry<K, V>>] entries)` and `entry<K, V>(key, value)`.
- Compiler matching logic still enumerates fixed-arity map helper names.
  Representative files:
  - `src/semantics/SemanticsValidatorBuildInitializerInferenceCalls.cpp`
  - `src/ir_lowerer/IrLowererAccessTargetResolution.cpp`
  - `src/ir_lowerer/IrLowererInlineParamHelpers.cpp`
  - `src/ir_lowerer/IrLowererInlinePackedArgs.cpp`
  - `src/ir_lowerer/IrLowererPackedResultHelpers.cpp`
  - `src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h`
  - `src/ir_lowerer/IrLowererSetupTypeDeclaredCollectionInference.cpp`
  - `src/ir_lowerer/IrLowererResultMetadataHelpers.cpp`
  - `src/ir_lowerer/IrLowererStructSlotLayoutHelpers.cpp`
- Test and fixture sources still use fixed-arity constructor helper families
  broadly, especially:
  - `tests/unit/test_compile_run_map_conformance_sources.h`
  - `tests/unit/test_compile_run_map_conformance_namespace_sources.h`
  - `tests/unit/test_compile_run_vm_collections_shim_maps_a.h`
  - `tests/unit/test_compile_run_native_backend_collections_shims_maps_a.h`

## 3) Builtin canonical map growth touchpoints

Goal reference: route remaining builtin canonical `map<K, V>` borrowed/non-local
growth mutation receiver families through the shared grown-pointer
write-back/repoint path.

Current lowering path for canonical insert rewrite:
- Statement path rewrites map insert helpers to
  `/std/collections/map/insert_builtin` in
  `src/ir_lowerer/IrLowererStatementCallEmission.cpp`, and now also probes
  map-returning helper receivers through `resolveDefinitionCall(...)` +
  `inferDeclaredReturnCollection(...)` before deciding whether to rewrite.
- Expression/tail-dispatch path performs the same rewrite in
  `src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h`, and now likewise
  probes map-returning helper receivers through `resolveDefinitionCall(...)` +
  `inferDeclaredReturnCollection(...)` before deciding whether to rewrite.
- Lowering for rewritten calls enters the inline builtin map insert path in
  `src/ir_lowerer/IrLowererLowerInlineCalls.h`.
- Grow/copy/repoint + write-back logic is centralized in
  `emitBuiltinCanonicalMapInsertOverwriteOrGrow(...)` in
  `src/ir_lowerer/IrLowererAccessLoadHelpers.cpp`.

Receiver recognition boundary used by rewrite/lowering:
- `resolveMapAccessTargetInfo(...)` in
  `src/ir_lowerer/IrLowererAccessTargetResolution.cpp` currently recognizes
  direct map locals, wrapped-map locals with `dereference(...)`, selected
  args-pack element forms, and direct constructor/call-derived map targets.

Receiver families to track as potentially still outside the shared write-back
path (based on current recognition hooks):
- Nested field-access/non-local lvalue receivers reached through map-returning
  helper calls now route through the shared rewrite path on direct canonical
  `/std/collections/map/insert(...)` calls (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`;
  runtime coverage:
  `tests/unit/test_compile_run_map_conformance_sources.h` +
  backend expectation suites).
- Helper-return borrowed method-sugar receiver forms now route through that
  same rewrite path as well (coverage:
  `tests/unit/test_semantics_calls_and_flow_collections_chunks/test_semantics_calls_and_flow_collections_17.h` and
  `tests/unit/test_compile_run_map_conformance_sources.h` + backend
  expectation suites).
- Direct canonical field-access/non-local lvalue receiver forms (for example
  `/std/collections/map/insert(..., holder.values, ...)`) now route through the
  shared rewrite path by reusing explicit call-template map typing or inferred
  canonical callee receiver typing when the receiver expression itself is not a
  standalone call target (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Method-sugar field-access/non-local receiver forms (`holder.values.insert(...)`)
  now route through that same rewrite path by inferring receiver map typing from
  resolved canonical method-callee receiver parameter types when method template
  arguments are omitted (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Compatibility helper alias field-access/non-local receiver forms (for example
  `/std/collections/mapInsert(..., holder.values, ...)` and method-sugar
  `holder.values.insert(...)` resolved through alias callees) now route through
  that same rewrite path by reusing resolved callee receiver parameter typing
  when template arguments are omitted (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Location-wrapped receiver forms (for example `location(holder.values)` and
  `location(makeValues())`) now route through that same rewrite path by peeling
  `location(...)` wrappers before canonical/helper-alias receiver-typing and
  helper-return collection inference when template arguments are omitted (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Location-wrapped local-map receiver forms (for example `location(valuesLocal)`)
  now route through that same rewrite path by peeling `location(...)` wrappers
  before local map-kind probing and reusing local map key/value metadata when
  template arguments are omitted (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Dereference-wrapped helper-return receiver forms (for example
  `dereference(makeValuesRef())`) now route through that same rewrite path by
  inferring map typing from resolved helper return collection declarations when
  template arguments are omitted (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Dereference-wrapped field-access/non-local receiver forms (for example
  `dereference(holder.valuesRef)`) now route through that same rewrite path by
  reusing resolved canonical/helper-alias callee receiver parameter typing when
  template arguments are omitted (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Nested location+dereference receiver chains (for example
  `location(dereference(location(makeValuesRef())))` and
  `location(dereference(location(holder.valuesRef)))`) now route through that
  same rewrite path by peeling stacked wrappers before helper-return and
  field-access receiver typing inference when template arguments are omitted
  (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Stacked location+dereference wrappers around args-pack map-access receivers
  (for example `location(dereference(location(/map/at(mapsPack, 0i32))))`)
  now route through that same rewrite path by reusing peeled-receiver
  map-target inference before canonical/helper-alias receiver typing (IR
  coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Args-pack map-access receiver-location wrappers (for example
  `/map/at(location(mapsPack), 0i32)`) now route through that same rewrite
  path by peeling receiver-side `location(...)`/`dereference(...)` wrappers
  before args-pack map-target probing (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Non-local args-pack map-access receiver sources (for example
  `/map/at(location(holder.mapsPack), 0i32)`) now route through that same
  rewrite path by inferring map key/value typing from resolved `/map/at`
  args-pack receiver parameter declarations when local-name probing is
  unavailable (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Non-local args-pack map-access `at_unsafe` receiver sources (for example
  `/map/at_unsafe(location(holder.mapsPack), 0i32)`) now route through that
  same rewrite path by extending args-pack map-target recognition and
  receiver-typing inference parity with `at` (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Non-local args-pack map-access compatibility-alias receiver sources (for
  example `/std/collections/mapAt(location(holder.mapsPack), 0i32)` and
  `/std/collections/mapAtUnsafe(location(holder.mapsPack), 0i32)`) now route
  through that same rewrite path by extending args-pack receiver-source
  map-target recognition/rewrite inference parity from `/map/at` /
  `/map/at_unsafe` to alias helper paths (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Non-local method-sugar args-pack map-access receiver sources (for example
  `location(holder.mapsPack).at(0i32)` and
  `location(holder.mapsPack).at_unsafe(0i32)`) now route through that same
  rewrite path by extending args-pack access recognition/rewrite inference
  parity to method-form access helpers when their resolved receiver parameter
  typing is args-pack based (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Non-local method-sugar args-pack map-access compatibility alias receiver
  sources using bare helper stems (for example
  `location(holder.mapsPack).mapAt(0i32)` and
  `location(holder.mapsPack).mapAtUnsafe(0i32)`) now route through that same
  rewrite path by extending method-form args-pack access recognition parity
  from `at`/`at_unsafe` to compatibility alias helper stems when resolved
  receiver parameter typing is args-pack based (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Non-local method-sugar args-pack map-access compatibility `_ref` receiver
  sources using canonical + alias stems (for example
  `location(holder.mapsPack).at_ref(0i32)`,
  `location(holder.mapsPack).at_unsafe_ref(0i32)`,
  `location(holder.mapsPack).mapAtRef(0i32)`, and
  `location(holder.mapsPack).mapAtUnsafeRef(0i32)`) now route through that
  same rewrite path by extending method-form args-pack access recognition
  parity from `at`/`at_unsafe` + `mapAt`/`mapAtUnsafe` to `_ref` helper
  variants when resolved receiver parameter typing is args-pack based (IR
  coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Non-local method-sugar args-pack map-access compatibility alias receiver
  sources using PascalCase helper stems (for example
  `location(holder.mapsPack).At(0i32)` and
  `location(holder.mapsPack).AtUnsafe(0i32)`) now route through that same
  rewrite path by extending method-form args-pack access recognition parity to
  PascalCase helper stems when resolved receiver parameter typing is args-pack
  based (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Non-local method-sugar args-pack map-access compatibility alias `_ref`
  receiver sources using PascalCase helper stems (for example
  `location(holder.mapsPack).AtRef(0i32)` and
  `location(holder.mapsPack).AtUnsafeRef(0i32)`) now route through that same
  rewrite path by extending method-form args-pack access recognition parity to
  PascalCase `_ref` helper stems when resolved receiver parameter typing is
  args-pack based (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Non-local method-sugar args-pack map-access compatibility alias receiver
  sources that carry generated helper suffixes (for example
  `location(holder.mapsPack).mapAt__generated(0i32)` and
  `location(holder.mapsPack).AtUnsafeRef__generated(0i32)`) now route through
  that same rewrite path by matching method-form args-pack access helper stems
  after stripping generated suffixes when resolved receiver parameter typing is
  args-pack based (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`).
- Non-local method-sugar args-pack map-access receiver-source chains now also
  route through that same rewrite path when the mutating method call itself
  carries generated helper suffixes (for example
  `location(holder.mapsPack).mapAt__generated(0i32).insert__generated(...)`)
  by matching method-form insert helper stems after stripping generated
  suffixes in both statement and tail-dispatch rewrite paths (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`;
  rewrite parity touchpoints:
  `src/ir_lowerer/IrLowererStatementCallEmission.cpp` and
  `src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h`).
- Non-local direct-call args-pack map-access `_ref` receiver-source chains now
  route through that same rewrite path as well (for example
  `/map/at_ref(location(holder.mapsPack), 0i32)`,
  `/map/at_unsafe_ref(location(holder.mapsPack), 0i32)`, and
  `/std/collections/mapAtUnsafeRef__generated(location(holder.mapsPack), 0i32)`)
  by extending direct-call args-pack access recognition/rewrite inference
  parity from `at` / `at_unsafe` + compatibility alias forms to `_ref`
  canonical + compatibility generated-suffix variants (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`;
  rewrite parity touchpoints:
  `src/ir_lowerer/IrLowererAccessTargetResolution.cpp` and
  `src/ir_lowerer/IrLowererStatementCallEmission.cpp`).
- Non-local direct-call args-pack map-access receiver-source chains now also
  route through that same rewrite path when using bare helper stems (for
  example `at_ref(location(holder.mapsPack), 0i32)` and
  `mapAtUnsafeRef__generated(location(holder.mapsPack), 0i32)`) by extending
  direct-call args-pack access recognition parity from path-qualified call
  forms to bare helper-stem call forms (including generated suffixes) when
  resolved receiver parameter typing is args-pack based (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`;
  rewrite parity touchpoint:
  `src/ir_lowerer/IrLowererStatementCallEmission.cpp`).
- Non-local direct-call args-pack map-access receiver-source chains now also
  route through that same rewrite path when the mutating insert call uses bare
  helper stems with generated suffixes (for example
  `insert__generated(at_ref(location(holder.mapsPack), 0i32), ...)` and
  `mapInsert__generated(mapAtUnsafeRef__generated(location(holder.mapsPack), 0i32), ...)`)
  by extending direct-call mutating insert recognition parity from
  path-qualified/helper-alias forms to bare helper-stem forms (including
  generated suffixes) while preserving args-pack receiver-source typing
  inference (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`;
  rewrite parity touchpoints:
  `src/ir_lowerer/IrLowererStatementCallEmission.cpp` and
  `src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h`).
- Non-local field-access receiver chains now also route through that same
  rewrite path when resolved mutating helper callee paths use bare generated
  insert helper stems (for example `/insert__generated` and
  `/mapInsert__generated`) by extending map-insert-like callee classification
  parity from path-qualified/helper-alias forms to bare generated helper-path
  forms for receiver-typing inference (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`;
  rewrite parity touchpoint:
  `src/ir_lowerer/IrLowererStatementCallEmission.cpp`).
- Non-local field-access receiver chains now also route through that same
  rewrite path when resolved mutating helper callee paths use bare generated
  PascalCase compatibility insert helper stems (for example
  `/MapInsert__generated` and `/MapInsertRef__generated`) by extending
  map-insert-like callee classification and bare mutating insert stem
  recognition parity from lowercase compatibility/helper-alias forms to
  PascalCase compatibility helper-path forms for receiver-typing inference (IR
  coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`;
  rewrite parity touchpoints:
  `src/ir_lowerer/IrLowererStatementCallEmission.cpp` and
  `src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h`).
- Non-local field-access receiver chains now also route through that same
  rewrite path when mutating method-call helper names themselves carry
  generated compatibility insert stems (for example
  `holder.values.mapInsert__generated(...)` and
  `holder.values.MapInsertRef__generated(...)`) by extending method-form
  insert stem recognition parity from `insert__generated` to compatibility
  lowercase/PascalCase generated helper stems in statement and tail-dispatch
  rewrite paths (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`;
  rewrite parity touchpoints:
  `src/ir_lowerer/IrLowererStatementCallEmission.cpp` and
  `src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h`).
- Non-local field-access receiver chains now also route through that same
  rewrite path when mutating slash-method helper names themselves are
  path-qualified generated compatibility insert stems (for example
  `holder.values./std/collections/mapInsert__generated(...)` and
  `holder.values./std/collections/MapInsertRef__generated(...)`) by extending
  method-form insert stem normalization to strip helper path segments before
  generated-suffix matching in statement and tail-dispatch rewrite paths (IR
  coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`;
  rewrite parity touchpoints:
  `src/ir_lowerer/IrLowererStatementCallEmission.cpp` and
  `src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h`).
- Non-local field-access receiver chains now also route through that same
  rewrite path when direct-call helper names are path-qualified generated
  PascalCase compatibility insert stems (for example
  `/std/collections/MapInsert__generated(...)` and
  `/std/collections/MapInsertRef__generated(...)`) by extending map-wrapper
  alias/classifier parity to map PascalCase compatibility insert stems onto
  canonical insert helper names before statement/tail-dispatch rewrite matching
  (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`;
  rewrite parity touchpoints:
  `src/ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp`,
  `src/ir_lowerer/IrLowererCountAccessClassifiers.cpp`,
  `src/ir_lowerer/IrLowererStatementCallEmission.cpp`, and
  `src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h`).
- Non-local field-access receiver chains now also route through that same
  rewrite path when direct-call helper names use path-qualified PascalCase
  stdlib map-wrapper prefixes (for example
  `/std/collections/MapInsert__generated(...)` and
  `/std/collections/MapInsertRef__generated(...)`) by extending map-helper
  alias/classifier wrapper-prefix matching parity from lowercase
  `std/collections/map...` to PascalCase `std/collections/Map...` before
  canonical insert rewrite matching (IR coverage:
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_74.h`;
  rewrite parity touchpoints:
  `src/ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp`,
  `src/ir_lowerer/IrLowererCountAccessClassifiers.cpp`,
  `src/ir_lowerer/IrLowererStatementCallEmission.cpp`, and
  `src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h`).
- Remaining non-local lvalue receiver shapes that do not currently route
  through that same path (for example non-local receiver forms beyond
  field-access, helper-return, and args-pack map-access receiver-source `at` /
  `at_unsafe` / `at_ref` / `at_unsafe_ref` + compatibility-alias path/stem
  call/method chains (including PascalCase/PascalCase `_ref` and
  generated-suffix variants on receiver-access + mutating-insert stems)).
- Temporary/helper-return receiver shapes that do not provide a stable writable
  lvalue target for pointer write-back now have deterministic compile-diagnostic
  coverage for both direct canonical and method-sugar insert forms (see
  `tests/unit/test_compile_run_map_conformance_sources.h`,
  `tests/unit/test_compile_run_map_conformance_expectations.h`, and the
  native/C++/VM conformance harness suites).

## 4) SoA compiler-owned fallback inventory by layer

Goal reference: inventory remaining compiler-owned `soa_vector` fallback and
special-case sites to retire as stdlib substrate ownership expands.

Semantics-layer pending fallback diagnostics:
- Pending borrowed-view diagnostics and fallback helpers route through:
  - `src/semantics/SemanticsBuiltinPathHelpers.cpp`
  - `src/semantics/SemanticsValidatorExprMapSoaBuiltins.cpp`
  - `src/semantics/SemanticsValidatorExpr.cpp`
  - `src/semantics/SemanticsValidatorExprMutationBorrows.cpp`
  - `src/semantics/SemanticsValidatorStatementBindings.cpp`
  - `src/semantics/SemanticsValidatorStatementReturns.cpp`
  - `src/semantics/SemanticsValidatorInferDefinition.cpp`
  - `src/semantics/TemplateMonomorphImplicitTemplateInference.h`
  - Borrowed-view `ref` pending fallback gating now checks visibility of either
    same-path `/soa_vector/ref` or canonical
    `/std/collections/soa_vector/ref` helper surfaces before emitting pending
    diagnostics, so canonical-only import surfaces no longer trip compiler-owned
    pending fallbacks (`src/semantics/SemanticsValidatorBuildInitializerInference.cpp`,
    `src/semantics/SemanticsValidatorExprMethodCompatibilitySetup.cpp`,
    `src/semantics/SemanticsValidatorInferLateFallbackBuiltins.cpp`,
    `src/semantics/SemanticsValidatorInferPreDispatchCalls.cpp`, and
    `src/semantics/TemplateMonomorphImplicitTemplateInference.h`).
  - Old-surface `get`/`ref` helper fallback gates now treat canonical
    `/std/collections/soa_vector/*` helper visibility as equivalent to
    same-path `/soa_vector/*` visibility in return-type inference and template
    fallback lookup, so canonical helper imports no longer trigger
    compiler-owned pending diagnostics for those old-surface call shapes
    (`src/semantics/SemanticsValidatorStatementReturns.cpp`,
    `src/semantics/SemanticsValidatorInferCollectionReturnInference.cpp`, and
    `src/semantics/TemplateMonomorphImplicitTemplateInference.h`).
  - Helper-return definition type-inference fallback now resolves helper targets
    through visible same-path or canonical stdlib SoA helper paths (instead of
    requiring visible same-path `/soa_vector/<helper>` definitions), so
    canonical helper imports can satisfy fallback inference for non-core helper
    names (`src/semantics/SemanticsValidatorInferDefinition.cpp`).
  - Late expression builtin validation fallback gates for old-surface
    `count`/`get`/`ref` call forms now key off old-surface call shape plus
    visible same-path or canonical stdlib helper targets before falling back,
    so canonical helper imports do not force compiler-owned pending diagnostics
    for those old-surface call forms
    (`src/semantics/SemanticsValidatorExprCountCapacityMapBuiltins.cpp` and
    `src/semantics/SemanticsValidatorExprMapSoaBuiltins.cpp`).
  - SoA field-helper-versus-field-view fallback probes in initializer and
    method target inference now key off visible same-path or canonical stdlib
    helper targets (instead of same-path-only `/soa_vector/<helper>` lookup),
    so canonical helper imports no longer mis-route those probes into
    field-view pending fallback diagnostics
    (`src/semantics/SemanticsValidatorBuildInitializerInference.cpp`,
    `src/semantics/SemanticsValidatorInferMethodResolution.cpp`, and
    `src/semantics/SemanticsValidatorExprMethodTargetResolution.cpp`).
  - SoA field-view rewrite visibility collection now includes canonical
    `/std/collections/soa_vector/*` helpers and aliases them onto
    `/soa_vector/*` lookup keys, so canonical helper imports no longer trip
    same-path-only fallback rewrites in both field-view rewrite passes
    (`src/semantics/SemanticsValidate.cpp`).
  - Experimental SoA same-path helper method rewrite now probes direct name
    receivers independent of same-path helper visibility, then selects
    same-path versus canonical helper targets by visibility, so canonical-only
    helper visibility no longer skips method rewrite for direct experimental
    SoA receiver bindings (`src/semantics/SemanticsValidate.cpp`).
  - Experimental SoA same-path helper method rewrite traversal now excludes
    canonical `/std/collections/soa_vector/*` helper definitions (alongside
    `/soa_vector/*` and experimental helper-module definitions), so the
    compatibility rewrite pass no longer traverses canonical stdlib helper
    bodies as fallback candidates (`src/semantics/SemanticsValidate.cpp`).
  - Experimental SoA `to_aos` method rewrite helper-target selection now
    detects visible root `/to_aos` and canonical
    `/std/collections/soa_vector/to_aos` surfaces (definitions/imports,
    including specialized defs) before falling back to
    `/std/collections/experimental_soa_vector_conversions/soaVectorToAos`, so
    canonical helper visibility no longer routes this rewrite family through
    the experimental fallback path (`src/semantics/SemanticsValidate.cpp`).
  - Experimental SoA `to_aos` method rewrite traversal now excludes root and
    canonical `to_aos` helper definitions plus
    `/std/collections/experimental_soa_vector_conversions/*`, so the pass no
    longer traverses helper-module bodies as rewrite candidates
    (`src/semantics/SemanticsValidate.cpp`).
  - SoA mutation-borrow receiver fallback detection now treats `ref_ref`
    helper call/method shapes as equivalent to `ref` across same-path,
    canonical stdlib, and experimental helper surfaces, so canonical
    `/std/collections/soa_vector/ref_ref` rewrites no longer miss mutable
    borrowed-receiver detection during assign-target validation
    (`src/semantics/SemanticsValidatorExprMutationBorrows.cpp`).
  - SoA late fallback return-kind helper-target selection for old-surface
    `get`/`ref` receiver probes now resolves through visible helper targets
    (same-path or canonical stdlib) instead of hardcoding
    `/soa_vector/<helper>`, so canonical helper imports satisfy this fallback
    path without relying on compiler-owned same-path assumptions
    (`src/semantics/SemanticsValidatorInferLateFallbackBuiltins.cpp`).
  - Late unknown-target vector-family fallback matching now treats canonical
    `/std/collections/soa_vector/*` helper targets as vector-family-equivalent
    (alongside same-path `/soa_vector/*`), so canonical SoA helper resolution
    no longer falls through this compatibility path when method fallback
    rewrites probe vector-family helper targets
    (`src/semantics/SemanticsValidatorExprLateUnknownTargetFallbacks.cpp`).
  - Resolved-call SoA borrowed-reference escape fallback detection now treats
    `ref_ref` helper call/method shapes as equivalent to `ref` across
    same-path, canonical stdlib, and experimental helper surfaces, so
    canonical `ref_ref` rewrites stay on the shared reference-escape
    validation path instead of relying on compiler-owned `ref`-only matching
    (`src/semantics/SemanticsValidatorExprResolvedCallArguments.cpp`).
  - Old-surface SoA borrowed-view pending fallback gates now treat `ref_ref`
    helper call/method shapes as equivalent to `ref` across same-path and
    canonical stdlib helper surfaces (including helper-visibility-aware pending
    diagnostics, method compatibility diagnostics, pre-dispatch and late-fallback
    unknown-method diagnostics, and old-surface helper fallback escape hatches
    in type inference), so canonical `ref_ref` helper visibility no longer
    falls back to compiler-owned `ref`-only pending/unknown-method handling
    (`src/semantics/SemanticsBuiltinPathHelpers.cpp`,
    `src/semantics/SemanticsValidatorBuildInitializerInference.cpp`,
    `src/semantics/SemanticsValidatorExprMethodCompatibilitySetup.cpp`,
    `src/semantics/SemanticsValidatorInferPreDispatchCalls.cpp`,
    `src/semantics/SemanticsValidatorInferLateFallbackBuiltins.cpp`,
    `src/semantics/SemanticsValidatorExprMapSoaBuiltins.cpp`, and
    `src/semantics/SemanticsValidatorInferCollectionReturnInference.cpp`).
  - Template-monomorph SoA borrowed-view pending fallback gates now treat
    `ref_ref` helper call/method shapes as equivalent to `ref` across
    same-path and canonical helper visibility checks (including method-call
    pending diagnostics, old-surface fallback escape hatches, and direct
    pending-diagnostic path selection), so canonical `ref_ref` helper
    visibility no longer falls back to compiler-owned `ref`-only pending
    handling during implicit template inference
    (`src/semantics/TemplateMonomorphImplicitTemplateInference.h` and
    `src/semantics/TemplateMonomorphFallbackTypeInference.h`).
  - Template-monomorph SoA expression-rewrite helper-target fallback selection
    now treats `ref_ref` helper call/method shapes as equivalent to `ref`
    across same-path helper-family matching (including synthetic same-path
    template-carry path guards and old-surface helper-target family
    selection), so canonical `ref_ref` helper visibility no longer falls back
    to compiler-owned `ref`-only path handling during expression rewrite
    target selection (`src/semantics/TemplateMonomorphExpressionRewrite.h`).
  - SoA method-target and late fallback return-kind resolution now treat
    `ref_ref` method/helper shapes as equivalent to `ref` across
    validator/inference same-path and canonical helper-target selection
    (including canonical method-target builtin fallback classification,
    collection-type-path method resolution, and late fallback receiver-probe
    helper retargeting), so canonical `ref_ref` helper visibility no longer
    falls back to compiler-owned `ref`-only method/return-kind target handling
    (`src/semantics/SemanticsValidatorExprMethodTargetResolution.cpp`,
    `src/semantics/SemanticsValidatorInferMethodResolution.cpp`, and
    `src/semantics/SemanticsValidatorInferLateFallbackBuiltins.cpp`).
  - SoA access-rewrite plus bridge-path snapshot helper-family handling now
    includes borrowed helper surfaces (including `ref_ref` rewrite-preserve
    gating for old-surface access rewrite and `_ref` bridge-helper
    classification for SoA bridge snapshots), so canonical `ref_ref`/`*_ref`
    helper visibility no longer falls back to compiler-owned `ref`-only
    handling in these rewrite/snapshot compatibility paths
    (`src/semantics/SemanticsValidate.cpp` and
    `src/semantics/SemanticsValidatorSnapshots.cpp`).
  - SoA `to_aos` conversion fallback vector-target probing now includes
    borrowed helper surfaces (`to_aos_ref`) across method-target and
    collection-dispatch resolver probing (including root/same-path/canonical
    borrowed helper call-shape matching and canonical borrowed helper builtin
    fallback classification), so canonical `to_aos_ref` helper visibility no
    longer falls back to compiler-owned `to_aos`-only conversion probing in
    these compatibility paths
    (`src/semantics/SemanticsValidatorExprMethodTargetResolution.cpp` and
    `src/semantics/SemanticsValidatorInferCollections.cpp`).
  - SoA conversion builtin/fallback classification now includes borrowed
    `to_aos_ref` helper surfaces across late map/collection dispatch,
    diagnostics, named-argument builtin guards, and field-view helper-inference
    exclusion lists, so canonical/same-path `to_aos_ref` visibility no longer
    falls back to compiler-owned `to_aos`-only classification in these
    compatibility dispatch/diagnostic paths
    (`src/semantics/SemanticsValidatorExprMapSoaBuiltins.cpp`,
    `src/semantics/SemanticsValidatorInferLateFallbackBuiltins.cpp`,
    `src/semantics/SemanticsValidatorExprCollectionAccess.cpp`,
    `src/semantics/SemanticsValidatorExprNamedArgumentBuiltins.cpp`,
    `src/semantics/SemanticsValidatorPassesDiagnostics.cpp`,
    `src/semantics/SemanticsValidatorExecutionDiagnostics.cpp`,
    `src/semantics/SemanticsValidatorInferDefinition.cpp`,
    `src/semantics/SemanticsValidatorBuildInitializerInference.cpp`, and
    `src/semantics/SemanticsBuiltinPathHelpers.cpp`).
  - Template-monomorph SoA conversion method-target selection now includes
    borrowed `to_aos_ref` helper-target parity for explicit `soa_vector`
    receiver dispatch, so canonical/same-path `to_aos_ref` visibility no
    longer falls back to compiler-owned hardcoded `/soa_vector/to_aos_ref`
    target selection during template method-target resolution
    (`src/semantics/TemplateMonomorphMethodTargets.h`).
  - Validator and inference SoA conversion method-target selection now
    includes borrowed `to_aos_ref` helper-target parity across
    collection-type-path, direct receiver, and normalized-type dispatch, so
    canonical/same-path `to_aos_ref` visibility no longer falls back to
    compiler-owned `to_aos`-only target selection during non-template method
    resolution (`src/semantics/SemanticsValidatorInferMethodResolution.cpp`
    and `src/semantics/SemanticsValidatorExprMethodTargetResolution.cpp`).
  - Remaining SoA conversion helper-name fallback classification now includes
    borrowed `to_aos_ref` parity in template pending helper gating and
    experimental field-view helper-name exclusions, so canonical/same-path
    `to_aos_ref` visibility no longer falls back to compiler-owned
    `to_aos`-only helper-name classification during implicit-template pending
    diagnostics and field-view rewrite fallback checks
    (`src/semantics/TemplateMonomorphImplicitTemplateInference.h` and
    `src/semantics/SemanticsValidate.cpp`).

IR-lowerer/runtime special-case boundaries:
- Non-empty `soa_vector` literal lowering now routes through:
  - `src/ir_lowerer/IrLowererOperatorCollectionMutationHelpers.cpp`
    (direct struct-element literal materialization with heap-backed payload
    slots and explicit struct-slot copy into contiguous SoA-compatible storage)
- Arbitrary-width pending width checks now route through the stdlib helper trap
  path instead of compiler-owned lowerer/runtime special-casing:
  - `stdlib/std/collections/experimental_soa_storage.prime`
    (`soaSupportedFieldCount<T>()` -> `soaArbitraryWidthPending<T>(...)`)
  - Lowerer/runtime no longer contain a dedicated
    `soaArbitraryWidthPending` special-case branch or runtime-error emitter.

Stdlib surfaces already acting as the canonical SoA bridge:
- `stdlib/std/collections/soa_vector.prime`
- `stdlib/std/collections/experimental_soa_vector.prime`
- `stdlib/std/collections/experimental_soa_vector_conversions.prime`
