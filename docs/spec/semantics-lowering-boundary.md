# Planned Semantics-to-Lowering Boundary

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **design direction**.

### Planned semantics-to-lowering boundary
PrimeStruct is migrating toward an explicit post-semantics product that sits between the syntax-faithful AST and IR
lowering. The goal is to stop re-deriving lowering facts from mutated AST state and instead hand IR preparation one
deterministic, inspectable semantics artifact.

Planned boundary shape:
- The raw AST remains the syntax-faithful structure used for parsing, source spans, and surface-oriented dumps.
- Semantics produces a dedicated lowering-facing artifact such as `SemanticProgram` or `ResolvedModule`.
- That semantic product owns resolved call targets, binding/result types, effect and capability facts, struct metadata,
  and the graph-backed local/query/`try(...)` inference facts that lowering currently has to reconstruct.
- IR preparation consumes that semantic product directly instead of re-reading helper aliases, binding kinds, or
  receiver metadata from the AST.

Proposed semantic-product contract:
- `SemanticProgram` is the whole-program post-semantics artifact published by the compile pipeline.
- `SemanticProgram` contains one `ResolvedModule` per parsed module or imported source unit, in deterministic import
  order.
- Each `ResolvedModule` owns lowering-facing facts only:
  - resolved definition signatures and canonical full paths
  - resolved call targets and helper-vs-canonical path choices
  - binding/type facts for parameters, locals, temporaries, and return values
  - collection specialization facts for vector/map/`soa_vector` binding sites,
    including canonical helper and constructor family routing
  - effect/capability facts needed by IR preparation
  - struct/enum/layout metadata needed by lowering and backend setup
  - graph-backed local `auto`, query, `try(...)`, and `on_error` inference facts
  - stable semantic node ids and syntax-faithful provenance handles for diagnostics/debug mapping
- The raw AST remains the owner of syntax-oriented structure:
  - original statement/expression tree shape
  - parser-oriented sugar before semantic interpretation
  - token/source-span ownership and surface dump formatting

Ownership and lifetime rules:
- The semantic product is immutable after `Semantics::validate` succeeds.
- The semantic product owns copied lowering facts; it must not depend on mutable AST-side caches or validator-global
  scratch state.
- Compilation-local symbol interning uses `SymbolId` via `primec::SymbolInterner`; ids are assigned in first-seen
  order within one compilation and are deterministic when traversal order is deterministic.
- `SymbolId` values are owned by the interner instance that produced them: `0` is invalid, non-zero ids are only valid
  against that same interner, and resolved text views are borrowed from interner storage until `clear()`/destruction.
- Deterministic worker-interner merge ordering and collision rules for parallel validation are defined in
  `docs/Semantics_Multithread_Design.md` (P4-02 section).
- Semantic nodes may reference syntax provenance only through stable ids/spans, not raw pointers into temporary
  validator state.
- During migration, the AST may still outlive the semantic product so debug/source-map consumers can read syntax-faithful
  provenance, but lowering must treat that as read-only provenance data rather than a source of semantic truth.
- Once the boundary is complete, lowering-facing decisions must come from the semantic product even when equivalent
  information is still visible on the mutated AST.

Ownership split by responsibility:
- Raw AST owns:
  - token text, parser tree shape, and original source-order structure
  - exact source spans used for parser-facing diagnostics and surface dumps
  - syntax-faithful forms that may be normalized away before lowering
- Semantic product owns:
  - canonical resolved paths after helper-shadow and import resolution
  - final binding/type/effect facts consumed by lowering
  - semantic node ids used to join lowering facts back to syntax provenance
  - deterministic per-definition metadata exported to IR preparation and later backends
- Consumer coverage for published fact families is tracked in
  `docs/SemanticProductConsumerMatrix.md`. Each row names the known production
  consumers or states that no production consumer exists, and representative
  rows link positive plus stale/missing-fact coverage until a richer manifest
  replaces the checked document.
- Shared boundary rule:
  - source spans, debug/source-map data, and user-facing syntax reproduction remain AST-owned, but the semantic product
    carries stable references to that provenance so lowering/debuggers never need to re-infer semantics from syntax.
  - any lowering consumer that needs both “what this means” and “where it came from” must read meaning from the
    semantic product and provenance from the AST-backed ids/spans, not from mutated AST semantics fields.

Planned first semantic-product builder slice:
- The first builder slice should materialize only the lowering-facing facts already treated as stable by the current
  semantics pipeline:
  - resolved call targets and helper-vs-canonical path choices, split concretely into:
    - direct-call targets and canonical callee paths
    - receiver/method-call targets and receiver-side helper-family choices
    - helper-vs-canonical path choices for collection/builtin bridge surfaces
  - final binding/result type facts for parameters, locals, temporaries, and returns
  - effect/capability summaries needed by IR preparation
  - struct/enum/layout metadata already computed during semantic validation
- This first slice should not yet absorb the graph-backed local/query/`try(...)`/`on_error` snapshot-style metadata;
  that remains a second builder slice so the initial publication surface stays narrow.
- The first slice should prefer direct transfer from existing validator-owned facts rather than re-deriving lowering
  facts from the AST a second time.
- Completion criteria:
  - one semantic-product builder path can materialize the above facts for successful semantic validation
  - those facts are deterministic and sufficient for later lowering cutover work to start consuming call targets,
    binding types, effects/capabilities, and struct metadata from the semantic product
  - existing graph-backed inference/testing-only metadata remains outside this first slice until the second builder
    stage is ready

Planned second semantic-product builder slice:
- The second builder slice should move the currently test-only graph-backed inference metadata onto the semantic
  product once the first builder slice is stable.
- That second slice should own:
  - graph-backed local `auto` facts
  - graph-backed query metadata
  - graph-backed `try(...)` metadata
  - graph-backed `on_error` metadata
- The goal is to replace testing-only snapshot plumbing for lowering-facing semantic facts with one published semantic
  product rather than a parallel testing surface.
- This second slice should reuse already-stabilized graph facts instead of recomputing them from the AST or validator
  scratch state after validation.
- Completion criteria:
  - the listed graph-backed inference facts are published on the semantic product in deterministic order
  - tests that currently need snapshot-only access to those facts can migrate to the semantic-product inspection surface
  - the first builder slice remains unchanged in scope, and this second slice is the only step that adds graph-backed
    local/query/`try(...)`/`on_error` metadata to the published product

Planned ownership-split test matrix:
- Source-span parity:
  semantic-product entries that represent lowered calls, bindings, and control-flow facts should still reference the
  same AST-owned spans used by diagnostics and syntax-facing dumps.
- Debug/source-map parity:
  lowering and VM/debug entrypoints should be able to recover syntax-faithful provenance from semantic-product
  ids/handles without reading semantic facts back out of the raw AST.
