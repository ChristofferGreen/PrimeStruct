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

- TODO-4127
- TODO-4128
- TODO-4129
- TODO-4130
- TODO-4131

### Priority Lanes (Current)

- Semantic-product authority and lowerer ownership: TODO-4129
- Validator/runtime boundary simplification: TODO-4126, TODO-4127,
  TODO-4128
- Test-surface contraction: TODO-4124, TODO-4125
- Runtime execution unification: TODO-4130
- Parallel semantic publication determinism: TODO-4131

### Execution Queue (Recommended)

1. TODO-4124
2. TODO-4125
3. TODO-4126
4. TODO-4127

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | TODO-4126, TODO-4127, TODO-4129, TODO-4131 |
| Stdlib surface-style alignment and public helper readability | none |
| Stdlib bridge consolidation and collection/file/gfx surface authority | none |
| Vector/map stdlib ownership cutover and collection surface authority | none |
| Stdlib de-experimentalization and public/internal namespace cleanup | none |
| Validator entrypoint and benchmark-plumbing split | TODO-4127 |
| Semantic-product publication by module and fact family | TODO-4127 |
| IR lowerer compile-unit breakup | TODO-4128 |
| Backend validation/build ergonomics | none |
| Emitter/semantics map-helper parity | none |
| VM debug-session argv ownership | none |
| Debug trace replay robustness | none |
| VM/runtime debug numeric opcode parity | TODO-4130 |
| Test-suite audit follow-up and release-gate stability | none |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | none |
| CodeExamples-aligned stdlib surface syntax conformance | none |
| Semantic-product publication parity and deterministic ordering | TODO-4127, TODO-4129, TODO-4131 |
| Lowerer/source-composition contract coverage | TODO-4128 |
| Vector/map bridge parity for imports, rewrites, and lowering | none |
| De-experimentalization surface and namespace parity | none |
| Focused backend rerun ergonomics and suite partitioning | none |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug numeric opcode behavior | TODO-4130 |
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

- [ ] TODO-4131: Implement origin-key worker interner merges
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: TODO-4127
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
  - scope: Consolidate the duplicated opcode execution logic in `VmExecution`
      and `VmDebugSession` behind one shared interpreter core with hooks or
      adapters for debug events and session state.
  - acceptance:
    - The selected opcode families no longer have separate production and
      debug-session implementations.
    - VM execution and debug-session stepping share one interpreter core for
      the migrated opcode surface.
    - Parity coverage proves identical execution behavior between normal VM and
      debug stepping for the migrated surface.
  - stop_rule: Stop once one representative interpreter slice runs through a
      shared VM/debug execution core with parity coverage.

- [ ] TODO-4129: Align `ResolvedModule` publication with import-order contract
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: none
  - scope: Rework semantic-product module publication so `ResolvedModule`
      identity and ordering match the documented contract of parsed or imported
      source units in deterministic import order, instead of top-level-path-key
      grouping plus lexical resorting.
  - acceptance:
    - `SemanticProgram.moduleResolvedArtifacts` groups facts by parsed or
      imported source unit rather than only by top-level path prefix.
    - Published module ordering follows deterministic import order.
    - Semantic-product dump or conformance coverage pins the new module
      identity and ordering contract.
  - stop_rule: Stop once published resolved modules match the documented
      source-unit and import-order contract with conformance coverage.

- [ ] TODO-4128: Split lowering contracts by backend surface
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: none
  - scope: Carve VM, native, and GPU lowering contracts out of the shared
      fallback helpers so backend-specific behavior is carried by explicit
      backend-scoped modules with clear inputs and validation boundaries.
  - acceptance:
    - At least one representative lowering contract family each for VM,
      native, and GPU no longer routes through shared backend-specific
      fallback branches.
    - Backend-specific lowering contracts are pinned by focused conformance or
      source-lock coverage.
    - Shared lowerer helpers retain only backend-agnostic orchestration.
  - stop_rule: Stop once representative VM/native/GPU lowering contracts are
      explicit modules and the shared fallback path for those families is
      deleted or reduced to backend-agnostic glue.

- [ ] TODO-4127: Publish semantic facts from deterministic worker merges
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: none
  - scope: Extend the parallel definition-validation path from
      benchmark-oriented worker chunks into a production semantic-product
      publication path with deterministic worker-local fact and diagnostic
      merges.
  - acceptance:
    - Worker-count `1`, `2`, and `4` runs produce identical published
      semantic-product output for the selected production fact families.
    - Worker-local fact publication merges through one deterministic
      single-writer boundary rather than benchmark-only shadow plumbing.
    - Parity coverage pins diagnostics and semantic-product output across the
      supported worker counts.
  - stop_rule: Stop once at least one production semantic-product publication
      path uses deterministic worker-fact merges with parity coverage.

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
  - scope: Narrow `include/primec/testing/SemanticsValidationHelpers.h` from
      step-level helper exports toward graph and semantic-product contract
      probes, then migrate representative tests onto the contract surface.
  - acceptance:
    - The selected step-level semantics helper exports are removed or narrowed
      behind contract-oriented graph or semantic-product probes.
    - Representative tests move off the removed step helpers without regressing
      coverage intent.
    - Architecture/source-lock coverage records the narrower testing surface.
  - stop_rule: Stop once representative step-level semantics helpers are
      replaced by contract probes and the migrated tests hold the same
      validation signal.

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
