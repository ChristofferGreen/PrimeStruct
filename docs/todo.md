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

- TODO-4226: Add a structured semantic diagnostic/result sink

### Immediate Next 10 (After Ready Now)

- TODO-4214: Introduce deterministic worker result bundles
- TODO-4227: Move semantic-product fact families into worker bundles
- TODO-4215: Make semantic-product publication consume merged fact bundles
- TODO-4228: Factor and version the semantic-product boundary API
- TODO-4216: Split semantic rewrites into an explicit pass manifest
- TODO-4217: Move stdlib compatibility rewrites behind surface adapters
- TODO-4224: Cut over vector/map compatibility decisions to surface adapters
- TODO-4229: Cut over SoA compatibility decisions to surface adapters
- TODO-4230: Cut over gfx compatibility decisions to surface adapters
- TODO-4218: Make local-auto graph facts the exclusive inference authority

### Priority Lanes (Current)

- Semantic phase contract hardening: TODO-4226
  -> TODO-4214 -> TODO-4227 -> TODO-4215 -> TODO-4228
  -> TODO-4216 -> TODO-4217 -> TODO-4224 -> TODO-4229 -> TODO-4230
  -> TODO-4218 -> TODO-4231 -> TODO-4219 -> TODO-4225 -> TODO-4232
  -> TODO-4233 -> TODO-4220 -> TODO-4234 -> TODO-4221 -> TODO-4235
- Deferred graph and inference hardening: TODO-4236 -> TODO-4237
  -> TODO-4238 -> TODO-4239
- Deferred semantic-product/backend/tooling follow-ups: TODO-4240
  -> TODO-4241; TODO-4242 -> TODO-4243; TODO-4245
- Deferred SoA finish: TODO-4244 -> TODO-4246 -> TODO-4247
  -> TODO-4248 -> TODO-4249 -> TODO-4250 -> TODO-4251 -> TODO-4252

### Execution Queue (Recommended)

