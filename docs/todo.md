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

1. TODO-1022

### Immediate Next 10 (After Ready Now)

1. TODO-0403

### Priority Lanes (Current)

- P1 Semantic memory/perf + multithread substrate + semantics/testing API boundary hardening: TODO-0402, TODO-0406, TODO-1022
- P3 Queue/snapshot governance: TODO-0403

### Execution Queue (Recommended)

Wave A (semantic memory/perf):
1. TODO-1022

Wave B (queue hygiene):
1. TODO-0403

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic memory footprint and multithread compile substrate | TODO-0402 |
| Semantic/testing API boundary hardening | TODO-0406, TODO-1022 |
| TODO queue quality gates and dependency/coverage synchronization | TODO-0403 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Release gate (`./scripts/compile.sh --release`) discipline | TODO-0402, TODO-0406, TODO-1022 |
| Benchmark/runtime regression checks (`./scripts/benchmark.sh`) | TODO-0402 |
| Semantic/testing API and deterministic conformance checks | TODO-0406, TODO-1022 |
| TODO/open-vs-finished hygiene (`docs/todo.md` vs `docs/todo_finished.md`) | TODO-0403 |

### Task Blocks

- [~] TODO-0406: Split production semantics APIs from testing snapshots
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 15
  - scope: Track bounded removal of snapshot-for-testing surface area from production semantics interfaces so `include/primec/testing/*` facades and semantic-product views become the only supported testing entry points.
  - acceptance:
    - Child leaves remove snapshot-only structs/methods from `SemanticsValidator` in bounded families while preserving existing `include/primec/testing/*` facades.
    - Existing semantics/IR snapshot tests continue to compile through testing facades or semantic-product projections without adding new `tests -> src` include-layer allowlist entries.
    - At least one snapshot-only compatibility path in production semantics code is deleted in each completed child rather than renamed in place.
  - stop_rule: If a snapshot family cannot be removed without touching unrelated runtime/backend consumers, split it into a smaller facade-projection child before continuing.
  - notes: Archived child `TODO-1021` removed the duplicate query snapshot family from `SemanticsValidator`; `TODO-1022` is the next live leaf for the remaining try/on_error/validation-context aliases.

- [ ] TODO-1022: Route remaining try/on_error/validation testing snapshots through production semantic facts
  - owner: ai
  - created_at: 2026-04-19
  - phase: Group 15
  - scope: Reuse semantic-product facts or callable summaries for the remaining try/on_error/validation-context testing snapshots so more `SemanticsValidator::*ForTesting()` aliases can be deleted.
  - acceptance:
    - `computeTypeResolutionTryValueSnapshotForTesting`, `computeTypeResolutionOnErrorSnapshotForTesting`, and `computeTypeResolutionValidationContextSnapshotForTesting` no longer depend on testing-only `SemanticsValidator` snapshot methods.
    - Focused snapshot/source-lock tests pin the new projection path without adding any `tests -> src` include-layer allowlist entries.
    - At least one additional snapshot-only method disappears from `SemanticsValidator.h`.
  - stop_rule: If any remaining snapshot view needs a new production fact shape instead of a pure projection, stop and split that shape work into its own follow-up leaf rather than widening this task.

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
  - notes: Prior Group 15 slices `[P0-17]` through `[P0-28]`, `[P1-01]` through `[P1-03]`, and `[P2-14]` through `[P2-42]` are already archived in `docs/todo_finished.md` (April 12-13, 2026). Archived items `TODO-0405`, `TODO-1013`, `TODO-1016`, `TODO-1018`, `TODO-1017`, and `TODO-1019`, and `TODO-1020` now complete the unified semantic-product benchmark-proof chain with a refreshed full baseline report plus paired contract and primary-fixture measured artifacts. `TODO-0406` now tracks the last live Group 15 workstream, with archived child `TODO-1021` done and `TODO-1022` remaining.
