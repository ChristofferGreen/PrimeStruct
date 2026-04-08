# Ownership/Runtime + SoA Touchpoints

This file inventories the concrete code touchpoints for the live Group 13/14
TODO queues in `docs/todo.md`.

## 1) Vector constructor migration touchpoints

Goal reference: move canonical imported vector constructor usage from fixed-arity
helpers to variadic `vector(values...)`.

- Stdlib fixed-arity constructor overloads currently live in
  `stdlib/std/collections/vector.prime` as
  `/std/collections/vector/vector<T>(...)` arity-specific overloads.
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

- Stdlib fixed-arity constructor overloads currently live in
  `stdlib/std/collections/map.prime` as
  `/std/collections/map/map<K, V>(...)` key/value arity-specific overloads.
- Fixed-arity compatibility wrappers still exist in the same file:
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
  `emitBuiltinCanonicalMapInsertOverwriteOrPending(...)` in
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
- Remaining non-local lvalue receiver shapes that do not currently route through
  that same path (for example non-field-access receiver forms that still bypass
  canonical/helper alias receiver-typing inference before rewrite).
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

IR-lowerer/runtime special-case boundaries:
- Non-empty literal rejection remains explicit in:
  - `src/ir_lowerer/IrLowererOperatorCollectionMutationHelpers.cpp`
    (`native backend does not support non-empty soa_vector literals`)
- Arbitrary-width pending helper path still exists in:
  - `src/ir_lowerer/IrLowererLowerInlineCalls.h`
    (`/std/collections/experimental_soa_storage/soaArbitraryWidthPending`)
  - `src/ir_lowerer/IrLowererRuntimeErrorHelpers.cpp`
    (`experimental soa storage arbitrary-width schemas pending`)

Stdlib surfaces already acting as the canonical SoA bridge:
- `stdlib/std/collections/soa_vector.prime`
- `stdlib/std/collections/experimental_soa_vector.prime`
- `stdlib/std/collections/experimental_soa_vector_conversions.prime`
