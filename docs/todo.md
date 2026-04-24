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

- TODO-4124
- TODO-4125
- TODO-4126

### Immediate Next 10 (After Ready Now)

- TODO-4135
- TODO-4136
- TODO-4137
- TODO-4138
- TODO-4139
- TODO-4132
- TODO-4133
- TODO-4134
- TODO-4130
- TODO-4131

### Priority Lanes (Current)

- Semantic-product authority and lowerer ownership: TODO-4137, TODO-4138,
  TODO-4132, TODO-4133, TODO-4134
- Pipeline/publication boundary hardening: TODO-4139, TODO-4140, TODO-4141
- Validator/runtime boundary simplification: TODO-4126, TODO-4135,
  TODO-4136, TODO-4140
- Test-surface contraction: TODO-4124, TODO-4125, TODO-4142
- Runtime execution unification: TODO-4130
- Parallel semantic publication determinism: TODO-4135, TODO-4136,
  TODO-4131

### Execution Queue (Recommended)

1. TODO-4124
2. TODO-4125
3. TODO-4126
4. TODO-4135
5. TODO-4136

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | TODO-4126, TODO-4135, TODO-4136, TODO-4137, TODO-4138, TODO-4131 |
| Compile-pipeline stage and publication-boundary contracts | TODO-4139, TODO-4141 |
| Stdlib surface-style alignment and public helper readability | none |
| Stdlib bridge consolidation and collection/file/gfx surface authority | none |
| Vector/map stdlib ownership cutover and collection surface authority | none |
| Stdlib de-experimentalization and public/internal namespace cleanup | none |
| Validator entrypoint and benchmark-plumbing split | TODO-4135, TODO-4136, TODO-4140 |
| Semantic-product publication by module and fact family | TODO-4135, TODO-4136 |
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
| Compile-pipeline stage handoff conformance | TODO-4139 |
| Semantic-product publication parity and deterministic ordering | TODO-4135, TODO-4136, TODO-4137, TODO-4138, TODO-4131 |
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
  - depends_on: TODO-4124, TODO-4125
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

- [ ] TODO-4139: Split `CompilePipeline` into explicit stage handoffs
  - owner: ai
  - created_at: 2026-04-24
  - phase: Pipeline Boundary Hardening
  - depends_on: none
  - scope: Break the current `runCompilePipeline(...)` orchestration into
      explicit stage handoff objects or stage runners for import, transform,
      parse, semantics, and dump/report branching, so the pipeline boundary is
      clearer without introducing a new compiler IR layer.
  - acceptance:
    - At least one meaningful compile-pipeline stage boundary is represented by
      an explicit typed handoff object or stage runner instead of implicit
      shared local state inside `runCompilePipeline(...)`.
    - The migrated stage preserves current dump behavior and failure staging for
      the covered path.
    - Contract or conformance coverage pins the new stage handoff without
      regressing user-visible compile-pipeline behavior.
  - stop_rule: Stop once one substantial compile-pipeline stage boundary uses an
      explicit handoff object or stage runner with preserved coverage.

- [ ] TODO-4138: Publish `ResolvedModule` ordering by import order
  - owner: ai
  - created_at: 2026-04-24
  - phase: Semantic Ownership Boundary
  - depends_on: TODO-4137
  - scope: Replace the current lexical `moduleKey` resort in
      `finalizeSemanticModuleArtifacts(...)` with deterministic import-order
      publication once `ResolvedModule` identity is already aligned to parsed
      or imported source units.
  - acceptance:
    - Published `SemanticProgram.moduleResolvedArtifacts` ordering follows
      deterministic source-import order instead of lexical `moduleKey`
      ordering.
    - `stableOrder` reflects the published import-order contract rather than
      the current lexical regrouping pass.
    - Semantic-product dump or conformance coverage pins the new module
      ordering contract.
  - stop_rule: Stop once module publication order matches the documented
      deterministic import-order contract with coverage.

