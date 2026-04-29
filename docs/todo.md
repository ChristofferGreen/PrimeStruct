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

- TODO-4298: Promote graph-backed non-template inference authority
- TODO-4266: Rewire `?` to the `Result` sum contract

### Immediate Next 10 (After Ready Now)

- TODO-4267: Retire legacy Maybe/Result representations
- TODO-4291: Decide sum-backed mutable `Maybe<T>` helpers
- TODO-4292: Promote and style canonical `.prime` vector implementation
- TODO-4293: Stabilize generic contiguous-storage substrate
- TODO-4294: Lower vector helpers through ordinary `.prime`
- TODO-4281: Lift vector dynamic capacity limit
- TODO-4295: Move collection surface metadata out of C++
- TODO-4296: Delete vector compatibility seams
- TODO-4297: Add zero C++ vector-surface audit
- TODO-4299: Promote and style canonical `.prime` map implementation

### Priority Lanes (Current)

- Semantic ownership authority: TODO-4298
- Deferred stdlib ADT migration: TODO-4266 -> TODO-4267
  -> TODO-4291
- Vector stdlib ownership cutover: TODO-4292 -> TODO-4293 -> TODO-4294
  -> TODO-4281 -> TODO-4295 -> TODO-4296 -> TODO-4297
- Map stdlib ownership cutover: TODO-4299 -> TODO-4300 -> TODO-4301
  -> TODO-4302 -> TODO-4303 -> TODO-4304
- SoA public surface rename and ownership cutover: TODO-4305 -> TODO-4306
  -> TODO-4307 -> TODO-4308 -> TODO-4309 -> TODO-4310
- Deferred generic tuple substrate: TODO-4268 -> TODO-4269 -> TODO-4270
  -> TODO-4275 -> TODO-4276 -> TODO-4271 -> TODO-4272 -> TODO-4274
  -> TODO-4273 -> TODO-4277 -> TODO-4278

### Execution Queue (Recommended)

- TODO-4298: Promote graph-backed non-template inference authority
- TODO-4266: Rewire `?` to the `Result` sum contract
- TODO-4267: Retire legacy Maybe/Result representations
- TODO-4291: Decide sum-backed mutable `Maybe<T>` helpers
- TODO-4292: Promote and style canonical `.prime` vector implementation
- TODO-4293: Stabilize generic contiguous-storage substrate
- TODO-4294: Lower vector helpers through ordinary `.prime`
- TODO-4281: Lift vector dynamic capacity limit
- TODO-4295: Move collection surface metadata out of C++
- TODO-4296: Delete vector compatibility seams
- TODO-4297: Add zero C++ vector-surface audit
- TODO-4299: Promote and style canonical `.prime` map implementation
- TODO-4300: Stabilize map lookup and insertion substrate
- TODO-4301: Lower map helpers through ordinary `.prime`
- TODO-4302: Move map surface metadata out of C++
- TODO-4303: Delete map compatibility seams
- TODO-4304: Add zero C++ map-surface audit
- TODO-4305: Rename and style canonical `.prime` SoA surface
- TODO-4306: Stabilize generic SoA substrate boundaries
- TODO-4307: Lower SoA helpers through ordinary `.prime`
- TODO-4308: Move SoA surface metadata out of C++
- TODO-4309: Delete `soa_vector` compatibility seams
- TODO-4310: Add zero C++ SoA collection-surface audit
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
| Semantic ownership boundary and graph/local-auto authority | TODO-4298 |
| Compile-pipeline stage and publication-boundary contracts | none |
| Compile-time macro hooks and AST transform ownership | none |
| Stdlib surface-style alignment and public helper readability | TODO-4292, TODO-4299, TODO-4305 |
| Stdlib bridge consolidation and collection/file/gfx surface authority | TODO-4295, TODO-4296, TODO-4297, TODO-4302, TODO-4303, TODO-4304, TODO-4308, TODO-4309, TODO-4310 |
| Vector/map stdlib ownership cutover and collection surface authority | TODO-4292, TODO-4293, TODO-4294, TODO-4281, TODO-4295, TODO-4296, TODO-4297, TODO-4299, TODO-4300, TODO-4301, TODO-4302, TODO-4303, TODO-4304 |
| Stdlib de-experimentalization and public/internal namespace cleanup | TODO-4292, TODO-4296, TODO-4297, TODO-4299, TODO-4303, TODO-4304, TODO-4305, TODO-4309, TODO-4310 |
| SoA maturity and `soa` public-surface rename | TODO-4305, TODO-4306, TODO-4307, TODO-4308, TODO-4309, TODO-4310 |
| Validator entrypoint and benchmark-plumbing split | none |
| Semantic-product publication by module and fact family | TODO-4298 |
| Semantic-product public API factoring and versioning | none |
| IR lowerer compile-unit breakup | none |
| Backend validation/build ergonomics | none |
| Emitter/semantics map-helper parity | TODO-4299, TODO-4300, TODO-4301, TODO-4302, TODO-4303, TODO-4304 |
| VM debug-session argv ownership | none |
| Debugger/source-map provenance parity | none |
| Debug trace replay robustness | none |
| VM/runtime debug stateful opcode parity | none |
| Test-suite audit follow-up and release-gate stability | none |
| Algebraic sum types and brace-only construction | none |
| Stdlib ADT migration for `Maybe` and `Result` | TODO-4266, TODO-4267, TODO-4291 |
| Generic type packs and tuple stdlib surface | TODO-4268, TODO-4269, TODO-4270, TODO-4275, TODO-4276, TODO-4271, TODO-4272, TODO-4274, TODO-4273, TODO-4277, TODO-4278 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | TODO-4298 |
| AST transform hook conformance | none |
| CodeExamples-aligned stdlib surface syntax conformance | TODO-4292, TODO-4299, TODO-4305 |
| Compile-pipeline stage handoff conformance | none |
| Semantic-product publication parity and deterministic ordering | none |
| Lowerer/source-composition contract coverage | TODO-4298 |
| Vector/map bridge parity for imports, rewrites, and lowering | TODO-4292, TODO-4294, TODO-4281, TODO-4295, TODO-4296, TODO-4297, TODO-4299, TODO-4301, TODO-4302, TODO-4303, TODO-4304 |
| De-experimentalization surface and namespace parity | TODO-4292, TODO-4296, TODO-4297, TODO-4299, TODO-4303, TODO-4304, TODO-4305, TODO-4309, TODO-4310 |
| `soa` maturity and canonical surface parity | TODO-4305, TODO-4306, TODO-4307, TODO-4308, TODO-4309, TODO-4310 |
| Focused backend rerun ergonomics and suite partitioning | none |
| Architecture contract probe migration | none |
| Emitter map-helper canonicalization parity | TODO-4299, TODO-4301, TODO-4302, TODO-4303, TODO-4304 |
| VM debug-session argv lifetime coverage | none |
| Debugger/source-map provenance parity | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug stateful opcode behavior | none |
| Release benchmark/example suite stability and doctest governance | none |
| Sum-type and brace-construction conformance | none |
| Maybe/Result sum migration conformance | TODO-4266, TODO-4267, TODO-4291 |
| Generic type-pack and tuple conformance | TODO-4268, TODO-4269, TODO-4270, TODO-4275, TODO-4276, TODO-4271, TODO-4272, TODO-4274, TODO-4273, TODO-4277, TODO-4278 |

### Vector/Map Bridge Contract Summary

- Bridge-owned public contract: exact and wildcard `/std/collections` imports,
  `vector<T>` / `map<K, V>` constructor and literal-rewrite surfaces, helper
  families, compatibility spellings plus removed-helper diagnostics, semantic
  surface IDs, and lowerer dispatch metadata.
- Migration-only seams: rooted `/vector/*` and `/map/*` spellings,
  `vectorCount` / `mapCount`-style lowering names, and
  `/std/collections/experimental_*` implementation modules stay temporary.
  The vector/map adapter cutover is complete for semantic and
  template-monomorph helper decisions. TODO-4292 through TODO-4297 split the
  vector half of that remaining seam into canonical implementation promotion,
  generic storage/lifecycle substrate, ordinary `.prime` lowering, metadata
  extraction, compatibility deletion, and a final zero-C++-vector audit.
  TODO-4299 through TODO-4304 apply the same ownership model to map while
  keeping map-specific lookup, insertion, `Result<ContainerError>`, and key
  comparability policy explicit.
