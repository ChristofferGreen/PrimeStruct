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
21. Keep active work leaf-shaped: `Ready Now`, `Immediate Next`, and the
    `Execution Queue` must not contain umbrella, tracker, phase, or
    "continue with another slice" items. If a TODO accumulates completed-slice
    history while staying open, close the satisfied parent and create bounded
    follow-up leaves for any remaining worthwhile work.
22. Keep parallel work explicit. `Ready Now` may contain multiple unrelated
    leaves, but each entry must name a parallel track and a primary surface so
    `$implement-todo-lite-parallel` can choose independent workers without
    re-reading the whole file.
23. Do not put two same-track successors in `Ready Now` at the same time unless
    their task blocks prove they touch different source/test surfaces. Serial
    successors belong in `Immediate Next 10` until their dependencies land.
24. Before launching a parallel run, the parent should choose at most one ready
    leaf per track, skip blocked/coupled tracks explicitly, and preserve
    worker-side TODO edits as provisional until root reconciliation.

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
  - parallel_track: short-track-name (required when listed in Ready Now)
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

### Ready Now (Parallel-Candidate Leaves; No Unmet TODO Dependencies)

- TODO-4271: Add compile-time pack indexing | track: tuple-type-packs |
  primary surface: generic pack-index selection and diagnostics

### Parallel Work Tracks (Current)

- `soa-zero-audit`: TODO-4529 replaced the residue inventory with a strict
  zero-production-trace audit; no SoA zero-audit leaf is ready.
- `map-zero-audit`: TODO-4532 reduced the lowerer native-dispatch slice;
  TODO-4464 remains the parent for future bounded trace-reduction splits
  and the final strict zero map-surface audit.
- `tuple-type-packs`: TODO-4276 completed helper/lifecycle pack
  expansion; ready TODO-4271, then serial successors TODO-4272
  -> TODO-4274 -> TODO-4273 -> TODO-4277 -> TODO-4278.
- `procedural-genericity`: blocked by the tuple-type-packs successor chain
  before TODO-4331 can start.
- `generic-requirements`: blocked by TODO-4331 and TODO-4334 before
  TODO-4341 can start.

### Immediate Next 10 (Track Successors; Not Ready Until Dependencies Land)

- TODO-4272: Add stdlib `tuple<Ts...>`
- TODO-4274: Add tuple bracket indexing sugar
- TODO-4273: Add heterogeneous value-pack inference
- TODO-4277: Add tuple destructuring sugar

### Priority Lanes (Current)

- Semantic ownership authority: none active; future semantic-authority work
  must enter as bounded leaves only.
- Deferred stdlib ADT migration: none active
- Vector stdlib ownership cutover: none active
- Map stdlib ownership cutover: TODO-4464 remains in progress for future
  bounded trace-reduction splits and the final strict zero audit
- SoA public surface rename and ownership cutover: TODO-4306 parent split;
  TODO-4526 removed semantic-validation inventory residue after TODO-4530
  reduced the shared semantic builtin path helper boundary; TODO-4527 removed
  template-monomorph residue; TODO-4533 removed lowerer call-resolution
  residual bridge traces; TODO-4528 removed lowerer count/access residue; and
  TODO-4529 replaced the residue inventory with a strict zero audit
- Deferred generic tuple substrate: ready TODO-4271 after TODO-4276
  completed helper/lifecycle pack expansion, followed by TODO-4272
  -> TODO-4274 -> TODO-4273 -> TODO-4277 -> TODO-4278
- Procedural compile-time genericity: TODO-4331 -> TODO-4332
  -> TODO-4333 -> TODO-4334 -> TODO-4335 -> TODO-4336 -> TODO-4337
  -> TODO-4338 -> TODO-4339 -> TODO-4340
- Generic constraint and compile-time flow alignment: TODO-4341
  -> TODO-4342 -> TODO-4343 -> TODO-4344 -> TODO-4352 -> TODO-4353
  -> TODO-4354 -> TODO-4355 -> TODO-4356 -> TODO-4357 -> TODO-4345
  -> TODO-4346 -> TODO-4358 -> TODO-4347 -> TODO-4351 -> TODO-4348
  -> TODO-4359 -> TODO-4349 -> TODO-4350

### Execution Queue (Recommended Track Order)

- TODO-4271: Add compile-time pack indexing
- TODO-4272: Add stdlib `tuple<Ts...>`
- TODO-4274: Add tuple bracket indexing sugar
- TODO-4273: Add heterogeneous value-pack inference
- TODO-4277: Add tuple destructuring sugar
- TODO-4278: Integrate multi-wait with stdlib tuple
- TODO-4331: Implement compile-time argument channel model
- TODO-4332: Add bare zero-arg execution syntax
- TODO-4333: Reject ambiguous value/execution names
- TODO-4334: Add compile-time `[type]` local bindings
- TODO-4335: Add `typeof<symbol>` compile-time primitive
- TODO-4336: Allow type locals in envelope positions
- TODO-4337: Add local generated nominal structs
- TODO-4338: Stabilize generated type identity and mangling
- TODO-4339: Lower procedural generic facts through semantics
- TODO-4340: Add procedural generic examples and conformance
- TODO-4341: Define generic requirement predicate surface
- TODO-4342: Represent requirement predicates as semantic facts
- TODO-4343: Add builtin type relation predicates
- TODO-4344: Add capability and trait support predicates
- TODO-4352: Add compile-time VM facade and host
- TODO-4353: Add typed compile-time value model
- TODO-4354: Factor reusable VM interpreter kernel
- TODO-4355: Add compile-time host and meta intrinsics
- TODO-4356: Add restricted compile-time callable lowering
- TODO-4357: Evaluate pure user predicates at compile time
- TODO-4345: Add compile-time `if` over type facts
- TODO-4346: Add compile-time flow effect and termination policy
- TODO-4358: Enforce compile-time cache, budget, and effects
- TODO-4347: Integrate requirements with overload selection
- TODO-4351: Add value-level compile-time requirement facts
- TODO-4348: Publish requirement diagnostics with provenance
- TODO-4359: Add compile-time VM conformance coverage
- TODO-4349: Add generic constraint conformance matrix
- TODO-4350: Add high-level generic design examples

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | none |
| Compile-pipeline stage and publication-boundary contracts | none |
| Compile-time macro hooks and AST transform ownership | none |
| Stdlib surface-style alignment and public helper readability | TODO-4305 |
| Stdlib bridge consolidation and collection/file/gfx surface authority | TODO-4430, TODO-4464, TODO-4524 |
| Vector/map stdlib ownership cutover and collection surface authority | TODO-4430, TODO-4464 |
| Stdlib de-experimentalization and public/internal namespace cleanup | TODO-4430, TODO-4464, TODO-4305, TODO-4524 |
| SoA maturity and `soa` public-surface rename | TODO-4305, TODO-4306, TODO-4514, TODO-4524 |
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
| Algebraic sum types and brace-only construction | none |
| Stdlib ADT migration for `Maybe` and `Result` | none |
| Generic type packs and tuple stdlib surface | TODO-4271, TODO-4272, TODO-4274, TODO-4273, TODO-4277, TODO-4278 |
| Procedural compile-time genericity and local type facts | TODO-4331, TODO-4332, TODO-4333, TODO-4334, TODO-4335, TODO-4336, TODO-4337, TODO-4338, TODO-4339, TODO-4340 |
| Generic constraints and compile-time flow control | TODO-4341, TODO-4342, TODO-4343, TODO-4344, TODO-4352, TODO-4353, TODO-4354, TODO-4355, TODO-4356, TODO-4357, TODO-4345, TODO-4346, TODO-4358, TODO-4347, TODO-4351, TODO-4348, TODO-4359, TODO-4349, TODO-4350 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | none |
| AST transform hook conformance | none |
| CodeExamples-aligned stdlib surface syntax conformance | TODO-4305 |
| Compile-pipeline stage handoff conformance | none |
| Semantic-product publication parity and deterministic ordering | none |
| Lowerer/source-composition contract coverage | none |
| Vector/map bridge parity for imports, rewrites, and lowering | TODO-4430, TODO-4464 |
| De-experimentalization surface and namespace parity | TODO-4430, TODO-4464, TODO-4305, TODO-4524 |
| `soa` maturity and canonical surface parity | TODO-4305, TODO-4306, TODO-4514, TODO-4524 |
| Focused backend rerun ergonomics and suite partitioning | none |
| Architecture contract probe migration | none |
| Emitter map-helper canonicalization parity | TODO-4464 |
| VM debug-session argv lifetime coverage | none |
| Debugger/source-map provenance parity | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug stateful opcode behavior | none |
| Release benchmark/example suite stability and doctest governance | none |
| Sum-type and brace-construction conformance | none |
| Maybe/Result sum migration conformance | none |
| Generic type-pack and tuple conformance | TODO-4271, TODO-4272, TODO-4274, TODO-4273, TODO-4277, TODO-4278 |
| Procedural compile-time genericity conformance | TODO-4331, TODO-4332, TODO-4333, TODO-4334, TODO-4335, TODO-4336, TODO-4337, TODO-4338, TODO-4339, TODO-4340 |
| Generic constraint and compile-time flow conformance | TODO-4341, TODO-4342, TODO-4343, TODO-4344, TODO-4352, TODO-4353, TODO-4354, TODO-4355, TODO-4356, TODO-4357, TODO-4345, TODO-4346, TODO-4358, TODO-4347, TODO-4351, TODO-4348, TODO-4359, TODO-4349, TODO-4350 |

### Vector/Map Bridge Contract Summary

- Bridge-owned public contract: exact and wildcard `/std/collections` imports,
  `vector<T>` / `map<K, V>` constructor and literal-rewrite surfaces, helper
  families, compatibility spellings plus removed-helper diagnostics, semantic
  surface IDs, and lowerer dispatch metadata.
- Migration-only seams: rooted `/map/*` spellings,
  `mapCount`-style internal lowering names, and
  `/std/collections/experimental_*` implementation modules stay temporary.
  Rooted `/vector/*` helper spellings are no longer builtin vector
  compatibility aliases; explicit user definitions under those paths remain
  ordinary definitions.
  The vector/map adapter cutover is complete for semantic,
  template-monomorph, and lowerer helper path-candidate decisions. Canonical
  read/access helper routing is finished in `docs/todo_finished.md`; vector
  header materialization now uses
  layout facts instead of hard-coded record slots, and canonical vector surface
  metadata is now owned by `stdlib/std/collections/surfaces.psmeta`, and the
  registry no longer advertises vector compatibility spellings through that
  manifest. Direct experimental vector source imports are now rejected, and
  the vector production C++ zero-trace audit is now mechanically enforced.
  TODO-4464 continues the same ownership model for map after the internal-map
  lookup/insertion substrate moved into `.prime` helper code, canonical
  read/access/insert wrappers stopped routing through builtin/native map fast
  paths, canonical map surface metadata moved into
  `stdlib/std/collections/surfaces.psmeta`, map compatibility spellings were
  removed from that manifest, slashless experimental-map helper path
  normalization was deleted, experimental-map access alias probes were removed
  from lowerer/emitter builtin-access classification, experimental-map entry
  constructor aliases were deleted from lowerer/emitter builtin-map
  constructor classification, experimental-map constructor direct targets were
  deleted from setup-type method-call resolution, and experimental-map method
  fallback guards were removed from lowerer dispatch, and frontend
  import-alias normalization no longer roots slashless map helper paths. The
  emitter collection fallback path no longer carries a dead slashless map path
  normalizer, and semantic snapshot receiver-query collection no longer treats
  rooted `/map/*` direct calls as receiver candidates, and semantic call
  validation no longer strips rooted `/map/*` specialization or overload
  suffixes to recover removed compatibility definition families. The remaining
  rooted `/map/*` template definitions are no longer treated as stdlib
  collection helpers by implicit template inference, and template
  monomorphization no longer strips slashless `map/` method prefixes from map
  receiver method targets, and emitter builtin collection inference no longer
  recognizes slashless `map/count` as an explicit map-count helper. The
  semantic late unknown-target map method fallback now resolves canonical
  helper targets through stdlib surface metadata instead of concatenating a
  production C++ canonical map helper path, and implicit template inference
  now classifies canonical map helper definitions through stdlib surface
  metadata instead of hard-coding the canonical map helper prefix, and
  statement body-argument map helper target routing now uses stdlib surface
  metadata instead of hard-coded map helper path prefixes and concatenations,
  and statement container drop/relocation triviality now recognizes
  experimental map backing structs through the shared collection backing-type
  helper instead of a production C++ map path literal, and semantic
  direct-call return binding inference now recognizes experimental map backing
  specializations through the same helper instead of a production C++ map path
  literal, and setup-type map struct classification, Result map identity, and
  statement-return collection normalization now derive canonical `MapValue`
  roots through collection path helpers instead of split-string map roots, and
  lowerer fallback setup plus struct-layout generated map classifiers now
  derive experimental map backing paths through shared collection helpers
  instead of direct backing path roots, and uninitialized-struct specialized
  map detection now derives generated experimental map backing recognition
  through the same helper, and initializer inference now derives generated
  experimental map backing recognition through
  `isExperimentalCollectionBackingTypeName`, including graph binding
  initializer inference, and statement return collection normalization now
  uses the same helper for generated map backing recognition. The remaining
  production lowerer/emitter
  experimental-map traces
  are source-locked as temporary internal backing substrate by
  `test_stdlib_map_ownership.cpp`, and
  all production `src`/`include` experimental-map/`Map__*` backing traces are
  capped by the decaying `scripts/check_map_backing_traces.py` release gate.
  The broader `scripts/check_map_surface_trace_inventory.py` release gate now
  caps all current production map-surface trace classes until TODO-4464 deletes
  or migrates them to reach the strict zero-trace state.
