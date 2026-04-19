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

- TODO-4005

### Immediate Next 10 (After Ready Now)

- TODO-4008

### Priority Lanes (Current)

- Semantic ownership cutover: TODO-4008
- Semantic-product publication shaping: TODO-4005

### Execution Queue (Recommended)

1. TODO-4005
2. TODO-4008

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | TODO-4008 |
| Validator entrypoint and benchmark-plumbing split | none |
| Semantic-product publication by module and fact family | TODO-4005 |
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
| Semantic-product-authority conformance | TODO-4008 |
| Semantic-product publication parity and deterministic ordering | TODO-4005 |
| Lowerer/source-composition contract coverage | none |
| Focused backend rerun ergonomics and suite partitioning | none |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug numeric opcode behavior | none |
| Release benchmark/example suite stability and doctest governance | none |

### Task Blocks

- [ ] TODO-4008: Retire first covered production semantic adapter slice
  - owner: ai
  - created_at: 2026-04-19
  - phase: Architecture Stabilization
  - depends_on: TODO-4004, TODO-4005
  - scope: Delete one production-path temporary semantic-product adapter slice and its AST-dependent lowerer re-derivations for a fact family already made authoritative by TODO-4004 and TODO-4005, then refresh the open architecture queue so any remaining adapter-retirement slices are called out explicitly instead of hiding behind this leaf.
  - acceptance:
    - One concrete covered adapter/fact-family slice is deleted or explicitly limited to non-production-only fixtures.
    - `primec`, `primevm`, and compile-pipeline production entrypoints consume the semantic product without hidden validator-owned side channels for that retired slice.
    - `docs/todo.md` is refreshed so any still-open adapter retirement work outside the retired slice remains as explicit follow-up instead of staying implicit.
  - stop_rule: Stop once one covered production adapter slice is retired end to end and any remaining slices are written down as separate follow-up work; do not use this leaf as an umbrella cleanup.
  - notes: Primary seams are `src/IrPreparation.cpp`, `src/primevm_main.cpp`, `src/main.cpp`, and the remaining adapter/fallback helpers under `src/ir_lowerer/`; choose one fact-family slice first and only keep neighboring retirements together when they share the same production entrypoints.

- [ ] TODO-4005: Split semantic publication into scoped builders
  - owner: ai
  - created_at: 2026-04-19
  - phase: Semantic Product Publication
  - depends_on: none
  - scope: Reorganize semantic-product publication around definition/module-scoped builders so fact publication, lookup indexing, and transient-cache release happen in smaller ownership-bounded stages.
  - acceptance:
    - Publication code is split into dedicated builder units by fact family or module scope instead of one broad `buildSemanticProgram(...)` sweep.
    - Module artifact population and fact ordering remain deterministic while avoiding repeated whole-program traversals where equivalent scoped data already exists.
    - Release validation and semantic-product dump coverage pass unchanged except for intentional contract updates.
  - stop_rule: Stop once semantic-product publication is partitioned into smaller builders with clear ownership and no new broad whole-program collector sweeps are introduced.
  - notes: This starts after TODO-4002 lands and should focus on `src/semantics/SemanticsValidate.cpp`; keep routing/callable-summary publication and graph-backed inference publication as the first candidate split if the builder breakup does not fit one commit.
