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
- Remaining non-local lvalue receiver shapes that do not currently route
  through that same path (for example non-local receiver forms beyond
  field-access, helper-return, and args-pack map-access receiver-source `at` /
  `at_unsafe` / `at_ref` / `at_unsafe_ref` + compatibility-alias path/stem
  call/method chains (including PascalCase/PascalCase `_ref` and
  generated-suffix variants)).
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