- TODO-4226: Add a structured semantic diagnostic/result sink
- TODO-4214: Introduce deterministic worker result bundles
- TODO-4227: Move semantic-product fact families into worker bundles
- TODO-4215: Make semantic-product publication consume merged fact bundles
- TODO-4228: Factor and version the semantic-product boundary API
- TODO-4216: Split semantic rewrites into an explicit pass manifest
- TODO-4217: Move stdlib compatibility rewrites behind surface adapters
- TODO-4224: Cut over vector/map compatibility decisions to surface adapters
- TODO-4229: Cut over SoA compatibility decisions to surface adapters
- TODO-4230: Cut over gfx compatibility decisions to surface adapters
- TODO-4218: Make local-auto graph facts the exclusive inference authority
- TODO-4231: Make query/try/on_error graph facts the exclusive authority
- TODO-4219: Fail closed on residual lowerer AST semantic fallbacks
- TODO-4225: Close call-target and helper-routing lowerer fallbacks
- TODO-4232: Close binding/type/effect/layout lowerer fallbacks
- TODO-4233: Close backend-adapter and source-composition fallbacks
- TODO-4220: Add semantic phase handoff conformance gates
- TODO-4234: Add semantic budget and worker-parity release gates
- TODO-4221: Retire stale semantic validator source locks
- TODO-4235: Retire remaining semantic/lowerer architecture source locks
- TODO-4236: Define graph invalidation contracts by edit family
- TODO-4237: Add graph invalidation fan-out regression tests
- TODO-4238: Pin the CT-eval graph and semantic-product boundary
- TODO-4239: Migrate helper-routing template inference onto graph facts
- TODO-4240: Add backend semantic-product conformance coverage
- TODO-4241: Retire semantic-product output compatibility callers
- TODO-4242: Inventory repo-wide source-lock replacement candidates
- TODO-4243: Improve focused backend rerun ergonomics
- TODO-4244: Decide the `soa_vector` maturity exit
- TODO-4246: Define final `soa_vector` promotion contract
- TODO-4247: Move canonical SoA wrapper off experimental implementation imports
- TODO-4248: Move canonical SoA conversions off experimental conversion imports
- TODO-4249: Retire direct experimental SoA public imports
- TODO-4250: Normalize raw builtin `soa_vector` bridges onto canonical wrappers
- TODO-4251: Add full cross-backend SoA parity coverage
- TODO-4252: Promote `soa_vector` docs after compatibility cleanup
- TODO-4245: Plan dynamic vector growth and runtime storage support

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | TODO-4214, TODO-4227, TODO-4218, TODO-4231, TODO-4236, TODO-4237, TODO-4238, TODO-4239 |
| Compile-pipeline stage and publication-boundary contracts | TODO-4226, TODO-4220, TODO-4234 |
| Compile-time macro hooks and AST transform ownership | TODO-4238, TODO-4239 |
| Stdlib surface-style alignment and public helper readability | none |
| Stdlib bridge consolidation and collection/file/gfx surface authority | TODO-4217, TODO-4224, TODO-4229, TODO-4230, TODO-4244, TODO-4246, TODO-4247, TODO-4248, TODO-4249 |
| Vector/map stdlib ownership cutover and collection surface authority | TODO-4217, TODO-4224, TODO-4245 |
| Stdlib de-experimentalization and public/internal namespace cleanup | none |
| SoA maturity and `soa_vector` promotion | TODO-4244, TODO-4246, TODO-4247, TODO-4248, TODO-4249, TODO-4250, TODO-4251, TODO-4252 |
| Validator entrypoint and benchmark-plumbing split | TODO-4226, TODO-4214, TODO-4227 |
| Semantic-product publication by module and fact family | TODO-4214, TODO-4227, TODO-4215, TODO-4240, TODO-4241 |
| Semantic-product public API factoring and versioning | TODO-4215, TODO-4228, TODO-4219, TODO-4225, TODO-4232, TODO-4233, TODO-4240, TODO-4241 |
| IR lowerer compile-unit breakup | TODO-4219, TODO-4225, TODO-4232, TODO-4233 |
| Backend validation/build ergonomics | TODO-4243 |
| Emitter/semantics map-helper parity | none |
| VM debug-session argv ownership | none |
| Debugger/source-map provenance parity | none |
| Debug trace replay robustness | none |
| VM/runtime debug stateful opcode parity | none |
| Test-suite audit follow-up and release-gate stability | TODO-4220, TODO-4234, TODO-4242, TODO-4243 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | TODO-4215, TODO-4228, TODO-4219, TODO-4225, TODO-4232, TODO-4233, TODO-4220, TODO-4240, TODO-4241 |
| AST transform hook conformance | TODO-4238, TODO-4239 |
| CodeExamples-aligned stdlib surface syntax conformance | none |
| Compile-pipeline stage handoff conformance | TODO-4226, TODO-4220, TODO-4234, TODO-4240 |
| Semantic-product publication parity and deterministic ordering | TODO-4214, TODO-4227, TODO-4215, TODO-4240 |
| Lowerer/source-composition contract coverage | TODO-4219, TODO-4225, TODO-4232, TODO-4233 |
| Vector/map bridge parity for imports, rewrites, and lowering | TODO-4217, TODO-4224, TODO-4245 |
| De-experimentalization surface and namespace parity | none |
| `soa_vector` maturity and canonical surface parity | TODO-4244, TODO-4246, TODO-4247, TODO-4248, TODO-4249, TODO-4250, TODO-4251, TODO-4252 |
| Focused backend rerun ergonomics and suite partitioning | TODO-4243 |
| Architecture contract probe migration | TODO-4221, TODO-4235, TODO-4242 |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debugger/source-map provenance parity | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug stateful opcode behavior | none |
| Release benchmark/example suite stability and doctest governance | TODO-4220, TODO-4234, TODO-4243 |

### Vector/Map Bridge Contract Summary

- Bridge-owned public contract: exact and wildcard `/std/collections` imports,
  `vector<T>` / `map<K, V>` constructor and literal-rewrite surfaces, helper
  families, compatibility spellings plus removed-helper diagnostics, semantic
  surface IDs, and lowerer dispatch metadata.
- Migration-only seams: rooted `/vector/*` and `/map/*` spellings,
  `vectorCount` / `mapCount`-style lowering names, and
  `/std/collections/experimental_*` implementation modules stay temporary.
  No active TODO currently targets their deletion or acceptance, so add a
  concrete cutover TODO before changing those seams.
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
  `/std/collections/experimental_soa_vector_conversions/*` remain
  compatibility seams behind the canonical SoA experiment surface. The
  deferred SoA finish chain (`TODO-4246` through `TODO-4252`) now owns the
  concrete exit, acceptance, or reclassification path for those seams.
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
- Further promotion work is tracked by the deferred SoA finish chain
  (`TODO-4246` through `TODO-4252`), which owns the compatibility seam exits,
  final promotion/acceptance decisions, generic substrate cleanup, parity
  coverage, and final promotion docs needed before `soa_vector` can be treated
  as a promoted public contract.
- First promotion pass complete: the canonical public helper wrapper is
  authoritative for ordinary construction/read/ref/mutator/conversion helper
  names, bound field-view borrow-root invalidation, and canonical-only
  C++/VM/native helper/conversion parity coverage. Conversion receiver
  contracts are spelled through canonical `SoaVector<T>` surfaces, the checked-in
  ECS example uses canonical wrapper/conversion imports, and ordinary public
  code no longer needs `experimental_soa_vector` or
  `experimental_soa_vector_conversions` imports. `soa_vector<T>` remains an
  incubating canonical experiment until the remaining compatibility seams and
  generic SoA substrate cleanup are explicitly retired or accepted.

