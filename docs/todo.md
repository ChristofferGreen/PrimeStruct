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

- TODO-4182

### Immediate Next 10 (After Ready Now)

- TODO-4181

### Priority Lanes (Current)

- `soa_vector` promotion and de-experimentalization: TODO-4182, TODO-4181

### Execution Queue (Recommended)

1. TODO-4182
2. TODO-4181

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | none |
| Compile-pipeline stage and publication-boundary contracts | none |
| Compile-time macro hooks and AST transform ownership | none |
| Stdlib surface-style alignment and public helper readability | none |
| Stdlib bridge consolidation and collection/file/gfx surface authority | none |
| Vector/map stdlib ownership cutover and collection surface authority | none |
| Stdlib de-experimentalization and public/internal namespace cleanup | TODO-4182 |
| SoA maturity and `soa_vector` promotion | TODO-4182 |
| Validator entrypoint and benchmark-plumbing split | none |
| Semantic-product publication by module and fact family | none |
| Semantic-product public API factoring and versioning | none |
| IR lowerer compile-unit breakup | none |
| Backend validation/build ergonomics | none |
| Emitter/semantics map-helper parity | none |
| VM debug-session argv ownership | none |
| Debugger/source-map provenance parity | none |
| Debug trace replay robustness | none |
| VM/runtime debug stateful opcode parity | none |
| Test-suite audit follow-up and release-gate stability | none |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | none |
| AST transform hook conformance | none |
| CodeExamples-aligned stdlib surface syntax conformance | none |
| Compile-pipeline stage handoff conformance | none |
| Semantic-product publication parity and deterministic ordering | none |
| Lowerer/source-composition contract coverage | none |
| Vector/map bridge parity for imports, rewrites, and lowering | none |
| De-experimentalization surface and namespace parity | TODO-4182 |
| `soa_vector` maturity and canonical surface parity | TODO-4182 |
| Focused backend rerun ergonomics and suite partitioning | none |
| Architecture contract probe migration | none |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debugger/source-map provenance parity | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug stateful opcode behavior | none |
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
- Active promotion lane now has the canonical public helper wrapper
  authoritative for ordinary construction/read/ref/mutator/conversion helper
  names, bound field-view borrow-root invalidation, and canonical-only
  C++/VM/native helper/conversion parity coverage, with conversion receiver
  contracts spelled through canonical `SoaVector<T>` surfaces. Remaining work
  should migrate examples.

### Skipped Doctest Debt Summary

- Retained `doctest::skip(true)` coverage is currently absent from the active
  queue because no skipped doctest cases remain under `tests/unit`.
- New skipped doctest coverage must create a new explicit TODO before it lands.
- The success condition for each lane is re-enable-or-delete; indefinite
  skipped coverage is not a stable end state.

### Task Blocks

- [ ] TODO-4182: Promote the `soa_vector` draft example and migrate canonical imports
  - owner: ai
  - created_at: 2026-04-25
  - phase: `soa_vector` promotion
  - depends_on: TODO-4180
  - scope: Replace draft/example-only `soa_vector` usage with one canonical
    checked-in example plus focused release coverage that imports
    `/std/collections/soa_vector/*` and `/std/collections/soa_vector_conversions/*`
    instead of the experimental namespaces for ordinary public flows.
  - acceptance:
    - `examples/3.Surface/soa_vector_ecs_draft.prime` is promoted, replaced, or
      renamed so the checked-in example reads like a supported canonical
      `soa_vector` surface instead of a draft-only sketch.
    - The touched tests/docs/examples exercise canonical `soa_vector` imports
      for ordinary public flows; direct experimental imports remain only in
      explicit compatibility coverage if still needed.
    - Release validation or example-lock coverage references the canonical
      example rather than the old draft/experimental import path.
  - stop_rule: Stop after one canonical example path plus the touched
    canonical-import coverage are in place; do not delete every compatibility
    test in one slice.

- [ ] TODO-4181: Track `soa_vector` promotion out of experimental
  - owner: human
  - created_at: 2026-04-25
  - phase: `soa_vector` promotion
  - depends_on: TODO-4180, TODO-4182
  - scope: Track the first full `soa_vector` promotion lane so the canonical
    `/std/collections/soa_vector/*` surface becomes authoritative, the
    experimental namespaces become compatibility-only or removable seams, and
    docs/spec/examples all describe the same maturity status.
  - acceptance:
    - TODO-4179, TODO-4177, TODO-4178, TODO-4180, and TODO-4182 land with one
      coherent canonical `soa_vector` public contract.
    - `docs/PrimeStruct.md`, `docs/PrimeStruct_SyntaxSpec.md`, `docs/todo.md`,
      and the checked-in example corpus classify `soa_vector` consistently as
      promoted or still compatibility-gated with explicit remaining blockers.
    - The promotion lane no longer requires ordinary public code to import
      `experimental_soa_vector` or
      `experimental_soa_vector_conversions`.
  - stop_rule: Stop after the first coherent public promotion pass is
    documented and validated; do not fold broader SoA storage redesign or new
    container families into this tracker.
