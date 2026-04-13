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
2. TODO-0401
3. TODO-0402
4. TODO-0404

### Immediate Next 10 (After Ready Now)

1. TODO-0408
2. TODO-0405
3. TODO-0406
4. TODO-0403

### Priority Lanes (Current)

- P0 Test implementation locality cleanup (no `TEST_CASE` in include chunks): TODO-0407
- P1 SoA canonicalization + semantic memory/perf + multithread substrate + semantic-product boundary hardening: TODO-0401, TODO-0402, TODO-0404, TODO-0405, TODO-0406
- P2 Test naming alignment + queue/snapshot governance: TODO-0408, TODO-0403

### Execution Queue (Recommended)

Wave A (test implementation locality cleanup):
1. TODO-0407

Wave B (SoA completion):
1. TODO-0401

Wave C (semantic memory/perf):
1. TODO-0402
2. TODO-0404
3. TODO-0405
4. TODO-0406

Wave D (test naming + queue hygiene):
1. TODO-0408
2. TODO-0403

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Test implementation locality and include-chunk retirement | TODO-0407 |
| Test file/suite naming alignment and numeric-chunk removal | TODO-0408 |
| SoA bring-up and stdlib-authoritative `soa_vector` end-state cleanup | TODO-0401 |
| Semantic memory footprint and multithread compile substrate | TODO-0402 |
| Semantic-product contract/index boundary hardening | TODO-0404, TODO-0405, TODO-0406 |
| TODO queue quality gates and dependency/coverage synchronization | TODO-0403 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Release gate (`./scripts/compile.sh --release`) discipline | TODO-0401, TODO-0402, TODO-0404, TODO-0405, TODO-0406, TODO-0407, TODO-0408 |
| Test source locality and include-layer guardrail verification | TODO-0407 |
| Test suite/file naming consistency audit + focused doctest reruns | TODO-0408 |
| Benchmark/runtime regression checks (`./scripts/benchmark.sh`) | TODO-0402 |
| Semantic-product contract/index and deterministic conformance checks | TODO-0404, TODO-0405, TODO-0406 |
| TODO/open-vs-finished hygiene (`docs/todo.md` vs `docs/todo_finished.md`) | TODO-0403 |

### Task Blocks