### Skipped Doctest Debt Summary

- Retained `doctest::skip(true)` coverage is currently absent from the active
  queue because no skipped doctest cases remain under `tests/unit`.
- New skipped doctest coverage must create a new explicit TODO before it lands.
- The success condition for each lane is re-enable-or-delete; indefinite
  skipped coverage is not a stable end state.

### Task Blocks

- [ ] TODO-4226: Add a structured semantic diagnostic/result sink
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4222
  - scope: Introduce one structured diagnostic/result sink for validator
    success/failure state, with an adapter back to the existing first-error
    string API and current collect-diagnostics behavior.
  - acceptance:
    - Definition and execution diagnostics flow through the structured sink or
      result surface before being adapted to legacy string outputs.
    - The first fatal diagnostic remains byte-stable for existing entrypoints.
    - Collected diagnostics preserve current ordering and related-span
      behavior for focused negative tests.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once validator diagnostics have one production result sink;
    do not bundle worker fact merging into this leaf.

- [ ] TODO-4214: Introduce deterministic worker result bundles
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4226
  - scope: Replace ad hoc worker outputs with a named worker-result bundle for
    structured diagnostics, counters, execution summaries, and worker-local
    string/interner snapshots behind one deterministic merge contract.
  - acceptance:
    - Worker results expose one explicit structure for the migrated result
      families instead of separate side channels.
    - Merge order is keyed only by deterministic partition/fact ordering, never
      task completion order.
    - Existing worker-count diagnostic and callable-summary tests remain
      byte-stable.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after the worker result contract owns current worker
    diagnostics, counters, execution summaries, and local interner state; leave
    semantic-product fact-family buffers to TODO-4227.

- [ ] TODO-4227: Move semantic-product fact families into worker bundles
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4214
  - scope: Move semantic-product fact-family buffers produced by definition
    workers into the deterministic worker-result bundle introduced by
    TODO-4214.
  - acceptance:
    - Direct-call, bridge-path, binding, query, try, on_error, and callable
      publication facts no longer merge through separate worker side channels.
    - Existing worker-count semantic-product dump tests remain byte-stable.
    - The merged result surface contains every fact family currently consumed
      by semantic-product publication.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once current publication fact families are in the worker
    result bundle; leave publication API changes to TODO-4215.

- [ ] TODO-4215: Make semantic-product publication consume merged fact bundles
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4227
  - scope: Move semantic-product publication to consume the deterministic
    merged fact bundle produced by validation rather than pulling publication
    data back through mutable validator snapshot methods.
  - acceptance:
    - `publishSemanticProgramAfterValidation(...)` or its replacement accepts a
      publication-ready fact bundle boundary.
    - Published lookup indexes and formatted `semantic-product` output remain
      byte-stable across serial and worker validation.
    - Lowerer adapter tests prove published indexes are still the lowering
      authority for routing/local-auto/query/try/on_error families.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once publication no longer depends on validator-owned
    mutable snapshot extraction for migrated families; leave public API and
    versioning cleanup to TODO-4228.

- [ ] TODO-4228: Factor and version the semantic-product boundary API
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4215
  - scope: Define the narrow post-semantics boundary API, including which facts
    are semantic-product owned, which provenance remains AST-owned, and how
    future semantic-product dump/API changes are versioned or documented.
  - acceptance:
    - Public or testing helper APIs make semantic-product-owned facts
      distinguishable from AST-owned provenance/body inventory.
    - Any semantic-product dump/API shape change is versioned or documented in
      `docs/PrimeStruct.md` with migration notes.
    - Existing semantic-product helper and dump tests remain byte-stable unless
      the versioned migration notes explicitly call out the expected diff.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once consumers can tell from API shape which facts are
    semantic-product owned; source-lock cleanup remains TODO-4221/TODO-4235.

- [ ] TODO-4216: Split semantic rewrites into an explicit pass manifest
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4228
  - scope: Extract the ordered semantic rewrite chain into a named manifest or
    pipeline table that records pass order, input/output ownership, and whether
    a pass mutates AST, publishes facts, or only validates.
  - acceptance:
    - The current ordered rewrite behavior is preserved and covered by focused
      pass-order tests.
    - The manifest makes compatibility rewrites distinguishable from core
      semantic canonicalization rewrites.
    - Diagnostics remain stable for representative reflection, SoA, map, and
      constructor rewrite failures.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after pass order is explicit and tested; do not change the
    compatibility behavior itself until TODO-4217.

