# PrimeStruct TODO Log

## Operating Rules (Human + AI)

1. This file contains only open work (`[ ]` or `[~]`).
2. Use one task block per item with a stable ID: `TODO-XXXX`.
3. Keep task ordering aligned with dependency and execution priority. New
   tasks should be inserted into the relevant lane/queue position rather than
   blindly prepended by creation date.
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
9. Every task block must be implementable by someone arriving with no session context, including an AI agent. Include
   enough local detail to identify the intended behavior, likely source/test areas, dependencies, diagnostics, and
   validation gate without relying on prior conversation.
10. Implementation tasks SHOULD include `implementation_notes` when source entry points, nearby tests, migration
    hazards, or validation strategy are not obvious from scope and acceptance alone.
11. Keep the `Execution Queue` and coverage snapshots current when adding/removing tasks.
    When adding, splitting, renaming, completing, or deleting a task, update every
    reference to that TODO ID in lanes, queues, snapshots, `depends_on`, stop
    rules, and notes; use `rg TODO-XXXX docs/todo.md` to catch stale references.
12. Keep an explicit `Ready Now` shortlist synced with dependencies; only items with no unmet TODO dependencies belong there.
13. When splitting broad tasks, update parent task scope to avoid duplicated acceptance criteria across child tasks.
14. Keep `Priority Lanes` aligned with queue order so critical-path tasks remain visible.
15. For phase-level tracking tasks, pair planning trackers with explicit acceptance-gate tasks before marking a phase complete.
16. Every active leaf must include a stop rule and deliver at least one of:
   - user-visible behavior change
   - measurable perf/memory improvement
   - deletion of a real compatibility subsystem
17. Avoid standalone micro-cleanups (alias renames, trivial bool rewrites, local dedup) unless bundled into one value outcome above.
18. If a leaf misses its value target after 2 attempts, archive it as low-value and replace it with a different hotspot.
19. Keep the live execution queue short: no more than 8 leaf tasks in `Ready Now` at once. Additional dependency-blocked or deferred follow-up leaves may stay in the task blocks and `Immediate Next 10`, but only `Ready Now` counts as the live queue cap.
20. Treat disabled tests as debt: every retained `doctest::skip(true)` cluster must either map to an active TODO leaf with a clear re-enable-or-delete outcome, or be removed once proven stale.

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
  - implementation_notes: optional, but required when source/test entry points are not obvious
  - acceptance:
    - ...
    - ...
  - stop_rule: ...
  - notes: optional
