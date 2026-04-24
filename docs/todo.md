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

- TODO-4132

### Immediate Next 10 (After Ready Now)

- TODO-4133
- TODO-4134
- TODO-4130
- TODO-4140
- TODO-4141
- TODO-4142
- TODO-4128

### Priority Lanes (Current)

- Semantic-product authority and lowerer ownership: TODO-4132, TODO-4133,
  TODO-4134
- Pipeline/publication boundary hardening: TODO-4140, TODO-4141
- Validator/runtime boundary simplification: TODO-4140
- Test-surface contraction: TODO-4142
- Runtime execution unification: TODO-4130

### Execution Queue (Recommended)

1. TODO-4132
2. TODO-4133
3. TODO-4134
4. TODO-4130
5. TODO-4140

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | none |
| Compile-pipeline stage and publication-boundary contracts | TODO-4141 |
| Stdlib surface-style alignment and public helper readability | none |
| Stdlib bridge consolidation and collection/file/gfx surface authority | none |
| Vector/map stdlib ownership cutover and collection surface authority | none |
| Stdlib de-experimentalization and public/internal namespace cleanup | none |
| Validator entrypoint and benchmark-plumbing split | TODO-4140 |
| Semantic-product publication by module and fact family | none |
| IR lowerer compile-unit breakup | TODO-4132, TODO-4133, TODO-4134 |
| Backend validation/build ergonomics | none |
| Emitter/semantics map-helper parity | none |
| VM debug-session argv ownership | none |
| Debug trace replay robustness | none |
| VM/runtime debug stateful opcode parity | TODO-4130 |
| Test-suite audit follow-up and release-gate stability | TODO-4142 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | TODO-4141 |
| CodeExamples-aligned stdlib surface syntax conformance | none |
| Compile-pipeline stage handoff conformance | none |
| Semantic-product publication parity and deterministic ordering | none |
| Lowerer/source-composition contract coverage | TODO-4132, TODO-4133, TODO-4134 |
| Vector/map bridge parity for imports, rewrites, and lowering | none |
| De-experimentalization surface and namespace parity | none |
| Focused backend rerun ergonomics and suite partitioning | none |
| Architecture contract probe migration | TODO-4142 |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug stateful opcode behavior | TODO-4130 |
| Release benchmark/example suite stability and doctest governance | none |

### Vector/Map Bridge Contract Summary

- Bridge-owned public contract: exact and wildcard `/std/collections` imports,
  `vector<T>` / `map<K, V>` constructor and literal-rewrite surfaces, helper
  families, compatibility spellings plus removed-helper diagnostics, semantic
  surface IDs, and lowerer dispatch metadata.
- Migration-only seams: rooted `/vector/*` and `/map/*` spellings,
  `vectorCount` / `mapCount`-style lowering names, and
  `/std/collections/experimental_*` implementation modules stay temporary until
  the later cutover TODOs retire them.
- Outside this lane: `array<T>` core ownership, `soa_vector<T>` maturity, and
  runtime/storage redesign remain separate boundaries and should not be folded
  into the vector/map bridge tasks below.

### Stdlib De-Experimentalization Policy Summary

- Canonical public API: non-`experimental` namespaces are the intended
  long-term user-facing contracts.
- Canonical collection contract: `/std/collections/vector/*` and
  `/std/collections/map/*` are the sole public vector/map collection surfaces.
- Temporary compatibility namespaces:
  `/std/collections/experimental_soa_vector/*`, and
  `/std/collections/experimental_soa_vector_conversions/*` stay transitional
  until their explicit shim, rename, or maturity TODOs land.
- Internal collection implementation modules:
  `/std/collections/experimental_vector/*` and
  `/std/collections/experimental_map/*` now remain implementation-owned seams
  behind canonical `/std/collections/vector/*` and `/std/collections/map/*`,
  with direct imports kept only for targeted compatibility or conformance
  coverage.
- Legacy gfx compatibility seam: `/std/gfx/experimental/*` remains importable
  only for targeted compatibility coverage and staged migration support;
  canonical `/std/gfx/*` is the only public gfx namespace.
