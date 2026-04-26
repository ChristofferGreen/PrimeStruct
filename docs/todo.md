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

- TODO-4148
- TODO-4168
- TODO-4170
- TODO-4153
- TODO-4172

### Immediate Next 10 (After Ready Now)

- TODO-4150
- TODO-4155
- TODO-4157
- TODO-4158
- TODO-4162
- TODO-4165
- TODO-4159
- TODO-4160
- TODO-4161
- TODO-4163

### Priority Lanes (Current)

- Semantic-product authority and lowerer boundary enforcement: TODO-4148,
  TODO-4170, TODO-4150, TODO-4155, TODO-4157, TODO-4158,
  TODO-4159, TODO-4160, TODO-4161, TODO-4164
- Test API cleanup and contract-probe migration: TODO-4153, TODO-4172
- Semantics orchestration cleanup: TODO-4168
- Compile-pipeline boundary hardening and provenance parity: TODO-4162,
  TODO-4163, TODO-4165, TODO-4166
- User-authored AST transform hooks: TODO-4174, TODO-4173, TODO-4176,
  TODO-4175
- `soa_vector` promotion and de-experimentalization: TODO-4179, TODO-4177,
  TODO-4178, TODO-4180, TODO-4182, TODO-4181

### Execution Queue (Recommended)

1. TODO-4148
2. TODO-4168
3. TODO-4170
4. TODO-4150
5. TODO-4153
6. TODO-4172
7. TODO-4155
8. TODO-4157
9. TODO-4158
10. TODO-4162
11. TODO-4165
12. TODO-4159
13. TODO-4160
14. TODO-4161
15. TODO-4163
16. TODO-4164
17. TODO-4166
18. TODO-4174
19. TODO-4173
20. TODO-4176
21. TODO-4175
22. TODO-4179
23. TODO-4177
24. TODO-4178
25. TODO-4180
26. TODO-4182
27. TODO-4181

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | TODO-4148, TODO-4170, TODO-4150, TODO-4155, TODO-4157, TODO-4158, TODO-4164 |
| Compile-pipeline stage and publication-boundary contracts | TODO-4155, TODO-4162, TODO-4166 |
| Compile-time macro hooks and AST transform ownership | TODO-4174, TODO-4173, TODO-4176 |
| Stdlib surface-style alignment and public helper readability | none |
| Stdlib bridge consolidation and collection/file/gfx surface authority | none |
| Vector/map stdlib ownership cutover and collection surface authority | none |
| Stdlib de-experimentalization and public/internal namespace cleanup | TODO-4179, TODO-4180, TODO-4182 |
| SoA maturity and `soa_vector` promotion | TODO-4177, TODO-4178, TODO-4179, TODO-4180, TODO-4182 |
| Validator entrypoint and benchmark-plumbing split | TODO-4168 |
| Semantic-product publication by module and fact family | TODO-4170, TODO-4155 |
| Semantic-product public API factoring and versioning | TODO-4160, TODO-4161, TODO-4163 |
| IR lowerer compile-unit breakup | none |
| Backend validation/build ergonomics | none |
| Emitter/semantics map-helper parity | none |
| VM debug-session argv ownership | none |
| Debugger/source-map provenance parity | TODO-4165 |
| Debug trace replay robustness | none |
| VM/runtime debug stateful opcode parity | none |
| Test-suite audit follow-up and release-gate stability | none |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | TODO-4155, TODO-4161, TODO-4163 |
| AST transform hook conformance | TODO-4173, TODO-4176 |
| CodeExamples-aligned stdlib surface syntax conformance | TODO-4176 |
| Compile-pipeline stage handoff conformance | TODO-4155, TODO-4162, TODO-4166 |
| Semantic-product publication parity and deterministic ordering | TODO-4170, TODO-4155, TODO-4163 |
| Lowerer/source-composition contract coverage | TODO-4148, TODO-4150, TODO-4153, TODO-4157, TODO-4158, TODO-4159 |
| Vector/map bridge parity for imports, rewrites, and lowering | none |
| De-experimentalization surface and namespace parity | TODO-4178, TODO-4182 |
| `soa_vector` maturity and canonical surface parity | TODO-4177, TODO-4178, TODO-4180, TODO-4182 |
| Focused backend rerun ergonomics and suite partitioning | none |
| Architecture contract probe migration | TODO-4172 |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debugger/source-map provenance parity | TODO-4165 |
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