- Compatibility adapter inventory: map insert helper compatibility no longer
  lives in `StdlibSurfaceRegistry::CollectionsMapHelpers`; that metadata now
  recognizes only canonical `/std/collections/map/*` helper spellings.
  Direct experimental map source imports are now rejected, public map wrapper
  bridge names are gone, and definition/execution intra-body diagnostics no
  longer carry special
  removed-map helper classification branches, and semantic pre-dispatch helper
  path candidates no longer mirror rooted and canonical map helpers, and native
  tail map-access helper probes no longer treat rooted `/map/*` imports or
  definitions as canonical map helper availability, and semantic helper-path
  preference no longer cross-resolves rooted `/map/*` and canonical
  `/std/collections/map/*` definitions, and semantic method resolution no
  longer treats explicit rooted `/map/*` method targets as canonical
  `/std/collections/map/*` helper calls, semantic validation no longer
  recognizes `mapCount`-style wrapper names as special map helper branches,
  and inline/native dispatch no longer treats rooted `/map/*` or experimental
  map helper raw paths as canonical map helper aliases, and lowerer/emitter
  code no longer adapts internal `mapCount`-style helper names as canonical map
  operations, emitter helper candidate generation no longer mirrors rooted
  `/map/*` and canonical `/std/collections/map/*` helpers, and semantic,
  lowerer, and emitter access-helper classification no longer treat rooted
  `/map/at*` calls as builtin map access helpers without same-path
  definitions, and lowerer setup-type, return-kind, and struct-return helper
  path candidates no longer mirror rooted `/map/*` and canonical
  `/std/collections/map/*` helper definitions, and semantic snapshot
  receiver-query collection no longer treats rooted `/map/*` direct calls as
  receiver candidates, and semantic call validation no longer strips rooted
  `/map/*` specialization or overload suffixes to recover removed
  compatibility definition families, and implicit template inference no longer
  classifies rooted `/map/*` template definitions as stdlib collection helpers,
  and template monomorphization no longer strips slashless `map/` method
  prefixes from map receiver method targets, and emitter builtin collection
  inference no longer recognizes slashless `map/count` as an explicit
  map-count helper and now resolves its canonical explicit map-count check
  through stdlib surface metadata, with release validation gates now locking
  those retired semantic and lowerer/emitter adapter names. The emitter helper
  header now resolves canonical map helper path members through stdlib surface
  metadata rather than carrying a hard-coded canonical map helper prefix.
  Lowerer setup-type return-kind inference now uses metadata-backed canonical
  map helper path lookup for string access overrides instead of a production
  C++ map path literal.
  Semantic named-argument builtin access filtering now uses metadata-backed
  canonical map helper lookup for `at`/`at_unsafe` guards instead of a
  production C++ map path prefix.
  Semantic infer target field-type extraction now recognizes the experimental
  map backing type through the shared collection backing-type helper instead
  of a production C++ map path literal.
  Statement container drop/relocation triviality now recognizes experimental
  map backing structs through the shared collection backing-type helper
  instead of a production C++ map path prefix.
  Semantic direct-call return binding inference now recognizes experimental
  map backing specializations through the shared collection backing-type
  helper instead of a production C++ map path prefix.
  Template
  monomorphization now asks the registry for preferred experimental vector/SoA
  helper spellings instead of carrying bespoke canonical-to-experimental maps.
  Scalar pointer/memory builtin validation no longer carries a direct
  rooted-or-canonical map access helper path classifier; only the generic
  memory-`at` map-like operand bypass remains for true memory builtin names.
  String argument validation no longer treats rooted `/map/at*` resolved
  paths or slashless `map/at*_ref` names as explicit map access helper
  classifiers.
  Collection-access validation no longer treats rooted `/map/at*` resolved
  paths as canonical map access helper paths.
  Collection-access validation also no longer treats slashless `map/at*_ref`
  names or a `map` namespace prefix as canonical map access helper names.
  Collection return-kind inference no longer grants rooted `/map/*` helper
  paths builtin map return kinds.
  Collection-dispatch setup inference no longer treats rooted
  `/map/at*_ref` resolved paths or a `map` namespace prefix as canonical map
  access helper-name classifiers.
  Infer-definition deferred map alias detection no longer treats rooted
  `/map/at*` resolved paths as map access helpers.
  Late map access validation no longer treats slashless `map/at*_ref` names
  or a `map` namespace prefix as canonical map access helper names.
  Statement printability no longer treats rooted `/map/at` or
  `/map/at_unsafe` calls as builtin map access printability shortcuts.
  Collection-access resolution no longer treats rooted `/map/at*_ref`,
  slashless `map/at*`, or a `map` namespace prefix as canonical map access
  helper-name classifiers; removed-alias rejection remains separate.
  Struct-return inference no longer carries the explicit `map/at` or
  `/map/at` compatibility probe for map access helper return structs.
  Template monomorphization no longer canonicalizes unknown-target
  diagnostics from rooted `/map/{count,contains,tryAt,at,at_unsafe,insert}`
  helper paths to `/std/collections/map/*`.
  Collection-access validation no longer retargets rooted `/map/*`
  diagnostic targets to `/std/collections/map/*`.
  Direct call resolution no longer returns a missing-target shortcut for
  rooted `/map/at*_ref` access helper calls.
  Diagnostic target formatting no longer carries a rooted `/map/count__t`
  specialization shortcut.
  Infer-method resolution no longer mirrors `/map/*` receiver paths to
  `/std/collections/map/*` candidates or vice versa.
  Pointer-like call target candidate generation no longer appends reciprocal
  `/map/*` and `/std/collections/map/*` helper candidates.
  Preferred map method target selection no longer returns explicit
  `/map/<helper>` definitions or imports ahead of canonical stdlib map
  helpers.
  Bare map helper rewrite target selection no longer falls back to visible
  rooted `/map/<helper>` helper families after canonical lookup.
  Initializer inference no longer prefers or falls back to rooted
  `/map/<helper>` aliases for explicit stdlib map helper targets.
  Method-target resolution no longer prefers rooted `/map/<helper>`
  definitions or imports when choosing map method targets.
  Removed-map body-argument target resolution no longer falls back from
  canonical `/std/collections/map/<helper>` to rooted `/map/<helper>`
  definitions.
  Collection-access resolution no longer prefers rooted `/map/at*` helper
  definitions when resolving canonical map access helper calls.
  SoA public helper, constructor, import-alias, field-view, and conversion
  metadata now lives in
  `stdlib/std/collections/surfaces.psmeta` and is consumed through the generic
  `StdlibSurfaceRegistry` manifest path. The `soa<T>` public surface is
  declared there without temporary `/std/collections/soa_vector/*`,
  same-path `/soa_vector/*`, mixed `/std/collections/{count,get,ref,reserve,push}`,
  experimental helper wrapper, or conversion-helper compatibility spellings.
  old fixture usage of those names was migrated, and the compatibility seams
  now reject. The
  registry keeps surface ids and generic APIs in C++, but it no longer owns
  SoA public collection member lists, import aliases, helper aliases,
  constructor spellings, or conversion spellings as handwritten tables.
  Vector/map and SoA constructor compatibility is metadata-backed by the
  constructor surface adapters. Gfx Buffer helper compatibility is routed
  through `StdlibSurfaceRegistry::GfxBufferHelpers` for canonical
  `/std/gfx/Buffer/*`, legacy `/std/gfx/experimental/Buffer/*`, and rooted
  `/Buffer/*` helper spellings before semantic gfx-buffer rewrites and GPU
  wrapper diagnostics choose canonical builtin targets. Remaining
  removed-helper diagnostics,
  import spellings, wildcard expansion, user-defined helper precedence,
  field-view field-name lowering, gfx constructor sugar, and lowerer raw-path
  dispatch checks are syntax/provenance-owned or lowering-owned.
- Outside this lane: `array<T>` core ownership and the `soa<T>` public-surface
  rename remain separate boundaries tracked by TODO-4305, TODO-4306, and the
  remaining TODO-4524 zero-audit gate.
  Generic contiguous-storage coverage needed before vector ordinary `.prime`
  lowering is complete and recorded in `docs/todo_finished.md`. Map-specific
  lookup/insertion substrate work is complete; remaining map work focuses on
  ordinary helper lowering, metadata deletion, compatibility deletion, and the
  final zero-trace audit.
- End-state rule for vector: production C++ under `src/` and
  `include/` must not contain PrimeStruct-vector-specific paths, helper names,
  type names, diagnostics, parser/lowering branches, or metadata tables.
  `std::vector` as the C++ standard-library container remains allowed; tests,
  docs, generated source-lock fixtures, and stdlib `.prime` files may still
  mention the PrimeStruct vector surface.
- End-state rule for map: after TODO-4464, production C++ under `src/` and
  `include/` must not contain PrimeStruct-map-specific paths, helper names,
  type names, diagnostics, parser/lowering branches, or metadata tables.
  Ordinary C++ words or containers such as `std::map`, generic mapping
  variables, and source-map infrastructure remain allowed; tests, docs,
  generated source-lock fixtures, and stdlib `.prime` files may still mention
  the PrimeStruct map surface.
- End-state rule for SoA: after TODO-4524, the public collection name is
  `soa<T>` under `/std/collections/soa/*`. The old `soa_vector<T>`,
  `/std/collections/soa_vector/*`, `/soa_vector/*`, `SoaVector<T>`,
  `soaVector*`, and direct experimental SoA imports are rejected
  compatibility spellings after TODO-4518 migrated old fixtures. Production
  C++ under `src/` and `include/` must
  not contain PrimeStruct-SoA-collection surface paths, helper names, type
  names, diagnostics, parser/lowering branches, or metadata tables after the
  TODO-4524 zero audit. `scripts/check_soa_surface_trace_inventory.py`
  records the current residue inventory. TODO-4523 deleted parser/header/
  registry source-family traces, TODO-4525 deleted text-filter/IR-printer
  traces, and TODO-4526 through TODO-4529 finish reducing and tightening it.
  Generic SoA substrate terms remain allowed where they do not encode
  the public collection surface: field-layout/codegen/introspection,
  `SoaColumn`, `SoaFieldView`, `SoaSchema*`, field-view
  borrowing/invalidation, and allocation primitives.

### Stdlib De-Experimentalization Policy Summary

- Canonical public API: non-`experimental` namespaces are the intended
  long-term user-facing contracts.
- Canonical collection contract: `/std/collections/vector/*` and
  `/std/collections/map/*` are the sole public vector/map collection surfaces.
- SoA compatibility shim: direct
  `/std/collections/soa_vector*` and `/std/collections/experimental_soa_vector*`
  imports are rejected; canonical public code uses `/std/collections/soa/*`.
- Internal collection implementation modules:
  `/std/collections/internal_vector/*` owns the canonical vector backing
  adapter while preserving the current compatibility `Vector<T>` identity;
  `/std/collections/internal_map/*` owns the canonical map backing adapter
  while preserving the current compatibility `Map<K, V>` identity.