- Syntax-reproduction boundary:
  `ast` and `ast-semantic` remain the syntax-shaped inspection surfaces, while the semantic-product dump should
  expose only lowering-facing facts plus provenance handles.
- Ownership isolation:
  semantic-product facts should remain valid even if AST-side semantic caches are removed or reorganized, as long as
  the stable AST ids/spans they reference still exist.
- Deterministic ordering:
  semantic-product ownership/provenance tests should be able to use golden files without incidental ordering noise
  from modules, definitions, bindings, or diagnostics.
- Current status:
  source-span parity tests, debug/source-map parity coverage, syntax-boundary coverage, and deterministic
  ownership-order coverage now pin the lowering-facing semantic-product surfaces that currently publish provenance
  handles.

Migration stages:
1. Define the semantic-product type and its ownership contract.
2. Materialize the first lowering-required facts into that product while keeping the existing AST-based lowerer path
   available behind a temporary adapter.
3. Thread the semantic product through compile-pipeline success results, dumps, and backend entrypoints.
4. Cut `prepareIrModule` and `IrLowerer::lower` over to the semantic product.
5. Remove the temporary adapter and the AST-dependent lowerer re-derivations once coverage proves parity.

Temporary migration adapter contract:
- The adapter exists only to let lowering entrypoints accept either a raw `Program` or a semantic product during the
  cutover; it is not a second permanent lowering API.
- If a semantic product is present, the adapter must prefer semantic-product facts over any AST-side re-derivation.
- If only a raw `Program` is present, the adapter may perform the minimum compatibility derivation needed to preserve
  current lowering behavior, but that fallback should stay explicitly transitional.
- The adapter should not invent a third ownership model; provenance still comes from the AST-backed ids/spans and
  lowering-facing meaning still comes from the semantic product whenever available.
- The adapter boundary should stay narrow: compile-pipeline handoff, `prepareIrModule`, and `IrLowerer::lower` are the
  intended temporary consumers.
- Current status: the adapter now feeds lowering entry/setup with semantic-product direct-call targets,
  receiver/method-call targets, helper-routing choices, source-owned binding/result facts, and the current
  callable-summary/type-metadata effect-layout surfaces. The published graph-backed local/query/`try(...)`/`on_error`
  facts are now used by lowerer setup where they carry lowering-facing meaning; executable `on_error` bound-arg
  expressions remain syntax-owned and are still parsed from published argument text when lowering needs handler
  arguments. Call-target adapter lookup now requires stable semantic-node identities for direct and method call facts
  instead of source line/column compatibility scans.
- Removal criteria:
  - `CompilePipelineOutput` publishes the semantic product on the success path.
  - `prepareIrModule` and `IrLowerer::lower` consume the semantic product directly in production codepaths.
  - Lowerer setup no longer needs AST-only compatibility queries for call targets, binding metadata, helper routing, or
    effect/layout facts.
  - Coverage proves parity for the semantic-product path across C++/VM/native before deleting the raw-`Program`
    fallback.

Compile-pipeline publication contract:
- `Semantics::validate` now produces an initial semantic product shell as its canonical post-semantics success result,
  not only a mutated AST.
- `CompilePipelineOutput` now publishes that semantic product shell on successful semantic validation so later dump,
  backend, and runtime entrypoints do not need private validator access to recover it.
- The published semantic product should remain paired with the raw AST rather than replacing it outright; syntax-facing
  consumers still need the AST for spans, source reproduction, and surface-shaped dumps.
- Failure paths should continue to report diagnostics against AST-backed provenance, but post-semantics compile-pipeline
  failures should still preserve the published semantic product plus a first-class failure object rather than dropping
  back to raw string/error-stage side channels.
- The `primec` and `primevm` runtime entrypoints, plus focused post-semantics failure preservation coverage, now
  consume explicit compile-pipeline success/failure variants for that preserved failure object. The legacy output
  struct remains only as the compatibility surface for success-only dump and conformance helpers that still need the
  full compile artifact but do not inspect failure objects.
  Post-semantics diagnostics may explicitly note that a semantic product is already available even though later target
  validation failed.
- The current published shell is still intentionally narrow, but it now includes the resolved call-target surface:
  entry path, import inventories, deterministic definition/execution inventories, resolved direct-call targets with
  canonical callee paths, resolved receiver/method-call targets with receiver-side helper-family choices, and direct
  collection/builtin bridge routing choices are available now. Deterministic callable summaries (return kind,
  compute/unsafe flags, active effects, capabilities, and result/on_error summaries), definition-level
  struct/enum/layout metadata (category, visibility, layout-policy flags, explicit alignment, and field/item counts),
  final binding/result facts for parameters/locals/temporary call results/returns, and graph-backed local-auto,
  query, `try(...)`, and `on_error` facts are also published now.
- The first semantic-product builder slice, second graph-backed builder slice, CLI/runtime plumbing cutover,
  temporary migration adapter, and deterministic semantic-product dump/formatter are therefore all complete. The
  active queue no longer tracks Group 12 entrypoint retirement, provenance/ownership coverage, backend conformance, or
  cleanup/removal passes; add a concrete TODO before changing any of those seams.
- Local-auto lowering consumes semantic-node-id facts only. Initializer-path plus binding-name indexes may remain as
  published metadata for inspection, but production lowering must fail closed instead of using them as a recovery path.
- Query, `try(...)`, and `on_error` lowering/completeness checks follow the same rule: semantic-node-id facts are the
  production authority, while resolved-path/source/path-id indexes remain inspection metadata only.
- The lowerer semantic-product adapter no longer builds local-auto initializer-path, query resolved-path/call-name, or
  `try(...)` operand-path/source composite recovery indexes. Those composite lookup maps remain publication and
  inspection metadata rather than production lowerer lookup surfaces.
- The same adapter quarantine now applies to `on_error` definition-path indexes: lowerer handler setup resolves
  `on_error` facts by definition semantic id, while definition-path lookup remains a published inspection surface.
- `on_error` definition-id indexing follows the same map authority: lowerer semantic-product indexes populate
  `on_error` facts from `onErrorFactIndicesByDefinitionId` and do not backfill matching raw facts by definition
  semantic node id.
- Return facts follow the same adapter-cache rule for direct definition lookup: the semantic-product adapter no longer
  materializes a return definition-path index as a recovery surface. The explicit return-by-path helper remains
  available for call-site consumers that already resolved a callee path, but it reads the published definition-path
  return-fact map instead of scanning raw return facts.
- Return definition-id indexing follows the same map authority: lowerer semantic-product indexes populate return facts
  from `returnFactIndicesByDefinitionId` and do not backfill matching raw return facts by definition semantic node id.
- Return result-metadata completeness validation follows the same published-map rule: return fact checks enumerate
  `returnFactIndicesByDefinitionId` entries and do not validate raw return facts that are absent from the map.
