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

- TODO-4016
- TODO-4007
- TODO-4003
- TODO-4004
- TODO-4005

### Immediate Next 10 (After Ready Now)

- TODO-4008

### Priority Lanes (Current)

- Lowerer architecture: TODO-4016
- Semantic ownership cutover: TODO-4004, TODO-4008
- Validator/publication simplification: TODO-4003, TODO-4005
- Build and validation ergonomics: TODO-4007

### Execution Queue (Recommended)

1. TODO-4016
2. TODO-4007
3. TODO-4003
4. TODO-4004
5. TODO-4005
6. TODO-4008

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | TODO-4004, TODO-4008 |
| Validator entrypoint and benchmark-plumbing split | TODO-4003 |
| Semantic-product publication by module and fact family | TODO-4005 |
| IR lowerer compile-unit breakup | TODO-4016 |
| Backend validation/build ergonomics | TODO-4007 |
| Emitter/semantics map-helper parity | none |
| VM debug-session argv ownership | none |
| Debug trace replay robustness | none |
| VM/runtime debug numeric opcode parity | none |
| Test-suite audit follow-up and release-gate stability | none |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | TODO-4004, TODO-4008 |
| Semantic-product publication parity and deterministic ordering | TODO-4005 |
| Lowerer/source-composition contract coverage | TODO-4016 |
| Focused backend rerun ergonomics and suite partitioning | TODO-4007 |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug numeric opcode behavior | none |
| Release benchmark/example suite stability and doctest governance | none |

### Task Blocks

- [ ] TODO-4016: Extract lowerer return/emit lane from mega translation unit
  - owner: ai
  - created_at: 2026-04-19
  - phase: Lowerer Architecture
  - depends_on: none
  - scope: Replace the remaining `IrLowererLowerReturnAndCalls.h` include ownership in `IrLowererLower.cpp` with an ordinary compiled orchestration unit for return-info, inline-call, and expression-emission setup while preserving the current step-helper split and inline-call bookkeeping behavior.
  - acceptance:
    - `IrLowererLower.cpp` no longer includes `IrLowererLowerReturnAndCalls.h` directly; the return/emit lane builds through a named `.cpp` orchestration unit.
    - Existing return-info, inline-call, and expression-emission step helpers remain the detailed behavior owners, with source-lock coverage pinning the new orchestration seam instead of the old include body.
    - Release validation and focused lowerer architecture coverage pass after the lane extraction.
  - stop_rule: Stop once the return/emit lane is compiled as a normal `.cpp` orchestration unit and the old include-owner seam is gone; leave operator or binding/loop lane breakup as separate follow-up work.
  - notes: Start with `src/ir_lowerer/IrLowererLowerReturnAndCalls.h`, `src/ir_lowerer/IrLowererLowerReturnInfo.h`, `src/ir_lowerer/IrLowererLowerInlineCalls.h`, and `src/ir_lowerer/IrLowererLowerEmitExpr.h`; keep the existing step-unit boundaries intact and avoid widening into operator or statement-lane rewrites.

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

- [ ] TODO-4007: Shard backend tests into focused binaries
  - owner: ai
  - created_at: 2026-04-19
  - phase: Build Ergonomics
  - depends_on: none
  - scope: Break `PrimeStruct_backend_tests` into narrower doctest binaries that separate IR-lowering contract coverage, backend-registry/runtime adapter coverage, and heavy compile-run suites.
  - acceptance:
    - CMake defines narrower backend-oriented test targets with the current backend coverage redistributed without losing suite labels or helper behavior.
    - Focused reruns can exercise IR-lowering or registry/runtime validation without rebuilding the entire current backend test surface.
    - Release validation and direct test-binary reruns still resolve `primec` and related helpers correctly.
  - stop_rule: Stop once ordinary focused backend reruns no longer depend on the current monolithic backend test binary for IR-lowering, registry/runtime adapter, or compile-run coverage.
  - notes: Start in `CMakeLists.txt` around `PrimeStruct_backend_tests` plus the architecture assertions under `tests/unit/test_ir_pipeline_backends_architecture.h`; if one pass cannot land the three-way split cleanly, split this leaf by target family rather than by random file groups.

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

- [ ] TODO-4004: Fail closed on missing semantic-product facts
  - owner: ai
  - created_at: 2026-04-19
  - phase: Semantic Ownership Cutover
  - depends_on: none
  - scope: Make production lowering fail closed when required semantic-product facts are missing instead of consulting validator-owned or AST-rederived semantic metadata.
  - acceptance:
    - Focused lowerer and `prepareIrModule` coverage proves the covered direct-call, method-call, local-auto, query, `try(...)`, and `on_error` families fail when published semantic-product facts are missing.
    - Production lowerer helpers read published semantic-product data only for the covered families.
    - Remaining compatibility fallbacks are deleted or explicitly limited to non-production fixture paths.
  - stop_rule: Stop once semantic-product presence fully disables production semantic fallback behavior for the covered lowering families.
  - notes: Primary verification seams are `prepareIrModule(...)`, `IrLowerer::lower(...)`, and the backend-registry / graph-context conformance tests that already pin semantic-product entry contracts.

- [ ] TODO-4003: Introduce production validate config and benchmark observer
  - owner: ai
  - created_at: 2026-04-19
  - phase: Validator Simplification
  - depends_on: none
  - scope: Separate the production `Semantics::validate` contract from benchmark instrumentation and memory/telemetry plumbing by introducing a narrower production-facing config surface plus a dedicated benchmark observer/config path.
  - acceptance:
    - Production callers use a narrower validation entrypoint or config surface than the current benchmark-heavy signature, and benchmark-only switches no longer appear directly on that production surface.
    - Allocation, RSS, semantic fact, and repeat-run benchmark reporting still work through a dedicated observer/config path with unchanged benchmark-harness behavior.
    - Compile-pipeline and benchmark harness coverage both pass after the split.
  - stop_rule: Stop once benchmark-only behavior is isolated from the normal production validation flow at the API/call-plumbing boundary; do not expand this leaf into unrelated validator-internal cleanup.
  - notes: Start with `include/primec/Semantics.h`, `src/CompilePipeline.cpp`, and the benchmark harness callers; if the API split is easy but observer plumbing is not, keep the API split in this leaf and create a follow-up for deeper benchmark internals.