- Vector compatibility shim: direct
  `/std/collections/experimental_vector/*` source imports are rejected; the
  file remains only as a legacy forwarding shim behind
  `/std/collections/internal_vector/*`.
- Map compatibility shim: direct
  `/std/collections/experimental_map/*` source imports are rejected; the file
  remains only as a legacy forwarding shim behind
  `/std/collections/internal_map/*`.
- Legacy gfx compatibility seam: `/std/gfx/experimental/*` remains importable
  only for targeted compatibility coverage and staged migration support;
  canonical `/std/gfx/*` is the only public gfx namespace.
- Internal substrate/helper namespaces:
  `/std/collections/internal_buffer_checked/*`,
  `/std/collections/internal_buffer_unchecked/*`,
  `/std/collections/internal_soa_vector_conversions/*`,
  `/std/collections/internal_soa_vector/*`, and
  `/std/collections/internal_soa_storage/*` are implementation-facing plumbing
  rather than public API.
- Default rule: no `experimental` namespace is canonical public API unless the
  docs call out an accepted compatibility exception explicitly.

### SoA Public Collection Summary

- Rename direction: `soa_vector<T>` is being retired as the public collection
  name in favor of `soa<T>`. The target canonical public spellings are
  `/std/collections/soa/*` plus conversion helpers under the same public
  surface unless TODO-4305 deliberately chooses a separate
  `/std/collections/soa_conversions/*` namespace.
- Retired compatibility spellings are `soa_vector<T>`,
  `/std/collections/soa_vector*`, rooted `/soa_vector/*`, `SoaVector<T>`,
  `soaVector*`, and direct `/std/collections/experimental_soa_vector*` imports;
  ordinary public examples use the `soa` spelling and `/std/collections/soa/*`.
- Rejection seams: C++/VM/native tests lock the direct-import rejection
  diagnostic for retired SoA compatibility modules and the `soa_vector<T>` type
  spelling rejection.
- Internal substrate namespaces: `/std/collections/internal_soa_vector/*`
  owns canonical wrapper implementation forwarding,
  `/std/collections/internal_soa_vector_conversions/*` owns canonical
  conversion implementation forwarding, while
  `/std/collections/internal_soa_storage/*` remains implementation-facing SoA
  storage/layout plumbing. The internal wrapper adapter still preserves the
  internal `SoaVector<T>` backing identity behind the promoted public wrapper
  surface. The inline-parameter and direct lowerer wrapper-dispatch bridges no
  longer use rooted `/soa_vector/*`, `/to_aos`, or `/to_aos_ref` spellings as
  hidden raw fallbacks.
- Promoted contract complete: the canonical public helper wrapper is
  authoritative for ordinary construction/read/ref/mutator/conversion helper
  names, bound field-view borrow-root invalidation, and canonical-only
  C++/VM/native helper, field-view, and conversion parity coverage. Conversion
  receiver contracts are spelled through canonical `soa<T>` surfaces, the
  checked-in ECS example uses canonical `soa` imports, and ordinary
  public code cannot use `experimental_soa_vector` or
  `experimental_soa_vector_conversions` imports. The canonical wrapper routes through
  `/std/collections/internal_soa_vector/*` and canonical conversions route
  through `/std/collections/internal_soa_vector_conversions/*` instead of
  directly importing experimental implementation modules.
- Stdlib-owned metadata: canonical `soa` helper/import/constructor,
  field-view, and conversion metadata lives in
  `stdlib/std/collections/surfaces.psmeta` and is consumed through the generic
  `StdlibSurfaceRegistry` manifest path, while generic SoA substrate metadata
  remains separate from the public collection surface manifest.
- Generic substrate boundary: compiler/runtime-owned SoA behavior is limited
  to field-layout/codegen/introspection, `SoaSchema*` metadata, `SoaColumn<T>`
  and `SoaFieldView<T>` storage/view carriers, borrow-root and invalidation
  provenance, and allocation primitives. Public construction, helper routing,
  conversion naming, import aliases, and compatibility diagnostics belong to
  stdlib wrappers or focused diagnostics.

### Skipped Doctest Debt Summary

- Retained `doctest::skip(true)` coverage is currently absent from the active
  queue because no skipped doctest cases remain under `tests/unit`.
- New skipped doctest coverage must create a new explicit TODO before it lands.
- The success condition for each lane is re-enable-or-delete; indefinite
  skipped coverage is not a stable end state.

### Task Blocks

- [ ] TODO-4271: Add compile-time pack indexing
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - parallel_track: tuple-type-packs
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

- [ ] TODO-4331: Implement compile-time argument channel model
  - owner: ai
  - created_at: 2026-05-04
  - phase: Procedural compile-time genericity
  - depends_on: TODO-4270
  - scope: Rework parser, AST, AST printer, and semantic diagnostics so
    `<...>` is represented as a typed compile-time argument channel while
    preserving existing explicit-template source compatibility.
  - implementation_notes:
    - Start from `docs/PrimeStruct.md`,
      `docs/PrimeStruct_SyntaxSpec.md`, `include/primec/Ast.h`,
      `src/parser/ParserListsSignatures.cpp`, `src/AstPrinter.cpp`, and
      `src/semantics/TemplateMonomorph*.h`.
    - Build on the typed non-type argument representation from TODO-4270
      rather than adding another string channel.
    - Keep existing `name<T>(value)`, `array<i32>`, `return<T>`, and
      transform template-argument behavior source-compatible.
  - acceptance:
    - The AST distinguishes type, integer, symbol, and future compile-time
      argument categories without changing accepted existing template sources.
    - AST/semantic dumps continue to print current template forms
      deterministically.
    - Invalid compile-time argument shapes produce stable diagnostics that
      name the compile-time argument channel.
    - Docs state that traditional templates are one use of `<...>`.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once `<...>` has a typed internal representation and all
    existing template tests still pass; leave bare zero-arg execution to
    TODO-4332.

- [ ] TODO-4332: Add bare zero-arg execution syntax
  - owner: ai
  - created_at: 2026-05-04
  - phase: Procedural compile-time genericity
  - depends_on: TODO-4331
  - scope: Allow a bare form such as `name` to execute a visible zero-argument
    definition in command and value position when resolution is unique.
  - implementation_notes:
    - Start from parser expression/name handling under `src/parser/`,
      semantic call resolution, return-expression validation, and existing
      tests that distinguish names from calls.
    - Preserve current binding reads and field-access behavior; do not make
      every name eagerly callable before semantic resolution.
  - acceptance:
    - `foo { return(1) }` and `bar { return(foo) }` execute `foo` and return
      its result after transforms/literal inference.
    - Statement-position `foo` executes a zero-arg definition exactly as
      `foo()` would.
    - Runtime argument calls and explicit `foo()` continue to work unchanged.
    - Unknown bare names and non-zero-arg callable bare names produce stable
      diagnostics.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once unique zero-arg definitions can be executed by bare
    name; leave ambiguous-name rejection to TODO-4333.

- [ ] TODO-4333: Reject ambiguous value/execution names
  - owner: ai
  - created_at: 2026-05-04
  - phase: Procedural compile-time genericity
  - depends_on: TODO-4332
  - scope: Add deterministic diagnostics for scopes where a bare name could
    mean both a stack value and a visible zero-argument callable, or where
    multiple callable/import candidates make the name non-unique.
  - implementation_notes:
    - Start from semantic name resolution, import alias resolution, local
      binding maps, and helper/call target diagnostics.
    - Prefer fail-closed behavior: if the compiler cannot prove one entity,
      reject instead of guessing.
  - acceptance:
    - A local binding and visible zero-arg definition with the same bare name
      reject before lowering.
    - Import aliases that make a bare zero-arg execution ambiguous reject with
      stable related-path diagnostics.
    - A unique local binding still reads as a value; a unique zero-arg
      callable still executes.
    - Runtime locals, compile-time type locals, generated type names, and
      visible zero-arg definitions share one bare-name namespace; duplicate
      names in that namespace reject deterministically.
    - Diagnostics are deterministic across import order and repeated builds.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once bare-name ambiguity is rejected consistently across
    local, definition, and import-alias scopes.

- [ ] TODO-4334: Add compile-time `[type]` local bindings
  - owner: ai
  - created_at: 2026-05-04
  - phase: Procedural compile-time genericity
  - depends_on: TODO-4333
  - scope: Parse and validate compile-time local type bindings such as
    `[type] LeftT { ... }` without yet allowing those locals in every
    envelope position.
  - implementation_notes:
    - Start from binding transform parsing, semantic binding validation,
      semantic-product binding facts, and local-auto/type inference code.
    - `[type]` bindings are compile-time facts, not runtime stack slots.
  - acceptance:
    - `[type] Name { expr }` parses in definition bodies and is rejected at
      top level or in unsupported scopes with stable diagnostics.
    - Type locals do not allocate runtime storage and do not appear as ordinary
      value bindings in IR.
    - Invalid initializers, duplicate local names, mutation qualifiers, and
      runtime-only expressions reject deterministically.
    - Semantic-product dumps can expose type-local provenance or explicitly
      document the temporary adapter boundary.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once type locals are represented and validated as
    compile-time facts; leave `typeof<symbol>` to TODO-4335.

- [ ] TODO-4335: Add `typeof<symbol>` compile-time primitive
  - owner: ai
  - created_at: 2026-05-04
  - phase: Procedural compile-time genericity
  - depends_on: TODO-4334
  - scope: Add the compile-time primitive `typeof<symbol>` so type locals can
    bind the concrete type of a parameter, local value, or other supported
    visible entity.
  - implementation_notes:
    - Reuse the compile-time argument channel from TODO-4331 and the
      ambiguity diagnostics from TODO-4333.
    - Do not accept `typeof(value)` as this primitive; parentheses remain the
      runtime argument channel.
  - acceptance:
    - `[type] T { typeof<value> }` binds the concrete type of an unambiguous
      value or parameter.
    - `typeof<missing>`, ambiguous symbols, unsupported symbol kinds, and
      runtime-argument forms reject with stable diagnostics.
    - The primitive works in generic definitions after monomorphization and
      produces deterministic facts for repeated call sites.
    - Docs and tests show why `typeof<value>` differs from a runtime call.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once `typeof<symbol>` can feed `[type]` locals; leave
    later envelope consumption to TODO-4336.

- [ ] TODO-4336: Allow type locals in envelope positions
  - owner: ai
  - created_at: 2026-05-04
  - phase: Procedural compile-time genericity
  - depends_on: TODO-4335
  - scope: Let compile-time type locals appear in local binding and
    struct-field envelope positions where concrete types are required.
  - implementation_notes:
    - Start from template type resolution, binding transform validation,
      struct layout validation, and semantic-product type metadata.
    - Leave public parameter/return envelope use of type locals to a later
      task once generated-type escape and caller-visible naming are settled.
    - Type locals must resolve before layout and lowering; unresolved type
      locals must fail before IR.
  - acceptance:
    - A type local produced by `typeof<value>` can annotate a later local or
      struct field in the same definition specialization.
    - Type locals in public parameter or return envelopes reject with a stable
      diagnostic unless another completed task has defined their
      caller-visible naming/export behavior.
    - Type locals cannot escape their valid scope except through generated
      concrete types or ordinary return values.
    - Cycles, forward references where unsupported, and runtime value use of a
      type local reject deterministically.
    - Semantic-product/lowering tests verify no backend re-derives type-local
      facts from source text.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once type locals are usable in later local-binding and
    struct-field envelopes; leave generated local structs to TODO-4337 and
    public parameter/return envelopes to a future caller-visible naming task.

- [ ] TODO-4337: Add local generated nominal structs
  - owner: ai
  - created_at: 2026-05-04
  - phase: Procedural compile-time genericity
  - depends_on: TODO-4336
  - scope: Support function-local generated struct definitions such as
    `[struct] PairT { [LeftT] first [RightT] second }` whose fields can use
    type locals.
  - implementation_notes:
    - Start from nested definition parsing, AST ordering rules, struct layout
      validation, template monomorphization family cloning, and semantic
      product type/field metadata.
    - Generated structs are nominal per enclosing definition specialization,
      not structural aliases.
  - acceptance:
    - A local generated struct can be constructed and used inside the
      enclosing definition specialization.
    - The struct's fields may use type locals that resolve from parameters or
      earlier compile-time facts.
    - Directly returning a local generated type rejects unless a later feature
      has defined an explicit caller-visible naming/export mechanism.
    - Tests or docs show that a returnable pair-like API must use a
      caller-visible generic type such as `Pair<LeftT, RightT>`, not a
      function-local generated struct.
    - Name shadowing, recursive generated storage, invalid field envelopes,
      and escaping unsupported type locals reject deterministically.
    - Struct metadata and field metadata are published for lowering without
      source-text reconstruction.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once local generated structs are validated, usable inside
    the enclosing specialization, and rejected when they escape without an
    explicit caller-visible name; leave path/mangling hardening to TODO-4338.