- Sum type and variant metadata helpers follow the same publication-authority rule: resolved sum path/variant-name
  consumers read the published sum metadata maps instead of scanning raw metadata vectors.
- Callable summary lookups read the published callable-summary path map and fail closed when a matching raw summary has
  no published lookup entry.
- Return-info precomputation follows the same callable-summary map authority: setup-stage precompute enumerates callable
  definitions through `callableSummaryIndicesByPathId` instead of using raw callable-summary vectors as a fallback.
- Query semantic-id indexing follows the same map authority: lowerer semantic-product indexes populate query facts from
  `queryFactIndicesByExpr` and do not backfill matching raw query facts by semantic node id.
- Try semantic-id indexing follows the same map authority: lowerer semantic-product indexes populate `try(...)` facts
  from `tryFactIndicesByExpr` and do not backfill matching raw try facts by semantic node id.
- Binding semantic-id indexing follows the same map authority: lowerer semantic-product indexes populate binding facts
  from `bindingFactIndicesByExpr` and do not backfill matching raw binding facts by semantic node id.
- Entry-argument setup follows the binding map authority rule: semantic-product entry parameter detection enumerates
  published `bindingFactIndicesByExpr` entries and does not scan raw binding facts by scope/site as a recovery path.
- Base call-kind inference now applies the same graph-backed rule for `try(...)`: semantic-product `try` facts provide
  the value kind before local `Result` state can infer it, and missing facts leave the semantic-product path unresolved.
- Aggregate/uninitialized struct inference applies that rule to `try(...)` payloads as well: published try value-type
  facts provide the payload struct before local `Result` struct metadata can infer it, published scalar facts suppress
  stale local aggregate metadata, and syntax-only contexts keep the existing fallback.
- Aggregate/uninitialized source expression inference now follows graph-backed type facts for direct binding,
  local-auto, and query expressions before consulting stale local struct metadata or recomputed definition returns;
  published scalar facts suppress aggregate fallback while syntax-only contexts keep compatibility inference.
- Memory pointer arithmetic sizing follows graph-backed pointer target facts for binding, local-auto, and query pointer
  expressions before local pointer struct metadata can determine aggregate slot scaling; published scalar facts suppress
  stale aggregate scaling, and syntax-only pointer expressions keep compatibility inference.
- Native `increment(dereference(...))` / `decrement(dereference(...))` target inference follows the same graph-backed
  pointer rule: semantic-product pointer/reference binding facts provide the mutable target value kind before stale
  dereference value-kind inference can answer, while syntax-only contexts keep the existing fallback.
- Native indexed assignment follows the same collection authority rule:
  semantic-product collection, binding, local-auto, and query facts classify
  `target[index] = value` receivers before stale local array/vector metadata;
  published non-collection facts suppress stale local collection fallback, and
  syntax-only contexts keep compatibility inference.
- Native indexed-access validation now follows the graph-backed rule for index
  expressions: query, binding, and local-auto type facts provide the index kind
  before legacy expression inference is consulted.
- GPU `buffer_store(...)` validation follows the same graph-backed index rule:
  semantic-product query, binding, and local-auto type facts provide the index
  kind before legacy expression inference is consulted.
- GPU `buffer_store(...)` target validation also follows graph-backed buffer
  facts: semantic-product binding, local-auto, and query facts provide the
  buffer element kind before local buffer metadata is consulted, and published
  non-buffer facts suppress stale local fallback.
- GPU `dispatch(...)` dimension validation follows the same graph-backed
  scalar rule: semantic-product binding, local-auto, and query facts must
  classify each dimension as `i32` before legacy expression inference is used.
- Statement-call `map.insert(...)` builtin rewriting follows the same map
  authority rule: semantic-product binding, local-auto, and query map facts
  provide receiver key/value kinds before local map metadata is used.
- Statement-call vector mutator rewriting follows the same graph-backed
  collection rule: semantic-product binding, local-auto, and query vector
  facts identify builtin vector receivers before local vector metadata is
  used.
- Dump-stage handling should be able to read either the syntax-facing canonical AST dump or the future semantic-product
  dump from the same compile-pipeline success result without re-running semantics.
- Backend/runtime entrypoints should consume the semantic product from compile-pipeline output once available rather
  than rebuilding semantic facts ad hoc from the raw `Program`.
- That success-path handoff is now live for `primec` compile/emit entrypoints, `primevm`, backend registry dispatch,
  and the shared `prepareIrModule(...)` / `IrLowerer::lower(...)` seam. The `primec` and `primevm` result boundaries
  now also use explicit success/failure variants, and the semantic-product dump/report API boundary is now versioned
  under the contract below. Add a separate concrete TODO before changing unrelated CLI/runtime seams.

CLI/runtime plumbing contract:
- `primec` and `primevm` should receive the semantic product only through compile-pipeline success artifacts, not
  through direct validator internals or separate semantic side channels.
- Dump-stage handling should treat the semantic product as a first-class inspectable result once that stage exists,
  while keeping existing `pre_ast`, `ast`, `ast-semantic`, `type-graph`, and `ir` behavior deterministic.
- Failure/report paths should continue to anchor diagnostics to AST-backed provenance, but any lowering-facing notes
  emitted after semantic success should refer to semantic-product facts rather than re-derived AST semantics.
- Backend dispatch, VM execution, and future debugger/runtime entrypoints should consume the same published
  semantic-product artifact so CLI/runtime code does not grow parallel semantic caches.
- The CLI/runtime handoff should stay one-way: once compile-pipeline success is produced, downstream consumers read the
  semantic product and provenance handles, but do not mutate semantic state in place.

Exit criteria for removing AST-dependent lowerer logic:
- `Semantics::validate` or `CompilePipelineOutput` publishes the semantic product as the canonical post-semantics
  success artifact.
- `prepareIrModule` and `IrLowerer::lower` accept the semantic product directly in production entrypoints.
- Lowerer setup no longer re-derives call targets, binding types, or helper-path decisions from raw AST state.
- A deterministic semantic-product dump exists and has golden coverage alongside the existing AST/IR dump stages.
- End-to-end C++/VM/native coverage exercises the semantic-product boundary rather than only snapshot helpers.
- The remaining AST dependency is limited to syntax-faithful provenance data such as spans/debug mapping, with that
  boundary documented explicitly.

Planned lowerer entrypoint cutover:
- `prepareIrModule` and `IrLowerer::lower` should treat the semantic product as the canonical lowering input rather
  than a secondary artifact attached to a raw `Program`.
- The top-level cutover should establish one production entry contract:
  - semantic success publishes a semantic product plus AST-backed provenance
  - IR preparation consumes the semantic product directly
  - any compatibility path is temporary adapter code only
