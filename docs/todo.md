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

1. TODO-0403

### Immediate Next 10 (After Ready Now)

1. none

### Priority Lanes (Current)

- P3 Queue/snapshot governance: TODO-0403

### Execution Queue (Recommended)

Wave A (queue hygiene):
1. TODO-0403

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| TODO queue quality gates and dependency/coverage synchronization | TODO-0403 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| TODO/open-vs-finished hygiene (`docs/todo.md` vs `docs/todo_finished.md`) | TODO-0403 |

### Task Blocks

- [ ] TODO-0403: Keep queue, lanes, and snapshots synchronized
  - owner: ai
  - created_at: 2026-04-13
  - phase: Cross-cutting
  - scope: Keep this TODO log internally consistent as Group 14/15 leaf tasks are added, split, completed, and archived.
  - acceptance:
    - Every TODO ID listed in `Ready Now`, `Immediate Next 10`, `Priority Lanes`, and `Execution Queue` has a matching task block.
    - Archived IDs are removed from open sections in the same change that moves their task blocks to `docs/todo_finished.md`.
    - Coverage snapshot tables reflect only currently open IDs.
  - stop_rule: If synchronization touches more than one logical workstream, split updates into focused queue-only follow-up tasks.