- [ ] TODO-4137: Publish `ResolvedModule` identity by source unit
  - owner: ai
  - created_at: 2026-04-24
  - phase: Semantic Ownership Boundary
  - depends_on: none
  - scope: Replace the current top-level-path-prefix `moduleKey` grouping in
      `ensureModuleResolvedArtifacts(...)` with identity keyed to parsed or
      imported source units, without yet changing the later ordering pass.
  - acceptance:
    - `SemanticProgram.moduleResolvedArtifacts` groups facts by parsed or
      imported source unit rather than only by top-level path prefix.
    - Published module identity no longer depends on `semanticModuleKeyForPath`
      style top-level-path collapsing for the migrated surface.
    - Semantic-product dump or conformance coverage pins the new module
      identity contract.
  - stop_rule: Stop once published resolved modules use source-unit identity
      with coverage, even if the later ordering pass is still unchanged.

- [ ] TODO-4136: Publish fact-family indices from deterministic worker merges
  - owner: ai
  - created_at: 2026-04-24
  - phase: Semantic Ownership Boundary
  - depends_on: TODO-4135
  - scope: Extend production worker publication from routing and callable
      metadata into one representative semantic fact-family slice in
      `publishedRoutingLookups`, with deterministic single-writer merges for
      both the published indices and the consumer-visible semantic-product
      view.
  - acceptance:
    - One representative fact-family slice (`onError`, `localAuto`, `query`,
      or `try`) publishes through deterministic worker-local merges in the
      production semantic-product path.
    - The migrated fact-family indices and their consumer-visible semantic-
      product view are identical for worker counts `1`, `2`, and `4`.
    - Parity coverage pins semantic-product output and diagnostics for the
      migrated fact-family slice across the supported worker counts.
  - stop_rule: Stop once one representative fact-family slice uses
      deterministic production worker merges with parity coverage.

- [ ] TODO-4135: Publish routing metadata and callable summaries from deterministic worker merges
  - owner: ai
  - created_at: 2026-04-24
  - phase: Semantic Ownership Boundary
  - depends_on: none
  - scope: Extend the parallel definition-validation path from benchmark-only
      worker chunks into one production publication path for routing metadata
      and callable summaries, with deterministic single-writer merges for the
      published semantic-product surface.
  - acceptance:
    - One production publication slice emits deterministic merged routing
      metadata or callable summaries from worker-local validation outputs.
    - Worker-count `1`, `2`, and `4` runs produce identical published routing
      output for the migrated slice.
    - Parity coverage pins semantic-product output and diagnostics for the
      migrated routing or callable-summary slice across the supported worker
      counts.
  - stop_rule: Stop once one routing or callable-summary publication slice
      uses deterministic worker merges in production with parity coverage.

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

- [ ] TODO-4131: Implement origin-key worker interner merges
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: TODO-4136
  - scope: Replace the current worker-id and local-order symbol-interner merge
      helper with the documented semantic-origin-key merge policy once
      production worker-fact publication depends on merged worker symbol
      tables.
  - acceptance:
    - Worker-local interner snapshots carry the earliest semantic origin key
      needed by the documented merge contract.
    - Global merged symbol ids are assigned by origin-key policy rather than
      worker-id and local-order only.
    - Worker-count and repeated-run parity coverage pins identical merged
      symbol-id views for the production publication path.
  - stop_rule: Stop once production worker symbol merges use origin-key
      ordering and parity coverage proves stability across worker counts and
      repeated runs.

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