- That first entrypoint seam is already live at `prepareIrModule(...)`, and the production `IrLowerer::lower(...)`
  path is now cut over as well.
- The remaining residual lowerer-entry boundary is narrower:
  - top-level lowerer entry/effect validation now prefers semantic-product callable summaries, while nested expression-transform checks remain syntax-owned
  - native-backend software-numeric rejection remains a backend policy preflight, while residual runtime-reflection paths are gated by `IrBackendProfiles` during IR preparation before backend-specific lowering runs
  - import/layout setup now treats its remaining `Definition*` map as an explicitly AST-owned provenance/body inventory for field statements, namespace prefixes, and recursive layout traversal; the import-alias table is pinned as a syntax-owned shorthand layer derived from spelled `import` directives plus wildcard expansion; top-level struct layout enumeration already prefers semantic-product type inventories, so the only live raw-`Program` seam left there is AST-owned field/provenance traversal inside layout computation
  - helper/local setup now prefers semantic-product `on_error`, callable, binding, and return facts wherever lowering-facing meaning exists; entry completeness validation checks `on_error` coverage from published callable summaries and `onErrorFacts` instead of rebuilding a handler map from raw AST transforms; callable-definition lowering enumerates semantic-product definitions and keeps the AST only as the executable body/provenance source, while statement-call lowering remains intentionally syntax-owned because it walks spelled statement bodies directly and the math-import probe remains syntax-owned import shorthand during uninitialized setup
  - source-map finalization consumes a narrow `FunctionSyntaxProvenance` map for fallback function line/column/source-unit data rather than the full `Definition*` inventory
  - no raw-`Program` lowerer entry remains; the old public compatibility overload used by tests/helpers is retired
- The entrypoint boundary should make ownership explicit:
  - lowering-facing meaning comes from the semantic product
  - syntax-faithful provenance, spans, and source reproduction remain AST-owned
  - no new lowerer entrypoint may silently fall back to AST-side semantic re-derivation once the semantic product is
    available
- The adapter may temporarily translate legacy callers onto the new entry contract, but it should not become a
  long-term second lowering API.
- Completion criteria:
  - production entrypoints pass semantic-product data into `prepareIrModule` and `IrLowerer::lower`
  - no raw-`Program` lowering entry remains on public production or testing helper paths
  - backend-facing tests can exercise the lowerer boundary without depending on AST-owned semantic caches
  - future lowerer setup work can assume semantic-product input instead of mixed AST/semantic entry state

Planned lowerer entry-setup handoff:
- `IrLowerer` entry setup should consume resolved call targets from the semantic product instead of re-deriving callee
  paths, helper-family choices, or canonical-versus-same-path decisions from AST state.
- Entry setup should treat the semantic product as the authoritative source for:
  - the resolved entry definition/call target
  - canonical helper-routing decisions already made during semantics
  - entry-facing binding/type facts needed before statement/expression lowering begins
- The lowerer may still consult AST-backed provenance for source locations/debug mapping, but it should not perform a
  second semantic resolution pass to discover which callable path was selected.
- Completion criteria:
  - entry setup can initialize lowering from semantic-product call-target data alone
  - helper-shadow and canonical-path behavior matches current semantics without lowerer-side re-resolution
  - the old AST-derived entry target path is removed or reduced to a temporary adapter boundary only

Planned lowerer type/binding handoff:
- Current status: lowerer parameter/local setup, helper-parameter setup, temporary call
  binding setup, helper-result binding setup, lowered entry-count setup, and entry
  return/result setup now prefer published semantic-product binding and return facts.
  Vector/map collection binding setup now also prefers published
  semantic-product specialization facts for kind, element, key/value, and
  helper-family classification. The
  remaining lowerer-generated synthetic locals and raw IR temporaries stay outside the
  semantic-product surface because they have no stable source-owned semantic identity.
  Native `pick(...)` arm dispatch, aggregate-result inference, and payload
  binding now route through the shared sum-variant semantic-product helpers.
  Native sum constructor, `Result.ok`, and initializer-matching selection now use
  published sum-variant payload metadata for selected payload storage shape on
  the semantic-product path. Native `try(...)` lowering for stdlib Result sums
  now uses published sum-variant metadata for Result payload storage and tag
  decisions on the semantic-product path.
- After entry setup is cut over, lowering should consume binding metadata from the semantic product instead of
  re-running helper-family checks, fallback type inference, or binding-shape recovery against the AST.
- The semantic product should publish, per lowered binding/expression site:
  - resolved binding/result types after semantic canonicalization
  - whether the site is value, reference, pointer, omitted-envelope, or query-backed
  - the resolved helper-family choice when old-surface versus canonical helper routing matters for lowering
  - enough provenance to map lowered temporaries and storage decisions back to source-owned spans and stable semantic
    identities
- Lowerer type setup should treat that metadata as authoritative for:
  - local storage layout and temp allocation decisions
  - expression result typing used by `prepareIrModule` and statement lowering
  - helper-specific binding behavior that currently depends on repeated AST pattern checks
- AST-backed semantic caches may still be consulted temporarily through the migration adapter, but the cutover target is
  that type/binding setup becomes a pure semantic-product consumer rather than a second inference pass.
- Completion criteria:
  - lowerer binding/type setup no longer re-derives helper routing or binding classification from AST structure
  - direct, helper-shadowed, and canonical helper paths produce the same lowered type/binding behavior when driven only
    from semantic-product metadata
  - lowering tests that currently need AST-semantic snapshots for binding/type facts can move to the semantic-product
    dump or conformance surface instead

Planned lowerer effect/struct-layout handoff:
- Current status: entry return/result setup, entry effect/capability mask setup, and lowered
  callable effect/capability setup now consume published semantic-product return facts and
  callable summaries. Lowerer import/layout setup now also prefers semantic-product type
  metadata for struct-like classification, explicit alignment, struct-name inventory, and
  ordered struct field name/envelope/type metadata instead of reconstructing
  `LayoutFieldBinding` order from raw field statements. Top-level struct layout enumeration
  also now prefers semantic-product type inventories instead of scanning raw definition order.
  Enums already rewrite to struct form before lowering, so there is no separate enum-specific
  lowerer metadata seam left to cut over.
- The remaining lowerer-owned `Definition*` inventory in import/layout setup is now pinned as
  AST-owned provenance/body access only:
  - walking original field statements for syntax-owned qualifiers, visibility, and field-local alignment
  - reading namespace prefixes and other source-owned context needed by recursive layout traversal
  - preserving one direct path back to source spans/debug provenance while layout planning is still partly AST-owned