- [ ] TODO-4165: Add debugger and source-map provenance parity coverage
  - owner: ai
  - created_at: 2026-04-25
  - phase: Compile-pipeline boundary hardening
  - depends_on: TODO-4158, TODO-4155
  - scope: Add release coverage proving debugger-facing and source-map
    provenance stay stable when lowering consumes published semantic-product
    identities plus syntax-owned provenance.
  - acceptance:
    - The touched tests assert matching debugger or source-map provenance for a
      representative compile or runtime entrypoint.
    - Failures surface readable provenance diffs for the touched coverage.
    - The touched coverage remains deterministic and runnable from
      `build-release/`.
  - stop_rule: Stop once one representative debugger or source-map provenance
    path has parity coverage; do not broaden into runtime or debugger redesign.

- [ ] TODO-4164: Remove AST-side semantic re-derivation caches after boundary cutover
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantic-product authority
  - depends_on: TODO-4157, TODO-4158, TODO-4155
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
  - depends_on: TODO-4155
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

- [ ] TODO-4162: Split compile-pipeline benchmark knobs from the production pipeline entrypoint
  - owner: ai
  - created_at: 2026-04-25
  - phase: Compile-pipeline boundary hardening
  - scope: Move the touched benchmark collector and worker-count plumbing out
    of the main compile-pipeline entrypoint into a narrower config or helper
    surface so production stage handoff stays explicit.
  - acceptance:
    - The main compile-pipeline entrypoint no longer directly owns the touched
      benchmark-only knob set.
    - Benchmark collection still works through a dedicated config or helper
      path.
    - Non-benchmark compile-pipeline behavior remains unchanged for the
      touched entrypoints.
  - stop_rule: Stop once one production compile-pipeline entrypoint no longer
    co-owns the touched benchmark knobs; do not refactor all pipeline config
    in one slice.

- [ ] TODO-4161: Add semantic-product contract-version compatibility coverage
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantic-product authority
  - depends_on: TODO-4155, TODO-4160
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

- [ ] TODO-4160: Split `SemanticProduct.h` into family-specific public surfaces
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantic-product authority
  - depends_on: TODO-4155
  - scope: Break the monolithic `include/primec/SemanticProduct.h` public API
    into smaller family-specific headers while keeping a stable umbrella
    include for existing consumers where needed.
  - acceptance:
    - At least two semantic-product fact families move to dedicated public
      headers.
    - `include/primec/SemanticProduct.h` becomes a smaller facade or umbrella
      over the new family-specific surfaces.
    - The touched consumers build through the new header layout without
      including private semantics internals.
  - stop_rule: Stop once one meaningful slice of the semantic-product API is
    decomposed; do not attempt a repo-wide header taxonomy rewrite in one
    slice.

- [ ] TODO-4159: Move lowerer import-alias handling to a frontend-owned syntax helper
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantic-product authority
  - depends_on: TODO-4158
  - scope: Move the touched import-alias expansion or shorthand-resolution
    logic out of lowerer-specific setup into a frontend-owned syntax helper so
    lowering stops owning syntax convenience behavior.
  - acceptance:
    - The touched lowerer files no longer own the moved import-alias handling
      logic.
    - The replacement helper surface is owned outside lowerer-specific setup
      and outside private semantics internals.
    - Release coverage for the touched alias-driven lowering paths remains
      unchanged.
  - stop_rule: Stop once one canonical import-alias helper surface is
    introduced and production lowering uses it; do not broaden into unrelated
    import resolver redesign.

- [ ] TODO-4158: Introduce a narrow lowerer syntax and provenance view
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantic-product authority
  - depends_on: TODO-4147, TODO-4148
  - scope: Replace one touched lowerer seam that currently receives broad AST
    ownership (`Program`, wide `Definition*` reach-through, or equivalent)
    with a syntax-owned body and provenance view that exposes only the data
    lowering still needs.
  - acceptance:
    - One touched lowering path consumes the narrow syntax and provenance view
      instead of broad AST ownership.
    - The touched diagnostics and provenance behavior remain unchanged.
    - The new view is owned outside private semantics internals.
  - stop_rule: Stop once one production lowering seam uses the narrow syntax
    and provenance view; do not migrate every lowerer entrypoint in one slice.