- [ ] TODO-4217: Move stdlib compatibility rewrites behind surface adapters
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4216
  - scope: Inventory vector/map/SoA/gfx compatibility decisions that currently
    live in broad semantic rewrite, validation, template-monomorph, or lowering
    paths, then move the first high-churn family behind an explicit
    `StdlibSurfaceRegistry`-backed adapter with canonical, compatibility,
    syntax-owned, and lowering spellings called out per family.
  - acceptance:
    - The inventory identifies every non-syntax-owned compatibility decision in
      the targeted families and marks it migrated, syntax/provenance-owned, or
      queued under TODO-4224, TODO-4229, or TODO-4230.
    - At least one high-churn collection family routes compatibility decisions
      through the shared surface adapter instead of bespoke validator rewrite
      code.
    - Existing canonical and compatibility import tests keep the same success
      or diagnostic behavior.
    - Any deleted compatibility branch is covered by a targeted regression test
      proving the adapter owns the behavior.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after the adapter boundary, inventory, and one
    representative family are migrated; TODO-4224 owns closing the rest of the
    inventory.

- [ ] TODO-4224: Cut over vector/map compatibility decisions to surface adapters
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4217
  - scope: Close the TODO-4217 compatibility inventory by moving every
    remaining non-syntax-owned vector/map decision behind shared surface
    adapters, or by documenting and testing the decision as explicitly
    syntax/provenance-owned.
  - acceptance:
    - No non-syntax-owned vector/map compatibility decision remains in
      bespoke semantic rewrite, validation, template-monomorph, or lowerer
      branches outside the shared adapters.
    - Any retained vector/map syntax/provenance-owned exception is named in
      docs and covered by a contract test.
    - Canonical and compatibility vector/map import/lowering parity remains
      stable across C++/VM/native where supported.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop when the vector/map inventory is empty except for
    documented syntax/provenance-owned exceptions; leave SoA and gfx adapter
    families to TODO-4229 and TODO-4230.

- [ ] TODO-4229: Cut over SoA compatibility decisions to surface adapters
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4224
  - scope: Move remaining non-syntax-owned SoA compatibility decisions behind
    shared surface adapters, or document and test them as explicitly
    syntax/provenance-owned.
  - acceptance:
    - No non-syntax-owned SoA compatibility decision remains in bespoke
      semantic rewrite, validation, template-monomorph, or lowerer branches
      outside the shared adapters.
    - Retained experimental SoA compatibility imports remain accepted or reject
      with the same diagnostics as before.
    - Canonical SoA wrapper and conversion coverage remains stable for
      C++/VM/native where supported.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop when the SoA inventory is empty except for documented
    syntax/provenance-owned exceptions; leave gfx adapter work to TODO-4230.

- [ ] TODO-4230: Cut over gfx compatibility decisions to surface adapters
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4229
  - scope: Move remaining non-syntax-owned gfx compatibility decisions behind
    shared surface adapters, or document and test them as explicitly
    syntax/provenance-owned.
  - acceptance:
    - No non-syntax-owned gfx compatibility decision remains in bespoke
      semantic rewrite, validation, template-monomorph, or lowerer branches
      outside the shared adapters.
    - Canonical `/std/gfx/*` behavior and legacy `/std/gfx/experimental/*`
      compatibility coverage remain stable.
    - Any retained gfx syntax/provenance-owned exception is named in docs and
      covered by a contract test.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop when the gfx inventory is empty except for documented
    syntax/provenance-owned exceptions; unrelated gfx feature work is out of
    scope.

- [ ] TODO-4218: Make local-auto graph facts the exclusive inference authority
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4230
  - scope: Remove or quarantine remaining production paths that re-derive
    local-auto facts outside the graph-backed semantic fact flow, keeping any
    legacy comparison only behind benchmark-only shadow wiring.
  - acceptance:
    - Production validation and semantic-product publication use graph-backed
      local-auto facts as the sole authority for migrated cases.
    - Benchmark-only legacy shadows cannot affect published facts or lowering.
    - Focused graph-local-auto tests cover at least one success path and one
      deterministic unresolved/contradictory diagnostic.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after local-auto facts are fail-closed on graph facts; leave
    query/try/on_error inference islands to TODO-4231.

- [ ] TODO-4231: Make query/try/on_error graph facts the exclusive authority
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4218
  - scope: Remove or quarantine remaining production paths that re-derive
    query, `try(...)`, or `on_error` facts outside the graph-backed semantic
    fact flow, keeping legacy comparison only behind benchmark-only shadow
    wiring.
  - acceptance:
    - Production validation and semantic-product publication use graph-backed
      query, `try(...)`, and `on_error` facts as the sole authority for
      migrated cases.
    - Benchmark-only legacy shadows cannot affect published facts or lowering.
    - Focused tests cover one success path and one deterministic unresolved or
      contradictory diagnostic per migrated fact family.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after query/try/on_error facts are fail-closed on graph
    facts; broad graph-performance work remains outside this leaf.