- [ ] TODO-4338: Stabilize generated type identity and mangling
  - owner: ai
  - created_at: 2026-05-04
  - phase: Procedural compile-time genericity
  - depends_on: TODO-4337
  - scope: Define and implement deterministic paths, names, provenance, and
    diagnostics for local generated nominal types across repeated builds and
    import order.
  - implementation_notes:
    - Start from template specialization mangling, semantic node ids,
      semantic-product provenance handles, AST/IR printers, and diagnostics
      that mention generated type paths.
    - Avoid path schemes that depend on unordered map iteration or incidental
      clone order.
  - acceptance:
    - Generated type paths include the enclosing definition specialization and
      local type name in a deterministic form.
    - Repeated builds and import-order variations produce identical
      semantic-product and IR names for the same source.
    - Diagnostics for generated type errors point at the local type definition
      and the relevant type-local facts.
    - Golden/snapshot tests pin the generated identity surface.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once generated type identity is deterministic and covered;
    leave full lowering integration to TODO-4339.

- [ ] TODO-4339: Lower procedural generic facts through semantics
  - owner: ai
  - created_at: 2026-05-04
  - phase: Procedural compile-time genericity
  - depends_on: TODO-4338, TODO-4269
  - scope: Route type locals, `typeof<symbol>` facts, and local generated
    types through template monomorphization, semantic-product publication, and
    IR lowering without backend-specific source reconstruction.
  - implementation_notes:
    - Start from `TemplateMonomorph*.h`, `SemanticProduct.h`,
      `SemanticsValidatorSnapshots.cpp`, `IrPreparation.cpp`, and lowerer
      setup/struct layout code.
    - This is the semantic/lowering handoff slice for the procedural generic
      model, not a new template solver.
  - acceptance:
    - The post-semantics AST and semantic-product dump contain concrete
      generated types and no unresolved type locals.
    - IR lowering consumes published semantic facts for generated type layout
      and constructor use.
    - VM/native/C++ compile-run coverage executes at least one procedural
      generic helper that uses a local generated type internally and returns a
      caller-known value.
    - Missing semantic-product facts fail closed with deterministic
      diagnostics instead of syntax-only fallback.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once procedural generic facts lower through the ordinary
    semantic-product boundary; leave examples/docs polish to TODO-4340.

- [ ] TODO-4340: Add procedural generic examples and conformance
  - owner: ai
  - created_at: 2026-05-04
  - phase: Procedural compile-time genericity
  - depends_on: TODO-4339
  - scope: Add user-facing examples, syntax docs, and conformance coverage for
    procedural compile-time genericity.
  - implementation_notes:
    - Start from `docs/PrimeStruct_SyntaxSpec.md`, `docs/CodeExamples.md`,
      `examples/`, parser/semantic diagnostics tests, compile-pipeline dumps,
      and compile-run suites.
    - Keep examples style-aligned: the feature should read like ordinary
      left-to-right PrimeStruct code, not like C++ template metaprogramming.
  - acceptance:
    - Docs include a minimal local generated-type example using `[type]`,
      `typeof<...>`, local `[struct]`, brace construction, and a non-escaping
      caller-known return value.
    - Examples compile and run under at least VM and native where supported.
    - Negative conformance covers ambiguous bare names, bad `typeof` targets,
      invalid type-local use, generated-type escape rejection, and
      generated-type identity diagnostics.
    - Source-lock/example-lock tests are updated if the docs/examples surface
      changes.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the procedural genericity surface is documented,
    example-backed, and covered by positive and negative conformance tests.

- [ ] TODO-4341: Define generic requirement predicate surface
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4331, TODO-4334
  - scope: Specify the user-facing requirement predicate surface for
    procedural generic code so authors can state type requirements directly
    instead of relying on late body failures.
  - implementation_notes:
    - Start from `docs/PrimeStruct.md`,
      `docs/PrimeStruct_SyntaxSpec.md`, the existing transform-style trait
      examples such as `[Additive<i32>]`, and planned
      `meta.has_trait<...>` reflection hooks.
    - Decide whether requirements are expressed as a statement, transform,
      compile-time command, or another surface form; document why the chosen
      form fits the `<...>` compile-time argument channel.
    - Use definition-transform syntax such as
      `[require(typeof<left> == i32, typeof<right> == i32)] add(left, right)
      { ... }` as the readability target, and document why `typeof<left>` is
      the compile-time type query while `typeof(left)` remains an ordinary
      runtime call shape.
    - Treat `require(...)` as a single transform with a comma-separated
      predicate list; repeated `[require(...)]` transforms are not the planned
      source style.
    - Keep v1 predicate expressions intentionally small: equality,
      inequality, comma-separated conjunction, builtin/user-defined predicate
      calls, and simple compile-time value comparisons such as `N > 0`.
    - Put builtin predicates under `/std/meta/*`; readable requirement syntax
      may rewrite to those helpers or compiler-recognized facts, but public
      user helpers should not collide with that namespace.
    - Include the initial predicate family names for equality, trait/capability
      support, construction, copy/move availability, field/member queries, and
      compile-time integer/value relations.
  - acceptance:
    - The docs define a minimal generic requirement syntax with examples for
      same-type checks and capability/trait checks.
    - The selected syntax uses a single bracketed `[require(...)]` transform
      form with multiple comma-separated predicates; repeated `require`
      transforms are rejected or canonicalized with a stable diagnostic.
    - The docs specify when requirements run, whether they can produce runtime
      values, and how they interact with ordinary function bodies.
    - The docs define how user-authored compile-time predicate helpers return
      ordinary source-level `bool` values, how semantics wraps those values as
      requirement facts, and how those helpers compose with builtin
      predicates.
    - The docs distinguish a predicate returning `false` from an invalid
      predicate evaluation; invalid predicate evaluation is a hard diagnostic,
      not a failed requirement or silent overload-filtering event.
    - The surface explicitly rejects C++-style SFINAE-by-accident; failed
      requirements are diagnostics, not silent candidate erasure unless a
      later overload-selection task says so.
    - Existing transform-style trait examples are either aligned with the new
      requirement vocabulary or marked as a compatibility surface.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the requirement language is specified tightly enough
    for parser and semantic-product implementation work to proceed.

- [ ] TODO-4342: Represent requirement predicates as semantic facts
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4341
  - scope: Add typed AST and semantic-product representation for generic
    requirement predicates and their evaluated results.
  - implementation_notes:
    - Start from `include/primec/Ast.h`, parser requirement syntax from
      TODO-4341, `SemanticProduct.h`, semantic validation snapshots, and the
      established graph-backed fact authority.
    - Preserve source provenance for every predicate, operand, inferred type
      fact, and final pass/fail result.
    - Represent predicate evaluation outcomes distinctly: satisfied,
      unsatisfied, and invalid evaluation.
    - Keep predicates typed; do not lower them into strings or backend-local
      source reconstruction.
  - acceptance:
    - Requirement predicates parse into typed nodes or typed semantic records
      with stable AST/semantic-product dump output.
    - Predicate operands distinguish type facts, values, compile-time
      symbols, literal compile-time arguments, and unsupported runtime-only
      expressions.
    - Predicate facts can reference future typed compile-time values by stable
      handles without requiring the CT value model to be implemented in this
      task.
    - Semantic validation publishes requirement facts before IR lowering and
      fails closed when a required fact is missing.
    - Invalid predicate evaluation is represented separately from a failed
      requirement so overload resolution and diagnostics cannot treat compiler
      or predicate-body errors as non-viable candidates.
    - Existing template and procedural-generic tests remain source-compatible.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once requirements have a typed semantic representation
    and diagnostics can point at their source operands.

- [ ] TODO-4343: Add builtin type relation predicates
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4342, TODO-4335
  - scope: Implement the initial builtin predicates for relations between
    compile-time type facts, limited to equality and type-kind checks.
  - implementation_notes:
    - Start with equality and kind checks such as same-type, is-type,
      is-struct, and is-sum.
    - Do not add type-argument or element extraction in this slice; split that
      into a later TODO once tuple/collection type-pack facts are stable.
    - Canonicalize readable source predicates such as `typeof<left> == i32`
      into builtin compile-time predicate calls such as `equals<left, i32>`
      before requirement facts are published.
    - Place builtin relation predicates under `/std/meta/*` or a compiler
      fact namespace that is surfaced through `/std/meta/*` docs.
    - Reuse `typeof<symbol>` and type-local facts from the procedural
      genericity lane rather than adding a second inference path.
    - Keep relation evaluation deterministic and independent of import order.
  - acceptance:
    - A generic helper can require an inferred type or symbol to equal a
      concrete type and reject a mismatched call before lowering.
    - Type-kind predicates work on generated nominal structs and ordinary
      named types with stable results.
    - Unsupported extraction-style predicates produce stable diagnostics or
      are documented as deferred rather than partially implemented.
    - Builtin predicate names and user predicate names cannot collide
      ambiguously with `/std/meta/*`.
    - Unsupported operands and unknown relation names produce stable
      diagnostics that name the requirement predicate.
    - Semantic-product tests cover positive, negative, and generated-type
      predicate cases.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once relation predicates are usable by semantic
    validation; leave trait/capability checks to TODO-4344.

- [ ] TODO-4344: Add capability and trait support predicates
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4342
  - scope: Implement the first capability/trait predicate slice for named
    operation support, construction availability, and one lifecycle
    availability query.
  - implementation_notes:
    - Start from transform-style trait constraints, lifecycle helper
      validation, helper/call resolution, struct constructor validation, and
      `meta.has_trait<...>` documentation.
    - Use C++'s practical generic toolbox as inspiration for the initial
      builtin surface while keeping PrimeStruct facts typed and deterministic:
      constructible, copyable, movable, comparable, callable, field/member
      presence, and operation/trait support, but implement only the minimal
      subset needed for this leaf.
    - Reflection-style predicates must obey normal visibility: private fields
      and helpers are invisible outside their visibility boundary unless a
      later privileged reflection feature explicitly authorizes access.
    - Define the exact bridge from legacy trait transforms to requirement
      facts so old code stays compatible while new generic code has one
      canonical predicate vocabulary.
    - Treat support checks as explicit semantic facts; do not execute arbitrary
      runtime code to prove a capability.
  - acceptance:
    - Requirement predicates can check at least one arithmetic/comparison
      trait, one constructor shape, and one lifecycle capability.
    - Other planned capability names are listed as deferred or split into
      follow-up TODOs if they are not implemented in this slice.
    - Failed capability checks produce diagnostics at the requirement site and
      include the call-site type facts that caused the failure.
    - Overloaded helpers, imported traits, and generated types produce
      deterministic support results.
    - Field/member predicates cannot observe private fields from outside the
      declaring type's visible API.
    - Compatibility behavior for existing transform-style traits is covered by
      tests or documented as still pending with a narrower follow-up TODO.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once generic code can state useful capability
    requirements without depending on body-instantiation failure.

- [ ] TODO-4352: Add compile-time VM facade and host
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4342, TODO-4339
  - scope: Define and add the public compiler-internal facade boundary for
    compile-time evaluation, without yet executing arbitrary user predicates.
  - implementation_notes:
    - Start from `include/primec/Vm.h`, `src/VmExecution.cpp`,
      `src/VmExecution.h`, `src/VmControlFlowOpcodeShared.*`,
      `src/VmNumericOpcodeShared.*`, semantic-product publication, and the
      requirement predicate representation from TODO-4342.
    - Leave the concrete typed compile-time value model to TODO-4353; this
      task only defines the facade boundary and result categories.
    - Define the facade API, result/fault categories, provenance inputs, and
      host interface expected by later CT value, VM-kernel, and host tasks.
    - Requirement evaluation runs before final backend lowering is complete,
      so the facade contract must explicitly forbid depending on final backend
      IR or launching `primevm`.
  - acceptance:
    - A compile-time evaluation facade type/API exists and can be included by
      semantics code without depending on runtime CLI entrypoints.
    - The facade result model distinguishes success, unsatisfied predicate,
      invalid evaluation, denied effect, budget exhaustion, and internal
      compiler errors.
    - The facade contract documents that final backend IR and `primevm` are
      unavailable during semantic requirement evaluation.
    - Stubbed facade tests verify result/fault formatting and provenance
      plumbing without running arbitrary user code.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the compiler has a stable compile-time evaluation
    boundary that later tasks can implement behind.