- [ ] TODO-4157: Retire temporary semantic-product adapter code
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantic-product authority
  - depends_on: TODO-4147, TODO-4148, TODO-4150, TODO-4155
  - scope: Delete one touched compatibility adapter or shim that keeps
    lowerer or compile-pipeline code working before the semantic-product
    boundary becomes fully authoritative.
  - acceptance:
    - One real semantic-product compatibility adapter or shim disappears from
      the touched production path.
    - The touched lowerer or compile-pipeline flow still works through
      published semantic-product facts only.
    - No touched caller falls back to the removed adapter seam.
  - stop_rule: Stop once one real adapter seam is retired end-to-end; do not
    attempt a repo-wide compatibility cleanup in one slice.

- [ ] TODO-4172: Migrate graph-pilot and architecture source-lock suites to public inspection surfaces
  - owner: ai
  - created_at: 2026-04-25
  - phase: Test API cleanup
  - depends_on: TODO-4171
  - scope: Retire the remaining semantics source-lock suites that read private
    `src/semantics/` fragments for graph-pilot or architecture checks,
    replacing them with semantic-product, type-graph, or dedicated public
    testing-helper assertions.
  - acceptance:
    - At least one graph-pilot or architecture source-lock suite no longer
      reads private `src/semantics/` fragments directly.
    - The migrated coverage still catches the touched architecture/delegation
      regressions through public inspection surfaces.
    - The touched tests remain deterministic and runnable from
      `build-release/`.
  - stop_rule: Stop once the touched graph-pilot or architecture suite
    preserves behavioral signal without pinning private fragment placement; do
    not broaden into unrelated semantics coverage reshaping.

- [ ] TODO-4170: Publish `soa_vector` specialization facts needed by the lowerer
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantic-product authority
  - depends_on: TODO-4169
  - scope: Add explicit semantic-product facts for `soa_vector` collection
    specialization and canonical helper-family routing that lowerer currently
    infers from type text and private semantics helpers.
  - acceptance:
    - The semantic product publishes the touched `soa_vector`
      specialization and helper-routing facts in deterministic order.
    - Touched lowerer collection setup helpers can consume the published
      `soa_vector` facts without private semantics specialization predicates.
    - Focused release coverage for `soa_vector` helper lowering passes through
      the new published surface.
  - stop_rule: Stop once the lowerer can classify the touched `soa_vector`
    flows from published facts; do not fold in broader vector/map ownership or
    runtime redesign work.

- [ ] TODO-4168: Extract semantic-product publication orchestration from `Semantics::validate(...)`
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantics orchestration cleanup
  - depends_on: TODO-4167
  - scope: Refactor `src/semantics/SemanticsValidate.cpp` so semantic-product
    publication lives in a separate compileable orchestration unit instead of
    sharing one oversized production validation flow.
  - acceptance:
    - `Semantics::validate(...)` delegates semantic-product publication
      orchestration to a dedicated unit.
    - Release validation and semantic-product publication behavior remain
      unchanged for the touched coverage.
    - The touched publication path keeps deterministic diagnostic and
      publication ordering.
  - stop_rule: Stop once semantic-product publication no longer shares one
    monolithic orchestration surface with the main validation path; do not
    broaden into benchmark plumbing or new semantic feature work.

- [ ] TODO-4156: Track semantics validation orchestration split
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantics orchestration cleanup
  - depends_on: TODO-4167, TODO-4168
  - scope: Track the split of benchmark-only and semantic-product publication
    orchestration out of the oversized `Semantics::validate(...)` production
    flow.
  - acceptance:
    - TODO-4167 and TODO-4168 land with unchanged release behavior for the
      touched validation and publication coverage.
    - `Semantics::validate(...)` no longer co-owns benchmark-only or
      semantic-product publication plumbing in one monolithic orchestration
      file.
  - stop_rule: Keep this item as a coordination tracker only; implement work
    through child leaves and do not reopen the old broad slice directly.

