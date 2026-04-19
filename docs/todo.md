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

- TODO-4015
- TODO-4012
- TODO-4011
- TODO-4010
- TODO-4009
- TODO-4002

### Immediate Next 10 (After Ready Now)

- TODO-4006
- TODO-4007
- TODO-4003
- TODO-4004
- TODO-4005
- TODO-4008

### Priority Lanes (Current)

- Test audit follow-ups: TODO-4015
- VM/runtime hardening: TODO-4012, TODO-4010, TODO-4009
- Emitter collection-helper parity: TODO-4011
- Semantic ownership cutover: TODO-4002, TODO-4004, TODO-4008
- Validator/publication simplification: TODO-4003, TODO-4005
- Build and validation ergonomics: TODO-4006, TODO-4007

### Execution Queue (Recommended)

1. TODO-4015
2. TODO-4012
3. TODO-4011
4. TODO-4010
5. TODO-4009
6. TODO-4006
7. TODO-4007
8. TODO-4003
9. TODO-4002
10. TODO-4004
11. TODO-4005
12. TODO-4008

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | TODO-4002, TODO-4004, TODO-4008 |
| Validator entrypoint and benchmark-plumbing split | TODO-4003 |
| Semantic-product publication by module and fact family | TODO-4005 |
| IR lowerer compile-unit breakup | TODO-4006 |
| Backend validation/build ergonomics | TODO-4007 |
| Emitter/semantics map-helper parity | TODO-4011 |
| VM debug-session argv ownership | TODO-4012 |
| Debug trace replay robustness | TODO-4010 |
| VM/runtime debug numeric opcode parity | TODO-4009 |
| Test-suite audit follow-up and release-gate stability | TODO-4015 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | TODO-4004, TODO-4008 |
| Semantic-product publication parity and deterministic ordering | TODO-4002, TODO-4005 |
| Lowerer/source-composition contract coverage | TODO-4006 |
| Focused backend rerun ergonomics and suite partitioning | TODO-4007 |
| Emitter map-helper canonicalization parity | TODO-4011 |
| VM debug-session argv lifetime coverage | TODO-4012 |
| Debug trace replay malformed-input coverage | TODO-4010 |
| Shared VM/debug numeric opcode behavior | TODO-4009 |
| Release benchmark/example suite stability and doctest governance | TODO-4015 |

### Task Blocks

- [ ] TODO-4015: Codify doctest size guardrails
  - owner: ai
  - created_at: 2026-04-19
  - phase: Test Audit Follow-up
  - depends_on: none
  - scope: Add explicit doctest size and runtime guardrails to the contributor workflow so oversized or slow doctest cases are called out consistently, including `AGENTS.md` guidance plus one repository-side check or lock that keeps those thresholds from drifting silently.
  - acceptance:
    - `AGENTS.md` explicitly states that doctest cases with more than 10 subcases or subtests should be split by default.
    - `AGENTS.md` explicitly states that doctest cases above 5 seconds with multiple subcases should be split and that single-focus tests above 5 seconds should be optimized or justified.
    - One repository-side check, source-lock, or architecture assertion keeps the documented thresholds visible in normal contributor validation instead of relying only on tribal memory.
  - stop_rule: Stop once the doctest split and optimization thresholds are both documented and anchored in normal contributor validation; do not widen this leaf into a broad test-style linter.
  - notes: The audit already confirmed release-first and parallel-test guidance are present, so this leaf should stay scoped to the missing size/runtime guardrails and their validation hook.

- [ ] TODO-4012: Make VmDebugSession own argv storage
  - owner: ai
  - created_at: 2026-04-19
  - phase: VM Runtime Hardening
  - depends_on: none
  - scope: Replace the non-owning `std::string_view` argv storage in `VmDebugSession` with owning storage so debug-trace, debug-json, and DAP sessions cannot retain dangling caller-backed argument text.
  - acceptance:
    - `VmDebugSession` stores argv text in owning storage, and any exposed or internal views point only into that owned storage for the lifetime of the session.
    - Focused VM debug-session coverage proves `PrintArgv` and `PrintArgvUnsafe` continue to work through debug-trace, debug-json, or adapter-driven execution after session startup.
    - Ownership comments and member naming in `include/primec/Vm.h` match the actual lifetime contract after the change.
  - stop_rule: Stop once debug-session argv handling no longer depends on caller-owned string storage; broader VM CLI argument refactors belong in a separate leaf.
  - notes: Start in `include/primec/Vm.h`, `src/VmDebugSessionSetup.cpp`, `src/VmIoHelpers.cpp`, and the debug entrypoints in `src/primevm_main.cpp` and `src/VmDebugAdapter.cpp`.

- [ ] TODO-4011: Unify emitter map-helper parity surface
  - owner: ai
  - created_at: 2026-04-19
  - phase: Emitter Parity
  - depends_on: none
  - scope: Extend emitter-side map helper resolution and canonicalization so alias/canonical map method calls and method-sugar resolution cover the same map helper family as semantics and IR lowering, including `*_ref` and `insert(_ref)` surfaces.
  - acceptance:
    - Emitter method resolution handles `count`, `count_ref`, `contains`, `contains_ref`, `tryAt`, `tryAt_ref`, `at`, `at_ref`, `at_unsafe`, `at_unsafe_ref`, `insert`, and `insert_ref` consistently for canonical and alias map paths, including method-sugar call sites that currently fall through to emitter-local subset logic.
    - Focused emitter metadata or compile-run coverage locks the covered map helper surfaces so emitter behavior matches semantics for the same helper family on direct helper calls and method-sugar entrypoints.
    - The covered map helper family is defined through a shared helper table or equivalent single-source policy instead of emitter-local subset logic.
  - stop_rule: Stop once emitter resolution no longer carries a smaller covered map-helper subset than semantics/IR-lowerer for the covered family; wider collection-helper unification beyond this slice should land as a follow-up.
  - notes: Start in `src/emitter/EmitterBuiltinMethodResolutionHelpers.cpp` and `src/emitter/EmitterBuiltinMethodResolutionMetadataHelpers.cpp`, with `src/semantics/SemanticsValidatorExprMethodResolution.cpp` and `src/ir_lowerer/IrLowererCountAccessClassifiers.cpp` as the parity reference.

