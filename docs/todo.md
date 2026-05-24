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

- TODO-4562: Add task handle semantic facts and lifetime diagnostics | track:
  task-spawn-semantics | primary surface: semantics/task facts

### Parallel Work Tracks (Current)

- `soa-zero-audit`: TODO-4529 replaced the residue inventory with a strict
  zero-production-trace audit; no SoA zero-audit leaf is ready.
- `map-zero-audit`: TODO-4537 split the broad lowerer substrate item into
  bounded leaves; TODO-4539 removed generated MapValue path synthesis, and
  TODO-4540 removed the lowerer key/value local kind plus ref/pointer flags.
  TODO-4541 removed direct key/value access emission, TODO-4542 retired
  native map-value gates, TODO-4543 moved residual access-target API names
  to generic collection-pair type facts, TODO-4538 replaced the hanging broad
  inventory with a fast strict audit, and TODO-4464 enforced the final zero
  map-surface audit; no map zero-audit leaf is ready.
- `generic-helper-call-lowering`: TODO-4544 fixed statement-context
  generated `.prime` helper calls without restoring map-specific native
  insert dispatch; no generic helper-call leaf is ready.
- `tuple-type-packs`: TODO-4276 completed helper/lifecycle pack
  expansion, TODO-4271 added compile-time pack indexing, TODO-4272 added
  the initial stdlib tuple surface, and TODO-4274 added tuple bracket
  indexing, TODO-4273 added heterogeneous `make_tuple` inference, and
  TODO-4277 added tuple destructuring. TODO-4278 is blocked on missing
  task-side spawn/wait support, now tracked by TODO-4563.
- `multithreading-substrate`: TODO-4545 was split into TODO-4561,
  TODO-4562, and TODO-4563 so single-task spawn/wait can land as parser/effect,
  semantic/lifetime, and runtime execution slices before TODO-4278. TODO-4561
  locked the parser/effect spelling; TODO-4562 is now the semantic/lifetime
  successor.
- `procedural-genericity`: TODO-4336 allowed type locals in local binding and
  struct-field envelopes, TODO-4337 added non-escaping local generated
  structs, TODO-4338 stabilized deterministic generated identity and
  provenance, TODO-4339 lowered procedural facts through semantic-product
  direct-call and layout metadata, and TODO-4340 added docs and positive
  examples, and TODO-4546 added negative conformance; no procedural-genericity
  leaf is ready.
- `generic-requirements`: TODO-4331, TODO-4334, TODO-4341, TODO-4342,
  TODO-4343, TODO-4344, TODO-4352, TODO-4353, and TODO-4354 are complete;
  TODO-4355 wired the compile-time host to published `/std/meta/*` predicate
  facts; TODO-4356 prepared restricted compile-time callables; TODO-4357
  evaluates pure user predicates; TODO-4345 added statement-level concrete
  `ct_if`; TODO-4547 added generic-specialized branch selection, and
  TODO-4548 added expression-position `ct_if` values; TODO-4549 scoped
  selected-branch generated type facts; TODO-4346 documented compile-time flow
  policy; TODO-4550 enforced active compile-time budget limits; TODO-4358
  enforced phase-qualified compile-time effects; TODO-4551 added
  deterministic cache keys and invalidation; TODO-4347 routed requirement
  facts into overload selection; and TODO-4351 added integer value
  requirement facts. TODO-4348 was split into bounded diagnostic leaves,
  TODO-4552 published provenance-rich direct requirement failures,
  TODO-4553 published requirement-overload diagnostics, TODO-4554 published
  compile-time-flow diagnostics, and TODO-4555 added predicate/value
  conformance coverage. TODO-4556 added focused conformance for compile-time
  effect opt-ins, cache invalidation material, and active budget enforcement.
  TODO-4557 locked compiler-hosted CT VM boundary coverage. TODO-4558 added
  parser and source-lock conformance for public generic requirement and
  `ct_if` syntax. TODO-4560 added compile-run and user-facing diagnostic
  conformance coverage. TODO-4559 added semantic-product and IR-preparation
  handoff conformance. TODO-4359 was split into TODO-4555, TODO-4556, and
  TODO-4557 so compile-time VM conformance could be covered in parallel by
  predicate/value, effects/cache/budget, and compiler-host boundary leaves.
  TODO-4349 was split into TODO-4558, TODO-4559, and TODO-4560 so the broader
  conformance matrix could proceed in parser/source-lock, semantic/IR, and
  compile-run/diagnostic tracks; all three split leaves are complete.

