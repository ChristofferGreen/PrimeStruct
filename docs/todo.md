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

- TODO-4001
- TODO-4003
- TODO-4006
- TODO-4007

### Immediate Next 10 (After Ready Now)

- TODO-4002
- TODO-4004
- TODO-4005
- TODO-4008

### Priority Lanes (Current)

- Semantic ownership cutover: TODO-4001, TODO-4002, TODO-4004, TODO-4008
- Validator/publication simplification: TODO-4003, TODO-4005
- Build and validation ergonomics: TODO-4006, TODO-4007

### Execution Queue (Recommended)

1. TODO-4001
2. TODO-4003
3. TODO-4002
4. TODO-4004
5. TODO-4005
6. TODO-4006
7. TODO-4007
8. TODO-4008

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | TODO-4001, TODO-4002, TODO-4004, TODO-4008 |
| Validator entrypoint and benchmark-plumbing split | TODO-4003 |
| Semantic-product publication by module and fact family | TODO-4005 |
| IR lowerer compile-unit breakup | TODO-4006 |
| Backend validation/build ergonomics | TODO-4007 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | TODO-4004, TODO-4008 |
| Semantic-product publication parity and deterministic ordering | TODO-4002, TODO-4005 |
| Lowerer/source-composition contract coverage | TODO-4006 |
| Focused backend rerun ergonomics and suite partitioning | TODO-4007 |

### Task Blocks

- [ ] TODO-4008: Retire covered production semantic adapters
  - owner: ai
  - created_at: 2026-04-19
  - phase: Architecture Stabilization
  - depends_on: TODO-4002, TODO-4004, TODO-4005
  - scope: Delete the remaining production-path temporary semantic-product adapters and AST-dependent lowerer re-derivations for the fact families already made authoritative by TODO-4004 and TODO-4005, then refresh the open architecture queue around the post-cutover baseline.
  - acceptance:
    - The temporary adapter and fallback branches that still serve the fact families covered by TODO-4004 and TODO-4005 are deleted or explicitly limited to non-production-only fixtures.
    - `primec`, `primevm`, and compile-pipeline production entrypoints consume the semantic product without hidden validator-owned side channels for those covered fact families.
    - `docs/todo.md` is refreshed so any still-open adapter retirement work outside the covered slice remains as explicit follow-up instead of staying implicit.
  - stop_rule: Stop once transitional production lowering adapters are no longer part of the normal compile/runtime path for the covered fact families; if uncovered adapter families remain, leave them as separate follow-up TODOs instead of widening this leaf further.
  - notes: Primary seams are `src/IrPreparation.cpp`, `src/primevm_main.cpp`, `src/main.cpp`, and the remaining adapter/fallback helpers under `src/ir_lowerer/`; if retirements naturally separate by fact family, split them into explicit follow-up leaves instead of keeping one umbrella cleanup.

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

- [ ] TODO-4006: Extract lowerer lanes from mega translation unit
  - owner: ai
  - created_at: 2026-04-19
  - phase: Lowerer Architecture
  - depends_on: none
  - scope: Break `src/ir_lowerer/IrLowererLower.cpp` include aggregation into normal compiled units for at least entry/setup, call lowering, and statement lowering lanes.
  - acceptance:
    - The extracted lanes build as ordinary `.cpp` compilation units rather than being re-included through `IrLowererLower.cpp`.
    - Lowerer interface coverage is updated to assert behavior/contracts instead of depending on the current source aggregation layout.
    - The full release gate passes after the decomposition.
  - stop_rule: Stop once the lowerer no longer depends on the current mega-aggregation for the extracted lanes.
  - notes: Start from `src/ir_lowerer/IrLowererLower.cpp` and the source-lock tests that still read that file directly; if one commit cannot cover entry/setup, call lowering, and statement lowering together, split by lane in that order.

- [ ] TODO-4005: Split semantic publication into scoped builders
  - owner: ai
  - created_at: 2026-04-19
  - phase: Semantic Product Publication
  - depends_on: TODO-4002
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
  - depends_on: TODO-4001, TODO-4002
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

- [ ] TODO-4002: Introduce unified semantic publication surface
  - owner: ai
  - created_at: 2026-04-19
  - phase: Semantic Product Publication
  - depends_on: TODO-4001
  - scope: Replace ad hoc `takeCollected*ForSemanticProduct()` sweeps with one authoritative semantic-publication surface produced during validation and transferred into `SemanticProgram` for routing, callable-summary, binding/result, and graph-backed inference fact families.
  - acceptance:
    - `buildSemanticProgram(...)` consumes one structured publication surface rather than a mix of independent collector pulls for routing, callable-summary, binding/result, local-auto, query, `try(...)`, and `on_error` fact families.
    - Deterministic semantic-product ordering and lookup behavior remain locked by focused dump and adapter coverage.
    - Redundant production traversals across fact families are removed for the unified publication path.
  - stop_rule: Stop once semantic-product publication has one authoritative production input surface for the covered fact families and duplicate fact-family sweeps are removed from that path; builder decomposition beyond that belongs to TODO-4005.
  - notes: Primary implementation seams are `src/semantics/SemanticsValidate.cpp`, `src/semantics/SemanticsValidator.h`, and `src/semantics/SemanticsValidatorSnapshots.cpp`; retire the `takeCollected*ForSemanticProduct()` sweep pattern for the covered families rather than widening into later builder-layout cleanup.

- [ ] TODO-4001: ADR ownership contract and delete production shadow state
  - owner: ai
  - created_at: 2026-04-19
  - phase: Semantic Ownership Cutover
  - depends_on: none
  - scope: Write an ADR that defines the authoritative ownership boundary between the type-resolution graph, validator scratch state, and semantic product, then use that contract to delete or quarantine production-path graph-local-auto legacy shadow state.
  - acceptance:
    - A new ADR documents the production owner for each covered semantic fact family plus any benchmark-only exceptions.
    - The default production validator path no longer populates or reads the graph-local-auto legacy shadow maps for the covered local-auto/query/try/direct-call/method-call side-channel families.
    - Release validation and focused semantic-product coverage pass with unchanged user-visible behavior.
  - stop_rule: Stop once the ownership contract is documented and the covered production shadow-state subsystem has been deleted or explicitly isolated from production execution.
  - notes: The deletion target is the graph-local-auto legacy shadow cluster in `src/semantics/SemanticsValidator.h`, `src/semantics/SemanticsValidatorBuild.cpp`, `src/semantics/SemanticsValidatorInferGraph.cpp`, and `src/semantics/SemanticsValidatorSnapshotLocals.cpp`; if benchmark-only remnants must survive, they should be explicitly gated and named as such in the ADR and code.