- [ ] TODO-4155: Add semantic-product-authority conformance coverage across entrypoints
  - owner: ai
  - created_at: 2026-04-25
  - phase: Boundary enforcement
  - depends_on: TODO-4147, TODO-4148, TODO-4150
  - scope: Add release coverage proving `primec` and `primevm` consume
    published semantic-product facts consistently across consuming entrypoints
    and reject missing lowerer-facing facts deterministically.
  - acceptance:
    - Tests exercise `primec` consuming backends and `primevm` through the same
      published semantic-product contract.
    - Missing or invalid lowerer-facing semantic facts fail with deterministic
      diagnostics for the touched entrypoints.
    - Compile-pipeline handoff coverage proves stage-dependent semantic-product
      consumption stays explicit.
  - stop_rule: Stop once the semantic-product boundary is covered as a contract
    at the compile/runtime entrypoints; do not broaden into unrelated backend
    feature additions.

- [ ] TODO-4154: Track semantics source-lock migration to public inspection surfaces
  - owner: ai
  - created_at: 2026-04-25
  - phase: Test API cleanup
  - depends_on: TODO-4171, TODO-4172
  - scope: Track retirement of semantics source-lock suites that read private
    `src/semantics/` fragments directly in favor of public inspection surfaces.
  - acceptance:
    - TODO-4171 and TODO-4172 land with deterministic coverage still runnable
      from `build-release/`.
    - The touched semantics source-lock suites preserve delegation and
      architecture signal through public inspection surfaces.
  - stop_rule: Keep this item as a coordination tracker only; implement work
    through child leaves rather than reopening the broad migration slice.

- [ ] TODO-4153: Migrate emitter-expr lowerer source-lock tests to contract-level assertions
  - owner: ai
  - created_at: 2026-04-25
  - phase: Test API cleanup
  - depends_on: TODO-4152
  - scope: Replace the emitter-expr lowerer source-lock suite that asserts
    exact file names, includes, or source strings with tests over
    semantic-product dumps, IR dumps, and backend-observable behavior.
  - acceptance:
    - The emitter-expr lowerer source-lock suite is rewritten to assert
      contract output instead of `readText(...)` against `src/`.
    - The migrated coverage still catches emitter-expr helper-routing or
      lowering behavior regressions.
    - The touched tests no longer break on file moves that preserve behavior.
  - stop_rule: Stop once the emitter-expr source-lock suite is retired in
    favor of contract assertions; do not attempt a repo-wide lowerer lock
    purge in one slice.

- [ ] TODO-4150: Remove remaining lowerer binding-type reach-through into semantics helpers
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantic-product authority
  - depends_on: TODO-4169, TODO-4170
  - scope: Refactor lowerer binding and uninitialized helpers so binding-kind
    and specialization decisions come from published semantic facts or neutral
    shared support utilities rather than private semantics helper headers.
  - acceptance:
    - The touched lowerer binding/setup helpers no longer include
      `src/semantics/SemanticsHelpers.h`.
    - Focused release coverage for binding setup, uninitialized flows, and
      struct-layout-sensitive lowering passes unchanged behavior through the
      published boundary.
    - Any replacement utility surface is owned outside private semantics.
  - stop_rule: Stop once the touched lowerer binding/type helpers no longer
    depend on semantics-private helper headers; do not broaden into unrelated
    runtime or backend redesign.

- [ ] TODO-4149: Track collection specialization fact publication for the lowerer
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantic-product authority
  - depends_on: TODO-4169, TODO-4170
  - scope: Track publication of collection specialization facts that lowerer
    currently infers from type text and private semantics helpers.
  - acceptance:
    - TODO-4169 and TODO-4170 land with deterministic semantic-product
      publication for the touched collection fact families.
    - The touched lowerer collection setup helpers can consume the published
      vector/map and `soa_vector` facts without private semantics
      specialization predicates.
  - stop_rule: Keep this item as a coordination tracker only; implement work
    through child leaves rather than reopening the old broad slice.

- [ ] TODO-4148: Cut method-target resolution over to published semantic-product facts
  - owner: ai
  - created_at: 2026-04-25
  - phase: Semantic-product authority
  - depends_on: TODO-4147
  - scope: Remove lowerer-side method target rediscovery from
    `IrLowererSetupTypeMethodCallResolution.cpp` and related helpers when
    semantics already published the receiver and method target choice.
  - acceptance:
    - The touched method-target resolution helpers no longer include private
      semantics helper headers.
    - Focused release coverage for method-call helper routing and shadow
      precedence passes through semantic-product-owned target lookups.
    - User-facing behavior and diagnostics remain unchanged for the touched
      method-call cases.
  - stop_rule: Stop once method-target selection in lowering is driven by
    published call-target facts rather than helper-path reconstruction.