- [ ] TODO-4353: Add typed compile-time value model
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4352, TODO-4342
  - scope: Add the typed compile-time value and result representation used by
    requirement predicates, `/std/meta/*` intrinsics, and future CT helpers.
  - implementation_notes:
    - Start from semantic-product type facts, compile-time integer arguments
      from TODO-4270, string-literal/string-table handling, and requirement
      result representation from TODO-4342.
    - Model at least `bool`, signed/unsigned integer constants, string
      literals, type facts, symbols, and requirement outcomes.
    - Keep values deterministic and printable; no backend/runtime slot layout
      should leak into this layer.
  - acceptance:
    - CT values have stable equality, hashing, debug formatting, and
      provenance handles suitable for cache keys and diagnostics.
    - `true`/`false` source predicate results can be wrapped as satisfied or
      unsatisfied requirement facts.
    - Unsupported runtime-only values reject with a stable invalid-evaluation
      result instead of being represented as raw VM slots.
    - Unit tests cover value equality, formatting, provenance preservation,
      and unsupported value rejection.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once compile-time values can represent predicate inputs
    and outputs independently of runtime VM `uint64_t` slots.

- [ ] TODO-4354: Factor reusable VM interpreter kernel
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4352
  - scope: Factor runtime VM execution so arithmetic, branching, calls, frame
    mechanics, and deterministic faults can be reused by compile-time
    evaluation without sharing runtime-only host state.
  - implementation_notes:
    - Start from `src/VmExecution.cpp`, `src/VmExecution.h`,
      `src/VmExecutionNumeric.*`, `src/VmControlFlowOpcodeShared.*`,
      `src/VmNumericOpcodeShared.*`, `src/VmHeapHelpers.*`, and
      `src/VmIoHelpers.*`.
    - Keep `Vm::execute(...)` behavior and public ABI stable while extracting
      narrow reusable helpers or a kernel interface.
    - Do not introduce compile-time dependencies into runtime-only debug,
      argv, heap, or IO paths.
  - acceptance:
    - Runtime `Vm::execute(...)` continues to pass existing VM tests with no
      behavior changes.
    - Shared interpreter code has an explicit host boundary for operations
      that differ between runtime and compile time.
    - Compile-time code can include the shared kernel without pulling in
      `primevm_main.cpp`, debug adapter state, argv ownership, or runtime IO
      helpers.
    - Source-lock or unit coverage protects the new kernel boundary from
      collapsing back into one runtime-only execution loop.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once VM execution has a reusable kernel boundary while
    preserving runtime VM behavior.

- [ ] TODO-4355: Add compile-time host and meta intrinsics
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4353, TODO-4354, TODO-4343, TODO-4344
  - scope: Implement `CompileTimeHost` support for semantic facts,
    `/std/meta/*` builtin predicates, provenance, and pure compile-time
    intrinsic dispatch.
  - implementation_notes:
    - Start from builtin predicate tasks TODO-4343/TODO-4344, semantic-product
      facts, visibility metadata, import resolution, and the CT value model.
    - Include relation predicates, capability/trait predicates, and
      field/member queries that obey normal visibility.
    - Keep `/std/meta/*` dispatch typed and deterministic; readable source
      predicates may lower to host intrinsics or explicit stdlib helper calls.
  - acceptance:
    - The CT host can answer builtin `/std/meta/*` predicates using published
      semantic facts without backend source reconstruction.
    - Reflection-style predicates cannot observe private fields outside their
      visibility boundary.
    - Unknown, ambiguous, or unsupported meta predicates return invalid
      evaluation with deterministic diagnostics.
    - Unit tests cover type equality, kind/capability checks, field visibility,
      and canonical `/std/meta/*` names.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once builtin meta predicates are executable through the
    compile-time host.

- [ ] TODO-4356: Add restricted compile-time callable lowering
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4354, TODO-4355, TODO-4339
  - scope: Add the restricted lowering or CT bytecode/fact-evaluator path
    needed to run compile-time callables before final backend IR exists.
  - implementation_notes:
    - Start from template monomorphization, semantic-product publication,
      IR-preparation/lowering boundaries, and the VM kernel facade.
    - Only allow operations supported by the compile-time host and CT value
      layer; reject runtime-only expressions before execution.
    - Avoid final backend IR dependencies and avoid launching `primevm`.
  - acceptance:
    - A pure compile-time callable can be prepared for CT execution during
      semantic validation.
    - Runtime-only operations, unsupported value shapes, and missing semantic
      facts fail before execution with deterministic diagnostics.
    - Prepared CT callables preserve source provenance for call sites,
      predicate bodies, and failed operations.
    - Tests prove CT callable preparation does not require final VM/native/C++
      lowering artifacts.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once pure CT callables can be prepared independently of
    final backend lowering.

- [ ] TODO-4357: Evaluate pure user predicates at compile time
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4356
  - scope: Execute pure user-defined requirement predicates that return source
    `bool`, then publish satisfied/unsatisfied/invalid requirement facts for
    semantic validation and overload filtering.
  - implementation_notes:
    - Start from the compile-time facade, CT callable preparation, requirement
      fact publication, and diagnostics for invalid predicate evaluation.
    - Keep ordinary `false` separate from VM/evaluator faults.
    - Do not enable compile-time IO or other effects in this slice.
  - acceptance:
    - A user-defined pure predicate returning `true` satisfies a requirement.
    - A user-defined pure predicate returning `false` makes the requirement
      unsatisfied and reports that fact at the requirement/call site.
    - Invalid predicate bodies, denied effects, unsupported operations, and
      missing facts are hard diagnostics, not non-viable candidates.
    - The semantic product records predicate identity, CT arguments, result
      category, and provenance.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once pure user predicates can drive requirement facts
    through semantic validation.

- [ ] TODO-4345: Add compile-time `if` over type facts
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4343, TODO-4344, TODO-4357
  - scope: Add a compile-time branching form that selects code paths from
    evaluated requirement/type facts before IR lowering.
  - implementation_notes:
    - Start from parser statement forms, semantic rewrites, requirement fact
      evaluation, and existing compile-time transform infrastructure.
    - Only the selected branch may contribute generated definitions, local
      type facts, runtime statements, or diagnostics from selected code.
    - Document how compile-time `if` differs from runtime `if` and how the
      language spells the distinction.
  - acceptance:
    - Generic code can branch on a type relation or capability predicate and
      lower only the selected branch.
    - The non-selected branch is still parsed enough for structural errors
      needed by the language contract, but unsupported type operations inside
      that branch do not fail the selected specialization.
    - Branch-local type facts and generated nominal types have deterministic
      scope and identity.
    - Diagnostics clearly identify compile-time branch conditions and selected
      versus discarded branches.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once compile-time branching is usable for type-directed
    generic implementation; leave global effect policy to TODO-4346.

- [ ] TODO-4346: Add compile-time flow effect and termination policy
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4345
  - scope: Define the purity, effect, recursion, caching, and termination
    policy for compile-time generic execution, leaving full enforcement to
    TODO-4358.
  - implementation_notes:
    - Start from docs for deterministic semantics, semantic validation pass
      ordering, template monomorphization recursion guards, import handling,
      and any constant-evaluation helpers.
    - Compile-time flow is pure by default and must not perform side effects,
      read ambient process state, or make backend-dependent decisions without
      explicit phase-qualified `effects<compiletime>(...)` opt-in on the
      enclosing definition.
    - Runtime `effects(...)` and compile-time effects use the same effect
      vocabulary but are interpreted by their own phase; runtime effects do
      not automatically authorize compile-time IO.
    - Caching keys must include the predicate/helper identity, compile-time
      arguments, visible imports and semantic facts, active compile-time
      effects, and the language/semantic-product version, while remaining
      stable across import order.
  - acceptance:
    - Docs specify which operations are legal during compile-time generic
      execution by default, which require explicit
      `effects<compiletime>(...)` opt-in, and which are always rejected.
    - Docs define recursion/instruction budget categories, cache-key fields,
      and the diagnostic categories that TODO-4358 must enforce.
    - Existing compile-time execution tests, if any, are updated only to match
      the documented policy; broad enforcement tests remain in TODO-4358.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the compile-time flow policy is precise enough for
    TODO-4358 to implement without making new language-design decisions.

- [ ] TODO-4358: Enforce compile-time cache, budget, and effects
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4346, TODO-4357
  - scope: Implement deterministic compile-time evaluation cache keys,
    recursion/instruction budgets, phase-qualified effect enforcement, and
    related diagnostics for the compile-time VM facade.
  - implementation_notes:
    - Start from the CT facade, `CompileTimeHost`, semantic-product versioning,
      import/fact publication, and effect metadata.
    - Cache keys must include predicate/helper identity, compile-time
      arguments, visible imports and semantic facts, active
      `effects<compiletime>(...)`, and the language/semantic-product version.
    - Effect checks must distinguish runtime `effects(...)` from
      compile-time `effects<compiletime>(...)`.
  - acceptance:
    - Repeated pure CT evaluations reuse cached results while preserving
      identical diagnostics when cache reuse is disabled.
    - Recursive or runaway CT evaluation stops with a deterministic
      budget-exhaustion diagnostic and provenance.
    - Compile-time IO or other effectful host operations reject without the
      matching `effects<compiletime>(...)` transform.
    - Cache-key tests prove that changing imports, semantic facts, CT
      arguments, active CT effects, or semantic-product version invalidates
      stale results.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once compile-time evaluation is deterministic, bounded,
    effect-gated, and cache-safe for semantic validation.

- [ ] TODO-4347: Integrate requirements with overload selection
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4344, TODO-4357, TODO-4358
  - scope: Route requirement facts into call and overload resolution so
    constrained generic candidates are selected or rejected deterministically.
  - implementation_notes:
    - Start from call resolution, import alias resolution, helper selection,
      template inference, ambiguity diagnostics, and semantic-product
      candidate metadata.
    - Prefer explicit candidate rejection with related diagnostics over
      silent C++-style substitution failure unless TODO-4341 chose a specific
      overload-filtering contract.
    - Do not rank candidates by requirement specificity. If more than one
      candidate remains viable after requirement filtering, reject as
      ambiguous and require clearer names or imports.
  - acceptance:
    - A constrained generic candidate is callable only when its requirements
      pass for the inferred compile-time facts.
    - Multiple viable constrained overloads reject deterministically instead
      of being automatically ranked by specificity.
    - Failing candidates contribute concise related diagnostics when no
      overload is viable.
    - Existing unconstrained calls and explicit template calls preserve their
      current behavior.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once requirements participate in call resolution without
    backend-local or order-dependent candidate behavior.

- [ ] TODO-4351: Add value-level compile-time requirement facts
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4353, TODO-4342, TODO-4270
  - scope: Extend requirement predicates from type facts to typed
    compile-time value facts such as integer template arguments, tuple
    indexes, array extents, SIMD widths, and GPU/workgroup dimensions.
  - implementation_notes:
    - Start from the compile-time integer argument representation in
      TODO-4270, requirement fact publication from TODO-4342, parser support
      for readable predicate expressions, and diagnostics for constant
      expression evaluation.
    - Keep v1 value predicates narrow: equality, inequality, ordered integer
      comparisons, range checks, divisibility/alignment checks, and named
      builtin predicates where the operand type is already a compile-time
      value fact.
    - Avoid a separate value-metaprogramming language; value facts should use
      the same compile-time argument channel and semantic-product provenance
      as type facts.
  - acceptance:
    - A generic helper can require a compile-time integer argument to satisfy
      a predicate such as `N > 0` and reject a failing specialization before
      lowering.
    - Value predicate facts publish operand values, predicate names, pass/fail
      results, and source provenance through the semantic product.
    - Invalid value predicates, unsupported operand types, and non-constant
      operands produce stable diagnostics.
    - Tests cover array/tuple-style lengths or indexes, one GPU/SIMD-style
      dimension predicate if that surface exists, and at least one negative
      range/alignment diagnostic.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once compile-time value facts can participate in
    requirements with the same provenance and determinism guarantees as type
    facts.

