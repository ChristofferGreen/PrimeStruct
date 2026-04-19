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

1. TODO-0998
2. TODO-0411
3. TODO-0401
4. TODO-0402
5. TODO-0405

### Immediate Next 10 (After Ready Now)

1. TODO-0406
2. TODO-0403

### Priority Lanes (Current)

- P1 Collection stdlib ownership cutover (`map`, `soa_vector`): TODO-0998, TODO-0411
- P2 SoA canonicalization + semantic memory/perf + multithread substrate + semantic-product boundary hardening: TODO-0401, TODO-0402, TODO-0405, TODO-0406
- P3 Queue/snapshot governance: TODO-0403

### Execution Queue (Recommended)

Wave A (collection stdlib ownership cutover):
1. TODO-0998
2. TODO-0411

Wave B (SoA completion):
1. TODO-0401

Wave C (semantic memory/perf):
1. TODO-0402
2. TODO-0405
3. TODO-0406

Wave D (queue hygiene):
1. TODO-0403

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Collection stdlib ownership cutover (`map`, `soa_vector`) | TODO-0998, TODO-0411 |
| SoA bring-up and stdlib-authoritative `soa_vector` end-state cleanup | TODO-0401 |
| Semantic memory footprint and multithread compile substrate | TODO-0402 |
| Semantic-product contract/index boundary hardening | TODO-0405, TODO-0406 |
| TODO queue quality gates and dependency/coverage synchronization | TODO-0403 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Release gate (`./scripts/compile.sh --release`) discipline | TODO-0401, TODO-0402, TODO-0405, TODO-0406, TODO-0998, TODO-0411 |
| Collection conformance and alias-deletion checks (`map`/`soa_vector`) | TODO-0998, TODO-0411 |
| Benchmark/runtime regression checks (`./scripts/benchmark.sh`) | TODO-0402 |
| Semantic-product contract/index and deterministic conformance checks | TODO-0405, TODO-0406 |
| TODO/open-vs-finished hygiene (`docs/todo.md` vs `docs/todo_finished.md`) | TODO-0403 |

### Task Blocks

- [~] TODO-0409: Track remaining `map` + `soa_vector` name-routing cleanup
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 14
  - scope: Track the remaining child leaves needed to finish stdlib-authoritative `map` and `soa_vector` behavior and delete the last production compiler/runtime name/path alias logic for those collections; implementation work lives in child TODOs.
  - acceptance:
    - Active child leaves for `map` and `soa_vector` (currently TODO-0998 and TODO-0411) stay explicit with bounded scope, deletion targets, and verification commands.
    - This tracker is only completed after the `map` and `soa_vector` child work is archived in `docs/todo_finished.md` with evidence notes.
    - At least one collection-alias deletion leaf remains `Ready Now` until the final collection compatibility routing is gone.
  - stop_rule: If either collection branch remains too large for one code-affecting commit, keep splitting it into explicit child leaves before implementation continues.
  - notes: Split the old broad leaf into `TODO-0410` (`map`) and `TODO-0411` (`soa_vector`). Archived children `TODO-0996` and `TODO-0997` removed the lowerer and emitter explicit `map` `contains`/`tryAt` slash-method compatibility routing; active child `TODO-0998` owns the next `map` emitter direct-call cleanup slice.

- [~] TODO-0410: Track remaining `map` name-routing cleanup
  - owner: ai
  - created_at: 2026-04-19
  - phase: Group 14
  - scope: Track the remaining child leaves needed to delete production `map` name/path alias routing after the vector cutover; implementation work lives in child TODOs.
  - acceptance:
    - Active `map` child leaves stay explicit with bounded scope, canonical-path goals, and verification commands.
    - Completed `map` child leaves move to `docs/todo_finished.md` with evidence notes in the same change that removes them from active planning.
    - This tracker is only completed after no production semantics/lowerer/emitter code path still preserves rooted `map` compatibility routing for normal helper dispatch.
  - stop_rule: If the remaining `map` cleanup still spans multiple subsystems, keep landing one subsystem-sized child leaf at a time before moving to `soa_vector`.
  - notes: Archived children `TODO-0996` and `TODO-0997` removed the lowerer and emitter explicit `map` `contains`/`tryAt` slash-method compatibility branch family. Active child `TODO-0998` owns the remaining emitter direct-call alias cleanup slice for that helper family.