- Compatibility adapter inventory: map insert helper compatibility is migrated
  through `StdlibSurfaceRegistry::CollectionsMapHelpers` for canonical
  `/std/collections/map/insert(_ref)`, compatibility `/map/insert(_ref)`,
  wrapper `/std/collections/mapInsert(_Ref)`, and experimental
  `/std/collections/experimental_map/mapInsert(_Ref)` spellings. Template
  monomorphization now asks the registry for preferred experimental vector/map/SoA
  helper spellings instead of carrying bespoke canonical-to-experimental maps.
  SoA helper compatibility is routed through
  `StdlibSurfaceRegistry::CollectionsSoaVectorHelpers` for canonical
  `/std/collections/soa_vector/*`, same-path `/soa_vector/*`, mixed
  `/std/collections/{count,get,ref,reserve,push}`, experimental helper wrapper,
  and conversion-helper spellings. Vector/map and SoA constructor compatibility
  is already metadata-backed by the constructor surface adapters. Gfx Buffer
  helper compatibility is routed through
  `StdlibSurfaceRegistry::GfxBufferHelpers` for canonical `/std/gfx/Buffer/*`,
  legacy `/std/gfx/experimental/Buffer/*`, and rooted `/Buffer/*` helper
  spellings before semantic gfx-buffer rewrites and GPU wrapper diagnostics
  choose canonical builtin targets. Remaining removed-helper diagnostics,
  import spellings, wildcard expansion, user-defined helper precedence,
  field-view field-name lowering, gfx constructor sugar, and lowerer raw-path
  dispatch checks are syntax/provenance-owned or lowering-owned.
- Outside this lane: `array<T>` core ownership and the `soa<T>` public-surface
  rename remain separate boundaries tracked by TODO-4305 through TODO-4310. Generic
  contiguous-storage work that is required to make vector ordinary `.prime`
  code is tracked explicitly in TODO-4293 instead of being folded into
  TODO-4281. Map-specific lookup/insertion substrate work is tracked in
  TODO-4300 instead of being folded into vector storage work.
- End-state rule for vector: after TODO-4297, production C++ under `src/` and
  `include/` must not contain PrimeStruct-vector-specific paths, helper names,
  type names, diagnostics, parser/lowering branches, or metadata tables.
  `std::vector` as the C++ standard-library container remains allowed; tests,
  docs, generated source-lock fixtures, and stdlib `.prime` files may still
  mention the PrimeStruct vector surface.
- End-state rule for map: after TODO-4304, production C++ under `src/` and
  `include/` must not contain PrimeStruct-map-specific paths, helper names,
  type names, diagnostics, parser/lowering branches, or metadata tables.
  Ordinary C++ words or containers such as `std::map`, generic mapping
  variables, and source-map infrastructure remain allowed; tests, docs,
  generated source-lock fixtures, and stdlib `.prime` files may still mention
  the PrimeStruct map surface.
- End-state rule for SoA: after TODO-4310, the public collection name is
  `soa<T>` under `/std/collections/soa/*`. The old `soa_vector<T>`,
  `/std/collections/soa_vector/*`, `/soa_vector/*`, `SoaVector<T>`, and
  `soaVector*` names are compatibility-only until TODO-4309 deletes or
  intentionally rejects them. Production C++ under `src/` and `include/` must
  not contain PrimeStruct-SoA-collection surface paths, helper names, type
  names, diagnostics, parser/lowering branches, or metadata tables after the
  TODO-4310 audit. Generic SoA substrate terms remain allowed where they do not
  encode the public collection surface: field-layout/codegen/introspection,
  `SoaColumn`, `SoaFieldView`, `SoaSchema*`, field-view borrowing/invalidation,
  and allocation primitives.

### Stdlib De-Experimentalization Policy Summary

- Canonical public API: non-`experimental` namespaces are the intended
  long-term user-facing contracts.
- Canonical collection contract: `/std/collections/vector/*` and
  `/std/collections/map/*` are the sole public vector/map collection surfaces.
- Accepted compatibility namespaces:
  `/std/collections/experimental_soa_vector/*` remains importable only for
  targeted compatibility and conformance coverage. Ordinary public examples
  should use `/std/collections/soa_vector/*`.
  `/std/collections/experimental_soa_vector_conversions/*` remains a bridge
  for direct experimental imports. Canonical conversion helpers now route
  through `/std/collections/internal_soa_vector_conversions/*`. These direct
  experimental imports are retained compatibility shims for targeted tests
  only, not ordinary public imports.
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
- Current compatibility spellings are `/std/collections/soa_vector/*` and
  `/std/collections/soa_vector_conversions/*`; ordinary new public examples
  should move to the `soa` spelling once TODO-4305 lands.
- Accepted compatibility seams: `/std/collections/experimental_soa_vector/*`
  and `/std/collections/experimental_soa_vector_conversions/*` remain importable
  only for targeted compatibility and conformance coverage; C++/VM/native
  direct-import tests lock that compatibility contract.
- Internal substrate namespaces: `/std/collections/internal_soa_vector/*`
  owns canonical wrapper implementation forwarding,
  `/std/collections/internal_soa_vector_conversions/*` owns canonical
  conversion implementation forwarding, while
  `/std/collections/internal_soa_storage/*` remains implementation-facing SoA
  storage/layout plumbing. The internal wrapper adapter still preserves the
  compatibility `SoaVector<T>` type identity behind the promoted public wrapper
  surface. The inline-parameter and direct lowerer wrapper-dispatch bridges no
  longer use rooted `/soa_vector/*`, `/to_aos`, or `/to_aos_ref` spellings as
  hidden raw fallbacks.
- Promoted contract complete: the canonical public helper wrapper is
  authoritative for ordinary construction/read/ref/mutator/conversion helper
  names, bound field-view borrow-root invalidation, and canonical-only
  C++/VM/native helper, field-view, and conversion parity coverage. Conversion
  receiver contracts are spelled through canonical `SoaVector<T>` surfaces, the
  checked-in ECS example uses canonical wrapper/conversion imports, and ordinary
  public code no longer needs `experimental_soa_vector` or
  `experimental_soa_vector_conversions` imports. The canonical wrapper routes through
  `/std/collections/internal_soa_vector/*` and canonical conversions route
  through `/std/collections/internal_soa_vector_conversions/*` instead of
  directly importing experimental implementation modules.

### Skipped Doctest Debt Summary

- Retained `doctest::skip(true)` coverage is currently absent from the active
  queue because no skipped doctest cases remain under `tests/unit`.
- New skipped doctest coverage must create a new explicit TODO before it lands.
- The success condition for each lane is re-enable-or-delete; indefinite
  skipped coverage is not a stable end state.

### Task Blocks

