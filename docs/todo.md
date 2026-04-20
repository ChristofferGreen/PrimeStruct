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

- TODO-4119
- TODO-4120
- TODO-4121
- TODO-4117
- TODO-4118
- TODO-4110
- TODO-4106
- TODO-4107

### Immediate Next 10 (After Ready Now)

- TODO-4134
- TODO-4133
- TODO-4132
- TODO-4122
- TODO-4123
- TODO-4124
- TODO-4125
- TODO-4126
- TODO-4127
- TODO-4128

### Priority Lanes (Current)

- Reflection ergonomics and docs alignment: TODO-4132, TODO-4133
- Struct helper call ergonomics: TODO-4134
- Semantic-product authority and lowerer ownership: TODO-4119, TODO-4120,
  TODO-4121, TODO-4122, TODO-4129
- Validator/runtime boundary simplification: TODO-4123, TODO-4126,
  TODO-4127, TODO-4128
- Test-surface contraction: TODO-4124, TODO-4125
- Runtime execution unification: TODO-4130
- Parallel semantic publication determinism: TODO-4131
- Skipped doctest debt: TODO-4117, TODO-4118, TODO-4110, TODO-4106, TODO-4107

### Execution Queue (Recommended)

1. TODO-4119
2. TODO-4120
3. TODO-4121
4. TODO-4117
5. TODO-4118
6. TODO-4110
7. TODO-4106
8. TODO-4107

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | TODO-4119, TODO-4120, TODO-4121, TODO-4122, TODO-4126, TODO-4127, TODO-4129, TODO-4131 |
| Stdlib surface-style alignment and public helper readability | TODO-4132, TODO-4133, TODO-4134 |
| Stdlib bridge consolidation and collection/file/gfx surface authority | none |
| Vector/map stdlib ownership cutover and collection surface authority | none |
| Stdlib de-experimentalization and public/internal namespace cleanup | none |
| Validator entrypoint and benchmark-plumbing split | TODO-4123, TODO-4127 |
| Semantic-product publication by module and fact family | TODO-4119, TODO-4121, TODO-4122, TODO-4127 |
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
| Semantic-product-authority conformance | TODO-4119, TODO-4121, TODO-4122 |
| CodeExamples-aligned stdlib surface syntax conformance | TODO-4132, TODO-4133, TODO-4134 |
| Semantic-product publication parity and deterministic ordering | TODO-4121, TODO-4122, TODO-4127, TODO-4129, TODO-4131 |
| Lowerer/source-composition contract coverage | TODO-4120, TODO-4128 |
| Vector/map bridge parity for imports, rewrites, and lowering | none |
| De-experimentalization surface and namespace parity | none |
| Focused backend rerun ergonomics and suite partitioning | none |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug numeric opcode behavior | TODO-4130 |
| Release benchmark/example suite stability and doctest governance | TODO-4117, TODO-4118, TODO-4110, TODO-4106, TODO-4107 |

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

- Retained `doctest::skip(true)` coverage is now tracked in five active
  clusters: `TODO-4117` for the remaining legacy VM numeric index-sugar
  blocker, `TODO-4118` for the remaining non-i32 VM map key runtime blockers,
  `TODO-4110` for the remaining VM support-matrix math skips, `TODO-4106` for
  collection compatibility and alias-precedence coverage, and `TODO-4107` for
  residual IR/docs/gfx/smoke skips.
- New skipped doctest coverage must either attach to one of those active leaves
  or replace them with a narrower explicit follow-up before it lands.
- The success condition for each lane is re-enable-or-delete; indefinite
  skipped coverage is not a stable end state.

### Task Blocks

