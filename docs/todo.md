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
17. Keep the live execution queue short: no more than 8 leaf tasks in `Ready Now` at once. Additional dependency-blocked or deferred follow-up leaves may stay in the task blocks and `Immediate Next 10`, but only `Ready Now` counts as the live queue cap.
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

### Ready Now (Live Leaves; No Unmet TODO Dependencies)

- TODO-4025

### Immediate Next 10 (After Ready Now)

- none

### Priority Lanes (Current)

- Semantic ownership cutover: TODO-4025

### Execution Queue (Recommended)

1. TODO-4025

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | TODO-4025 |
| Validator entrypoint and benchmark-plumbing split | none |
| Semantic-product publication by module and fact family | none |
| IR lowerer compile-unit breakup | none |
| Backend validation/build ergonomics | none |
| Emitter/semantics map-helper parity | none |
| VM debug-session argv ownership | none |
| Debug trace replay robustness | none |
| VM/runtime debug numeric opcode parity | none |
| Test-suite audit follow-up and release-gate stability | none |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | TODO-4025 |
| Semantic-product publication parity and deterministic ordering | none |
| Lowerer/source-composition contract coverage | none |
| Focused backend rerun ergonomics and suite partitioning | none |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug numeric opcode behavior | none |
| Release benchmark/example suite stability and doctest governance | none |

### Task Blocks

- [ ] TODO-4025: Retire remaining statement-binding semantic fact threading
  - owner: ai
  - created_at: 2026-04-19
  - phase: Architecture Stabilization
  - depends_on: TODO-4024
  - scope: Remove the remaining production-path semantic fact threading after the direct query/`try(...)` lookup slice is in place, focusing on statement binding, result metadata, and lower-inference/setup helpers that still thread `SemanticProductTargetAdapter` for binding-fact-aware paths.
  - acceptance:
    - Production lowering no longer threads a broad `SemanticProductTargetAdapter` through the remaining statement-binding and result-metadata semantic-fact helpers.
    - Any retained semantic lookup adapter surface is either deleted or explicitly limited to test-only/non-production helpers with that boundary documented in source-lock coverage.
    - `docs/todo.md` and the coverage snapshot keep the remaining semantic adapter retirement work explicit instead of collapsing it back into hidden lowerer fallback cleanup.
  - stop_rule: Stop once the remaining production binding/result metadata paths no longer depend on `SemanticProductTargetAdapter`; if statement binding and result-metadata teardown still want different seams, split them into additional explicit leaves instead of widening this item further.
  - notes: Start in `src/ir_lowerer/IrLowererStatementBindingHelpers.cpp`, `src/ir_lowerer/IrLowererResultMetadataHelpers.cpp`, `src/ir_lowerer/IrLowererLowerInferenceFallbackSetup.cpp`, and the remaining lower-inference/setup helpers that still pass `SemanticProductTargetAdapter` for binding-fact-aware paths; the routing/callable-summary slice moved to finished `TODO-4018`, the binding/local-auto entry slice moved to finished `TODO-4020`, the definition-scoped return/`on_error` slice moved to finished `TODO-4022`, and the direct query/`try(...)` slice moved to finished `TODO-4024`.