- [ ] TODO-4298: Promote graph-backed non-template inference authority
  - owner: ai
  - created_at: 2026-04-28
  - phase: Semantic ownership authority
  - scope: Migrate the next remaining non-template inference island among
    local-auto, query, `try(...)`, or `on_error` onto graph-owned semantic
    facts, then delete or quarantine the old validator-local/recomputed branch
    for that surface. The lowerer-side `Result.ok(query())` payload and
    `try(Result.ok(query()))` base-kind fallback slice is complete; continue
    with another adjacent island rather than reopening that fallback. The
    lowerer-side `try(...)` dispatch fallback slice is also complete; continue
    with local-auto, `on_error`, or another uncovered query/control-flow
    consumer. The lowerer-side unresolved Result-combinator metadata slice is
    complete for `Result.map`, `Result.and_then`, and `Result.map2`. The
    lowerer-side args-pack parameter metadata slice and binding metadata
    diagnostic slice are complete. The direct `Result.ok(name)` payload
    metadata slice and statement initializer binding-type slice are also
    complete. The statement name-initializer final LocalInfo metadata slice is
    complete for scalar, pointer/reference, and array/vector fallbacks. The
    local-auto stale binding-type and binding metadata diagnostic slices are
    complete. The local-auto initializer path and return-kind diagnostic
    slices are complete. The
    `on_error` stale fact and
    result-metadata diagnostic slices are complete; the query stale-target
    diagnostic slice is complete. The try on-error context diagnostic slice
    and try context-return diagnostic slice are complete. The try
    result-metadata and operand-metadata stale diagnostic slices are complete.
    The return-fact stale diagnostic and return metadata diagnostic slices are
    complete. Continue with a different graph-backed consumer. The query
    result-metadata and type-metadata stale diagnostic slices are complete. The
    collection specialization, direct-call, method-call, and bridge-path metadata
    diagnostic slices are complete. The native pick target sum-resolution slice
    is complete for named `pick(value)` targets.
  - implementation_notes:
    - Start from the semantic ownership boundary and graph migration plan in
      `docs/PrimeStruct.md`, especially the sections that call for
      graph-backed inference facts to replace ad hoc ordering before template
      inference and tuple/type-pack work expand the semantic surface.
    - Inspect `src/semantics/SemanticsValidatorPassesDefinitions.cpp` for
      validator-local inference structures, worker publication merging, and
      post-worker fact sorting; use that inventory to choose one concrete
      surface with a small but real consumer path.
    - Candidate surfaces include local-auto facts, query facts, `try(...)`
      facts, and `on_error` facts. Pick one, document why it is the first
      slice, and keep broader template inference, tuple, and vector work out of
      scope.
    - Completed slice: semantic-product-addressed direct `Result.ok(query())`
      payloads and base-kind `try(Result.ok(query()))` inference now consume
      published binding/query facts and leave the value kind unresolved when
      the fact is absent, rather than consulting recursive fallback inference.
    - Completed slice: semantic-product-addressed `try(...)` dispatch
      inference now consumes published try facts before local-result,
      callable-result, map-helper, or file-helper fallback can infer the value
      kind; missing or incomplete try facts stay unresolved with the existing
      semantic-product try diagnostic.
    - Completed slice: semantic-product-addressed Result-combinator metadata
      now requires published query facts when direct lambda payload analysis
      cannot infer the resulting value kind, covering `Result.map`,
      `Result.and_then`, and `Result.map2`.
    - Completed slice: semantic-product-addressed args-pack parameter element
      metadata now requires the published binding fact when syntax does not
      carry the element type, instead of leaving variadic element metadata
      unknown for later lowering.
    - Completed slice: semantic-product binding completeness validation now
      rejects missing or stale interned binding type and reference-root
      metadata before lowerer consumers can read inconsistent binding fact
      text fields.
    - Completed slice: semantic-product-addressed direct `Result.ok(name)`
      payload metadata now requires the published binding fact instead of
      asking local maps or recursive expression-kind fallback to reconstruct
      the payload type.
    - Completed slice: semantic-product-addressed statement bindings
      initialized from names now prefer published initializer binding facts
      before local-map metadata can decide collection shape.
    - Completed slice: semantic-product-addressed statement bindings
      initialized from names now preserve published initializer binding facts
      through the final scalar, pointer/reference, and array/vector LocalInfo
      fallback branches instead of letting stale local-map metadata overwrite
      the graph-owned fact.
    - Completed slice: semantic-product local-auto completeness validation now
      rejects stale local-auto binding type facts that contradict the
      published binding fact before lowering can use the stale graph entry.
    - Completed slice: semantic-product local-auto completeness validation now
      rejects missing or stale interned local-auto binding type metadata before
      lowering can synthesize a binding transform from inconsistent local-auto
      fact text fields.
    - Completed slice: semantic-product `on_error` completeness validation now
      rejects stale on-error facts whose handler path, error type, or bound
      arg count contradicts the published callable summary.
    - Completed slice: semantic-product `on_error` completeness validation now
      rejects stale interned return Result metadata that contradicts the
      published callable summary before lowering setup can install the
      inconsistent handler fact.
    - Completed slice: semantic-product query completeness validation now
      rejects stale query facts whose resolved target path contradicts the
      published direct or method call target for the same semantic node.
    - Completed slice: semantic-product local-auto completeness validation
      now rejects missing or stale initializer direct-call/method-call path
      facts that contradict the published initializer path before lowering can
      consume the inconsistent local-auto metadata.
    - Completed slice: semantic-product local-auto completeness validation
      now rejects missing or stale interned initializer direct-call/method-call
      return-kind metadata that contradicts the published callable summary
      before lowering can consume the inconsistent local-auto metadata.
    - Completed slice: semantic-product try completeness validation now
      rejects missing or stale `try(...)` on-error handler metadata that
      contradicts the published callable summary before lowering can consume
      the inconsistent try fact.
    - Completed slice: semantic-product try completeness validation now
      rejects stale interned `try(...)` context return-kind metadata that
      contradicts the published callable summary before lowering can consume
      the inconsistent try fact.
    - Completed slice: semantic-product try completeness validation now
      rejects stale interned `try(...)` value/error metadata that contradicts
      the published callable summary for the resolved operand before lowering
      can consume the inconsistent try fact.
    - Completed slice: semantic-product try completeness validation now
      rejects missing or stale interned operand binding type, receiver binding
      type, and query type metadata before lowerer consumers can read
      inconsistent try fact text fields.
    - Completed slice: semantic-product return completeness validation now
      rejects stale interned return-kind metadata that contradicts the
      published callable summary before lowering can consume the inconsistent
      return fact.
    - Completed slice: semantic-product return completeness validation now
      rejects missing or stale interned return binding type, struct path, and
      reference-root metadata before return inference can read inconsistent
      return fact text fields.
    - Completed slice: semantic-product query completeness validation now
      rejects stale interned Result payload/error metadata that contradicts
      the published callable summary for the resolved target before lowering
      can consume the inconsistent query fact.
    - Completed slice: semantic-product query completeness validation now
      rejects missing or stale interned query type, binding type, and receiver
      binding type metadata before lowerer consumers can read inconsistent
      query fact text fields.
    - Completed slice: semantic-product collection specialization
      completeness validation now rejects missing or stale interned collection
      family, binding type, element type, and key/value type metadata before
      lowerer collection consumers can read inconsistent collection fact text
      fields.
    - Completed slice: semantic-product direct-call completeness validation
      now rejects missing or stale interned scope, call name, resolved target,
      and published lookup metadata before lowerer routing consumers can
      dispatch through inconsistent direct-call target facts.
    - Completed slice: semantic-product method-call completeness validation
      now rejects missing or stale interned scope, method name, receiver type,
      resolved target, and published lookup metadata before lowerer routing
      consumers can dispatch through inconsistent method-call target facts.
    - Completed slice: semantic-product bridge-path completeness validation
      now rejects missing or stale interned scope, collection family, chosen
      path, and published lookup metadata before lowerer routing consumers can
      dispatch through inconsistent bridge-path choice facts.
    - Completed slice: native `pick(value)` lowering now uses the published
      binding fact plus published sum metadata to resolve named pick targets,
      instead of letting local-map shape reconstruction own that decision on
      the semantic-product path. Missing/contradictory graph-owned facts fail
      closed with deterministic pick-target diagnostics; the old local-map
      reconstruction remains only for syntax-only compatibility.
    - Add semantic-product and lowerer contract coverage proving consumers read
      the published graph-owned fact instead of reconstructing equivalent state
      from AST or validator-local caches.
  - acceptance:
    - One chosen non-template inference surface is inventoried, and docs record
      the old ownership path, the graph-backed fact that replaces it, and the
      remaining adjacent islands.
    - Semantic validation publishes the chosen fact through a graph-owned or
      graph-authoritative path in deterministic order.
    - Semantic-product publication exposes that fact through a narrow API that
      downstream consumers can use without private validator state.
    - At least one lowerer or backend-adapter consumer is changed to consume the
      published fact and no longer reconstructs the same information from raw
      AST, import aliases, or validator-local caches.
    - The retired branch is deleted when possible, or quarantined behind an
      explicitly named compatibility/test-only seam with a follow-up note.
    - Tests cover the successful graph-backed path, a stale/contradictory fact
      diagnostic, deterministic publication order, and the lowerer/adapter
      consumer contract.
    - No broader type-pack, tuple, template-inference, or collection feature
      work is bundled into this task.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once exactly one non-template inference island has
    graph-backed semantic-product authority, one downstream consumer depends on
    it, and the old duplicate ownership path is removed or quarantined.

