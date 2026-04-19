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

- TODO-1098

### Immediate Next 10 (After Ready Now)

- TODO-1098

### Priority Lanes (Current)

- Native soa_vector slash-method skip debt: TODO-1098

### Execution Queue (Recommended)

1. TODO-1098

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Native `soa_vector` slash-method skipped-test debt | TODO-1098 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Native compile-run skipped-test debt | TODO-1098 |

### Task Blocks

- [ ] TODO-1098: Audit native soa_vector get slash-method skip debt
  - owner: ai
  - created_at: 2026-04-19
  - phase: Backend skip-debt cleanup
  - scope: Re-enable or delete the skipped native canonical `soa_vector/get` slash-method coverage, and lock the current native backend contract with non-skipped assertions.
  - acceptance:
    - `tests/unit/test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp` no longer carries the skipped native canonical `soa_vector/get` slash-method case in stale form.
    - The native test locks the actual current contract for slash-method `get` on experimental `SoaVector`, whether that is a successful runtime path or a stable diagnostic.
  - stop_rule: Stop after the native canonical `soa_vector/get` slash-method surface is covered without `doctest::skip(true)` and any stale contradictory expectations are removed.
