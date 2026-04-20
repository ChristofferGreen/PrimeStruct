# Repository Memories

This file stores durable session-derived facts that are useful in later work. Keep it short, factual, and easy to diff.

## Active Memories

### compile-run-tests-shell-out-to-build-binaries
- Updated: 2026-04-19
- Tags: tests, build, tooling
- Fact: Compile-run suites invoke `./primec` and `./primevm` from the active build directory, so focused reruns need fresh standalone tool binaries and not just freshly linked test binaries.
- Evidence: `tests/unit/test_compile_run_smoke_core_basic.cpp` and many other compile-run suites build shell commands like `./primec --emit=...` and `./primevm ...`.

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

### native-vector-auto-inference-expression-blockers
- Updated: 2026-04-20
- Tags: tests, native, collections
- Fact: In the native compile-run path, expression-form named `push` receiver precedence cases now fail in native lowering with the generic “calls in expressions” diagnostic, while the std namespaced auto-inference/count/capacity alias-precedence rejects still fail semantically but now say `return type mismatch: expected i32`.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_native_backend_collections_auto_inferred_helper_precedence.cpp` plus direct `./primec --emit=native` reproductions against the same fixtures.

### vm-vector-shadow-precedence-and-expression-blockers
- Updated: 2026-04-20
- Tags: tests, vm, collections
- Fact: In the VM compile-run path, imported stdlib vector `push` still wins over rooted `/vector/push` shadows for named-call and method syntax, but `push(...)` expression forms reject during VM lowering, auto-inferred named access helpers fail with `missing semantic-product local-auto fact`, and auto-inferred std namespaced access/push alias cases fail earlier with `return type mismatch: expected i32`.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_vm_collections_vector_shadow_access.cpp` plus direct `./primec --emit=vm` reproductions against the same fixtures.

### vm-map-helper-calls-currently-reject-without-canonical-support
- Updated: 2026-04-20
- Tags: tests, vm, collections
- Fact: The VM compile-run path currently rejects map indexing and direct `at`/`at_unsafe` helper use for numeric, bool, and `u64` map keys with `unknown call target: /std/collections/map/at` rather than executing those helper forms.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_vm_maps.cpp` now lock those rejects in `tests/unit/test_compile_run_vm_maps.cpp`.

### vm-vector-limits-growth-and-expression-blockers
- Updated: 2026-04-20
- Tags: tests, vm, collections
- Fact: In the VM compile-run path, reserve/push growth helpers and the exact 256-element local vector literal now execute successfully, but expression-form `reserve`, `remove_at`, and `remove_swap` user shadows still reject with the VM “calls in expressions” lowering diagnostic.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_vm_collections_vector_limits_a.cpp` plus direct `./primec --emit=vm` reproductions against those fixtures.

### source-lock-tests-read-private-semantics-fragments-directly
- Updated: 2026-04-19
- Tags: tests, semantics, source-lock
- Fact: Some source-lock IR and semantics tests read individual `src/semantics/*.h` and `*.cpp` fragments directly, including private headers, so moving logic between fragments requires updating those file-path assertions instead of assuming the assembled include surface is enough.
- Evidence: `tests/unit/test_ir_pipeline_validation_fragments/test_ir_pipeline_validation_semantics_expr_source_delegation_01.h` explicitly loads `SemanticsValidatorPrivateExprValidation.h` and many sibling fragments with `readText(...)`.

### text-filter-collect-diagnostics-locks-stdlib-fallback-messages
- Updated: 2026-04-20
- Tags: tests, diagnostics, text-filter
- Fact: The wrapper-temporary collect-diagnostics coverage in `tests/unit/test_compile_run_text_filters_diagnostics_c.cpp` currently locks the stdlib fallback messages `unknown call target: /std/collections/map/at` and `unknown method: /vector/capacity` rather than the older user-helper arg-mismatch wording.
- Evidence: Release reruns from `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_text_filters_diagnostics_c.cpp` plus direct `./primec --emit-diagnostics --collect-diagnostics` reproductions against the same sources emitted those messages.

## Maintenance Notes

- Keep entries sorted by slug within the section.
- Delete wrong entries instead of leaving contradictory facts behind.
- Prefer updating an existing entry over adding a near-duplicate.
- Avoid copying obvious facts from `AGENTS.md` or canonical design docs unless the shorter memory adds unique operational value.
