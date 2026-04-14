# PrimeStruct TODO Log

## Operating Rules (Human + AI)

1. This file contains only open work (`[ ]` or `[~]`).
2. Use one task block per item with a stable ID: `TODO-XXXX`.
3. Keep newest items at the top of `## Open Tasks`.
4. Every task must include:
   - clear scope
   - acceptance criteria
   - owner (`human` or `ai`)
5. When a task is completed:
   - mark it `[x]`
   - add `finished_at` and short evidence note
   - move the full block to `docs/todo_finished.md`
   - remove it from this file (do not keep completed tasks here)
6. `docs/todo_finished.md` is append-only history. Do not rewrite old entries except to fix factual mistakes.
7. Each implementation task SHOULD include `phase` and `depends_on` to keep execution order explicit.
8. Prefer small, testable tasks over broad epics; split before starting if acceptance cannot be verified in one PR.
9. Keep the `Execution Queue` and coverage snapshots current when adding/removing tasks.
10. Keep an explicit `Ready Now` shortlist synced with dependencies; only items with no unmet TODO dependencies belong there.
11. When splitting broad tasks, update parent task scope to avoid duplicated acceptance criteria across child tasks.
12. Keep `Priority Lanes` aligned with queue order so critical-path tasks remain visible.
13. For phase-level tracking tasks, pair planning trackers with explicit acceptance-gate tasks before marking a phase complete.
14. Every active leaf must include a stop rule and deliver at least one of:
   - user-visible behavior change
   - measurable perf/memory improvement
   - deletion of a real compatibility subsystem
15. Avoid standalone micro-cleanups (alias renames, trivial bool rewrites, local dedup) unless bundled into one value outcome above.
16. If a leaf misses its value target after 2 attempts, archive it as low-value and replace it with a different hotspot.
17. Keep the active queue short: no more than 8 live leaves at once.

Status legend:
- `[ ]` queued
- `[~]` in progress
- `[x]` completed (must be moved to `docs/todo_finished.md`)

Task template:

```md
- [ ] TODO-<id>: Short title
  - owner: ai|human
  - created_at: YYYY-MM-DD
  - phase: Group/Phase name (optional but recommended)
  - depends_on: TODO-XXXX, TODO-YYYY (optional but recommended)
  - scope: ...
  - acceptance:
    - ...
    - ...
  - stop_rule: ...
  - notes: optional
```

## Open Tasks

### Ready Now (No Unmet TODO Dependencies)

1. TODO-0407
2. TODO-0408
3. TODO-0401
4. TODO-0402
5. TODO-0405

### Immediate Next 10 (After Ready Now)

1. TODO-0409
2. TODO-0406
3. TODO-0403

### Priority Lanes (Current)

- P0 Test implementation locality cleanup (no `TEST_CASE` in include chunks): TODO-0407
- P1 Collection stdlib ownership cutover (`vector`, `map`, `soa_vector`): TODO-0408, TODO-0409
- P2 SoA canonicalization + semantic memory/perf + multithread substrate + semantic-product boundary hardening: TODO-0401, TODO-0402, TODO-0405, TODO-0406
- P3 Queue/snapshot governance: TODO-0403

### Execution Queue (Recommended)

Wave A (test implementation locality cleanup):
1. TODO-0407

Wave B (collection stdlib ownership cutover):
1. TODO-0408
2. TODO-0409

Wave C (SoA completion):
1. TODO-0401

Wave D (semantic memory/perf):
1. TODO-0402
2. TODO-0405
3. TODO-0406

