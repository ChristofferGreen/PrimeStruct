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
18. Treat disabled tests as debt: every retained `doctest::skip(true)` cluster must either map to an active TODO leaf with a clear re-enable-or-delete outcome, or be removed once proven stale.

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

1. TODO-1063

### Immediate Next 10 (After Ready Now)

1. TODO-1063
2. TODO-1064
3. TODO-1065

### Priority Lanes (Current)

- Pilot semantic boundary ownership: TODO-1063, TODO-1064, TODO-1065

### Execution Queue (Recommended)

Wave A:
1. TODO-1063

Wave B:
1. TODO-1064
2. TODO-1065

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Pilot semantic ownership boundary | TODO-1063, TODO-1064, TODO-1065 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product publication parity | TODO-1063, TODO-1064 |
| Lowering conformance and fallback deletion | TODO-1065 |

### Task Blocks

- [ ] TODO-1065: Delete remaining lowering-side AST semantic fallbacks for migrated families
  - owner: ai
  - created_at: 2026-04-19
  - phase: Architecture boundary hardening
  - depends_on: TODO-1064
  - scope: remove residual lowering paths that still reconstruct semantic meaning from AST state for the migrated pilot routing slice whenever a semantic product is present, turning gaps into explicit contract failures instead.
  - acceptance:
    - the migrated pilot routing families consume semantic-product facts only and no longer parse equivalent meaning back out of AST nodes
    - negative tests fail deterministically when required semantic-product fields are absent or inconsistent
    - at least one compatibility fallback or adapter-only reconstruction path is deleted for each migrated pilot routing family
  - stop_rule: stop after the pilot routing slice is semantic-product-only; do not broaden to untouched fact families in the same change

- [ ] TODO-1064: Precompute lowering lookup tables during semantic-product publication
  - owner: ai
  - created_at: 2026-04-19
  - phase: Architecture boundary hardening
  - depends_on: TODO-1063
  - scope: move the lowering lookup/index work now rebuilt in `IrLowererSemanticProductTargetAdapters.cpp` into semantic-product publication for the pilot routing slice so lowering consumes preindexed views instead of reconstructing routing maps per run.
  - acceptance:
    - lowering no longer rebuilds the current semantic-product routing index tables for the pilot slice
    - semantic-product publication emits the preindexed views or equivalent owned lookup tables needed by lowering for that slice
    - focused lowering tests prove parity while memory/perf coverage shows the adapter rebuild cost is removed or reduced for the pilot slice
  - stop_rule: stop after direct-call, method-call, bridge-path, and callable-summary lookup support are precomputed; leave other fact families for follow-up if needed

- [ ] TODO-1063: Replace pilot routing snapshot export with append-only fact collectors
  - owner: ai
  - created_at: 2026-04-19
  - phase: Architecture boundary hardening
  - scope: route the pilot routing semantic-product families through builder-owned collectors populated during validation, reducing the snapshot-export surface on `SemanticsValidator` for the first ownership slice.
  - acceptance:
    - direct-call, method-call, bridge-path, and callable-summary facts are emitted from append-only collectors instead of post-run snapshot reconstruction
    - the corresponding pilot routing snapshot structs or export methods disappear from `SemanticsValidator`
    - semantic-product formatter and lowering parity stay unchanged for the migrated pilot routing facts
  - stop_rule: stop after the pilot routing families prove the collector pattern and shrink the validator export surface; leave non-routing families for follow-up tasks