### Immediate Next 10 (Track Successors; Not Ready Until Dependencies Land)

- TODO-4563: Add single-task spawn/wait runtime execution
- TODO-4278: Integrate multi-wait with stdlib tuple

### Priority Lanes (Current)

- Semantic ownership authority: none active; future semantic-authority work
  must enter as bounded leaves only.
- Deferred stdlib ADT migration: none active
- Vector stdlib ownership cutover: none active
- Map stdlib ownership cutover: none active after TODO-4464 enforced the
  strict zero audit
- Generic helper-call lowering: none active after TODO-4544
- SoA public surface rename and ownership cutover: TODO-4306 parent split;
  TODO-4526 removed semantic-validation inventory residue after TODO-4530
  reduced the shared semantic builtin path helper boundary; TODO-4527 removed
  template-monomorph residue; TODO-4533 removed lowerer call-resolution
  residual bridge traces; TODO-4528 removed lowerer count/access residue; and
  TODO-4529 replaced the residue inventory with a strict zero audit
- Deferred generic tuple substrate: TODO-4278 is blocked on task-side
  TODO-4563 after TODO-4277 added tuple destructuring sugar over ordinary
  stdlib tuple values
- Multithreading substrate: TODO-4562 -> TODO-4563 -> TODO-4278
- Procedural compile-time genericity: none active after TODO-4340 and
  TODO-4546
- Generic constraint and compile-time flow alignment: none active after
  TODO-4350

### Execution Queue (Recommended Track Order)