- [ ] TODO-4129: Align `ResolvedModule` publication with import-order contract
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: TODO-4137, TODO-4138
  - scope: Track the `ResolvedModule` publication rollout by first aligning
      module identity to parsed or imported source units and then aligning the
      published module ordering to deterministic import order.
  - acceptance:
    - TODO-4137 and TODO-4138 are completed.
    - Published resolved modules match the documented source-unit identity and
      deterministic import-order contract.
    - Semantic-product dump or conformance coverage pins the new module
      identity and ordering contract.
  - stop_rule: Stop once the `ResolvedModule` publication rollout is complete
      through TODO-4137 and TODO-4138.
  - notes: Split on 2026-04-24 after deeper code review showed that module
      identity is created in `ensureModuleResolvedArtifacts(...)` while module
      ordering is imposed later in `finalizeSemanticModuleArtifacts(...)`,
      making them separate shippable seams.

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

- [ ] TODO-4127: Track deterministic worker publication rollout
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: TODO-4135, TODO-4136
  - scope: Track the rollout from benchmark-oriented worker validation into
      production semantic-product publication by landing the routing and
      callable slice first, then a representative semantic fact-family slice,
      with deterministic worker-local merges at each boundary.
  - acceptance:
    - TODO-4135 and TODO-4136 are completed.
    - Production worker publication covers at least one routing or callable
      slice and one semantic fact-family slice through deterministic
      single-writer merges.
    - Parity coverage pins diagnostics and semantic-product output across the
      supported worker counts for the migrated slices.
  - stop_rule: Stop once the deterministic worker-publication rollout is
      complete through TODO-4135 and TODO-4136.
  - notes: Split on 2026-04-24 after deeper code review showed that routing
      and callable publication in `SemanticPublicationBuilders.cpp` and fact-
      family publication consumed through
      `IrLowererSemanticProductTargetAdapters.cpp` are separate shippable
      production seams.

- [ ] TODO-4126: Release AST bodies after a lowered semantic handoff
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: TODO-4120
  - scope: Introduce a body-scoped lowered semantic handoff so heavy AST body
      storage can be released earlier without breaking provenance, source-map,
      or backend emission requirements.
  - acceptance:
    - A compile-pipeline path can release AST body-heavy state earlier than the
      current post-IR-preparation cutoff while preserving required provenance.
    - A representative RSS or allocation benchmark shows a measurable memory
      improvement on that path.
    - Source-map or debugger-oriented coverage proves the earlier release does
      not break provenance consumers.
  - stop_rule: Stop once one measured earlier-release path lands with preserved
      provenance behavior and documented memory improvement.

- [ ] TODO-4125: Replace semantics step helpers with contract probes
  - owner: ai
  - created_at: 2026-04-20
  - phase: Testing API Contraction
  - depends_on: none
  - scope: Narrow the remaining step-level exports in
      `include/primec/testing/SemanticsValidationHelpers.h` toward graph and
      semantic-product contract probes, then migrate representative tests onto
      the contract surface.
  - acceptance:
    - At least one remaining step-level semantics helper cluster is removed or
      narrowed behind contract-oriented graph or semantic-product probes.
    - Representative tests move off the removed step helpers without regressing
      coverage intent.
    - Architecture/source-lock coverage records the narrower testing surface.
  - stop_rule: Stop once one remaining step-level helper cluster is replaced by
      contract probes and the migrated tests hold the same validation signal.

- [ ] TODO-4124: Replace bulk lowerer helper exports with scenario contracts
  - owner: ai
  - created_at: 2026-04-20
  - phase: Testing API Contraction
  - depends_on: TODO-4120
  - scope: Shrink `include/primec/testing/IrLowererHelpers.h` from a blanket
      internal-helper export surface into smaller scenario-focused semantic-
      product and backend-conformance helpers.
  - acceptance:
    - `IrLowererHelpers.h` no longer bulk-reexports at least one significant
      cluster of internal lowerer helper headers.
    - Replacement scenario helpers cover the migrated test cases without
      restoring `tests -> src` includes.
    - Source-lock or architecture tests pin the narrower lowerer testing API.
  - stop_rule: Stop once one significant lowerer helper cluster is removed from
      the public testing aggregator and its tests use scenario contracts
      instead.