- [ ] TODO-4010: Replace replay trace JSON string matching
  - owner: ai
  - created_at: 2026-04-19
  - phase: Debug Trace Robustness
  - depends_on: none
  - scope: Replace the ad hoc debug-trace replay JSON field extractors in `primevm_main.cpp` with the shared JSON parser and fail closed when checkpoint-capable trace lines are malformed.
  - acceptance:
    - Trace replay parses `event`, `sequence`, `snapshot`, `snapshot_payload`, and `reason` through the shared JSON parsing utilities instead of bespoke line-level string matching helpers.
    - Malformed checkpoint-capable trace lines report deterministic errors instead of being silently skipped or truncating replay state.
    - Focused trace replay coverage locks whitespace, escape, and malformed-input behavior for checkpoint parsing.
  - stop_rule: Stop once replay no longer depends on the current `extractJson*` helpers and malformed checkpoint-capable input fails deterministically.
  - notes: Start in `src/primevm_main.cpp` and reuse the parser in `src/VmDebugDapProtocol.cpp`; if the parser reuse fits cleanly but broader replay cleanup does not, keep this leaf scoped to checkpoint parsing only.

- [ ] TODO-4009: Unify VM/debug numeric opcode behavior
  - owner: ai
  - created_at: 2026-04-19
  - phase: VM Runtime Hardening
  - depends_on: none
  - scope: Delete the duplicated arithmetic/comparison/conversion opcode interpreter split between the normal VM executor and `VmDebugSession` so debug and non-debug VM execution share one numeric behavior path for results and faults.
  - acceptance:
    - Numeric opcode execution flows through one shared implementation used by both `VmExecution` and `VmDebugSession`, and the duplicate debug-only numeric opcode interpreter is removed.
    - Focused VM and debug-session coverage proves representative arithmetic, comparison, conversion, and fault cases produce the same observable behavior through normal and debug execution.
    - Debug-specific hook sequencing and state management remain outside the shared numeric semantics layer.
  - stop_rule: Stop once numeric opcode behavior is defined in one shared implementation for both execution paths and the duplicate numeric interpreter is gone; non-numeric opcode dedup belongs to a separate follow-up.
  - notes: Start in `src/VmExecutionNumeric.cpp`, `src/VmDebugSessionInstructionNumeric.cpp`, and the numeric dispatch call sites in `src/VmExecution.cpp` and `src/VmDebugSessionInstruction.cpp`.

- [ ] TODO-4008: Retire first covered production semantic adapter slice
  - owner: ai
  - created_at: 2026-04-19
  - phase: Architecture Stabilization
  - depends_on: TODO-4002, TODO-4004, TODO-4005
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

- [ ] TODO-4006: Extract first lowerer lanes from mega translation unit
  - owner: ai
  - created_at: 2026-04-19
  - phase: Lowerer Architecture
  - depends_on: none
  - scope: Break `src/ir_lowerer/IrLowererLower.cpp` include aggregation into normal compiled units for entry/setup plus the first additional lowering lane that fits cleanly in one implementation slice, starting with call lowering and then statement lowering only if it still fits.
  - acceptance:
    - Entry/setup and at least one additional named lowering lane build as ordinary `.cpp` compilation units rather than being re-included through `IrLowererLower.cpp`.
    - Lowerer interface coverage is updated to assert behavior/contracts instead of depending on the current source aggregation layout.
    - The full release gate passes after the decomposition.
  - stop_rule: Stop once entry/setup and one additional named lowering lane no longer depend on the current mega-aggregation; if another lane still remains, leave it as a follow-up instead of widening this leaf.
  - notes: Start from `src/ir_lowerer/IrLowererLower.cpp` and the source-lock tests that still read that file directly; take call lowering before statement lowering, and if both do not fit after entry/setup, write the remaining lane down as explicit follow-up work.

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
  - depends_on: TODO-4002
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
  - depends_on: none
  - scope: Replace ad hoc `takeCollected*ForSemanticProduct()` sweeps with one authoritative semantic-publication surface produced during validation and transferred into `SemanticProgram` for routing, callable-summary, binding/result, and graph-backed inference fact families.
  - acceptance:
    - `buildSemanticProgram(...)` consumes one structured publication surface rather than a mix of independent collector pulls for routing, callable-summary, binding/result, local-auto, query, `try(...)`, and `on_error` fact families.
    - Deterministic semantic-product ordering and lookup behavior remain locked by focused dump and adapter coverage.
    - Redundant production traversals across fact families are removed for the unified publication path.
  - stop_rule: Stop once semantic-product publication has one authoritative production input surface for the covered fact families and duplicate fact-family sweeps are removed from that path; if routing/callable-summary publication and graph-backed inference publication do not fit one change, split those clusters before code changes instead of widening the leaf.
  - notes: Primary implementation seams are `src/semantics/SemanticsValidate.cpp`, `src/semantics/SemanticsValidator.h`, and `src/semantics/SemanticsValidatorSnapshots.cpp`; retire the `takeCollected*ForSemanticProduct()` sweep pattern for the covered families rather than widening into later builder-layout cleanup.