- [ ] TODO-4266: Rewire `?` to the `Result` sum contract
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred stdlib ADT migration
  - scope: Make postfix `?`, `try(...)`, and `on_error` propagation consume the
    stdlib-owned `Result` sum shape instead of bespoke Result metadata or
    legacy runtime representation.
  - implementation_notes:
    - Start from result/try/on_error semantic validation, result metadata in
      `include/primec/SemanticProduct.h`, and IR lowerer result helpers.
    - Semantic validation, semantic-product `try(...)`/query facts, and IR
      Result type parsing now recognize both `Result<T, E>` and the qualified
      `/std/result/Result<T, E>` spelling for value-carrying results. The
      remaining work is the actual sum-backed propagation/lowering contract.
    - IR-backed `try(...)` now consumes local imported stdlib value-result sums
      for `return<int> on_error<...>` status-code flows by branching on the
      `ok`/`error` tag and extracting int-backed error payloads. Result-return
      functions now propagate local imported stdlib value-result sum errors by
      copying the `error` payload into the declared return `Result` sum. Direct
      calls that return imported stdlib value-result sums can now be consumed
      by postfix `?` on the same VM/native sum-backed paths. Legacy
      `Result.map`, `Result.and_then`, and `Result.map2` construction for typed
      imported stdlib Result sums now accepts local sources and direct calls
      that return imported stdlib Result sums. Dereferenced local
      `Reference<Result<T, E>>` and `Pointer<Result<T, E>>` values now feed
      `try(...)`, `Result.error(...)`, and `Result.why(...)` when they point at
      imported stdlib Result sums. `/std/result/*` now exposes the
      status-only `Result<E>` sum beside `Result<T, E>` at the same public path,
      with a unit `ok` variant, an `error(E)` payload variant, default `ok`
      construction, `ok<E>()`, and semantic `pick` coverage. IR-backed
      `try(...)` now consumes local, direct-call, and dereferenced local
      borrowed/pointer imported status-only `Result<E>` sums for status-code
      returns and Result-return error propagation. IR-backed
      `Result.error(...)` / `Result.why(...)` now inspect those same imported
      status-only operand families. Remaining work covers non-VM/native bridge
      cleanup. SyntaxSpec now documents the landed IR-backed status-only
      `try(...)`, postfix `?`, `Result.error(...)`, and `Result.why(...)`
      operand families and leaves only the broader/non-IR bridge cleanup as
      migration work. The legacy source C++ emitter now preserves nested
      `Result<T...>` types under `Reference` / `Pointer` and recognizes
      dereferenced local/indexed borrowed Result operands for `try(...)`,
      `Result.error(...)`, and `Result.why(...)` while it still uses a
      compatibility bridge. Its source C++ Result storage-width decisions and
      construction/accessor expression emission are quarantined behind named
      emitter helpers. Value-carrying source C++ Result storage now emits the
      tagged `ps_result_value` bridge type instead of raw `uint64_t`
      return/binding types or legacy `ps_legacy_result_*` helper names; the
      bridge uses named ok/error tag constants, an `ok` success field,
      value-qualified tag/error/success accessors, and explicit ok/error
      construction helpers. Raw packed-integer conversion, construction
      compatibility, generic `ps_result_*` value accessor names, and the
      generated `ps_result_pack(...)` helper have been deleted. Status-only
      source C++ Result storage now emits the tagged `ps_result_status` bridge
      type instead of raw `uint32_t` return/binding types, uses the same named
      ok/error tag constants, and wraps low-level file helper status codes at
      the source Result boundary. Explicit source C++ constructors for
      supported `Result<T, E>{[ok] value}`,
      `Result<T, E>{[error] err}`, `Result<E>{}`, `Result<E>{ok}`, and
      `Result<E>{[error] err}` shapes now route through the same bridge
      helper constructors. Source C++ `Result.why(...)` now binds status-only
      and value-carrying bridge operands once, returns an empty string for
      `ok` tags, and only calls the error-domain `why` helper for `error`
      payloads. Explicit source C++ `error` constructors now also pack
      single-field int-backed error structs through their code field before
      entering the status-only or value-carrying bridge. Direct source C++
      `Result.ok(value)` now also packs local or explicitly constructed
      single-field success structs through their code field before entering the
      value-carrying bridge. Remaining cleanup should retarget broader bridge
      construction and propagation to the stdlib Result sum contract.
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

- [ ] TODO-4291: Decide sum-backed mutable `Maybe<T>` helpers
  - owner: ai
  - created_at: 2026-04-28
  - phase: Deferred stdlib ADT migration
  - depends_on: TODO-4267
  - scope: Decide whether `Maybe<T>.set(value)`, `Maybe<T>.clear()`, and
    `Maybe<T>.take()` should return as stdlib helpers on top of sum-backed
    `Maybe<T>`, or remain retired in favor of explicit `pick` plus value
    construction.
  - implementation_notes:
    - Start from `stdlib/std/maybe/maybe.prime`,
      `tests/unit/test_semantics_maybe.cpp`, and the VM/native Maybe
      compile-run tests.
    - If mutable helpers return, first define the general language contract for
      in-place active-variant mutation and payload movement on sum values.
    - If mutable helpers stay retired, strengthen diagnostics so callers get a
      named migration error rather than a generic missing-helper path.
  - acceptance:
    - The chosen direction is documented in `docs/PrimeStruct.md`.
    - Semantic tests cover `set`, `clear`, and `take` either as supported
      helper behavior or as deterministic migration diagnostics.
    - Compile-run coverage covers the chosen supported Maybe mutation or
      explicit-construction/pick replacement surface.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the sum-backed Maybe mutable-helper policy is
    implemented or intentionally retired with focused diagnostics and docs.

- [ ] TODO-4268: Add heterogeneous type-pack syntax and metadata
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4267
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

- [ ] TODO-4292: Promote and style canonical `.prime` vector implementation
  - owner: ai
  - created_at: 2026-04-28
  - phase: Vector stdlib ownership cutover
  - scope: Move the real `Vector<T>` implementation out of the public
    `/std/collections/experimental_vector/*` namespace and make the canonical
    `/std/collections/vector/*` surface, or a non-public
    `/std/collections/internal_vector/*` module behind that surface, own the
    implementation while bringing the public vector `.prime` source up to the
    readability bar in `docs/CodeExamples.md`.
  - implementation_notes:
    - Start from `stdlib/std/collections/vector.prime`,
      `stdlib/std/collections/experimental_vector.prime`,
      `stdlib/std/collections/collections.prime`,
      `src/StdlibSurfaceRegistry.cpp`, collection import tests, vector
      compile-run suites, and docs/source-lock tests that mention
      `experimental_vector`.
    - Cross-check `docs/CodeExamples.md`: `vector.prime` is listed as
      style-aligned surface code, while `experimental_vector.prime` and
      `internal_*` collection files are bridge/substrate-oriented. If the
      canonical implementation needs low-level buffer mechanics, keep those
      mechanics in an internal implementation module and keep the public vector
      surface readable.
    - Prefer CodeExamples surface style in public vector code where the current
      language supports it: descriptive names, shallow control flow, concise
      inferred locals, method-style calls, operator syntax instead of canonical
      helper-call noise, and lowerCamelCase member helpers. Do not copy the
      current `vectorPair`/`vectorTriple`/manual-forwarding style into the
      canonical implementation except as isolated compatibility shims.
    - Canonical helper ownership should be path/module based, not encoded into
      function names: prefer `/std/collections/vector/count`,
      `/std/collections/vector/push`, `/std/collections/vector/remove_at`,
      and namespace-owned `count`/`push`/`remove_at` helper definitions over
      `vectorCount`, `vectorPush`, `vectorRemoveAt`, or similar prefixed names.
    - Preserve exact `import /std/collections/vector`, wildcard
      `import /std/collections/*`, constructor/literal rewrite, method sugar,
      and fixed-arity compatibility behavior while moving the implementation
      owner.
    - Keep `/std/collections/experimental_vector/*` only as a compatibility shim
      for targeted tests; do not change map or SoA public status in this task.
  - acceptance:
    - Canonical vector imports exercise a non-experimental implementation owner
      in `.prime`, not a public wrapper whose primary body lives in
      `experimental_vector`.
    - Direct experimental-vector imports either continue through a documented
      shim or are limited to explicitly named compatibility/conformance tests.
    - Public docs and examples no longer present `experimental_vector` as the
      vector implementation namespace.
    - `stdlib/std/collections/vector.prime` is no longer a thin low-quality
      wrapper over experimental helpers; its public code follows the
      `docs/CodeExamples.md` style boundary or delegates only to an explicitly
      internal implementation module.
    - The canonical public vector implementation and examples use
      `/std/collections/vector/<helper>` / namespace-owned helper names rather
      than `vectorCount` / `vectorPush` / `vectorPair`-style prefixed names.
    - Any remaining low-level buffer, slot, ownership, or compatibility code is
      quarantined in `internal_*` or compatibility-shim files and is not used as
      the public style reference.
    - Existing vector constructor, access, mutation, lifecycle, and import
      conformance remains behavior-compatible.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the implementation owner is canonical/internal and
    compatibility imports are only shims; leave generic storage/lowering
    extraction to TODO-4293 and TODO-4294.