- [ ] TODO-4348: Publish requirement diagnostics with provenance
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4347, TODO-4351
  - scope: Add stable, user-facing diagnostics for failed requirements,
    ambiguous requirement-driven overloads, and invalid compile-time flow.
  - implementation_notes:
    - Start from the diagnostic engine, semantic-product provenance handles,
      template instantiation diagnostics, generated type path diagnostics, and
      golden diagnostic tests.
    - Diagnostics should show the requirement site, the call site, the
      relevant inferred type facts, and the missing/failed predicate.
    - Use the diagnostic style examples in
      `docs/PrimeStruct_SyntaxSpec.md` as the target shape: call site first,
      failed requirement second, concrete facts third, then a short actionable
      hint.
    - Emit different diagnostic categories for unsatisfied predicates versus
      predicates that cannot be evaluated.
    - Keep output deterministic across import order and repeated builds.
  - acceptance:
    - Failed requirement diagnostics point at both the generic requirement and
      the concrete call or specialization that triggered it.
    - Overload failures summarize why each relevant candidate was rejected
      without dumping every internal predicate detail by default.
    - Compile-time branch and termination diagnostics include enough
      provenance to debug generated types and type-local facts.
    - Golden diagnostics cover failed type requirements, ambiguous constrained
      calls, local generated type escape, missing compile-time effects, and
      value-level predicates such as `N > 0`.
    - Golden diagnostics cover at least one invalid user-defined predicate
      body and make clear that it is not merely a failed requirement.
    - Golden diagnostic tests pin representative success and failure output.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once requirement failures are explainable from user code
    without inspecting compiler internals.

- [ ] TODO-4359: Add compile-time VM conformance coverage
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4348, TODO-4358
  - scope: Add focused conformance coverage for the compile-time VM facade,
    host, typed values, pure user predicates, cache/budget behavior, and
    phase-qualified effects.
  - implementation_notes:
    - Start from CT facade unit tests, semantic-product snapshot tests,
      parser/semantic diagnostics tests, and compile-pipeline golden tests.
    - Prefer narrow tests that prove CT execution uses the compiler-hosted
      facade and not final backend IR or `primevm`.
    - Include source-lock coverage only where it protects an intentional
      architecture boundary such as shared VM kernel extraction.
  - acceptance:
    - Tests cover builtin `/std/meta/*` predicates, pure user predicates,
      `true` versus `false`, invalid evaluation, and typed CT value formatting.
    - Tests cover missing `effects<compiletime>(...)`, allowed
      compile-time effects, cache-key invalidation, and budget exhaustion.
    - Tests prove requirement evaluation does not require final VM/native/C++
      lowering artifacts or `primevm`.
    - Diagnostics coverage distinguishes unsatisfied predicates from invalid
      predicate evaluation.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once compile-time VM behavior has targeted regression
    coverage independent of the broader generic conformance matrix.

- [ ] TODO-4349: Add generic constraint conformance matrix
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4348, TODO-4359
  - scope: Add parser, semantic, IR-preparation, compile-run, and diagnostic
    conformance coverage for the generic requirement and compile-time flow
    model.
  - implementation_notes:
    - Start from existing parser/semantics/lowering test shards,
      compile-pipeline dumps, compile-run suites, and source-lock/example-lock
      coverage.
    - Cover both the high-level language surface and the semantic-product
      facts consumed by IR lowering.
    - Include negative tests for ambiguity, unsupported predicates, missing
      compile-time effect opt-ins, and non-selected compile-time branches.
  - acceptance:
    - Positive tests cover same-type requirements, capability requirements,
      compile-time value requirements, compile-time branching, generated
      types, and constrained overloads.
    - Negative tests cover failed requirements, ambiguous candidates,
      effect opt-in rejection, recursion-limit rejection, value predicate
      failure, and provenance output.
    - IR/semantic-product tests prove unresolved requirement or type facts do
      not reach backend lowering.
    - Source-lock/example-lock tests are updated for any user-facing examples
      added by this lane.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the feature has enough conformance coverage that a
    later backend or syntax refactor cannot silently change semantics.

- [ ] TODO-4350: Add high-level generic design examples
  - owner: ai
  - created_at: 2026-05-04
  - phase: Generic constraint and compile-time flow alignment
  - depends_on: TODO-4349
  - scope: Add docs and example programs that demonstrate PrimeStruct's
    intended generic coding style against common alternatives.
  - implementation_notes:
    - Start from `docs/PrimeStruct.md`, `docs/PrimeStruct_SyntaxSpec.md`,
      `docs/CodeExamples.md`, `examples/`, and any conformance examples from
      TODO-4349.
    - Include examples such as identity, caller-visible `Pair<LeftT, RightT>`
      construction, non-escaping local generated struct use, same-type add,
      value-level length requirements, optional type-directed branch, and a
      constrained overload pair.
    - Align generic naming conventions in examples: use `T`, `ElemT`,
      `LeftT`, `RightT`, and value-level names such as `N` consistently.
    - Keep the examples procedural and readable: type work should look like a
      left-to-right program over named facts, not a template metaprogramming
      puzzle.
  - acceptance:
    - Docs include a short "generic coding style" section that explains when
      to use plain inference, requirements, compile-time value facts,
      compile-time branching, generated types, and explicit template
      arguments.
    - At least one runnable example compares unconstrained failure with a
      requirement-based diagnostic.
    - Examples compile under the supported backends for the feature or carry a
      narrow documented skip reason.
    - `docs/todo.md` is updated to remove or split any remaining genericity
      follow-up discovered during example work.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the generic design direction is documented through
    runnable examples rather than only prose.