- [ ] TODO-0408: Align test file and suite names with test intent
  - owner: ai
  - created_at: 2026-04-13
  - phase: Test architecture cleanup
  - depends_on: TODO-0407
  - scope: Apply file-by-file naming cleanup so each migrated test file and `TEST_SUITE` label matches the behavior under test and avoids numeric shard naming.
  - file_todos:
    - [x] `TODO-0408-F01` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_vm_math.cpp`; fix: rename suite from `primestruct.compile.run.vm.collections` to `primestruct.compile.run.vm.math` and keep test names math-focused.
    - [x] `TODO-0408-F02` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_vm_maps.cpp`; fix: rename suite from `primestruct.compile.run.vm.collections` to `primestruct.compile.run.vm.maps` and keep map-only coverage in this file.
    - [x] `TODO-0408-F03` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_vm_outputs.cpp`; fix: rename suite from `primestruct.compile.run.vm.collections` to `primestruct.compile.run.vm.outputs` after moving include-defined tests into `.cpp` units.
    - [ ] `TODO-0408-F04` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_vm_collections.cpp`; fix: keep suite `primestruct.compile.run.vm.collections` but ensure only collection tests remain after split-out.
    - [x] `TODO-0408-F05` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_vm_bounds.cpp`; fix: move suite from `primestruct.compile.run.vm.collections` to a bounds-focused suite name such as `primestruct.compile.run.vm.bounds`.
    - [x] `TODO-0408-F06` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_backends_registry.cpp`; fix: narrow suite name from generic `primestruct.ir.pipeline.backends` to registry-specific naming (for example `primestruct.ir.pipeline.backends.registry`) when tests are moved out of the header.
    - [x] `TODO-0408-F07` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_backends.cpp`; fix: rename generic suite to backend-core-focused naming so it no longer collides with backend-specialized files.
    - [x] `TODO-0408-F08` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_backends_glsl.cpp`; fix: rename suite to GLSL-specific naming (for example `primestruct.ir.pipeline.backends.glsl`).
    - [x] `TODO-0408-F09` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_backends_cpp_vm.cpp`; fix: rename suite to cpp/vm-focused naming (for example `primestruct.ir.pipeline.backends.cpp_vm`).
    - [ ] `TODO-0408-F10` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_validation.cpp`; fix: after chunk retirement, rename resulting files/suites to semantic topics (no numeric suffixes like `_01`, `_45`, `_92`).
    - [ ] `TODO-0408-F11` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_semantics_calls_and_flow_collections.cpp`; fix: after chunk retirement, split into topic files/suites (`calls`, `flow`, `collections`) without numeric suffix naming.
    - [ ] `TODO-0408-F12` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_compile_run_emitters.cpp`; fix: after chunk retirement, split into emitter-topic file/suite names (for example `vm`, `cpp-ir`, `exe-ir`, `glsl-ir`, `spirv-ir`) and remove numeric chunk naming.
    - [x] `TODO-0408-F13` file: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_semantics_manual.cpp`; fix: remove the duplicate `TEST_SUITE_BEGIN("primestruct.semantics.manual")` so suite nesting is well-formed and unambiguous.
  - acceptance:
    - Every entry in `file_todos` is completed for renamed files/suites in scope.
    - Migrated test filenames use semantic topic names instead of numeric-only suffix chunks such as `001`, `003`, or `0045`.
    - Migrated `.cpp` files use `TEST_SUITE` labels that match their file purpose and no longer reuse unrelated generic suite names.
    - Add/update one lightweight guardrail (scripted check or documented grep gate) that detects new numeric-suffix test files and obvious suite/file naming mismatches.
  - stop_rule: If full-repo rename fallout is too large for one safe slice, land one subsystem at a time with follow-up TODO leaves.

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

- [ ] TODO-0405: Introduce unified semantic-product index builder
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 15
  - depends_on: TODO-0404
  - scope: Replace ad-hoc lowerer semantic-product lookup-map construction with one typed `SemanticProductIndex` builder and shared lookup API for direct/method/bridge/binding/local-auto/query/try/on_error families.
  - acceptance:
    - Lowerer semantic-product adapter paths consume one shared index builder instead of duplicating per-family map assembly logic.
    - Determinism parity coverage keeps semantic-product output and diagnostics stable across worker counts (`1`, `2`, `4`) on existing stress fixtures.
    - Semantic memory benchmark reports show non-regression for the semantic-product/lowering path, with measurable allocation or runtime improvement in at least one tracked fixture.
  - stop_rule: If measurable benchmark impact is not achieved after two attempts, archive this leaf as low-value per rule 16 and replace it with a different Group 15 hotspot.

- [ ] TODO-0404: Add semantic-product contract manifest and versioned verifier
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 15
  - scope: Define a declarative `SemanticProductContract v1` (required fact families, key fields, and identity rules) and enforce it before lowering/emit paths so contract violations fail with stable diagnostics.
  - acceptance:
    - One versioned contract source-of-truth exists for lowering-facing semantic-product families and required fields.
    - Compile-pipeline/lowering preflight rejects contract-version mismatch and missing required family/field data with deterministic diagnostics before backend dispatch.
    - Conformance tests cover at least one positive case and one missing-family or missing-key negative case through consuming emit paths.
  - stop_rule: If contract wiring requires broad cross-module generator work, land a minimal verifier over existing data first and split generator work into follow-up leaves.

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
  - scope: Define and execute remaining code-affecting leaves required to finish stdlib-authoritative `soa_vector` behavior and remove compatibility scaffolding.
  - acceptance:
    - Remaining Group 14 leaves are broken into explicit IDs with bounded scope and verification steps.
    - At least one leaf is always `Ready Now` with no unmet TODO dependencies.
    - Completed Group 14 leaves are archived to `docs/todo_finished.md` with concise evidence notes.
  - stop_rule: If a leaf is too large for one commit plus focused conformance updates, split it before implementation.
  - notes: Prior Group 14 slices `[S2-01]` through `[S2-05]`, `[S3-01]` through `[S3-132]`, and `[S4-01a1]` through `[S4-03]` are already archived in `docs/todo_finished.md` (April 12-13, 2026).