- [ ] TODO-4219: Fail closed on residual lowerer AST semantic fallbacks
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4231
  - scope: Audit lowerer and backend-adapter paths for semantic decisions still
    inferred from raw AST or import state after semantic-product publication,
    then convert one remaining representative fallback into a fail-closed
    semantic-product lookup.
  - acceptance:
    - The audit records every residual lowerer/backend-adapter semantic
      fallback as migrated, syntax/provenance-owned, or queued under TODO-4225,
      TODO-4232, or TODO-4233.
    - The chosen fallback no longer re-derives semantic meaning from raw AST
      when a semantic product is present.
    - Missing or stale semantic-product facts produce a deterministic lowering
      diagnostic instead of silently falling back.
    - Focused lowerer contract tests pin the fail-closed behavior.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after one real fallback is deleted and covered; TODO-4225,
    TODO-4232, and TODO-4233 own closing the remaining audited fallback
    inventory.

- [ ] TODO-4225: Close call-target and helper-routing lowerer fallbacks
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4219
  - scope: Close the TODO-4219 audit items for call-target and helper-routing
    fallbacks by deleting or fail-closing production lowerer paths that
    silently re-derive those decisions from raw AST or import state.
  - acceptance:
    - The audited call-target and helper-routing fallback inventory is empty
      except for documented syntax/provenance-owned paths.
    - Missing, stale, or contradictory semantic-product facts produce
      deterministic diagnostics for migrated call/helper fallback families.
    - C++/VM/native boundary tests prove migrated call/helper families consume
      the same published semantic-product facts.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop when call-target and helper-routing fallbacks are closed;
    leave binding/type/effect/layout fallbacks to TODO-4232.

- [ ] TODO-4232: Close binding/type/effect/layout lowerer fallbacks
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4225
  - scope: Close the TODO-4219 audit items for binding, type, effect, and
    layout fallbacks by deleting or fail-closing production lowerer paths that
    silently re-derive those decisions from raw AST or import state.
  - acceptance:
    - The audited binding/type/effect/layout fallback inventory is empty except
      for documented syntax/provenance-owned paths such as source spans, body
      traversal, or explicitly syntax-owned field provenance.
    - Missing, stale, or contradictory semantic-product facts produce
      deterministic diagnostics for migrated fallback families.
    - C++/VM/native boundary tests prove migrated setup consumes the same
      published semantic-product facts.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop when binding/type/effect/layout fallbacks are closed; leave
    backend-adapter/source-composition cleanup to TODO-4233.

- [ ] TODO-4233: Close backend-adapter and source-composition fallbacks
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4232
  - scope: Close the TODO-4219 audit items for backend adapters,
    source-composition helpers, and public lowerer helper APIs that could
    reintroduce AST-side semantic inference after semantic-product publication.
  - acceptance:
    - Backend adapter and source-composition APIs require semantic-product facts
      for lowering-facing meaning or are documented as syntax/provenance-only.
    - No production backend adapter silently recovers lowering-facing semantics
      from raw AST or import state when semantic-product facts are missing.
    - Boundary tests prove stale/missing semantic-product facts fail closed.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop when no production lowerer or backend-adapter path silently
    falls back to AST-side semantic re-derivation for lowering-facing meaning.

- [ ] TODO-4220: Add semantic phase handoff conformance gates
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4233
  - scope: Add focused release-gate coverage that verifies the import,
    transform, semantic-plan, validation, merged-fact, publication, and lowerer
    handoffs remain deterministic.
  - acceptance:
    - A focused CTest or helper check covers semantic phase handoff parity
      without relying on brittle source text locks.
    - The check runs under the normal release gate or an explicitly documented
      release-gate helper.
    - The gate catches a deliberately stale or missing handoff fact in a
      focused negative case.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the handoff gate catches contract drift for the
    migrated semantic boundary; leave budget and worker-parity gates to
    TODO-4234.

- [ ] TODO-4234: Add semantic budget and worker-parity release gates
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4220
  - scope: Add release-gate coverage for semantic memory budgets, graph budget
    counters, and worker-count parity after the handoff conformance gate exists.
  - acceptance:
    - Semantic memory and graph budget checks run under the normal release gate
      or an explicitly documented release-gate helper.
    - Repeated 1/2/4-worker validation keeps semantic-product dumps,
      diagnostics, and handoff facts byte-stable.
    - Budget failures report actionable counters rather than opaque test
      failures.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once semantic boundary correctness and budget regressions
    are both caught by release validation; do not broaden this into a full
    test-suite audit.

- [ ] TODO-4221: Retire stale semantic validator source locks
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4234
  - scope: Replace stale source-lock tests that pin old semantic validator
    composition details for the migrated boundary with API-level contract tests
    over the plan, worker result, semantic-product, and diagnostic surfaces.
  - acceptance:
    - Stale semantic-validator source assertions for the migrated boundary are
      deleted or converted to public contract assertions.
    - `docs/memories.md` is updated if any remembered source-lock fact becomes
      stale.
    - No direct `tests -> src` include-layer allowlist entries are introduced.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after migrated semantic-validator source locks are
    replaced; leave lowerer/source-composition locks to TODO-4235.