- [~] TODO-4464: Add full zero C++ map-surface audit
  - owner: ai
  - created_at: 2026-05-14
  - phase: Map stdlib ownership cutover
  - parallel_track: map-zero-audit
  - depends_on: TODO-4506
  - split_from: TODO-4304
  - scope: Add a deterministic validation gate that proves the PrimeStruct map
    surface is fully `.prime`/stdlib-owned and absent from production C++
    source.
  - implementation_notes:
    - Extend the validation-script model used by vector and the map adapter
      trace check to scan production C++ under `src/` and `include/` for
      PrimeStruct-map-specific traces such as `/map`,
      `/std/collections/map`, `experimental_map`, `mapCount`, `mapContains`,
      `mapTryAt`, `mapAt`, `mapInsert`, `Map<K`, map-specific diagnostics,
      and map-specific parser/semantic/lowering branch names.
    - The check must allow ordinary C++ library usage such as `std::map`,
      generic mapping variable names, source-map infrastructure, and test/docs
      files outside production `src/` and `include/`.
    - If generic collection code needs examples or fixtures, place them in
      tests/docs or stdlib-owned manifests rather than production C++ strings.
    - Start from the TODO-4473 decaying full-trace inventory in
      `scripts/check_map_surface_trace_inventory.py`, the TODO-4472
      backing-trace inventory in `scripts/check_map_backing_traces.py`, and
      the narrower TODO-4468 source-lock in
      `tests/unit/test_stdlib_map_ownership.cpp`.
    - TODO-4474 removed frontend syntax normalization for slashless map helper
      import aliases, so `src/FrontendSyntax.cpp` should stay absent from the
      map-surface trace inventory.
    - TODO-4475 removed the dead slashless map path normalizer from emitter
      collection fallback helpers, so
      `src/emitter/EmitterExprCollectionFallbackHelpers.h` should stay absent
      from the map-surface trace inventory.
    - TODO-4476 removed rooted `/map/*` receiver-query candidate treatment
      from semantic snapshot locals, so
      `src/semantics/SemanticsValidatorSnapshotLocals.cpp` should stay absent
      from the map-surface trace inventory.
    - TODO-4477 removed rooted `/map/*` specialization/overload family
      recovery from semantic call validation, so
      `src/semantics/SemanticsValidatorExpr.cpp` should stay absent from the
      map-surface trace inventory.
    - TODO-4478 removed rooted `/map/*` stdlib-helper classification from
      implicit template inference, so
      `src/semantics/TemplateMonomorphImplicitTemplateInference.h` should keep
      only its canonical map-helper trace in the map-surface trace inventory.
    - TODO-4479 removed slashless `map/` method-target prefix stripping from
      template monomorphization, leaving only the canonical map-helper traces
      later removed by TODO-4499.
    - TODO-4499 removed the remaining hard-coded canonical map helper path
      construction from template method-target resolution, so
      `src/semantics/TemplateMonomorphMethodTargets.h` should stay absent from
      the map-surface trace inventory.
    - TODO-4480 removed slashless `map/count` matching from emitter builtin
      collection inference, and TODO-4486 removed the remaining hard-coded
      canonical map-count path from that file, so
      `src/emitter/EmitterBuiltinCollectionInferenceHelpers.cpp` should stay
      absent from the map-surface trace inventory.
    - TODO-4481 removed map-specific import-alias normalization from emitter
      struct-type helpers, so `src/emitter/EmitterHelpersStructTypes.cpp`
      should stay absent from the map-surface trace inventory.
    - TODO-4482 removed canonical-constructor-gated rooted `/map`
      constructor alias inference from semantic collection call resolution, so
      `src/semantics/SemanticsValidatorInferCollectionCallResolution.cpp`
      should stay absent from the map-surface trace inventory.
    - TODO-4483 removed rooted `/map/` helper-name parsing from emitter
      helper path utilities, and TODO-4485 removed the remaining hard-coded
      canonical map helper prefix from `src/emitter/EmitterHelpers.h`, so the
      helper header should stay absent from the map-surface trace inventory.
    - TODO-4484 removed the canonical `/std/collections/map` receiver
      exclusion from semantic non-collection struct-access fallback setup, so
      `src/semantics/SemanticsValidatorExprBuiltinContextSetup.cpp` should
      stay absent from the map-surface trace inventory.
    - Current `experimental_map` traces are classified as temporary backing
      substrate: `Map`/`Map__*` type identity, layout, binding, result,
      access, and inference hooks; `Entry`/`Entry__*` variadic constructor
      support; canonical-constructor-to-backing-helper rewrites; generated
      `map__` constructor reentry; and broad backing fallback blockers in
      setup-type/later collection-expression lowering.
    - 2026-05-16: `stdlib/std/collections/map2.prime` now exists as an
      isolated replacement candidate with no imports or references to
      `map.prime`, `internal_map.prime`, `experimental_map.prime`, `/map/*`,
      `Map__*`, or `CollectionsMap*`. It deliberately uses unique helper
      names such as `map2Get` and `map2Insert` while the old C++ map hooks
      still reserve canonical `at`/`insert`/method-sugar behavior; the final
      cutover should delete the legacy hooks first, then rename `map2` and
      its helpers to the canonical map surface.
    - 2026-05-16: Primitive key equality moved out of `internal_map.prime`
      into `stdlib/std/collections/equality.prime`, so the isolated `map2`
      candidate can support string-key lookup without importing the old map
      implementation. The vector/SoA uninitialized-buffer frees now route
      through `bufferFreeUninitialized<T>` to avoid parse-sensitive explicit
      nested template calls in statement position.
    - 2026-05-16: `stdlib/std/collections/map.prime` now carries a
      stdlib-owned `MapValue` implementation with non-builtin `map*` helpers
      (`mapInsert`, `mapAt`, `mapCount`, and related ref/access variants).
      Focused native smoke coverage proves empty-map insertion, overwrite,
      lookup, and missing-key contains behavior without importing the retired
      native map implementation. Remaining work is to delete the now-unused
      production C++ map hooks instead of preserving them as compatibility
      dispatch.
    - 2026-05-16: Explicit rooted `/map/count` method spellings now reject
      before semantic pre-dispatch, return-kind inference, lowerer method
      lookup, or emitter helper parsing can borrow canonical
      `/std/collections/map/count` behavior; the map-surface trace caps for
      the touched inference/emitter files were tightened with that removal.
    - 2026-05-16: The C++ emitter collection classifier no longer treats
      rooted `/map/map` or rooted `/map/entry` spellings as builtin map
      constructor aliases; canonical and experimental backing constructor
      handling remain separate follow-up cleanup areas.
    - 2026-05-16: Canonical public helpers in
      `stdlib/std/collections/map.prime` now route through the stdlib-owned
      `MapValue` implementation, and native lowering no longer rewrites
      canonical map constructors or helper calls through rooted `/map/map` or
      bare builtin map hooks. Semantic and lowering paths now share the
      template-monomorph `MapValue__t*` identity for `map<K, V>` bindings,
      returns, and parameters. Focused native constructor binding/return/
      parameter tests pass; the remaining blocker is stale native collection
      compatibility coverage around method-helper forwarding, reference
      helpers, and `MapValue__t*` layout/ref mismatches.
    - 2026-05-16: Native layout predicates and semantic reference type
      compatibility now recognize the stdlib-owned `MapValue__t*` identity, so
      the focused native map method/helper forwarding, reference-helper,
      wrapper-returned, explicit canonical binding, and typed-return cases pass
      on `MapValue`. The broad native collections shard is still not a clean
      gate: it is dominated by stale SoA public-surface tests plus older
      map-literal/insert compatibility fixtures that still expect retired C++
      map literal behavior.
    - 2026-05-16: The native map literal/string-key compile-run fixture now
      uses the stdlib constructor-call surface for numeric/bool/string-keyed
      maps instead of retired key/value brace literal syntax. The
      string-valued native map case is compile-only because the current native
      runtime still hangs after successfully compiling that path.
    - 2026-05-16: Native templated stdlib wrapper-temporary fixtures now use
      `map<K, V>(key, value)` and `return<auto>` rather than the retired
      `mapSingle<K, V>` helper plus old bare `map<K, V>` return envelope.
    - 2026-05-16: Public `/std/collections/map/insert*` helpers no longer
      take the native generated-map-insert shortcut; only
      `internal_map/insert*Impl` remains on that compatibility path. Local
      canonical `MapValue` insert/overwrite/growth tests now run through the
      stdlib implementation in native, VM, and C++ emitter modes. Non-local
      `MapValue` field receivers now compile-reject in the shared conformance
      fixtures because supporting them still depends on the retired C++ map
      field bridge.
    - 2026-05-16: Rooted retired public constructor aliases such as
      `/std/collections/mapSingle` and `/std/collections/mapPair` are no
      longer recognized by the semantic builtin-map constructor classifier or
      lowerer setup-type direct-target classifier; only the still-public
      `/std/collections/map/map` constructor path remains on those checks.
    - 2026-05-16: `MapConstructorHelpers.h` no longer carries the dead
      fixed-arity `mapNew`/`mapSingle`/`mapPair` through `mapOct` rewrite
      table for metadata-backed canonical/experimental map constructor
      rewrites; those registry-backed rewrite shims now return no replacement
      until a real public constructor member exists again.
    - 2026-05-16: The no-op metadata-backed map constructor rewrite shims and
      their semantic/template-monomorph call sites are removed. Remaining map
      constructor aliasing is limited to the explicit `/std/collections/map/map`
      surface alias rewrite plus the still-separate experimental-map entry-args
      path.
    - 2026-05-16: Inline parameter, variadic packed-argument, and packed
      `Result.ok` map constructor normalization no longer retargets canonical
      stdlib map constructors to the retired rooted `/map/map` builtin path.
      The remaining temporary normalization points at the public
      `/std/collections/map/map` constructor while the old builtin-map
      variadic/result fixtures are retired or retargeted to `MapValue`.
    - 2026-05-16: Semantic constructor-backed map binding detection and
      parameter map-value checks no longer classify fixed-arity legacy names
      such as `mapSingle`, `mapPair`, or `mapOct` as canonical map
      constructors; only the public `map` constructor surface remains.
    - 2026-05-16: IR lowering no longer rewrites public
      `/std/collections/map/map` constructor calls into
      `/std/collections/experimental_map/mapNew`/`mapSingle`/fixed-arity
      backing helpers when assigning, initializing, or packing experimental
      map structs.
    - 2026-05-16: Semantic call resolution and template-monomorph overload
      selection no longer classify rooted `/map/entry` or generated
      `/map/entry__*` spellings as map entry constructors. Canonical
      `/std/collections/map/entry` remains covered only for the still-existing
      entry-args path while that old path is retired separately.
    - 2026-05-16: Semantic struct-return method inference no longer mirrors
      map receiver method candidates to rooted `/map/*` definitions; map
      receiver struct-return probing now checks only the canonical
      `/std/collections/map/*` method candidate.
    - 2026-05-16: Lowerer statement-call receiver inference comments no
      longer preserve a rooted `/map/at` args-pack example; source-lock
      coverage now keeps that retired rooted trace absent.
    - 2026-05-16: Template monomorphization no longer treats rooted
      `/map/count` as equivalent to canonical `/std/collections/map/count`
      when reporting template arguments on non-templated count calls; the
      stdlib map ownership source lock keeps that diagnostic shortcut absent.
    - 2026-05-16: Template expression rewriting no longer clears inferred
      canonical map receiver template arguments through a rooted `/map/*`
      branch; the canonical `/std/collections/map/*` branch remains.
    - 2026-05-16: Collection-dispatch setup no longer suppresses namespaced
      canonical map access inference when a rooted `/map/<access>` definition
      exists; explicit removed-alias rejection remains separate.
    - 2026-05-16: Method-target resolution no longer reclassifies direct
      calls as removed `/map/*` compatibility solely from resolved callee
      paths; only explicit syntactic removed-spelling probes remain there.
    - 2026-05-16: Template expression rewriting no longer probes visible
      rooted `/map/<helper>` definitions to report explicit-template
      diagnostics for bare map receiver calls.
    - 2026-05-16: Late fallback return-kind inference now targets canonical
      `/std/collections/map/<helper>` paths rather than rooted
      `/map/<helper>` paths when resolving map receiver builtin access
      spelling.
    - 2026-05-16: Emitter method resolution no longer lets implicit map
      helper methods resolve through rooted `/map/<helper>` metadata when the
      canonical stdlib map helper metadata is absent.
    - 2026-05-16: Emitter method type inference no longer falls back from
      canonical `/std/collections/map/<access>` metadata to rooted
      `/map/<access>` metadata for bare map access methods.
    - 2026-05-16: Emitter call-path preference no longer falls back from
      missing canonical `/std/collections/map/<suffix>` paths to rooted
      `/map/<suffix>` aliases.
    - 2026-05-16: Emitter return inference no longer adds or prefers rooted
      `/map/<suffix>` aliases when canonical
      `/std/collections/map/<suffix>` paths are missing.
    - 2026-05-16: Emitter collection-type inference no longer prunes rooted
      `/map/<access>` candidates from canonical map access paths after the
      reverse candidates were removed.
    - 2026-05-16: Emitter method metadata no longer prunes rooted
      `/map/<access>` candidates from canonical map access paths after the
      reverse candidates were removed.
    - 2026-05-16: Emitter return inference no longer prunes rooted
      `/map/<access>` candidates from canonical map access paths after the
      reverse candidates were removed.
    - 2026-05-16: Emitter return inference no longer rewrites missing rooted
      `/map/<suffix>` paths to canonical `/std/collections/map/<suffix>`
      paths.
    - 2026-05-16: Shared emitter call-path preference no longer rewrites
      missing rooted `/map/<suffix>` paths to canonical
      `/std/collections/map/<suffix>` paths.
    - 2026-05-16: Emitter return inference no longer adds an implicit rooted
      `/map/<method>` candidate after canonical map method candidates.
    - 2026-05-16: Emitter method metadata no longer rewrites rooted
      `/map/<helper>` method paths to canonical
      `/std/collections/map/<helper>` paths when canonical metadata exists.
    - 2026-05-16: Emitter return-struct inference no longer synthesizes
      canonical `/std/collections/map/<suffix>` candidates from rooted
      `/map/<suffix>` inputs.
    - 2026-05-16: Emitter return-struct inference no longer carries the
      unused rooted `/map/<access>` pruning lambda.
    - 2026-05-16: Emitter vector stdlib helper preference no longer
      normalizes map paths while handling array-to-vector fallback.
    - 2026-05-16: Stale semantic struct-return coverage now locks that map
      method sugar ignores rooted `/map/at` helpers and uses the canonical
      map helper return type.
    - 2026-05-16: Emitter collection-type inference no longer prunes
      canonical map access candidates from rooted `/map/<access>` inputs.
    - 2026-05-16: Shared emitter method metadata/type-inference no longer
      prunes rooted map access candidates from collection helper path lists.
    - 2026-05-16: Emitter method metadata no longer normalizes slashless
      rooted/canonical map helper paths to leading-slash candidates.
    - 2026-05-16: Emitter import-alias resolution no longer routes through
      the identity `normalizeMapImportAliasPath` helper.
    - 2026-05-16: Emitter method metadata no longer adds slashless candidates
      for rooted map helper metadata paths.
    - 2026-05-16: Emitter return inference no longer normalizes slashless
      rooted/canonical map helper paths to leading-slash candidates.
    - 2026-05-16: Emitter return inference no longer strips `map/` or
      `std/collections/map/` prefixes before explicit candidate construction.
    - 2026-05-16: IR call resolution no longer names
      `StdlibSurfaceId::CollectionsMapHelpers` directly when classifying
      published bridge helper surfaces; it now resolves the map helper
      surface through `collections.map_helpers` metadata like the vector
      helper path.
    - 2026-05-16: Map literal lowering no longer hard-codes the empty
      inferred-map backing path as `/std/collections/experimental_map/Map`;
      it now uses the shared `experimentalCollectionTypePath("map", "Map")`
      helper and the ownership test no longer classifies the mutation lowerer
      as an allowed experimental-map backing trace file.
    - 2026-05-16: Struct-type map local inference no longer hard-codes the
      canonical `MapValue` root path; it now builds that root from
      `collectionTypePath("map")` and keeps focused name-expression coverage
      for the resulting `MapValue__*` path.
    - 2026-05-16: Packed Result payload metadata no longer compares payload
      bases to a hard-coded experimental map path; it now uses
      `isExperimentalCollectionTypeName` and the ownership test no longer
      classifies the packed-result helper as an allowed experimental-map
      backing trace file.
    - 2026-05-16: Statement-binding map struct metadata no longer hard-codes
      the canonical `MapValue` root path; it now builds that root from
      `collectionTypePath("map")`.
    - 2026-05-16: Uninitialized map target inference no longer hard-codes
      the canonical `MapValue` root path; it now builds that root from
      `collectionTypePath("map")`, and stale unit coverage now targets the
      canonical `/std/collections/map/map` constructor plus stdlib-owned
      `MapValue` value metadata instead of retired `mapPair`/builtin-map
      compatibility expectations.
    - 2026-05-16: Struct-slot map type recognition and specialized
      `MapValue` path construction no longer spell the canonical map value
      root directly; both now derive it through `collectionTypePath("map")`.
    - 2026-05-16: Map access target struct-path inference no longer spells
      the canonical `MapValue` root directly; it now builds the root through
      `collectionTypePath("map")`, and focused access-target tests now assert
      the stdlib-owned `MapValue__*` identity instead of stale experimental
      `Map__*` expectations.
    - 2026-05-16: Statement binding helper map struct-path inference no
      longer spells the canonical `MapValue` root directly; it now builds the
      root through `collectionTypePath("map")`.
    - 2026-05-16: Semantics binding map type recognition no longer spells
      canonical map or `MapValue` roots directly; it now derives those strings
      through a local collection path helper.
    - 2026-05-16: Semantics struct-return helper path normalization and
      specialized `MapValue` return-path construction no longer spell
      canonical map roots directly; both now derive those strings through a
      local collection path helper.
    - 2026-05-16: Generated collection struct classification no longer
      carries a split-string canonical map `MapValue__*` prefix; it now
      derives that prefix through `collectionTypePath("map")`.
    - TODO-4487 removed the hard-coded canonical map access return-kind path
      from `src/ir_lowerer/IrLowererSetupTypeReturnKindHelpers.cpp`, so the
      file should stay absent from the map-surface trace inventory.
    - TODO-4488 removed the hard-coded canonical map access named-argument
      guard from `src/semantics/SemanticsValidatorExprNamedArgumentBuiltins.cpp`,
      so the file should stay absent from the map-surface trace inventory.
    - TODO-4489 removed the hard-coded experimental map backing type check
      from `src/semantics/SemanticsValidatorInferTargetResolution.cpp`, so the
      file should stay absent from the map-surface trace inventory.
    - TODO-4490 removed the hard-coded canonical map helper path
      concatenation from late unknown-target fallback inference, so
      `src/semantics/SemanticsValidatorExprLateUnknownTargetFallbacks.cpp`
      should stay absent from the map-surface trace inventory.
    - TODO-4491 removed the hard-coded canonical map helper prefix from
      implicit template stdlib-collection-helper classification, so
      `src/semantics/TemplateMonomorphImplicitTemplateInference.h` should stay
      absent from the map-surface trace inventory.
    - TODO-4492 removed the hard-coded map helper prefixes and canonical path
      construction from statement body-argument map helper routing, so
      `src/semantics/SemanticsValidatorStatementBodyArguments.cpp` should stay
      absent from the map-surface trace inventory.
    - TODO-4493 removed the hard-coded experimental map backing path prefix
      from statement container drop/relocation triviality, so
      `src/semantics/SemanticsValidatorStatementContainerHelpers.cpp` should
      stay absent from the map-surface trace inventory.
    - TODO-4494 removed the hard-coded experimental map backing specialization
      path prefix from semantic direct-call return binding inference, so
      `src/semantics/SemanticsValidatorBuildDirectCallBinding.cpp` should stay
      absent from the map-surface trace inventory.
    - TODO-4495 removed the duplicate canonical map type-text recognizer from
      semantic collection predicates, so
      `src/semantics/SemanticsValidatorExprCollectionPredicates.cpp` should
      stay absent from the map-surface trace inventory.
    - TODO-4496 removed the duplicate canonical map type-text recognizer from
      semantic collection-access validation and routed that classification
      through shared binding-type key/value extraction.
    - TODO-4497 removed hard-coded experimental map constructor root literals
      from template monomorph experimental collection type helpers, so only
      shared metadata enum references remain in that helper.
    - TODO-4498 removed hard-coded experimental map backing type literals from
      semantic struct-layout validation, so
      `src/semantics/SemanticsValidatorPassesStructLayouts.cpp` should stay
      absent from the map-surface trace inventory.
    - TODO-4502 removed direct map surface-id cases from semantic snapshot
      bridge-choice collection, so
      `src/semantics/SemanticsValidatorSnapshots.cpp` should stay absent from
      the map-surface trace inventory.
    - TODO-4503 removed the remaining direct map constructor surface-id enum
      trace from template experimental collection type helpers, so
      `src/semantics/TemplateMonomorphExperimentalCollectionTypeHelpers.h`
      should stay absent from the map-surface trace inventory.
    - TODO-4504 removed the direct experimental map backing path check from
      IR flow-control temporary-copy disarming, so
      `src/ir_lowerer/IrLowererFlowControlHelpers.cpp` should stay absent from
      the map-surface and map-backing trace inventories.
    - TODO-4505 removed the local experimental map backing predicate from IR
      access-load helper selection, so
      `src/ir_lowerer/IrLowererAccessLoadHelpers.cpp` should stay absent from
      the map-surface and map-backing trace inventories.
    - TODO-4506 removed the local experimental map backing predicate from IR
      access-target preservation, so
      `src/ir_lowerer/IrLowererAccessTargetResolution.cpp` should keep using
      the shared lowerer map backing predicate for preservation checks.
    - Tighten or replace the TODO-4473 and TODO-4472 allowed-count
      inventories as traces are deleted; the final TODO-4464 state is zero
      tolerance for all PrimeStruct-map-specific production C++ traces, not a
      permanent allowlist.
  - acceptance:
    - The audit runs in the release validation path and fails when a
      PrimeStruct-map-specific production C++ trace is reintroduced.
    - The audit passes with canonical map construction, imports, helper calls,
      lookup, insertion, miss-result handling, destruction, and direct `.prime`
      implementation behavior covered by existing tests.
    - Production C++ under `src/` and `include/` contains no PrimeStruct map
      paths, helper aliases, type-name recognizers, diagnostics, special
      parser/text-transform cases, semantic rewrites, lowering dispatches, or
      metadata tables.
    - The only allowed map mentions after the audit are ordinary C++ `std::map`
      or source-map/generic mapping usage, stdlib `.prime` source, tests, docs,
      and explicitly non-production generated source-lock fixtures.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the release gate mechanically enforces that map is
    fully stdlib-owned and no PrimeStruct-map-specific production C++ traces
    remain.