- [ ] TODO-4293: Stabilize generic contiguous-storage substrate
  - owner: ai
  - created_at: 2026-04-28
  - phase: Vector stdlib ownership cutover
  - depends_on: TODO-4292
  - scope: Make the contiguous heap-buffer and lifecycle primitives needed by
    vector usable as generic `.prime` substrate rather than vector-specific
    runtime behavior.
  - implementation_notes:
    - Start from `/std/collections/internal_buffer_checked/*`,
      `/std/collections/internal_buffer_unchecked/*`,
      `Pointer<uninitialized<T>>`, `init`, `drop`, `take`, `borrow`,
      `src/ir_lowerer/IrLowererUninitializedTypeHelpers.*`, and the reusable
      lifecycle pieces currently embedded in `IrLowererFlowVectorHelpers.cpp`.
    - Add or adjust focused fixtures that use a generic internal buffer helper
      outside vector to allocate, offset by dynamic index, initialize, move,
      borrow, take, drop, and free supported element types.
    - Keep the buffer namespaces internal implementation plumbing; this task
      proves the substrate for stdlib code and is not a public collection API.
  - acceptance:
    - VM/native lowering supports a non-vector `.prime` fixture that moves a
      prefix between two `Pointer<uninitialized<T>>` buffers with dynamic
      indexes and lifecycle-aware element handling.
    - Generic buffer allocation, checked/unchecked offsetting, free, and
      lifecycle failure paths produce deterministic diagnostics or runtime
      traps instead of vector-named errors.
    - Docs record the internal substrate contract and its relationship to
      `uninitialized<T>` and ownership helpers.
    - Existing vector and map behavior does not regress.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once generic buffer/lifecycle fixtures pass without relying
    on vector-specific helper recognition; leave vector helper rerouting to
    TODO-4294.

- [ ] TODO-4294: Lower vector helpers through ordinary `.prime`
  - owner: ai
  - created_at: 2026-04-28
  - phase: Vector stdlib ownership cutover
  - depends_on: TODO-4293
  - scope: Route canonical vector helper behavior through imported `.prime`
    helper bodies over the generic storage substrate instead of vector-specific
    semantic/lowering fast paths.
  - implementation_notes:
    - Start from `src/semantics/SemanticsValidatorCollectionHelperRewrites.cpp`,
      `src/semantics/SemanticsValidatorStatement.cpp`,
      `src/ir_lowerer/IrLowererFlowVectorHelpers.cpp`,
      `src/ir_lowerer/IrLowererOperatorCollectionMutationHelpers.cpp`,
      `src/ir_lowerer/IrLowererStructSlotLayoutHelpers.cpp`,
      `src/ir_lowerer/IrLowererCountAccessClassifiers.cpp`, and the vector
      import/mutator/source-lock tests.
    - Preserve user-visible canonical helper behavior while shrinking
      production C++ to generic call resolution, field/layout metadata,
      buffer/lifecycle primitives, and compatibility diagnostics.
    - Keep rooted `/vector/*`, `vectorCount`-style names, and direct
      experimental imports working only as temporary compatibility paths until
      TODO-4296.
  - acceptance:
    - Canonical `/std/collections/vector/*` `count`, `capacity`, `at`,
      `at_unsafe`, `push`, `pop`, `reserve`, `clear`, `remove_at`, and
      `remove_swap` run through ordinary `.prime` helper lowering on VM/native
      for the supported element kinds.
    - Production C++ no longer emits vector push/pop/reserve/remove semantics by
      matching vector helper paths or builtin vector names.
    - Vector layout used by lowering comes from ordinary struct metadata or the
      generic storage substrate, not a hard-coded vector header special case.
    - Any remaining vector strings in production C++ are declarative surface
      metadata or compatibility diagnostics, and are listed for removal in
      TODO-4295/TODO-4296.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once canonical vector helpers no longer depend on
    vector-specific semantic/lowering emission; leave capacity widening and
    compatibility deletion to TODO-4281, TODO-4295, and TODO-4296.

- [ ] TODO-4281: Lift vector dynamic capacity limit
  - owner: ai
  - created_at: 2026-04-28
  - phase: Vector stdlib ownership cutover
  - depends_on: TODO-4294
  - scope: Lift the current VM/native vector local dynamic-capacity limit
    beyond `256` after vector growth has been routed through ordinary `.prime`
    helpers over the generic contiguous-storage substrate.
  - implementation_notes:
    - Start from `src/ir_lowerer/IrLowererHelpers.{h,cpp}`,
      the replacement generic storage/lifecycle lowering from TODO-4293 and
      TODO-4294, `src/ir_lowerer/IrLowererFlowVectorHelpers.cpp` if any
      compatibility path still exists,
      `src/ir_lowerer/IrLowererOperatorCollectionMutationHelpers.cpp`,
      `tests/unit/test_compile_run_vm_collections_vector_limits_a.cpp`, and
      `tests/unit/test_compile_run_native_backend_collections_mutators_and_limits_*.cpp`.
    - Preserve deterministic allocation-failure diagnostics for requests that
      still exceed the widened runtime/allocator contract.
    - Update docs and source locks that mention the `256` ceiling in the same
      change.
  - acceptance:
    - VM and native vector literals, `reserve`, and repeated `push` can grow
      past the old `256` local dynamic-capacity limit for supported element
      kinds.
    - Out-of-range folded and runtime capacity requests still produce stable
      diagnostics instead of silent wraparound or backend faults.
    - Existing ownership, relocation, drop, and borrowed-access behavior stays
      stable across growth.
    - Docs record the new capacity contract and any remaining backend limits.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the old `256` ceiling is either removed or replaced by
    a documented wider allocator/runtime bound with VM/native coverage.

- [ ] TODO-4295: Move collection surface metadata out of C++
  - owner: ai
  - created_at: 2026-04-28
  - phase: Vector stdlib ownership cutover
  - depends_on: TODO-4294
  - scope: Remove vector-specific public-surface knowledge from handwritten C++
    and generated production C++ by moving canonical vector
    helper/import/constructor metadata into a stdlib-owned manifest or
    equivalent data-driven source consumed through generic collection-surface
    APIs.
  - implementation_notes:
    - Start from `include/primec/StdlibSurfaceRegistry.h`,
      `src/StdlibSurfaceRegistry.cpp`,
      `src/semantics/TemplateMonomorph*.h`, collection import resolution,
      wildcard import tests, and source locks that currently assert registry
      spellings.
    - The target is not to delete the notion of public stdlib surfaces; it is to
      stop encoding vector-specific lists and compatibility names in C++ source
      at all. Avoid generated C++ tables that still contain PrimeStruct vector
      strings; prefer a non-C++ manifest or stdlib metadata read through generic
      code.
  - acceptance:
    - Canonical vector import, wildcard import, constructor, and method-helper
      metadata is loaded from stdlib-owned data rather than handwritten or
      generated vector lists in production C++.
    - Existing canonical vector behavior and diagnostics stay stable across
      repeated builds.
    - Production `src/` and `include/` C++ no longer contain canonical vector
      surface tables, path aliases, import aliases, constructor spellings, or
      helper-name lists; temporary compatibility traces are limited to the
      deletion work tracked by TODO-4296.
    - Docs describe the stdlib surface metadata ownership boundary.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once canonical vector surface metadata no longer requires
    PrimeStruct-vector-specific C++ entries; leave removal of compatibility
    spellings and the final zero-trace audit to TODO-4296 and TODO-4297.

- [ ] TODO-4296: Delete vector compatibility seams
  - owner: ai
  - created_at: 2026-04-28
  - phase: Vector stdlib ownership cutover
  - depends_on: TODO-4281, TODO-4295
  - scope: Remove or intentionally reject the old vector compatibility spellings
    once the canonical `.prime` implementation, generic lowering path, widened
    capacity contract, and data-driven surface metadata are in place.
  - implementation_notes:
    - Target rooted `/vector/*`, `vectorCount` / `vectorCapacity` /
      `vectorPush`-style wrapper names, fixed-arity constructor wrappers that no
      longer have a public compatibility reason, and direct
      `/std/collections/experimental_vector/*` imports.
    - Preserve the canonical helper shape as path/module ownership:
      `/std/collections/vector/count`, `/std/collections/vector/capacity`,
      `/std/collections/vector/push`, `/std/collections/vector/pop`,
      `/std/collections/vector/reserve`, `/std/collections/vector/clear`,
      `/std/collections/vector/remove_at`,
      `/std/collections/vector/remove_swap`, `/std/collections/vector/at`, and
      `/std/collections/vector/at_unsafe`.
    - Start from `StdlibSurfaceRegistry`, semantic helper rewrites,
      template-monomorph compatibility adapters, lowerer raw-path dispatch
      checks, `stdlib/std/collections/collections.prime`, vector import tests,
      diagnostics snapshots, and docs/source locks.
    - Do not change map compatibility spellings in this task; add a separate map
      TODO before deleting `/map/*`, `mapCount`-style, or
      `/std/collections/experimental_map/*` seams.
  - acceptance:
    - Ordinary user code can use only canonical `/std/collections/vector/*`,
      exact `import /std/collections/vector`, wildcard
      `import /std/collections/*`, and documented vector construction syntax.
    - Rooted `/vector/*`, `vectorCount`-style names, and direct
      `experimental_vector` imports are either removed from tests/docs or reject
      with stable, intentional compatibility diagnostics.
    - No public docs, examples, or canonical stdlib vector source use prefixed
      helper names to encode the module path; they use slash paths,
      namespaces, imports, and method sugar instead.
    - Production C++ contains no PrimeStruct-vector-specific compatibility
      handling, compatibility diagnostics, removed-helper adapters, parser/text
      transform branches, lowerer raw-path checks, or surface metadata entries.
    - The de-experimentalization policy and vector/map bridge summary record the
      final vector status and any intentionally retained map seams.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once vector compatibility spellings are deleted or
    intentionally rejected and the only supported public vector surface is the
    canonical stdlib one; leave the mechanical C++ trace audit to TODO-4297.