- [ ] TODO-4235: Retire remaining semantic/lowerer architecture source locks
  - owner: ai
  - created_at: 2026-04-27
  - phase: Semantic phase contract hardening
  - depends_on: TODO-4221
  - scope: Replace remaining source-lock tests that pin old semantic/lowerer
    composition details for the migrated boundary with API-level contract tests
    over semantic-product and lowerer handoff surfaces.
  - acceptance:
    - Remaining migrated-boundary lowerer/source-composition source assertions
      are deleted or converted to public contract assertions.
    - `docs/memories.md` is updated if any remembered source-lock fact becomes
      stale.
    - No direct `tests -> src` include-layer allowlist entries are introduced.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after migrated semantic/lowerer architecture source locks
    are replaced; unrelated legacy source locks should become separate TODO
    leaves.

- [ ] TODO-4236: Define graph invalidation contracts by edit family
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred graph and inference hardening
  - depends_on: TODO-4235
  - scope: Introduce a concrete graph invalidation contract for at least
    local-binding, control-flow, initializer-shape, definition-signature,
    import-alias, and receiver-type edits. Make the contract visible through
    named edit-family metadata or a focused graph invalidation API instead of
    relying on incidental cache misses.
  - acceptance:
    - Each edit family declares immediate invalidations, lazy revisits,
      diagnostic discard rules, and definition-local versus cross-definition
      propagation behavior.
    - Focused tests prove at least one intra-definition and one
      cross-definition edit family use the contract.
    - `docs/PrimeStruct.md` records the supported invalidation contract.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after the invalidation contract exists and two
    representative edit families consume it; leave broader fan-out regression
    coverage to TODO-4237.

- [ ] TODO-4237: Add graph invalidation fan-out regression tests
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred graph and inference hardening
  - depends_on: TODO-4236
  - scope: Add observable graph invalidation fan-out regression coverage for
    the edit families defined by TODO-4236 using counters, dumps, or focused
    helper output.
  - acceptance:
    - Tests prove local-binding, control-flow, initializer-shape,
      definition-signature, import-alias, and receiver-type edits refresh the
      expected dependent facts.
    - Negative coverage proves unrelated definitions or modules are not
      spuriously invalidated for at least two edit families.
    - Failure output reports enough counter or dump detail to identify the
      over-invalidated family.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once each contracted edit family has one positive fan-out
    check and the two highest-risk families have negative scope checks; leave
    performance tuning to existing semantic budget gates.

- [ ] TODO-4238: Pin the CT-eval graph and semantic-product boundary
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred graph and inference hardening
  - depends_on: TODO-4237
  - scope: Make CT-eval consumers either consume graph-backed semantic facts
    directly or pass through one documented adapter boundary that cannot invent
    new inference state.
  - acceptance:
    - At least one CT-eval receiver/call-target path consumes graph or
      semantic-product facts through the chosen boundary.
    - The adapter rejects missing or contradictory graph-backed dependencies
      deterministically instead of falling back to a private CT-eval cache.
    - Focused CT-eval parity tests cover one success case and one unresolved or
      contradictory diagnostic.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after the CT-eval boundary is explicit and one consumer is
    migrated; leave template inference migration to TODO-4239.

- [ ] TODO-4239: Migrate helper-routing template inference onto graph facts
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred graph and inference hardening
  - depends_on: TODO-4238
  - scope: Move one helper-routing or call-target template-inference slice onto
    graph-backed facts instead of a local ad hoc inference cache.
  - acceptance:
    - The selected template-inference slice consumes graph-backed receiver,
      call-target, or helper-routing facts.
    - Existing helper-shadow and canonical-path choices remain stable for the
      migrated slice.
    - Focused positive and negative tests pin unchanged diagnostics for
      unresolved or contradictory template inference inputs.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after one template-inference slice is graph-backed and
    covered; broader template solving remains separate follow-up work.

- [ ] TODO-4240: Add backend semantic-product conformance coverage
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred semantic-product/backend/tooling follow-up
  - depends_on: TODO-4235
  - scope: Add pipeline-facing C++/VM/native conformance coverage proving
    backend behavior consumes semantic-product facts rather than relying only
    on semantic-product formatter golden output.
  - acceptance:
    - At least one C++ backend case, one VM case, and one native case exercise
      the same semantic-product-owned fact family through lowering.
    - The tests compare the semantic-product dump or inspection surface with
      backend behavior in the same scenario.
    - Missing or stale semantic-product facts produce deterministic backend or
      lowering diagnostics in a focused negative case.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after one shared fact family has cross-backend conformance
    coverage; leave compatibility caller deletion to TODO-4241.