- TODO-4562: Add task handle semantic facts and lifetime diagnostics
- TODO-4563: Add single-task spawn/wait runtime execution
- TODO-4278: Integrate multi-wait with stdlib tuple

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | none |
| Compile-pipeline stage and publication-boundary contracts | none |
| Compile-time macro hooks and AST transform ownership | none |
| Stdlib surface-style alignment and public helper readability | TODO-4305 |
| Stdlib bridge consolidation and collection/file/gfx surface authority | none |
| Vector/map stdlib ownership cutover and collection surface authority | none |
| Stdlib de-experimentalization and public/internal namespace cleanup | none |
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
| Generic type packs and tuple stdlib surface | TODO-4274, TODO-4273, TODO-4277, TODO-4278 |
| Multithreading substrate | TODO-4562, TODO-4563 |
| Procedural compile-time genericity and local type facts | none |
| Generic constraints and compile-time flow control | TODO-4352, TODO-4353, TODO-4354, TODO-4355, TODO-4356, TODO-4357, TODO-4345, TODO-4547, TODO-4548, TODO-4549, TODO-4346, TODO-4550, TODO-4551, TODO-4552, TODO-4553, TODO-4554, TODO-4555, TODO-4556, TODO-4557, TODO-4558, TODO-4559, TODO-4560 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | none |
| AST transform hook conformance | none |
| CodeExamples-aligned stdlib surface syntax conformance | TODO-4305 |
| Compile-pipeline stage handoff conformance | none |
| Semantic-product publication parity and deterministic ordering | none |
| Lowerer/source-composition contract coverage | none |
| Vector/map bridge parity for imports, rewrites, and lowering | none |
| De-experimentalization surface and namespace parity | none |
| `soa` maturity and canonical surface parity | TODO-4305, TODO-4306, TODO-4514, TODO-4524 |
| Focused backend rerun ergonomics and suite partitioning | none |
| Architecture contract probe migration | none |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debugger/source-map provenance parity | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug stateful opcode behavior | none |
| Release benchmark/example suite stability and doctest governance | none |
| Sum-type and brace-construction conformance | none |
| Maybe/Result sum migration conformance | none |
| Generic type-pack and tuple conformance | TODO-4274, TODO-4273, TODO-4277, TODO-4278 |
| Multithreading substrate conformance | TODO-4562, TODO-4563 |
| Procedural compile-time genericity conformance | none |
| Generic constraint and compile-time flow conformance | TODO-4352, TODO-4353, TODO-4354, TODO-4355, TODO-4356, TODO-4357, TODO-4345, TODO-4547, TODO-4548, TODO-4549, TODO-4346, TODO-4550, TODO-4551, TODO-4552, TODO-4553, TODO-4554, TODO-4555, TODO-4556, TODO-4557, TODO-4558, TODO-4559, TODO-4560 |

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
  TODO-4464 completed the same ownership model for map after the internal-map
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
  uses the same helper for generated map backing recognition, and
  collection-type normalization now uses the same helper for generated map
  backing recognition, and struct-return inference now uses the same helper
  for generated map backing recognition, and Result helper map payload
  recognition now uses the same helper for generated map backing paths, and
  receiver-path map backing exclusions now use the same helper, and late
  map-access receiver classification now uses the same helper, and statement
  init map type matching now uses the same helper, and lowerer simple-call
  scoped collection alias rejection now recognizes map helper surface paths
  through `CollectionsMapHelpers` metadata instead of a direct slashless
  `map/` prefix check, and semantic method-target resolution now recognizes
  slashless map helper alias paths through
  `metadataBackedMapHelperRootAliasMethodName` instead of direct `map/`
  prefix checks, and semantic simple-call path helpers now resolve map helper
  member names through `collections.map_helpers` metadata instead of direct
  slashless and canonical map prefix stripping, and semantic concrete call
  resolution now recognizes map entry helper and constructor base paths
  through map surface metadata instead of direct canonical map
  entry/constructor literals, and lowerer count fallback now recognizes
  removed map helper alias calls through `collections.map_helpers` metadata
  instead of a local slashless `map/` helper table, and semantic collection
  dispatch setup now recognizes rooted map access compatibility paths
  through `CollectionsMapHelpers` import-alias metadata instead of a literal
  `/map/at*` table, and semantic initializer helper preference now resolves
  explicit stdlib map helper names through `collections.map_helpers` metadata
  instead of a hard-coded canonical map namespace and helper-name table, and
  semantic struct-return map helper probing now derives explicit map access
  helper names and canonical method candidates through the same metadata
  instead of direct rooted/canonical map helper strings, and semantic
  map/SOA builtin validation now resolves canonical map contains helper paths
  through `collections.map_helpers` metadata instead of direct map surface
  IDs, and statement printability now resolves canonical map access helper
  paths through the same metadata instead of direct map surface IDs, and
  expression collection-dispatch setup now resolves canonical map access
  helper paths through that metadata instead of direct map surface IDs, and
  expression method resolution now derives canonical map access helper paths
  through that metadata instead of direct map surface IDs, and try builtin
  validation now derives canonical map tryAt helper paths through that
  metadata instead of direct map surface IDs, and pointer-like collection
  method normalization now derives the unrooted canonical map helper prefix
  through that metadata instead of direct map surface IDs, and collection
  return inference now derives canonical map access helper return lookup
  paths through that metadata instead of direct path concatenation, and
  template monomorph collection compatibility now derives explicit map helper
  member names and map collection-root matching through stdlib surface
  metadata instead of direct `map/` or `std/collections/map` checks, and core
  semantic unknown-call formatting and diagnostic target normalization now
  derive map helper membership and canonical count paths through that metadata
  instead of direct map path checks, and late fallback map access diagnostics
  now derive rooted import-alias access helper recognition from stdlib map
  import-alias metadata instead of a direct `/map/at*` path table, and
  template monomorph core collection-base/import coverage plus map entry and
  constructor overload checks now derive roots and member paths through stdlib
  surface metadata instead of direct map path literals, and infer collection
  dispatch setup now derives map helper metadata through the bridge key before
  resolving rooted aliases, canonical access helpers, and namespace checks
  instead of directly naming the map helper surface ID, and expression
  argument validation now derives canonical map access helper matching through
  stdlib map helper metadata and map template-base checks through
  `isMapCollectionTypeName` instead of direct canonical map path and type
  spellings, and definition return inference now detects deferred canonical
  map access helpers through stdlib map helper metadata instead of direct
  canonical map access path literals, and lowerer map lookup helper APIs now
  use key/value lookup names instead of map-specific lookup helper names, and
  setup-inference access element-kind helpers now use array/key-value names
  instead of array/map API names, and inline/native-tail contains/tryAt helper
  predicates plus key comparison opcode selection now use key/value names
  instead of map-specific predicate and opcode-helper names.
  The
  remaining production
  lowerer/emitter
  experimental-map traces
  are source-locked as temporary internal backing substrate by
  `test_stdlib_map_ownership.cpp`, and
  all production `src`/`include` experimental-map/`Map__*` backing traces are
  capped by the decaying `scripts/check_map_backing_traces.py` release gate.
  The old broad `scripts/check_map_surface_trace_inventory.py` inventory has
  been replaced by the tracked-file
  `scripts/check_map_surface_strict_audit.py` gate, whose normal mode and
  `--enforce-zero` mode both reject production map-surface traces.
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
  IR access-target resolution now recognizes canonical map constructor and
  access-helper paths through stdlib surface metadata instead of direct
  canonical map path literals.
  Native-tail dispatch now derives canonical map helper paths, explicit
  helper classification, import coverage, and missing-helper diagnostics
  through stdlib surface metadata instead of direct canonical map path text.
  Statement-expression lowering now derives direct canonical map helper and
  constructor recognition through stdlib surface metadata instead of direct
  canonical map path literals.
  Lowerer inline-call map-kind inference now uses the shared builtin and
  experimental collection classifiers instead of local rooted/canonical map type
  string checks.
  Uninitialized-struct inference now recognizes explicit map args-pack access
  through the published map-helper surface instead of direct rooted/canonical map
  access strings.
  Struct-slot layout now recognizes builtin map type names through the shared
  collection classifier instead of direct rooted/canonical map type strings.
  Declared collection inference now recognizes map type bases and direct map
  constructors through shared collection classifiers and constructor surface
  metadata instead of direct map type and constructor path strings.
  Binding type normalization now recognizes map types through shared collection
  classifiers instead of direct rooted/canonical map type strings.
  Inference base-kind and dispatch setup now recognize map family type text
  through shared collection classifiers instead of direct rooted/canonical map
  strings.
  Setup-type method target resolution now recognizes map receiver types through
  shared collection classifiers instead of direct rooted/canonical map strings.
  Setup-type method-call canonical map constructor direct-target checks now
  construct the canonical path through collection path helpers instead of a
  direct map path literal.
  Setup-type method-call map helper prefix stripping now derives rooted and
  canonical map prefixes through collection path helpers instead of direct map
  path strings.
  Setup-type method-call canonical map helper lookup now builds the helper path
  through collection path helpers instead of direct map helper path
  concatenation.
  Setup-type method-call synthetic fallback blocking and explicit vector-count
  map-target checks now derive map paths through collection path helpers instead
  of direct map path strings.
  Inference base-kind map `tryAt`/`contains` call-name checks now derive
  slashless canonical helper paths through collection path helpers instead of
  direct map helper path strings.
  Lowerer struct-return map helper candidate paths now derive rooted and
  canonical prefixes through collection path helpers instead of direct map
  helper path strings.
  Emitter method metadata removed-alias detection now resolves canonical map
  helper paths through stdlib surface metadata instead of a direct slashless
  canonical map path string.
  Emitter method metadata removed-alias detection now resolves both map import
  aliases and canonical map helper paths through the published stdlib surface
  member resolver instead of direct map helper prefix checks.
  Emitter method metadata receiver normalization now relies on the shared map
  type classifier instead of a direct rooted `/map` type check.
  Emitter setup return-inference map method candidate construction now derives
  explicit import-alias and canonical helper paths through published stdlib
  surface metadata instead of direct map helper path strings.
  Emitter method resolution now resolves explicit map helper names and helper
  paths through published stdlib surface metadata instead of direct map helper
  prefix and path strings.
  Emitter method type inference now classifies canonical and import-alias map
  access helper paths through published stdlib surface metadata instead of
  direct map helper prefix checks.
  Emitter call-path helper classification now derives map helper and
  constructor path ownership through published stdlib surface metadata instead
  of direct map helper prefix strings.
  Emitter collection-type canonical map access detection now resolves helper
  ownership through `collections.map_helpers` metadata instead of parsing the
  canonical map helper prefix.
  Lowerer statement-binding explicit map helper canonicalization now detects
  raw published map helper paths through `CollectionsMapHelpers` metadata
  instead of direct rooted/canonical map path prefix checks.
  Lowerer emit-expression explicit map helper rewriting now detects raw alias
  and canonical map helper paths through `CollectionsMapHelpers` metadata
  instead of direct rooted/canonical map helper prefix checks.
  Lowerer builtin array-access classification now rejects published map
  helper surface paths through `CollectionsMapHelpers` metadata instead of
  direct `map/` and `std/collections/map/` prefix checks.
  Inline native dispatch now classifies removed explicit map access helper
  paths through `CollectionsMapHelpers` metadata instead of a literal
  `map/at*` raw-path table.
  Emitter binding-type map compatibility checks now derive canonical and
  experimental map type paths through local collection path helpers instead of
  direct map path fragments.
  Setup-type collection helper map alias and normalization checks now derive
  rooted and canonical map prefixes through collection path helpers instead of
  direct map helper path strings.
  Setup-type method-target map method prefix stripping now derives rooted and
  canonical map prefixes through collection path helpers instead of direct map
  path strings.
  Count/access map access helper lookup, explicit std-map spelling checks,
  and rewritten parser-shaped map access receivers now derive canonical map
  helper paths and prefixes through collection path helpers instead of direct
  map path strings.
  Semantic vector helper method-target normalization now derives published map
  helper member names through stdlib surface metadata instead of direct
  `map/` and `std/collections/map/` prefix stripping.
  Infer-method method-name normalization now derives explicit map helper names
  through stdlib surface metadata instead of direct `map/` and
  `std/collections/map/` prefix stripping.
  Semantic-product collection specialization publication now derives map
  type-root recognition and helper/constructor surface ids through stdlib
  surface metadata instead of direct map paths and map surface enum constants.
  Result metadata direct map constructor recognition now derives rooted and
  unrooted canonical map constructor paths through collection path helpers
  instead of direct map path strings.
  Inline-call context generated canonical map constructor helper recognition
  now derives the map constructor prefix through collection path helpers
  instead of a direct map path string.
  Statement binding explicit map helper canonicalization now derives the
  canonical map helper root through a local collection path helper instead of
  a direct canonical map path string.
  Lower emit expression collection helper explicit map helper canonicalization
  now derives the canonical map helper root through the local collection path
  helper instead of a direct canonical map path string.
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

