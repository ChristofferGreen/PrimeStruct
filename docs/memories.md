# Repository Memories

This file stores durable session-derived facts that are useful in later work. Keep it short, factual, and easy to diff.

## Active Memories

### compile-run-tests-shell-out-to-build-binaries
- Updated: 2026-04-28
- Tags: tests, build, tooling
- Fact: Compile-run suites invoke `./primec` and `./primevm` from the active build directory, so focused reruns need fresh standalone tool binaries and should use release-build helpers such as `scripts/rerun_backend_shard.sh`.
- Evidence: `tests/unit/test_compile_run_smoke_core_basic.cpp` and many other compile-run suites build shell commands like `./primec --emit=...` and `./primevm ...`; `scripts/rerun_backend_shard.sh vm-math` prints the required `build-release/` cwd, CTest regex, and `PrimeStruct_compile_run_tests` command.

### cpp-emitter-wrapper-map-direct-count-diagnostics
- Updated: 2026-04-20
- Tags: tests, emitters, collections
- Fact: In the C++ emitter, wrapper-returned canonical map indexing and non-imported direct reference count flows reject with `unknown call target: /std/collections/map/at`, imported wrapper-reference count falls through to `unknown call target: /map/at`, and direct custom `/std/collections/map/at(wrapMap(), ...)` count calls fail later with `EXE IR lowering error: debug: branch=inferExprString`.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_emitters_explicit_vector_mutator_statement_helpers.cpp` plus direct `./primec --emit=exe` reproductions against those wrapper-map fixtures.

### cpp-emitter-vector-mutator-shadow-precedence
- Updated: 2026-04-20
- Tags: tests, emitters, collections
- Fact: In the C++ emitter, imported stdlib vector mutators outrank rooted `/vector/*` shadows for direct and named calls, while positional `push(value, values)` shadow calls still reject during EXE lowering.
- Evidence: `tests/unit/test_compile_run_emitters_lambda_mutator_resolution.cpp` now passes focused release reruns that return `0` for the full mutator matrix, `3` for the named-call case, and `push requires mutable vector binding` for the positional shadow reject.

### exact-stdlib-vector-import-covers-helper-surface
- Updated: 2026-04-19
- Tags: semantics, imports, collections
- Fact: Exact `import /std/collections/vector` is special-cased to expose the bare `vector(...)` alias and to treat `/std/collections/vector/*` helper paths as imported.
- Evidence: `src/semantics/SemanticsValidatorBuildImports.cpp`, `src/semantics/SemanticsValidatorInferCollectionCompatibility.cpp`, and `src/semantics/TemplateMonomorphCoreUtilities.h` all special-case that import path, and the behavior is covered in `tests/unit/test_compile_run_imports_operations.cpp` and `tests/unit/test_semantics_calls_and_flow_collections_bare_map_call_form_statement_args.cpp`.

### fileerror-why-callbacks-preserve-empty-state
- Updated: 2026-04-28
- Tags: ir, native, diagnostics
- Fact: Native `FileError.why` and `Result.why(FileError)` lowering must forward the FileError why emitter as a possibly-empty callback instead of wrapping it in an unconditional lambda.
- Evidence: Known native image compile-run failures aborted with `std::bad_function_call`; `IrLowererLowerEmitExpr.h`, `IrLowererResultWhyHelpers.cpp`, and `IrLowererRuntimeErrorHelpers.cpp` now preserve empty callbacks and report `FileError.why emitter is unavailable` through focused helper tests.

### map-compatibility-aliases-require-source-definitions
- Updated: 2026-05-01
- Tags: semantics, collections, compatibility
- Fact: Removed map aliases such as `/map/count` should count as available only when an explicit source definition family exists, not when template monomorphization has generated a `__...` specialization.
- Evidence: Field-bound `Map<K, V>` compatibility triage showed generated map helper specializations could mask missing `/map/count` aliases unless removed-alias checks ignored generated-only definition paths.

### maybe-method-helpers-use-rooted-families
- Updated: 2026-05-01
- Tags: semantics, stdlib, sums
- Fact: Imported stdlib `Maybe<T>` method calls resolve through rooted helper families such as `/Maybe/is_empty` and `/Maybe/isSome` even when the receiver type is monomorphized under `/std/maybe/Maybe__...`.
- Evidence: The saved release log rejected `empty.is_empty()` and `value.isSome()` with unknown call targets until semantic validation and template monomorphization fell back from missing same-path helpers to the rooted `Maybe` helper family.

### native-vector-auto-inference-expression-blockers
- Updated: 2026-04-20
- Tags: tests, native, collections
- Fact: In the native compile-run path, expression-form named `push` receiver precedence cases now fail in native lowering with the generic “calls in expressions” diagnostic, while the std namespaced auto-inference/count/capacity alias-precedence rejects still fail semantically but now say `return type mismatch: expected i32`.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_native_backend_collections_auto_inferred_helper_precedence.cpp` plus direct `./primec --emit=native` reproductions against the same fixtures.

### namespaced-indexed-writes-use-access-aliases
- Updated: 2026-04-28
- Tags: semantics, ir, collections
- Fact: Namespaced indexed writes can be validated or lowered after access calls have been canonicalized to stdlib legacy helper paths such as `/std/collections/vectorAt__...`, so semantic, IR-lowerer, and emitter array-access classifiers must all recognize those aliases.
- Evidence: `./primec --emit=ir` on a `/std/image` helper with `values[0i32] = 9i32` reproduced the original semantic rejection until `getBuiltinArrayAccessName` in semantics accepted `vectorAt` aliases; the follow-on IR lowering repro required the matching alias support in `IrLowererBuiltinNameHelpers.cpp` and `EmitterBuiltinCallPathHelpers.cpp`.

### result-ok-arithmetic-payloads-are-syntax-owned
- Updated: 2026-05-01
- Tags: ir, semantics, results
- Fact: Direct `Result.ok(...)` arithmetic payload calls such as `multiply(...)` are syntax-owned builtin payloads and must not require semantic-product query metadata before normal expression-kind inference can classify them.
- Evidence: The saved release log showed `missing semantic-product Result.ok payload metadata: multiply` and unsupported auto-bound `Result.and_then` payloads; `IrLowererPackedResultHelpers.cpp` and `IrLowererResultMetadataHelpers.cpp` now exempt builtin operators from semantic query requirements alongside builtin comparisons.

### semantic-product-pick-target-query-facts
- Updated: 2026-05-01
- Tags: semantics, ir, sums
- Fact: Sum `pick(...)` targets that are direct or method calls must publish semantic-product query facts because native/VM pick lowering resolves the target sum type from those facts.
- Evidence: The saved release log failed `native pick call target sum resolution uses query facts` and its method-target companion; `SemanticsValidatorSnapshotLocals.cpp` now adds a deduped recovery pass for `pick` target calls, with regression coverage in `test_semantics_type_resolution_graph_snapshots.cpp`.

### semantic-product-routing-completeness-gates-lowering
- Updated: 2026-05-01
- Tags: semantics, ir, routing
- Fact: IR lowering entry setup treats semantic-product direct-call, bridge-path, and method-call routing facts as required completeness gates before any fallback lowering path can run.
- Evidence: The saved release log showed `IrLowerer::lower` accepting missing direct-call and bridge-path facts; `IrLowererLowerEntrySetup.cpp` now includes routing.direct-call, routing.bridge-path, and routing.method-call in the semantic-product completeness manifest.

### semantic-product-sums-omit-callable-summaries
- Updated: 2026-05-01
- Tags: semantics, ir, sums
- Fact: Semantic-product sum definitions publish type metadata but not callable summaries, so lowerer setup code must treat sums as type metadata instead of requiring callable-summary facts.
- Evidence: The saved release log showed `missing semantic-product callable summary: /Choice`; `SemanticsValidatorSnapshots.cpp` skips sum definitions when collecting callable summaries, and `IrLowererOnErrorHelpers.cpp`, `IrLowererLowerEffects.cpp`, and `IrLowererLowerInferenceReturnInfoHelpers.cpp` now skip or short-circuit semantic-product sum metadata before requiring callable summaries.

### skipped-doctest-queue-locks-current-docs-and-source-surfaces
- Updated: 2026-04-20
- Tags: tests, docs, todo
- Fact: `tests/unit/test_compile_run_examples_docs_locks.cpp` source-locks both the skipped-debt queue shape in `docs/todo*.md` and the current active `vm_math` and `vm_maps` surfaces, so resolving skipped-doctest lanes requires updating the queue docs and that lock file together.
- Evidence: Refreshing the `examples_docs_locks` shard after landing the VM math/map skip cleanups required retiring `TODO-4110`, `TODO-4117`, and `TODO-4118` from `docs/todo.md`, archiving them in `docs/todo_finished.md`, and updating the queue/surface assertions in `tests/unit/test_compile_run_examples_docs_locks.cpp` before the focused release rerun passed.

### soa-storage-temporaries-own-nested-buffers
- Updated: 2026-04-28
- Tags: ir, native, collections
- Fact: `SoaColumn`, generated `SoaColumnsN`, and `SoaVector` temporary copies need native disarm logic for nested `ownsData` fields so copied heap buffers are not freed through the source temporary.
- Evidence: `stdlib/std/collections/internal_soa_storage.prime` stores `ownsData` inside each `SoaColumn`, and `IrLowererFlowControlHelpers.cpp` now disarms `SoaColumn`, `SoaColumnsN`, and `SoaVector` temporary copies at their computed `ownsData` slot offsets.

### stdlib-vector-capacity-direct-calls-keep-helper
- Updated: 2026-05-01
- Tags: semantics, collections, emitters
- Fact: Explicit `/std/collections/vector/capacity(...)` direct calls with a declared same-path helper must remain non-builtin semantic targets so C++ emission calls the helper instead of falling through to `ps_vector_capacity`.
- Evidence: The saved release log showed a same-path helper returning `15` still producing runtime capacity `3`; `SemanticsValidatorExprCollectionCountCapacity.cpp` now keeps declared canonical capacity helpers as non-builtin targets, with focused semantics coverage beside the stdlib vector count cases.

### struct-brace-constructors-recover-callees-by-type
- Updated: 2026-05-01
- Tags: ir, structs, lowering
- Fact: Statement binding lowering can infer a brace constructor's struct path even when direct call resolution misses the constructor, so it must recover the struct definition by inferred path before falling back to generic struct copy lowering.
- Evidence: The saved release log failed `ir lowers struct brace constructor binding`; `IrLowererLowerStatementsBindings.h` now looks up inferred struct constructor paths in `defMap` and inline-lowers them, and the serialization test executes the lowered module to confirm the named-field default path returns `3`.

### vm-vector-shadow-precedence-and-expression-blockers
- Updated: 2026-04-20
- Tags: tests, vm, collections
- Fact: In the VM compile-run path, imported stdlib vector `push` still wins over rooted `/vector/push` shadows for named-call and method syntax, but `push(...)` expression forms reject during VM lowering, auto-inferred named access helpers fail with `missing semantic-product local-auto fact`, and auto-inferred std namespaced access/push alias cases fail earlier with `return type mismatch: expected i32`.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_vm_collections_vector_shadow_access.cpp` plus direct `./primec --emit=vm` reproductions against the same fixtures.

### vm-canonical-map-helper-failure-modes
- Updated: 2026-04-25
- Tags: tests, vm, collections
- Fact: In the VM path, explicit canonical map helper overrides can still reach runtime and fail with `VM error: unaligned indirect address in IR`, while imported canonical map-reference helper calls fail earlier in VM lowering with `call=/std/collections/map/at`.
- Evidence: Focused release reruns of `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_51_60` plus direct `./primec --emit=vm` reproductions of `vm_direct_canonical_map_helper_same_path_precedence.prime` and `vm_stdlib_map_reference_helpers.prime`.

### vm-map-helper-calls-currently-reject-without-canonical-support
- Updated: 2026-04-20
- Tags: tests, vm, collections
- Fact: The VM compile-run path currently rejects map indexing and direct `at`/`at_unsafe` helper use for numeric, bool, and `u64` map keys with `unknown call target: /std/collections/map/at` rather than executing those helper forms.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_vm_maps.cpp` now lock those rejects in `tests/unit/test_compile_run_vm_maps.cpp`.

### vm-math-support-matrix-lowering-split
- Updated: 2026-04-20
- Tags: tests, vm, math
- Fact: In the VM compile-run math shard, nominal support-matrix helpers execute successfully, but quaternion-reference and matrix-composition reference flows still fail in VM lowering through `/std/math/quat_multiply_internal` and `/std/math/mat3_mul_vec3_internal`, while the broad matrix and quaternion arithmetic tolerance tests intentionally accept either success or the current unsupported-call fallback.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_vm_math.cpp --no-skip --duration=true` plus direct `./primec --emit=vm` reproductions against the rewritten fixtures.

### vm-vector-limits-growth-and-expression-blockers
- Updated: 2026-04-20
- Tags: tests, vm, collections
- Fact: In the VM compile-run path, reserve/push growth helpers and the exact 256-element local vector literal now execute successfully, but expression-form `reserve`, `remove_at`, and `remove_swap` user shadows still reject with the VM “calls in expressions” lowering diagnostic.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_vm_collections_vector_limits_a.cpp` plus direct `./primec --emit=vm` reproductions against those fixtures.

### source-lock-tests-read-private-semantics-fragments-directly
- Updated: 2026-04-27
- Tags: tests, semantics, source-lock
- Fact: `docs/source_lock_inventory.md` is the canonical inventory for source-lock tests that read private `src/` files, private headers, public headers as architecture proxies, or checked-in docs/examples, including the existing private semantics fragment locks.
- Evidence: `tests/unit/test_ir_pipeline_validation_fragments/test_ir_pipeline_validation_semantics_expr_source_delegation_01.h` explicitly loads `SemanticsValidatorPrivateExprValidation.h` and many sibling fragments with `readText(...)`; `docs/source_lock_inventory.md` classifies the wider source-lock surface and records intended replacement contracts.

### sum-variant-envelopes-are-line-sensitive
- Updated: 2026-04-29
- Tags: parser, text-filter, sums
- Fact: A bracketed sum payload envelope on the line after a bare unit variant must stay a separate variant declaration instead of being rewritten or parsed as postfix indexing on the previous name.
- Evidence: Release triage showed `Maybe<T> { none\n  [T] some }` becoming `at(none, T)` before `TextFilterPipelineOperatorRewrite` and `ParserExpr` learned to stop cross-line bracket chaining.

### text-filter-collect-diagnostics-locks-stdlib-fallback-messages
- Updated: 2026-04-20
- Tags: tests, diagnostics, text-filter
- Fact: The wrapper-temporary collect-diagnostics coverage in `tests/unit/test_compile_run_text_filters_diagnostics_c.cpp` currently locks the stdlib fallback messages `unknown call target: /std/collections/map/at` and `unknown method: /vector/capacity` rather than the older user-helper arg-mismatch wording.
- Evidence: Release reruns from `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_text_filters_diagnostics_c.cpp` plus direct `./primec --emit-diagnostics --collect-diagnostics` reproductions against the same sources emitted those messages.

### variadic-borrowed-pointer-packs-are-supported
- Updated: 2026-05-01
- Tags: tests, ir, native, variadic
- Fact: Direct struct and Result packs plus borrowed and pointer variadic arg-pack access for scalar, array, struct, nested struct-field, uninitialized, Result, and FileError surfaces are supported materialized behavior, so regression tests should assert direct value results instead of older rejection diagnostics or raw-pointer-shaped numeric results.
- Evidence: Saved release logs showed current IR/native lowering accepting these pack forms and returning direct sums such as `2`, `3`, `15`, `17`, `23`, `24`, `27`, `36`, `39`, `65`, `72`, and `75` while older tests still expected `variadic parameter type mismatch`, `struct field type mismatch`, `unknown struct field: value`, or stale large numeric constants.

### vector-args-pack-elements-are-handles
- Updated: 2026-04-28
- Tags: ir, collections, variadic
- Fact: `args<vector<T>>` pack elements are single-slot vector handles, not inline vector struct payloads, so indexed access and binding initialization must not classify them as inline struct args-pack storage.
- Evidence: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_collection_refs` failed until `IrLowererAccessTargetResolution`, `IrLowererIndexedAccessEmit`, and statement binding initialization all preserved vector args-pack handle semantics.

## Maintenance Notes

- Keep entries sorted by slug within the section.
- Delete wrong entries instead of leaving contradictory facts behind.
- Prefer updating an existing entry over adding a near-duplicate.
- Avoid copying obvious facts from `AGENTS.md` or canonical design docs unless the shorter memory adds unique operational value.