- That residual field/provenance traversal is now explicitly the intended long-term boundary for
  layout planning rather than a deferred semantic-product seam:
  - `public/private`, `static`, `pod/handle/gpu_lane`, and field-local alignment remain syntax-owned
  - top-level struct inventories, explicit struct alignment, and field type/envelope facts remain semantic-product-owned
  - lowerer layout code should keep consulting AST field statements only for those syntax-owned qualifiers and
    provenance handles, not to re-derive lowering-facing type/layout facts
- Import aliases are also pinned as syntax-owned rather than semantic-product-owned:
  - the alias table is a short-name convenience layer built from spelled `import` directives
  - wildcard imports still expand against raw definition names to produce that shorthand map
  - lowerer/import-layout consumers should keep using canonical semantic-product full paths for lowering facts and
    consult the alias table only when resolving source-spelled type names that have not yet been canonicalized
- With those ownership decisions pinned, there is no remaining semantic-product import/layout inventory seam; the
  residual AST reads are intentional syntax/provenance inputs to layout planning.
- After the lowerer consumes semantic-product entry targets and binding metadata, effect/capability setup and
  struct-layout setup should consume published semantic-product facts instead of re-reading AST annotations,
  transform-produced helper state, or struct-shape details directly from canonicalized syntax.
- The semantic product should publish, for every lowered callable and layout-sensitive type:
  - resolved effect/capability summaries exactly as validated by semantics
  - struct/layout metadata needed for IR storage, offsets, and aggregate lowering
  - canonical collection/layout classifications that currently have to be rediscovered from AST spellings
  - enough provenance to report layout-sensitive diagnostics and backend errors against the original source-owned spans
- Lowerer effect/capability setup should treat that metadata as authoritative for:
  - call-effect masks and capability gating
  - execution metadata required by backend validation and runtime/debug surfaces
  - any helper-family effect distinctions that currently depend on AST re-inspection
- Lowerer struct-layout setup should treat that metadata as authoritative for:
  - field ordering, offsets, and size/alignment facts used during IR storage planning
  - aggregate classification for structs, collections, and wrapper-owned runtime shapes
  - backend-facing layout facts that should no longer depend on AST transform order
- The next concrete migration seam is therefore:
  - explicitly leave syntax-owned field qualifiers (`public/private`, `static`,
    `pod/handle/gpu_lane`, field-local alignment) on the AST until there is a separate reason
    to publish them
- Temporary adapter code may still translate semantic-product layout/effect facts into existing lowerer inputs during
  migration, but the target state is that lowering no longer recomputes those facts from AST/transforms.
- Completion criteria:
  - lowerer effect/capability setup no longer reads AST state to rebuild validated effect metadata
  - lowerer struct-layout setup no longer reclassifies layout-sensitive types from AST spellings or transform output
  - C++/VM/native lowering all consume the same published effect/layout facts through one semantic-product path

Completed lowerer alias-fallback removal:
- Lowering no longer performs lowerer-local stdlib/helper alias fallback for direct-call targets, receiver/method-call targets, or collection/builtin bridge routing on the semantic-product-aware path.
- The semantic product now publishes resolved call/helper targets in the exact canonical-or-same-path form lowering needs, so alias-sensitive helper routing is decided during semantics and merely consumed during lowering.
- Same-path helper-shadow behavior is preserved by semantic-product targets rather than lowerer-local recovery.
- Backend-facing conformance and source-lock tests pin that production lowering path, while explicit raw helper tests still keep alias-aware behavior only where they are intentionally exercising non-semantic-product compatibility helpers.

Current residual semantic fallback audit:
- Migrated/fail-closed in the initial audit slice: semantic-product direct-call resolution no longer falls
  back through raw `defMap` or import-alias resolution when published direct-call/bridge facts are missing;
  it returns syntax-only spelling and lets downstream semantic-product validation/lowering report missing facts.
- Completed in TODO-4225: production lowerer entry now runs direct-call, bridge-path, and method-call
  semantic-product coverage checks before helper dispatch, and bridge-path coverage no longer reclassifies
  missing bridge facts through a published-lookup fallback. Inline/native tail-dispatch classifiers and
  collection helper dispatch guards are lowering-owned compatibility dispatch after those routing facts validate.
- Completed in TODO-4232: binding/type/effect/layout lowerer fallback closure now runs binding,
  local-auto, call-routing, result, effect, on_error, type metadata, and struct-field metadata
  semantic-product coverage before backend setup. Struct layout rejects missing type metadata, stale
  struct provenance, and missing field facts instead of recovering layout-facing meaning from raw AST
  transforms once a semantic product is present. Syntax/provenance-owned field qualifiers and body
  traversal remain AST-owned.
- Completed in TODO-4233: backend adapter and source-composition cleanup removed the remaining return-fact
  definition-path fallback from `findSemanticProductReturnFact(...)`; definition-path indexes stay inspection
  metadata only, and raw call/path helpers remain syntax/provenance-owned compatibility APIs.
- Syntax/provenance-owned: source spans, debug/source maps, syntax reproduction, function bodies/statements used
  for emission traversal, import shorthand maps, and field qualifiers/visibility/alignment.

Current inspection-surface relationship:
- `pre_ast`: post-import, post-text-transform source text
- `ast`: parser-owned syntax tree before semantic canonicalization
- `ast-semantic`: canonicalized AST after semantic rewrites/inference, still syntax-oriented
- `semantic-product`: lowering-facing resolved facts, types, targets, effects, and provenance handles
- `ir`: canonical lowered IR after semantic-product consumption

Current semantic validation pass manifest:
- The authoritative `Semantics::validate` pass order is
  `semanticValidationPassManifest()` in `include/primec/SemanticValidationPlan.h`.
  The manifest records each pass name, executable pass id, pass kind,
  input/output ownership, action (`MutatesAst`, `ValidatesOnly`, or
  `PublishesFacts`), and whether the pass is a compatibility rewrite.
- `Semantics::validate` executes that manifest directly from pre-validator
  rewrites through validator fact collection, post-validator canonicalization,
  stable node-id assignment, and semantic-product publication. Adding a pass
  requires a manifest entry with a unique executable pass id and switch runner
  coverage; duplicate ids fail focused manifest coverage and new ids without
  switch coverage fail the warning-as-error build contract.
- Compatibility rewrites are explicitly marked in the manifest. Core
  canonicalization passes, template monomorphization, validator fact collection,
  omitted-struct initializer rewriting, semantic-node-id assignment, and final
  semantic-product publication are distinct pass kinds/ownership boundaries.
- Type-resolution analysis still uses its smaller preparation path; if that path
  needs another semantic rewrite, add it intentionally rather than assuming it
  inherits the full validation manifest.

Current IR preparation phase manifest:
- `prepareIrModule(...)` publishes its backend-facing phase contract through
  `irPreparationPhaseManifest()` in `include/primec/IrPreparation.h`.
  The manifest names the semantic-product preflight, semantic-product-to-IR
  lowering, lowered-IR validation, optional call inlining, post-inline
  validation, and lowered-AST body release phases.