- [ ] TODO-4134: Accept type-qualified dot-call syntax for static helpers
  - owner: ai
  - created_at: 2026-04-20
  - phase: Language Surface Alignment
  - depends_on: none
  - scope: Extend struct helper call syntax so static helpers accept
      type-qualified dot calls such as `Counter.default_step()` in addition to
      the current direct `/Counter/default_step()` spelling, and update the
      docs under `./docs` to prefer the cleaner surface form once it works.
  - acceptance:
    - A representative `[public static int] default_step()` helper compiles
      through `Counter.default_step()` and preserves the current direct-call
      behavior of `/Counter/default_step()`.
    - Deterministic diagnostics still reject genuinely invalid static-helper
      call sites instead of silently routing unrelated receiver forms.
    - `docs/CodeExamples.md` and the relevant helper/struct guidance under
      `./docs` are updated to use the supported type-qualified dot-call form.
  - stop_rule: Stop once one static-helper slice accepts `Type.helper()`
      end-to-end with focused validation coverage and the docs no longer need
      the slash-path workaround for that case.

- [ ] TODO-4133: Let `generate(...)` imply reflection enablement
  - owner: ai
  - created_at: 2026-04-20
  - phase: Language Surface Alignment
  - depends_on: none
  - scope: Remove the current requirement that struct definitions spell both
      `reflect` and `generate(...)` together when the requested generator set
      already depends on reflection metadata, while keeping deterministic
      validation for non-struct targets and unsupported generator names.
  - acceptance:
    - A representative `[struct generate(Equal)]` definition succeeds and
      exposes the same generated helper surface as the equivalent explicit
      `[struct reflect generate(Equal)]` form.
    - Reflection metadata queries and helper generation still reject
      non-struct targets and keep deterministic diagnostics for unsupported or
      duplicate generator names.
    - The docs under `./docs` are updated to stop teaching redundant
      `reflect generate(...)` spelling when generation alone is sufficient.
  - stop_rule: Stop once one generated-helper slice treats `generate(...)` as
      sufficient reflection enablement end-to-end, with focused coverage and
      docs no longer requiring the redundant paired spelling.

- [ ] TODO-4132: Add `==` support for reflected `Equal` helpers
  - owner: ai
  - created_at: 2026-04-20
  - phase: Language Surface Alignment
  - depends_on: none
  - scope: Teach the surface comparison operator path to route reflected
      struct equality through generated `Equal` helpers where that contract is
      available, and update the docs under `./docs` to show the operator form
      instead of the current explicit helper-call workaround.
  - acceptance:
    - A representative `[struct reflect generate(Equal)]` type accepts
      `left == right` and preserves the current `/Type/Equal(left, right)`
      helper behavior.
    - Deterministic diagnostics still reject unsupported `==` uses on struct
      types that do not expose the reflected `Equal` contract.
    - `docs/CodeExamples.md` and the relevant reflection/spec guidance under
      `./docs` are updated to prefer the supported operator spelling once the
      language surface lands.
  - stop_rule: Stop once one reflected-struct equality slice supports `==`
      end-to-end with focused validation coverage and the docs no longer need
      to teach the explicit `/Type/Equal(...)` workaround for that case.

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
  - depends_on: TODO-4120, TODO-4122
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
  - depends_on: TODO-4119, TODO-4122
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
  - depends_on: TODO-4121, TODO-4122
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
  - depends_on: TODO-4120, TODO-4121
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

- [ ] TODO-4123: Split validator core from publication and benchmark shadows
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: none
  - scope: Separate `SemanticsValidator` production validation logic from
      semantic-publication collectors and benchmark/shadow instrumentation so
      benchmark-only state stops sharing the same hot-path class boundary.
  - acceptance:
    - Production validation, semantic-publication collection, and benchmark or
      shadow instrumentation have explicit component boundaries.
    - Benchmark-only state is no longer carried through unaffected production
      validation paths.
    - Compile-pipeline and benchmark coverage confirm behavior parity after the
      split.
  - stop_rule: Stop once one explicit production/publication/instrumentation
      split lands and removes benchmark-only state from the unaffected
      validation hot path.