- [ ] TODO-4562: Add task handle semantic facts and lifetime diagnostics
  - owner: ai
  - created_at: 2026-05-24
  - phase: Multithreading substrate
  - depends_on: TODO-4561
  - split_from: TODO-4545
  - scope: Add semantic `Task<T>` facts, binding inference, task-effect
    requirements, and first-slice lifetime diagnostics for single-task
    spawn/wait.
  - implementation_notes:
    - Start from semantic validation, effect checks, binding inference,
      `docs/MultithreadingPrototype.md`, and the parser/effect locks from
      TODO-4561.
    - Keep runtime/native execution out of this slice except for minimal
      fixtures needed to prove semantic publication boundaries.
    - Preserve the first-slice limits: no multi-wait, detached tasks, task
      groups, channels, scheduler controls, or mutable/reference captures.
  - acceptance:
    - `[spawn] f(...)` publishes a structured `Task<T>` handle fact when `f`
      returns `T`, and `wait(task)` consumes that handle and returns `T`.
    - Functions using `[spawn]` or `wait` require the documented task effect.
    - Diagnostics reject returning with live tasks, double waiting the same
      task, escaping task handles, and unsupported mutable/reference captures.
    - Focused semantic/product tests cover successful single-task facts and
      the required diagnostics.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once single-task spawn/wait semantic facts and lifetime
    diagnostics are covered without implementing runtime execution.