Wave E (queue hygiene):
1. TODO-0403

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Test implementation locality and include-chunk retirement | TODO-0407 |
| Collection stdlib ownership cutover (`vector`, `map`, `soa_vector`) | TODO-0408, TODO-0409 |
| SoA bring-up and stdlib-authoritative `soa_vector` end-state cleanup | TODO-0401 |
| Semantic memory footprint and multithread compile substrate | TODO-0402 |
| Semantic-product contract/index boundary hardening | TODO-0405, TODO-0406 |
| TODO queue quality gates and dependency/coverage synchronization | TODO-0403 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Release gate (`./scripts/compile.sh --release`) discipline | TODO-0401, TODO-0402, TODO-0405, TODO-0406, TODO-0407, TODO-0408, TODO-0409 |
| Test source locality and include-layer guardrail verification | TODO-0407 |
| Collection conformance and alias-deletion checks (`vector`/`map`/`soa_vector`) | TODO-0408, TODO-0409 |
| Benchmark/runtime regression checks (`./scripts/benchmark.sh`) | TODO-0402 |
| Semantic-product contract/index and deterministic conformance checks | TODO-0405, TODO-0406 |
| TODO/open-vs-finished hygiene (`docs/todo.md` vs `docs/todo_finished.md`) | TODO-0403 |

### Task Blocks

- [ ] TODO-0409: Remove C++ name routing for `map` + `soa_vector`
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 14
  - depends_on: TODO-0408
  - scope: Complete stdlib-authoritative collection migration by routing `map` and `soa_vector` behavior through `.prime` definitions, then delete remaining production compiler/runtime C++ name/path alias logic for these collections in semantics/lowering (`src/` + `include/primec`, excluding tests/testing-only facades).
  - acceptance:
    - Production `src/` + `include/` collection routing no longer hardcodes `map`/`soa_vector` symbol/path aliases for normal call/access classification; dispatch is generic or intrinsic-driven.
    - Release-mode collection conformance/tests for `map` and `soa_vector` pass, and any changed diagnostics are updated via deterministic snapshots.
    - At least one real compatibility subsystem (legacy alias/canonicalization branch family) is deleted rather than renamed.
    - Focused release-mode suites pass for the migration surface: `./build-release/PrimeStruct_semantics_tests --test-suite=primestruct.semantics.calls_flow.collections`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.ir.pipeline.validation`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.vm.maps`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.vm.collections`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.native_backend.collections`, and `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.emitters.cpp`.
    - Production guardrail check confirms no hardcoded collection compatibility routing remains in semantics/lowerer/emitter code paths (for example via `rg` over `src/semantics`, `src/ir_lowerer`, and emitter sources), with any remaining mentions justified as canonical stdlib or test-only references.
    - Final release gate passes with `./scripts/compile.sh --release`.
  - stop_rule: If landing both collections together is too risky for one commit, split into TODO-0410 (`map`) and TODO-0411 (`soa_vector`) with independent acceptance before continuing.

