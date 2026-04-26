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

- TODO-4161
- TODO-4163
- TODO-4164
- TODO-4166
- TODO-4174

### Immediate Next 10 (After Ready Now)

- TODO-4173
- TODO-4176

### Priority Lanes (Current)

- Semantic-product authority and lowerer boundary enforcement: TODO-4161,
  TODO-4164
- Compile-pipeline boundary hardening and provenance parity: TODO-4163,
  TODO-4166
- User-authored AST transform hooks: TODO-4174, TODO-4173, TODO-4176,
  TODO-4175
- `soa_vector` promotion and de-experimentalization: TODO-4179, TODO-4177,
  TODO-4178, TODO-4180, TODO-4182, TODO-4181

### Execution Queue (Recommended)

1. TODO-4161
2. TODO-4163
3. TODO-4164
4. TODO-4166
5. TODO-4174
6. TODO-4173
7. TODO-4176
8. TODO-4175
9. TODO-4179
10. TODO-4177
11. TODO-4178
12. TODO-4180
13. TODO-4182
14. TODO-4181

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | TODO-4164 |
| Compile-pipeline stage and publication-boundary contracts | TODO-4166 |
| Compile-time macro hooks and AST transform ownership | TODO-4174, TODO-4173, TODO-4176 |
| Stdlib surface-style alignment and public helper readability | none |
| Stdlib bridge consolidation and collection/file/gfx surface authority | none |
| Vector/map stdlib ownership cutover and collection surface authority | none |
| Stdlib de-experimentalization and public/internal namespace cleanup | TODO-4179, TODO-4180, TODO-4182 |
| SoA maturity and `soa_vector` promotion | TODO-4177, TODO-4178, TODO-4179, TODO-4180, TODO-4182 |
| Validator entrypoint and benchmark-plumbing split | none |
| Semantic-product publication by module and fact family | none |
| Semantic-product public API factoring and versioning | TODO-4161, TODO-4163 |
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
| Semantic-product-authority conformance | TODO-4161, TODO-4163 |
| AST transform hook conformance | TODO-4173, TODO-4176 |
| CodeExamples-aligned stdlib surface syntax conformance | TODO-4176 |
| Compile-pipeline stage handoff conformance | TODO-4166 |
| Semantic-product publication parity and deterministic ordering | TODO-4163 |
| Lowerer/source-composition contract coverage | none |
| Vector/map bridge parity for imports, rewrites, and lowering | none |
| De-experimentalization surface and namespace parity | TODO-4178, TODO-4182 |
| `soa_vector` maturity and canonical surface parity | TODO-4177, TODO-4178, TODO-4180, TODO-4182 |
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
- Active promotion lane should keep the canonical public wrapper authoritative
  first, then migrate conversions/examples/tests, and only then reclassify the
  experimental namespaces as compatibility-only or removable seams.

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
  - depends_on: TODO-4178, TODO-4180
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
  - depends_on: TODO-4179, TODO-4177, TODO-4178, TODO-4180, TODO-4182
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

- [ ] TODO-4180: Move canonical `soa_vector` conversions off experimental receiver types
  - owner: ai
  - created_at: 2026-04-25
  - phase: `soa_vector` promotion
  - depends_on: TODO-4179
  - scope: Move `to_aos` and related conversion helpers onto canonical
    `/std/collections/soa_vector/*`-owned receiver and helper contracts so the
    public conversion surface no longer names
    `/std/collections/experimental_soa_vector/SoaVector<T>` directly.
  - acceptance:
    - The touched conversion `.prime` surface accepts canonical `soa_vector`
      receiver types and helper paths without requiring experimental receiver
      names in ordinary public code.
    - The touched docs/spec explain conversion usage in canonical namespace
      terms.
    - Remaining experimental conversion seams, if any, are limited to
      compatibility forwarding instead of defining the public receiver contract.
  - stop_rule: Stop once the canonical conversion receiver contract is
    authoritative for the touched flows; do not delete every experimental shim
    in one slice.

- [ ] TODO-4179: Make `/std/collections/soa_vector/*` the full public helper authority
  - owner: ai
  - created_at: 2026-04-25
  - phase: `soa_vector` promotion
  - depends_on: TODO-4170
  - scope: Expand the canonical `soa_vector` wrapper layer so ordinary public
    count/get/ref/push/reserve/conversion flows route through
    `/std/collections/soa_vector/*` and
    `/std/collections/soa_vector_conversions/*` instead of exposing the
    experimental implementation modules as peer public APIs.
  - acceptance:
    - The touched canonical wrapper and conversion modules own the public helper
      names needed for ordinary `soa_vector` use without requiring direct
      experimental imports.
    - The touched docs classify `experimental_soa_vector` and
      `experimental_soa_vector_conversions` as internal or compatibility seams
      rather than ordinary public APIs.
    - The touched public wrapper surface stays aligned with the documented
      canonical `soa_vector` helper contract.
  - stop_rule: Stop after the canonical wrapper surface is authoritative for
    the touched helper families; do not complete full promotion, borrow
    semantics, and example migration in one slice.