- [~] TODO-4305: Rename and style canonical `.prime` SoA surface
  - owner: ai
  - created_at: 2026-04-28
  - phase: SoA public surface rename and ownership cutover
  - scope: Rename the public SoA collection surface from `soa_vector<T>` to
    `soa<T>` and from `/std/collections/soa_vector/*` to
    `/std/collections/soa/*`, while keeping the public `.prime` source aligned
    with `docs/CodeExamples.md`.
  - implementation_notes:
    - Start from `stdlib/std/collections/soa_vector.prime`,
      `stdlib/std/collections/soa_vector_conversions.prime`,
      `stdlib/std/collections/internal_soa_vector.prime`,
      `stdlib/std/collections/internal_soa_vector_conversions.prime`,
      `stdlib/std/collections/experimental_soa_vector.prime`,
      `stdlib/std/collections/experimental_soa_vector_conversions.prime`,
      `docs/PrimeStruct.md`, `docs/PrimeStruct_SyntaxSpec.md`,
      `docs/CodeExamples.md`, SoA compile-run suites, ECS examples, and
      source-lock tests.
    - Decide whether conversions live directly under `/std/collections/soa/*`
      or under `/std/collections/soa_conversions/*`; document the final shape
      before changing tests.
    - Prefer CodeExamples surface style in public SoA code where the current
      language supports it: descriptive names, shallow control flow, concise
      inferred locals, method-style calls, operator syntax instead of canonical
      helper-call noise, and lowerCamelCase member helpers.
    - Canonical helper ownership should be path/module based, not encoded into
      function names: prefer `/std/collections/soa/count`,
      `/std/collections/soa/get`, `/std/collections/soa/ref`,
      `/std/collections/soa/push`, `/std/collections/soa/reserve`,
      field-view helpers, and conversion helpers over `soaVectorCount`,
      `soaVectorGet`, `soaVectorRef`, `soaVectorPush`,
      `soaVectorReserve`, or `soaVectorToAos`.
    - Treat `soa_vector<T>`, `/std/collections/soa_vector/*`, `/soa_vector/*`,
      `SoaVector<T>`, and `soaVector*` as retired public compatibility
      spellings; TODO-4520 installed the production C++ inventory, TODO-4521
      reduced semantic residue, TODO-4522 reduced lowerer/emitter residue,
      TODO-4523 reduced parser/header/registry residue, and TODO-4524 tightens
      it to zero.
  - acceptance:
    - Docs define `soa<T>` and `/std/collections/soa/*` as the target canonical
      public SoA collection surface, with retired `soa_vector` spellings
      documented as rejected public compatibility names.
    - Canonical public SoA examples and style-aligned stdlib code use `soa<T>`
      and `/std/collections/soa/<helper>` / namespace-owned helper names rather
      than `soa_vector<T>` or `soaVector*` prefixed helper names.
    - Existing `soa_vector` imports and examples either continue through
      compatibility shims or are limited to explicitly named compatibility
      tests.
    - Low-level column storage, field-view, schema, conversion, and
      compatibility mechanics are quarantined in `internal_*` or
      compatibility-shim files and are not used as the public style reference.
    - Existing SoA construction, count/get/ref, push/reserve, field-view,
      conversion, invalidation, and import behavior remains behavior-compatible
      unless deliberately updated with docs and tests.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the target public spelling is `soa<T>` /
    `/std/collections/soa/*` with `soa_vector` isolated as compatibility; leave
    generic substrate and lowering extraction to TODO-4306 and the
    TODO-4514/TODO-4516 sequence.
  - notes:
    - Split into bounded implementation leaves because the full rename spans
      public wrapper modules, type spelling support, examples/docs migration,
      compatibility shims, and later C++ surface cleanup.
    - TODO-4507 introduces the canonical `/std/collections/soa/*` wrapper
      namespace over the existing `SoaVector<T>` backing without removing
      `soa_vector` compatibility.
    - TODO-4508 added the user-facing `soa<T>` type spelling once the helper
      namespace existed.
    - TODO-4509 migrated public docs and examples to the new `soa` spelling
      now that the helper namespace and type spelling are both available.

- [~] TODO-4306: Stabilize generic SoA substrate boundaries
  - owner: ai
  - created_at: 2026-04-28
  - phase: SoA public surface rename and ownership cutover
  - depends_on: TODO-4509
  - scope: Make the remaining compiler/runtime-owned SoA behavior explicit as
    generic substrate rather than public `soa` collection semantics.
  - implementation_notes:
    - Start from `stdlib/std/collections/internal_soa_storage.prime`,
      generated `SoaSchema*` helpers, `SoaColumn<T>`, `SoaFieldView<T>`,
      field-view borrow-root invalidation tests, and the C++ helper paths that
      currently mention builtin `soa_vector` storage or field-view behavior.
    - Keep generic SoA substrate allowed in C++ where necessary:
      field-layout/codegen/introspection, column storage, field-view
      borrowing/invalidation, schema metadata, and allocation primitives.
    - Move public collection policy such as construction, count/get/ref,
      push/reserve, conversion naming, helper routing, and compatibility names
      out of compiler-owned substrate.
  - acceptance:
    - Docs separate generic SoA substrate from the public `soa<T>` collection
      surface and identify which pieces may remain compiler/runtime-owned.
    - Internal `.prime` fixtures cover `SoaColumn`, `SoaFieldView`, and
      `SoaSchema*` behavior without spelling public `soa_vector` collection
      helpers.
    - Field-view borrow-root invalidation and structural mutation boundaries
      are documented and covered for the canonical `soa<T>` surface.
    - Existing SoA behavior and diagnostics remain stable unless the task
      intentionally updates them with docs and tests.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once generic SoA substrate boundaries are documented and
    covered independently of the old `soa_vector` public surface; leave helper
    lowering extraction to TODO-4514/TODO-4516.
  - notes:
    - Split because the docs/source-lock boundary and canonical field-view
      invalidation coverage are separable.
    - TODO-4510 documented and source-locked the generic substrate/public
      collection boundary, with internal storage and reflection fixtures pinned
      as substrate coverage.
    - TODO-4511 completed the remaining canonical `soa<T>` field-view
      invalidation surface before helper lowering extraction continues.
    - TODO-4512 moved the internal AoS conversion adapter loop onto
      `/std/collections/soa/*` read helpers and split the remaining lowering
      extraction into TODO-4513/TODO-4514/TODO-4516.
    - TODO-4513 routed canonical `/std/collections/soa/to_aos`
      classification through semantic, emitter, and lowerer classifiers while
      keeping compatibility `soa_vector` conversion paths alive.

- [~] TODO-4524: Tighten SoA surface audit to zero
  - owner: ai
  - created_at: 2026-05-15
  - phase: SoA public surface rename and ownership cutover
  - split_from: TODO-4310
  - depends_on: TODO-4523
  - scope: Convert the SoA public-surface trace inventory gate into the final
    zero-production-trace audit.
  - implementation_notes:
    - Tighten the TODO-4520 inventory so production C++ under `src/` and
      `include/` contains no SoA public collection paths, helper aliases,
      type-name recognizers, diagnostics, parser/text-transform cases, semantic
      rewrites, lowering dispatches, or metadata tables.
    - Keep allowlisted generic SoA substrate terms only where they do not encode
      the public collection surface.
  - acceptance:
    - The release validation path fails when any PrimeStruct SoA public
      collection-surface production C++ trace is reintroduced.
    - The only allowed SoA mentions after the audit are generic SoA substrate
      terms, stdlib `.prime` source, tests, docs, and explicitly
      non-production generated source-lock fixtures.
    - Canonical `soa<T>` construction, imports, helper calls, field views,
      conversion, mutation, invalidation, destruction, and direct `.prime`
      implementation behavior remain covered by existing tests.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the release gate mechanically enforces that the public
    SoA collection surface is `soa<T>`, fully stdlib-owned, and has no
    PrimeStruct-SoA-public-surface production C++ traces.
  - notes:
    - Split after TODO-4525 because the remaining inventory still spans
      semantic validation, template monomorphization, emitter helpers, and IR
      lowerer routing. TODO-4529 replaced the inventory cap with a strict
      zero-production-trace audit; keep this parent for root release-gate
      reconciliation.