- [ ] TODO-4297: Add zero C++ vector-surface audit
  - owner: ai
  - created_at: 2026-04-28
  - phase: Vector stdlib ownership cutover
  - depends_on: TODO-4296
  - scope: Add a deterministic validation gate that proves the PrimeStruct
    vector surface is fully `.prime`/stdlib-owned and absent from production
    C++ source.
  - implementation_notes:
    - Add a script or CTest-integrated check that scans production C++ under
      `src/` and `include/` for PrimeStruct-vector-specific traces such as
      `/vector`, `/std/collections/vector`, `experimental_vector`,
      `vectorCount`, `vectorCapacity`, `vectorPush`, `vectorPop`,
      `vectorReserve`, `vectorRemove`, `vectorAt`, `Vector<T>`, vector-specific
      diagnostics, and vector-specific parser/semantic/lowering branch names.
    - The check must allow ordinary C++ library usage such as `std::vector`,
      variable names that refer to C++ containers, and test/docs/source-lock
      files outside production `src/` and `include/`.
    - If generic collection code needs examples or fixtures, place them in
      tests/docs or stdlib-owned manifests rather than production C++ strings.
  - acceptance:
    - The audit runs in the release validation path and fails when a
      PrimeStruct-vector-specific production C++ trace is reintroduced.
    - The audit passes with canonical vector construction, imports, helper
      calls, mutation, growth, destruction, and direct `.prime` implementation
      behavior covered by existing tests.
    - Production C++ under `src/` and `include/` contains no PrimeStruct vector
      paths, helper aliases, type-name recognizers, diagnostics, special
      parser/text-transform cases, semantic rewrites, lowering dispatches, or
      metadata tables.
    - The only allowed vector mentions after the audit are `std::vector` as a
      C++ implementation container, stdlib `.prime` source, tests, docs, and
      explicitly non-production generated source-lock fixtures.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the release gate mechanically enforces that vector is
    fully stdlib-owned and no PrimeStruct-vector-specific production C++ traces
    remain.

- [ ] TODO-4299: Promote and style canonical `.prime` map implementation
  - owner: ai
  - created_at: 2026-04-28
  - phase: Map stdlib ownership cutover
  - depends_on: TODO-4297, TODO-4291
  - scope: Move the real `Map<K, V>` and `Entry<K, V>` implementation out of
    the public `/std/collections/experimental_map/*` namespace and make the
    canonical `/std/collections/map/*` surface, or a non-public
    `/std/collections/internal_map/*` module behind that surface, own the
    implementation while bringing the public map `.prime` source up to the
    readability bar in `docs/CodeExamples.md`.
  - implementation_notes:
    - Start from `stdlib/std/collections/map.prime`,
      `stdlib/std/collections/experimental_map.prime`,
      `stdlib/std/collections/collections.prime`,
      `src/StdlibSurfaceRegistry.cpp`, collection import tests, map
      compile-run suites, and docs/source-lock tests that mention
      `experimental_map`.
    - Cross-check `docs/CodeExamples.md`: `map.prime` is listed as
      style-aligned surface code, while `experimental_map.prime` and
      `internal_*` collection files are bridge/substrate-oriented. Keep
      low-level storage, slot overwrite, lookup loops, and compatibility
      scaffolding out of the public style-facing file when possible.
    - Prefer CodeExamples surface style in public map code where the current
      language supports it: descriptive names, shallow control flow, concise
      inferred locals, method-style calls, operator syntax instead of canonical
      helper-call noise, and lowerCamelCase member helpers.
    - Canonical helper ownership should be path/module based, not encoded into
      function names: prefer `/std/collections/map/count`,
      `/std/collections/map/contains`, `/std/collections/map/tryAt` or the
      documented final miss-result spelling, `/std/collections/map/at`,
      `/std/collections/map/at_unsafe`, and `/std/collections/map/insert` over
      `mapCount`, `mapContains`, `mapTryAt`, `mapAt`, or `mapInsert`.
    - Preserve exact `import /std/collections/map`, wildcard
      `import /std/collections/*`, constructor/literal rewrite, method sugar,
      and fixed-arity compatibility behavior while moving the implementation
      owner.
    - Keep `/std/collections/experimental_map/*` only as a compatibility shim
      for targeted tests; do not change vector or SoA public status in this
      task.
  - acceptance:
    - Canonical map imports exercise a non-experimental implementation owner in
      `.prime`, not a public wrapper whose primary body lives in
      `experimental_map`.
    - Direct experimental-map imports either continue through a documented shim
      or are limited to explicitly named compatibility/conformance tests.
    - Public docs and examples no longer present `experimental_map` as the map
      implementation namespace.
    - `stdlib/std/collections/map.prime` is no longer a thin low-quality wrapper
      over experimental helpers; its public code follows the
      `docs/CodeExamples.md` style boundary or delegates only to an explicitly
      internal implementation module.
    - The canonical public map implementation and examples use
      `/std/collections/map/<helper>` / namespace-owned helper names rather than
      `mapCount` / `mapContains` / `mapTryAt` / `mapInsert`-style prefixed
      names.
    - Any remaining low-level lookup, storage, overwrite, ownership, or
      compatibility code is quarantined in `internal_*` or compatibility-shim
      files and is not used as the public style reference.
    - Existing map construction, lookup, insertion, Result/ContainerError,
      lifecycle, and import conformance remains behavior-compatible.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the implementation owner is canonical/internal and
    compatibility imports are only shims; leave lookup/insertion substrate and
    lowering extraction to TODO-4300 and TODO-4301.

- [ ] TODO-4300: Stabilize map lookup and insertion substrate
  - owner: ai
  - created_at: 2026-04-28
  - phase: Map stdlib ownership cutover
  - depends_on: TODO-4299
  - scope: Make map lookup, miss reporting, overwrite, and insertion behavior
    expressible as ordinary `.prime` code over canonical vector/generic storage
    plus the stdlib `Result<ContainerError>` contract.
  - implementation_notes:
    - Start from `stdlib/std/collections/experimental_map.prime`,
      `stdlib/std/collections/errors.prime`, canonical vector helpers after
      TODO-4297, Result migration notes from TODO-4266/TODO-4291, and
      map compile-run tests covering `contains`, `tryAt`, `at`, `at_unsafe`,
      and `insert`.
    - Keep key comparability policy explicit: `Comparable<K>` or its successor
      should be a documented requirement of lookup/insertion, not a hidden C++
      classifier.
    - Decide and document whether the canonical miss-result helper remains
      `tryAt` for compatibility or migrates to a CodeExamples-aligned spelling
      such as `try_at`; keep any old spelling as a compatibility shim until
      TODO-4303.
    - Map may reuse vector/generic storage substrate, but map-specific policy
      such as duplicate-key overwrite, key equality, checked missing-key
      behavior, and `ContainerError` payloads belongs in this task.
  - acceptance:
    - Map lookup and insertion fixtures execute through `.prime` helpers over
      canonical vector/generic storage without relying on experimental vector
      or experimental map public imports.
    - Duplicate-key insertion overwrites the payload with ownership-sensitive
      drop/init behavior and keeps key/value counts aligned.
    - `contains`, miss-result lookup, checked `at`, unchecked `at_unsafe`, and
      `insert` behavior are documented for supported key/value kinds.
    - `Result<ContainerError>` miss behavior is stdlib-owned and covered on the
      supported VM/native paths without map-specific C++ result shims.
    - Existing map behavior and diagnostics remain stable unless the task
      intentionally updates them with docs and tests.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once map lookup/insertion policy is executable through
    ordinary `.prime` substrate and documented; leave semantic/lowering
    fast-path deletion to TODO-4301.