- [ ] TODO-4563: Add single-task spawn/wait runtime execution
  - owner: ai
  - created_at: 2026-05-24
  - phase: Multithreading substrate
  - depends_on: TODO-4562
  - split_from: TODO-4545
  - scope: Implement native/runtime execution for one structured task handle
    so TODO-4278 can build multi-wait on real task handles.
  - implementation_notes:
    - Start from native/runtime execution boundaries and the semantic facts
      from TODO-4562.
    - Keep execution single-task only; do not add multi-wait, detached tasks,
      task groups, channels, or scheduler controls here.
    - Update `docs/MultithreadingPrototype.md` and `docs/PrimeStruct.md` once
      the first task surface actually runs.
  - acceptance:
    - Focused native/runtime tests cover successful `[spawn] f(...)` followed
      by `wait(task)` for a function returning `T`.
    - Runtime behavior respects the structured lifetime and first-slice
      capture restrictions established by TODO-4562.
    - `docs/MultithreadingPrototype.md` and `docs/PrimeStruct.md` describe the
      implemented first task surface and leave multi-wait to TODO-4278.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once single-task spawn/wait is real enough for TODO-4278
    to wire multi-wait to stdlib `tuple<...>`; leave multi-wait itself to
    TODO-4278.

- [ ] TODO-4278: Integrate multi-wait with stdlib tuple
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4277, TODO-4563
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