```

## Open Tasks

### Ready Now (Live Leaves; No Unmet TODO Dependencies)

- TODO-4214: Introduce deterministic worker result bundles

### Immediate Next 10 (After Ready Now)

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

### Priority Lanes (Current)

- Semantic phase contract hardening: TODO-4214 -> TODO-4227 -> TODO-4215
  -> TODO-4228 -> TODO-4216 -> TODO-4217 -> TODO-4224 -> TODO-4229 -> TODO-4230
  -> TODO-4218 -> TODO-4231 -> TODO-4219 -> TODO-4225 -> TODO-4232
  -> TODO-4233 -> TODO-4220 -> TODO-4234 -> TODO-4221 -> TODO-4235
- Deferred graph and inference hardening: TODO-4236 -> TODO-4237
  -> TODO-4238 -> TODO-4239
- Deferred semantic-product/backend/tooling follow-ups: TODO-4240
  -> TODO-4241; TODO-4242 -> TODO-4243; TODO-4245
- Deferred SoA finish: TODO-4244 -> TODO-4246 -> TODO-4247
  -> TODO-4248 -> TODO-4249 -> TODO-4250 -> TODO-4251 -> TODO-4252
- Deferred algebraic types and brace-only construction: TODO-4253
  -> TODO-4254 -> TODO-4255 -> TODO-4256 -> TODO-4257
  -> TODO-4258 -> TODO-4259 -> TODO-4260 -> TODO-4261 -> TODO-4262
- Deferred stdlib ADT migration: TODO-4263 -> TODO-4264 -> TODO-4265
  -> TODO-4266 -> TODO-4267
- Deferred generic tuple substrate: TODO-4268 -> TODO-4269 -> TODO-4270
  -> TODO-4275 -> TODO-4276 -> TODO-4271 -> TODO-4272 -> TODO-4274
  -> TODO-4273 -> TODO-4277 -> TODO-4278

### Execution Queue (Recommended)

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
- TODO-4253: Implement brace-only construction semantics
- TODO-4254: Migrate generated construction surfaces
- TODO-4255: Migrate collection construction surfaces
- TODO-4256: Classify constructor-shaped helper compatibility
- TODO-4257: Add sum declaration metadata and layout
- TODO-4258: Add explicit sum construction
- TODO-4259: Add inferred sum variant construction
- TODO-4260: Add `pick` semantic validation
- TODO-4261: Lower and execute `pick` matches
- TODO-4262: Add public sum-type examples
- TODO-4263: Design generic and unit sum variants
- TODO-4264: Add stdlib-owned `Maybe<T>` sum
- TODO-4265: Add stdlib-owned `Result<T, E>` sum
- TODO-4266: Rewire `?` to the `Result` sum contract
- TODO-4267: Retire legacy Maybe/Result representations
- TODO-4268: Add heterogeneous type-pack syntax and metadata
- TODO-4269: Bind and monomorphize type-pack arguments
- TODO-4270: Add compile-time integer template arguments
- TODO-4275: Expand type packs into struct storage
- TODO-4276: Expand type packs in helpers and lifecycle hooks
- TODO-4271: Add compile-time pack indexing
- TODO-4272: Add stdlib `tuple<Ts...>`
- TODO-4274: Add tuple bracket indexing sugar
- TODO-4273: Add heterogeneous value-pack inference
- TODO-4277: Add tuple destructuring sugar
- TODO-4278: Integrate multi-wait with stdlib tuple

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | TODO-4214, TODO-4227, TODO-4218, TODO-4231, TODO-4236, TODO-4237, TODO-4238, TODO-4239 |
| Compile-pipeline stage and publication-boundary contracts | TODO-4220, TODO-4234 |
| Compile-time macro hooks and AST transform ownership | TODO-4238, TODO-4239 |
| Stdlib surface-style alignment and public helper readability | none |
| Stdlib bridge consolidation and collection/file/gfx surface authority | TODO-4217, TODO-4224, TODO-4229, TODO-4230, TODO-4244, TODO-4246, TODO-4247, TODO-4248, TODO-4249 |
| Vector/map stdlib ownership cutover and collection surface authority | TODO-4217, TODO-4224, TODO-4245 |
| Stdlib de-experimentalization and public/internal namespace cleanup | none |
| SoA maturity and `soa_vector` promotion | TODO-4244, TODO-4246, TODO-4247, TODO-4248, TODO-4249, TODO-4250, TODO-4251, TODO-4252 |
| Validator entrypoint and benchmark-plumbing split | TODO-4214, TODO-4227 |
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
| Algebraic sum types and brace-only construction | TODO-4253, TODO-4254, TODO-4255, TODO-4256, TODO-4257, TODO-4258, TODO-4259, TODO-4260, TODO-4261, TODO-4262 |
| Stdlib ADT migration for `Maybe` and `Result` | TODO-4263, TODO-4264, TODO-4265, TODO-4266, TODO-4267 |
| Generic type packs and tuple stdlib surface | TODO-4268, TODO-4269, TODO-4270, TODO-4275, TODO-4276, TODO-4271, TODO-4272, TODO-4274, TODO-4273, TODO-4277, TODO-4278 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | TODO-4215, TODO-4228, TODO-4219, TODO-4225, TODO-4232, TODO-4233, TODO-4220, TODO-4240, TODO-4241 |
| AST transform hook conformance | TODO-4238, TODO-4239 |
| CodeExamples-aligned stdlib surface syntax conformance | TODO-4262 |
| Compile-pipeline stage handoff conformance | TODO-4220, TODO-4234, TODO-4240 |
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
| Sum-type and brace-construction conformance | TODO-4253, TODO-4254, TODO-4255, TODO-4256, TODO-4257, TODO-4258, TODO-4259, TODO-4260, TODO-4261, TODO-4262 |
| Maybe/Result sum migration conformance | TODO-4263, TODO-4264, TODO-4265, TODO-4266, TODO-4267 |
| Generic type-pack and tuple conformance | TODO-4268, TODO-4269, TODO-4270, TODO-4275, TODO-4276, TODO-4271, TODO-4272, TODO-4274, TODO-4273, TODO-4277, TODO-4278 |

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

- [ ] TODO-4253: Implement brace-only construction semantics
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred algebraic types and brace-only construction
  - depends_on: TODO-4228
  - scope: Move user-facing value construction onto `Type{...}` and
    context-typed `{...}` forms so `Type(...)` remains ordinary execution/call
    syntax rather than struct construction.
  - implementation_notes:
    - Start from `include/primec/Ast.h`, `src/parser/ParserExpr.cpp`,
      `src/parser/ParserLists.cpp`, `src/semantics/SemanticsValidatorExprStructConstructors.cpp`,
      `src/semantics/SemanticsValidatorExprBlock.cpp`, and initializer
      inference helpers under `src/semantics/`.
    - Add focused coverage near parser tests, `test_semantics_bindings_struct_defaults.cpp`,
      and the existing struct/manual semantics tests before broad compile-run
      coverage.
    - Preserve statement-position `name{...}` binding behavior while adding a
      canonical value-constructor representation that later TODOs can reuse.
  - acceptance:
    - Parser and semantic tests cover labeled and positional `Type{...}` struct
      construction plus context-typed binding initializers.
    - Binding-vs-constructor disambiguation is explicit: `name{...}` remains a
      binding in statement position, while value positions and typed
      initializers can form constructors deterministically.
    - Constructor diagnostics cover unknown labels, duplicate labels, too many
      positional entries, missing required fields, ambiguous context-typed
      `{...}` initializers, and accidental `Type(...)` field-mapping attempts.
    - `Type(...)` no longer maps to struct fields on the migrated path and
      produces deterministic diagnostics or ordinary call behavior.
    - Omitted-initializer rewrites and zero-field/default struct construction
      use brace construction in the canonicalized AST.
    - Docs and user-facing examples use brace construction for structs.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once ordinary struct construction has a brace-only
    production path and old call-shaped struct construction is not silently used
    for field mapping; leave generated-surface migration to TODO-4254.

- [ ] TODO-4254: Migrate generated construction surfaces
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred algebraic types and brace-only construction
  - depends_on: TODO-4253
  - scope: Move compiler-generated construction paths onto the brace-only
    contract, limited to enum static values, omitted-default rewrites, and
    reflection-generated `Default`/`Clone`/`Clear` helpers.
  - implementation_notes:
    - Start from enum expansion and reflection generation code under
      `src/semantics/SemanticsValidateReflectionGeneratedHelpers*`,
      `src/semantics/SemanticsValidateReflectionMetadata*`, and any omitted
      initializer rewrite path left after TODO-4253.
    - Lock behavior with `test_semantics_enum.cpp`,
      `test_semantics_capabilities_structs_reflect_default.cpp`,
      `test_semantics_capabilities_structs_reflect_clear.cpp`, and
      `test_compile_run_reflection_codegen*.cpp`.
    - Treat generated helper calls such as `/Type/Default()` as helper calls;
      only the generated value materialization inside those helpers should use
      brace construction.
  - acceptance:
    - Enum expansion, reflected `Default`/`Clone`/`Clear`, omitted defaults,
      and zero-field/default struct helper generation use brace construction in
      the canonicalized AST.
    - Focused parser/semantic or IR tests lock the generated forms and confirm
      no user-visible constructor-call semantics are synthesized.
    - User-facing docs no longer present `Type(...)`, `vector<T>(...)`, or
      `map<K, V>(...)` as value construction syntax.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after compiler-generated construction surfaces are
    brace-backed; leave collection rewrites to TODO-4255.

- [ ] TODO-4255: Migrate collection construction surfaces
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred algebraic types and brace-only construction
  - depends_on: TODO-4254
  - scope: Move `array<T>`, `vector<T>`, `map<K, V>`, and `soa_vector<T>`
    literal/builder rewrites toward brace construction without changing
    unrelated stdlib or hybrid helper calls.
  - implementation_notes:
    - Start from `src/semantics/SemanticsValidatorExprCollectionLiterals.cpp`,
      `src/semantics/TemplateMonomorphExperimentalCollectionConstructorRewrites.h`,
      `src/semantics/TemplateMonomorphCollectionCompatibilityPaths.h`,
      `include/primec/StdlibSurfaceRegistry.h`, and collection setup/lowering
      helpers under `src/ir_lowerer/`.
    - Add tests beside existing collection semantics shards and compile-run
      shards such as `test_compile_run_*collections*`,
      `test_compile_run_emitters_map_access_and_collection_rewrites.cpp`, and
      VM/native collection literal tests.
    - Keep map entry syntax and bracket aliases in the same change only where
      they share the collection literal rewrite path.
  - acceptance:
    - Collection literal rewrites no longer require user-visible
      constructor-call semantics for value construction.
    - Legacy `vector<T>(...)`, `map<K, V>(...)`, and similar compatibility
      helper calls either route through a documented bridge or produce stable
      diagnostics.
    - Focused tests cover brace collection construction, bracket aliases,
      map-entry lowering, and invalid labeled/positional collection cases.
    - User-facing collection docs prefer brace construction and mark old
      call-shaped forms as compatibility helpers only.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after collection construction syntax is brace-backed or
    explicitly bridge-owned; leave non-collection helper compatibility to
    TODO-4256.

- [ ] TODO-4256: Classify constructor-shaped helper compatibility
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred algebraic types and brace-only construction
  - depends_on: TODO-4255
  - scope: Inventory remaining constructor-shaped stdlib, graphics, file,
    optional, and buffer helper surfaces and classify each as a helper call,
    migration diagnostic, or follow-up implementation task.
  - implementation_notes:
    - Start from `include/primec/StdlibSurfaceRegistry.h`,
      `src/semantics/SemanticsValidateExperimentalGfxConstructors.cpp`,
      file/result helpers in semantics and `src/ir_lowerer/IrLowererFileWriteHelpers.*`,
      maybe helpers, and imported collection alias tests.
    - Record the inventory in `docs/PrimeStruct.md` or a tightly scoped table
      in this TODO section before changing behavior.
    - Prefer focused semantic/IR tests that prove retained forms resolve as
      calls to named helpers, not constructor/value forms.
  - acceptance:
    - Remaining surfaces such as `File<Mode>(path)`, `Buffer<T>(count)`,
      `Window(...)`, `Device()`, `Maybe()`/`none<T>()`, and imported
      collection aliases are listed with their intended status.
    - Each retained compatibility form has focused tests proving it is a helper
      call, not value construction.
    - Any compatibility form too large to migrate in this task gets its own
      follow-up TODO with scope, acceptance, and stop_rule.
    - Public docs mark retained constructor-shaped forms as compatibility or
      helper calls rather than construction syntax.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after remaining constructor-shaped forms are inventoried
    and either tested, migrated, or split into follow-up TODOs.

- [ ] TODO-4257: Add sum declaration metadata and layout
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred algebraic types and brace-only construction
  - depends_on: TODO-4256
  - scope: Add parser, AST, semantic-product, import/visibility, and
    backend-neutral metadata support for `[sum]` declarations without adding
    value construction or `pick` matching yet.
  - implementation_notes:
    - Start from `include/primec/Ast.h`, parser definition-body files under
      `src/parser/`, `include/primec/SemanticProduct.h`,
      `src/semantics/SemanticPublicationBuilders.h`,
      `src/semantics/SemanticsDefinitionPrepass.*`, and layout/type metadata
      setup under `src/semantics/`.
    - Add a dedicated semantic shard such as `test_semantics_sum_types.cpp`
      plus semantic-product or IR-layout snapshot coverage rather than hiding
      the feature inside general struct tests.
    - Keep sum declaration support parseable and diagnosable even if
      construction and `pick` still report unsupported-feature diagnostics.
  - acceptance:
    - Parser and semantic tests accept `[sum] Shape { [Circle] circle ... }`.
    - The AST and semantic product record sum metadata deterministically:
      variant order, variant name, payload envelope, visibility/import
      behavior, and a backend-neutral active-tag/payload layout contract.
    - Sum declarations reject duplicate variant names, unsupported payload
      envelopes, missing payload envelopes, and invalid field-like modifiers
      with stable diagnostics.
    - IR/layout metadata snapshots include enough information for later
      construction and `pick` lowering without backend-specific assumptions.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once sum declarations are represented and validated
    deterministically; leave construction to TODO-4258.

- [ ] TODO-4258: Add explicit sum construction
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred algebraic types and brace-only construction
  - depends_on: TODO-4257
  - scope: Add explicit sum value construction through `[variant] payload`
    entries when the target sum type is known from context.
  - implementation_notes:
    - Reuse the brace-construction machinery from TODO-4253 and sum metadata
      from TODO-4257; add a narrow sum-construction resolver instead of
      special-casing it inside unrelated collection or struct paths.
    - Cover typed local bindings, return values, fields, and `auto`/omitted
      envelope rejection or inference behavior explicitly.
    - Add positive and negative tests in the dedicated sum semantics shard
      before adding backend execution tests.
  - acceptance:
    - Explicit sum construction accepts `[Shape] s{[circle] Circle{...}}`.
    - Explicit construction also accepts positional payload construction such
      as `[Shape] s{[circle] Circle{3.4}}`.
    - Unknown variant labels, duplicate variant entries, missing payloads, and
      payload type mismatches produce stable diagnostics.
    - Payload ownership and move/copy behavior is checked consistently with
      ordinary brace construction.
    - Construction without a target sum context is rejected unless the source
      names the sum type explicitly through a documented form.
    - Empty sum initializers such as `[Shape] s{}` produce a deterministic
      diagnostic until the unit-variant default rule lands in TODO-4263.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once explicit variant construction works and has negative
    diagnostics; leave inferred variant selection to TODO-4259.

- [ ] TODO-4259: Add inferred sum variant construction
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred algebraic types and brace-only construction
  - depends_on: TODO-4258
  - scope: Add target-typed inferred variant construction, where a payload
    without an explicit `[variant]` label selects a sum variant only when the
    match is unique.
  - implementation_notes:
    - Build on the explicit-construction resolver from TODO-4258; the inferred
      path should be a thin target-typed selection layer over the same payload
      validation.
    - Add diagnostics that include the target sum and candidate variant names so
      ambiguity can be fixed mechanically by adding `[variant]`.
    - Cover interactions with imports, duplicate payload envelopes, struct
      payload defaults, and no implicit conversions.
  - acceptance:
    - Inferred variant construction accepts `[Shape] s{Circle{...}}` only when
      exactly one variant accepts the payload type.
    - Zero-match and multi-match inferred construction produce deterministic
      diagnostics that name the relevant variants.
    - Matching uses the same conversion/no-implicit-conversion rules as
      ordinary construction and is deterministic across imports.
    - Tests cover duplicate payload envelopes, subtype/template-like ambiguity
      if present, and successful inference with all three accepted forms.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once sums can be declared and constructed with deterministic
    inferred variant-selection semantics; leave `pick` matching to TODO-4260.

- [ ] TODO-4260: Add `pick` semantic validation
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred algebraic types and brace-only construction
  - depends_on: TODO-4259
  - scope: Add parsing and semantic validation for
    `pick(sumValue) { variant(payload) { ... } }`, including exhaustiveness,
    arm binding, branch typing, and diagnostics, without requiring backend
    execution support yet.
  - implementation_notes:
    - Start from control-flow parsing/validation in `src/parser/ParserExprControl.cpp`,
      `src/semantics/SemanticsValidatorStatementControlFlow.cpp`,
      `src/semantics/SemanticsValidatorInferControlFlow.cpp`, and block/result
      validation helpers.
    - Add a dedicated semantic shard for `pick` diagnostics and result/effect
      joining before any backend lowering.
    - Unsupported lowering should fail at a clear semantic or IR-preparation
      boundary until TODO-4261 lands.
  - acceptance:
    - `pick` requires every sum variant to be handled unless a future explicit
      fallback form is added and documented.
    - Duplicate arms, unknown variant arms, missing payload binders, and binder
      type mismatches produce stable diagnostics.
    - Branch payload bindings have the declared variant payload type and obey
      ownership/borrow rules.
    - Branch result types and effects are validated deterministically.
    - Statement-position and value-position `pick` behavior is specified and
      tested, including non-void result joining and `return(...)` inside arms.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once `pick` is semantically validated and unsupported
    execution paths are deterministic; leave lowering to TODO-4261.

- [ ] TODO-4261: Lower and execute `pick` matches
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred algebraic types and brace-only construction
  - depends_on: TODO-4260
  - scope: Lower sum construction and `pick` to executable backend behavior for
    the supported backend set, with deterministic diagnostics elsewhere.
  - implementation_notes:
    - Add explicit IR representation or documented lowering conventions in
      `include/primec/Ir.h`, IR serialization/dump code, and narrow
      `src/ir_lowerer/` units rather than growing broad include fragments.
    - Cover C++ emitter, VM/native runtime, and unsupported backend diagnostics
      separately; do not silently route unsupported sum payloads through struct
      or integer fallbacks.
    - Add IR pipeline tests before compile-run tests so tag/payload lowering is
      easy to diff.
  - acceptance:
    - IR lowering emits an explicit tag dispatch plus payload access model.
    - Exactly one active payload is moved, borrowed, or dropped according to the
      selected branch; inactive variants cannot be observed.
    - C++/VM/native or explicitly supported backend tests execute representative
      sum construction and `pick` programs.
    - Unsupported backend/type combinations fail with deterministic diagnostics
      instead of falling back silently.
    - Serialized IR changes are documented or versioned where needed.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once representative sum construction and `pick` execution
    works on supported backends and unsupported paths are deterministic.

- [ ] TODO-4262: Add public sum-type examples
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred algebraic types and brace-only construction
  - depends_on: TODO-4261
  - scope: Add runnable algebraic sum examples to `README.md` and
    `docs/CodeExamples.md` once `[sum]` construction and `pick` matching are
    implemented.
  - implementation_notes:
    - Update `README.md`, `docs/CodeExamples.md`, and any example locks or
      documentation tests that read snippets, especially
      `test_compile_run_examples_docs.cpp` and
      `test_compile_run_examples_docs_locks.cpp`.
    - Keep examples minimal and runnable; do not add tutorial prose that is not
      enforced by tests.
    - Prefer a compact `Shape` example in README and a broader style example in
      `CodeExamples.md` that includes the ambiguity rule.
  - acceptance:
    - `README.md` includes a compact `Shape`-style example using `[sum]`,
      brace-only construction, inferred variant construction where
      unambiguous, and exhaustive `pick`.
    - `docs/CodeExamples.md` includes a style-aligned example that demonstrates
      all three valid sum construction forms plus an exhaustive `pick`.
    - The examples include a short ambiguity note or negative snippet for the
      inferred payload form when two variants accept the same payload envelope.
    - The examples are runnable or covered by the existing example/style
      validation path.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop after the public examples are added and validated; leave
    broader tutorial or cookbook material for a separate docs task.

- [ ] TODO-4263: Design generic and unit sum variants
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred stdlib ADT migration
  - depends_on: TODO-4262
  - scope: Extend the sum-type design for the features needed to express
    stdlib `Maybe<T>` and `Result<T, E>` as ordinary algebraic data types:
    generic sums and unit/no-payload variants.
  - implementation_notes:
    - This is a gated design/implementation slice after core `[sum]` and
      `pick` lowering are complete; do not pull it into TODO-4257 through
      TODO-4261.
    - Start from the sum parser/metadata/resolver introduced by TODO-4257
      through TODO-4259 and template monomorphization paths under
      `src/semantics/TemplateMonomorph*.h`.
    - Unit variants must support bare lowerCamelCase names such as `none`;
      document how bare variants are represented in the semantic product and
      IR alongside payload-carrying variants.
    - Default construction must follow the source-order tag model: `[Sum] s{}`
      is valid only when the first variant is unit, and it selects that
      variant/tag without constructing hidden payloads.
  - acceptance:
    - Generic sum declarations such as `Maybe<T>` and `Result<T, E>` parse,
      validate, monomorphize, and expose deterministic semantic-product
      metadata.
    - Unit variants are supported with bare names, including `none`, and can be
      constructed, matched, serialized in IR, and diagnosed without an empty
      struct payload workaround.
    - `pick` supports unit arms such as `none { ... }` and rejects payload
      binders on unit variants.
    - Default construction accepts sums whose first variant is unit and rejects
      sums whose first variant has a payload, even if that payload type is
      default-constructible.
    - Docs note that variant order controls tag order/default behavior and is
      layout/serialization-sensitive.
    - Negative tests cover duplicate generic variants, invalid unit payload
      syntax, unsupported recursive inline payloads, and template inference
      ambiguity.
    - `docs/PrimeStruct.md` and `docs/PrimeStruct_SyntaxSpec.md` record the
      generic/unit-variant contract and note that Maybe/Result migration is
      still deferred to TODO-4264 and TODO-4265.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once generic and unit sum syntax is specified, implemented,
    and covered well enough for stdlib `Maybe`/`Result` migration.

- [ ] TODO-4264: Add stdlib-owned `Maybe<T>` sum
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred stdlib ADT migration
  - depends_on: TODO-4263
  - scope: Re-express `Maybe<T>` as a stdlib-owned sum type while preserving
    the existing public optional-value helper surface.
  - implementation_notes:
    - Start from current Maybe docs, stdlib Maybe files, maybe semantics tests,
      and VM/native Maybe compile-run tests.
    - Keep compatibility wrappers such as `some<T>(value)`, `none<T>()`,
      `isEmpty()`, `isSome()`, `set(value)`, `clear()`, and `take()` until
      this TODO explicitly removes or reclassifies them.
    - Do not migrate `Result` or `?` in this item; use this as the smaller
      proof that ordinary stdlib code can own a generic sum surface.
  - acceptance:
    - `Maybe<T>` is declared as a sum with a unit `none` variant and a
      payload-carrying `some` variant.
    - `[Maybe<T>] value{}` defaults to `none`, and `[Maybe<T>] value{none}`
      remains the explicit equivalent.
    - Existing Maybe helper behavior and diagnostics remain source-compatible
      unless an explicit migration diagnostic is documented and tested.
    - Compile-run coverage proves `none`, `some`, `isEmpty`/`isSome`, `take`,
      and `clear` on supported payload kinds.
    - Docs mark the old struct/storage representation as retired or
      compatibility-only.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once `Maybe<T>` is stdlib-owned on top of sum semantics and
    existing Maybe behavior is preserved or intentionally migrated.

- [ ] TODO-4265: Add stdlib-owned `Result<T, E>` sum
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred stdlib ADT migration
  - depends_on: TODO-4264
  - scope: Re-express `Result<T, E>` and status-only `Result<E>` as
    stdlib-owned sum types while preserving the current helper/combinator
    surface before changing `?`.
  - implementation_notes:
    - Start from Result helper semantics, `Result.ok/error/why`,
      `Result.map/and_then/map2`, result payload metadata, and VM/native/C++
      result compile-run tests.
    - Keep `?` on the existing runtime/semantic contract until TODO-4266; this
      item should make Result values sum-backed without changing propagation.
    - Status-only `Result<Error>` should use a unit success variant plus an
      error payload variant instead of a synthetic empty struct payload.
  - acceptance:
    - Value-carrying and status-only `Result` forms are represented through the
      sum contract from TODO-4263, including a unit success variant for
      status-only results.
    - Status-only `Result<Error>{}` defaults to the unit success variant, while
      value-carrying `Result<T, Error>{}` is rejected unless an explicit
      variant is selected.
    - Existing helper/combinator behavior remains source-compatible for the
      supported payload matrix.
    - Diagnostics remain deterministic for unsupported payloads and ambiguous
      generic inference.
    - Focused tests cover `Result.ok`, error construction, `Result.error`,
      `Result.why`, map/and_then/map2, and representative error domains
      (`FileError`, `ContainerError`, `GfxError`).
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once `Result` values are sum-backed and helper behavior is
    stable; leave `?` propagation rewiring to TODO-4266.

- [ ] TODO-4266: Rewire `?` to the `Result` sum contract
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred stdlib ADT migration
  - depends_on: TODO-4265
  - scope: Make postfix `?`, `try(...)`, and `on_error` propagation consume the
    stdlib-owned `Result` sum shape instead of bespoke Result metadata or
    legacy runtime representation.
  - implementation_notes:
    - Start from result/try/on_error semantic validation, result metadata in
      `include/primec/SemanticProduct.h`, and IR lowerer result helpers.
    - Preserve current user-facing `?` behavior first; any broader propagation
      syntax changes should be split into separate TODOs.
    - Add semantic-product and IR tests before broad compile-run tests so the
      result-sum contract is observable.
  - acceptance:
    - `?` unwraps the `ok`/success variant and propagates the `error` variant
      using the same observable behavior as the current Result contract.
    - `on_error` handlers and status-code entry flows still work for current
      file, image, collection, and gfx error domains.
    - Unsupported or malformed Result-like sums do not accidentally participate
      in `?` unless they match the documented Result contract.
    - VM/native/C++ tests cover direct Result values, map/and_then/map2
      sources, borrowed/pointer Result values, and status-only results.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once `?` and `try(...)` are sum-backed for the supported
    Result surface and unsupported cases fail deterministically.

- [ ] TODO-4267: Retire legacy Maybe/Result representations
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred stdlib ADT migration
  - depends_on: TODO-4266
  - scope: Remove or quarantine old compiler/runtime special cases for Maybe
    and Result after both are stdlib-owned sums and `?` consumes the Result sum
    contract.
  - implementation_notes:
    - Inventory bespoke Maybe/Result code in semantics, semantic product,
      lowerer, VM/native runtime helpers, C++ emitter, docs, and tests before
      deleting behavior.
    - Keep compatibility helpers only when they are implemented in stdlib or
      routed through documented helper-call bridges.
    - Split any deletion that risks changing unrelated error domains or payload
      support into a follow-up TODO.
  - acceptance:
    - No production path silently depends on the retired Maybe/Result struct or
      bespoke runtime representation.
    - Compatibility helpers either live in stdlib, have explicit bridge tests,
      or produce deterministic migration diagnostics.
    - Docs and examples describe Maybe/Result as sums and remove obsolete
      representation claims.
    - Release, compile-run, semantic-product, and IR tests cover the migrated
      Maybe/Result surface.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once legacy Maybe/Result special cases are removed,
    quarantined behind named compatibility bridges, or split into explicit
    follow-up TODOs.

- [ ] TODO-4268: Add heterogeneous type-pack syntax and metadata
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4253
  - scope: Add parser, AST, semantic-product, and diagnostic support for
    heterogeneous type-parameter packs such as `tuple<Ts...>` without adding
    pack expansion or tuple implementation yet.
  - implementation_notes:
    - Start from `docs/TuplePrototype.md`,
      `docs/PrimeStruct_SyntaxSpec.md`, `include/primec/Ast.h`, parser
      template-list handling under `src/parser/`, template setup under
      `src/semantics/TemplateMonomorph*.h`, and semantic-product metadata.
    - Current `args<T>` is homogeneous value-pack support and must not be
      reused as the representation for heterogeneous type packs.
    - Keep the syntax generic; do not add tuple-specific parsing rules.
  - acceptance:
    - Definitions and structs can declare a single type pack parameter using a
      documented syntax such as `Ts...`.
    - The AST and semantic product distinguish ordinary template parameters
      from pack parameters deterministically.
    - Pack parameters can appear in type positions only where the language has
      explicitly documented them as valid; unsupported placements produce
      stable diagnostics.
    - Parser/semantic tests reject duplicate pack names, more than one pack
      when not supported, packs before ordinary template parameters if that is
      disallowed, and `args<T>` misuse as a heterogeneous pack.
    - `docs/TuplePrototype.md` and `docs/PrimeStruct_SyntaxSpec.md` record the
      accepted syntax and note that binding is deferred to TODO-4269 and
      expansion is deferred to TODO-4275/TODO-4276.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once heterogeneous type-pack declarations are parseable,
    represented, and diagnosable; leave binding and monomorphization to
    TODO-4269.

- [ ] TODO-4269: Bind and monomorphize type-pack arguments
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4268
  - scope: Add template binding and monomorphization support for an arbitrary
    number of type arguments bound to a type-pack parameter, without expanding
    pack fields or helper bodies yet.
  - implementation_notes:
    - Build on the pack metadata from TODO-4268 and the existing template
      monomorphization path; avoid generating source-level `tuple1`/`tuple2`
      families.
    - Keep the output as monomorphized pack metadata and deterministic
      specialized names that later expansion tasks can consume.
    - Treat pack argument order as source-order and deterministic because tuple
      layout will depend on it.
  - acceptance:
    - A definition or struct with `Ts...` can be specialized with zero, one, or
      multiple concrete type arguments and records the bound pack contents
      deterministically.
    - Specialized names, semantic-product metadata, and serialized diagnostics
      preserve pack argument order across repeated builds and import order.
    - Ordinary non-pack template parameters continue to monomorphize exactly as
      before.
    - Monomorphized names and serialized diagnostics are deterministic across
      repeated builds and import order.
    - Negative tests cover missing pack arguments where disallowed, conflicting
      pack inference, unsupported multiple packs, and attempts to use pack
      expansion before TODO-4275.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once type-pack arguments bind and monomorphize
    deterministically; leave field expansion to TODO-4275 and helper/lifecycle
    expansion to TODO-4276.

- [ ] TODO-4270: Add compile-time integer template arguments
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4268
  - scope: Extend template arguments to support compile-time integer values
    needed by APIs such as `get<0>(tuple)` and
    `tuple_element<1, tuple<...>>()`.
  - implementation_notes:
    - Current template arguments are parsed as envelope/type names. Add a
      typed representation for non-type template arguments rather than
      smuggling integer indexes through strings.
    - Start from `include/primec/Ast.h`, parser template-list code,
      template-argument validation, AST/IR printers, semantic diagnostics, and
      tests that currently assume template args are strings.
    - Keep this generic; do not special-case the name `get`.
  - acceptance:
    - Calls and method calls accept integer template arguments in documented
      positions, for example `get<0>(value)` and `value.get<1>()`.
    - Non-type template arguments are represented distinctly from type
      template arguments through parser, AST printer, semantic validation, and
      monomorphization.
    - Invalid non-constant expressions, negative indexes if unsupported,
      floats, strings, and bools produce stable diagnostics.
    - Existing type-template behavior for collections, transforms, and
      user-defined helpers remains source-compatible.
    - Focused tests cover parsing, printing, semantic rejection, and at least
      one generic helper that accepts an integer template argument.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once integer template arguments are a generic language
    feature with diagnostics; leave pack-index lookup to TODO-4271.

- [ ] TODO-4275: Expand type packs into struct storage
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4269
  - scope: Add `.prime` struct-field expansion for heterogeneous type packs so
    a generic struct can store one field per bound type, without adding helper
    body expansion or tuple-specific APIs yet.
  - implementation_notes:
    - Build on type-pack monomorphization from TODO-4269.
    - The expansion syntax should stay generic and documented in
      `docs/PrimeStruct_SyntaxSpec.md`; `tuple<Ts...>` is only the first known
      consumer.
    - Use deterministic internal field names for expanded storage so semantic
      product, diagnostics, and lowering do not depend on source text hacks.
  - acceptance:
    - A `.prime` generic struct can expand a type pack into one stored field
      per bound type with deterministic field order and stable internal names.
    - Pack-expanded fields participate in brace construction and field
      initialization for zero, one, and multi-element packs.
    - Semantic-product or layout snapshots expose enough field metadata for
      later indexed access without tuple-specific facts.
    - Negative tests cover invalid field modifiers on pack expansions,
      unsupported recursive pack-expanded storage, arity mismatches, and
      unsupported placements outside documented struct-field contexts.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once pack-expanded storage is represented and initialized
    deterministically; leave helper bodies and lifecycle hooks to TODO-4276.

- [ ] TODO-4276: Expand type packs in helpers and lifecycle hooks
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4275
  - scope: Extend pack expansion from stored fields into helper signatures,
    helper bodies, return envelopes, and generated copy/move/destroy lifecycle
    behavior.
  - implementation_notes:
    - Build on pack-expanded storage from TODO-4275 and existing lifecycle
      semantics for ordinary struct fields.
    - Cross-reference the reflection field metadata API and generated helper
      model in `docs/PrimeStruct.md`: `meta.field_count<T>()`,
      `meta.field_name<T>(i)`, `meta.field_type<T>(i)`,
      `meta.field_visibility<T>(i)`, plus generated `/Type/Default`,
      `/Type/Clone`, `/Type/Clear`, and lifecycle behavior. Pack-expanded
      lifecycle order should reuse that source-order field model rather than
      introducing tuple-specific `drop(item...)` semantics.
    - Keep lifecycle order deterministic and documented; tuple element
      lifecycle depends on it.
    - Add focused tests before broad tuple tests so generic pack expansion is
      validated independently of `tuple`.
  - acceptance:
    - Helper definitions can mention pack-expanded parameters, locals, and
      return envelopes in documented positions.
    - Pack-expanded fields participate in copy, move, and destruction according
      to ordinary struct field rules, including non-trivial element types.
    - Reflection metadata for reflect-enabled pack-expanded structs reports
      expanded fields in the same order that lifecycle hooks copy, move, clear,
      and destroy them.
    - Lifecycle failures in one element type produce diagnostics that identify
      the element index or expanded field.
    - Tests cover zero-element, one-element, and multi-element packs in helper
      and lifecycle contexts, including at least one reflect-enabled
      pack-expanded struct that exercises `meta.field_count<T>()` and generated
      helper behavior.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once generic pack expansion works outside stored fields and
    lifecycle behavior is covered; leave indexed access to TODO-4271.

- [ ] TODO-4271: Add compile-time pack indexing
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4276, TODO-4270
  - scope: Add generic compile-time pack indexing so helpers can resolve the
    `I`th type and storage slot of a heterogeneous type pack.
  - implementation_notes:
    - Build on pack expansion from TODO-4276 and integer template arguments
      from TODO-4270.
    - The feature must be generic enough for `get<I>`, `tuple_element<I, T>`,
      and future stdlib metaprogramming helpers.
    - Publish enough semantic-product facts that lowerers do not need to
      re-derive tuple field selection from source text.
  - acceptance:
    - A `.prime` helper can use an integer template argument to select the
      corresponding expanded pack field and return that field's precise type.
    - Borrowed and mutable borrowed pack-index access preserves
      `Reference<TI>` behavior when ordinary field borrowing would allow it.
    - Out-of-range indexes, non-pack operands, and non-integer indexes produce
      stable diagnostics at semantic validation time.
    - Semantic-product and IR-lowering tests verify that indexed pack lookup is
      represented without tuple-specific AST/source fallbacks.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once indexed pack lookup can power a generic `get<I>`-style
    helper; leave the public tuple stdlib surface to TODO-4272.

- [ ] TODO-4272: Add stdlib `tuple<Ts...>`
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4271
  - scope: Implement the public `tuple<Ts...>` stdlib type and minimal helper
    surface in `.prime` using the generic type-pack substrate.
  - implementation_notes:
    - Add files under `stdlib/std/tuple/` plus a public import surface aligned
      with existing stdlib naming/import conventions.
    - Do not add compiler/runtime tuple opcodes, special envelopes, generated
      fixed-arity tuple families, or fixed helper names such as `get0`.
    - Start with construction, `get<I>`, borrowed access where supported,
      tuple arity metadata, and focused diagnostics.
  - acceptance:
    - `tuple<Ts...>{...}` constructs owned heterogeneous tuples of arbitrary
      arity using `.prime` source and ordinary field lifecycle rules.
    - `get<I>(tuple)` and method sugar where supported return the precise
      element type for value and borrowed receivers.
    - `tuple<>`, `tuple<T>`, and multi-element tuples are covered without
      separate arity-specific implementations.
    - Diagnostics cover initializer arity mismatch, bad element type,
      out-of-range `get`, non-constant `get`, and unsupported element
      lifecycle operations.
    - Docs record `tuple` as stdlib-owned and explicitly reject fixed-arity
      tuple families as implementation strategy.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the minimal tuple API is implemented in `.prime` and
    validated; leave heterogeneous value-pack inference to TODO-4273.

- [ ] TODO-4273: Add heterogeneous value-pack inference
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4272
  - scope: Add generic heterogeneous value-pack inference for helpers such as
    `make_tuple(values...)` without weakening the homogeneous `args<T>` model.
  - implementation_notes:
    - Keep `args<T>` homogeneous. Add a separate heterogeneous value-pack
      inference path that binds one type per positional argument.
    - Start from implicit-template inference, spread-argument validation, and
      call binding; do not change vector/map variadic behavior unless covered
      by explicit compatibility tests.
    - Treat spread forwarding of heterogeneous packs as a separate documented
      feature; split it out if it grows beyond this task.
  - acceptance:
    - A `.prime` helper can infer `Ts...` from heterogeneous positional values
      and return `tuple<Ts...>`.
    - `make_tuple(1, "x", true)` or the accepted stdlib spelling constructs
      `tuple<i32, string, bool>` through ordinary tuple construction.
    - Existing homogeneous `args<T>` diagnostics and vector/map constructors
      remain unchanged.
    - Negative tests cover conflicting inference, empty packs if unsupported,
      named-argument interactions, and unsupported spread forwarding.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once heterogeneous value-pack inference is available for
    tuple helper construction and does not regress existing homogeneous packs.

- [ ] TODO-4274: Add tuple bracket indexing sugar
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4272
  - scope: Add `value[0]` style tuple element access as compile-time-indexed
    sugar over `get<0>(value)`, distinct from runtime collection indexing.
  - implementation_notes:
    - Start from `docs/TuplePrototype.md`, parser/indexing forms, semantic
      collection access validation, and tuple `get<I>` from TODO-4272.
    - Tuple indexing must use a compile-time integer literal or equivalent
      folded constant; it must not route through runtime `at(tuple, index)`.
    - Preserve existing runtime indexing behavior for arrays, vectors, maps,
      strings, and homogeneous `args<T>` packs.
  - acceptance:
    - `pair[0]` and `pair[1]` on `tuple<i32, string>` resolve to `i32` and
      `string` respectively.
    - Runtime index variables such as `pair[i]` reject with a diagnostic that
      explains tuple indexes must be compile-time constants.
    - Out-of-range tuple indexes reject with the same index provenance as
      `get<I>`.
    - Existing array/vector/map/string/indexed-pack tests keep their current
      runtime-index behavior.
    - `docs/TuplePrototype.md` records bracket indexing as accepted sugar.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once bracket indexing is validated as tuple-specific
    compile-time sugar and ordinary runtime indexing behavior is unchanged.

- [ ] TODO-4277: Add tuple destructuring sugar
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4274
  - scope: Add tuple destructuring binding sugar over ordinary `tuple<...>`
    values without involving task spawning or multi-wait.
  - implementation_notes:
    - Start from parser binding forms, semantic validation, and tuple indexed
      access from TODO-4272/TODO-4274.
    - Follow the bracket-list name binding rule in `docs/PrimeStruct.md`, as
      summarized by `docs/TuplePrototype.md`: known bracket entries keep their
      existing transform/type/modifier meaning, while fresh identifiers in a
      destructuring pattern introduce new bindings.
    - Destructuring should lower to `get<I>` calls, bracket-index facts, or
      equivalent semantic facts over ordinary tuple values; do not introduce a
      separate product type.
    - Keep mutable and borrowed destructuring narrow; split advanced patterns
      into follow-ups if needed.
  - acceptance:
    - tuple destructuring accepts a documented binding form such as
      `[left right] tupleValue` or the accepted final syntax and binds elements
      in tuple order.
    - Parser and semantic diagnostics distinguish destructuring patterns from
      existing transform/type bracket lists, including cases where one bracket
      entry is already known and another is fresh.
    - Destructuring rejects arity mismatch, non-tuple operands, duplicate
      binding names, and unsupported mutable/borrowed patterns with stable
      diagnostics.
    - Destructured element bindings have the precise element types and obey
      normal move/copy/borrow rules.
    - Docs and examples show destructuring as sugar over ordinary stdlib
      `tuple`, not as a separate product type.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once tuple destructuring works for ordinary tuple values;
    leave multi-wait integration to TODO-4278.

- [ ] TODO-4278: Integrate multi-wait with stdlib tuple
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4277
  - scope: Wire future multi-task `wait(left, right, ...)` to return ordinary
    `tuple<...>` values instead of a task-specific product type.
  - implementation_notes:
    - Start from `docs/MultithreadingPrototype.md`,
      `docs/TuplePrototype.md`, tuple destructuring from TODO-4277, and task
      handle facts once task support exists.
    - If task spawning/wait is not implemented yet, keep this TODO blocked or
      split the task-side prerequisite into the multithreading lane rather than
      adding placeholder runtime behavior.
  - acceptance:
    - Multi-task `wait(left, right, ...)` returns `tuple<Left, Right, ...>`
      using the stdlib tuple type when task support is present.
    - The result can be consumed through `get<I>`, bracket indexing, and tuple
      destructuring.
    - Multi-wait preserves the structured task lifetime rule from
      `docs/MultithreadingPrototype.md`.
    - `docs/MultithreadingPrototype.md` and `docs/TuplePrototype.md` describe
      the implemented integration and remove prototype-only caveats that are no
      longer true.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once multi-wait returns stdlib `tuple<...>` or the missing
    task-side prerequisite is split into an explicit multithreading TODO.