- Each entry records required inputs, input ownership, output ownership,
  mutation action, invalidation notes, consumer notes, and whether the phase is
  optional. In particular, `inline-ir-calls` is marked as an optional IR
  mutation that invalidates the prior validation result, and
  `validate-inlined-ir` is the required consumer before backend emitters see
  inlined IR.
- Future changes that add, remove, split, or reorder IR preparation lowering,
  validation, inline, or cleanup phases must update the manifest and focused
  manifest tests in the same change. Any phase that mutates IR or releases AST
  storage must state which previous result is invalidated and which later phase
  or backend consumer is allowed to use the new output.
- The old private-source assertion that read `src/IrPreparation.cpp` only to
  prove the semantic-product preflight has been replaced by public behavior
  coverage and this manifest contract. Broader compile-pipeline architecture
  source locks remain temporary migration guards until equivalent public
  contracts exist for each remaining handoff.

Current semantic phase handoff conformance gate:
- The release doctest suite includes a compile-pipeline handoff gate that runs
  imported, transform-normalized source through validation, semantic-product
  publication, published module artifacts/lookups, and IR preparation.
- The same gate deliberately removes published direct-call handoff facts before
  IR preparation and expects a deterministic lowerer failure, proving stale or
  missing semantic-product facts fail closed at the compile-pipeline boundary.

Current semantic budget and worker-parity release gates:
- `PrimeStruct_graph_budget` runs the graph-budget checker against the checked-in
  type-graph baseline and reports the offending graph counters when thresholds
  drift.
- `PrimeStruct_semantic_memory_benchmark` collects the semantic memory artifact
  set, and `PrimeStruct_semantic_memory_trend` depends on that artifact target to
  enforce the semantic memory budget policy in normal release validation.
- `PrimeStruct_semantic_memory_definition_worker_parity` runs the semantic
  memory benchmark across definition-validation worker modes. The helper fails
  if the semantic-product dump hash differs or if the semantic-product
  index-family counters differ, so fact drift is reported with the single-worker
  and dual-worker counters instead of as an opaque benchmark failure.
- Focused 1/2/4-worker release doctests pin semantic-product dump stability,
  diagnostic stability, and semantic-product index-family parity for the
  compile-pipeline worker-count stress scenarios.

Current semantic-product dump contract:
- One deterministic module/program view per compile pipeline success, positioned after `ast-semantic` and before `ir`.
- The dump should expose lowering-facing facts directly: resolved call targets, binding/result types, effects or
  capability summaries, vector/map/`soa_vector` collection specializations, struct/layout metadata, and stable
  provenance handles back to AST-owned spans/ids.
- The dump should not duplicate full syntax trees. Raw source text, token order, and syntax-only transforms stay in
  `pre_ast` / `ast` / `ast-semantic`.
- Ordering must be stable across runs: modules, definitions, bindings, and diagnostics should be emitted in one
  canonical order so golden files do not depend on hash iteration or incidental traversal order.
- The text form should be concise enough for golden tests and human inspection, but complete enough that lowering
  tests can assert semantic facts without falling back to AST snapshots.
- Once this dump exists, lowering-facing tests should prefer it over `ast-semantic` whenever they are asserting
  resolved targets, inferred types, effects, or helper-routing facts.

Current semantic-product API/version contract:
- `SemanticProductContractVersionCurrent` is `SemanticProductContractVersionV3`.
- Version 3 adds `arrayExtentFacts`, a semantic-product-owned dump/API family for array element-count facts on
  local `array<T>` values, `array<T>` / `Reference<array<T>>` parameters, and direct `count(...)` expressions.
- Version 2 is an API-only contract bump; it does not intentionally change the `semantic-product` text dump shape.
- Version 2 adds fact-family ownership metadata through `semanticProgramFactFamilyInfos()`,
  `semanticProgramFactFamilyOwnership(...)`, `semanticProgramFactFamilyIsSemanticProductOwned(...)`, and
  `semanticProgramFactFamilyIsAstProvenanceOwned(...)`.
- Fact-family ownership is split into:
  - `SemanticProduct`: lowering-facing facts owned by the semantic product, including resolved call/method/bridge
    targets, callable summaries, type/field metadata, collection specializations, array extent facts,
    binding/return facts, local-auto facts, query/try facts, `on_error` facts, and lowerer preflight facts.
  - `AstProvenance`: syntax/body/provenance inventory that remains paired with the AST, currently source/import
    inventories plus definition/execution body inventories.
  - `DerivedIndex`: indexes and intern tables derived from semantic-product facts for deterministic lookup and
    module grouping.
- Consumers should use the ownership helpers before deciding whether a new assertion belongs on the semantic-product
  surface or on an AST/provenance helper. Future dump/API shape changes must either keep the versioned contract
  byte-stable or update this section with migration notes and expected golden-output diffs.

Planned semantic-product unit/golden suite:
- Current status: the exact semantic-product formatter golden now pins resolved call/helper targets,
  binding/result facts, effect/capability plus struct/layout metadata, and the explicit
  `provenance_handle=<id> source="line:column"` provenance surface carried by lowering-facing semantic-product facts.
  Pipeline-facing/backend conformance now covers semantic-product facts beyond
  the formatter golden.
- Keep one narrow golden corpus focused on exported lowering facts rather than full pipeline behavior.
- Prefer small representative programs that pin one fact family at a time:
  - resolved call/helper targets
  - binding/result type facts
  - vector/map/`soa_vector` collection specialization facts
  - effect/capability summaries
  - struct/layout metadata
  - provenance handles back to AST-owned ids/spans
- Golden output should be deterministic and compact enough that failures identify semantic-product drift directly,
  without requiring AST-snapshot interpretation.
- This suite should validate the semantic-product formatter/inspection surface itself; end-to-end C++/VM/native
  conformance stays in separate compile-pipeline coverage.
- Existing snapshot-helper tests that are really asserting lowering-facing facts should migrate here once the semantic
  product is available, so there is one canonical golden surface for those facts.

Planned pipeline-facing semantic-product conformance matrix:
- Keep a separate compile-pipeline matrix for consumers that cross the semantic/lowering boundary instead of extending
  the narrow semantic-product golden suite to cover full backend behavior.
- Current status: pipeline-facing inspection-order coverage now captures `ast-semantic`, `semantic-product`, and `ir`
  from the same compile-pipeline source and pins that the semantic-product dump is the lowering-facing boundary between
  syntax-shaped AST output and backend-facing IR output. The public pipeline helper now also covers the C++/VM/native
  lowering paths end to end, and backend-consumer conformance pins direct-call/method-call plus
  collection-specialization semantic facts on all three backend families. Missing collection-specialization facts for
  collection bindings now fail closed through the lowerer semantic-product completeness matrix.
