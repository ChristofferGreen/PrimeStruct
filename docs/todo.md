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

- TODO-4116

### Immediate Next 10 (After Ready Now)

- TODO-4110
- TODO-4106
- TODO-4107

### Priority Lanes (Current)

- Skipped doctest debt: TODO-4116, TODO-4110, TODO-4106, TODO-4107

### Execution Queue (Recommended)

1. TODO-4116
2. TODO-4110
3. TODO-4106
4. TODO-4107

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | none |
| Stdlib surface-style alignment and public helper readability | none |
| Stdlib bridge consolidation and collection/file/gfx surface authority | none |
| Vector/map stdlib ownership cutover and collection surface authority | none |
| Stdlib de-experimentalization and public/internal namespace cleanup | none |
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
| Semantic-product-authority conformance | none |
| CodeExamples-aligned stdlib surface syntax conformance | none |
| Semantic-product publication parity and deterministic ordering | none |
| Lowerer/source-composition contract coverage | none |
| Vector/map bridge parity for imports, rewrites, and lowering | none |
| De-experimentalization surface and namespace parity | none |
| Focused backend rerun ergonomics and suite partitioning | none |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug numeric opcode behavior | none |
| Release benchmark/example suite stability and doctest governance | TODO-4116, TODO-4110, TODO-4106, TODO-4107 |

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

- Retained `doctest::skip(true)` coverage is now tracked in four active
  clusters: `TODO-4116` for the remaining legacy VM map numeric-key runtime
  blockers, `TODO-4110` for the remaining VM support-matrix math skips,
  `TODO-4106` for collection compatibility and alias-precedence coverage, and
  `TODO-4107` for residual IR/docs/gfx/smoke skips.
- New skipped doctest coverage must either attach to one of those active leaves
  or replace them with a narrower explicit follow-up before it lands.
- The success condition for each lane is re-enable-or-delete; indefinite
  skipped coverage is not a stable end state.

### Task Blocks

- [ ] TODO-4116: Re-enable or prune remaining VM numeric-key map runtime skips
  - owner: ai
  - created_at: 2026-04-20
  - phase: Skipped Doctest Debt
  - depends_on: none
  - scope: Audit the remaining uniquely valuable skipped numeric-key runtime
      cases in `tests/unit/test_compile_run_vm_maps.cpp`, especially the legacy
      `values[3i32]`, bool-key, and u64-key VM access blockers, then re-enable
      or delete them so the file no longer carries a residual legacy VM map
      runtime skip cluster.
  - acceptance:
    - `tests/unit/test_compile_run_vm_maps.cpp` no longer carries the remaining
      numeric-key VM runtime skips as one undifferentiated legacy cluster.
    - Any residual skipped map cases in that file are narrowed to explicit
      uniquely valuable blockers with clear rationale.
    - The queue/docs state stays aligned with the reduced VM map skip surface.
  - stop_rule: Stop once the remaining VM numeric-key runtime skips are either
      active, deleted, or narrowed into explicit blocker-owned follow-ups.

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