- [ ] TODO-4301: Lower map helpers through ordinary `.prime`
  - owner: ai
  - created_at: 2026-04-28
  - phase: Map stdlib ownership cutover
  - depends_on: TODO-4300
  - scope: Route canonical map helper behavior through imported `.prime`
    helper bodies over the map/vector/generic storage substrate instead of
    map-specific semantic/lowering fast paths.
  - implementation_notes:
    - Start from `src/semantics/SemanticsValidatorExprVectorHelpers.cpp`,
      `src/semantics/SemanticsValidatorInferPreDispatchCalls.cpp`,
      `src/semantics/TemplateMonomorphCollectionCompatibilityPaths.h`,
      `src/semantics/TemplateMonomorphExperimentalCollectionConstructorRewrites.h`,
      `src/ir_lowerer/IrLowererInlineNativeCallDispatch.cpp`,
      `src/ir_lowerer/IrLowererNativeTailDispatch.cpp`,
      `src/ir_lowerer/IrLowererPackedResultHelpers.cpp`,
      `src/ir_lowerer/IrLowererSetupTypeStructPathHelpers.cpp`, and map
      import/lookup/insert/source-lock tests.
    - Preserve user-visible canonical helper behavior while shrinking
      production C++ to generic call resolution, field/layout metadata,
      storage/lifecycle primitives, Result/sum primitives, and compatibility
      diagnostics.
    - Keep rooted `/map/*`, `mapCount`-style names, and direct experimental
      imports working only as temporary compatibility paths until TODO-4303.
  - acceptance:
    - Canonical `/std/collections/map/*` `entry`, `map`, `count`,
      `count_ref`, `contains`, `contains_ref`, miss-result lookup,
      `at`, `at_ref`, `at_unsafe`, `at_unsafe_ref`, `insert`, and `insert_ref`
      run through ordinary `.prime` helper lowering on VM/native for supported
      key/value kinds.
    - Production C++ no longer emits map lookup, insertion, miss-result, or
      checked-access semantics by matching map helper paths or builtin map
      names.
    - Map layout used by lowering comes from ordinary struct metadata and
      canonical vector/generic storage, not a hard-coded experimental map type
      recognizer.
    - Any remaining map strings in production C++ are declarative surface
      metadata or compatibility diagnostics, and are listed for removal in
      TODO-4302/TODO-4303.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once canonical map helpers no longer depend on map-specific
    semantic/lowering emission; leave metadata and compatibility deletion to
    TODO-4302 and TODO-4303.

- [ ] TODO-4302: Move map surface metadata out of C++
  - owner: ai
  - created_at: 2026-04-28
  - phase: Map stdlib ownership cutover
  - depends_on: TODO-4301
  - scope: Remove map-specific public-surface knowledge from handwritten C++
    and generated production C++ by moving canonical map
    helper/import/constructor metadata into a stdlib-owned manifest or
    equivalent data-driven source consumed through generic collection-surface
    APIs.
  - implementation_notes:
    - Start from `include/primec/StdlibSurfaceRegistry.h`,
      `src/StdlibSurfaceRegistry.cpp`,
      `src/semantics/TemplateMonomorph*.h`, collection import resolution,
      wildcard import tests, and source locks that currently assert map
      registry spellings.
    - Avoid generated C++ tables that still contain PrimeStruct map strings;
      prefer a non-C++ manifest or stdlib metadata read through generic code.
  - acceptance:
    - Canonical map import, wildcard import, constructor, and method-helper
      metadata is loaded from stdlib-owned data rather than handwritten or
      generated map lists in production C++.
    - Existing canonical map behavior and diagnostics stay stable across
      repeated builds.
    - Production `src/` and `include/` C++ no longer contain canonical map
      surface tables, path aliases, import aliases, constructor spellings, or
      helper-name lists; temporary compatibility traces are limited to the
      deletion work tracked by TODO-4303.
    - Docs describe the map surface metadata ownership boundary.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once canonical map surface metadata no longer requires
    PrimeStruct-map-specific C++ entries; leave removal of compatibility
    spellings and the final zero-trace audit to TODO-4303 and TODO-4304.

- [ ] TODO-4303: Delete map compatibility seams
  - owner: ai
  - created_at: 2026-04-28
  - phase: Map stdlib ownership cutover
  - depends_on: TODO-4302
  - scope: Remove or intentionally reject the old map compatibility spellings
    once the canonical `.prime` implementation, generic lowering path, and
    data-driven surface metadata are in place.
  - implementation_notes:
    - Target rooted `/map/*`, `mapCount` / `mapContains` / `mapTryAt` /
      `mapAt` / `mapInsert`-style wrapper names, fixed-arity constructor
      wrappers that no longer have a public compatibility reason, and direct
      `/std/collections/experimental_map/*` imports.
    - Preserve the canonical helper shape as path/module ownership:
      `/std/collections/map/entry`, `/std/collections/map/map`,
      `/std/collections/map/count`, `/std/collections/map/count_ref`,
      `/std/collections/map/contains`, `/std/collections/map/contains_ref`,
      the documented miss-result lookup helper spelling,
      `/std/collections/map/at`, `/std/collections/map/at_ref`,
      `/std/collections/map/at_unsafe`,
      `/std/collections/map/at_unsafe_ref`, `/std/collections/map/insert`, and
      `/std/collections/map/insert_ref`.
    - Start from `StdlibSurfaceRegistry`, semantic helper rewrites,
      template-monomorph compatibility adapters, lowerer raw-path dispatch
      checks, `stdlib/std/collections/collections.prime`, map import tests,
      diagnostics snapshots, and docs/source locks.
    - Do not change vector or SoA compatibility spellings in this task.
  - acceptance:
    - Ordinary user code can use only canonical `/std/collections/map/*`, exact
      `import /std/collections/map`, wildcard `import /std/collections/*`, and
      documented map construction syntax.
    - Rooted `/map/*`, `mapCount`-style names, fixed-arity bridge names, and
      direct `experimental_map` imports are either removed from tests/docs or
      reject with stable, intentional compatibility diagnostics.
    - No public docs, examples, or canonical stdlib map source use prefixed
      helper names to encode the module path; they use slash paths,
      namespaces, imports, and method sugar instead.
    - Production C++ contains no PrimeStruct-map-specific compatibility
      handling, compatibility diagnostics, removed-helper adapters, parser/text
      transform branches, lowerer raw-path checks, or surface metadata entries.
    - The de-experimentalization policy and vector/map bridge summary record
      the final map status.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once map compatibility spellings are deleted or
    intentionally rejected and the only supported public map surface is the
    canonical stdlib one; leave the mechanical C++ trace audit to TODO-4304.

- [ ] TODO-4304: Add zero C++ map-surface audit
  - owner: ai
  - created_at: 2026-04-28
  - phase: Map stdlib ownership cutover
  - depends_on: TODO-4303
  - scope: Add a deterministic validation gate that proves the PrimeStruct map
    surface is fully `.prime`/stdlib-owned and absent from production C++
    source.
  - implementation_notes:
    - Add a script or CTest-integrated check that scans production C++ under
      `src/` and `include/` for PrimeStruct-map-specific traces such as
      `/map`, `/std/collections/map`, `experimental_map`, `mapCount`,
      `mapContains`, `mapTryAt`, `mapAt`, `mapInsert`, `Map<K`, map-specific
      diagnostics, and map-specific parser/semantic/lowering branch names.
    - The check must allow ordinary C++ library usage such as `std::map`,
      generic mapping variable names, source-map infrastructure, and test/docs
      files outside production `src/` and `include/`.
    - If generic collection code needs examples or fixtures, place them in
      tests/docs or stdlib-owned manifests rather than production C++ strings.
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

- [ ] TODO-4305: Rename and style canonical `.prime` SoA surface
  - owner: ai
  - created_at: 2026-04-28
  - phase: SoA public surface rename and ownership cutover
  - depends_on: TODO-4297
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
    - Keep `soa_vector<T>`, `/std/collections/soa_vector/*`, `/soa_vector/*`,
      `SoaVector<T>`, and `soaVector*` only as compatibility shims until
      TODO-4309.
  - acceptance:
    - Docs define `soa<T>` and `/std/collections/soa/*` as the target canonical
      public SoA collection surface, with `soa_vector` documented as a
      compatibility spelling only.
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
    generic substrate and lowering extraction to TODO-4306 and TODO-4307.

- [ ] TODO-4306: Stabilize generic SoA substrate boundaries
  - owner: ai
  - created_at: 2026-04-28
  - phase: SoA public surface rename and ownership cutover
  - depends_on: TODO-4305
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
    lowering extraction to TODO-4307.