- The first pipeline-facing cases should prove inspection-surface order and consistency:
  - `ast-semantic` remains syntax-shaped.
  - the semantic-product dump exposes lowering-facing facts directly.
  - `ir` remains the first backend-facing dump after semantic-product consumption.
- Initial end-to-end cases should pin one lowering-facing fact family at a time across the normal compile pipeline:
  - resolved helper/call targets
  - inferred binding/result types
  - effect/capability summaries consumed by lowering
  - struct/layout facts consumed by lowering
- Coverage should stay split by intent:
  - semantic-product golden tests validate formatter and deterministic fact ordering.
  - pipeline-facing conformance tests validate that compile-pipeline entrypoints, dump stages, and C++/VM/native
    lowering all consume the same semantic-product facts.
- When the semantic-product stage exists, tests that currently compare AST snapshots only because no lowering-facing
  inspection surface exists should move to the semantic-product dump or the pipeline-facing conformance matrix,
  depending on whether they are formatter-facing or backend-facing.
- Exit criteria for this test layer:
  - semantic-product dump and `ir` dumps can be compared in the same scenario without AST-only re-derivation.
  - C++/VM/native compile-pipeline cases prove that lowering uses published semantic-product facts rather than hidden
    AST-side caches.
  - graph-backed SCC, condensation-DAG, and type-resolution snapshot assertions now live in
    `primec/testing/SemanticsGraphHelpers.h`, while `primec/testing/SemanticsValidationHelpers.h`
    is reduced to syntax-owned canonicalization/assertion helpers only.

Planned testing-only snapshot removal contract:
- The repository should converge on one canonical lowering-facing inspection surface: the published semantic product and
  its deterministic dump, backed by compile-pipeline conformance.
- Testing-only semantic snapshot plumbing may remain temporarily while semantic-product builder slices, dumps, and
  backend conformance are incomplete, but it should be treated as transitional compatibility scaffolding rather than a
  second permanent inspection API.
- Snapshot-only helpers should be removed in this order:
  - migrate lowering-facing fact assertions onto the semantic-product dump or pipeline-facing conformance matrix
  - leave AST/testing helpers only where the asserted fact is intentionally syntax-owned or provenance-owned
  - delete redundant snapshot transport/plumbing once no lowering-facing test depends on it
- `primec/testing/SemanticsValidationHelpers.h` and related helper surfaces should survive only for syntax-owned or
  provenance-owned assertions that are intentionally not part of the semantic product.
- Completion criteria:
  - lowering-facing tests no longer require testing-only semantic snapshot transport
  - one semantic-product inspection surface remains canonical for targets, inferred types, helper routing, layout
    facts, and graph-backed inference facts
  - any remaining AST/testing helpers are explicitly limited to syntax/provenance assertions and do not duplicate
    lowering-facing semantic facts

Planned testing-helper migration contract:
- Public testing helpers should be split by ownership boundary rather than by current implementation history.
- Current status: `primec/testing/CompilePipelineDumpHelpers.h` now provides the semantic-product-aware dump helper for
  compile-pipeline boundary tests plus the shared backend-conformance helper, and lowering-facing dump/compile-pipeline
  assertions now route through that helper surface rather than direct CLI shell dumps or ad hoc test-local
  compile-pipeline wiring. The earlier public prepared-IR helper has been internalized behind the backend-conformance
  path, while the remaining one-off prepared-IR setup now lives in local test helpers instead of the public testing
  surface. Backend-facing assertions for C++/VM/native now also route through the same helper surface via shared
  backend conformance helpers, and the old generic dump-compatibility entrypoint is gone. Entry-arg, pointer,
  pointer-numeric, GPU, variadic basics/results, method-call/argv, and core struct/control-flow serialization IR unit
  tests now also lower through a semantic-product-aware local helper instead of the raw-`Program` overload directly.
  The conversions-heavy remainder plus the remaining serialization-calls, serialization-control-flow-metadata, and
  validation IR families are now cut over as well, and the raw overload plus its last fallback-parity case are gone.
  `primec/testing/SemanticsValidationHelpers.h` is now reduced to syntax-owned canonicalization/assertion helpers,
  while graph/type-resolution snapshots live in `primec/testing/SemanticsGraphHelpers.h`. The old diagnostic-string
  capture layer, serialized-IR transport, and dedicated type-resolution parity fixture header are now gone, so no
  separate testing-only semantic snapshot transport layer remains; the helper migration cleanup is complete and the remaining public
  backend-oriented helpers
  (`primec/testing/CompilePipelineDumpHelpers.h`, `primec/testing/EmitterHelpers.h`, and
  `primec/testing/IrLowererTestHelpers.h`) are now pinned as intentional stable testing APIs rather than temporary
  compatibility wrappers.
- `primec/testing/SemanticsValidationHelpers.h` and related helpers should migrate in this order:
  - move lowering-facing assertions onto semantic-product dump helpers or pipeline-facing conformance helpers
  - retain AST-facing helpers only for syntax-owned, provenance-owned, parser-facing, or canonicalization-facing checks
  - delete or narrow helper entrypoints whose only purpose was exposing lowering facts before the semantic product
    existed
- The replacement helper surface should make the distinction explicit:
  - semantic-product helpers for resolved targets, inferred types, helper routing, effect/layout facts, and graph-backed
    inference facts
  - AST/syntax helpers for spans, syntax-faithful dumps, parser/transform assertions, and source reproduction
- Migration should prefer moving existing tests rather than creating duplicate assertion paths that keep both helper
  surfaces alive for the same lowering-facing fact.
- Completion criteria:
  - lowering-facing tests no longer need public snapshot helpers from `primec/testing/SemanticsValidationHelpers.h`
  - any remaining public testing helpers in that surface are clearly syntax/provenance scoped
  - helper consumers can tell from the API which facts are semantic-product-owned versus AST-owned

Planned diagnostic ordering contract for semantic-product identities:
- Diagnostics that survive semantic validation should be attachable to stable semantic-product node identities or
  deterministic sort keys before any later parallel validation work begins.
- The ordering contract should not depend on emission time alone. Each lowering-facing diagnostic should carry enough
  ordering data to be merged deterministically after parallel or staged validation.
- A complete ordering key should be able to distinguish at least:
  - owning module/import order
  - owning definition order within that module
  - semantic node identity or local sort key within that definition
  - diagnostic class/phase tie-breakers when multiple diagnostics attach to the same semantic node
- Provenance remains syntax-owned:
  - source spans and syntax-faithful reproduction still come from AST-backed ids/spans
  - semantic-product node ids/sort keys provide merge order and stable semantic attachment, not replacement source
    locations
- The contract should allow deterministic merging across:
  - single-threaded staged validation passes
  - future parallel validation tasks
  - semantic-product dump generation and golden output