- Internal substrate/helper namespaces:
  `/std/collections/internal_buffer_checked/*`,
  `/std/collections/internal_buffer_unchecked/*`, and
  `/std/collections/internal_soa_storage/*` are implementation-facing
  plumbing rather than public API.
- Default rule: no `experimental` namespace is canonical public API unless the
  docs call out a temporary incubating exception explicitly.

### SoA Maturity Track Summary

- `soa_vector<T>` remains an incubating public extension instead of joining the
  already-promoted vector/map public contract.
- Canonical experiment spellings for current docs/examples are
  `/std/collections/soa_vector/*` and
  `/std/collections/soa_vector_conversions/*`.
- Compatibility-only namespaces:
  `/std/collections/experimental_soa_vector/*` and
  `/std/collections/experimental_soa_vector_conversions/*` remain bridge seams
  behind the canonical experiment surface.
- Internal substrate namespace:
  `/std/collections/internal_soa_storage/*` remains implementation-facing
  SoA storage/layout plumbing.
- Promotion requires borrowed-view/lifetime rules, backend/runtime parity, and
  retirement of direct experimental implementation imports before `soa_vector`
  can be treated as a promoted public contract.

### Skipped Doctest Debt Summary

- Retained `doctest::skip(true)` coverage is currently absent from the active
  queue because no skipped doctest cases remain under `tests/unit`.
- New skipped doctest coverage must create a new explicit TODO before it lands.
- The success condition for each lane is re-enable-or-delete; indefinite
  skipped coverage is not a stable end state.

### Task Blocks

- [ ] TODO-4142: Retire brittle architecture source locks after contract migration
  - owner: ai
  - created_at: 2026-04-24
  - phase: Testing API Contraction
  - depends_on: none
  - scope: Replace the remaining architecture/doc/file-placement source-lock
      tests with narrower contract probes once the helper-export contraction
      work has landed, so structural cleanup no longer requires large batches of
      string-literal test churn.
  - acceptance:
    - At least one remaining architecture source-lock cluster moves off direct
      file-path or exact-doc-string assertions onto a contract-oriented helper
      or dump/conformance probe.
    - The migrated coverage still proves the intended architectural boundary
      without depending on internal file placement.
    - Release validation continues to catch real architectural regressions after
      the migrated source-lock cluster is removed or narrowed.
  - stop_rule: Stop once one meaningful architecture source-lock cluster is
      replaced by contract probes with equivalent regression signal.

- [ ] TODO-4141: Freeze published semantic-product storage
  - owner: ai
  - created_at: 2026-04-24
  - phase: Semantic Ownership Boundary
  - depends_on: none
  - scope: Add one explicit post-build freeze boundary for
      `SemanticProgram` publication so downstream lowering and reporting code
      consume immutable published storage instead of treating the build artifact
      as a mutable extension surface.
  - acceptance:
    - The semantic-product builder finishes through one explicit freeze step
      before the published artifact leaves semantics.
    - Downstream production consumers no longer mutate or rely on mutating the
      published semantic-product storage for the migrated slice.
    - Conformance or source-lock coverage pins the new immutable-publication
      contract for the migrated semantic-product slice.
  - stop_rule: Stop once one published semantic-product slice crosses an
      explicit immutable freeze boundary with coverage.

- [ ] TODO-4140: Split benchmark semantic validation from the production API
  - owner: ai
  - created_at: 2026-04-24
  - phase: Validator Boundary Simplification
  - depends_on: none
  - scope: Move benchmark-only semantic validation knobs and observers out of
      the primary `Semantics` production entry contract into a dedicated harness
      or benchmark-facing entrypoint, while preserving the existing measurement
      coverage.
  - acceptance:
    - The main production `Semantics` validation contract no longer exposes at
      least one benchmark-only configuration or observer surface.
    - Benchmark runs still support the migrated measurement surface through a
      dedicated benchmark-facing entrypoint or harness.
    - Focused validation and benchmark coverage prove that production and
      benchmark semantic publication still agree on observable results.
  - stop_rule: Stop once one benchmark-only semantic configuration slice is
      removed from the production-facing API and preserved in benchmark
      coverage.