- [ ] TODO-0998: Remove emitter direct-call `map` result alias routing for `contains` and `tryAt`
  - owner: ai
  - created_at: 2026-04-19
  - phase: Group 14
  - depends_on: TODO-0997
  - scope: Delete the remaining emitter-side rooted `map` compatibility routing that still preserves same-path `/map/{contains,tryAt}` direct-call result metadata and struct-method chaining after explicit slash-method receiver routing has been removed.
  - acceptance:
    - Production emitter direct-call resolution no longer preserves rooted `/map/{contains,tryAt}` result metadata or struct-method chaining when canonical `/std/collections/map/*` helpers exist.
    - Bare map helper emission continues to resolve through canonical `/std/collections/map/*` helpers or fail with canonical unknown-target diagnostics when those helpers are absent.
    - Focused emitter coverage pins the rejection of rooted `/map/{contains,tryAt}` direct-call chaining while leaving any untouched `map` access-family behavior explicit.
    - At least one real emitter-side `map` alias branch family is deleted rather than renamed.
  - stop_rule: If the remaining emitter direct-call cleanup still spans multiple helper families, land `contains`/`tryAt` first and add one follow-up leaf for any untouched access-family routing.

- [ ] TODO-0411: Remove C++ name routing for `soa_vector`
  - owner: ai
  - created_at: 2026-04-19
  - phase: Group 14
  - scope: Complete stdlib-authoritative `soa_vector` migration by deleting the remaining production compiler/runtime name/path alias logic for `soa_vector` helpers in semantics/lowering/emitter (`src/` + `include/primec`, excluding tests/testing-only facades).
  - acceptance:
    - Production `src/` + `include/` `soa_vector` helper routing no longer hardcodes rooted `soa_vector` symbol/path aliases for normal call/access classification.
    - Release-mode `soa_vector` conformance/tests can run against canonical stdlib helper paths with any changed diagnostics updated via deterministic snapshots.
    - At least one real `soa_vector` compatibility subsystem (legacy alias/canonicalization branch family) is deleted rather than renamed.
    - Focused release-mode suites for the affected `soa_vector` migration surface remain identified in the task notes before implementation starts.
  - stop_rule: If the remaining `soa_vector` cleanup spans multiple helper families or subsystems, split it into explicit child leaves before implementation.

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
    - Active Group 14 implementation leaves (currently TODO-0998 and TODO-0411, tracked under TODO-0410 and TODO-0409) stay explicit with bounded scope, dependencies, and verification steps.
    - This tracker is only completed after all Group 14 child leaves are archived in `docs/todo_finished.md` with evidence notes.
    - At least one leaf is always `Ready Now` with no unmet TODO dependencies.
  - stop_rule: If a leaf is too large for one commit plus focused conformance updates, split it before implementation.
  - notes: Current active Group 14 child leaves are TODO-0998 and TODO-0411, tracked under TODO-0410 and TODO-0409. Archived Group 14 trackers/leaves include the vector cleanup set TODO-0408, TODO-0990, TODO-0991, TODO-0992, TODO-0993, TODO-0994, and TODO-0995 plus the first two map children TODO-0996 and TODO-0997; earlier Group 14 slices `[S2-01]` through `[S2-05]`, `[S3-01]` through `[S3-132]`, and `[S4-01a1]` through `[S4-03]` are already recorded in `docs/todo_finished.md` (April 12-19, 2026).