- [~] TODO-0408: Make `vector` stdlib-authoritative and delete vector-specific C++ classifiers
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 14
  - scope: Move `vector` constructor/method/member behavior to stdlib `.prime` definitions and remove vector-specific production compiler/runtime C++ path/name checks in semantics plus IR lowerer classification/mutation helpers (`src/` + `include/primec`, excluding tests/testing-only facades).
  - acceptance:
    - `vector` semantics/IR behavior is resolved through standard symbol/type flow without dedicated vector-name branches for normal production call/access classification.
    - Release-mode vector semantics + compile-run suites pass after deleting vector-specific compatibility branches.
    - Removed code includes at least one explicit vector alias/canonical-path branch from lowerer/semantics helpers.
    - Focused release-mode suites pass for vector behavior and routing: `./build-release/PrimeStruct_semantics_tests --test-suite=primestruct.semantics.calls_flow.collections`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.ir.pipeline.validation`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.vm.collections`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.native_backend.collections`, and `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.emitters.cpp`.
    - Tests cover both canonical helper success paths and removed-alias rejection paths (unknown-target/unknown-method diagnostics) so deleted compatibility routing cannot silently regress.
    - Final release gate passes with `./scripts/compile.sh --release`.
  - stop_rule: If shared classifier rewrites destabilize `map`/`soa_vector`, isolate the generic mechanism in this leaf and defer remaining shared deletions to TODO-0409.
  - notes: This leaf should leave a reusable generic path that TODO-0409 can apply to `map` and `soa_vector`.
  - progress: Removed semantic method-resolution fallback from canonical stdlib vector helpers back to `/vector/*` alias paths in `SemanticsValidatorExprMethodResolution.cpp`, and added semantics coverage for canonical helper success plus explicit `/vector/count` method-call rejection.
  - progress: Removed method-call fallback that treated `/std/collections/vectorCount` and `/std/collections/vectorCapacity` alias helpers as valid providers for `values.count()` / `values.capacity()`, and added semantics coverage that alias-only helpers now reject with `unknown method` diagnostics.
  - progress: Extended semantics coverage to include canonical `values.capacity()` success and explicit `values./vector/capacity()` alias rejection when only canonical stdlib helpers exist.
  - progress: Extended semantics coverage to include canonical `values.at(...)` / `values.at_unsafe(...)` success and explicit `values./vector/at(...)` / `values./vector/at_unsafe(...)` alias rejection when only canonical stdlib helpers exist.
  - progress: Removed alias-dependent gating from vector method prevalidation so canonical `/std/collections/vector/at` and `/std/collections/vector/at_unsafe` integer-index diagnostics stay enforced even when `/vector/*` aliases exist, with coverage for alias-shadow scenarios.
  - progress: Removed explicit `values./vector/...` method-namespace compatibility for vector `count`/`capacity`/`at`/`at_unsafe` receivers and added coverage that those method forms now reject with `unknown method` even when `/vector/*` helpers are declared.
  - progress: Pruned `/vector/count` and `/vector/capacity` alias-path checks from explicit vector count/capacity method-path validation and added alias-declared rejection coverage for `values./vector/at(...)` and `values./vector/at_unsafe(...)`.
  - progress: Removed production wrapper-routing classifiers that treated `/std/collections/vectorCount` and `/std/collections/vectorCapacity` as direct vector builtins in semantics + lowerer helper-resolution/monomorph compatibility paths; direct canonical handling now stays on `/std/collections/vector/count` and `/std/collections/vector/capacity` (with experimental-vector canonical rewrites preserved).
  - progress: Removed the remaining `/std/collections/vectorXxx` wrapper-name routing for vector access/mutator helpers in production lowerer helper classification plus semantics compatibility/monomorph bridge tables, so canonical `/std/collections/vector/*` and experimental-vector helper paths are the only direct vector helper routes.
  - progress: Deleted the last semantics-only wrapper compatibility surface that accepted `/std/collections/vectorAt`/`/std/collections/vectorAtUnsafe`, and removed the monomorph exception that allowed templated `auto` parameters for `/std/collections/vectorXxx` wrapper definitions.
  - progress: Removed production `/vector/count` and `/vector/capacity` alias routing from lowerer vector helper alias resolution/receiver-probe classification and from semantics builtin count-capacity dispatch return-kind + method-call handling, including pre-dispatch canonical rewrite from `/vector/count` to `/std/collections/vector/count`.
  - progress: Removed remaining semantics dispatch/setup compatibility scaffolding that depended on `/vector/count` alias canonicalization, restricted late-call vector count/capacity compatibility checks to canonical `/std/collections/vector/*` paths, and switched unresolved `capacity()` builtin promotion to canonical `/std/collections/vector/capacity`.
  - progress: Removed remaining emitter + lowerer count/capacity alias hardcoding for `/vector/*` in direct-call type inference and removed-helper inline filtering; canonical `/std/collections/vector/count` and `/std/collections/vector/capacity` are now the only explicit vector count/capacity helper paths in those production classifiers.
  - progress: Pruned vector count/capacity `/vector/*` alias entries from template-monomorph compatibility mapping and semantic compatibility descriptor tables (including removed-helper descriptor list), and removed method-target compatibility guards that still treated `/vector/count` and `/vector/capacity` as distinct paths.
  - progress: Removed remaining `/vector/at` and `/vector/at_unsafe` alias routing from semantics direct-fallback/return-kind dispatch, emitter vector-access type inference/classification, lowerer explicit-vector-access classifiers, and template-compatibility alias tables so vector access helper routing is canonicalized on `/std/collections/vector/at` and `/std/collections/vector/at_unsafe`.
  - progress: Removed remaining `/vector/*` mutator alias mapping (`push`/`pop`/`reserve`/`clear`/`remove_at`/`remove_swap`) from experimental-vector helper canonicalization and vector compatibility descriptor aliases, and pruned emitter/lowerer direct-call alias filters so production mutator routing stays on canonical `/std/collections/vector/*` helper paths.
  - progress: Removed another cross-stage `/vector/*` compatibility subsystem by deleting alias candidate expansion/fallback from template-monomorph, pre-dispatch return-kind inference, struct-return path helpers, and emitter metadata candidate lookup; direct `/vector/*` paths now stay isolated instead of auto-bridging to canonical `/std/collections/vector/*`.
  - progress: Pruned additional `/vector/*` compatibility fallback from semantics method-target alias candidate expansion plus lowerer/emitter setup helper path preference (`preferVectorStdlibHelperPath`, setup collection candidate builders, and receiver-target method lookup), so canonical `/std/collections/vector/*` is preferred without back-routing to `/vector/*`.
  - progress: Removed another struct-return/type-inference compatibility family that cross-expanded `/vector/*` and `/std/collections/vector/*` in semantics + emitter collection helper candidate builders and return-inference method-path selection, keeping direct `/vector/*` lookup isolated while canonical stdlib vector paths remain authoritative.
  - progress: Canonicalized struct-return method candidate routing in semantics + lowerer and aligned emitter vector-access candidate pruning to `/std/collections/vector/*`, removing remaining fallback branches that still treated `/vector/*` as an automatic bridge in this struct-return inference path family.

- [ ] TODO-0407: Move test implementations from include chunks into `.cpp`
  - owner: ai
  - created_at: 2026-04-13
  - phase: Test architecture cleanup
  - scope: Execute file-by-file migration from include-defined tests to `.cpp`-defined tests, keeping headers for shared helpers only.
  - file_todos:
    - [x] `TODO-0407-F01` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_validation.cpp`; fix: replace the `test_ir_pipeline_validation_chunks/*.h` fanout with topic-named `.cpp` files and move all `TEST_CASE` bodies out of chunk/fragment headers.
    - [x] `TODO-0407-F02` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_semantics_calls_and_flow_collections.cpp`; fix: remove `test_semantics_calls_and_flow_collections_chunks/*.h` includes by moving those tests into topic `.cpp` files that share `test_semantics_helpers.h`.
    - [x] `TODO-0407-F03` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_emitters.cpp`; fix: keep cache/helper utilities local, but migrate `test_compile_run_emitters_chunks/*.h` test implementations into emitter-topic `.cpp` files.
    - [x] `TODO-0407-F04` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_vm_outputs.cpp`; fix: move `test_compile_run_vm_outputs_*.h` `TEST_CASE` bodies into `.cpp` files and keep any remaining headers helper-only.
    - [x] `TODO-0407-F05` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_vm_collections.cpp`; fix: migrate `test_compile_run_vm_collections_*.h` test implementations into `.cpp` files; retain only shared helper declarations in headers.
    - [x] `TODO-0407-F06` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_bindings_and_examples.cpp`; fix: move included example/docs test implementation headers into dedicated `.cpp` files by topic while keeping shared harness helpers reusable.
    - [x] `TODO-0407-F07` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_backends_registry.cpp`; fix: inline/move all test implementation currently housed in `test_ir_pipeline_backends_registry.h` into this `.cpp` and convert the header to helper declarations only (or delete it if no helpers remain).
    - [x] `TODO-0407-F08` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_vm_core.cpp`; fix: replace the `test_compile_run_vm_core_*.h` include fanout with dedicated topic `.cpp` tests and keep shared helpers in helper headers only.
    - [x] `TODO-0407-F09` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_native_backend_collections.cpp`; fix: move test implementation out of `test_compile_run_native_backend_collections_*.h` into topic `.cpp` files and keep Apple gating centralized.
    - [x] `TODO-0407-F10` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_native_backend_core.cpp`; fix: move test implementation out of `test_compile_run_native_backend_core_*.h` into topic `.cpp` files and keep Apple gating centralized.
    - [x] `TODO-0407-F11` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_imports.cpp`; fix: move tests from `test_compile_run_imports_operations.h` and sibling headers into `test_compile_run_imports_*.cpp` files while leaving helper-only headers.
    - [x] `TODO-0407-F12` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_semantics_calls_and_flow_effects.cpp`; fix: move tests from `test_semantics_calls_and_flow_effects_*.h` into topic `.cpp` files and keep only shared helper declarations in headers.
    - [x] `TODO-0407-F13` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_semantics_capabilities.cpp`; fix: move tests from `test_semantics_capabilities_*.h` into `.cpp` units and keep headers helper-only.
    - [x] `TODO-0407-F14` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_semantics_result_helpers.cpp`; fix: move tests from `test_semantics_result_helpers_*.h` into `.cpp` units and keep headers helper-only.
    - [x] `TODO-0407-F15` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_smoke.cpp`; fix: move tests from `test_compile_run_smoke_*.h` into `.cpp` units and keep headers helper-only.
    - [x] `TODO-0407-F16` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_math_conformance.cpp`; fix: move tests from `test_compile_run_math_conformance_*.h` into `.cpp` units grouped by topic and keep headers helper-only.
    - [x] `TODO-0407-F17` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_glsl.cpp`; fix: move tests from `test_compile_run_glsl_*.h` into `.cpp` units and keep headers helper-only.
    - [x] `TODO-0407-F18` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_text_filters.cpp`; fix: move tests from `test_compile_run_text_filters_*.h` into `.cpp` units and keep headers helper-only.
    - [x] `TODO-0407-F19` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_to_glsl.cpp`; fix: move tests from `test_ir_pipeline_to_glsl_*.h` into `.cpp` units and keep headers helper-only.
    - [x] `TODO-0407-F20` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_semantics_imports.cpp`; fix: move tests from `test_semantics_imports.h` into `.cpp` and keep header content helper-only.
    - [x] `TODO-0407-F21` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_semantics_bindings_pointers.cpp`; fix: move tests from `test_semantics_bindings_pointer_types.h` into `.cpp` and keep header content helper-only.
  - acceptance:
    - Every entry in `file_todos` is completed for migration in scope.
    - In migrated areas, test headers contain no doctest `TEST_CASE`/`TEST_SUITE` definitions; test logic lives in `.cpp` files.
    - Current wrapper hotspots (`test_ir_pipeline_validation`, `test_semantics_calls_and_flow_collections`, and `test_compile_run_emitters`) are converted to direct `.cpp` test implementations with helper-only includes.
    - Include-layer checks and release-mode test binaries pass without adding new `tests -> src` allowlist entries.
  - stop_rule: If one hotspot is too large for a safe commit, split by thematic slices (IR validation, semantics flow, emitter compile-run) and keep each slice independently verifiable.
  - notes: Baseline audit found `6667` header-defined test cases across `347` headers and `31` wrapper `.cpp` files with zero local `TEST_CASE` bodies.

- [ ] TODO-0406: Split production semantics APIs from testing snapshots
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 15
  - depends_on: TODO-0405
  - scope: Move snapshot-for-testing surface area out of production semantics interfaces and into explicit `include/primec/testing/*` facades so runtime/compiler consumers only depend on production contracts.
  - acceptance:
    - Production-facing semantics headers stop exposing snapshot-only structs/methods used exclusively by tests.
    - Existing semantics/IR snapshot tests compile and pass through testing facades without adding new `tests -> src` include-layer allowlist entries.
    - At least one snapshot-only compatibility path in production semantics code is deleted rather than relocated.
  - stop_rule: If removal breaks unrelated runtime/backend callsites, split into two leaves (facade introduction first, production API pruning second) before continuing.

- [~] TODO-0405: Introduce unified semantic-product index builder
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 15
  - depends_on: TODO-0404
  - scope: Replace ad-hoc lowerer semantic-product lookup-map construction with one typed `SemanticProductIndex` builder and shared lookup API for direct/method/bridge/binding/local-auto/query/try/on_error families.
  - file_todos:
    - [x] `TODO-0405-F01` files: `/Users/chrgre01/src/PrimeStruct/src/ir_lowerer/IrLowererSemanticProductTargetAdapters.{h,cpp}`, `/Users/chrgre01/src/PrimeStruct/include/primec/testing/ir_lowerer_helpers/IrLowererSemanticProductTargetAdapters.h`; fix: introduce a typed `SemanticProductIndex` plus `SemanticProductIndexBuilder`, wire adapter construction through that builder for direct/method/bridge/binding/local-auto/query/try/on_error family indices, and route semantic lookup helpers through shared index-backed lookup paths.
    - [x] `TODO-0405-F02` files: `/Users/chrgre01/src/PrimeStruct/src/ir_lowerer/IrLowererSemanticProductTargetAdapters.cpp`, `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_source_delegation_stays_stable.cpp`, `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_backends_graph_contexts.h`; fix: remove transitional duplicated legacy adapter-map population/fallback paths and migrate downstream checks to consume `SemanticProductIndex` directly.
    - [x] `TODO-0405-F03` files: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_backends_registry.cpp`, `/Users/chrgre01/src/PrimeStruct/tests/unit/test_semantics_definition_partitioner.cpp`; fix: add/refresh deterministic parity coverage across worker counts (`1`, `2`, `4`) for the unified index path.
  - acceptance:
    - Lowerer semantic-product adapter paths consume one shared index builder instead of duplicating per-family map assembly logic.
    - Determinism parity coverage keeps semantic-product output and diagnostics stable across worker counts (`1`, `2`, `4`) on existing stress fixtures.
    - Semantic memory benchmark reports show non-regression for the semantic-product/lowering path, with measurable allocation or runtime improvement in at least one tracked fixture.
  - stop_rule: If measurable benchmark impact is not achieved after two attempts, archive this leaf as low-value per rule 16 and replace it with a different Group 15 hotspot.

- [ ] TODO-0403: Keep queue, lanes, and snapshots synchronized
  - owner: ai
  - created_at: 2026-04-13
  - phase: Cross-cutting
  - depends_on: TODO-0401, TODO-0402
  - scope: Keep this TODO log internally consistent as Group 14/15 leaf tasks are added, split, completed, and archived.
  - acceptance:
    - Every TODO ID listed in `Ready Now`, `Immediate Next 10`, `Priority Lanes`, and `Execution Queue` has a matching task block.
    - Archived IDs are removed from open sections in the same change that moves their task blocks to `docs/todo_finished.md`.
    - Coverage snapshot tables reflect only currently open IDs.
  - stop_rule: If synchronization touches more than one logical workstream, split updates into focused queue-only follow-up tasks.

- [ ] TODO-0402: Group 15 completion tracker (semantic memory + multithread substrate)
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 15
  - scope: Define and execute remaining measurable leaf slices after archived P0/P1/P2 work to ship memory/runtime wins with strict regression control.
  - acceptance:
    - Remaining Group 15 leaf tasks are listed with stable IDs and explicit metric baselines/targets.
    - Each new leaf includes concrete validation commands, including release build/test and focused benchmark or memory checks.
    - Completed Group 15 leaves are moved to `docs/todo_finished.md` with evidence notes.
  - stop_rule: If a leaf cannot demonstrate measurable impact within one code-affecting commit, split it into smaller measurable leaves before implementation.
  - notes: Prior Group 15 slices `[P0-17]` through `[P0-28]`, `[P1-01]` through `[P1-03]`, and `[P2-14]` through `[P2-42]` are already archived in `docs/todo_finished.md` (April 12-13, 2026).

- [ ] TODO-0401: Group 14 completion tracker (SoA end-state cleanup)
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 14
  - scope: Track and close Group 14 child leaves required to finish stdlib-authoritative collection behavior and remove compatibility scaffolding; implementation work lives in child TODOs.
  - acceptance:
    - Active Group 14 implementation leaves (currently TODO-0408 and TODO-0409) stay explicit with bounded scope, dependencies, and verification steps.
    - This tracker is only completed after all Group 14 child leaves are archived in `docs/todo_finished.md` with evidence notes.
    - At least one leaf is always `Ready Now` with no unmet TODO dependencies.
  - stop_rule: If a leaf is too large for one commit plus focused conformance updates, split it before implementation.
  - notes: Current active Group 14 child leaves are TODO-0408 and TODO-0409. Prior Group 14 slices `[S2-01]` through `[S2-05]`, `[S3-01]` through `[S3-132]`, and `[S4-01a1]` through `[S4-03]` are already archived in `docs/todo_finished.md` (April 12-13, 2026).