- [ ] TODO-4134: Extract representative GPU lowering contract family
  - owner: ai
  - created_at: 2026-04-24
  - phase: Semantic Ownership Boundary
  - depends_on: none
  - scope: Move one GPU-only lowering contract family out of the shared
      backend fallback helpers into an explicit GPU-scoped module with clear
      inputs, validation boundaries, and focused conformance coverage.
  - acceptance:
    - One representative GPU lowering contract family no longer routes through
      shared backend-specific fallback branches.
    - The migrated behavior is owned by an explicit GPU-scoped lowering module
      with clear interfaces.
    - Focused conformance or source-lock coverage pins the migrated GPU
      contract.
  - stop_rule: Stop once one representative GPU lowering family is explicit
      backend-scoped code and the shared fallback branch for that family is
      deleted or reduced to backend-agnostic glue.

- [ ] TODO-4133: Extract representative native lowering contract family
  - owner: ai
  - created_at: 2026-04-24
  - phase: Semantic Ownership Boundary
  - depends_on: none
  - scope: Move one native-only lowering contract family out of the shared
      backend fallback helpers into an explicit native-scoped module with
      clear inputs, validation boundaries, and focused conformance coverage.
  - acceptance:
    - One representative native lowering contract family no longer routes
      through shared backend-specific fallback branches.
    - The migrated behavior is owned by an explicit native-scoped lowering
      module with clear interfaces.
    - Focused conformance or source-lock coverage pins the migrated native
      contract.
  - stop_rule: Stop once one representative native lowering family is
      explicit backend-scoped code and the shared fallback branch for that
      family is deleted or reduced to backend-agnostic glue.

- [ ] TODO-4132: Extract representative VM lowering contract family
  - owner: ai
  - created_at: 2026-04-24
  - phase: Semantic Ownership Boundary
  - depends_on: none
  - scope: Move one VM-only lowering contract family out of the shared
      backend fallback helpers into an explicit VM-scoped module with clear
      inputs, validation boundaries, and focused conformance coverage.
  - acceptance:
    - One representative VM lowering contract family no longer routes through
      shared backend-specific fallback branches.
    - The migrated behavior is owned by an explicit VM-scoped lowering module
      with clear interfaces.
    - Focused conformance or source-lock coverage pins the migrated VM
      contract.
  - stop_rule: Stop once one representative VM lowering family is explicit
      backend-scoped code and the shared fallback branch for that family is
      deleted or reduced to backend-agnostic glue.

- [ ] TODO-4130: Unify VM and debug-session interpreter cores
  - owner: ai
  - created_at: 2026-04-20
  - phase: Runtime Architecture
  - depends_on: none
  - scope: Consolidate the still-duplicated stateful opcode execution logic in
      `VmExecution.cpp` and `VmDebugSessionInstruction.cpp` behind one shared
      interpreter-step core, while keeping the already-shared numeric opcode
      helpers and layering debug hooks or session-state transitions around the
      shared step path.
  - acceptance:
    - The selected stateful opcode slice no longer has separate production and
      debug-session implementations.
    - VM execution and debug-session stepping share one interpreter-step core
      for the migrated stateful slice.
    - Parity coverage proves identical execution behavior between normal VM and
      debug stepping for the migrated stateful slice.
  - stop_rule: Stop once one representative stateful interpreter slice runs
      through a shared VM/debug execution core with parity coverage.

- [ ] TODO-4128: Split lowering contracts by backend surface
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: TODO-4132, TODO-4133, TODO-4134
  - scope: Track the VM, native, and GPU backend-surface lowering split as
      explicit child tasks so each backend contract family can land as one
      coherent change instead of one oversized mixed-backend refactor.
  - acceptance:
    - TODO-4132, TODO-4133, and TODO-4134 are completed.
    - Shared lowerer helpers retain only backend-agnostic orchestration for
      the migrated VM/native/GPU families.
    - Focused conformance or source-lock coverage exists for each migrated
      backend family.
  - stop_rule: Stop once the backend-specific lowering work is complete
      through TODO-4132, TODO-4133, and TODO-4134.
  - notes: Split on 2026-04-24 because the previous mixed VM/native/GPU leaf
      was too broad for one confident implementation slice.