- [ ] TODO-4122: Publish indexed graph-backed facts for lowering
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: TODO-4119, TODO-4121
  - scope: Publish lowerer-authoritative indexed `query`, `try(...)`,
      `on_error`, and local-`auto` semantic facts in `SemanticProgram` so
      lowering can consume exact indexed facts instead of AST compatibility
      lookups.
  - acceptance:
    - `SemanticProgram` publishes indexed lookup surfaces for `query`,
      `try(...)`, `on_error`, and local-`auto` fact families.
    - At least one lowering path consumes each of those fact families directly
      from the semantic product.
    - Deterministic ordering and lookup parity are pinned by semantic-product
      dump or compile-pipeline conformance coverage.
  - stop_rule: Stop once all four fact families exist in lowerer-ready indexed
      form and representative lowering paths consume them directly.

- [ ] TODO-4121: Move semantic-product coverage checks to publication time
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: none
  - scope: Replace lowerer-time AST rewalks in
      `validateSemanticProduct*Coverage` with semantic-product builder-time
      invariants plus compile-pipeline conformance coverage.
  - acceptance:
    - Lowerer entry setup no longer performs the selected AST rewalk coverage
      checks for semantic-product completeness.
    - Incomplete required fact families fail during semantic-product
      publication or compile-pipeline conformance checks instead of later in
      lowerer setup.
    - Conformance coverage pins the moved invariant at the semantic-product
      boundary.
  - stop_rule: Stop once representative semantic-product completeness checks
      fail at publication time and the corresponding lowerer-time AST rewalks
      are removed.

- [ ] TODO-4120: Remove raw `Program` from `IrLowerer` ownership
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: none
  - scope: Cut `IrLowerer::lower` away from raw semantic ownership of
      `Program`, replacing AST-dependent lowering decisions with a
      semantic-product-first interface plus provenance-only access keyed by
      semantic node identity.
  - acceptance:
    - `IrLowerer::lower` no longer requires raw `Program` for representative
      lowering-facing semantic decisions.
    - Lowering reads meaning from semantic-product inputs and AST data only for
      provenance or syntax-owned executable structure.
    - Compile-pipeline and lowerer contract coverage prove parity for the
      migrated path.
  - stop_rule: Stop once representative lowerer entry/setup paths no longer
      depend on raw `Program` for semantic meaning and rely on provenance-only
      AST access instead.

- [ ] TODO-4119: Publish lowerer-ready entry and routing lookup indexes
  - owner: ai
  - created_at: 2026-04-20
  - phase: Semantic Ownership Boundary
  - depends_on: none
  - scope: Add `SemanticProgram` lookup indexes for entry metadata,
      effect/capability masks, and alias-equivalent routing data so lowerer
      entry setup stops rebuilding AST-side definition and import maps for
      those lookups.
  - acceptance:
    - `SemanticProgram` exposes direct lookup surfaces for entry metadata and
      the routing facts currently rebuilt in `runLowerEntrySetup`.
    - Lowerer entry setup resolves at least one representative entry or routing
      metadata path without rebuilding AST-side definition or import alias
      maps.
    - Semantic-product conformance coverage pins lookup parity for the migrated
      data.
  - stop_rule: Stop once representative entry/routing lookups are served
      directly from `SemanticProgram` and the matching AST rebuild path is
      deleted or narrowed to provenance-only use.

- [ ] TODO-4117: Re-enable or prune remaining VM numeric map indexing sugar skip
  - owner: ai
  - created_at: 2026-04-20
  - phase: Skipped Doctest Debt
  - depends_on: none
  - scope: Audit the remaining skipped canonical VM numeric map indexing-sugar
      case in `tests/unit/test_compile_run_vm_maps.cpp`, then either re-enable
      it or delete it if the surrounding active helper coverage already makes
      the bracket-sugar duplicate stale.
  - acceptance:
    - The remaining `values[3i32]` VM map indexing-sugar skip is either active,
      deleted, or still explicitly owned as a narrow blocker with rationale.
    - `tests/unit/test_compile_run_vm_maps.cpp` and the queue/docs state no
      longer treat canonical VM numeric bracket sugar as part of a broad
      umbrella skip cluster.
  - stop_rule: Stop once the canonical numeric indexing-sugar case is either
      active, deleted, or explicitly justified as a narrow blocker.