- [ ] TODO-4307: Lower SoA helpers through ordinary `.prime`
  - owner: ai
  - created_at: 2026-04-28
  - phase: SoA public surface rename and ownership cutover
  - depends_on: TODO-4306
  - scope: Route canonical `/std/collections/soa/*` helper and conversion
    behavior through imported `.prime` helper bodies over the generic SoA
    substrate instead of `soa_vector`-specific semantic/lowering fast paths.
  - implementation_notes:
    - Start from `include/primec/SoaPathHelpers.h`,
      `src/semantics/TemplateMonomorphCollectionCompatibilityPaths.h`,
      SoA helper/field-view semantic validators,
      `src/ir_lowerer/IrLowererInlineNativeCallDispatch.cpp`,
      `src/ir_lowerer/IrLowererInlineParamHelpers.cpp`,
      `src/ir_lowerer/IrLowererStatementBindingHelpers.cpp`,
      `src/ir_lowerer/IrLowererNativeTailDispatch.cpp`, and SoA
      helper/conversion/source-lock tests.
    - Preserve user-visible canonical behavior while shrinking production C++
      to generic call resolution, struct/field metadata, generic SoA substrate,
      borrow/invalidation primitives, and compatibility diagnostics.
    - Keep `/soa_vector/*`, `/std/collections/soa_vector/*`, `soaVector*`, and
      direct experimental imports working only as temporary compatibility paths
      until TODO-4309.
  - acceptance:
    - Canonical `/std/collections/soa/*` construction, count/get/ref,
      push/reserve, field-view, and conversion helpers run through ordinary
      `.prime` helper lowering on VM/native for supported element kinds.
    - Production C++ no longer emits public SoA collection helper behavior by
      matching `soa_vector` helper paths, `soaVector*` names, or builtin
      `soa_vector` receiver names.
    - Any remaining C++ SoA code is classified as generic substrate rather than
      public collection-surface behavior.
    - Any remaining old public-surface strings in production C++ are
      compatibility diagnostics or metadata listed for removal in
      TODO-4308/TODO-4309.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once canonical `soa` helpers no longer depend on
    `soa_vector`-specific semantic/lowering emission; leave metadata and
    compatibility deletion to TODO-4308 and TODO-4309.

- [ ] TODO-4308: Move SoA surface metadata out of C++
  - owner: ai
  - created_at: 2026-04-28
  - phase: SoA public surface rename and ownership cutover
  - depends_on: TODO-4307
  - scope: Remove public SoA collection-surface knowledge from handwritten C++
    and generated production C++ by moving canonical `soa`
    helper/import/constructor/conversion metadata into stdlib-owned data
    consumed through generic collection-surface APIs.
  - implementation_notes:
    - Start from `include/primec/StdlibSurfaceRegistry.h`,
      `src/StdlibSurfaceRegistry.cpp`, `include/primec/SoaPathHelpers.h`,
      template-monomorph compatibility paths, collection import resolution,
      wildcard import tests, and source locks that currently assert
      `soa_vector` registry spellings.
    - Avoid generated C++ tables that still contain PrimeStruct `soa` or
      `soa_vector` public-surface strings; prefer a non-C++ manifest or stdlib
      metadata read through generic code.
    - Keep generic SoA substrate metadata separate from public collection
      surface metadata.
  - acceptance:
    - Canonical `soa` import, wildcard import, constructor, method-helper,
      field-view, and conversion metadata is loaded from stdlib-owned data
      rather than handwritten or generated public SoA surface lists in
      production C++.
    - Existing canonical SoA behavior and diagnostics stay stable across
      repeated builds.
    - Production `src/` and `include/` C++ no longer contain public SoA
      collection-surface tables, path aliases, import aliases, constructor
      spellings, conversion spellings, or helper-name lists; temporary
      compatibility traces are limited to the deletion work tracked by
      TODO-4309.
    - Docs describe the boundary between public `soa` surface metadata and
      generic SoA substrate metadata.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once canonical SoA surface metadata no longer requires
    PrimeStruct-SoA-public-surface C++ entries; leave removal of compatibility
    spellings and the final zero-trace audit to TODO-4309 and TODO-4310.

- [ ] TODO-4309: Delete `soa_vector` compatibility seams
  - owner: ai
  - created_at: 2026-04-28
  - phase: SoA public surface rename and ownership cutover
  - depends_on: TODO-4308
  - scope: Remove or intentionally reject the old `soa_vector` compatibility
    spellings once the canonical `soa<T>` implementation, generic substrate
    boundary, `.prime` lowering path, and data-driven surface metadata are in
    place.
  - implementation_notes:
    - Target `soa_vector<T>`, `/std/collections/soa_vector/*`,
      `/std/collections/soa_vector_conversions/*`, rooted `/soa_vector/*`,
      rooted `/to_aos` / `/to_aos_ref` compatibility spellings when they encode
      the old surface, `SoaVector<T>`, `soaVector*` wrappers, and direct
      `/std/collections/experimental_soa_vector*` imports.
    - Preserve the canonical helper shape as path/module ownership:
      `/std/collections/soa/soa`, `/std/collections/soa/count`,
      `/std/collections/soa/get`, `/std/collections/soa/ref`,
      `/std/collections/soa/count_ref`, `/std/collections/soa/get_ref`,
      `/std/collections/soa/ref_ref`, `/std/collections/soa/push`,
      `/std/collections/soa/reserve`, field-view helpers, and the documented
      conversion helper paths.
    - Start from `StdlibSurfaceRegistry`, `SoaPathHelpers`, semantic helper
      rewrites, template-monomorph compatibility adapters, lowerer raw-path
      dispatch checks, SoA stdlib files, SoA import tests, diagnostics
      snapshots, ECS examples, and docs/source locks.
  - acceptance:
    - Ordinary user code can use only canonical `soa<T>`,
      `/std/collections/soa/*`, wildcard `import /std/collections/*`, and
      documented SoA construction/conversion syntax.
    - `soa_vector<T>`, `/soa_vector/*`, `soaVector*` names, and direct
      `experimental_soa_vector*` imports are either removed from tests/docs or
      reject with stable, intentional compatibility diagnostics.
    - No public docs, examples, or canonical stdlib SoA source use prefixed
      helper names to encode the module path; they use slash paths,
      namespaces, imports, and method sugar instead.
    - Production C++ contains no PrimeStruct-SoA-public-surface compatibility
      handling, compatibility diagnostics, removed-helper adapters, parser/text
      transform branches, lowerer raw-path checks, or public surface metadata
      entries.
    - The de-experimentalization policy and SoA public collection summary record
      the final `soa` status.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once old `soa_vector` compatibility spellings are deleted
    or intentionally rejected and the only supported public SoA collection
    surface is `soa<T>`; leave the mechanical C++ trace audit to TODO-4310.

- [ ] TODO-4310: Add zero C++ SoA collection-surface audit
  - owner: ai
  - created_at: 2026-04-28
  - phase: SoA public surface rename and ownership cutover
  - depends_on: TODO-4309
  - scope: Add a deterministic validation gate that proves the PrimeStruct SoA
    public collection surface is fully `.prime`/stdlib-owned and absent from
    production C++ source while allowing generic SoA substrate code.
  - implementation_notes:
    - Add a script or CTest-integrated check that scans production C++ under
      `src/` and `include/` for PrimeStruct-SoA-public-surface traces such as
      `/soa_vector`, `/std/collections/soa_vector`, `/std/collections/soa`,
      `experimental_soa_vector`, `SoaVector`, `soaVector`, old `to_aos`
      compatibility shims, SoA public-surface diagnostics, and
      collection-surface parser/semantic/lowering branch names.
    - The check must allow generic SoA substrate terms where they are not public
      collection surface names: `SoaColumn`, `SoaFieldView`, `SoaSchema`,
      field-layout/codegen/introspection helpers, field-view borrow/invalidation
      machinery, and allocation primitives.
    - The check must also allow tests/docs/source-lock files outside production
      `src/` and `include/`.
  - acceptance:
    - The audit runs in the release validation path and fails when a
      PrimeStruct-SoA-public-surface production C++ trace is reintroduced.
    - The audit passes with canonical `soa<T>` construction, imports, helper
      calls, field views, conversion, mutation, invalidation, destruction, and
      direct `.prime` implementation behavior covered by existing tests.
    - Production C++ under `src/` and `include/` contains no PrimeStruct SoA
      public collection paths, helper aliases, type-name recognizers,
      diagnostics, special parser/text-transform cases, semantic rewrites,
      lowering dispatches, or metadata tables.
    - The only allowed SoA mentions after the audit are generic SoA substrate
      terms, stdlib `.prime` source, tests, docs, and explicitly
      non-production generated source-lock fixtures.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the release gate mechanically enforces that the public
    SoA collection surface is `soa<T>`, fully stdlib-owned, and has no
    PrimeStruct-SoA-public-surface production C++ traces.