- [ ] TODO-4178: Add canonical `soa_vector` backend and runtime parity coverage
  - owner: ai
  - created_at: 2026-04-25
  - phase: `soa_vector` promotion
  - depends_on: TODO-4179, TODO-4177
  - scope: Add focused release coverage proving the canonical
    `/std/collections/soa_vector/*` helper path reaches the same C++/VM/native
    semantics and diagnostics for the touched public flows, without depending on
    experimental-only detours in ordinary public tests.
  - acceptance:
    - The touched release coverage exercises canonical `soa_vector` helper and
      conversion imports across the supported backends or entrypoints.
    - Failures surface canonical-path diagnostics rather than experimental-only
      path leaks for the touched cases.
    - Direct experimental imports remain only in explicit compatibility tests
      when the canonical path is not the subject under test.
  - stop_rule: Stop after one representative canonical parity slice is covered
    end-to-end; do not migrate every historical compatibility test in one pass.

- [ ] TODO-4177: Stabilize the borrowed-view and field-view contract for public `soa_vector`
  - owner: ai
  - created_at: 2026-04-25
  - phase: `soa_vector` promotion
  - depends_on: TODO-4170
  - scope: Finish the next borrowed-view / field-view maturity slice so the
    public `soa_vector` contract can rely on stable `ref(...)` and
    `field_view`-style behavior, deterministic invalidation diagnostics, and
    public wrapper semantics rather than pending or experimental-only gaps.
  - acceptance:
    - The touched `soa_vector` borrowed-view or field-view flows have one
      stable documented contract on the canonical wrapper path.
    - The touched diagnostics and validation coverage prove the invalidation /
      lifetime rule for those borrowed flows deterministically.
    - The touched docs/spec remove at least one current “pending” or
      draft-only caveat from the promoted borrowed/field-view surface.
  - stop_rule: Stop after one coherent borrowed-view maturity slice is stable
    on the canonical path; do not attempt every future SoA borrow feature at
    once.

- [ ] TODO-4176: Add a checked-in `.prime` example for user-authored AST transforms
  - owner: ai
  - created_at: 2026-04-25
  - phase: Compile-time macro hooks
  - depends_on: TODO-4173
  - scope: Add one checked-in `.prime` AST-transform module plus one consumer
    `.prime` source that imports it, attaches `[trace_calls, ...]`, and serves
    as the canonical example surface for docs and release validation.
  - acceptance:
    - The repo contains one `.prime` file that declares a user-authored AST
      transform with the approved declaration contract.
    - The repo contains one `.prime` file that imports that transform and uses
      it through the bare `[trace_calls, ...]` attachment syntax on a
      definition.
    - The touched docs or release validation reference that checked-in example
      instead of only inline pseudo-snippets.
  - stop_rule: Stop after one end-to-end example module pair is checked in and
    wired into docs or validation; do not broaden this slice into multiple
    derives, text transforms, or stdlib packaging work.

- [ ] TODO-4175: Track user-authored AST transform hooks
  - owner: human
  - created_at: 2026-04-25
  - phase: Compile-time macro hooks
  - depends_on: TODO-4174, TODO-4173, TODO-4176
  - scope: Track the first user-authored AST transform lane so definitions can
    attach visible transform symbols like `[trace_calls, effects(...), int]`,
    resolve them through normal imports, and keep the docs/spec/examples aligned
    with the chosen `[ast]` marker or signature-inference contract.
  - acceptance:
    - TODO-4174, TODO-4173, and TODO-4176 land with one documented
      AST-transform surface that resolves normal local or imported symbols from
      the transform list.
    - `docs/PrimeStruct.md`, `docs/PrimeStruct_SyntaxSpec.md`, and
      `docs/CodeExamples.md` describe the same attachment syntax and declaration
      contract for user-authored AST transforms.
    - The tracked lane stays limited to AST transforms on definitions; text
      rewriting and broader CT-eval expansion remain out of scope.
  - stop_rule: Stop after the first definition-attached AST transform lane is
    documented and validated; do not fold text transforms, struct/type derives,
    or general CT-eval work into this tracker.