- [ ] TODO-4118: Re-enable or prune remaining VM non-i32 map key runtime skips
  - owner: ai
  - created_at: 2026-04-20
  - phase: Skipped Doctest Debt
  - depends_on: none
  - scope: Audit the remaining skipped bool-key and u64-key VM map runtime
      cases in `tests/unit/test_compile_run_vm_maps.cpp`, then either re-enable
      them or delete them if adjacent active VM collection coverage already
      makes them stale.
  - acceptance:
    - The remaining bool-key and u64-key VM map runtime skips are either
      active, deleted, or still explicitly owned as narrow blockers with clear
      rationale.
    - `tests/unit/test_compile_run_vm_maps.cpp` and the queue/docs state no
      longer treat non-i32 VM map key runtime as part of a broad umbrella skip
      cluster.
  - stop_rule: Stop once the remaining non-i32 VM map key runtime cases are
      either active, deleted, or explicitly justified as narrow blockers.

- [ ] TODO-4110: Re-enable or prune remaining VM support-matrix math skips
  - owner: ai
  - created_at: 2026-04-20
  - phase: Skipped Doctest Debt
  - depends_on: none
  - scope: Audit the remaining skipped support-matrix and quaternion/matrix math
      cases in `tests/unit/test_compile_run_vm_math.cpp`, then re-enable or
      delete stale coverage so only current VM-specific blockers remain.
  - acceptance:
    - `tests/unit/test_compile_run_vm_math.cpp` no longer carries the remaining
      support-matrix skip cluster without explicit queue ownership.
    - Any residual VM math skips are reduced to narrow blockers with clear
      backend rationale instead of one broad umbrella.
    - The queue/docs state stays aligned with the narrowed VM math skip surface.
  - stop_rule: Stop once the remaining VM support-matrix math skips are either
      active, deleted, or narrowed into explicit blocker-owned follow-ups.

- [ ] TODO-4106: Re-enable or prune skipped collection compatibility suites
  - owner: ai
  - created_at: 2026-04-20
  - phase: Skipped Doctest Debt
  - depends_on: none
  - scope: Audit the retained skipped collection compatibility and
      alias-precedence suites across semantics, IR lowerer validation, VM,
      native, and C++ emitter coverage, then re-enable or delete stale cases so
      collection bridge parity stops depending on broad skipped clusters.
  - acceptance:
    - The largest skipped collection compatibility clusters in semantics,
      VM/native compile-run, and C++ emitter files are either re-enabled or
      broken into smaller explicit follow-up leaves.
    - Skipped alias-precedence and helper-routing coverage no longer sits in
      broad umbrella files without an active owning TODO.
    - Docs/todo and adjacent source locks reflect the narrowed collection skip
      surface.
  - stop_rule: Stop once the current collection compatibility umbrella skips
      are reduced to actively owned narrow blockers or deleted entirely.

- [ ] TODO-4107: Re-enable or prune residual skipped IR, docs, and smoke suites
  - owner: ai
  - created_at: 2026-04-20
  - phase: Skipped Doctest Debt
  - depends_on: none
  - scope: Audit the remaining skipped IR/source-lock, docs, gfx, wasm, demo,
      and registry/pilot suites, then either re-enable them or delete stale
      coverage so residual skipped tests no longer drift without queue
      ownership.
  - acceptance:
    - Retained skipped IR/source-lock and docs/smoke suites are each either
      re-enabled or explicitly covered by this lane rather than orphaned.
    - Gfx/wasm/demo/pilot skipped suites are reduced to narrower blockers or
      removed when stale.
    - The residual skipped-test queue remains explicit and synchronized with the
      surviving clusters.
  - stop_rule: Stop once every remaining non-collection skipped suite is either
      active and explicitly owned here or no longer skipped.