- [ ] TODO-4241: Retire semantic-product output compatibility callers
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred semantic-product/backend/tooling follow-up
  - depends_on: TODO-4240
  - scope: Replace remaining compatibility callers of legacy semantic-product
    output structs or adapters with explicit success/failure result variants.
  - acceptance:
    - At least one legacy semantic-product output compatibility caller is
      deleted or migrated to the explicit result variant.
    - Success and failure paths remain covered by focused tests.
    - `docs/PrimeStruct.md` no longer describes the migrated caller as a live
      compatibility seam.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after one real compatibility caller is retired and the
    replacement result API is covered; split additional callers into follow-up
    leaves if more remain.

- [ ] TODO-4242: Inventory repo-wide source-lock replacement candidates
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred semantic-product/backend/tooling follow-up
  - depends_on: TODO-4235
  - scope: Create a repo-wide inventory of source-lock tests that read private
    source files and classify each as contract-worthy, temporary migration
    lock, or stale lock with a proposed replacement surface.
  - acceptance:
    - The inventory covers source-lock tests under `tests/unit` that read
      `src/`, private headers, or checked-in docs as architecture proxies.
    - At least one stale source-lock assertion is converted to or paired with a
      public contract assertion.
    - `docs/memories.md` is updated if any remembered source-lock fact changes.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after the inventory exists and one stale lock is replaced;
    do not migrate every source-lock test in this leaf.

- [ ] TODO-4243: Improve focused backend rerun ergonomics
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred semantic-product/backend/tooling follow-up
  - depends_on: TODO-4234
  - scope: Add clearer CTest labels, helper output, or documented release-mode
    rerun commands for focused backend slices so failures do not require broad
    compile-run reruns to triage.
  - acceptance:
    - At least one backend test shard can be rerun through a documented
      release-mode CTest label or helper command.
    - The helper or label output names the matching release binary and expected
      working directory.
    - Existing full release validation behavior is unchanged.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after one high-value backend slice has ergonomic focused
    rerun support; leave broader suite partitioning to separate leaves.

- [ ] TODO-4244: Decide the `soa_vector` maturity exit
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred semantic-product/backend/tooling follow-up
  - depends_on: TODO-4229
  - scope: Choose and implement the next concrete `soa_vector` maturity step:
    accept current incubating status with explicit compatibility ownership,
    retire one remaining experimental compatibility seam, or promote one
    remaining canonical surface.
  - acceptance:
    - `docs/PrimeStruct.md`, `docs/CodeExamples.md`, and `docs/todo.md`
      describe the selected maturity decision consistently.
    - At least one compatibility seam is either retired, accepted with contract
      coverage, or promoted with canonical coverage.
    - Existing SoA example and canonical wrapper/conversion tests remain
      stable or are updated with documented migration notes.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after one real SoA compatibility or maturity decision is
    implemented and covered; do not attempt full SoA stabilization in this leaf.

- [ ] TODO-4246: Define final `soa_vector` promotion contract
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred SoA finish
  - depends_on: TODO-4244
  - scope: Replace the remaining incubating-language ambiguity with a final
    promotion contract for `soa_vector`, including which canonical APIs must
    exist, which experimental compatibility imports must retire or remain
    explicit shims, and which internal substrate paths are implementation-only.
  - acceptance:
    - `docs/PrimeStruct.md`, `docs/CodeExamples.md`, and `docs/todo.md`
      agree on the final SoA promotion contract and blocker list.
    - The contract names construction/read/ref/mutator, field-view, conversion,
      and backend-support requirements.
    - At least one focused source or docs lock proves the promotion contract
      cannot drift silently.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the final contract and blocker list are explicit and
    locked; leave implementation seam cleanup to TODO-4247 and TODO-4248.

- [ ] TODO-4247: Move canonical SoA wrapper off experimental implementation imports
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred SoA finish
  - depends_on: TODO-4246
  - scope: Move canonical `/std/collections/soa_vector/*` construction,
    read/ref, mutator, and field-view wrappers away from direct
    `/std/collections/experimental_soa_vector/*` implementation imports where a
    stable internal substrate or promoted implementation path can own the
    behavior.
  - acceptance:
    - At least construction/read/ref/mutator wrappers route through a
      non-experimental implementation path or a documented internal substrate
      adapter.
    - Direct experimental implementation imports remain only in targeted
      compatibility/conformance tests or explicit shims.
    - Canonical SoA wrapper tests remain stable across C++/VM/native where
      supported.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after the core canonical wrapper family is off direct
    experimental implementation imports; leave conversion wrappers to
    TODO-4248.

