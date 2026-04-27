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

### native-vector-auto-inference-expression-blockers
- Updated: 2026-04-20
- Tags: tests, native, collections
- Fact: In the native compile-run path, expression-form named `push` receiver precedence cases now fail in native lowering with the generic “calls in expressions” diagnostic, while the std namespaced auto-inference/count/capacity alias-precedence rejects still fail semantically but now say `return type mismatch: expected i32`.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_native_backend_collections_auto_inferred_helper_precedence.cpp` plus direct `./primec --emit=native` reproductions against the same fixtures.

### skipped-doctest-queue-locks-current-docs-and-source-surfaces
- Updated: 2026-04-20
- Tags: tests, docs, todo
- Fact: `tests/unit/test_compile_run_examples_docs_locks.cpp` source-locks both the skipped-debt queue shape in `docs/todo*.md` and the current active `vm_math` and `vm_maps` surfaces, so resolving skipped-doctest lanes requires updating the queue docs and that lock file together.
- Evidence: Refreshing the `examples_docs_locks` shard after landing the VM math/map skip cleanups required retiring `TODO-4110`, `TODO-4117`, and `TODO-4118` from `docs/todo.md`, archiving them in `docs/todo_finished.md`, and updating the queue/surface assertions in `tests/unit/test_compile_run_examples_docs_locks.cpp` before the focused release rerun passed.

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