- [ ] TODO-4174: Resolve user-authored AST transform symbols from normal imports
  - owner: ai
  - created_at: 2026-04-25
  - phase: Compile-time macro hooks
  - depends_on: TODO-4168
  - scope: Teach transform-list validation to accept visible symbols marked
    `[ast]` or carrying the approved AST-transform signature, attach them to
    definition nodes, reject ambiguous or non-transform symbols
    deterministically, and document the chosen declaration rule.
  - acceptance:
    - A definition-attached transform like `[trace_calls, effects(...), int]`
      can resolve to a visible local or imported AST-transform symbol under one
      documented declaration contract.
    - Non-visible, wrong-signature, or phase-mismatched symbols fail with
      deterministic diagnostics.
    - The touched docs/spec show the approved declaration rule without relying
      on built-in-only transform names for the attached definition surface.
  - stop_rule: Stop once definition-level AST-transform symbol resolution works
    for one input/output contract; do not execute transforms yet or add text
    hooks in this slice.

- [ ] TODO-4173: Execute user-authored AST transform hooks in the semantic pipeline
  - owner: ai
  - created_at: 2026-04-25
  - phase: Compile-time macro hooks
  - depends_on: TODO-4174
  - scope: Run resolved AST-transform symbols through the compile-time VM /
    CT-eval path on a narrow `FunctionAst`-style API, replace the touched
    definition AST before the downstream semantic/lowering path, and add
    release conformance coverage for success and failure cases.
  - acceptance:
    - One local or imported AST transform rewrites a definition deterministically
      before the downstream semantic and lowering pipeline runs.
    - The touched compile-pipeline coverage proves stable transformed output or
      deterministic diagnostics when the transform fails or returns the wrong
      shape.
    - Release tests cover both a successful rewrite and at least one
      wrong-result or transform-failure case.
  - stop_rule: Stop after one definition-to-definition AST transform contract
    works end-to-end; do not add text transforms, execution hooks, or broad
    mutable AST editing APIs in this slice.

- [ ] TODO-4166: Refactor compile-pipeline results into explicit success and failure variants
  - owner: ai
  - created_at: 2026-04-25
  - phase: Compile-pipeline boundary hardening
  - depends_on: TODO-4162
  - scope: Replace the touched flag-heavy compile-pipeline result surface with
    explicit success and failure variants so callers cannot observe invalid
    state combinations around diagnostics, semantic-product handoff, or IR
    preparation.
  - acceptance:
    - The touched compile-pipeline callers consume explicit success or failure
      variants instead of checking multiple flags or optional fields.
    - Stage diagnostics and semantic-product handoff behavior remain unchanged
      for the touched flows.
    - Invalid mixed result states disappear from the touched pipeline surface.
  - stop_rule: Stop once one compile-pipeline result surface uses explicit
    variants end-to-end; do not rewrite every caller in one slice.

- [ ] TODO-4164: Remove AST-side semantic re-derivation caches after boundary cutover
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantic-product authority
  - scope: Delete one touched AST-side semantic fallback or cache that
    re-derives facts already available through the published semantic-product
    boundary.
  - acceptance:
    - One real AST-side semantic re-derivation seam is removed from the
      touched production path.
    - The touched lowerer or compile-pipeline flow reads published
      semantic-product data instead of recomputing equivalent facts.
    - Release diagnostics and lowering behavior remain unchanged for the
      touched coverage.
  - stop_rule: Stop once one real re-derivation seam is deleted end-to-end; do
    not attempt a repo-wide cache purge in one slice.

- [ ] TODO-4163: Add worker-count parity golden coverage for the full semantic-product dump
  - owner: ai
  - created_at: 2026-04-25
  - phase: Compile-pipeline boundary hardening
  - scope: Add release coverage comparing the formatted full semantic-product
    dump across multiple validation worker counts so deterministic publication
    regressions fail with readable diffs.
  - acceptance:
    - The touched tests assert identical semantic-product dump output for the
      selected modules across worker counts `1`, `2`, and `4`.
    - Parity failures surface readable dump diffs for the touched coverage.
    - The touched coverage remains deterministic and runnable from
      `build-release/`.
  - stop_rule: Stop once one representative full semantic-product dump path
    has worker-count parity coverage; do not build a massive worker-count
    matrix in one slice.

- [ ] TODO-4161: Add semantic-product contract-version compatibility coverage
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantic-product authority
  - depends_on: TODO-4160
  - scope: Turn the touched semantic-product contract version into an enforced
    compatibility surface with coverage that fails clearly when the published
    shape changes without the required versioning update.
  - acceptance:
    - The touched coverage fails clearly when the semantic-product contract
      shape changes without the corresponding compatibility update.
    - Tests assert the touched contract version or compatibility marker for the
      selected public surface.
    - Version mismatch diagnostics or failures stay deterministic for the
      touched coverage.
  - stop_rule: Stop once one canonical semantic-product surface enforces
    contract-version compatibility; do not broaden into general serialization
    migration policy.
