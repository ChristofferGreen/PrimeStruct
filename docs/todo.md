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

1. TODO-1002
2. TODO-0401
3. TODO-0402
4. TODO-0405
5. TODO-0406

### Immediate Next 10 (After Ready Now)

1. TODO-0403

### Priority Lanes (Current)

- P1 Collection stdlib ownership cutover (`soa_vector`): TODO-0411, TODO-1002
- P2 SoA canonicalization + semantic memory/perf + multithread substrate + semantic-product boundary hardening: TODO-0401, TODO-0402, TODO-0405, TODO-0406
- P3 Queue/snapshot governance: TODO-0403

### Execution Queue (Recommended)

Wave A (collection stdlib ownership cutover):
1. TODO-1002

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
| Collection stdlib ownership cutover (`soa_vector`) | TODO-0411, TODO-1002 |
| SoA bring-up and stdlib-authoritative `soa_vector` end-state cleanup | TODO-0401 |
| Semantic memory footprint and multithread compile substrate | TODO-0402 |
| Semantic-product contract/index boundary hardening | TODO-0405, TODO-0406 |
| TODO queue quality gates and dependency/coverage synchronization | TODO-0403 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Release gate (`./scripts/compile.sh --release`) discipline | TODO-0401, TODO-0402, TODO-0405, TODO-0406, TODO-0411, TODO-1002 |
| Collection conformance and alias-deletion checks (`soa_vector`) | TODO-0411, TODO-1002 |
| Benchmark/runtime regression checks (`./scripts/benchmark.sh`) | TODO-0402 |
| Semantic-product contract/index and deterministic conformance checks | TODO-0405, TODO-0406 |
| TODO/open-vs-finished hygiene (`docs/todo.md` vs `docs/todo_finished.md`) | TODO-0403 |

### Task Blocks

- [ ] TODO-1002: Remove remaining lowerer `soa_vector` `to_aos` helper bridge
  - owner: ai
  - created_at: 2026-04-19
  - phase: Group 14
  - scope: Delete the remaining lowerer compatibility bridge that still lets `soa_vector` helper lookup bounce between rooted `/to_aos` and canonical `/std/collections/soa_vector/to_aos` paths.
  - acceptance:
    - Canonical `soa_vector` receiver lookup for `to_aos` no longer falls back to rooted `/to_aos`.
    - Bare `soa_vector` receiver probing either resolves canonical `/std/collections/soa_vector/to_aos` definitions or reports the existing canonical mismatch diagnostic when only rooted compatibility defs remain.
    - Focused lowerer tests pin the canonical-only `to_aos` behavior.
  - stop_rule: If semantics or emitter compatibility cleanup is needed after the lowerer bridge is removed, split that work into separate `TODO-XXXX` leaves before continuing.
  - notes: Focused validation surface when full release-mode runs resume: `PrimeStruct_ir_tests` cases in `test_ir_pipeline_validation_ir_lowerer_setup_type_helper_resolves_method_definitions_from_receiver_targets.cpp` and adjacent `soa_vector` lowerer coverage.

- [~] TODO-0411: Track remaining `soa_vector` name-routing cleanup
  - owner: ai
  - created_at: 2026-04-19
  - phase: Group 14
  - scope: Track the remaining child leaves needed to finish stdlib-authoritative `soa_vector` migration and delete the last production compiler/runtime name/path alias logic across lowerer, semantics, and emitter surfaces; implementation work lives in child TODOs.
  - acceptance:
    - Active child leaves for the remaining `soa_vector` helper families stay explicit with bounded scope, deletion targets, and focused validation surfaces.
    - This tracker is only completed after the remaining `soa_vector` children are archived in `docs/todo_finished.md` with evidence notes.
    - At least one real `soa_vector` compatibility subsystem (legacy alias/canonicalization branch family) is deleted rather than renamed by the child leaves.
  - stop_rule: If the remaining `soa_vector` cleanup spans multiple helper families or subsystems, keep splitting it into explicit child leaves before implementation continues.
  - notes: Archived child `TODO-1000` canonicalized lowerer bare `get`/`ref` receiver routing to `/std/collections/soa_vector/*`, and archived child `TODO-1001` canonicalized lowerer bare `push`/`reserve` receiver routing. Active child `TODO-1002` now owns the remaining lowerer `to_aos` bridge cleanup before later semantics/emitter slices.

- [~] TODO-0409: Track remaining `map` + `soa_vector` name-routing cleanup
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 14
  - scope: Track the remaining child leaves needed to finish stdlib-authoritative `map` and `soa_vector` behavior and delete the last production compiler/runtime name/path alias logic for those collections; implementation work lives in child TODOs.
  - acceptance:
    - Active child leaves for the remaining `soa_vector` branch (currently tracker TODO-0411 with live leaf TODO-1002) stay explicit with bounded scope, deletion targets, and verification commands.
    - This tracker is only completed after the remaining `soa_vector` child work is archived in `docs/todo_finished.md` with evidence notes.
    - At least one collection-alias deletion leaf remains `Ready Now` until the final collection compatibility routing is gone.
  - stop_rule: If either collection branch remains too large for one code-affecting commit, keep splitting it into explicit child leaves before implementation continues.
  - notes: Split the old broad leaf into `TODO-0410` (`map`) and `TODO-0411` (`soa_vector`). Archived `map` children `TODO-0996` through `TODO-0999` removed the lowerer, emitter explicit slash-method, and emitter direct-call compatibility routing for the remaining `map` helper families. Archived `soa_vector` children `TODO-1000` and `TODO-1001` canonicalized lowerer bare access and mutator receiver routing, and active leaf `TODO-1002` now owns the remaining lowerer `to_aos` bridge slice.

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
    - Active Group 14 implementation leaves (currently TODO-1002 under tracker TODO-0411, tracked under TODO-0409) stay explicit with bounded scope, dependencies, and verification steps.
    - This tracker is only completed after all Group 14 child leaves are archived in `docs/todo_finished.md` with evidence notes.
    - At least one leaf is always `Ready Now` with no unmet TODO dependencies.
  - stop_rule: If a leaf is too large for one commit plus focused conformance updates, split it before implementation.
  - notes: Current active Group 14 child leaf is TODO-1002 under tracker TODO-0411, tracked under TODO-0409. Archived Group 14 trackers/leaves include the vector cleanup set TODO-0408, TODO-0990, TODO-0991, TODO-0992, TODO-0993, TODO-0994, and TODO-0995, the full `map` cleanup set TODO-0410, TODO-0996, TODO-0997, TODO-0998, and TODO-0999, and the first `soa_vector` lowerer children TODO-1000 and TODO-1001; earlier Group 14 slices `[S2-01]` through `[S2-05]`, `[S3-01]` through `[S3-132]`, and `[S4-01a1]` through `[S4-03]` are already recorded in `docs/todo_finished.md` (April 12-19, 2026).