- Implementation is complete only when diagnostics emitted from equivalent semantic facts retain the same order across
  repeated runs and across future parallel merge boundaries, without depending on hash iteration or worker scheduling.

Planned validation-context split contract:
- Semantic-product construction should move toward explicit per-definition validation contexts instead of relying on
  validator-global mutable caches.
- A per-definition validation context should own only the state needed to validate one definition or execution body:
  - current definition identity and canonical path
  - definition-local binding/type/effect facts
  - graph-backed local/query/`try(...)` working state for that definition
  - deterministic diagnostic buffering tied to that definition
- Validator-global state should be reduced to shared read-only inputs and canonical registries, such as:
  - imported/declared definition maps
  - canonical type/layout registries
  - immutable compile-pipeline configuration and options
- The split should eliminate hidden coupling where one definition’s validation mutates caches later read as semantic
  truth for another definition without an explicit handoff.
- Temporary compatibility adapters are acceptable during migration, but they must make the context boundary explicit:
  global registries flow into per-definition contexts, and validated semantic facts flow back out as published results.
- Completion criteria:
  - semantic-product builder slices can run from explicit per-definition contexts
  - validator-global fields are no longer required to reconstruct lowering-facing semantic facts after validation
  - repeated validation of the same definition under unchanged global inputs is deterministic and context-local
- This split is a prerequisite for later parallel validation because worker tasks need isolated validation state and one
  deterministic merge surface for published semantic facts and diagnostics.
- Current definition-worker validation returns `SemanticDefinitionWorkerResultBundle` as that named merge surface for
  partition identity, structured diagnostics, validation counters, callable-summary slices, migrated `on_error` facts,
  and worker-local publication string snapshots. Follow-up work moves the remaining semantic-product fact families into
  that same bundle before publication stops pulling them from validator-owned side channels.

Structured diagnostic-sink contract:
- `SemanticsValidator` publishes fatal diagnostics through one `SemanticValidationResultSink` that owns the adapter
  between structured diagnostic records and the legacy first-error string surface.
- The sink carries enough structure for later semantic-product publication and deterministic merging:
  - severity
  - stable diagnostic code/category
  - primary message
  - optional notes
  - AST-backed provenance handles/spans
  - stable semantic node identity or sort key when the diagnostic is attached to lowering-facing facts
- In the current single-threaded path, the first-error rule remains unchanged:
  - validation still stops at the first emitted fatal diagnostic for the active entrypoint
  - the legacy error string remains an output adapter and scratch surface, not the validator's diagnostic publication
    boundary
- The sink should separate production from policy:
  - validators emit structured diagnostics into the sink
  - the compile pipeline decides whether to stop immediately, buffer, dump, or publish them
- Current compatibility adapters convert structured diagnostics back to the old single-error surface at the boundary
  while preserving existing byte-stable messages and collected-diagnostic ordering.
- Follow-up work may add stable diagnostic codes, severity modeling, and merge keys to the same sink/result surface
  without reintroducing string-only validator publication state.

Diagnostic stability tiers:
- Each public diagnostic field is classified independently:
  - `stable` means the field is a user/tooling contract. Changing it requires
    an explicit docs update, focused source tests, and a migration note when
    downstream tools could observe the difference.
  - `implementation` means the field may change as compiler internals move.
    Tests may assert it for local regression coverage, but those assertions do
    not make the field a public compatibility promise.
- Diagnostic `code` strings such as `PSC1003` are stable identifiers once
  assigned. The current contract is source-locked by
  `diagnosticStabilityContract(...)`.
- Parser diagnostics are the first phase with a full stability classification:

| Phase/code | Field | Tier | Contract |
| --- | --- | --- | --- |
| Parser / `DiagnosticCode::ParseError` (`PSC1003`) | code | stable | `PSC1003` identifies parser/AST-builder failures in structured output. |
| Parser / `DiagnosticCode::ParseError` (`PSC1003`) | message | stable | The normalized structured `message` omits the legacy trailing `at line:column` suffix and preserves the parser's selected diagnostic text. |
| Parser / `DiagnosticCode::ParseError` (`PSC1003`) | primary span | stable | `primary_span.file`, `line`, `column`, `end_line`, and `end_column` identify the source-unit-mapped parser anchor chosen by the parser. |
| Parser / `DiagnosticCode::ParseError` (`PSC1003`) | notes | stable | CLI structured output includes `stage: parse`. Parser diagnostics currently do not publish related spans. |
| Semantic unknown-call target / `DiagnosticCode::SemanticError` (`PSC1005`) | code | stable | `PSC1005` identifies semantic failures in structured output; this row promotes only messages beginning with `unknown call target: `. |
| Semantic unknown-call target / `DiagnosticCode::SemanticError` (`PSC1005`) | message | stable | The normalized structured `message` has the exact shape `unknown call target: <callee>`, where `<callee>` is the semantic call target text selected by resolution. |
| Semantic unknown-call target / `DiagnosticCode::SemanticError` (`PSC1005`) | primary span | stable | `primary_span.file`, `line`, `column`, `end_line`, and `end_column` identify the source-unit-mapped call expression anchor. |
| Semantic unknown-call target / `DiagnosticCode::SemanticError` (`PSC1005`) | notes | stable | CLI structured output includes `stage: semantic`. When the validator has an enclosing definition or execution source location, `related_spans` include the source-unit-mapped note label `definition: <path>` or `execution: <path>`. |
| Lowerer variadic reference-pack forwarding / `DiagnosticCode::LoweringError` (`PSC2001`) | code | stable | `PSC2001` identifies IR-lowering/backend preparation failures in structured output; this row promotes only the reference-pack forwarding diagnostic named below. |
| Lowerer variadic reference-pack forwarding / `DiagnosticCode::LoweringError` (`PSC2001`) | message | stable | The normalized structured `message` is exactly `variadic args<Reference<T>> requires reference values or location(...) forwarding`. |
| Lowerer variadic reference-pack forwarding / `DiagnosticCode::LoweringError` (`PSC2001`) | primary span | stable | `primary_span.file`, `line`, `column`, `end_line`, and `end_column` identify the source-unit-mapped first forwarded pack argument when present; otherwise the call expression is used. |
| Lowerer variadic reference-pack forwarding / `DiagnosticCode::LoweringError` (`PSC2001`) | notes | stable | CLI structured output includes the backend note `backend: <tag>` for the selected IR backend. This diagnostic currently publishes no related spans. |

- Plain text prefixes, caret snippets, and parser recovery choices beyond the
  selected stable primary span remain implementation tier unless a focused
  contract test promotes them.
- Semantic diagnostics outside the unknown-call target family, and other
  non-parser diagnostic message, span, and notes fields, remain implementation
  tier until a later slice classifies that family explicitly.
