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

1. TODO-1016

### Immediate Next 10 (After Ready Now)

1. TODO-0406
2. TODO-0403

### Priority Lanes (Current)

- P1 Semantic memory/perf + multithread substrate + semantic-product boundary hardening: TODO-0402, TODO-0405, TODO-1013, TODO-1016, TODO-0406
- P3 Queue/snapshot governance: TODO-0403

### Execution Queue (Recommended)

Wave A (semantic memory/perf):
1. TODO-1016
2. TODO-0406

Wave B (queue hygiene):
1. TODO-0403

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic memory footprint and multithread compile substrate | TODO-0402 |
| Semantic-product contract/index boundary hardening | TODO-0405, TODO-1013, TODO-1016, TODO-0406 |
| TODO queue quality gates and dependency/coverage synchronization | TODO-0403 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Release gate (`./scripts/compile.sh --release`) discipline | TODO-0402, TODO-0405, TODO-1013, TODO-1016, TODO-0406 |
| Benchmark/runtime regression checks (`./scripts/benchmark.sh`) | TODO-0402 |
| Semantic-product contract/index and deterministic conformance checks | TODO-0405, TODO-1013, TODO-1016, TODO-0406 |
| TODO/open-vs-finished hygiene (`docs/todo.md` vs `docs/todo_finished.md`) | TODO-0403 |

### Task Blocks

- [ ] TODO-1016: Refresh checked-in semantic-memory baseline report for unified index proof
  - owner: ai
  - created_at: 2026-04-19
  - phase: Group 15
  - scope: Regenerate and check in the semantic-memory benchmark artifact needed to record unified lowerer `SemanticProductIndex` non-regression explicitly, including the semantic-product index-family and worker-parity fields added in `TODO-1014`.
  - acceptance:
    - A checked-in semantic-memory report artifact or paired checked-in evidence file includes `semantic_product_index_family_counts` for semantic-product rows and `definition_validation_worker_mode_deltas` family-parity fields for the unified index path.
    - `TODO-0405` notes cite that refreshed artifact explicitly when claiming non-regression evidence.
    - The refresh stays in benchmark/reporting/docs surfaces and does not reopen lowerer adapter implementation.
  - stop_rule: If the full canonical benchmark refresh is too expensive for one safe change, split a narrower artifact-refresh leaf (for example worker-parity evidence only) before continuing.
  - notes: Follow-on to archived children `TODO-1014` and `TODO-1015`, which added the report fields and updated the checked-in policy/evidence note surface.

- [~] TODO-1013: Track unified semantic-product index benchmark proof
  - owner: ai
  - created_at: 2026-04-19
  - phase: Group 15
  - scope: Gather the remaining benchmark and deterministic non-regression evidence for the unified lowerer `SemanticProductIndex` builder now that the per-family fallback scans are removed.
  - acceptance:
    - Benchmark or focused measurement output records non-regression for the unified semantic-product adapter path relative to the previous baseline expectations.
    - Existing deterministic parity coverage across worker counts stays aligned with the fully shared index-backed lookup path.
    - `docs/todo.md` / `docs/todo_finished.md` notes reflect the measured outcome instead of leaving `TODO-0405` blocked on an implicit benchmark promise.
  - stop_rule: If the benchmark harness cannot isolate this path cleanly, split the measurement work into a narrower reporting leaf instead of reopening lowerer adapter implementation work.
  - notes: Archived child `TODO-1014` added explicit semantic-product index-family counts and worker-parity fields to the semantic-memory benchmark report, and archived child `TODO-1015` updated the checked-in policy/evidence notes to reference that surface explicitly. Active child `TODO-1016` now targets the refreshed checked-in benchmark artifact.

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
  - notes: Archived children `TODO-1008`, `TODO-1009`, `TODO-1010`, `TODO-1011`, `TODO-1012`, `TODO-1014`, and `TODO-1015` moved return-fact, on-error, local-auto, query, and try fallback lookup plus benchmark-report family-count wiring and checked-in evidence notes into the shared semantic-product adapter path. Active tracker `TODO-1013` now points at refreshed benchmark artifact work `TODO-1016`.

- [ ] TODO-0403: Keep queue, lanes, and snapshots synchronized
  - owner: ai
  - created_at: 2026-04-13
  - phase: Cross-cutting
  - depends_on: TODO-0402
  - scope: Keep this TODO log internally consistent as Group 14/15 leaf tasks are added, split, completed, and archived.
  - acceptance:
    - Every TODO ID listed in `Ready Now`, `Immediate Next 10`, `Priority Lanes`, and `Execution Queue` has a matching task block.
    - Archived IDs are removed from open sections in the same change that moves their task blocks to `docs/todo_finished.md`.
    - Coverage snapshot tables reflect only currently open IDs.
  - stop_rule: If synchronization touches more than one logical workstream, split updates into focused queue-only follow-up tasks.

- [~] TODO-0402: Group 15 completion tracker (semantic memory + multithread substrate)
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 15
  - scope: Define and execute remaining measurable leaf slices after archived P0/P1/P2 work to ship memory/runtime wins with strict regression control.
  - acceptance:
    - Remaining Group 15 leaf tasks are listed with stable IDs and explicit metric baselines/targets.
    - Each new leaf includes concrete validation commands, including release build/test and focused benchmark or memory checks.
    - Completed Group 15 leaves are moved to `docs/todo_finished.md` with evidence notes.
  - stop_rule: If a leaf cannot demonstrate measurable impact within one code-affecting commit, split it into smaller measurable leaves before implementation.
  - notes: Prior Group 15 slices `[P0-17]` through `[P0-28]`, `[P1-01]` through `[P1-03]`, and `[P2-14]` through `[P2-42]` are already archived in `docs/todo_finished.md` (April 12-13, 2026). Archived children `TODO-1008`, `TODO-1009`, `TODO-1010`, `TODO-1011`, `TODO-1012`, `TODO-1014`, and `TODO-1015` moved return-fact, on-error, local-auto, query, and try fallback lookup plus benchmark-report family-count wiring and checked-in evidence notes into the shared semantic-product adapter path.