- [ ] TODO-4248: Move canonical SoA conversions off experimental conversion imports
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred SoA finish
  - depends_on: TODO-4247
  - scope: Move canonical `/std/collections/soa_vector_conversions/*` and
    canonical `to_aos`/`from_aos` helper flows away from direct
    `/std/collections/experimental_soa_vector_conversions/*` imports where a
    stable internal substrate or promoted implementation path can own the
    behavior.
  - acceptance:
    - Canonical conversion helpers use canonical `SoaVector<T>` receiver
      spellings and no longer depend on direct experimental conversion imports
      outside explicit compatibility shims.
    - AoS/SoA conversion tests cover direct calls, method sugar, and
      helper-return receivers for supported backends.
    - Unsupported conversion paths keep deterministic diagnostics.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once canonical conversion wrappers are off direct
    experimental conversion imports; leave public experimental-import
    retirement to TODO-4249.

- [ ] TODO-4249: Retire direct experimental SoA public imports
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred SoA finish
  - depends_on: TODO-4248
  - scope: Retire, gate, or explicitly reclassify direct public imports of
    `/std/collections/experimental_soa_vector/*` and
    `/std/collections/experimental_soa_vector_conversions/*` now that canonical
    wrapper and conversion paths own ordinary use.
  - acceptance:
    - Ordinary public examples and non-compatibility tests no longer import
      experimental SoA namespaces.
    - Direct experimental imports either reject with deterministic migration
      diagnostics or remain only as explicitly documented compatibility shims.
    - Compatibility coverage proves any retained experimental import path maps
      to the canonical behavior.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after direct experimental public import behavior is
    retired or reclassified and covered; leave raw builtin bridge cleanup to
    TODO-4250.

- [ ] TODO-4250: Normalize raw builtin `soa_vector` bridges onto canonical wrappers
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred SoA finish
  - depends_on: TODO-4249
  - scope: Remove remaining compiler-owned raw `soa_vector<T>` bridge
    scaffolding that routes through special lowerer/emitter/backend behavior
    when the canonical `SoaVector<T>` wrapper surface can own the same
    semantics.
  - acceptance:
    - At least one remaining raw builtin `soa_vector<T>` count/get/ref,
      push/reserve, field-view, or conversion bridge is deleted or converted to
      a canonical wrapper path.
    - Same-path user-helper shadowing and canonical helper diagnostics remain
      stable for the migrated bridge family.
    - Missing canonical wrapper facts fail deterministically instead of using a
      hidden raw-builtin fallback.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after one real raw-builtin bridge family is removed or
    normalized; add follow-up leaves if the audit finds unrelated bridge
    families.

- [ ] TODO-4251: Add full cross-backend SoA parity coverage
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred SoA finish
  - depends_on: TODO-4250
  - scope: Add a focused cross-backend SoA parity matrix for canonical
    construction/read/ref/mutator, field-view, and conversion flows after the
    compatibility and raw-builtin bridge cleanup.
  - acceptance:
    - C++/VM/native coverage exercises the same canonical SoA wrapper program
      for construction/read/ref/mutator behavior where supported.
    - Field-view and AoS/SoA conversion coverage exists for each supported
      backend, with deterministic unsupported diagnostics elsewhere.
    - The matrix uses canonical imports only except for explicit compatibility
      assertions.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once one canonical SoA parity matrix covers the required
    flow families; leave final docs promotion to TODO-4252.

- [ ] TODO-4252: Promote `soa_vector` docs after compatibility cleanup
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred SoA finish
  - depends_on: TODO-4251
  - scope: Promote `soa_vector` from incubating extension to the documented
    public collection contract once compatibility seams, raw-builtin bridges,
    and cross-backend parity gates are complete.
  - acceptance:
    - `docs/PrimeStruct.md`, `docs/CodeExamples.md`, checked-in examples, and
      `docs/todo.md` consistently describe `soa_vector` as promoted or record
      any remaining explicitly accepted compatibility exception.
    - The canonical `core` / `hybrid` / `stdlib-owned` matrix is updated for
      the final SoA ownership classification.
    - No ordinary public example imports experimental SoA namespaces.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after docs, examples, ownership matrix, and tests reflect
    the final SoA status; unrelated dynamic storage work remains separate.

- [ ] TODO-4245: Plan dynamic vector growth and runtime storage support
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred semantic-product/backend/tooling follow-up
  - depends_on: TODO-4224
  - scope: Add the first concrete runtime/storage design slice for dynamic
    vector growth beyond current fixed-capacity behavior, including one
    executable prototype or guarded runtime helper path.
  - acceptance:
    - The design records ownership for allocation, layout, drop, and capacity
      growth across C++/VM/native support boundaries.
    - One focused runtime or compile-run case exercises the prototype or
      guarded helper path without changing unsupported backend behavior
      silently.
    - Unsupported backends keep deterministic diagnostics for dynamic growth.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after one executable storage-support slice is designed and
    prototyped or gated; full dynamic collection storage remains follow-up
    work.
