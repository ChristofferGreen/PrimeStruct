# PrimeStruct Plan

PrimeStruct is built around a simple idea: program meaning comes from two primitives—**definitions** (potential) and
**executions** (actual). Both map to a single canonical **Envelope** in the AST; executions are envelopes with an
implicit empty body. Compile-time transforms rewrite the surface into a small canonical core. That core is what we
target to C++, GLSL, and the PrimeStruct VM. It also keeps semantics deterministic and leaves room for future tooling
and visual editors.

At a glance:
- One canonical envelope model underlies both definitions and executions.
- Surface syntax is convenience only; transforms rewrite it into a smaller deterministic core.
- Backends target the shared canonical representation rather than reinterpreting surface syntax.

### Source-processing pipeline
1. **Import resolver:** first pass walks the raw text and expands every `import<...>` source entry so the compiler
   always works on a single flattened source stream.
2. **Text transforms:** the flattened stream flows through ordered token-level transforms (operator sugar, collection
   literals, implicit suffixes, project-specific macros, etc.). Text transforms apply to the entire envelope (transform
   list, templates, parameters, and body). Executions are treated as envelopes with an implicit empty body, so the same
   rule applies. Text transforms may append additional text transforms to the same node.
3. **AST builder:** once text transforms finish, the parser builds the canonical AST.
4. **Template & semantic resolver:** monomorphise templates, resolve namespaces, and apply semantic transforms (effects)
   so the tree is fully resolved and contains only concrete envelopes.
5. **IR lowering:** emit the shared SSA-style IR only after templates/semantics are resolved; the base-level tree
   contains no templates or `auto`, and every backend consumes an identical canonical form.

Pipeline operating rules:
- Each stage halts on error and exposes `--dump-stage=<name>` so tooling/tests can capture the text/tree output just
  before failure.
- The current dump surface is `pre_ast`, `ast`, `ast-semantic`, `semantic-product`, and `ir`. `semantic-product`
  is the lowering-facing inspection surface between `ast-semantic` and `ir`, so tooling can inspect resolved semantic
  facts without forcing users to infer them from the canonicalized AST or from lowered IR.
- `--collect-diagnostics` enables stable multi-error reporting for parse-stage failures, semantic build-map failures
  (duplicate-definition/import/invalid-transform/return-kind), semantic definition/execution pass failures across
  independent definitions/executions, and multiple intra-body call diagnostics inside a single definition/execution
  body (unresolved targets plus resolved-call argument-shape/type and flow/effect errors such as duplicate/unknown
  named arguments, count mismatches, inferable parameter type mismatches, and effect/capability subset failures).
  Other validation inside a single definition/execution body remains fail-fast.
- Text transforms are configured via `--text-transforms=<list>`. The default list enables `collections`, `operators`,
  `implicit-utf8` (auto-appends `utf8` to bare string literals), and `implicit-i32` (auto-appends `i32` to bare
  integer literals). Order matters: `collections` runs before `operators` so map literal `key=value` pairs are
  rewritten as key/value arguments rather than assignment expressions.
- Semantic transforms are configured via `--semantic-transforms=<list>`.
- `--transform-list=<list>` is an auto-deducing shorthand that routes each transform name to its declared phase (text
  or semantic); ambiguous names are errors.
- Use `--no-text-transforms`, `--no-semantic-transforms`, or `--no-transforms` to disable transforms and require
  canonical syntax.
- `--ir-inline` enables a post-validation IR inlining optimization pass before VM/native/IR emission.

### Language levels (0.Concrete → 3.Surface)
PrimeStruct is organized into four language levels. Each higher level desugars into the level below it.
- **0.Concrete:** fully explicit envelopes only (definitions may omit an empty parameter list; executions still require
  parentheses). Definition transforms use prefix placement only (`[transforms] name(...) { ... }`). No text transforms,
  no templates, no `auto`. Explicit `return<T>`, explicit literal suffixes, canonical calls (`if(cond, then(){...},
  else(){...})`, `loop/while/for` as calls).
- **1.Template:** canonical syntax plus explicit templates (`array<i32>`, `Pointer<T>`, `convert<T>(...)`). No `auto`.
- **2.Inference:** canonical syntax plus `auto`/omitted envelopes. Implicit template parameters are resolved per call
  site, then lowered to explicit templates.
- **3.Surface:** surface syntax and text transforms (operator sugar, collection literals, indexing sugar, `if(...) {}`
  blocks, and `name() [transforms] { ... }` definition sugar) that rewrite into canonical forms.

### Compilation model (v1)
- **Whole-program by default:** `import` expansion produces a single compilation unit, and semantic resolution runs over
  that full unit; implicit-template inference may use call sites anywhere in the expanded source. The v1 toolchain
  prioritises fast full rebuilds over incremental compilation.
- **Envelope stream boundary:** high-level features are lowered into the canonical envelope form, and backends consume
  this stable envelope stream. Emission can stream envelopes directly into IR/bytecode or native codegen without
  reintroducing surface syntax.
- **Deterministic emission:** canonicalization happens once, before backend selection, so all emitters see the same
  fully-resolved envelopes and produce consistent results.
- **Backend boundary policy:** all codegen modes consume canonical IR via
  `IrBackend`, including production aliases (`cpp`, `exe`, `glsl`, `spirv`)
  that resolve to canonical IR backend kinds before dispatch.
- **Semantic ownership boundary policy:** graph-backed inference facts, validator-local scratch state, and the published
  semantic product follow an explicit ownership split; benchmark-only legacy
  shadow comparisons must stay isolated from production
  lowering/publication paths.

### Planned Type-Resolution Graph
The planned graph-backed resolver is an internal semantics model built from the canonical AST after semantic transforms
and template monomorphization, and before IR lowering. Its purpose is to replace ad hoc inference ordering with one
deterministic dependency model without changing the public language surface.
- **Node kinds:** definition return-kind nodes model the inferred or validated result of each callable definition;
  call-site constraint nodes model argument/receiver/template constraints that connect a specific call to its callee;
  local `auto` constraint nodes model each local or omitted-envelope inference site that must converge to one concrete
  envelope.
- **Edge kinds:** dependency edges mean the source node must wait for or revisit the target node during solving because
  the target contributes information needed for convergence; requirement edges mean the source imposes a concrete
  compatibility requirement on the target and preserve diagnostic provenance even when they do not introduce a new
  solve-order dependency.
- **Cycle policy:** strongly connected components are the unit of solving. Cycles that represent real mutual
  dependencies are legal and are solved to a fixed point across the whole SCC. Cycles that remain ungrounded or collapse
  to contradictory requirements are illegal and must produce deterministic diagnostics instead of partial inference.
- **Deterministic ordering guarantees:** for a fixed canonical program, node creation order, node IDs, edge insertion
  order, SCC member ordering, condensation-DAG traversal order, and emitted diagnostics must be stable. Graph dumps and
  diagnostics must not depend on hash iteration order, parallel scheduling, or unrelated definitions elsewhere in the
  program.

Planned graph performance guardrails:
- The single-threaded graph path must establish a stable regression budget before optional parallel solve work starts.
- Performance tracking should pin at least these facts on representative corpora:
  - node and edge counts
  - SCC counts and largest SCC size
  - invalidation fan-out for local-binding, control-flow, initializer-shape, definition-signature, import-alias, and
    receiver-type edits
  - solve/revisit counts for local `auto`, query, `try(...)`, and `on_error` consumers
  - wall-clock solve time and peak graph memory for clean builds and incremental invalidation rebuilds
- Sustained perf coverage should include both micro cases (small focused invalidation/query scenarios) and larger
  end-to-end compile-pipeline corpora so the graph budget is not tuned only to toy cases.
- New graph consumers such as CT-eval, template inference, broader omitted-envelope inference, or parallel solve should
  not land without either fitting inside the existing regression budget or explicitly revising that budget in docs and
  perf coverage together.
- Parallel solve remains blocked until the single-threaded graph path has deterministic perf baselines, reproducible
  invalidation measurements, and coverage that can distinguish correctness regressions from acceptable budget updates.

Planned CT-eval boundary on the graph path:
- Compile-time evaluation must not grow a second hidden inference/cache model beside the graph-backed resolver.
- Each CT-eval consumer should do exactly one of two things:
  - consume graph-backed query/binding/result facts directly, or
  - stop at one explicit adapter boundary that is documented and tested as syntax-owned or semantic-product-owned.
- Current executable AST transform hook evaluation is the first pinned
  CT-eval boundary slice. It is syntax-owned because it runs before semantic
  validation publishes the semantic product, but it must still fail closed
  through the `ct-eval ast-transform adapter` instead of rediscovering call
  targets or accepting unsupported helper shapes from private cache state.
- The syntax-owned adapter currently exposes exactly one supported FunctionAst
  helper target, `/ct_eval/replace_body_with_return_i32`, mapped from
  `replace_body_with_return_i32(fn, value)`. Unknown helper targets, method or
  field helper targets, templated helper calls, body-argument helper calls, and
  helper inputs that do not use the hook's `FunctionAst` parameter are
  deterministic CT-eval diagnostics.
- The preferred end-state is direct graph consumption for:
  - compile-time receiver classification
  - call-target and template-argument resolution needed by CT-eval
  - local `auto`, query, `try(...)`, and `on_error` facts consulted during compile-time execution
- If any CT-eval path keeps a temporary adapter boundary during migration, that adapter must not invent new inference
  state; it may only translate already-published graph/semantic-product facts into the shape CT-eval currently expects.
- CT-eval parity coverage should pin both:
  - successful compile-time evaluation that depends on shared graph facts, and
  - deterministic diagnostics when graph-backed dependencies remain unresolved or contradictory.
- Template-inference migration and optional parallel solve remain blocked on this boundary being explicit, because both
  features would otherwise multiply hidden solver state across compile-time and runtime-oriented semantics paths.

Graph invalidation contract:
- Graph invalidation is explicit per edit family rather than inferred from
  incidental cache misses. The supported edit-family metadata is published by
  `typeResolutionGraphInvalidationContracts()` and mirrored in the type-graph
  testing snapshot/dump surface.
- Each edit family declares immediate invalidations, lazy revisits, diagnostic
  discard rules, and whether propagation stays definition-local or crosses
  definition/import boundaries:

| Edit family | Propagation | Immediate invalidations | Lazy revisits | Diagnostics discarded |
| --- | --- | --- | --- | --- |
| `local_binding` | definition-local | local-auto, binding, query, `try(...)`, and `on_error` nodes in the edited definition | binding/result consumers reached from the edited definition's dependency edges | diagnostics attached to the edited binding and dependent local facts |
| `control_flow` | definition-local | definition-return, branch-local binding, and control-dependent query nodes | return/result consumers whose dependency edges touch the edited control-flow region | branch reachability and return-consistency diagnostics in the edited definition |
| `initializer_shape` | definition-local | local-auto and call-constraint nodes owned by the edited initializer | initializer binding/result queries and downstream local facts in dependency order | initializer type, result-shape, and helper-resolution diagnostics for the edited site |
| `definition_signature` | cross-definition | definition-return nodes for the edited definition and directly dependent call sites | dependent call-constraint, binding, result, and return nodes across import boundaries | call-compatibility, return-contract, and template-argument diagnostics at dependent sites |
| `import_alias` | cross-definition | call-constraint nodes whose canonical path or helper-shadow choice used the alias | dependent helper-routing, binding, and result queries in deterministic path order | unresolved import, ambiguous helper, and alias-derived call diagnostics |
| `receiver_type` | cross-definition | method call-constraint nodes and receiver-derived helper-family selections | dependent method targets, binding/result facts, and helper-routing queries | method-target, receiver-binding, and helper-family diagnostics at dependent sites |

- Intra-definition invalidation remains the cheapest path: local-binding,
  control-flow, and initializer-shape edits do not force unrelated definition or
  module rebuilds when dependency edges do not cross into that definition.
- Cross-definition invalidation preserves deterministic propagation order:
  definition-signature, import-alias, and receiver-type edits revisit dependent
  graph nodes through stable path/node ordering rather than hash iteration.
- Current coverage pins the contract shape, representative observed counts, and
  positive fan-out for every supported family. Negative fan-out coverage proves
  local-binding and definition-signature edits do not invalidate unrelated
  definition-local or cross-definition facts.
- Template inference, CT-eval expansion, and optional parallel solve must
  consume this explicit, covered contract before they reuse graph state whose
  invalidation boundaries affect correctness.

Planned optional parallel-solve contract:
- Baseline implementation design note: `docs/Semantics_Multithread_Design.md`.
- Parallel solve is an optimization of the established single-threaded graph resolver, not a second semantic model.
- The single-threaded path remains the source of truth for:
  - node creation order
  - SCC membership
  - condensation-DAG ordering
  - diagnostic ordering
  - final published semantic-product facts
- Any parallel execution must preserve the same visible results as the single-threaded path for a fixed canonical
  program, including identical diagnostics, dump order, and lowering-facing facts.
- Parallelism should be introduced only at dependency-safe boundaries such as independent SCCs or other explicitly
  documented work partitions; it must not speculate across unresolved cycles or hidden side channels.
- Per-task execution must use isolated local state for temporary solver work. Shared caches or published semantic facts
  may only be merged back through one deterministic commit order.
- Merge rules must be explicit:
  - if two parallel tasks publish non-overlapping facts, merge order must still be deterministic
  - if tasks would publish conflicting facts, that conflict must resolve through the same deterministic contradiction
    path as the single-threaded solver rather than “last writer wins”
  - diagnostics emitted during parallel work must be buffered and ordered by stable node/sort keys before publication
- Entry criteria before prototyping:
  - single-threaded performance guardrails are stable
  - graph invalidation rules are explicit and tested
  - CT-eval interaction is explicit and tested
  - semantic-product node identities or sort keys exist for deterministic diagnostic/fact merges
- Parallel coverage should prove both parity and scheduling-independence by exercising the same corpus under repeated
  runs and different worker counts without changing semantic-product output or diagnostic order.

Planned non-template inference migration contract:
- Remaining non-template inference islands should migrate onto graph-backed query/binding state in thin slices rather
  than by one broad rewrite.
- Each migration slice should identify:
  - the current ad hoc inference island being removed
  - the graph-backed facts that replace it
  - the exact fallback/compatibility behavior that must remain unchanged during the cutover
- Completed lowerer-side query payload slice: semantic-product-addressed direct `Result.ok(query())`
  payloads and base-kind `try(Result.ok(query()))` inference now use published binding/query facts as
  the authority. When those facts are absent on the semantic-product path, the lowerer leaves the value
  kind unresolved instead of asking recursive expression-kind fallback to reconstruct the query result.
  The recursive fallback remains available only for syntax-only or no-semantic-product compatibility
  contexts.
- Completed lowerer-side `try(...)` dispatch slice: semantic-product-addressed `try(...)` expressions
  now use published try facts as the authority before local-result, callable-result, map-helper, or
  file-helper inference can answer. Missing or incomplete semantic try facts produce the existing
  deterministic semantic-product try diagnostic and keep the inferred value kind unresolved; legacy
  reconstruction remains available only for `try(...)` expressions without semantic-product identity.
- Completed lowerer-side Result-combinator metadata slice: semantic-product-addressed
  `Result.map`, `Result.and_then`, and `Result.map2` metadata only require query facts when direct
  lambda payload analysis leaves the result value unresolved. In that unresolved case, missing or
  incomplete semantic query facts now fail closed with the existing semantic-product query diagnostic
  instead of leaving a partially inferred result shape for later lowering.
- Completed lowerer-side args-pack binding metadata slice: semantic-product-addressed variadic
  parameter element metadata now uses the published binding fact when the syntax shape does not carry
  the element type. Missing or incomplete binding facts fail closed with semantic-product args-pack
  diagnostics instead of leaving the element kind unresolved for later lowering.
- Completed binding metadata diagnostic slice: semantic-product binding completeness validation now
  checks interned binding type and reference-root metadata before lowerer consumers can read binding
  fact text fields. Missing or contradictory present ids fail closed with deterministic binding
  metadata diagnostics.
- Completed lowerer-side direct name payload slice: semantic-product-addressed direct
  `Result.ok(name)` payload metadata now uses the published binding fact before local maps or
  recursive expression-kind fallback can answer. Missing binding facts leave the payload kind
  unresolved on the semantic-product path while syntax-only compatibility keeps the legacy local
  reconstruction behavior.
- Completed lowerer-side statement initializer binding slice: semantic-product-addressed statement
  bindings initialized from names now prefer the published initializer binding fact before local-map
  metadata can decide the collection shape. Syntax-only or no-semantic-product contexts keep the
  legacy locals-based compatibility path.
- Completed lowerer-side statement initializer LocalInfo slice: after a statement initializer binding
  fact has selected the scalar, pointer/reference, or array/vector shape, final lowerer fallback
  branches preserve the published fact instead of letting stale local-map metadata overwrite value
  kinds or struct paths. Syntax-only or no-semantic-product contexts keep the legacy locals-based
  compatibility path.
- Completed lowerer-side for-condition binding slice: semantic-product-addressed omitted/`auto`
  `for(...)` condition bindings now declare their branch-local storage from the published binding
  fact before expression-kind or struct-path reconstruction can answer. Missing binding facts fail
  closed on the semantic-product path; syntax-only compatibility keeps the legacy local
  reconstruction behavior.
- Completed local-auto stale-fact diagnostic slice: semantic-product local-auto completeness
  validation now checks the local-auto binding type against the published binding fact before
  lowering can synthesize a binding transform from the local-auto entry. Mismatches fail closed with
  a deterministic stale local-auto diagnostic.
- Completed local-auto binding metadata diagnostic slice: semantic-product local-auto completeness
  validation now checks the interned binding type metadata before lowering can synthesize a binding
  transform from the local-auto entry. Missing or contradictory present ids fail closed with
  deterministic local-auto metadata diagnostics.
- Completed local-auto initializer-path diagnostic slice: semantic-product local-auto completeness
  validation now checks initializer direct-call and method-call path ids against the published
  initializer path before lowerer handoff can consume inconsistent local-auto metadata. Missing or
  contradictory initializer call path ids fail closed with deterministic local-auto diagnostics.
- Completed local-auto initializer return-kind diagnostic slice: semantic-product local-auto
  completeness validation now checks interned initializer direct-call and method-call return-kind
  metadata against the published callable summary before lowerer handoff can consume inconsistent
  local-auto metadata. Missing or contradictory initializer return-kind ids fail closed with
  deterministic local-auto diagnostics.
- Completed try on-error context diagnostic slice: semantic-product try completeness validation now
  checks `try(...)` on-error handler path, error type, and bound-arg count against the published
  callable summary before lowerer handoff can consume inconsistent try metadata. Missing handler
  path ids or contradictory on-error context fail closed with deterministic try diagnostics.
- Completed try context-return diagnostic slice: semantic-product try completeness validation now
  checks interned `try(...)` context return-kind metadata against the published callable summary
  before lowerer handoff can consume inconsistent try metadata. Contradictory return-context facts
  fail closed with a deterministic try diagnostic.
- Completed try result-metadata diagnostic slice: semantic-product try completeness validation now
  checks interned `try(...)` value/error metadata against the published callable summary for the
  resolved operand before lowerer handoff can consume inconsistent try metadata. Contradictory
  operand Result facts fail closed with a deterministic try metadata diagnostic.
- Completed try operand-metadata diagnostic slice: semantic-product try completeness validation now
  checks interned operand binding type, operand receiver binding type, and operand query type
  metadata before lowerer handoff can consume inconsistent try metadata. Missing or contradictory
  operand type ids fail closed with deterministic try metadata diagnostics.
- Completed lowerer-side try result ID slice: semantic-product-backed try result setup,
  expression-kind inference, and completeness checks now resolve interned try value/error type IDs
  before copied try fact text, so stale duplicated text cannot override graph-owned try result
  metadata.
- Completed return-fact stale diagnostic slice: semantic-product return completeness validation now
  checks interned return-kind facts against the published callable summary before lowerer handoff
  can consume inconsistent control-flow metadata. Contradictory return-kind facts fail closed with
  a deterministic return diagnostic.
- Completed return metadata diagnostic slice: semantic-product return completeness validation now
  checks interned return binding type, struct path, and reference-root metadata before return
  inference can consume inconsistent return fact text fields. Missing or contradictory present ids
  fail closed with deterministic return metadata diagnostics.
- Completed `on_error` stale-fact diagnostic slice: semantic-product on-error completeness
  validation now checks the on-error fact against the published callable summary before lowerer
  setup can install the handler. Handler path, error-type, and bound-arg-count mismatches fail
  closed with a deterministic stale on-error diagnostic.
- Completed `on_error` result-metadata diagnostic slice: semantic-product on-error completeness
  validation now checks interned return Result metadata against the published callable summary
  before lowerer setup can install the handler. Contradictory return Result shape facts fail closed
  with a deterministic stale on-error metadata diagnostic.
- Completed lowerer-side `on_error` ID slice: semantic-product-backed handler setup now resolves
  interned `errorTypeId` and `boundArgTextIds` before copied on-error fact text, so stale
  duplicated text cannot override graph-owned error and bound-argument metadata.
- Completed query stale-target diagnostic slice: semantic-product query completeness validation
  now checks query facts against the published direct or method call target for the same
  semantic node before lowerer result metadata can consume the query fact. Resolved-target
  mismatches fail closed with a deterministic stale query diagnostic.
- Completed query result-metadata diagnostic slice: semantic-product query completeness validation
  now checks interned query Result payload/error metadata against the published callable summary
  for the resolved target before lowerer result metadata can consume the query fact. Contradictory
  Result shape facts fail closed with a deterministic stale query metadata diagnostic.
- Completed lowerer-side query Result-metadata ID slice: semantic-product-addressed generic
  call result metadata now resolves `resultValueTypeId` and `resultErrorTypeId` before consulting
  copied query-fact text fields, so unresolved `Result.map(...)` and generic call result paths
  consume the graph-owned intern table rather than requiring duplicated payload/error text.
- Completed query type-metadata diagnostic slice: semantic-product query completeness validation now
  checks interned query type, binding type, and receiver binding type metadata before lowerer
  consumers can read query fact text fields. Missing or contradictory type ids fail closed with
  deterministic query metadata diagnostics.
- Completed lowerer-side call-base scalar query ID slice: semantic-product-addressed call-base
  scalar inference now resolves `queryTypeTextId` and `bindingTypeTextId` before copied query-fact
  text fields, so scalar call kind inference consumes the graph-owned intern table rather than
  stale duplicated text.
- Completed lowerer-side Result method base-kind slice: semantic-product-addressed
  `Result.ok(...)`, `Result.error(...)`, and `Result.why(...)` call-base inference now consumes
  published query/binding facts before the legacy Result method arity fallback. Missing facts keep
  the value unresolved on the semantic-product path while syntax-only compatibility keeps the old
  fallback.
- Completed lowerer-side tail-dispatch collection query ID slice: native tail-dispatch map/vector
  target classification now resolves `bindingTypeTextId` and `queryTypeTextId` before copied
  query-fact text fields, so collection receiver classification consumes graph-owned query
  metadata instead of requiring duplicated text.
- Completed lowerer-side binding adapter ID slice: binding-kind, value-kind, string/file-error,
  and reference-array adapter decisions now resolve `bindingTypeTextId` before copied binding-fact
  text fields, so local setup consumes graph-owned binding metadata instead of stale duplicated
  text.
- Completed lowerer-side binding coverage ID slice: binding, local-auto, and collection
  specialization completeness checks now resolve interned binding type ids before copied fact text,
  so coverage validation consumes graph-owned binding metadata before compatibility text.
- Completed lowerer-side args-pack binding ID slice: args-pack parameter metadata now resolves
  interned binding type ids before copied binding-fact text, so variadic element metadata consumes
  graph-owned binding facts before compatibility text.
- Completed lowerer-side field/packed payload ID slice: native field receiver and packed Result
  payload classifiers now resolve binding/query type ids before copied semantic-product text, so
  struct and packed-payload classification consume graph-owned metadata before compatibility text.
- Completed lowerer-side try operand Result ID slice: `try(...)` operand result metadata now
  resolves `resultValueTypeId` and `resultErrorTypeId` before copied query-fact text, so
  value/error propagation consumes graph-owned Result metadata before compatibility text.
- Completed lowerer-side status Result source ID slice: native `Result.why(...)` and
  `Result.error(...)` direct-call status-only source checks now resolve `resultErrorTypeId`
  before copied query-fact text, so error-domain matching consumes graph-owned metadata before
  compatibility text.
- Completed lowerer-side query Result completeness ID slice: query Result value-shape
  completeness validation now resolves `resultValueTypeId` before copied query-fact text, so
  valid interned value metadata no longer depends on duplicated compatibility text.
- Completed lowerer-side callable Result completeness ID slice: callable result metadata
  completeness validation now resolves `resultValueTypeId` before copied callable-summary text, so
  valid interned value metadata no longer depends on duplicated compatibility text.
- Completed lowerer-side return-info ID slice: entry return transform analysis and return-info
  setup now resolve interned callable return/result ids plus return binding ids before copied
  semantic-product text, so return metadata consumes graph-owned facts before compatibility text.
- Completed lowerer-side return binding completeness ID slice: return metadata completeness
  validation now resolves interned return binding type ids before copied return-fact text, so valid
  binding metadata no longer depends on duplicated compatibility text.
- Completed lowerer-side pointer/location return ID slice: native aggregate `dereference(...)` and
  `location(...)` direct-call return checks now resolve interned return binding type ids before
  copied return-fact text, so pointer/reference classification consumes graph-owned facts first.
- Completed lowerer-side statement binding ID slice: local-auto statement bindings,
  for-condition bindings, and statement binding LocalInfo fallback now resolve
  `bindingTypeTextId` before copied binding-fact text, so declaration and final local setup
  consume graph-owned binding metadata before compatibility text.
- Completed lowerer-side statement binding type-info ID slice: statement binding and initializer
  type-info inference now resolves interned binding type ids before copied binding-fact text, so
  map/vector/scalar binding metadata consumes graph-owned facts before compatibility text.
- Completed lowerer-side pick target ID slice: native `pick(...)` target sum resolution now
  resolves binding, query, and return binding type ids before copied semantic-product text, so
  named and direct-call pick target classification consumes graph-owned metadata before
  compatibility text.
- Completed lowerer-side named pick binding lookup slice: semantic-product-addressed
  `pick(value)` target classification now finds the published binding fact by the target
  expression's semantic node id instead of scanning binding facts by scope/name. Missing semantic-id
  facts fail closed on the semantic-product path while syntax-only compatibility keeps the local
  reconstruction path.
- Completed lowerer-side sum source ID slice: native sum initializer payload-shape and
  `pick(...)` aggregate-result source classification now resolve binding/query type ids before
  copied semantic-product text, so source payload classification consumes graph-owned metadata
  before compatibility text.
- Completed lowerer-side collection specialization ID slice: binding-type adapters now resolve
  interned collection family, element, key, and value metadata before copied collection
  specialization text, so collection local setup consumes graph-owned facts before compatibility
  text.
- Completed collection specialization metadata diagnostic slice: semantic-product collection
  specialization completeness validation now checks interned family, binding type, element type, and
  key/value type metadata before lowerer collection consumers can read collection fact text fields.
  Missing or contradictory present ids fail closed with deterministic collection specialization
  metadata diagnostics.
- Completed direct-call metadata diagnostic slice: semantic-product direct-call completeness
  validation now checks interned scope, call name, resolved target, and published lookup metadata
  before lowerer routing consumers can dispatch through direct-call target facts. Missing or
  contradictory present ids fail closed with deterministic direct-call metadata diagnostics.
- Completed method-call metadata diagnostic slice: semantic-product method-call completeness
  validation now checks interned scope, method name, receiver type, resolved target, and published
  lookup metadata before lowerer routing consumers can dispatch through method-call target facts.
  Missing or contradictory present ids fail closed with deterministic method-call metadata
  diagnostics.
- Completed bridge-path metadata diagnostic slice: semantic-product bridge-path completeness
  validation now checks interned scope, collection family, chosen path, and published lookup
  metadata before lowerer bridge-routing consumers can dispatch through bridge-path choice facts.
  Missing or contradictory present ids fail closed with deterministic bridge-path metadata
  diagnostics.
- Completed lowerer call-target adapter lookup slice: direct-call, method-call, and bridge-path
  target and stdlib-surface helpers now resolve only through published routing lookup maps. Raw
  semantic-product target vectors remain stored facts, but missing published maps no longer recover
  targets by scanning those vectors by semantic node id.
- Completed native pick target sum slice: semantic-product-addressed `pick(value)` lowering now
  resolves named targets from the published binding fact and confirms the sum layout through
  published sum metadata before local-map shape reconstruction can answer. Missing sum metadata or
  binding facts, and binding facts that contradict lowerer-local shape, fail closed with
  deterministic pick-target diagnostics. Syntax-only compatibility keeps the old local-map/type-name
  reconstruction path.
- Completed native pick target binding lookup slice: that `pick(value)` binding lookup is now keyed
  by the target expression semantic node id rather than a lowerer-local scope/name scan over all
  published binding facts.
- Completed native pick query-target slice: semantic-product-addressed `pick(makeValue())`
  lowering now resolves direct call targets from the published query fact, checks the query type
  against the callee's published return fact when available, and confirms the sum layout through
  published sum metadata before type-name reconstruction can answer. Missing or incomplete query
  facts and query facts that contradict the callee return fact fail closed with deterministic
  pick-target diagnostics.
- Completed native pick method-target slice: semantic-product-addressed `pick(receiver.makeValue())`
  lowering now routes method-call targets through the same published query fact and callee return
  fact authority as direct-call targets. Direct sum constructors remain on the constructor path;
  method-call pick targets no longer depend on target-name reconstruction to identify the sum.
- Completed native pick variant-metadata slice: semantic-product-addressed native `pick(...)`
  arms now validate published sum-variant metadata before dispatch and use the published tag value
  for tag comparisons. Missing or stale arm metadata fails closed with deterministic pick-arm
  diagnostics, while syntax-only compatibility keeps the old AST variant-order path.
- Completed native pick payload-local slice: semantic-product-addressed `pick(...)` payload
  binding and aggregate result inference now build branch-local payload shape from the published
  sum-variant metadata after validation, rather than reconstructing that payload shape from the raw
  AST variant. Syntax-only compatibility keeps the old AST payload path.
- Completed native pick shared-variant-metadata slice:
  semantic-product-addressed `pick(...)` arm dispatch, aggregate-result
  inference, and payload binding now use the shared sum-variant metadata
  helpers for tag and payload storage decisions instead of carrying a
  pick-local duplicate validator and published-variant pointer.
- Completed native pick aggregate-result source slice:
  semantic-product-addressed `pick(...)` aggregate-result inference now reads
  published binding/query type facts for arm value expressions before
  branch-local struct-path reconstruction can identify aggregate result shape.
  Stale published arm value metadata fails closed with a deterministic pick
  aggregate-result diagnostic; syntax-only or no-fact compatibility keeps the
  old branch-local reconstruction path.
- Completed native field receiver slice: semantic-product-addressed field
  access now reads published binding/query type facts for the receiver
  expression before struct-path reconstruction can identify the receiver
  layout. Stale receiver type metadata fails closed with a deterministic
  field-receiver diagnostic; syntax-only or no-fact compatibility keeps the
  old receiver struct-path reconstruction path.
- Completed native packed Result payload slice:
  semantic-product-addressed native packed Result payload inference now reads
  published binding/query type facts for the payload expression before
  expression-kind or packed struct-path reconstruction can identify payload
  shape. Stale payload type metadata fails closed with a deterministic packed
  Result diagnostic; syntax-only or no-fact compatibility keeps the old
  payload reconstruction path.
- Completed native sum slot-layout slice: semantic-product-addressed lowered sum
  slot allocation now validates and consumes published sum-variant metadata for
  every variant before choosing the maximum payload slot width. Syntax-only
  compatibility keeps the old AST payload-storage path.
- Completed native sum variant-selection slice: semantic-product-addressed
  constructor, `Result.ok`, and initializer-matching selection now validates
  and consumes published sum-variant payload metadata before choosing the
  selected payload storage shape. Syntax-only compatibility keeps the old AST
  payload-storage path.
- Completed native sum initializer-source slice: semantic-product-addressed
  inferred sum initializer matching now reads published binding/query type
  facts for the initializer expression before asking expression-kind or
  struct-path reconstruction to identify the payload shape. Stale initializer
  type metadata fails closed with a deterministic sum-initializer diagnostic;
  syntax-only or no-fact compatibility keeps the old reconstruction path.
- Completed native active sum payload tag slice: semantic-product-addressed sum payload move and
  destroy helper dispatch now validate published sum-variant metadata and use the published tag
  value for active-payload comparisons instead of reading tag values from AST variant order.
  Syntax-only compatibility keeps the old AST tag path.
- Completed native active sum payload-storage slice: semantic-product-addressed
  sum payload move and destroy helper dispatch now validates and consumes
  published sum-variant metadata before choosing aggregate payload helpers or
  copying payload slots. Syntax-only compatibility keeps the old AST
  payload-storage path.
- Completed native sum construction tag slice: semantic-product-addressed sum construction now
  validates published sum-variant metadata and stores the published tag value into the active tag
  slot instead of reading the tag from AST variant order. Syntax-only compatibility keeps the old
  AST tag path.
- Completed native Result-combinator tag slice: semantic-product-addressed
  `Result.map`, `Result.and_then`, and `Result.map2` lowering now validates published
  sum-variant metadata before comparing source `ok` tags or storing target `ok`/`error`
  tags. Syntax-only compatibility keeps the old AST tag path.
- Completed native Result-combinator payload-storage slice:
  semantic-product-addressed `Result.map`, `Result.and_then`, and
  `Result.map2` lowering now validates and consumes published sum-variant
  metadata before binding source `ok` payload locals, storing mapped target
  payloads, or copying propagated `error` payloads. Syntax-only compatibility
  keeps the old AST payload-storage path.
- Completed native Result-combinator source-query slice:
  semantic-product-addressed direct-call sources for `Result.map`,
  `Result.and_then`, and `Result.map2` now resolve their stdlib Result sum
  type from the published query fact before falling back to struct-path
  reconstruction. Stale query metadata fails closed with a deterministic
  Result-combinator source diagnostic; syntax-only or no-query compatibility
  keeps the old struct-path path.
- Completed lowerer-side Result-combinator source-query ID slice:
  semantic-product-addressed direct-call sources for `Result.map`,
  `Result.and_then`, and `Result.map2` now resolve interned query
  binding/query type IDs before copied query text when classifying the source
  stdlib Result sum. Stale duplicated query text can no longer override
  graph-owned interned metadata.
- Completed native `Result.why(...)` source-query slice:
  semantic-product-addressed direct-call operands for `Result.why(...)` now
  resolve status-only stdlib Result sources from the published query fact
  before scanning callee return transforms. Missing or stale query metadata
  fails closed with deterministic Result.why source diagnostics; syntax-only
  or no-query compatibility keeps the old transform-scan path.
- Completed native `Result.error(...)` source-query slice:
  semantic-product-addressed direct-call operands for `Result.error(...)` now
  resolve status-only stdlib Result sources from the published query fact
  before scanning callee return transforms. Missing or stale query metadata
  fails closed with deterministic Result.error source diagnostics; syntax-only
  or no-query compatibility keeps the old transform-scan path.
- Completed native `try(...)` Result-variant slice: semantic-product-addressed
  stdlib Result sum matching, source `ok`/`error` payload loads, propagated
  return-error copies, and source/target tag writes now validate and consume
  published sum-variant metadata instead of reading AST payload shape or
  variant order. Syntax-only compatibility keeps the old AST payload/tag path.
- Completed native aggregate-pointer return slice:
  semantic-product-addressed direct-call operands for aggregate
  `dereference(...)` now resolve pointer/reference return shape from the
  published return fact before scanning callee return transforms. Missing
  return metadata fails closed with a deterministic aggregate-pointer return
  diagnostic; syntax-only or no-semantic-product compatibility keeps the old
  transform-scan path.
- Completed native location-reference return slice:
  semantic-product-addressed direct-call operands for `location(...)` now
  resolve reference return shape from the published return fact before scanning
  callee return transforms. Missing return metadata fails closed with a
  deterministic location-reference return diagnostic; syntax-only or
  no-semantic-product compatibility keeps the old transform-scan path.
- Completed direct `Result.ok(...)` payload-metadata slice:
  semantic-product-addressed direct-call payloads now resolve payload type
  metadata from published binding/query facts before direct callee collection
  or struct reconstruction can classify the payload. Missing direct-call
  payload facts keep the value unresolved on the semantic-product path, while
  syntax-only compatibility keeps the legacy reconstruction path. The metadata
  path also resolves interned binding/query payload type IDs before treating
  semantic-product payload metadata as absent.
- Completed lowerer-side direct `Result.ok(...)` payload ID-order slice:
  semantic-product-addressed direct payload metadata now lets interned
  binding/query type IDs override stale copied payload text before collection,
  struct, or local fallbacks classify the payload. Copied payload text remains
  only the compatibility fallback when no usable interned ID is present.
- Completed native `Result.ok(...)` payload-emission slice:
  packed native `Result.ok(...)` emission now consumes semantic-product
  binding/query payload facts before invoking scalar inference, direct map
  rewrite reconstruction, collection fallback, or struct fallback. Missing
  semantic-product payload facts now fail closed with a deterministic
  `Result.ok(...)` payload diagnostic; syntax-only compatibility keeps the
  legacy inference and direct-callee paths.
- Completed lowerer-side native `Result.ok(...)` payload-emission ID-order
  slice: packed native payload emission now lets interned semantic-product
  binding/query type IDs override stale copied payload text before scalar,
  collection, or struct payload classification. Copied text remains only the
  compatibility fallback when no usable interned ID is present.
- Preferred migration order:
  - direct local/binding inference islands that still bypass graph-backed local/query facts
  - control-flow and initializer-shape inference paths that currently reconstruct state outside the graph
  - compile-time or helper-routing inference paths that still depend on local ad hoc caches
- A migrated slice is only complete when:
  - the old ad hoc inference branch is deleted or reduced to one explicit compatibility adapter
  - compile-pipeline parity still holds on the existing return/query/local pilot surfaces
  - the new path consumes published graph-backed facts instead of recomputing equivalent local state
- Migration coverage should pin both positive and negative boundaries:
  - successful inference on the intended graph-backed path
  - unchanged diagnostics for unresolved or contradictory inference sites
  - unchanged helper-family and canonical-path choices where inference affects call routing
- Explicit and implicit template inference migration stays blocked on this work, because template solving should not be
  layered on top of unresolved non-template inference islands that still own their own caches or ordering rules.

Planned template-inference migration contract:
- Explicit and implicit template inference should move onto the graph-backed path only after non-template inference
  islands and graph invalidation rules are stable.
- Current status: repeated implicit-template helper calls publish and consume
  definition-scoped graph facts keyed by the helper target, explicit template
  arguments, ordered argument types, and packed-argument shape. The first
  migrated helper-routing slice covers stdlib vector helper calls whose receiver
  type determines the inferred template argument while preserving existing
  conflict diagnostics.
- Template migration should not introduce a separate graph-independent solver cache for template arguments; the graph
  remains the owner of dependency ordering and revisit rules.
- Each migration slice should identify:
  - which template-inference surface is moving (explicit template specialization checks, implicit template argument
    inference, or template-dependent helper-family/call-target selection)
  - which graph-backed facts now drive that surface
  - which legacy diagnostics and precedence rules must remain unchanged during the cutover
- Preferred migration order:
  - template-dependent consumers that already sit adjacent to the stabilized return/query/local pilot
  - helper-family and canonical-path decisions that depend on template argument resolution
  - broader cross-definition template solving once invalidation and CT-eval boundaries are already pinned
- A migrated template slice is only complete when:
  - template argument resolution is driven by published graph-backed facts instead of deferred side-state
  - implicit and explicit template diagnostics stay deterministic and keep current precedence/ambiguity behavior
  - helper-shadow, canonical-path, and overload-selection choices remain stable for the affected surface
- Coverage should pin:
  - successful explicit and implicit template resolution on the graph-backed path
  - unchanged diagnostics for unresolved, ambiguous, or contradictory template constraints
  - parity for helper-routing and call-target choices when template inference determines which callee is selected
- Broader omitted-envelope and local-`auto` expansion should remain sequenced after these migrations prove stable, so
  new local inference surfaces do not outrun the template-dependency contract that consumes them.

Planned omitted-envelope and local-`auto` expansion contract:
- Broader omitted-envelope and local-`auto` inference should expand only after the next non-template and template
  migrations prove stable on the graph path.
- Expansion should proceed in thin surfaces rather than by reopening “all local inference” at once.
- Each expansion slice should define:
  - which omitted-envelope or local-`auto` forms are newly graph-backed
  - which existing pilot surfaces stay unchanged
  - which diagnostics and canonical helper/call-target choices must remain stable
- Priority should stay on surfaces that directly reuse already-published graph facts, such as:
  - initializer-driven local `auto` propagation
  - omitted-envelope bindings that already sit beside stabilized local/query/`try(...)` metadata
  - control-flow-local inference shapes that currently reconstruct facts already present in graph state
- Expansion is not complete until:
  - the new surface consumes the same published graph-backed facts as the existing pilot
  - helper-routing and canonical-path decisions remain deterministic
  - unchanged unresolved/contradictory diagnostics are pinned in coverage
- This work remains downstream of template migration because widening local/omitted inference before template
  dependencies are graph-backed would create new pilot-only islands around template-dependent call routing.

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
- Local-auto fact lookup follows the same published-map authority rule for semantic-node ids: public published
  local-auto lookup helpers and the lowerer semantic-product adapter require `localAutoFactIndicesByExpr` for
  expression-scoped local-auto facts instead of recovering by scanning raw `localAutoFacts` storage.
- Binding fact lookup follows the same published-map authority rule once the semantic product is frozen by the
  publication path: the lowerer semantic-product adapter requires `bindingFactIndicesByExpr` for expression-scoped
  binding facts instead of recovering by scanning raw `bindingFacts` storage. Mutable hand-built products may still
  use the raw scan before `freezeSemanticProgramPublishedStorage(...)`.
- Return fact lookup follows the same published-map authority rule once the semantic product is frozen by the
  publication path: the lowerer semantic-product adapter requires `returnFactIndicesByDefinitionId` for
  definition-scoped return facts instead of recovering by scanning raw `returnFacts` storage. Mutable hand-built
  products may still use the raw scan before `freezeSemanticProgramPublishedStorage(...)`.
- Definition lookup follows the same published-map authority rule once the semantic product is frozen by the
  publication path: public definition lookup helpers require `definitionIndicesByPathId` for path-id lookups instead
  of recovering by scanning raw `definitions` storage. Mutable hand-built products may still use the raw scan before
  `freezeSemanticProgramPublishedStorage(...)`.
- Published fact-family views follow the same rule once the semantic product is frozen by the publication path:
  per-module `moduleResolvedArtifacts` indexes are the authority for direct-call, method-call, bridge-path,
  callable-summary, binding, return, collection-specialization, local-auto, query, try, and `on_error` views. Mutable
  hand-built products may still use the raw flat-storage scan before `freezeSemanticProgramPublishedStorage(...)`.
- Type-shape metadata lookup follows the same rule once the semantic product is frozen by the publication path:
  `typeMetadataIndicesByPathId` and `structFieldMetadataIndicesByStructPathId` are the authorities for type and
  struct-field metadata lookup instead of recovering by scanning raw `typeMetadata` or `structFieldMetadata` storage.
  Mutable hand-built products may still use the raw scan before `freezeSemanticProgramPublishedStorage(...)`.
- Collection-specialization lookup follows the same published-map authority rule for semantic-node ids: public
  published collection-specialization lookup helpers and the lowerer semantic-product adapter require
  `collectionSpecializationIndicesByExpr` for expression-scoped collection facts instead of recovering by scanning
  raw `collectionSpecializations` storage.
- Query, `try(...)`, and `on_error` lowering/completeness checks follow the same rule: semantic-node-id facts are the
  production authority, while resolved-path/source/path-id indexes remain inspection metadata only.
- Callable-summary lookup follows the same published-map authority rule once the semantic product is frozen by the
  publication path: `callableSummaryIndicesByPathId` is the authority, while raw `callableSummaries` scanning remains
  limited to mutable hand-built products before `freezeSemanticProgramPublishedStorage(...)`.
- The lowerer semantic-product adapter no longer builds local-auto initializer-path, query resolved-path/call-name, or
  `try(...)` operand-path/source composite recovery indexes. Those composite lookup maps remain publication and
  inspection metadata rather than production lowerer lookup surfaces.
- Query fact lookup now follows the same published-map authority rule for semantic-node ids: public published query
  lookup helpers and the lowerer semantic-product adapter require `queryFactIndicesByExpr` for expression-scoped
  query facts instead of recovering by scanning raw `queryFacts` storage.
- Try fact lookup follows the same published-map authority rule for semantic-node ids: public published try lookup
  helpers and the lowerer semantic-product adapter require `tryFactIndicesByExpr` for expression-scoped try facts
  instead of recovering by scanning raw `tryFacts` storage.
- `on_error` fact lookup follows the same published-map authority rule for definition semantic-node ids: public
  published `on_error` lookup helpers and the lowerer semantic-product adapter require
  `onErrorFactIndicesByDefinitionId` for definition-scoped handler facts instead of recovering by scanning raw
  `onErrorFacts` storage.
- The same adapter quarantine now applies to `on_error` definition-path indexes: lowerer handler setup resolves
  `on_error` facts by definition semantic id, while definition-path lookup remains a published inspection surface.
- Return facts follow the same adapter-cache rule for direct definition lookup: the semantic-product adapter no longer
  materializes a return definition-path index as a recovery surface. The explicit return-by-path helper remains
  available for call-site consumers that already resolved a callee path.
- Base call-kind inference now applies the same graph-backed rule for `try(...)`: semantic-product `try` facts provide
  the value kind before local `Result` state can infer it, and missing facts leave the semantic-product path unresolved.
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
  - native-backend software-numeric and runtime-reflection rejection are pinned as syntax-owned backend policy scans over spelled expression surfaces rather than semantic-product facts
  - import/layout setup now treats its remaining `Definition*` map as an explicitly AST-owned provenance/body inventory for field statements, namespace prefixes, and recursive layout traversal; the import-alias table is pinned as a syntax-owned shorthand layer derived from spelled `import` directives plus wildcard expansion; top-level struct layout enumeration already prefers semantic-product type inventories, so the only live raw-`Program` seam left there is AST-owned field/provenance traversal inside layout computation
  - helper/local setup now prefers semantic-product `on_error`, callable, binding, and return facts wherever lowering-facing meaning exists; entry completeness validation checks `on_error` coverage from published callable summaries and `onErrorFacts` instead of rebuilding a handler map from raw AST transforms; callable-definition lowering enumerates semantic-product definitions and keeps the AST only as the executable body/provenance source, while statement-call lowering remains intentionally syntax-owned because it walks spelled statement bodies directly and the math-import probe remains syntax-owned import shorthand during uninitialized setup
  - source-map finalization consumes a narrow `FunctionSyntaxProvenance` map for fallback function line/column data rather than the full `Definition*` inventory
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
  The manifest records each pass name, pass kind, input/output ownership, action
  (`MutatesAst`, `ValidatesOnly`, or `PublishesFacts`), and whether the pass is
  a compatibility rewrite.
- The pre-validator AST pass runner consumes the manifest through the
  `validator-passes` boundary, so adding, removing, or reordering a semantic AST
  rewrite requires changing the manifest and the matching runner together.
- Compatibility rewrites are explicitly marked in the manifest. Core
  canonicalization passes, template monomorphization, validator fact collection,
  omitted-struct initializer rewriting, semantic-node-id assignment, and final
  semantic-product publication are distinct pass kinds/ownership boundaries.
- Type-resolution analysis still uses its smaller preparation path; if that path
  needs another semantic rewrite, add it intentionally rather than assuming it
  inherits the full validation manifest.

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
- `SemanticProductContractVersionCurrent` is `SemanticProductContractVersionV2`.
- Version 2 is an API-only contract bump; it does not intentionally change the `semantic-product` text dump shape.
- Version 2 adds fact-family ownership metadata through `semanticProgramFactFamilyInfos()`,
  `semanticProgramFactFamilyOwnership(...)`, `semanticProgramFactFamilyIsSemanticProductOwned(...)`, and
  `semanticProgramFactFamilyIsAstProvenanceOwned(...)`.
- Fact-family ownership is split into:
  - `SemanticProduct`: lowering-facing facts owned by the semantic product, including resolved call/method/bridge
    targets, callable summaries, type/field metadata, collection specializations, binding/return facts, local-auto
    facts, query/try facts, `on_error` facts, and lowerer preflight facts.
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
  `primec/testing/IrLowererHelpers.h`) are now pinned as intentional stable testing APIs rather than temporary
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

### Language ethos (v1)
- **Simplified and coherent C:** keep the core small, explicit, and close to how the machine behaves when it matters.
- **Sane subset of C++:** keep value types, structs, and explicit control flow, but avoid implicit conversions,
  surprising overload rules, or hidden allocations.
- **Python spirit for ergonomics:** readable defaults and small conveniences (e.g., optional separators, concise
  literals), without sacrificing determinism or making meaning depend on tooling.
- **Ease of understanding first:** prefer features that are easy to explain and reason about, and reject features that
  add power without clarity.
- **Concrete rules:** explicit envelopes, explicit conversions, explicit effects, immutable-by-default bindings, and
  deterministic evaluation order.

## Phase 0 — Scope & Acceptance Gates (must precede implementation)
- **Charter:** capture exactly which language primitives, transforms, and effect rules belong in PrimeStruct, and list
  anything explicitly deferred to later phases.
- **Success criteria:** define measurable gates (parser coverage, IR validation, backend round-trips, sample programs)
- **Ownership map:** assign leads for parser, IR/envelope system, first backend, and test infrastructure, plus
  security/runtime reviewers.
- **Integration plan:** describe how the compiler/test suite slots into the build (targets, CI loops, feature flags,
  artifact publishing).
- **Risk log:** record open questions (borrow checker, capability taxonomy, GPU backend constraints) and
  mitigation/rollback strategies.
- **Exit:** only after this phase is reviewed/approved do parser/IR/backend implementations begin; the conformance suite
  derives from the frozen charter instead of chasing a moving target.

## Project Charter (v1 target)
- **Language core:** envelope syntax (definitions + executions), slash paths, namespaces, imports (source + namespace
  exposure), binding initializers, return annotations, effects annotations, and deterministic canonicalization rules.
- **Transform pipeline:** ordered text transforms, ordered semantic transforms, explicit `text(...)` / `semantic(...)`
  grouping, and auto-deduction for registered transforms.
- **Determinism:** stable diagnostics, fixed evaluation order, stable IR emission, and canonical string normalization
  across backends.
- **IR:** PSIR serialization with versioning, explicit opcode list, and a stable module layout shared by all backends.
- **Backends:** C++ emitter and VM bytecode are required targets; GLSL/SPIR-V is a supported optional target with
  explicit documented constraints.
- **Standard library:** a documented core subset (math + collections + IO primitives) with a per-backend support
  matrix.
- **Tooling:** `primec`, `primevm`, `--dump-stage`, and snapshot-style tests for parser/IR/diagnostics.
- **Change control:** any feature outside this charter requires a docs update plus an explicit acceptance gate.

## Risk Log (Phase 0)
Each risk lists a mitigation and a concrete fallback so the project can keep moving if the
open question is unresolved.

- Risk: Borrow checker deferred in v1.
  Impact: Resource safety rules vary by backend, leading to unsound or inconsistent behavior.
  Mitigation: Document explicit resource rules and keep borrow checks out of v1.
  Fallback: Keep borrow-checking claims removed until a design lands.
- Risk: Capability taxonomy is not finalized.
  Impact: Effects/capabilities drift between docs, diagnostics, and runtime logging.
  Mitigation: Define a minimal capability list (IO + memory + GPU) and lock it for v1.
  Fallback: Treat capabilities as diagnostic-only metadata until taxonomy stabilizes.
- Risk: GPU backend constraints are underspecified.
  Impact: GLSL/SPIR-V output may accept unsupported effects or types.
  Mitigation: Publish an explicit GPU support matrix and reject unsupported effects/types early.
  Fallback: Mark GPU backend as experimental and limit to math-only + POD structs.
- Risk: IR/PSIR versioning slips.
  Impact: Backends diverge on opcode interpretation or serialized layouts.
  Mitigation: Freeze v1 opcode set, add migration notes, and version the serialized header.
  Fallback: Reject unknown IR versions and require recompile in tooling.
- Risk: Struct layout guarantees are ambiguous.
  Impact: ABI/layout mismatches across native/VM/GPU.
  Mitigation: Require layout manifests and add conformance tests per backend.
  Fallback: Document layout as backend-defined and disallow cross-backend struct reuse.
- Risk: Default effect policy changes.
  Impact: Behavior differs between CLI defaults and docs.
  Mitigation: Keep defaults centralized in `Options` and tests covering defaults.
  Fallback: Require explicit `[effects]` in examples and templates.
- Risk: Execution emission is unclear.
  Impact: Executions parse but are ignored by some backends.
  Mitigation: Decide and document whether executions are lowered per backend.
  Fallback: Treat executions as diagnostics-only until a backend implements them.
- Risk: Stdlib surface drifts across backends.
  Impact: Code runs on one backend but fails on another.
  Mitigation: Maintain a per-backend stdlib support table + tests.
  Fallback: Provide a "core subset" and gate all samples to it.

## Deferred Features (not in v1)
- Borrow checker and lifetime enforcement beyond basic effects gating.
- Full capability taxonomy and policy-driven capability enforcement (beyond documented effects).
- Tooling integrations beyond basic CLI workflows.
- Backend support for software numeric envelopes (`integer`/`decimal`/`complex`) and mixed-mode numeric ops.
- Placement transforms (`stack`/`heap`/`buffer`) and placement-driven layout guarantees.
- Recursive struct layouts and cross-module layout stability guarantees.
- JIT, chunk caching, or dynamic recompilation tooling.
- IDE/LSP integration and editor tooling.
- Standard library packaging/version negotiation beyond a single in-tree reference set.
- `tools/PrimeStructc` feature parity with the main compiler and template codegen (PrimeStructc stays a minimal subset;
  template codegen and import version selection are explicitly out of scope for v1).
- Tail-call or tail-execution optimization guarantees across all backends.

## Phase 1 — Minimal Compiler That Emits an Executable
Goal: a tiny end-to-end compiler path that turns a single PrimeStruct source file into a runnable native executable.
This is the smallest vertical slice that proves parsing, IR, a backend, and a host toolchain handoff.

### Acceptance criteria
- A single-file PrimeStruct program with one entry definition compiles to a native executable on macOS (initial target).
- The compiler can:
  - Parse a subset of the Envelope syntax (definitions + executions).
  - Build a canonical AST for that subset.
  - Lower to a minimal IR (calls, literals, return).
  - Emit C++ and invoke a host compiler to produce an executable.
- The produced executable runs and returns a deterministic exit code.

### Example source and expected IR (sketch)
PrimeStruct:
```
[return<i32>]
main() {
  return(42i32)
}
```

Expected IR (shape only):
```
module {
  def main(): i32 {
    return 42
  }
}
```

### Compiler driver behavior
- `primec --emit=cpp input.prime -o hello.cpp`
- `primec --emit=exe input.prime -o hello`
  - Uses the C++ emitter plus the host toolchain (initially `clang++`).
  - Bundles a minimal runtime shim that maps `main` to `int main()`.
- `primec --emit=native input.prime -o hello`
  - Emits a self-contained macOS/arm64 executable directly (no external linker).
  - Lowers through the portable IR that also feeds the VM/network path.
  - Current subset: fixed-width integer/bool/float literals (`i32`, `i64`, `u64`, `f32`, `f64`), locals + assign, basic
    arithmetic/comparisons (signed/unsigned integers plus floats), boolean ops (`and`/`or`/`not`), explicit conversions
    via `T{value}` for `i32/i64/u64/bool/f32/f64`, abs/sign/min/max/clamp/saturate, `if`, `print`, `print_line`,
    `print_error`, and `print_line_error` for integer/bool or string literals/bindings, and pointer/reference helpers
    (`location`, `dereference`, `Reference`) in a single entry definition.
- `primec --emit=ir input.prime -o module.psir`
  - Emits serialized PSIR bytecode after semantic validation (no execution).
  - Output is written as `.psir` and includes a PSIR header/version tag.
- `primec --emit=glsl input.prime -o module.glsl`
  - Routes through canonical IR into `IrToGlslEmitter`.
  - Runs `IrValidationTarget::Glsl` before emission to reject unsupported GLSL IR shapes.
- `primec --emit=spirv input.prime -o module.spv`
  - Emits SPIR-V by first generating GLSL and invoking `glslangValidator` or `glslc` (compute stage).
  - Runs `IrValidationTarget::Glsl` before GLSL generation.
  - Requires `glslangValidator` or `glslc` on `PATH`.
- `primec --emit=wasm input.prime -o module`
  - Routes through canonical IR into `WasmEmitter`.
  - Runs `IrValidationTarget::Wasm` or `IrValidationTarget::WasmBrowser` before emission based on `--wasm-profile`
    (`wasi` default, `browser` optional).
  - Rejects unsupported opcode/effect/capability combinations during `ir-validate` before backend emission.
  - When `-o` is omitted, output defaults to `<input-stem>.wasm`.
- `primevm input.prime --entry /main -- <args>`
  - Runs the source via the PrimeStruct VM (equivalent to `primec --emit=vm`). `--entry` defaults to `/main` if omitted.
  - `--debug-json` streams VM debug events as NDJSON to stdout (`session_start`, hook events, and `stop` records with
    snapshots).
  - `--debug-json-snapshots=none|stop|all` adds on-demand `snapshot_payload` fields (`instruction_pointer`,
    `call_stack`, `frame_locals`, `current_frame_locals`, `operand_stack`) to debug-json events.
  - `--debug-trace <path>` writes a deterministic VM event log (NDJSON) to the given file path using the same event
    schema family as debug-json (`session_start`, hook events, and `stop`).
  - `--debug-replay <trace>` replays a trace file generated by `--debug-trace` and emits a single `replay_checkpoint`
    NDJSON event to stdout containing the restored snapshot + snapshot payload.
  - `--debug-replay-sequence <n>` time-travels to the latest checkpoint with `sequence <= n` (without this flag, replay
    restores the terminal checkpoint from the trace).
  - `--debug-dap` runs a stdio DAP endpoint using `Content-Length` framing and routes debugger requests to
    `VmDebugAdapter`.
- `--ir-inline`
  - Enables the optional IR inlining optimization pass after IR validation and before VM/native/IR output.
- Defaults: if `--emit` and `-o` are omitted, `primec input.prime` uses `--emit=native` and writes the output using the
  input filename stem (still under `--out-dir`).
- All generated outputs land in the current directory (configurable by `--out-dir`).

### Wasm backend limits (current)
- `WASM-LIMIT-MEM-ON-DEMAND`: the emitter allocates linear memory only when the lowered IR uses WASI runtime opcodes
  (`PushArgc`, `Print*`, `File*`). Pure compute/control-flow modules emit no memory section.
- `WASM-LIMIT-MEM-SINGLE`: when memory is present, the module uses exactly one linear memory (`memory index 0`) sized
  for runtime scratch space and literal data segments; no additional memories are emitted.
- `WASM-LIMIT-IMPORTS-WASI`: imported host calls are limited to `wasi_snapshot_preview1` and the fixed symbol set
  `{fd_write, fd_read, args_sizes_get, args_get, path_open, fd_close, fd_sync}` selected on demand from IR opcode usage.
- `WASM-LIMIT-PROFILE-BROWSER`: `--wasm-profile=browser` rejects all non-zero effect/capability masks and rejects
  WASI-only opcodes (`PushArgc`, `Print*`, `File*`) during IR validation.
- `WASM-LIMIT-PROFILE-WASI-ALLOWLIST`: `--wasm-profile=wasi` accepts only the explicit Wasm opcode/effect/capability
  allowlist; unsupported IR operations fail with deterministic `ir-validate` diagnostics.

## Goals
- Single authoring language spanning gameplay/domain scripting, UI logic, automation, and rendering shaders.
- Emit high-performance C++ for engine integration, optional GLSL/SPIR-V targets via external toolchains, and bytecode
  for an embedded VM without diverging semantics.
- Share a consistent standard library (math, texture IO, resource bindings) across backends while preserving determinism
  for replay/testing.

## Proposed Architecture
- **Front-end parser:** C/TypeScript-inspired surface syntax with explicit envelope annotations, deterministic control
  flow, and explicit resource usage.
- **Transform pipeline:** ordered text transforms rewrite raw tokens before the AST exists; semantic transforms annotate
  the parsed AST before lowering. The compiler can auto-inject transforms per definition/execution (e.g., attach
  `operators` to every function) with optional path filters (`/std/math/*`, recurse or not) so common rewrites don’t
  have to be annotated manually. Transforms may also rewrite a definition’s own transform list (for example,
  `single_type_to_return`). The default text chain desugars infix operators, control-flow, assignment, etc.; the default
  semantic chain enables `single_type_to_return`. Projects can override via `--text-transforms` /
  `--semantic-transforms` or the auto-deducing `--transform-list`.
- **Intermediate representation:** envelope-tagged SSA-style IR shared by every backend (C++, GLSL, VM). Normalisation
  happens once; backends never see syntactic sugar.
- **Graphics contract:** the windowed graphics language surface and locked spinning-cube v1 mini-spec are defined in
  `docs/Graphics_API_Design.md` (`/std/gfx/*`, profile deduction, `VertexColored` wire layout, deterministic `GfxError`
  codes). Current implementation status: the contract and host/sample coverage exist, canonical `/std/gfx/*` is now the
  authoritative public gfx surface, and `/std/gfx/experimental/*` remains only as a legacy compatibility shim over
  that canonical helper layer rather than as part of the public gfx contract. The experimental path still keeps an explicit `.prime` `GraphicsSubstrate` token/config
  boundary for its legacy wrapper types, the legacy constructor-shaped experimental and canonical `Window(...)`,
  `Device()`, and `Buffer<T>(count)` compatibility entry points now rewrite onto matching stdlib helpers, canonical
  `Window(...)` and `Device()`
  now also route through a private `.prime` `GraphicsSubstrate.createWindow(...)` / `createDevice(...)` /
  `createQueue(...)` boundary inside `/std/gfx/*`, the experimental and canonical `create_swapchain(...)`,
  `create_mesh(...)`, `frame()`, and `Device.create_pipeline([vertex_type] VertexColored, ...)` wrapper paths now run
  through the same proven first-slice logic in `.prime`, canonical `/std/gfx/*` now also routes those fallible
  resource/frame/pipeline helpers through the same private `GraphicsSubstrate.createSwapchain(...)` / `createMesh(...)`
  / `createPipeline(...)` / `acquireFrame(...)` layer, the non-Result `render_pass(...)` / `draw_mesh(...)` / `end()`
  path now routes through minimal pass-encoding helpers with deterministic zero-token / no-op fallback on invalid
  handles, canonical `/std/gfx/*` now also routes `render_pass(...)`, `draw_mesh(...)`, `end()`, `submit(...)`, and
  `present()` through the matching private `GraphicsSubstrate.openRenderPass(...)` / `drawMesh(...)` /
  `endRenderPass(...)` / `submitFrame(...)` / `presentFrame(...)` layer, canonical and experimental `Buffer<T>` now
  also expose `.prime`-authored `count()`,
  `empty()`, `is_valid()`, `readback()`, compute-only `load(index)`, and compute-only `store(index, value)` plus the
  legacy constructor-shaped `Buffer<T>(count)` compatibility entry point and explicit slash-call `allocate<T>(count)` /
  `upload(...)` / `load(...)` / `store(...)` helpers so public buffer inspection, host-side allocation/readback/upload,
  and compute storage access no longer have to route directly through raw fields or builtin `/std/gpu/buffer(...)` /
  `/std/gpu/readback(...)` / `/std/gpu/upload(...)` / `/std/gpu/buffer_load(...)` / `/std/gpu/buffer_store(...)` call
  sites, status-only experimental and canonical gfx flows now use the same stdlib-owned `GfxError.status(err)` helper
  layer as other `Result<Error>` surfaces instead of hand-packing `err.code`, the shared spinning-cube native-window
  sample path now emits a deterministic canonical `/std/gfx/*` stream (`cubeStdGfxEmitFrameStream`) that the macOS host
  and launcher consume via `--gfx` so submit/present can drive one real window end-to-end, the native launcher script
  itself is now only a thin wrapper over a shared canonical gfx launch helper, the native window host runtime shell now
  also lives in one shared presenter helper instead of staying embedded in the spinning-cube sample file, the Metal
  sample launcher now also delegates to one shared metal launch helper while its offscreen runtime shell lives in one
  shared helper instead of staying embedded in `metal_host.mm`, the Metal sample’s snapshot/parity helper modes now also
  bind to one shared spinning-cube simulation reference helper instead of carrying their own inline fixed-step copy, the
  browser sample launcher now also delegates to one shared browser launch helper while its bootstrap/runtime shell now
  lives in `examples/web/shared/browser_runtime_shared.js` instead of staying embedded in `main.js`, real compile-run
  conformance now imports canonical `/std/gfx/*` and exercises that end-to-end wrapper path across exe/vm/native while
  separate compatibility-shim tests keep `/std/gfx/experimental/*` pinned for residual legacy imports, canonical and
  experimental gfx imports now reject deterministically on wasm (`wasm-browser`,
  `wasm-wasi`) and shader-only (`glsl`, `spirv`) emits until those targets gain runtime substrate, host-side sample
  `GfxError` mapping plus locked `VertexColored` upload layout definitions now live in one shared example header instead
  of being duplicated per macOS host, bare explicit bindings of Result-returning gfx wrappers still fail
  deterministically during semantics, unsupported `create_pipeline` vertex types now reject deterministically instead of
  degrading into generic compiler errors. No active TODO currently tracks broader backend/runtime package-path cleanup.
  Add a concrete TODO before changing that graphics backend/runtime seam; current spinning-cube execution
  therefore still mixes shared `.prime` simulation with browser/native/Metal host glue outside the fully canonical
  package path.
- **Layered UI/rendering roadmap:** the first `/std/ui/*` foundation now includes deterministic command-list rendering,
  a two-pass layout tree contract, basic control emission, a basic panel container primitive, and the first composite
  widget helper, plus deterministic HTML/backend adapter records and deterministic platform input records
  (`CommandList`, `draw_text`, `draw_rounded_rect`, `push_clip`, `pop_clip`, `draw_label`, `draw_button`, `draw_input`,
  `begin_panel`, `end_panel`, `draw_login_form`, `HtmlCommandList`, `emit_panel`, `emit_label`, `emit_button`,
  `emit_input`, `bind_event`, `emit_login_form`, `UiEventStream`, `push_pointer_move`, `push_pointer_down`,
  `push_pointer_up`, `push_key_down`, `push_key_up`, `push_ime_preedit`, `push_ime_commit`, `push_resize`,
  `push_focus_gained`, `push_focus_lost`, `LayoutTree`, `LoginFormNodes`, `append_root_column`, `append_column`,
  `append_leaf`, `append_label`, `append_button`, `append_input`, `append_panel`, `append_login_form`, `measure`,
  `arrange`, deterministic `serialize()` output), and the current host bridge can blit a deterministic BGRA8 software
  surface through the native window presenter and macOS Metal host paths while the shared widget/layout model can also
  emit deterministic HTML/backend adapter records and normalize pointer, keyboard, IME, resize, and focus input into
  deterministic UI event-stream records. No active TODO currently tracks platform/runtime consumption of that shared
  event stream. Add a concrete TODO before changing that UI runtime seam; composite-widget composition remains locked
  to the basic widget/container APIs rather than raw draw-command helpers or raw HTML record append helpers.
- **IR definition (stable, PSIR v21):**
  - **Module:** `{ string_table, struct_layouts, functions, instruction_source_map, entry_index, version }`.
  - **Function:** `{ name, metadata, local_debug_slots, instructions }` where instructions are linear, stack-based ops
    with immediates and debug IDs.
  - **Metadata:** `{ effect_mask, capability_mask, scheduling_scope, instrumentation_flags }` (see PSIR binary layout).
  - **Instruction:** `{ op, imm, debug_id }`; `op` is an `IrOpcode`, `imm` is a 64-bit immediate payload whose meaning
    depends on `op`, and `debug_id` is a deterministic per-instruction identifier used for source-map linkage.
  - **Instruction source map entry:** `{ debug_id, line, column, provenance }`; entries map instruction debug IDs back
    to canonical AST statement/expression coordinates when available (including inlined callee statements) and
    provenance tags (`canonical_ast` for direct statement/expression mappings, `synthetic_ir` for compiler-generated
    instructions). Instructions with no direct AST origin currently fall back to definition coordinates.
  - **Locals:** addressed by index; `LoadLocal`, `StoreLocal`, `AddressOfLocal` operate on the index encoded in `imm`.
  - **Strings:** string literals are interned in `string_table` and referenced by index in print ops (see PSIR
    versioning).
  - **Entry:** `entry_index` points to the entry function in `functions`; its signature is enforced by the front-end.
  - **PSIR binary layout (little-endian):**
    - `u32 magic` (`0x50534952` = `"PSIR"`), `u32 version`, `u32 function_count`, `u32 entry_index`, `u32 string_count`.
    - `string_count` entries: `u32 byte_len` + raw bytes.
    - `u32 struct_count`.
    - `struct_count` entries: `u32 name_len` + name bytes, `u32 total_size`, `u32 alignment`, `u32 field_count`, then
      `field_count` entries:
      `u32 field_name_len` + bytes, `u32 envelope_len` + bytes, `u32 offset`, `u32 size`, `u32 alignment`,
      `u32 padding_kind`, `u32 category`, `u32 visibility`, `u32 is_static`.
    - `function_count` entries: `u32 name_len` + name bytes, `u64 effect_mask`, `u64 capability_mask`,
      `u32 scheduling_scope`, `u32 instrumentation_flags`, `u32 local_debug_count`, then `local_debug_count` entries:
      `u32 slot_index`, `u32 name_len` + name bytes, `u32 type_len` + type bytes, then `u32 instruction_count` and
      `instruction_count` entries: `u8 opcode` + `u64 imm` + `u32 debug_id`.
    - `u32 instruction_source_map_count`, then `instruction_source_map_count` entries:
      `u32 debug_id`, `u32 line`, `u32 column`, `u8 provenance`.
  - **PSIR opcode set:** see the `IrOpcode` enum and the “PSIR opcode set (v21, VM/native)” section below.
- **PSIR versioning:** serialized IR includes a version tag; v2 introduces `AddressOfLocal`, `LoadIndirect`, and
  `StoreIndirect` for pointer/reference lowering; v4 adds `ReturnVoid` to model implicit void returns in the VM/native
  backends; v5 adds a string table + print opcodes for stdout/stderr output; v6 extends print opcodes with
  newline/stdout/stderr flags to support `print`/`print_line`/`print_error`/`print_line_error`; v7 adds `PushArgc` for
  entry argument counts in VM/native execution; v8 adds `PrintArgv` for printing entry argument strings; v9 adds
  `PrintArgvUnsafe` to emit unchecked entry-arg prints for `at_unsafe`; v10 adds `LoadStringByte` for string indexing in
  VM/native backends; v11 adds struct layout manifests; v12 adds struct field visibility/static metadata; v13 adds float
  arithmetic/compare/convert opcodes; v14 adds float return opcodes (`ReturnF32`, `ReturnF64`); v15 adds per-function
  execution metadata (effect/capability masks plus scheduling/instrumentation fields); v16 adds `Call` and `CallVoid`
  function-call opcodes for callable IR; v17 adds per-function local debug slot metadata (`slot_index`, `name`, `type`)
  without runtime semantic changes; v18 adds per-instruction debug IDs for source-map linkage; v19 adds per-instruction
  source-map metadata entries keyed by instruction debug ID (`line`, `column`, `provenance`); v20 adds `FileReadByte`
  for deterministic single-byte file reads with explicit EOF mapping and `HeapFree` for `/std/intrinsics/memory/free`;
  v21 adds `HeapRealloc` for `/std/intrinsics/memory/realloc`.
  - **PSIR v2:** adds pointer opcodes (`AddressOfLocal`, `LoadIndirect`, `StoreIndirect`) to support
    `location`/`dereference`.
  - **PSIR v4:** adds `ReturnVoid` so void definitions can omit explicit returns without losing a bytecode terminator.
  - **Versioning policy:** the `version` field is a single, monotonically increasing integer for incompatible changes.
    There is no forward/backward compatibility guarantee today; tools reject unknown versions and require recompilation.
    Migration tooling may be added later, but no automatic migrations exist yet.
- **Backends:**
  - **C++ emitter** – generates host code for native binaries.
  - **GLSL emitter** – produces shader code; SPIR-V output is available via `--emit=spirv`.
  - **VM bytecode** – compact instruction set executed by the embedded interpreter/JIT.
- **Tooling:** CLI compiler `primec`, plus the VM runner `primevm` and build/test helpers. The compiler accepts `--entry
  /path` to select the entry definition (default: `/main`). Import search roots are configured with `--import-path
  <dir>` (or `-I <dir>`). Entry definitions currently accept either no parameters or a single `[array<string>]`
  parameter for command-line arguments; `args.count()` and `count(args)` are supported, and checked indexing is
  available via either `args[index]` or `args.at(index)` (`at_unsafe(args, index)` / `args.at_unsafe(index)` skips
  checks). String bindings may be initialised from checked/unchecked entry-arg indexing (print-only). The C++ emitter
  maps the array argument to `argv` and otherwise uses the same restriction. The definition/execution split maps cleanly
  to future node-based editors; full IDE/LSP integration is deferred until the compiler stabilises.
- **AST/IR dumps:** the debug printers include executions with their argument lists so tooling can capture scheduling
  intent in snapshots.
  - Dumps show collection literals after text-transform rewriting while preserving brace construction (e.g.,
    `array<i32>{1i32,2i32}` remains `array<i32>{1i32,2i32}`).
  - Labeled execution arguments appear inline (e.g., `exec /execute_task([count] 2)`).

## Language Design Highlights
- **Identifiers:** `[A-Za-z_][A-Za-z0-9_]*` plus the slash-prefixed form `/segment/segment/...` for fully-qualified
  paths. Unicode may arrive later, but identifiers are constrained to ASCII for predictable tooling and hashing. `auto`,
  `mut`, `return`, `import`, `namespace`, `true`, `false`, `if`, `else`, `loop`, `while`, and `for` are reserved
  keywords; any other identifier (including slash paths) can serve as a transform, path segment, parameter, or binding.
-- **String literals:** surface forms accept `"..."utf8` / `"..."ascii` with escape processing, or raw `'...'utf8` /
`'...'ascii` with no escape processing. The `implicit-utf8` text transform (enabled by default) appends `utf8` when
omitted in surface syntax. **Canonical/bottom-level form uses double-quoted strings with escapes normalized and an
explicit `utf8`/`ascii` suffix.** `ascii` enforces 7-bit ASCII (the compiler rejects non-ASCII bytes). Example surface:
`"hello"utf8`, `'moo'ascii`. Example canonical: `"hello"utf8`. Raw example: `'C:\temp'ascii` keeps backslashes verbatim.
- **Numeric envelopes:** fixed-width `i32`, `i64`, `u64`, `f32`, and `f64` map directly to hardware instructions and are
  the only numeric envelopes supported across all backends today. Software numeric envelopes `integer`, `decimal`, and
  `complex` are accepted by the parser/semantic validator (bindings, returns, collections, and `convert<T>` targets),
  but current backends reject them at lowering/emission time. Mixed software/fixed arithmetic is rejected, and mixed
  software categories or ordered comparisons on `complex` are also diagnostics today.
- **Core type set (v1):** the closed set of envelopes that every backend must understand and that the type system treats
  as cross-backend portable is:
  - `bool`, `i32`, `i64`, `u64`, `f32`, `f64`, `string`
  - `array<T>`, `vector<T>`, `map<K, V>`
  - `Pointer<T>`, `Reference<T>`
  - User-defined struct types (including `[struct]`-tagged definitions)
  - Draft math extension types (`Vec2`, `Vec3`, `Vec4`, `Mat2`, `Mat3`, `Mat4`, `Quat`) are currently backend-specific
    and are not part of this portable core set.
  Backends may accept additional types, but any type outside this core set is backend-specific and must be rejected by
  backends that do not explicitly support it. For collections, element/key/value types must themselves be in the core
  set unless a backend explicitly documents wider support.
- **Aliases:** none for numeric widths; use explicit `i32`, `i64`, `u64`, `f32`, `f64`.
- **`auto` (implicit templates + local inference):** `auto` may appear on binding envelopes, parameters, or return
  transforms. In parameter/return positions it introduces an implicit template parameter (equivalent to adding a fresh
  type parameter) and is inferred per call site; omitted parameter envelopes are treated as `auto`. In bindings, `auto`
  requests local inference from the initializer and must resolve to a concrete envelope; unresolved or conflicting local
  inference is a diagnostic. Return `auto` is constrained by return statements; if all constraints resolve to a concrete
  envelope the definition becomes monomorphic.
  - Float literals accept standard decimal forms, including optional fractional digits (e.g., `1.`, `1.0`, `1.f32`,
    `1.e2`).
- **Envelope:** the canonical AST uses a single envelope form for definitions and executions: `[transform-list]
  identifier<template-list>(parameter-or-arg-list) {body-list}`. Surface definitions require an explicit `{...}` body
  and may spell transforms either in canonical prefix form (`[transform-list] name(...) { ... }`) or in surface
  post-parameter form (`name(...) [transform-list] { ... }`), which is normalized to the prefix form before semantic
  validation/lowering. The post-parameter form is definition-only and requires `[]` to be followed by `{...}`. `name[]()
  { ... }` is rejected to avoid indexing-like ambiguity. Surface executions are call-style
  (`identifier<template-list>(arg-list)`) and map to an envelope with an implicit empty body. **Definitions may omit an
  empty parameter list** (e.g., `Foo { ... }` is treated as `Foo() { ... }`), and this is accepted even at the concrete
  level; executions still require parentheses. Bindings use the form `[Envelope qualifiers…] name{initializer}`. In
  inference/surface levels, locals and struct fields may omit the envelope annotation when the initializer resolves to
  one concrete envelope; unresolved or ambiguous inference is a diagnostic. Struct field envelopes must be concrete
  before layout manifest emission. Parameters that omit an explicit envelope are treated as `auto` and become implicit
  template parameters inferred per call site. Lists recursively reuse whitespace-separated tokens.
  - Syntax markers: `[]` compile-time transforms, `<>` templates, `()` runtime parameters/calls, `{}` runtime code
    (definition bodies, binding initializers).
  - `[...]` enumerates metafunction transforms applied in order (see “Built-in transforms”).
  - **Bracket-list name binding rule:** `[...]` is contextual. Entries already known in the current syntactic position
    keep their transform, type-envelope, modifier, or label meaning. When the grammar expects a binding pattern, fresh
    identifiers inside the bracket list introduce new names. This preserves existing forms such as `[i32] count{0i32}`
    and `[mut] value{0i32}` while allowing future destructuring forms such as `[left right] pair`; mixed or ambiguous
    lists must be diagnosed deterministically rather than silently reinterpreted.
  - `<...>` supplies compile-time envelopes/templates—primarily for transforms or when inference must be overridden.
  - `(...)` lists runtime parameters; calls always include `()` (even with no args), and `()` never appears on bindings.
  - **Parameters:** use the same binding envelope as locals: `main([array<string>] args, [i32] limit{10i32})`.
    Qualifiers like `mut`/`copy` apply here as well; defaults are optional and currently limited to literal/pure forms
    (no name references).
  - `{...}` holds runtime code for definition bodies and value blocks for binding initializers. Binding initializers
    evaluate the block and use its resulting value (last item or `return(value)`); value construction uses brace
    constructor forms, including multi-field construction (e.g., `[T] name{T{arg1, arg2}}`). `Type(...)` is ordinary
    execution/call syntax and is not construction.
  - Bindings are only valid inside definition bodies or parameter lists; top-level bindings are rejected.
- **Draft variadic argument packs (parser + call semantics + read-only body API + spread call-lowering landed;
  backend/runtime materialization is now partial):** to support stdlib-owned `vector`/`map` implementations without
  hand-written `Single/Pair/Triple/...` constructor ladders, the surface syntax now parses rest parameters and spread
  calls while keeping the canonical meaning inside the envelope system.
  - Surface parameter sugar: `collect(values...) { ... }` now desugars during parsing to
    `collect<__pack_T>([args<__pack_T>] values) { ... }`.
  - Typed surface parameter sugar: `collect([string] values...) { ... }` now desugars during parsing to
    `collect([args<string>] values) { ... }`.
  - Surface call sugar: `build(values...)` inside a call now desugars to `[spread] values` on that argument node, and
    explicit canonical `[spread] values` is accepted directly in call-argument position.
  - Canonical parser form therefore uses a real pack envelope plus an explicit spread marker instead of storing syntax
    in bare identifier spelling. Call semantics now bind trailing positional arguments into that trailing pack, infer
    omitted pack element types homogeneously across packed values, reject named arguments targeting the variadic
    parameter directly, and allow a final `[spread] values` argument to forward an existing `args<T>` pack into another
    trailing variadic slot with the same omitted-pack inference path. The read-only body API is now available, while
    backend/runtime materialization remains partial and should get a new explicit TODO before further implementation
    work starts.
  - After monomorphisation, bottom-level form contains no templates. Example:
    - Surface:
      ```text
      collect(values...) { return(vector(values...)) }
      main() { [auto] xs{collect(1i32, 2i32, 3i32)} }
      ```
    - Canonical envelope form:
      ```text
      [return<vector<i32>>]
      collect__i32([args<i32>] values) {
        return(vector__i32([spread] values))
      }

      [return<void>]
      main() {
        [vector<i32>] xs{collect__i32(1i32, 2i32, 3i32)}
      }
      ```
  - Current v1 constraints: one `args<T>` parameter per definition, it must be last, it is homogeneous, named arguments
    bind only fixed parameters, and `[spread]` is only valid in call-argument position.
  - Current body API: `count(values)`, `values.count()`, `values[index]`, `at(values, index)`, `values.at(index)`, and
    `values.at_unsafe(index)` work on `args<T>` parameters.
  - Runtime status: the legacy C++ emitter now materializes concrete `args<T>` parameters for direct variadic calls plus
    mixed explicit-prefix + final-spread forwarding, and that emitted path executes the full read-only body API
    including downstream method resolution on indexed values. IR-backed VM/native lowering now covers direct
    numeric/bool/string variadic calls, pure final-spread forwarding of an existing pack, and mixed explicit-prefix +
    final-spread forwarding rebuilt from known-size numeric/bool/string packs through the same array-like body API,
    including indexed downstream string helpers. Struct packs now also materialize for direct calls plus pure/mixed
    forwarding across `count(...)`, checked/unchecked access, and downstream indexed field/helper resolution. `Result<T,
    Error>` packs now use the same IR storage across direct/pure/mixed forwarding and preserve indexed `Result.why(...)`
    / `?` behavior on VM/native, status-only `Result<Error>` packs preserve indexed `Result.error(...)` /
    `Result.why(...)` behavior across those same forwarding modes, `FileError` packs preserve indexed downstream `why()`
    mapping, `Reference<FileError>` packs preserve indexed downstream `dereference(...).why()` mapping,
    `Pointer<FileError>` packs preserve indexed downstream `dereference(...).why()` mapping, `Reference<Result<T,
    Error>>` packs preserve indexed downstream `dereference(...)`, `try(...)`, and `Result.why(...)` access, status-only
    `Reference<Result<Error>>` packs preserve indexed downstream `dereference(...)`, `Result.error(...)`, and
    `Result.why(...)` access, `Pointer<Result<T, Error>>` packs preserve indexed downstream `dereference(...)`,
    `try(...)`, and `Result.why(...)` access including payload-kind inference for `auto` bindings on indexed `try(...)`
    results, and status-only `Pointer<Result<Error>>` packs preserve indexed downstream `dereference(...)`,
    `Result.error(...)`, and `Result.why(...)` access. `File<Mode>` packs preserve indexed downstream file-handle method
    access, `Reference<File<Mode>>` packs preserve indexed downstream file-handle method access alongside explicit
    `dereference(...).write*` / `flush()` receiver forms plus helper-style `at(values, i).write*()` /
    `values.at(i).flush()` receivers, canonical free-builtin `at([values] values, [index] i).write*()` / `.flush()`
    receivers, and direct indexed `readByte(...)` `?` inference plus canonical free-builtin `at([values] values,
    [index] i).readByte(...)` `?`, `Pointer<File<Mode>>` packs preserve indexed downstream file-handle method access
    alongside explicit `dereference(...).write*` / `flush()` receiver forms plus helper-style `at(values, i).write*()` /
    `values.at(i).flush()` receivers, canonical free-builtin `at([values] values, [index] i).write*()` / `.flush()`
    receivers, and direct indexed `readByte(...)` `?` inference plus canonical free-builtin `at([values] values,
    [index] i).readByte(...)` `?`, `Buffer<T>` packs preserve indexed downstream `buffer_load(...)` and
    `buffer_store(...)` on the IR/VM GPU path, `Reference<Buffer<T>>` packs preserve indexed downstream
    `buffer_load(dereference(...), ...)` and `buffer_store(dereference(...), ...)` on that same IR/VM GPU path,
    `Pointer<Buffer<T>>` packs preserve indexed downstream `buffer_load(dereference(...), ...)` and
    `buffer_store(dereference(...), ...)` on that same IR/VM GPU path, `array<T>`, `Reference<array<T>>`,
    `Pointer<array<T>>`, `vector<T>`, `Reference<vector<T>>`, `Pointer<vector<T>>`, empty/header-only `soa_vector<T>`,
    `Reference<soa_vector<T>>`, `Pointer<soa_vector<T>>`, `map<K, V>`, `Reference<map<K, V>>`, plus `Pointer<map<K, V>>`
    packs now preserve indexed downstream `count()` resolution across the same forwarding modes, `vector<T>` packs also
    preserve indexed downstream `capacity()` and statement-mutator access, borrowed/pointer array and vector packs
    preserve explicit indexed `dereference(...)` receiver wrappers for downstream checked/unchecked access,
    borrowed/pointer vector packs preserve that same indexed `capacity()` and statement-mutator surface through explicit
    `dereference(...)` receiver wrappers, borrowed/pointer map packs preserve that same count and lookup surface through
    explicit indexed `dereference(...)` receiver wrappers, those same value, borrowed, and pointer map packs preserve
    indexed downstream `tryAt(...)` payload-kind inference for `auto` bindings, and `Pointer<map<K, V>>` packs preserve
    indexed downstream `contains()` / `at()` / `at_unsafe()` lookup access. Scalar `Pointer<T>` plus scalar
    `Reference<T>` packs now preserve indexed downstream `dereference(...)`, and struct `Pointer<T>` plus struct
    `Reference<T>` packs now preserve indexed downstream field/helper access. Any newly discovered unsupported
    non-string pack element should get a concrete TODO only after a reproducible semantics, lowering, or backend
    failure is identified.
    Latest checkpoint: canonical free-builtin `at([values] values, [index] i)` on wrapped borrowed/pointer `File<Mode>`
    arg-packs now preserves `write*()` / `flush()` receivers plus `readByte(...)` `?` inference across direct calls
    plus pure/mixed spread forwarding, while wrapped `FileError` free-builtin named access remains on the existing
    named-argument rejection path.
- **Definitions vs executions:** definitions include a body (`{…}`) and optional transforms; executions are call-style
  (`execute_task<…>(args)`) with mandatory parentheses and no body, and map to an envelope with an implicit empty body.
  Calls always use `()`; the `name{...}` form is reserved for bindings so `execute_task{...}` is invalid.
  - Executions accept the same argument syntax as calls, including labeled arguments (`[param] value`).
  - Nested forms inside execution arguments still follow their own rules (e.g., labeled entries are valid in
    `array<i32>{[first] 1i32}` only if that collection form explicitly supports them).
  - Example: `execute_task([items] array<i32>{1i32, 2i32} [pairs] map<i32, i32>{1i32=2i32})`.
  - **Definition order:** call sites may reference definitions that appear later in the same file or namespace. Name
    resolution runs after import expansion and namespace expansion; unresolved names remain diagnostics.
  - **Helper overloading:** non-struct definitions may reuse the same public helper path when their exact parameter
    counts differ. Call resolution picks the exact-arity match before template specialization and method-call lowering,
    while same-path same-arity definitions are still duplicates. Minimal example:
    ```text
    [return<i32>] /helper/value<T>([T] value) { return(1i32) }
    [return<i32>] /helper/value<A, B>([A] first, [B] second) { return(2i32) }
    [return<int> effects(io_out)]
    main() {
      print_line(/helper/value(7i32))
      print_line(/helper/value(7i32, true))
      return(0i32)
    }
    ```
  - Note: current VM/native/GLSL/C++ emitters only generate code for definitions; top-level executions are
    parsed/validated but not emitted (tooling-only for now).
- **Return annotation:** definitions declare return envelopes via transforms (e.g., `[return<f32>] blend<…>(…) { … }`).
  Definitions return values explicitly (`return(value)`); the desugared form is always canonical.
- **Surface vs canonical:** surface syntax may omit the return transform or use `return<auto>` (or `[auto]` with
  `single_type_to_return`) and rely on inference; canonical/bottom-level syntax always carries an explicit concrete
  `return<T>` after monomorphisation, and the base-level tree contains no templates or `auto`. Example surface: `main()
  { return(0) }` → canonical: `[return<i32>] main() { return(0i32) }`. If all return paths yield no value,
  `return<auto>` resolves to `return<void>`.
- **Default convenience:** the `single_type_to_return` transform rewrites a single bare envelope in the transform list
  into `return<envelope>` (e.g., `[i32] main()` → `[return<i32>] main()`), and it is enabled by default (disable via
  `--no-semantic-transforms` or override the semantic transform list). If the bare envelope is `auto`, the transform
  yields `return<auto>` and inference resolves it later.
Array returns use `return<array<T>>` (or `[array<T>]` with `single_type_to_return`) and surface as `array` in the IR.
Struct returns use `return<StructName>` (or inference when the body returns a struct constructor/value); they surface as
`array` in the IR with the struct layout manifest attached.

Example:
```
[return<array<i32>>]
make_pair() {
  return(array<i32>{1i32, 2i32})
}
```

Expected IR (shape only):
```
module {
  def /make_pair(): array {
    return array(1, 2)
  }
}
```
- **Effects:** by default, definitions/executions start with `io_out` enabled so logging works without explicit
  annotations. Authors can override with `[effects(...)]` (e.g., `[effects(global_write, io_out)]`) or tighten to pure
  behavior by passing `primec --default-effects=none`. Standard library routines permit stdout/stderr logging via
  `io_out`/`io_err`; backends reject unsupported effects (e.g., GPU code requesting filesystem access). `primec
  --default-effects <list>` supplies the default effect set for any definition/execution that omits `[effects]`
  (comma-separated list; `default` and `none` are supported tokens). If `[capabilities(...)]` is present it must be a
  subset of the active effects (explicit or default). VM/native accept `io_out`, `io_err`, `heap_alloc`, `file_read`,
  `file_write`, `gpu_dispatch`, and `pathspace_*` effects (`pathspace_notify`, `pathspace_insert`, `pathspace_take`,
  `pathspace_bind`, `pathspace_schedule`); `file_write` also implies `file_read` for compatibility. GLSL accepts
  `io_out`, `io_err`, plus `pathspace_*` metadata effects/capabilities.
- **Execution effects:** executions may also carry `[effects(...)]`. The execution’s effects must be a subset of the
  enclosing definition’s active effects; otherwise it is a diagnostic. The default set is controlled by
  `--default-effects` in the compiler/VM.

### Backend Type Support (v1)
- **VM/native:** scalar `i32`, `i64`, `u64`, `bool`, `f32`, `f64`. `array`/`vector`/`map` support numeric/bool values;
  map string keys must be string literals or literal-backed bindings. Strings are limited to literals/literal-backed
  bindings for print/map contexts; string returns are supported for literal-backed values, while string arrays and
  string pointers/references are rejected. `convert<T>` supports `i32`, `i64`, `u64`, `bool`, `f32`, `f64`.
- **VM/native emitter restrictions (current):** recursive calls are rejected; lambdas are rejected (use the C++
  emitter); string comparisons are rejected and string literals are limited to print/count/index/map contexts; string
  array returns and string pointer/reference bindings are rejected; block arguments on non-control-flow calls and
  arguments on `if` branch blocks are rejected; `print*` and vector helper calls are statement-only; `File<Mode>(path)`
  requires a string literal or literal-backed binding; `Result.ok(value)` plus `Result.map(...)`,
  `Result.and_then(...)`, and `Result.map2(...)` currently accept `i32`, `bool`, `f32`, literal-backed `string`,
  ordinary user structs whose fields lower through the existing stack-backed struct path, the current single-slot
  int-backed stdlib error structs (`FileError`, `ImageError`, `ContainerError`, `GfxError`), packed `File<Mode>`
  handles, and `Buffer<T>` handles when downstream `try(...)` consumers are explicitly typed. Downstream `try(...)`
  preserves those handle/error-struct payloads alongside `array<T>` / `vector<T>` and `map<K, V>` handles whose element
  or key/value kinds already fit the current collection contract. IR-backed `[args<Result<T, Error>>]`,
  `[args<Reference<Result<T, Error>>>]`, and `[args<Pointer<Result<T, Error>>>]` packs now preserve indexed `try(...)`,
  `Result.error(...)`, and `Result.why(...)` access across direct, pure-spread, and mixed variadic forwarding when `T`
  already fits that same payload contract, and native executable `Result<Buffer<T>, GfxError>` values now preserve
  `try(...)`, `Result.error(...)`, and success/error `Result.why(...)` on that same explicit typed path. Unsupported
  math or GPU builtins fail.
- **GLSL:** numeric/bool scalar locals (`i32`, `i64`, `u64`, `bool`, `f32`, `f64`) plus nominal `Vec2`, `Vec3`, `Vec4`,
  `Quat`, `Mat2`, `Mat3`, and `Mat4` bindings; string literals and other non-supported composites are rejected, and
  entry definitions must return `void`. `convert<T>` targets match the numeric/bool list above.
- **GLSL emitter restrictions (current):** at most one `return()` statement; static bindings are rejected;
  assign/increment/decrement require local mutable targets; control flow must use canonical forms (`if(cond, then() {
  ... }, else() { ... })`, `loop(count, body() { ... })`, `while(cond, body() { ... })`, `for(init, cond, step, body() {
  ... })`); builtins require positional args with no template/block arguments, and unsupported builtins fail.
- **GLSL type support (current):** scalar `bool`, `i32`, `u32`, `i64`, `u64`, `f32`, `f64` plus nominal `Vec2`, `Vec3`,
  `Vec4`, `Quat`, `Mat2`, `Mat3`, and `Mat4`. Using `i64`/`u64` or `f64` emits
  `GL_ARB_gpu_shader_int64`/`GL_ARB_gpu_shader_fp64` requirements. Arrays, strings, general structs,
  pointers/references, maps, and other unsupported composites are rejected.
- **Matrix/quaternion status (draft):** VM/native, Wasm, and the C++ emitter support stdlib matrix/quaternion nominal
  values, conversion helpers, component-wise `Mat2`/`Mat3`/`Mat4` and `Quat` `plus` + `minus`, matrix/quaternion scalar
  scaling + divide, matrix-vector multiply, matching matrix-matrix multiply, quaternion Hamilton products, and
  quaternion-`Vec3` rotation. GLSL now lowers nominal `Vec2`/`Vec3`/`Vec4`, `Quat`, `Mat2`, `Mat3`, and `Mat4` values,
  direct vector/quaternion/matrix field access, component-wise vector/quaternion `plus`/`minus`, vector/quaternion
  scalar scale/divide, `MatN * VecN` interop, matching matrix-matrix multiply, quaternion Hamilton products,
  quaternion-`Vec3` rotation, and the explicit quaternion conversion helpers `quat_to_mat3`, `quat_to_mat4`, and
  `mat3_to_quat`.
- **GLSL effects/capabilities (current):** `io_out`, `io_err`, and `pathspace_*` metadata entries are accepted; other
  effects/capabilities are rejected. `print*` calls are accepted but emitted as no-op expressions.
- **GLSL determinism (current):** only local scalar plus nominal vector/matrix bindings are allowed; no static storage
  or heap/placement transforms. GPU backends are treated as deterministic with no external I/O.
- **GPU compute (draft):**
  - A definition tagged with `[compute]` is lowered as a GPU kernel. Kernels are `void` and write outputs via buffer
    parameters rather than return values.
  - `workgroup_size(x, y, z)` fixes the local group size for the kernel; only valid alongside `[compute]`.
  - Kernel bodies are restricted to the GPU-safe subset (POD/`gpu_lane` types, fixed-width numeric envelopes, no IO, no
    heap, no strings, no recursion). Backends reject unsupported features early with diagnostics.
  - Host-side submission uses `/std/gpu/dispatch(kernel, gx, gy, gz, args...)` and requires `effects(gpu_dispatch)`.
  - VM/native fallback currently requires `/std/gpu/buffer<T>(count)` to use a constant `i32` literal size.
  - GPU builtins live under `/std/gpu/*` (see Core library surface).
  - GPU ID helpers are scalar: `/std/gpu/global_id_x()`, `/std/gpu/global_id_y()`, `/std/gpu/global_id_z()` return
    `i32`.
- **Capability taxonomy (v1):**
  - **IO:** `io_out` (stdout), `io_err` (stderr), `file_read` (filesystem input), `file_write` (filesystem output; also
    implies `file_read`).
  - **Memory:** `heap_alloc` (dynamic allocation), `global_write` (mutating global state).
  - **Assets:** `asset_read`, `asset_write` (asset/database I/O).
  - **GPU:** `gpu_dispatch` (host-side GPU submission/dispatch).
  - **PathSpace (internal):** `pathspace_notify`, `pathspace_insert`, `pathspace_take`, `pathspace_bind`,
    `pathspace_schedule` (host metadata/event hooks; currently treated as backend metadata/no-op operations).
  - Unknown capability names are errors; capability identifiers are `lower_snake_case`.
- **Tooling vs runtime visibility:**
  - **Tooling surfaces:** declared effects/capabilities, resolved defaults, entry defaults, and backend allowlist
    violations (diagnostics).
  - **Runtime-only logs:** resolved effect/capability masks and execution identifiers (path hashes) for tracing; source
    spans are optional/debug-only.
- **Paths & imports:** every definition/execution lives at a canonical path (`/ui/widgets/log_button_press`). Authors
  can spell the path inline or rely on `namespace foo { ... }` blocks to prepend `/foo` automatically. Import expansion
  produces a single compilation unit; implicit-template inference may use call sites anywhere in that unit; there are no
  module boundaries. Import paths are parsed before text transforms, so they remain quoted without literal suffixes.
  - **Source imports:** `import<"/std/io", version="1.2.0">` resolves packages from the import path (zipped archive or
    plain directory) whose layout mirrors `/version/first_namespace/second_namespace/...`. The angle-bracket list may
    contain multiple quoted string paths—`import<"/std/io", "./local/io/helpers", version="1.2.0">`—and the resolver
    applies the same version selector to each path; mismatched archives raise an error before expansion. Versions live
    in the leading segment (e.g., `1.2/std/io/*.prime` or `1/std/io/*.prime`). If the version attribute provides one or
    two numbers (`1` or `1.2`), the newest matching archive is selected; three-part versions (`1.2.0`) require an exact
    match. Each `.prime` source file is expanded exactly once and registered under its namespace/path (e.g., `/std/io`);
    duplicate imports are ignored. Folders prefixed with `_` remain private.
    Legacy `include<...>` source imports are removed; use `import<...>` only.
    Legacy `--include-path` is also removed; configure import roots with
    `--import-path` (or `-I`).
  - **Namespace imports:** `import /foo/*` brings the immediate **public** children of `/foo` into the root namespace.
    `import /foo/bar` brings a single **public** definition (or builtin) by its final segment; importing a non-public
    definition is a diagnostic. `import /foo` is shorthand for `import /foo/*` (except `/std/math`, which is unsupported
    without `/*` or an explicit name). `import /std/math/*` brings all math builtins into the root namespace, or import
    a subset via `import /std/math/sin /std/math/pi`; `import /std/math` without a wildcard or explicit name is not
    supported. Imports are resolved after source imports and can be listed as `import /std/math/*, /util/*` or
    whitespace-separated paths.
  - **Exports:** definitions are private by default. Add `[public]` to a definition (function, struct, method) to make
    it importable; `[private]` explicitly marks it as non-exported. Private definitions are still callable within the
    same compilation unit; visibility only affects imports.
- **Transform-driven control flow:** control structures desugar into prefix calls that accept envelope arguments.
  Surface `if(condition) { … } else { … }` rewrites into `if(condition, then() { … }, else() { … })`. Surface
  `if(condition) { … }` (without `else`) is statement-only sugar and rewrites into `if(condition, then() { … }, else() {
  })`. `loop(count) { … }`, `while(condition) { … }`, and `for(init cond step) { … }` rewrite into `loop(count, do() { …
  })`, `while(condition, do() { … })`, and `for(init, cond, step, do() { … })`. `match(condition) { … } else { … }`
  rewrites into `match(condition, then() { … }, else() { … })` and behaves like `if` for boolean conditions. Value
  matching uses the canonical call form `match(value, case(pattern) { … }, case(other) { … }, else() { … })`; cases
  compare with `equal(...)`, and a final `else` block is required. The envelope names (`do`, `then`, `else`, `case`) are
  for readability only; any name is accepted and ignored by the compiler. Infix operators (`a + b`) become canonical
  calls (`plus(a, b)`), ensuring IR/backends see a small, predictable surface.
- **Mutability:** bindings are immutable by default. Opt into mutation by placing `mut` inside the stack-value execution
  or helper (`[Integer mut] exposure{42}`, `[mut] Create()`). Transforms enforce that only mutable bindings can serve as
  `assign` or pointer-write targets.

### Transform phases (draft)
- **Two phases:** text transforms operate on raw tokens before AST construction; semantic transforms operate on the
  parsed AST node.
- **Explicit grouping:** use `text(...)` and `semantic(...)` inside the transform list to force phase placement.
  - Example:
    ```
    [text(operators, collections, implicit-utf8) semantic(return<i32>, effects(io_out))]
    main([i32] a, [i32] b) {
      return(a + b)
    }
    ```
- **Auto-deduce by name:** transforms listed without `text(...)` or `semantic(...)` are assigned to their declared phase
  automatically. If a name exists in both registries, the compiler emits an ambiguity error.
- **Ordering:** the compiler scans transforms left-to-right, appending each to its phase list while preserving relative
  order within the text and semantic phases.
- **Scope:** a text transform may rewrite any token inside the enclosing definition/execution envelope (transform list,
  templates, parameters, and body). Nested definitions/lambdas receive their own transform lists.
- **Self-expansion:** text transforms may append additional text transforms to the same node; appended transforms run
  after the current transform.
- **Applicability limits (v1):**
  - **Definitions/executions only:** `return<T>`, `effects(...)`, `capabilities(...)`, `text(...)`, `semantic(...)`,
    `single_type_to_return`.
  - **Definitions only:** `compute`, `workgroup_size(x, y, z)`, `unsafe`, `ast`.
  - **Struct/tag only (definitions):** `struct`, `pod`, `handle`, `gpu_lane`, `align_bytes(n)`, `align_kbytes(n)`.
  - **Definitions/bindings:** access/visibility markers (`public`, `private`). `static` is valid on bindings and struct
    helpers (disables implicit `this` on helpers).
  - **Reserved/rejected in v1:** `stack`, `heap`, `buffer` placement transforms (diagnostic).
  - Any transform outside its allowed scope is a compile-time error with a diagnostic naming the enclosing path.
- **User-authored AST hooks (narrow executable v1):** declare metadata-only hooks with
  `[ast return<void>] hook_name() { ... }`, or executable definition rewrites with
  `[ast return<FunctionAst>] hook_name([FunctionAst] fn) { return(replace_body_with_return_i32(fn, 7i32)) }`.
  Imported hooks must also be `public`; private hooks remain visible only to local definitions in the same expanded
  source. A definition attaches the hook by spelling `[hook_name return<T>]` or `semantic(hook_name)` in its transform
  list. Resolution records the hook's full path on the transform metadata, rejects ambiguous imports, rejects private
  imported hooks, and rejects `text(hook_name)` because AST hooks are semantic-phase metadata. Executable v1 hooks are
  compile-time only: the semantic pipeline evaluates the single supported `FunctionAst` result helper through the
  syntax-owned `ct-eval ast-transform adapter`, rewrites the touched definition body before downstream
  validation/lowering, and removes hook definitions from the runtime program. The adapter maps only
  `replace_body_with_return_i32(fn, value)` to `/ct_eval/replace_body_with_return_i32` and emits deterministic
  diagnostics for unknown helper targets or contradictory helper inputs. The checked-in example module and consumer live
  under `examples/4.Transforms/`.

### Example function syntax
```
import /std/math/*
namespace demo {
  [return<void> effects(io_out)]
  hello_values() {
    [string] message{"Hello PrimeStruct"utf8}
    [i32] iterations{3i32}
    [f32 mut] exposure{1.25f32}
    [float3] tone_curve{float3(0.8f32, 0.9f32, 1.1f32)}

    print_line(message)
    assign(exposure, clamp(exposure, 0.0f32, 2.0f32))
    loop(iterations) {
      apply_curve(tone_curve, exposure)
    }
  }
}
```
Statements are separated by whitespace (including newlines). Commas and semicolons are ignored separators with no
meaning outside numeric literals (where commas may act as digit separators). PrimeStruct does not distinguish between
statements and envelopes—any envelope can stand alone as a statement, and unused values are discarded (for example,
`helper()` or `1i32` can appear as standalone statements).
When an expression is immediately followed by a binding transform list (`[...] name{...}`), the parser treats the
binding as a new statement rather than indexing sugar on the previous expression; use `at(value, index)` /
`value.at(index)`
or a semicolon if you intended to index.

### Slash paths & textual operator transforms
- Slash-prefixed identifiers (`/pkg/module/thing`) are valid anywhere the Envelope expects a name; `namespace foo { ...
  }` is shorthand for prepending `/foo` to enclosed names, and namespaces may be reopened freely.
- Text transforms run before the AST exists. Operator transforms scan the raw character stream and rewrite when they see
  a left operand and right operand, allowing optional whitespace around the operator token. Slash paths remain intact
  when `/` begins a path segment with no left operand (start of line or immediately after whitespace/delimiters). Binary
  operators respect standard precedence and associativity: `*`/`/` bind tighter than `+`/`-`, comparisons (`<`, `>`,
  `<=`, `>=`, `==`, `!=`) bind tighter than `&&`/`||`, and assignment (`=`) is lowest precedence and right-associative.
  Operators follow the same operand-based rule (`a > b` → `greater_than(a, b)`, `a < b` → `less_than(a, b)`, `a >= b` →
  `greater_equal(a, b)`, `a <= b` → `less_equal(a, b)`, `a == b` → `equal(a, b)`, `a != b` → `not_equal(a, b)`, `a && b`
  → `and(a, b)`, `a || b` → `or(a, b)`, `!a` → `not(a)`, `-a` → `negate(a)`, `a = b` → `assign(a, b)`, `++a` / `a++` →
  `increment(a)`, `--a` / `a--` → `decrement(a)`).
- Because imports expand first, slash paths survive every transform untouched until the AST builder consumes them, and
  IR lowering never needs to reason about infix syntax.

### Struct & envelope categories
- **Struct tag as transform:** any of `[struct]`, `[pod]`, `[handle]`, or `[gpu_lane]` marks the envelope as a
  struct-style definition. It records a layout manifest (field names, envelopes, offsets) and validates the body, but
  the underlying syntax remains a standard definition. Struct-tagged definitions are field-only: no parameters or return
  transforms, and no return statements. The body may include nested helper definitions; only stack-value bindings
  contribute to layout. Non-lifecycle helpers lower into the struct method namespace (`/<Struct>/name`) and can be
  called via method sugar or as plain namespace functions. Un-tagged definitions may still be instantiated as structs;
  they simply skip the extra validation/metadata until another transform demands it.
- **Placement policy:** where a value lives (stack/heap/buffer) is decided by allocation helpers plus capabilities, not
  by struct tags. Envelopes may express requirements (e.g., `pod`, `handle`, `gpu_lane`), but placement is a call-site
  decision gated by capabilities. The `stack`/`heap`/`buffer` transforms remain reserved and rejected in v1.
- **POD tag as validation:** `[pod]` on a struct-style definition asserts trivially-copyable semantics. Violations
  (hidden lifetimes, handles, async captures) raise diagnostics; without the tag the compiler treats the body
  permissively.
- **Member syntax:** every field is a stack-value binding (`[f32 mut] exposure{1.0f32}`, `[handle<PathNode>]
  target{get_default()}`). In inference/surface levels, fields may omit the envelope when the initializer resolves
  concretely (e.g., `center{Vec3{0.0f32, 0.0f32, 0.0f32}}`). Attributes (`[mut]`, `[align_bytes(16)]`,
  `[handle<PathNode>]`) decorate the execution, and transforms record the metadata for layout consumers.
- **Method calls & indexing:** `value.method(args...)` desugars to `/<envelope>/method(value, args...)` in the method
  namespace (no hidden object model), where `<envelope>` is the envelope name associated with `value`. For struct
  helpers with implicit `this`, the receiver becomes the hidden `this`
  parameter. Static helpers reject value-receiver method-call sugar
  (`value.helper()`), but accept type-qualified dot calls such as
  `Counter.defaultStep()` as a surface form for the same direct helper path.
  For collections, method syntax is the preferred surface style
  (`value.count()`, `value.at(i)`, `value.push(x)`), while helper calls remain
  canonical (`count(value)`, `at(value, i)`, `push(value, x)`). Vector helper
  surface syntax currently requires `import /std/collections/*` so the
  canonical stdlib wrappers are in scope. Indexing uses the safe helper by
  default: `value[index]` rewrites to `at(value, index)` with bounds checks and
  is equivalent to `value.at(index)`; `at_unsafe(value, index)` /
  `value.at_unsafe(index)` skips checks.
- **Struct body semantics (example):**
  ```
  [struct]
  BrushSettings {
    // Field bindings contribute to layout.
    [f32] size{12.0f32}
    [f32] hardness{0.75f32}

    // Helpers are ordinary definitions in /BrushSettings/* and do not affect layout.
    // Non-static helpers have an implicit `this` (Reference<Self>).
    [public return<f32>]
    clampSize([f32] value) {
      return(clamp(value, 1.0f32, 64.0f32))
    }

    // Method-call sugar supplies the implicit `this` (settings.normalize()).
    [public return<f32>]
    normalize() {
      return(this.clampSize(this.size))
    }

    // Static helper: no implicit `this`; call via BrushSettings.defaultSize().
    [public static return<f32>]
    defaultSize() {
      return(12.0f32)
    }

    // `mut` makes `this` writable inside the helper.
    [public mut return<void>]
    setSize([f32] value) {
      assign(this.size, this.clampSize(value))
    }
  }
  ```
- **Baseline layout rule:** members default to source-order packing. Backend-imposed padding is allowed only when the
  metadata (`layout.fields[].padding_kind`) records the reason; `[no_padding]` and `[platform_independent_padding]` fail
  the build if the backend cannot honor them bit-for-bit.
- **Alignment transforms:** `[align_bytes(n)]` (or `[align_kbytes(n)]`) may appear on the struct or field; violations
  again produce diagnostics instead of silent adjustments.
- **Stack value executions:** every local binding and struct field uses the binding form `name{initializer}` with
  optional envelope annotation (`[Type qualifiers…] name{initializer}`), so stack frames remain declarative (e.g., `[f32
  mut] exposure{1.0f32}`). Concrete level keeps the envelope explicit. In inference/surface levels, omitted envelopes
  are allowed only when inference resolves a single concrete envelope; struct fields must resolve before layout manifest
  emission. Default initializers are mandatory for fields; local bindings may omit the initializer only when the
  envelope is a struct type with a zero-argument constructor **and** the compiler can prove the construction has no
  outside effects. In the planned brace-only construction model, the binding desugars to `Type{}` (e.g.,
  `[BrushSettings] s` -> `[BrushSettings] s{BrushSettings{}}`).
  - **No outside effects (definition):** zero-arg construction is outside-effect-free only when all of the following
    hold:
    - The constructor path has an empty effects/capabilities mask (no `effects(...)`/`capabilities(...)`, and no callees
      with non-empty effects).
    - Writes are limited to the newly constructed value (`this`) and local temporaries; any writes through
      pointers/references or to non-local bindings are disallowed.
    - Field initializers are effect-free under the same rules, and all called helpers are effect-free transitively.
    - If the compiler cannot prove these constraints, omitted-initializer bindings are rejected.
  - Multi-step initializer example:
    ```
    [return<f32>]
    main() {
    [f32] x{
      [f32 mut] tmp{1.0f32}
      tmp = tmp + 2.0f32
      tmp
    }
      return(x)
    }
    ```
- **Lifecycle helpers (Create/Destroy):** Within a struct-tagged or field-only struct definition, nested definitions
  named `Create` and `Destroy` gain constructor/destructor semantics. Placement-specific variants add suffixes
  (`CreateStack`, `DestroyHeap`, etc.). Without these helpers the field initializer list defines the default
  constructor/destructor semantics. Struct helpers receive an implicit `this` unless marked `[static]`; add `mut` to the
  helper’s transform list when it writes to `this` (otherwise `this` stays immutable). Lifecycle helpers must return
  `void` and accept no parameters. We capitalise system-provided helper names so they stand out, but authors are free to
  use uppercase identifiers elsewhere—only the documented helper names receive special treatment.
  - **Helper visibility:** nested non-lifecycle helpers are normal definitions in the struct method namespace. Use
    `[public]` on the helper definition to export it; private helpers remain callable within the same compilation unit.
    Non-static helpers receive an implicit `this` (`Reference<Self>`); `[static]`
    disables the implicit `this` and value-receiver method-call sugar, but still
    allows type-qualified dot calls on the struct name.
  ```
  import /std/math/*
  namespace demo {
    [struct pod]
    color_grade() {
      [f32 mut] exposure{1.0f32}

      [mut]
      Create() {
        assign(this.exposure, clamp(this.exposure, 0.0f32, 2.0f32))
      }

      [effects(io_out)]
      Destroy() {
        print_line("color grade destroyed"utf8)
      }
    }
  }
  ```
- **IR layout manifest:** `[struct]` extends the IR descriptor with `layout.total_size_bytes`, `layout.alignment_bytes`,
  and ordered `layout.fields`. Each field record stores `{ name, envelope, offset_bytes, size_bytes, padding_kind,
  category }`. Placement transforms consume this manifest verbatim, ensuring C++, VM, and GPU backends share one source
  of truth.
- **Categories:** `[pod]`, `[handle]`, `[gpu_lane]` tags classify members for resource rules. Handles remain opaque
  tokens with subsystem-managed lifetimes; GPU lanes require staging transforms before CPU inspection.
- **Category mapping:**
  - `pod`: stored inline; treated as trivially-copyable for layout.
  - `handle`: stored as an opaque reference token; lifetime is managed by the owning subsystem.
  - `gpu_lane`: stored as a GPU-only handle; CPU access requires explicit staging transforms.
  - Un-tagged fields default to `default`.
  - Conflicts are rejected (`pod` with `handle` or `gpu_lane`, `handle` with `gpu_lane`, and `pod` definitions
    containing `handle`/`gpu_lane` fields).

### Transforms (draft)
- **Purpose:** transforms are metafunctions that rewrite tokens (text transforms) or stamp semantic flags on the AST
  (semantic transforms). Later passes (backend filters) consume the semantic flags; transforms do not emit code
  directly.
- **Evaluation mode:** when the compiler sees `[transform ...]`, it routes through the metafunction's declared
  signature—pure token rewrites operate on the raw stream, while semantic transforms receive the AST node and in-place
  metadata writers.
- **Registry note:** only registered text transforms can appear in `text(...)` groups or `--text-transforms`; only
  registered semantic transforms can appear in `semantic(...)` groups or `--semantic-transforms`. Other transform names
  are treated as semantic directives and validated by the semantics pass (they cannot be forced into `text(...)`).

**Text transforms (token-level, registered)**
- **`append_operators`:** injects `operators` into the leading transform list when missing, enabling text-transform
  self-expansion without repeating `operators` everywhere.
- **`operators`:** desugars infix/prefix operators, comparisons, boolean ops, assignment, and increment/decrement
  (`++`/`--`) into canonical calls (`plus`, `less_than`, `assign`, `increment`, etc.).
  - Example: `a = b` rewrites to `assign(a, b)`.
- **`collections`:** preserves `array<T>{...}` / `vector<T>{...}` / `map<K,V>{...}` as brace construction,
  normalizes bracket aliases to braces, and rewrites map `key=value` entries into alternating key/value entries inside
  the braces. Legacy compatibility lowering may still route call-shaped collection helpers through stdlib adapters.
- **`implicit-utf8`:** appends `utf8` to bare string literals.
- **`implicit-i32`:** appends `i32` to bare integer literals (enabled by default).
  - Text transform arguments are limited to identifiers and literals (no nested envelopes or calls).

**Semantic transforms (AST-level, registered)**
- **`single_type_to_return`:** semantic transform that rewrites a single bare envelope in a transform list into
  `return<envelope>` (e.g., `[i32] main()` → `[return<i32>] main()`); enabled by default, but can be disabled or
  overridden via `--no-semantic-transforms`, `--semantic-transforms`, or `--transform-list`.

**Semantic directives (AST-level, validated)**
- **`copy`:** force a copy (instead of a move) on entry for a parameter or binding. Only valid for `Copy` types;
  otherwise a diagnostic. Often paired with `mut`.
- **`mut`:** mark the local binding as writable; without it the binding behaves like a `const` reference. On
  definitions, `mut` is valid on struct helpers (including lifecycle helpers) to make the implicit `this` mutable; using
  `mut` on a `[static]` helper is a diagnostic. Executions do not accept `mut`.
- **`restrict<T>`:** constrain the accepted envelope to `T`. For bindings/parameters this is equivalent to writing the
  envelope directly (e.g., `[i32] x{...}`), and canonicalization rewrites `[i32]` into `[restrict<i32>]` at the low
  level.
- **`unsafe`:** marks a definition body as an unsafe scope. Aliasing rules are relaxed within the body, and
  `Reference<T>` bindings may use pointer-like initializers (`Pointer<T>`/`Reference<T>` values, including pointer
  arithmetic) when the pointee type matches. References created there must not escape the unsafe scope.
- **`return<T>`:** optional contract that pins the inferred return envelope. `return<auto>` requests inference;
  unresolved or conflicting returns are diagnostics.
- **`effects(...)`:** declare side-effect capabilities; absence implies purity. Backends reject unsupported
  capabilities.
- **Transform scope:** `effects(...)` and `capabilities(...)` are only valid on definitions/executions, not bindings.
- **`align_bytes(n)`, `align_kbytes(n)`:** encode alignment requirements for struct members and buffers. `align_kbytes`
  applies `n * 1024` bytes before emitting the metadata.
- **`no_padding`, `platform_independent_padding`:** layout constraints for struct definitions; they reject backend-added
  padding or non-deterministic padding respectively.
- **`capabilities(...)`:** reuse the transform plumbing to describe opt-in privileges without encoding backend-specific
  scheduling hints.
- **`compute`:** marks a definition as a GPU kernel; kernel bodies are validated against the GPU-safe subset.
- **`workgroup_size(x, y, z)`:** fixes the kernel's local workgroup size (must appear with `compute`).
- **`struct`, `pod`, `handle`, `gpu_lane`:** declarative tags that emit metadata/validation only. They never change
  syntax; instead they fail compilation when the body violates the advertised contract (e.g., `[pod]` forbids
  handles/async fields).
- **`public`, `private`:** visibility tags. On definitions, they control export visibility for imports (default:
  private). On bindings, they control field visibility (default: public). Mutually exclusive.
- **`static`:** on fields, hoists storage to namespace scope while keeping the field in the layout manifest. On struct
  helpers, disables the implicit `this` parameter and method-call sugar.
- **`reflect`:** marks a struct definition as reflection-enabled. It accepts no template arguments or call arguments and
  is rejected on non-struct definitions/executions. Use it when a type needs
  reflection metadata queries without requesting generated helpers.
- **`generate(...)`:** declares reflection helper generation intents for struct
  definitions. It accepts one or more generator names, implies reflection
  enablement for that type, and validates the generator allowlist (`Equal`,
  `NotEqual`, `Default`, `IsDefault`, `Clone`, `DebugPrint`, `Compare`,
  `Hash64`, `Clear`, `CopyFrom`, `Validate`, `Serialize`, `Deserialize`,
  `SoaSchema`) with deterministic diagnostics. Writing both `reflect` and
  `generate(...)` remains valid but is redundant when generation alone is
  sufficient.
  - **Naming contract:** generated helpers always use `/Type/Helper` paths (`/Type/Equal`, `/Type/Default`, etc.), where
    `Type` is the canonical struct path.
  - **Visibility contract:** generated helpers are emitted with explicit `[public]`, so `import /Type/*` exposes them by
    helper name.
  - **Collision/override contract:** generated helpers never silently override existing definitions; if any
    `/Type/Helper` path already exists, semantics reports `generated reflection helper already exists: /Type/Helper`.
  - **Current v1 generated helpers:** `Equal`, `NotEqual`, `Default`, `IsDefault`, `Clone`, and `DebugPrint`.
  - **`Equal`/`NotEqual`:** emits `/Type/Equal([Type] left, [Type] right) -> bool` and `/Type/NotEqual([Type] left,
    [Type] right) -> bool` with field-wise comparisons over non-static fields (`equal(...)` folded by `and`,
    `not_equal(...)` folded by `or`), using identity results when no instance fields exist (`Equal -> true`, `NotEqual
    -> false`). When `Equal` is generated for a reflected struct, surface `left == right` routes through the generated
    `/Type/Equal(left, right)` helper instead of requiring the explicit helper spelling.
  - **`Default`:** emits `/Type/Default() -> Type` returning `Type{}`; static fields are ignored by constructor entry
    mapping.
  - **`IsDefault`:** emits `/Type/IsDefault([Type] value) -> bool` by comparing non-static fields against a synthesized
    default instance (`Type{}`); structs with no instance fields return `true`.
  - **`Clone`:** emits `/Type/Clone([Type] value) -> Type` by rebuilding `Type{...}` from non-static fields
    (static-only structs clone as `Type{}`).
  - **`DebugPrint`:** emits `/Type/DebugPrint([Type] value) -> void` with deterministic line-based output (`/Type {`,
    one line per non-static field name, closing `}`, or `/Type {}` when no instance fields exist).
  - **v1.1 progress (`Compare`, `Hash64`, `Clear`, `CopyFrom`):** `Compare` emits `/Type/Compare([Type] left, [Type]
    right) -> i32` with lexicographic field ordering over non-static fields (`-1` when `left < right`, `1` when `left >
    right`, `0` when equal); `Hash64` emits `/Type/Hash64([Type] value) -> u64` and folds non-static fields in
    deterministic source order using `convert<u64>(field)` with stable FNV-style multiply/add constants; `Clear` emits
    `/Type/Clear([Type mut] value) -> void` and resets non-static fields by assigning from a synthesized default
    instance (`Type{}`); `CopyFrom` emits `/Type/CopyFrom([Type mut] value, [Type] other) -> void` and copies non-static
    fields from `other` into `value` in deterministic source order. Structs with no instance fields (or only static
    fields) keep identity/no-op behavior (`Compare -> 0`, `Hash64 -> 1469598103934665603u64`, `Clear -> no-op`,
    `CopyFrom -> no-op`), and `Compare`/`Hash64` now emit deterministic helper-scoped diagnostics when a field envelope
    is unsupported.
  - **v2 progress (`Validate`, `Serialize`, `Deserialize`):** `Validate` emits `/Type/Validate([Type] value) ->
    Result<FileError>` plus deterministic field-check scaffolding hooks `/Type/ValidateField_<field>([Type] value) ->
    bool` for each non-static field in source order. The generated hooks currently return `true` by default, and
    `/Type/Validate` short-circuits in source order: the first failing hook returns a deterministic non-zero error code
    (`1`-based field index), otherwise `Result.ok()`. `Serialize` emits `/Type/Serialize([Type] value) -> array<u64>`
    with a stable leading format tag (`1u64`) followed by `convert<u64>(field)` payload slots in deterministic
    non-static field order. `Deserialize` emits `/Type/Deserialize([Type mut] value, [array<u64>] payload) ->
    Result<FileError>`, enforces deterministic payload shape/version guards (`count(payload) == field_count + 1`,
    `at(payload, 0i32) == 1u64`), returns deterministic non-zero error codes (`1` = size mismatch, `2` = version
    mismatch), decodes payload slots in source order via `convert<fieldType>(at(payload, index))`, and returns
    `Result.ok()` on success. Structs with no instance fields (or only static fields) keep identity behavior (`Validate`
    returns `Result.ok()`, `Serialize` returns `array<u64>{1u64}`, `Deserialize` only applies the size/version guards
    then returns `Result.ok()`). `ToString` generation remains deferred; requesting `generate(ToString)` emits a
    deterministic diagnostic directing users to `DebugPrint`.
  - **v2.1 progress (`SoaSchema`):** `SoaSchema` emits `/Type/SoaSchemaFieldCount() -> i32`,
    `/Type/SoaSchemaFieldName([i32] index) -> string`, `/Type/SoaSchemaFieldType([i32] index) -> string`, and
    `/Type/SoaSchemaFieldVisibility([i32] index) -> string` over the non-static fields of `Type` in deterministic
    source order. `SoaSchema` also emits `/Type/SoaSchemaChunkCount() -> i32`,
    `/Type/SoaSchemaChunkFieldStart([i32] index) -> i32`, and `/Type/SoaSchemaChunkFieldCount([i32] index) -> i32`
    so wide reflected schemas can be grouped into deterministic sixteen-column storage chunks. `SoaSchema` now also
    emits `/Type/SoaSchemaStorage` and `/Type/SoaSchemaStorageNew()` on top of that chunk layer, where each chunk
    field is backed by `SoaColumn<T>` or `SoaColumnsN<...>` (`N <= 16`) over the reflected source-order fields.
    `SoaSchema` now also emits `/Type/SoaSchemaStorageCount([/Type/SoaSchemaStorage] value) -> i32`,
    `/Type/SoaSchemaStorageCapacity([/Type/SoaSchemaStorage] value) -> i32`,
    `/Type/SoaSchemaStorageReserve([/Type/SoaSchemaStorage mut] value, [i32] capacity) -> void`, and
    `/Type/SoaSchemaStorageClear([/Type/SoaSchemaStorage mut] value) -> void`, plus
    `/Type/SoaSchemaStorage/Destroy()`, routing chunk-wise count/capacity/reserve/clear and lifecycle cleanup
    through the existing fixed-width `SoaColumn<T>` / `SoaColumnsN<...>` substrate.
    These helpers bridge the current constant-index-only `meta.field_*<T>(i)` rule for reflected SoA work: callers can loop from `0` to
    `/Type/SoaSchemaFieldCount()` and query names/types/visibility through runtime
    `i32` indices without direct `meta.field_name<T>(...)` / `meta.field_type<T>(...)` /
    `meta.field_visibility<T>(...)` calls. Out-of-range string helper indices currently return the empty string
    sentinel, while out-of-range chunk helper indices return `0i32`. Wide reflected schemas can now synthesize
    deterministic chunked allocation, grow/realloc, explicit clear, and implicit drop/free cleanup on top of the
    fixed-width sixteen-column storage substrate.
- **Baseline reflection API scope (v1):** reflection APIs are compile-time-only metadata queries. Runtime reflection
  objects/tables are out of scope and rejected (`/meta/object`, `/meta/table`).
- **Reserved compile-time metadata query names:** `meta.type_name<T>`, `meta.type_kind<T>`, `meta.is_struct<T>`,
  `meta.field_count<T>`, `meta.field_name<T>(i)`, `meta.field_type<T>(i)`, `meta.field_visibility<T>(i)`,
  `meta.has_transform<T>(name)`, and `meta.has_trait<T>(traitName)` / `meta.has_trait<T, Elem>(Indexable)`. Query
  execution semantics now evaluate at compile time for the listed primitives; add a concrete reflection TODO before
  changing that metadata-query contract.
- **Current primitive status:** `meta.type_name<T>`, `meta.type_kind<T>`, `meta.is_struct<T>`, `meta.field_count<T>`,
  `meta.field_name<T>(i)`, `meta.field_type<T>(i)`, `meta.field_visibility<T>(i)`, `meta.has_transform<T>(name)`, and
  `meta.has_trait<...>(...)` evaluate at compile time in semantics. Reflection diagnostics for non-reflect
  field-metadata targets, invalid indices, and unsupported metadata query names are deterministic.
- **Field metadata target rule:** `meta.field_count<T>`, `meta.field_name<T>(i)`, `meta.field_type<T>(i)`, and
  `meta.field_visibility<T>(i)` require `T` to be a `reflect`-enabled struct.
- **Field metadata index rule:** `meta.field_name<T>(i)`, `meta.field_type<T>(i)`, and
  `meta.field_visibility<T>(i)` currently require constant integer indices, so `.prime`
  code cannot yet iterate arbitrary reflected field lists dynamically.
- **IR elimination rule:** reflection metadata queries must be eliminated before IR emission; the IR lowerer rejects
  non-eliminated reserved `/meta/*` reflection query paths.
- **`stack`, `heap`, `buffer`:** placement transforms reserved for future backends; currently rejected in validation.
- **`shared_scope`:** loop-only transform that makes a loop body share one scope across all iterations. Valid on
  `loop`/`while`/`for` only. Bindings declared in the loop body are initialized once before the loop body runs and
  persist for the duration of the loop without escaping the surrounding scope.
The lists above reflect the built-in transforms recognized by the compiler today; future additions will extend them
here.

### Core library surface (draft)
- **Standard math (draft):** the core math set lives under `/std/math/*` (e.g., `/std/math/sin`, `/std/math/pi`).
  `import /std/math/*` brings these names into the root namespace so `sin(...)`/`pi` resolve without qualification.
  Unsupported envelope/operation pairs produce diagnostics. Only fixed-width numeric envelopes are supported today;
  software numeric envelopes are parsed/typed but are rejected by current backends.
  - **Constants:** `/std/math/pi`, `/std/math/tau`, `/std/math/e`.
  - **Basic:** `/std/math/abs`, `/std/math/sign`, `/std/math/min`, `/std/math/max`, `/std/math/clamp`, `/std/math/lerp`,
    `/std/math/saturate`.
  - **Rounding:** `/std/math/floor`, `/std/math/ceil`, `/std/math/round`, `/std/math/trunc`, `/std/math/fract`.
  - **Power/log:** `/std/math/sqrt`, `/std/math/cbrt`, `/std/math/pow`, `/std/math/exp`, `/std/math/exp2`,
    `/std/math/log`, `/std/math/log2`, `/std/math/log10`.
  - **Operand rules (current):** `abs`, `sign`, `saturate`, `min`, `max`, `clamp`, `lerp`, and `pow` accept numeric
    operands (`i32`, `i64`, `u64`, `f32`, `f64`). `min`, `max`, `clamp`, `lerp`, and `pow` reject mixed signed/unsigned
    or mixed integer/float operands. All remaining `/std/math/*` builtins require float operands (`atan2`, `hypot`,
    `copysign` are binary; `fma` is ternary).
  - **Integer pow:** for integer operands, `pow` requires a non-negative exponent; negative exponents abort in VM/native
    (stderr + exit code `3`), and the C++ emitter mirrors this behavior.
  - **Trig:** `/std/math/sin`, `/std/math/cos`, `/std/math/tan`, `/std/math/asin`, `/std/math/acos`, `/std/math/atan`,
    `/std/math/atan2`, `/std/math/radians`, `/std/math/degrees`.
  - **Hyperbolic:** `/std/math/sinh`, `/std/math/cosh`, `/std/math/tanh`, `/std/math/asinh`, `/std/math/acosh`,
    `/std/math/atanh`.
  - **Float utils:** `/std/math/fma`, `/std/math/hypot`, `/std/math/copysign`.
  - **Predicates:** `/std/math/is_nan`, `/std/math/is_inf`, `/std/math/is_finite`.
  - **Backend limits (current):** VM/native math uses fast approximations and is validated against the C++/exe baseline
    within tolerances. Large-magnitude trig/log/exp inputs are only required to stay finite and within basic range
    bounds (e.g., `|sin(x)| <= 1`), and may diverge from the C++/exe baseline. Conformance tests use tolerance or range
    checks for these cases. Float matrix/quaternion reference suites currently use an absolute `1e-4` tolerance policy.
  - **Vector, color, matrix, quaternion types (draft):** stdlib ships `.prime` definitions for `Vec2`, `Vec3`, `Vec4`,
    `ColorRGB`, `ColorRGBA`, `ColorSRGB`, `ColorSRGBA`, `Mat2`, `Mat3`, `Mat4`, and `Quat`. These are distinct nominal
    types; colors are not aliases of vectors, matrices are not aliases of arrays/vectors, and quaternions are not
    aliases of `Vec4`.
    - **Vectors:** constructors, component accessors, and member methods like `length()`, `normalize()` (in-place), and
      `toNormalized()` (returns a new value).
    - **Colors:** constructors plus color-space helpers (e.g., sRGB/linear conversions) and per-channel ops. sRGB types
      remain distinct from linear `ColorRGB`/`ColorRGBA`.
    - **Matrices:** constructors plus direct public scalar component access via `mRC` field names (`m00`, `m01`, ...),
      where the first index is the row and the second index is the column.
    - **Quaternions:** constructors plus direct public scalar component access via `x`, `y`, `z`, and `w`, along with
      `toNormalized()` / `normalize()` helpers. The stdlib now also ships `quat_to_mat3(q)`, `quat_to_mat4(q)`, and
      `mat3_to_quat(m)`.
  - **Matrix/quaternion interaction contract (draft):**
    - No implicit conversion between scalar/vector/matrix/quaternion families; use explicit constructor/helper calls.
    - `plus`/`minus` require identical operand envelopes (`VecN` with same `N`, `MatRxC` with same shape, or `Quat` with
      `Quat`).
    - Current implementation status: semantics already enforces the `Mat*`/`Quat` `plus` and `minus` rules, the
      documented `Mat*`/`Quat` `multiply` allowlist, `Mat* / scalar` plus `Quat / scalar` divide validation, and
      deterministic binding/return/call diagnostics for implicit `Mat*`/`Quat` family conversions. VM/native, Wasm, and
      the C++ emitter now also lower component-wise `Mat2`/`Mat3`/`Mat4` and `Quat` `plus` + `minus`, scalar-left/right
      matrix/quaternion scaling, matrix/quaternion-by-scalar divide, matrix-vector multiply, matching matrix-matrix
      multiply, quaternion-quaternion Hamilton products, and quaternion-`Vec3` rotation through the documented contract.
      GLSL now lowers nominal `Vec2`/`Vec3`/`Vec4`, `Quat`, `Mat2`/`Mat3`/`Mat4` values, direct vector/quaternion/matrix
      field access, component-wise vector/quaternion `plus`/`minus`, vector/quaternion scalar scale/divide, `MatN *
      VecN` interop, matching matrix-matrix multiply, quaternion-quaternion Hamilton products, quaternion-`Vec3`
      rotation, and the explicit conversion helpers `quat_to_mat3`, `quat_to_mat4`, and `mat3_to_quat`.
    - `multiply` is allowed for: scalar scaling (`S * VecN`, `VecN * S`, `S * Mat`, `Mat * S`, `S * Quat`, `Quat * S`),
      matrix-vector (`Mat * VecN` with compatible inner dimension), matrix-matrix (`MatRxC * MatCxK`),
      quaternion-quaternion (Hamilton product), and quaternion-vector rotation (`Quat * Vec3`).
    - `divide` is allowed only as composite-by-scalar (`VecN / S`, `Mat / S`, `Quat / S`); scalar/composite and
      composite/composite division are diagnostics unless explicitly documented.
    - Matrix/vector multiplication uses a column-vector convention (`result = Mat * Vec`), and transform composition
      order follows canonical call nesting (`MatA * (MatB * Vec)`).
    - Conformance reference tests for float matrix/quaternion outputs use an absolute `1e-4` tolerance policy.
    - Equality is component-wise exact (`equal`, `not_equal`); tolerance-based comparison is explicit helper API, not
      operator sugar.
    - Scalar `/std/math/*` builtins remain scalar-only unless a specific builtin documents vector/matrix/quaternion
      overloads.
- **`assign(target, value)`:** canonical mutation primitive; only valid when `target` carried `mut` at declaration time.
  The call evaluates to `value`, so it can be nested or returned.
- **`increment(target)` / `decrement(target)`:** canonical mutation helpers used by `++`/`--` desugaring. Only valid on
  mutable numeric bindings; they evaluate to the updated value.
- **`count(value)` / `value.count()` / `contains(value, key)` / `at(value, index)` / `value.at(index)` / `value[index]`
  / `at_unsafe(value, index)` / `value.at_unsafe(index)`:** collection helpers. Method-call and index forms are
  preferred surface syntax where supported; helper-call forms are canonical equivalents. `contains` is currently
  map-only and returns `bool`. `at` performs bounds checking; `at_unsafe` does not. In VM/native backends, an
  out-of-bounds `at` aborts execution (prints a diagnostic to stderr and returns exit code `3`).
- **Runtime error reporting (VM/native):** runtime guards (bounds checks, missing map keys, invalid integer `pow`
  exponents, and the current vector dynamic-capacity guard) abort execution, print a diagnostic to stderr, and
  return exit code `3`. Parse/semantic/lowering errors remain exit code `2`.
- **`print(value)` / `print_line(value)` / `print_error(value)` / `print_line_error(value)`:** stdout/stderr output
  primitives (statement-only). `print`/`print_line` require `io_out`, and `print_error`/`print_line_error` require
  `io_err`. VM/native backends support integer/bool values plus string literals/bindings; other string operations still
  require the C++ emitter.
- **`plus`, `minus`, `multiply`, `divide`, `negate`:** arithmetic wrappers used after operator desugaring. Operands must
  be numeric (`i32`, `i64`, `u64`, `f32`, `f64`); bool/string/pointer operands are rejected. Mixed signed/unsigned
  integer operands are rejected in VM/native lowering (`u64` only combines with `u64`), and `negate` rejects unsigned
  operands. Pointer arithmetic is only defined for `plus`/`minus` with a pointer on the left and an integer offset (see
  Pointer arithmetic below).
- **`greater_than(left, right)`, `less_than(left, right)`, `greater_equal(left, right)`, `less_equal(left, right)`,
  `equal(left, right)`, `not_equal(left, right)`, `and(left, right)`, `or(left, right)`, `not(value)`:** comparison
  wrappers used after operator/control-flow desugaring. Comparisons respect operand signedness (`u64` uses unsigned
  ordering; `i32`/`i64` use signed ordering), and mixed signed/unsigned comparisons are rejected in the current
  IR/native subset; `bool` participates as a signed `0/1`, so `bool` with `u64` is rejected as mixed signedness. Boolean
  combinators accept `bool` inputs only. Control-flow conditions (`if`/`while`/`for`) require `bool` results; use
  comparisons or `bool{value}` when needed. The current IR/native subset accepts integer/bool/float operands for
  comparisons; string comparisons still require the C++ emitter.
- **`/std/math/clamp(value, min, max)`:** numeric helper used heavily in rendering scripts. VM/native lowering supports
  integer clamps (`i32`, `i64`, `u64`) and float clamps (`f32`, `f64`) and follows the usual integer promotion rules
  (`i32` mixed with `i64` yields `i64`, while `u64` requires all operands to be `u64`). Mixed signed/unsigned clamps are
  rejected.
- **`if(condition, then() { ... }, else() { ... })`:** canonical conditional form after control-flow desugaring.
  - Signature: `if(Envelope, Envelope, Envelope)`
  - 1) must evaluate to a boolean (`bool`), either a boolean value or a function returning boolean
  - 2) must be a definition envelope; its body yields the `if` result when the condition is `true`
  - 3) must be a definition envelope; its body yields the `if` result when the condition is `false`
  - Surface `if(condition) { ... }` is statement-only sugar and lowers to `if(condition, then() { ... }, else() { })`.
  - Evaluation is lazy: the condition is evaluated first, then exactly one of the two definition bodies is evaluated.
- **`loop(count) { ... }`:** statement-only loop helper. `count` must be an integer envelope (`i32`, `i64`, `u64`), and
  the body is required. Negative counts are errors.
- **`while(condition) { ... }`:** statement-only loop helper. `condition` must evaluate to `bool` (or a function
  returning `bool`).
- **`for(init cond step) { ... }`:** statement-only loop helper. `init`, `cond`, and `step` are evaluated in order;
  `cond` must evaluate to `bool`. Bindings are allowed in any slot. Semicolons and commas are optional separators (e.g.,
  `for(init; cond; step)`).
- **Loop scope:** loop bodies default to per-iteration scope. Add `[shared_scope]` before the loop to share one scope
  across all iterations.
  - Example:
    ```
    [shared_scope]
    loop(3i32) {
      [i32 mut] total{0i32}
      assign(total, plus(total, 1i32))
    }
    ```

### Loop/For/While Examples (surface → canonical)
Surface forms:
```
loop(5i32) { work() }

while(i < 10i32) { work(i) }

for([i32 mut] i{0i32}; i < 10; ++i) {
  work(i)
}
```

Canonical (after desugaring):
```
loop(5i32, do() { work() })

while(less_than(i, 10i32), do() { work(i) })

for(
  [i32 mut] i{0i32},
  less_than(i, 10i32),
  increment(i),
  do() { work(i) }
)
```
- **`return(value)`:** explicit return primitive; may appear as a statement inside control-flow blocks. For `void`
  definitions, `return()` is allowed. Implicit `return(void)` fires at end-of-body when omitted. Non-void definitions
  must return on all control paths; fallthrough is a compile-time error. Inside value blocks (binding initializers /
  brace constructors), `return(value)` returns from the block and yields its value.
- **IR note:** VM/native IR lowering supports numeric/bool `array<T>{...}` and `vector<T>{...}` construction plus their
  current call-shaped compatibility helper paths, along with `count`/`at`/`at_unsafe` on those sequences. Map literals
  are supported in VM/native for numeric/bool values, and string-keyed maps work when the keys are string literals or
  bindings backed by literals (string table entries). VM/native vectors now lower through an explicit heap-backed
  record header (`count`, `capacity`, `data_ptr`): `push`/`reserve` reallocate backing storage while capacity remains
  within the current `256` dynamic-capacity ceiling and error once exceeded, and shrinking helpers (`pop`, `clear`,
  `remove_at`, `remove_swap`) work against that heap-backed record. Vector element access/mutation uses `data_ptr`
  indirection instead of fixed in-header element offsets. Vector constructors above the current VM/native local capacity
  limit (`256`) are rejected during lowering with `vector literal exceeds local capacity limit (256)`,
  and `reserve` with out-of-range or negative integer literal expressions (including folded signed/unsigned
  `plus`/`minus`/`negate`, such as `plus(200i32, 57i32)`, `minus(1u64, 2u64)`, `plus(18446744073709551615u64, 1u64)`, or
  `negate(1i32)`) is also rejected at lowering time (`vector reserve exceeds local capacity limit (256)` / `vector
  reserve expects non-negative capacity` / `vector reserve literal expression overflow`). Folded signed and unsigned
  literal-expression overflow now emits the deterministic lowering diagnostic `vector reserve literal expression
  overflow` instead of deferring to backend/runtime integer-overflow behavior.

### Standard Library Reference (v0)
- **Status:** stable snapshot of the current builtin surface (v0).
- **Versioning (planned):**
  - Each package declares a semantic version (e.g., `1.2.0`).
  - `import<..., version="1.2.0">` selects a specific package revision.
  - There is no compiler flag to pin the default package set yet; use explicit versions in `import` declarations.
- **Namespaces:**
  - `/std/math/*` is imported via `import /std/math/*` (or explicit names like `import /std/math/sin /std/math/pi`).
  - Core builtins (`assign`, `count`, `print*`, `convert`, etc.) live in the root namespace.
- **Conformance markers:**
  - **`C++`**: supported only by the C++ emitter (VM/native reject).
  - **`VM/native`**: supported by the VM + native backends.
  - **`VM/native (limited)`**: supported with the listed restrictions.
- **Conformance notes:**
  - String comparisons are **`C++`** only; VM/native reject them.
  - `map<K, V>` keys currently use the builtin ordered-map `Comparable` subset: `i32`, `i64`, `u64`, `f32`, `f64`,
    `bool`, or `string`. Template parameters defer this check until monomorphisation; user-defined `Comparable` keys
    remain blocked on the future stdlib-owned map runtime.
  - Builtin `map<K, V>` values must be numeric/bool for **`VM/native`**; string values remain **`C++`** only on the
    builtin map path, while the experimental stdlib map helper path now has VM/native `mapTryAt` coverage for string
    values.
  - String indexing in **`VM/native (limited)`** requires string literals or bindings backed by literals.
  - `vector<T>` is specified as a C++-style dynamic contiguous sequence
    (`push`/`reserve` may grow capacity). VM/native now use a heap-backed
    `count/capacity/data_ptr` record for vector locals, so push/reserve growth
    reallocates backing storage while preserving existing elements. `TODO-4281`
    tracks lifting the remaining local dynamic-capacity limit beyond `256`;
    add a separate concrete TODO before changing runtime semantics outside
    that scope.
  - Stdlib collection helpers now share `ContainerError` for deterministic error payloads: `containerMissingKey()`
    (`1`), `containerIndexOutOfBounds()` (`2`), `containerEmpty()` (`3`), and `containerCapacityExceeded()` (`4`).
    `containerErrorStatus(err)` packs a status-only `Result<ContainerError>`, `containerErrorResult<T>(err)` packs a
    `Result<T, ContainerError>` error value for the current IR-backed backends, the public
    `/ContainerError/why([ContainerError] err)` wrapper keeps explicit type-owned error strings on the stdlib surface,
    the canonical type-owned `ContainerError.missingKey()`, `ContainerError.indexOutOfBounds()`,
    `ContainerError.empty()`, and `ContainerError.capacityExceeded()` helpers keep the current constructor values on
    that same surface, the public camelCase root wrappers `/ContainerError/missingKey()`,
    `/ContainerError/indexOutOfBounds()`, `/ContainerError/empty()`, and `/ContainerError/capacityExceeded()` expose
    those same values for slash-call code, compatibility wrappers keep the older snake_case root spellings, and
    `Result.why(Result<ContainerError>)` maps container error codes to the same stable literal-backed messages.
    Builtin empty-vector `pop` runtime aborts and checked vector indexing/removal aborts now use the same `"container
    empty"` / `"container index out of bounds"` wording across VM/native/C++ flows.
  - Canonical `/std/collections/vector/*` is now the sole public namespaced vector contract. The
    `/std/collections/experimental_vector/*` family now remains only as the internal implementation seam behind that
    public contract. That backing namespace (`vector<T>(...)`, `vectorNew`, `vectorSingle`,
    `vectorPair`, `vectorTriple`, `vectorQuad`, `vectorQuint`, `vectorSext`, `vectorSept`, `vectorOct`, `vectorCount`,
    `vectorCapacity`, `vectorReserve`, `vectorPush`, `vectorPop`, `vectorClear`, `vectorRemoveAt`,
    `vectorRemoveSwap`, `vectorAt`, `vectorAtUnsafe`) returns the current `.prime` `Vector<T>` record backed by
    heap-pointer storage, but user-facing docs/examples should prefer the canonical wrappers in
    `stdlib/std/collections/vector.prime` instead of treating that experimental namespace as a peer public API. The
    current slice includes a real variadic `.prime` constructor built on `[args<T>]` parameters, with the older
    fixed-arity helper names retained as backing compatibility forwarders, plus reserve/push and pop/clear/remove
    helpers on that pointer-backed storage. `Vector<T>` now carries `.prime` `Move` and `Destroy` hooks plus
    `Pointer<uninitialized<T>>` slot storage, so constructor materialization, growth, checked/unchecked access,
    removed-element destruction, survivor compaction/swap, and scope-exit cleanup all run through stdlib-owned
    `init(...)`, `borrow(...)`, `take(...)`, and `drop(...)` flows instead of reusing builtin vector ownership gates.
    Imported canonical `/std/collections/vector/*` plus `/std/collections/vector*` helper spellings still rewrite onto
    that same backing implementation whenever their receiver is an experimental `Vector<T>` value, and the wrapper-layer
    `vectorCount|vectorCapacity|vectorAt*|vectorPush|vectorPop|vectorReserve|vectorClear|vectorRemove*` helpers keep
    forwarding through the canonical vector wrapper path with inferred receivers instead of hard-coded builtin
    `vector<T>` parameters. Imported wrapper-layer `vectorNew|vectorSingle|vectorPair|vectorTriple|...` legacy
    constructor-shaped aliases still rewrite onto `/std/collections/experimental_vector/*` whenever an explicit
    experimental `Vector<T>` destination already pins the target type, infer that same experimental `Vector<T>` type for
    imported `[auto]` locals and `return<auto>` definitions, and now also rewrite nested direct helper-call plus method-call receiver
    expressions built from those aliases onto the experimental constructor path before helper resolution. Imported
    positional canonical `/std/collections/vector/vector<T>(...)` calls now prefer a real variadic canonical wrapper
    over that same backing constructor for `auto` bindings and temporary receiver flows, and imported named-argument
    legacy constructor-shaped canonical calls now resolve through same-path fixed-arity
    `/std/collections/vector/vector` overloads instead of helper-path rewrites for those same inferred and temporary
    receiver cases. Canonical
    `/std/collections/vector/*` `pop` / `clear` helpers now reuse that same backing discard path for
    ownership-sensitive element types on explicit experimental `Vector<T>` bindings, and canonical
    `/std/collections/vector/*` `remove_at` / `remove_swap` helpers now reuse the backing indexed-removal path for
    those same explicit experimental `Vector<T>` bindings.
  - Canonical `/std/collections/map/*` is now the sole public namespaced map contract. The
    `/std/collections/experimental_map/*` family now remains only as the internal implementation seam behind that
    public contract. That backing namespace (`Entry<K, V>`, `entry(key, value)`,
    `map<K, V>(entries...)`, `mapNew`, `mapSingle`, `mapDouble`, `mapPair`, `mapTriple`, `mapQuad`, `mapQuint`,
    `mapSext`, `mapSept`, `mapOct`, `mapInsert`, `mapCount`, `mapContains`, `mapTryAt`, `mapAt`, `mapAtUnsafe`,
    `mapInsertRef`, `mapCountRef`, `mapContainsRef`, `mapTryAtRef`, `mapAtRef`, `mapAtUnsafeRef`) returns the current
    `.prime` `Map<K, V>` struct backed by parallel experimental `Vector<K>` / `Vector<V>` storage, but user-facing
    docs/examples should prefer the canonical wrappers in `stdlib/std/collections/map.prime` instead of treating that
    experimental namespace as a peer public API. The current constructor surface includes a real variadic `.prime`
    `map(entries...)` helper over trailing `[args<Entry<K, V>>]` parameters, with the older fixed-arity helper names
    retained as backing compatibility forwarders. Lookup follows `Comparable<K>` instead of the canonical builtin map
    runtime, checked experimental `mapAt(...)` reuses the canonical `map key not found` runtime contract on misses,
    literal-backed `Map<string, V>` helper flows now work across C++/VM/native, VM/native non-literal string-key
    constructors/lookups preserve the canonical string-key reject diagnostics, and the backing map values support
    `mapInsert(...)` plus `values.insert(...)` updates together with `.count()`/`.contains()`/`.tryAt()`/`.at()`/
    `.at_unsafe()` method sugar on ownership-sensitive element flows. Explicit experimental `Map<K, V>` bindings now
    also support canonical `/std/collections/map/insert(...)` on that same overwrite/update path, builtin canonical
    `map<K, V>` bindings now route both `.insert(...)` method sugar and direct canonical `/std/collections/map/insert(...)`
    calls through a helper that performs the real in-place overwrite path when the numeric key already exists and
    otherwise grows through one generic arbitrary-`n` grow/copy/repoint path for owning local, borrowed/pointer, and
    non-local field/lvalue numeric map receivers without retaining the old count-by-count lowerer staircase, borrowed
    references also support canonical `/std/collections/map/insert_ref(...)`, and overwrite/update plus scope-exit
    cleanup now run through the same pointer-backed uninitialized-slot ownership flow as experimental vectors by
    explicitly `drop(...)`ing and `init(...)`ing payload slots. Borrowed `Reference<Map<K, V>>` values now support
    distinct `*Ref` free-helper calls plus `.count()`/`.contains()`/`.tryAt()`/`.at()`/`.at_unsafe()`/`.insert()`
    method-call sugar through `.prime` `/Reference/*` helpers, and both value plus borrowed-reference experimental maps
    now participate in shared `value[key]` bracket access with the same key/type diagnostics as other map helper forms.
- **Core builtins (root namespace):**
  - **`assign(target, value)`** (statement): mutates a mutable binding or dereferenced pointer.
  - **`increment(target)` / `decrement(target)`** (statement): mutation helpers used by `++`/`--` desugaring.
  - **`if(cond, then() { ... }, else() { ... })`**: canonical conditional form after desugaring.
  - **`loop(count, do() { ... })`** (statement): loop helper with integer count.
  - **`while(cond, do() { ... })`** (statement): loop helper with bool condition.
  - **`for(init, cond, step, do() { ... })`** (statement): loop helper with init/cond/step.
  - **`return(value)`**: explicit return helper.
  - **`count(value)` / `value.count()`**: collection length. Vector forms currently require `import /std/collections/*`.
  - **`contains(value, key)`**: map key probe without triggering the checked missing-key abort path used by `at(value,
    key)`.
  - **`at(value, index)` / `value.at(index)` / `value[index]` / `at_unsafe(value, index)`**: bounds-checked and
    unchecked indexing (`value[index]` and `value.at(index)` are the same safe operation). Vector forms currently
    require `import /std/collections/*`.
  - **`print*`**: `print`, `print_line`, `print_error`, `print_line_error`.
  - **Collections:** `array<T>{...}`, `vector<T>{...}`, `map<K, V>{...}`. Legacy call-shaped collection helpers remain
    compatibility helper surfaces.
  - **Pointer helpers:** `location`, `dereference`.
  - **Ownership helpers:** `move`, `clone`.
  - **Uninitialized helpers (draft):** `init`, `drop`, `take`, `borrow`.
  - **GPU builtins (draft):**
    - `Buffer<T>` is a hybrid GPU resource-handle surface: allocation, upload/readback, dispatch, and storage access
      stay in language/runtime substrate, while higher-level wrappers should converge on stdlib `.prime` definitions.
      Canonical and experimental `/std/gfx/*` now provide `.prime`-authored `Buffer.count()`, `Buffer.empty()`,
      `Buffer.is_valid()`, `Buffer.readback()`, and compute-only `Buffer.load(index)` / `Buffer.store(index, value)`
      convenience helpers, the preferred constructor-shaped `Buffer<T>(count)` allocation surface, plus explicit
      slash-call `/std/gfx/Buffer/allocate<T>(count)`, `/std/gfx/Buffer/upload(...)`, `/std/gfx/Buffer/load(...)`, and
      `/std/gfx/Buffer/store(...)` wrappers over the public handle layout and builtin host
      allocation/readback/upload/storage substrate.
    - `/std/gpu/global_id_x()` → `i32` (kernel invocation x coordinate).
    - `/std/gpu/global_id_y()` → `i32` (kernel invocation y coordinate).
    - `/std/gpu/global_id_z()` → `i32` (kernel invocation z coordinate).
    - `/std/gpu/buffer_load(Buffer<T>, index)` / `/std/gpu/buffer_store(Buffer<T>, index, value)` for storage buffers.
    - `/std/gpu/buffer<T>(count)` / `/std/gpu/upload(array<T>)` / `/std/gpu/readback(Buffer<T>)` for host-side resource
      management.
    - `/std/gpu/dispatch(kernel, gx, gy, gz, args...)` submits a compute kernel and requires `effects(gpu_dispatch)`.
      For determinism in v1, dispatch is blocking and `/std/gpu/readback` returns only after completion.
  - **Operators (desugared forms):** `plus`, `minus`, `multiply`, `divide`, `negate`, `increment`, `decrement`.
  - **Comparisons/booleans:** `greater_than`, `less_than`, `greater_equal`, `less_equal`, `equal`, `not_equal`, `and`,
    `or`, `not`.
  - **Result helpers (draft):**
  - `Result<Error>` is a status-only wrapper for fallible operations; `Result<T, Error>` carries a value on success.
  - **ADT migration note:** `/std/result/*` now exposes importable `Result<E>` and value-carrying `Result<T, E>` sums
    at the same public path. `Result<E>` has a unit `ok` variant, an `error(E)` payload variant, default construction
    to `ok`, and an `ok<E>()` helper; error construction currently uses explicit sum construction such as
    `Result<E>{[error] err}`. `Result<T, E>` has `ok` and `error` payload variants plus explicit
    `ok<T, E>(value)` / `error<T, E>(err)` construction helpers. Legacy packed status-only `Result<Error>` values
    without the stdlib import remain a compiler/runtime bridge and produce a deterministic compatibility diagnostic
    if used as a `pick` target. `try(...)`
    semantic validation and semantic-product metadata accept both `Result<T, E>` and the qualified
    `/std/result/Result<T, E>` spelling for value-carrying results. IR-backed `try(...)` now consumes local imported
    stdlib value-result sums for `return<int> on_error<...>` status-code flows by branching on the `ok`/`error` tag,
    and Result-returning functions can propagate local imported stdlib value-result sum errors by copying the
    `error` payload into the declared return `Result` sum after running the active `on_error` handler. Direct calls
    that return imported stdlib value-result sums can also be consumed through postfix `?` on those same VM/native
    sum-backed paths. Typed imported value-carrying sum
    locals/returns may use legacy `Result.ok(value)` as an `ok`-variant compatibility initializer on IR-backed
    VM/native paths, and typed imported value-carrying sum locals/returns may use legacy `Result.map(result, fn)` or
    `Result.and_then(result, fn)` when the source is a local imported stdlib Result sum or a direct call returning
    one; `Result.map2(left, right, fn)` is available when both sources are local imported stdlib Result sums or direct
    calls returning them. Dereferenced local `Reference<Result<T, E>>` and `Pointer<Result<T, E>>` values can feed
    `try(...)`, `Result.error(...)`, and `Result.why(...)` when they point at imported stdlib Result sums.
    `Result.error(value)` already reads the imported value-carrying sum tag on those paths, so it returns `false` for
    `ok` and `true` for `error` values from `/std/result/*`.
    `Result.why(value)` also reads imported value-carrying sums, yielding the empty string for `ok` and calling the
    error payload type's `why` helper for `error`. IR-backed `try(...)` also consumes local, direct-call, and
    dereferenced local `Reference<Result<E>>` / `Pointer<Result<E>>` imported status-only `Result<E>` sums for
    status-code returns and Result-return error propagation. IR-backed `Result.error(...)` / `Result.why(...)` also
    inspect imported status-only `Result<E>` sums from locals, direct calls, and dereferenced local
    `Reference<Result<E>>` / `Pointer<Result<E>>` sources instead of falling back to the legacy packed-status bridge.
    The legacy source C++ emitter still uses a compatibility Result bridge, but it now preserves nested
    `Result<T...>` types under `Reference` / `Pointer` and recognizes dereferenced local/indexed borrowed Result
    operands for `try(...)`, `Result.error(...)`, and `Result.why(...)`. Its source C++ Result storage-width decisions
    and construction/accessor expression emission are quarantined behind named emitter helpers. Value-carrying Result
    storage emits the tagged `ps_result_value` bridge type instead of raw `uint64_t` return/binding types or legacy
    `ps_legacy_result_*` helper names. That generated type has separate tag, error-payload, and `ok` success fields
    plus explicit ok/error construction helpers and value-qualified accessor helper names. The generated bridge uses
    named ok/error tag constants, and it does not retain raw
    packed-integer construction, conversion, generic `ps_result_*` value accessors, or `ps_result_pack(...)`
    compatibility. Status-only source C++ Result storage emits the tagged `ps_result_status` bridge type instead of
    raw `uint32_t` return/binding types, with the same named ok/error tag constants and low-level file helper status
    codes wrapped at the source Result boundary. Explicit source C++ `Result<T, E>{[ok] value}`,
    `Result<T, E>{[error] err}`, `Result<E>{}`, `Result<E>{ok}`, and `Result<E>{[error] err}` constructors route
    through those bridge helpers for supported scalar-compatible payloads. Source C++ `Result.why(...)` binds the
    bridge operand once, returns an empty string for `ok`, and only calls the error-domain `why` helper for `error`
    payloads. Explicit source C++ `error` constructors also pack single-field int-backed error structs through their
    code field before entering the bridge. Direct source C++ `Result.ok(value)` packs local or explicitly constructed
    single-field success structs through their code field before entering the bridge.
  - `Result<T, Error>` is in transition: explicit imported value construction is stdlib-owned, while `?` propagation
    and the minimum success/error runtime contract stay language-defined until the sum-backed propagation contract is
    implemented. The semantic `try(...)` contract already recognizes the unqualified and qualified stdlib-owned
    value-result type spellings, and the IR-backed VM/native paths already branch on local stdlib Result sums for
    status-code returns, Result-return error propagation, direct-call postfix `?` operands, and imported status-only
    `try(...)` operands across local, direct-call, and dereferenced local pointer/reference sources, plus status-only
    `Result.error(...)` / `Result.why(...)` helpers on those same operand families. The source C++ emitter now mirrors
    the borrowed/pointer helper operand inference while still using a compatibility bridge; storage-width decisions and
    construction/accessor expression emission are quarantined behind named emitter helpers. Value-carrying Result
    storage is named through the tagged `ps_result_value` bridge type, uses tag-based error checks, and no longer
    accepts or converts back to a raw packed integer, generic value accessor helper, or `ps_result_pack(...)` helper.
    Source C++ Result bridge construction and checks now share named ok/error tag constants, and value-carrying
    storage uses an `ok` field for the success payload. Status-only source C++ Result storage is named through the
    tagged `ps_result_status` bridge type, uses tag-based error checks, and wraps raw low-level file helper status
    codes at the source Result boundary. Explicit source C++ Result sum constructors for supported scalar-compatible
    ok/error payloads now lower through the same bridge helpers. Source C++ `Result.why(...)` uses those tag checks
    before extracting the error payload, so `ok` bridge values yield the empty string instead of calling the
    error-domain `why` helper. Explicit source C++ `error` constructors pack single-field int-backed error structs
    through their code field before entering the status-only or value-carrying bridge. Direct source C++
    `Result.ok(value)` does the same for local or explicitly constructed single-field success structs before entering
    the value-carrying bridge. The remaining migration work can focus on retargeting broader bridge construction to
    the stdlib Result sum contract.
  - `Result.ok()` (or `Result.ok(value)` for value-carrying results) constructs a success value.
  - `Result.error()` returns `true` when the result is an error.
  - `Result.why()` returns an owned `string` describing the error (heap-allocated by default).
  - Nested `Result.map(...)`, `Result.and_then(...)`, and `Result.map2(...)` expressions now preserve enough
    `Result` metadata for downstream `Result.error(...)`, `Result.why(...)`, `try(...)`, and `[auto]` bindings on the
    semantics side. Lambda-backed execution now covers `Result.map(...)`, `Result.and_then(...)`, and
    `Result.map2(...)` on IR-backed native/VM paths for the current `i32`/`bool`/`f32`/`string` payload subset as
    well as the C++ emitter path, including direct `Result.ok(...)` source expressions instead of only local- or
    definition-backed `Result` inputs, and the IR-side result resolver now recognizes those direct combinator
    expressions themselves so VM/native `try(...)`, `Result.error(...)`, and `Result.why(...)` can consume them
    without an intermediate binding. IR-backed auto bindings now also preserve that same `Result` metadata for direct
    `Result.ok(...)`, `Result.map(...)`, `Result.and_then(...)`, and `Result.map2(...)` initializer expressions, so
    later `try(...)`, `Result.error(...)`, and `Result.why(...)` calls can consume the bound local without forcing an
    explicit `Result<...>` annotation.
  - Direct `Result.ok(...)` expressions now participate in that same metadata flow, including `Result.and_then(...)`
    lambdas that return `Result.ok(...)` and need to inherit the input `Result` error domain instead of depending on
    an unrelated outer context. Unsupported wider payload kinds now reject through the same explicit IR-backed
    contract across `Result.ok(...)`, `Result.map(...)`, `Result.and_then(...)`, and `Result.map2(...)`, and
    VM/native now also resolve simple value-carrying `Result.and_then(...)` lambda returns that end in an explicit
    block-bodied `return(...)` or in a final `if(...)` expression whose `then(){...}` / `else(){...}` branches each
    produce a `Result`, instead of forcing those lambdas down to a single direct expression. The same IR inference now
    recognizes direct string `try(Result.map(...))`, `try(Result.and_then(...))`, and `try(Result.map2(...))`
    expressions for downstream string consumers such as `count(...)` and `print_line(...)` without an intermediate
    binding, and the lowerer’s call-base inference helper now resolves those same direct combinators for `try(...)`
    while reusing the configured inference state for ordinary mapped/chained arithmetic bodies instead of depending on
    the later dispatch-only path. That same call-base path also now resolves definition-backed and receiver-method
    `Result` sources inside those combinators, so named helpers such as `greeting()` and method-sugar helpers such as
    `reader.read()` can feed direct `Result.map(...)`, `Result.and_then(...)`, and `Result.map2(...)` consumers on
    IR-backed VM/native paths without an intermediate local.
  - `/std/file/*` now also exposes a stdlib-owned `FileError` namespace surface: `FileError.why(err)`,
    `FileError.status(err)`, `FileError.result<T>(err)`, `FileError.eof()`, and `FileError.isEof(err)` resolve
    through `/std/file/FileError/*` even in direct nested `Result.error(...)` / `Result.why(...)` expressions, while
    receiver-style `err.status()` / `err.result<T>()` method sugar now routes through the same stdlib-owned FileError
    helpers as `err.why()` / `err.isEof()`. The old root `/FileError/*` wrappers and package-level
    `fileErrorStatus(err)` / `fileErrorResult<T>(err)` compatibility helpers are removed, while `fileReadEof()` and
    `fileErrorIsEof(err)` remain as the narrow convenience helpers over that same type-owned implementation.
  - Stdlib containers use `Result<ContainerError>` / `Result<T, ContainerError>` as the shared error contract;
    `ContainerError.status(err)` / `ContainerError.result<T>(err)` now own the type-level packing surface while
    `containerErrorStatus(err)` / `containerErrorResult<T>(err)` remain compatibility helpers,
    `/ContainerError/status([ContainerError] err)` and `/ContainerError/result<T>([ContainerError] err)` are the
    public root wrappers for those packers, `/ContainerError/why([ContainerError] err)` is the explicit public
    wrapper for container error strings, `ContainerError.missingKey()`, `ContainerError.indexOutOfBounds()`,
    `ContainerError.empty()`, and `ContainerError.capacityExceeded()` are the canonical constructor helpers,
    `/ContainerError/missingKey()`, `/ContainerError/indexOutOfBounds()`, `/ContainerError/empty()`, and
    `/ContainerError/capacityExceeded()` expose the same values on the public root surface, compatibility wrappers keep
    `/ContainerError/missing_key()`, `/ContainerError/index_out_of_bounds()`, and
    `/ContainerError/capacity_exceeded()` available for migration, and unknown codes fall back to
    `"container error"`. On current IR-backed backends, `Result.ok(value)`
    plus `Result.map(...)`, `Result.and_then(...)`, and `Result.map2(...)` support `i32`, `bool`, `f32`, `string`,
    ordinary user structs whose payload storage stays on the existing stack-backed struct path, the single-slot
    int-backed stdlib error structs (`FileError`, `ImageError`, `ContainerError`, `GfxError`), packed
    `File<Mode>` handles, and `Buffer<T>` handles on VM plus native codegen when downstream `try(...)` consumers
    stay explicitly typed. Direct `Result.ok(...)` plus downstream `try(...)` also preserve `array<T>` /
    `vector<T>` handles whose element kinds fit the current collection contract, and `map<K, V>` handles whose
    key/value kinds fit that same map contract. Native executable `Result<Buffer<T>, GfxError>` values preserve
    `try(...)`, `Result.error(...)`, and success/error `Result.why(...)` on that same explicitly typed path.
    Downstream `try(...)` preserves those direct handle/error-struct payloads, rebuilds single-slot struct payloads,
    and keeps multi-slot struct payloads on that same pointer-backed struct path on VM/native. Other wider payloads
    remain unsupported; add a concrete Result payload TODO before widening that IR-backed payload contract.
    Value-carrying
    container helpers such as `mapTryAt` can now return string values when the underlying container path supports
    them.
  - Canonical and experimental stdlib gfx use `Result<GfxError>` / `Result<T, GfxError>` as their shared error
    contract; `GfxError.why(err)`, `GfxError.status(err)`, the public `windowCreateFailed()` /
    `deviceCreateFailed()` / `swapchainCreateFailed()` / `meshCreateFailed()` / `pipelineCreateFailed()` /
    `materialCreateFailed()` / `frameAcquireFailed()` / `queueSubmitFailed()` / `framePresentFailed()`
    wrappers, and receiver-style `err.why()` / `err.status()` / `err.result<T>()`
    explicitly prefer the matching canonical or experimental `GfxError` helper surface. The old canonical
    root `/GfxError/*` compatibility wrappers plus the package-level `gfxErrorStatus(err)` / `gfxErrorResult<T>(err)`
    aliases are removed so mixed canonical+experimental imports no longer need a second public gfx error path or a
    second package helper layer.
  - The postfix `?` operator unwraps a `Result` or propagates the error (see Error Handling).
    - `Result.map(result, fn)` applies `fn` to the success value (if any) and returns a new `Result`.
    - `Result.and_then(result, fn)` (a.k.a. bind) applies `fn` to the success value and flattens the result.
    - `Result.map2(a, b, fn)` applies `fn` if both results are ok; otherwise returns the first error.

Signature sketches (not surface syntax):
```
Result.map      : Result<T, Error> x (T -> U) -> Result<U, Error>
Result.and_then : Result<T, Error> x (T -> Result<U, Error>) -> Result<U, Error>
Result.map2     : Result<A, Error> x Result<B, Error> x (A, B -> C) -> Result<C, Error>
```

Example (surface):
```
[return<Result<i32, ParseError>>]
parse_and_double([string] text) {
  return(Result.map(parse_i32(text), []([i32] v) { return(multiply(v, 2i32)) }))
}

[return<Result<i32, FileError>>]
sum_two_files([string] a, [string] b) {
  return(Result.map2(read_i32(a), read_i32(b), []([i32] x, [i32] y) {
    return(plus(x, y))
  }))
}
```

### Error Handling (draft)
- **`Result` propagation:** the postfix `?` operator unwraps `Result<T, Error>` in-place; on error, it invokes a local
  `on_error` handler and then returns the error from the current function.
  - **Monadic view:** `value?` is equivalent to binding the success value and early-returning the error; it matches
    `Result.and_then` semantics and is the recommended shorthand for fallible sequencing.
  - **Graphics entry flow:** `?` is also valid inside `return<int>` definitions with a matching
    `on_error<ErrorType, Handler>(...)`; on error, the handler runs and the definition returns the raw error code.
  - **Current stdlib progress:** import `/std/file/*` to use `.prime`-authored `fileReadEof()`,
    `fileErrorIsEof(err)`, the type-owned `FileError.status(err)` / `FileError.result<T>(err)` namespace surface
    instead of hand-packing `FileError` results or hard-coding the EOF status code, plus receiver-style
    `err.status()` / `err.result<T>()` and the type-owned `FileError.why(err)` / `FileError.eof()` /
    `FileError.isEof(err)` helpers for explicit access to the current stdlib FileError helper surface. The same
    import also exposes
    `.prime`-authored `/File/openRead(...)`, `/File/openWrite(...)`, and `/File/openAppend(...)` wrappers so the
    legacy constructor-shaped `File<Mode>(path)` compatibility surface can resolve through stdlib-owned helpers while
    the host file substrate remains builtin and effect-gated underneath.
    Import `/std/collections/*` to use `.prime`-authored `containerErrorStatus(err)` /
    `containerErrorResult<T>(err)` compatibility helpers, the type-owned `ContainerError.status(err)` /
    `ContainerError.result<T>(err)` namespace surface, the canonical constructor helpers
    `ContainerError.missingKey()` / `ContainerError.indexOutOfBounds()` / `ContainerError.empty()` /
    `ContainerError.capacityExceeded()`, or the public `/ContainerError/status(err)` /
    `/ContainerError/result<T>(err)` wrappers plus `/ContainerError/why([ContainerError] err)`,
    `/ContainerError/missingKey()`, `/ContainerError/indexOutOfBounds()`, `/ContainerError/empty()`, and
    `/ContainerError/capacityExceeded()` wrappers. Compatibility wrappers keep the older snake_case root spellings for
    migration, and receiver-style `err.why()` / `err.status()` / `err.result<T>()` stay available instead of
    hand-packing container error codes or reaching through namespace-private helper paths.
    Import `/std/image/*` to use `.prime`-authored `imageReadUnsupported()`, `imageWriteUnsupported()`,
    `imageInvalidOperation()`, `imageErrorStatus(err)`, and `imageErrorResult<T>(err)` compatibility helpers, the
    type-owned `ImageError.status(err)` / `ImageError.result<T>(err)` namespace surface, or the public
    `/ImageError/status(err)` / `/ImageError/result<T>(err)` wrappers instead of hand-packing `ImageError` result
    codes, and load the public `/ImageError/why([ImageError] err)`, `/ImageError/read_unsupported()`,
    `/ImageError/write_unsupported()`, and `/ImageError/invalid_operation()` wrappers plus receiver-style
    `err.why()` / `err.status()` / `err.result<T>()` for explicit type-owned access to the current stdlib error
    strings and constructors. Import `/std/gfx/*` to use the type-owned `GfxError.why(err)` /
    `GfxError.status(err)` namespace surface, the public `windowCreateFailed()` / `deviceCreateFailed()` /
    `swapchainCreateFailed()` / `meshCreateFailed()` / `pipelineCreateFailed()` / `materialCreateFailed()` /
    `frameAcquireFailed()` / `queueSubmitFailed()` / `framePresentFailed()` wrappers, plus receiver-style
    `err.why()` / `err.status()` / `err.result<T>()`. Import `/std/gfx/experimental/*` only when preserving legacy
    compatibility imports; that namespace now acts as a compatibility shim over the canonical `/std/gfx/*` helper
    layer rather than as a peer public graphics contract.
- **Local handlers:** error handling is explicit and local to the scope that declares it.
  - `on_error<ErrorType, Handler>(args...)` is a semantic transform that attaches an error handler to a definition or
    block body.
  - Handlers do **not** flow into nested blocks; any nested block using `?` must declare its own `on_error`.
  - A missing `on_error` for a `?` usage is a compile-time error.
  - The handler signature is `Handler(ErrorType err, args...)`.
  - Bound arguments are evaluated when the error is raised, not at declaration time.

### File I/O (draft)
- **RAII object:** `File<Mode>` is the owning file handle with automatic close on scope exit (`Destroy`).
  - `File<Mode>` is a hybrid surface: host file open/read/write/close behavior remains effect-gated runtime substrate,
    while the user-facing helper layer should live in stdlib `.prime`.
  - `File<Mode>` is move-only; `Clone` is a compile-time error.
  - `close()` disarms the handle so `Destroy` becomes a no-op.
- **Modes:** `Read`, `Write`, `Append`.
- **Construction:** `/File/openRead(path)`, `/File/openWrite(path)`, and `/File/openAppend(path)` return
  `Result<File<Mode>, FileError>`. The imported constructor-shaped `File<Mode>(path)` form is legacy compatibility
  helper syntax that routes through the same mode-specific open helpers; it is not value construction syntax.
  - `path` is a `string` (string literal or literal-backed binding in VM/native).
- **Methods (all return `Result<FileError>`):**
  - `readByte([i32 mut] value)` (reads one byte into `value`; leaves `value` unchanged on error)
  - `write(values...)` (variadic text write)
  - `writeLine(values...)` (variadic + newline)
  - `writeByte(u8_value)`
  - `writeBytes(array<u8>)`
  - `flush()`
  - `close()`
- **Error type:** `FileError` carries `why()` (owned `string`).
  - `readByte(...)` reports deterministic end-of-file as `EOF`.
  - Import `/std/file/*` for the current stdlib-authored file helper layer:
    `fileReadEof()`, `fileErrorIsEof(err)`,
    `FileError.why(err)`, `FileError.eof()`, `FileError.isEof(err)`,
    `/File/openRead(...)`, `/File/openWrite(...)`, `/File/openAppend(...)`,
    `/File/readByte(...)`, zero-to-nine-value heterogenous `/File/write(...)` and
    `/File/writeLine(...)` overload families,
    `/File/writeByte(...)`, `/File/writeBytes(...)`, `/File/flush(...)`, and
    `/File/close<Mode>(...)`.
    Compatibility wrappers keep the older snake_case spellings (`open_read`, `read_byte`, `write_line`,
    `write_byte`, `write_bytes`, `is_eof`) available for migration-only callers.
    Imported legacy constructor-shaped `File<Mode>(path)` calls now route through those `.prime`
    mode-specific open wrappers, and imported method/free-call sugar now prefers the same stdlib
    layer for `readByte`, `write`, `writeLine`, `writeByte`, `writeBytes`, `flush`, and
    `close` across that current zero-to-nine-value overload family. Once `write(...)` or
    `writeLine(...)` exceeds the fixed wrapper ladder, imported broader slash-calls and method
    sugar now fall back to the builtin variadic `/file/*` surface instead of extending the stdlib
    overload family further.
  - The stdlib file layer defines `FileError.why(err)` as the public type-owned wrapper over the intrinsic
    file-error string mapping, so direct `err.why()` and `Result.why(...)` can route through stdlib-owned helper
    surface while platform-specific code-to-string translation stays builtin substrate. It also defines
    `FileError.eof()` so EOF values can be constructed from the same type-owned stdlib surface, plus
    `FileError.isEof(err)` so EOF classification can use that same surface via direct calls or `err.isEof()`.
- **Effect requirement:** read-only file operations require `effects(file_read)` and write/append operations require
  `effects(file_write)`. `file_write` also implies `file_read` for compatibility.
- **Example:**
  ```
  [return<Result<FileError>> effects(file_write, io_err)
   on_error<FileError, /log_file_error>(path)]
  write_ppm([string] path, [i32] width, [i32] height) {
    [File<Write>] file{ File<Write>(path)? }
    file.writeLine("P3")?
    file.writeLine(width, " ", height)?
    file.writeLine("255")?
    file.writeLine(255, " ", 0, " ", 0)?
    return(Result.ok())
  }

  [effects(io_err)]
  /log_file_error([FileError] err, [string] path) {
    print_line_error(path)
    print_line_error(": "utf8)
    print_line_error(err.why())
  }
  ```

### Image File I/O (draft)
- **Shared stdlib surface:** the shared image file-I/O API currently lives under `/std/image/*`.
- **Current prototype surface:**
  - `ImageError`
  - `ppm.read(width, height, pixels, path) -> Result<ImageError>`
  - `ppm.write(path, width, height, pixels) -> Result<ImageError>`
  - `png.read(width, height, pixels, path) -> Result<ImageError>`
  - `png.write(path, width, height, pixels) -> Result<ImageError>`
- **Buffer contract:** `width` and `height` are `i32` out-parameters, and `pixels` is a flat `vector<i32>` in RGB byte
  order (`r, g, b, r, g, b, ...`).
- **Current effect requirement:** image file I/O follows `File<...>` behavior: `ppm.read(...)` and `png.read(...)`
  require `effects(file_read, heap_alloc)` because they reset/materialize the pixel buffer, while `ppm.write(...)` and
  `png.write(...)` require `effects(file_write)`. `file_write` still implies `file_read` for compatibility with mixed
  read/write entrypoints.
- **Current backend contract:** `ppm.read(...)` currently parses ASCII `P3` and binary `P6` PPM files in VM/native/Wasm
  (and C++ emitter flows via the shared stdlib implementation). Missing files, malformed headers, overflowed read-side
  size arithmetic, unsupported max values, non-positive dimensions, missing binary-raster separators, truncated
  payloads, and out-of-range ASCII component values deterministically return `image_invalid_operation`. On Wasm-wasi,
  the current `effects(file_read, heap_alloc)` read contract now compiles through target validation instead of failing
  on the shared heap-backed pixel-buffer materialization path. `ppm.write(...)` now emits ASCII `P3` PPM files in
  VM/native/Wasm for strictly positive `width`/`height` pairs with an exact `width * height * 3` RGB payload; invalid
  dimensions, payload mismatches, overflowed write-side size arithmetic, out-of-range components, and file-open/write
  failures deterministically return `image_invalid_operation`. `png.read(...)` now validates PNG signatures/chunks,
  including CRCs for critical chunks and stricter `PLTE`/`IDAT` ordering for the current subset, and fully decodes the
  current PNG read subset for both non-interlaced and Adam7-interlaced images: 1/2/4/8/16-bit grayscale, 1/2/4/8-bit
  indexed-color, 8/16-bit grayscale+alpha, and 8/16-bit RGB/RGBA inputs whose `IDAT` payload uses stored/no-compression
  deflate blocks, fixed-Huffman deflate blocks, or dynamic-Huffman deflate blocks, with filter-`0`, filter-`1` (`Sub`),
  filter-`2` (`Up`), filter-`3` (`Average`), and filter-`4` (`Paeth`) scanlines. The shared decoder accepts a single
  `PLTE` chunk before the `IDAT` run when present, indexed-color inputs require that palette before decode, and
  multi-chunk `IDAT` payloads must stay consecutive once decoding data begins. Fixed-Huffman reads now cover both
  literal-only payloads and length/distance backreferences with overlapping copy semantics, and dynamic-Huffman reads
  now cover both literal-only payloads and length/distance backreferences with explicit code-length tables. Successful
  reads materialize the public flat RGB buffer, reconstructing Adam7 passes into image order while expanding packed
  grayscale samples to full-range RGB, downscaling 16-bit channel samples into RGB bytes, expanding palette indexes into
  RGB, and dropping alpha when decoding 8/16-bit grayscale+alpha or 8/16-bit RGBA inputs. Malformed or missing PNGs,
  including critical-chunk CRC mismatches, overflowed read-side size arithmetic, invalid `PLTE`/`IDAT` ordering,
  malformed Adam7 scanline payloads, and indexed palette overruns, deterministically return `image_invalid_operation`.
  `png.write(...)` now emits non-interlaced 8-bit RGB PNG files in VM/native/Wasm (and C++ emitter flows via the shared
  stdlib implementation) using a single `IHDR`/`IDAT`/`IEND` layout, stored/no-compression deflate blocks, and
  filter-`0` scanlines for the current write subset; invalid dimensions, payload mismatches, overflowed write-side size
  arithmetic, out-of-range components, and file-open/write failures deterministically return `image_invalid_operation`.
- **Error strings:** `ImageError.why()` currently returns `image_read_unsupported`, `image_write_unsupported`, or
  `image_invalid_operation`.
  - Import `/std/image/*` for the current stdlib-authored ImageError helper layer:
    `imageReadUnsupported()`, `imageWriteUnsupported()`, `imageInvalidOperation()`,
    `imageErrorStatus(err)`, and `imageErrorResult<T>(err)`.
  - The image stdlib layer also defines `/ImageError/why([ImageError] err)` as the public wrapper over the
    current `ImageError.why(err)` mapping so explicit type-owned calls stay on the stdlib surface. It also
    defines `/ImageError/read_unsupported()`, `/ImageError/write_unsupported()`, and
    `/ImageError/invalid_operation()` so ImageError constructor values can come from that same public
    type-owned stdlib surface instead of only from package-level image helpers.
- **Example:**
  ```
  import /std/image/*

  [effects(heap_alloc, io_out, file_write), return<int>]
  main() {
    [i32 mut] width{0i32}
    [i32 mut] height{0i32}
    [vector<i32> mut] pixels{vector<i32>{}}
    print_line(Result.why(ppm.read(width, height, pixels, "input.ppm"utf8)))
    print_line(Result.why(ppm.write("output.ppm"utf8, width, height, pixels)))
    print_line(Result.why(png.read(width, height, pixels, "input.png"utf8)))
    print_line(Result.why(png.write("output.png"utf8, width, height, pixels)))
    return(plus(width, height))
  }
  ```

## Runtime Stack Model
- **Frames:** VM and native execution both support dynamic call frames for `Call`/`CallVoid` when callable IR opcodes
  are present. Lowering still inlines source-level definition calls, so entry-lowered source programs typically use one
  frame unless callable IR is emitted directly. Locals live in fixed 16-byte slots; `location(...)` yields a byte offset
  into this slot space and `dereference` uses `LoadIndirect`/`StoreIndirect`.
- **Deterministic evaluation:** arguments evaluate left-to-right; boolean `and`/`or` short-circuit; `return(value)`
  unwinds the current definition. In value blocks, `return(value)` exits the block and yields its value. Implicit
  `return(void)` fires if a definition body reaches the end.
- **Indirect alignment:** indirect addresses must be 16-byte aligned; misaligned dereferences are VM runtime errors.
- **Transform boundaries:** text/semantic rewrites decide where bodies inline; IR lowering preserves left-to-right
  argument evaluation inside the active frame.
- **Resource handles:** handles live inside frame slots as opaque values; lifetimes follow lexical scope.
- **Tail execution:** lowering marks tail-position calls in metadata; backends may reuse frames when the tail flag is
  set. VM/native currently ignore the hint; GPU backends may require tail-safe forms for determinism.
- **Effect annotations:** purity by default; explicit `[effects(...)]` opt-ins. Effects are validated during lowering;
  runtime enforcement is limited to builtin checks.

### Execution Metadata
- **Scheduling scope:** queue/thread selection stays host-driven; there are no stack- or runner-specific annotations
  yet, so executions inherit the embedding runtime’s default placement. Lowering writes `scheduling_scope = 0` in PSIR
  metadata until explicit scheduling transforms exist.
- **Effects & capabilities:** lowering records `effect_mask` and `capability_mask` in PSIR metadata. If no
  `capabilities` transform is supplied, `capability_mask` mirrors the active effects for the definition; explicit
  capability lists narrow the mask.
- **Instrumentation:** `instrumentation_flags` are reserved for tracing/source-map hooks. Bit 0 (`tail_execution`) is
  set when the final statement is `return(def_call(...))` or (for `void` definitions) a final `def_call(...)`. Backends
  may treat this as a tail-call hint; no backend currently requires it.

### Native Allocator & Scheduler (IR Optimization Path)
- **Pipeline shape:** stack-form IR is lowered to block-local virtual registers, then processed in deterministic order:
  liveness intervals, linear-scan allocation, spill/reload insertion, block-local scheduling, and verifier checks.
- **Linear-scan allocator design:** each virtual register interval uses one `startPosition`/`endPosition` pair derived
  from liveness ranges; intervals are processed sorted by `(start, end, virtual_register)`.
- **Register assignment policy:** free physical registers are kept sorted ascending, so allocation picks the lowest
  available register first.
- **Spill heuristic:** only `SpillFarthestEnd` is supported. When pressure exceeds available physical registers, the
  active interval with the farthest end point is the spill candidate; ties spill the higher virtual register ID first.
- **Spill insertion contract:** spill/reload insertion consumes allocator assignments and emits deterministic edge-safe
  reloads/spills; verifier passes reject assignment/edge mismatches before lifting back to stack-form IR.
- **Scheduler design:** scheduling runs per basic block and preserves control-flow edge metadata. A dependency DAG is
  built from def-use edges plus barrier ordering constraints.
- **Scheduler barriers:** control flow, calls, returns, prints, file I/O, `StoreLocal`, and indirect loads/stores are
  treated as barriers to preserve observable behavior and memory ordering.
- **Scheduler heuristic:** ready instructions are chosen by highest latency score first (division > multiply >
  add/sub/compare > default), with a +2 penalty for each spilled use/def register; ties pick lower original instruction
  index.
- **Verification contract:** the schedule/allocation verifier checks one-to-one instruction mapping, dependency
  ordering, barrier ordering, register range validity, and branch-edge compatibility.
- **Debug dump format:** allocator/scheduler outcomes are tracked through the native emitter instrumentation dump format
  `native_emitter_debug_v1`, emitted by `formatNativeEmitterDebugDump(...)`.
  - `[instrumentation]` reports totals and sorted per-function counters (`function[i].*`), ordered by `(functionIndex,
    functionName)`.
  - `[optimization]` reports pre/post totals and `applied` (`1` when an optimization comparison was recorded, else `0`).
- **Debug dump example (optimization applied):**
  ```text
  native_emitter_debug_v1
  [instrumentation]
  total_instruction_count=4
  total_value_stack_push_count=3
  total_value_stack_pop_count=2
  total_spill_count=1
  total_reload_count=1
  function_count=1
  function[0].index=0
  function[0].name=/main
  function[0].instruction_total=4
  function[0].value_stack_push_count=3
  function[0].value_stack_pop_count=2
  function[0].spill_count=1
  function[0].reload_count=1
  [optimization]
  applied=1
  instruction_total_before=4
  instruction_total_after=3
  value_stack_push_count_before=3
  value_stack_push_count_after=2
  value_stack_pop_count_before=2
  value_stack_pop_count_after=2
  spill_count_before=1
  spill_count_after=0
  reload_count_before=1
  reload_count_after=0
  ```
- **Debug dump example (defaults):**
  ```text
  native_emitter_debug_v1
  [instrumentation]
  total_instruction_count=2
  total_value_stack_push_count=1
  total_value_stack_pop_count=1
  total_spill_count=1
  total_reload_count=1
  function_count=1
  function[0].index=0
  function[0].name=/main
  function[0].instruction_total=2
  function[0].value_stack_push_count=1
  function[0].value_stack_pop_count=1
  function[0].spill_count=1
  function[0].reload_count=1
  [optimization]
  applied=0
  instruction_total_before=0
  instruction_total_after=0
  value_stack_push_count_before=0
  value_stack_push_count_after=0
  value_stack_pop_count_before=0
  value_stack_pop_count_after=0
  spill_count_before=0
  spill_count_after=0
  reload_count_before=0
  reload_count_after=0
  ```

## Type System v1 (draft)

### Goals
- Deterministic, explicit typing across backends.
- Minimal implicit behavior; no hidden conversions.
- Portable core type set with explicit backend extensions.
- Whole-program checks remain supported, but local reasoning is preferred.

### Type Grammar (canonical)
- **Atomic:** `bool`, `i32`, `i64`, `u64`, `f32`, `f64`, `string`, `void`, `Self`.
- **Composite:** `array<T>`, `vector<T>`, `map<K, V>`, `Pointer<T>`, `Reference<T>`, stdlib-owned `soa_vector<T>`,
  and draft math value types (`Mat2`, `Mat3`, `Mat4`, `Quat`).
- **User types:** struct definitions and named aliases.
- **Template applications:** `Name<T1, T2, ...>`.

### Core Type Set (portable)
- `bool`, `i32`, `i64`, `u64`, `f32`, `f64`, `string`.
- `array<T>`, `vector<T>`, `map<K, V>` where parameters are core types.
- `soa_vector<T>` for explicit structure-of-arrays storage of SoA-safe struct `T`.
  The public API is stdlib-owned on top of generic language/runtime SoA
  substrate rather than a compiler-owned collection contract.
- Draft extension: `Mat2`, `Mat3`, `Mat4`, and `Quat` with explicit conversion-only interaction rules (not yet portable
  across backends).
- `Pointer<T>`, `Reference<T>` where `T` is primitive or a struct type.
- User-defined structs with layout manifests.
- Types outside this set are backend-specific and must be rejected by backends that do not support them.

### Type Ownership Model (architectural direction)
The ownership matrix below is the canonical reference for which public type surfaces stay
language/runtime-owned, which remain hybrid, and which should move fully into stdlib
`.prime` implementations.

- `core`
  Public types/surfaces: fixed-width scalars, `string`, `array<T>`, `Pointer<T>`, `Reference<T>`.
  Ownership rule: language/runtime owns both the public surface and the substrate because other
  features depend on them directly.
  Migration stance: treat these as stable substrate; delete workaround routing around them instead
  of trying to de-builtinize them.
- `hybrid`
  Public types/surfaces: `Result<T, Error>`, `File<Mode>`, `Buffer<T>`, `/std/gfx/*`.
  Ownership rule: keep only minimal builtin/runtime substrate for propagation, host I/O, and
  device interaction. Imported value-carrying `Result<T, Error>` construction now has a
  stdlib-owned sum surface under `/std/result/*`; legacy `Result.ok(value)`,
  `Result.map(result, fn)`, `Result.and_then(result, fn)`, and
  `Result.map2(left, right, fn)` have typed value-carrying sum compatibility
  paths on IR-backed VM/native. Those paths accept local imported Result sum
  sources and direct calls returning imported Result sums. Imported
  status-only `Result<Error>` construction, `pick`, `try(...)`, and
  `Result.error(...)` / `Result.why(...)` operands now live in stdlib on
  IR-backed VM/native for local, direct-call, and dereferenced local
  pointer/reference sources. `?` propagation still uses the hybrid bridge.
  Migration stance: move public constructors, helper APIs, and error-domain behavior into stdlib
  `.prime` wherever practical, then delete the old compatibility paths once the bridge is empty.
- `stdlib-owned`
  Public types/surfaces: `Maybe<T>`, `vector<T>`, `map<K, V>`, `soa_vector<T>`.
  Ownership rule: public API should live in stdlib `.prime` on top of minimal generic substrate.
  Migration stance: prefer slices that replace type-named compiler special cases with generic
  allocation/layout/drop substrate, then delete the old compatibility paths.

- `vector<T>` and `map<K, V>` therefore still appear in the portable type set today, but that
  should not be read as a permanent compiler-owned collection contract.
- `soa_vector<T>` is a promoted stdlib-owned public collection surface; direct
  experimental SoA imports remain targeted compatibility shims, not ordinary
  public API.

### Stdlib Surface-Style Boundary
This boundary is the scope reference for the stdlib surface-style cleanup lane in
`docs/todo.md`. It decides which `stdlib/std` paths should read like
user-facing library code and which ones remain intentionally canonical,
bridge-oriented, or substrate-heavy while ownership migrations are still in
flight.

- **Style-aligned surface code:** `stdlib/std/math/*`, `stdlib/std/maybe/*`,
  `stdlib/std/file/*`, `stdlib/std/image/*`, `stdlib/std/ui/*`,
  `stdlib/std/collections/vector.prime`,
  `stdlib/std/collections/map.prime`,
  `stdlib/std/collections/errors.prime`,
  `stdlib/std/collections/soa_vector.prime`,
  `stdlib/std/collections/soa_vector_conversions.prime`, and
  `stdlib/std/gfx/gfx.prime`.
- **Internal implementation, bridge, or substrate code:** `stdlib/std/bench_non_math/*`,
  `stdlib/std/collections/collections.prime`,
  `stdlib/std/collections/experimental_vector.prime`,
  `stdlib/std/collections/experimental_map.prime`,
  `stdlib/std/collections/experimental_soa_vector.prime`,
  `stdlib/std/collections/experimental_soa_vector_conversions.prime`,
  `stdlib/std/collections/internal_*`, and
  `stdlib/std/gfx/experimental.prime`.
- **Mixed-directory rule:** `stdlib/std/collections` and `stdlib/std/gfx`
  remain mixed on purpose, so contributors should follow the file-level mapping
  above rather than assuming the whole directory has one style target.

Style-aligned modules should follow `docs/CodeExamples.md`. The internal implementation or
bridge-oriented files may stay helper-heavy until the relevant migration or
cleanup TODO explicitly retargets them.

### Vector/Map Bridge Contract
This contract is the scope reference for future vector/map ownership-cutover
TODOs. Follow-on bridge tasks should share this boundary instead of
re-defining it piecemeal.

- **Bridge-owned public contract:** exact and wildcard `/std/collections`
  imports for `vector`/`map`, constructor and literal-rewrite surfaces for
  `vector<T>` and `map<K, V>`, collection helper families, compatibility
  spellings plus removed-helper diagnostics, semantic surface IDs, and lowerer
  dispatch metadata.
- **Migration-only seams:** rooted `/vector/*` and `/map/*` spellings plus
  `vectorCount` / `mapCount`-style lowering names remain temporary
  compatibility seams. The vector/map adapter cutover is complete for
  semantic and template-monomorph helper decisions; no active TODO targets
  deleting or accepting those temporary seams, so add a concrete successor TODO
  before changing their public status.
- **Compatibility adapter inventory:** map insert helper compatibility was the
  first migrated family. The semantic map-insert rewrite asks the
  `StdlibSurfaceRegistry` `CollectionsMapHelpers` adapter to classify
  canonical `/std/collections/map/insert(_ref)`, compatibility
  `/map/insert(_ref)`, wrapper `/std/collections/mapInsert(_Ref)`, and
  experimental `/std/collections/experimental_map/mapInsert(_Ref)` spellings
  before it rewrites constructor-backed receivers to the canonical helper or
  non-local/builtin receivers to `/std/collections/map/insert_builtin`.
  Template monomorphization now asks the same registry for preferred
  experimental vector/map/SoA helper spellings instead of carrying bespoke
  canonical-to-experimental helper maps. SoA helper compatibility is routed
  through `StdlibSurfaceRegistry::CollectionsSoaVectorHelpers` for canonical
  `/std/collections/soa_vector/*`, same-path `/soa_vector/*`, mixed
  `/std/collections/{count,get,ref,reserve,push}`, experimental helper wrapper,
  and conversion-helper spellings. Vector/map and SoA constructor
  compatibility is already metadata-backed by the constructor surface adapters.
  Gfx Buffer helper compatibility is routed through
  `StdlibSurfaceRegistry::GfxBufferHelpers` for canonical `/std/gfx/Buffer/*`,
  legacy `/std/gfx/experimental/Buffer/*`, and rooted `/Buffer/*` helper
  spellings before semantic gfx-buffer rewrites and GPU wrapper diagnostics
  choose canonical builtin targets. Remaining explicit removed-helper
  diagnostics, import spellings, wildcard expansion, user-defined helper
  precedence, field-view field-name lowering, gfx constructor sugar, and lowerer
  raw-path dispatch checks are syntax/provenance-owned or lowering-owned.
- **Internal implementation seams:** `/std/collections/experimental_vector/*`
  and `/std/collections/experimental_map/*` remain implementation-owned
  modules behind the canonical wrappers; direct imports should stay limited to
  targeted compatibility or conformance coverage rather than ordinary public
  API use.
- **Out of scope for this bridge lane:** `array<T>` core ownership, promoted
  `soa_vector<T>` implementation details, and runtime storage/allocator
  redesign stay outside the vector/map bridge contract and require separate
  TODO lanes when they move.

### Stdlib De-Experimentalization Policy
This policy is the scope reference for the stdlib de-experimentalization lane
in `docs/todo.md`. Use it to classify every `experimental` stdlib namespace
before renaming, deleting, or promoting that surface.

- **Canonical public API:** a non-`experimental` namespace that docs, examples,
  and future compiler-path authority should treat as the long-term user-facing
  contract.
- **Temporary compatibility namespace:** an `experimental` namespace that
  remains importable only to preserve current behavior while the canonical
  namespace or maturity decision finishes. Every compatibility namespace must
  get an explicit exit or downgrade TODO before work starts to retire, promote,
  or reclassify it.
- **Internal substrate/helper namespace:** an `experimental` namespace that is
  implementation plumbing rather than public API. It may stay imported by
  wrappers, conformance harnesses, or bridge code, but it should not be
  presented as an ordinary user-facing contract.
- **Default rule:** no `experimental` namespace counts as canonical public API
  by default. If a surface still needs public incubation, the docs must say so
  explicitly, and future promotion or retirement work must be tied to a
  concrete TODO instead of relying on blanket `experimental` wording.

Current `stdlib/std` experimental module classification:

| Namespace family | Current role | Current interpretation | Follow-up |
| --- | --- | --- | --- |
| `/std/collections/experimental_vector/*` | Internal substrate/helper namespace | Internal implementation module behind the canonical `/std/collections/vector/*` public contract; direct imports remain only for targeted compatibility or conformance coverage. | none |
| `/std/collections/experimental_map/*` | Internal substrate/helper namespace | Internal implementation module behind the canonical `/std/collections/map/*` public contract; direct imports remain only for targeted compatibility or conformance coverage. | none |
| `/std/gfx/experimental/*` | Temporary compatibility namespace | Legacy compatibility shim over canonical `/std/gfx/*`; no longer part of the public gfx contract and retained only for targeted compatibility coverage while the residual seam remains importable. | none |
| `/std/collections/experimental_soa_vector/*` | Accepted compatibility namespace | Compatibility module behind the promoted canonical `/std/collections/soa_vector/*` public surface; direct imports are accepted only for targeted compatibility or conformance coverage. C++/VM/native compile-run coverage locks this compatibility seam; ordinary public examples should use `/std/collections/soa_vector/*`. | none |
| `/std/collections/experimental_soa_vector_conversions/*` | Accepted compatibility namespace | Compatibility conversion module for direct experimental SoA conversion imports; canonical conversions route through `/std/collections/internal_soa_vector_conversions/*`, and direct imports remain only for targeted compatibility or conformance coverage. | none |
| `/std/collections/internal_buffer_checked/*` | Internal substrate/helper namespace | Explicitly internal checked buffer plumbing for container conformance and memory-wrapper flows, not a stable user-facing stdlib API. | none |
| `/std/collections/internal_buffer_unchecked/*` | Internal substrate/helper namespace | Explicitly internal unchecked buffer plumbing for container conformance and memory-wrapper flows, not a stable user-facing stdlib API. | none |
| `/std/collections/internal_soa_vector/*` | Internal substrate/helper namespace | Internal SoA wrapper implementation adapter used by canonical `/std/collections/soa_vector/*`; it is not a public import surface. It still preserves the current compatibility `SoaVector<T>` type identity behind the promoted public wrapper surface. | none |
| `/std/collections/internal_soa_vector_conversions/*` | Internal substrate/helper namespace | Internal SoA conversion implementation adapter used by canonical `/std/collections/soa_vector_conversions/*` and `/std/collections/soa_vector/to_aos*`; it is not a public import surface. | none |
| `/std/collections/internal_soa_storage/*` | Internal substrate/helper namespace | Explicitly internal SoA storage/layout plumbing used by wrappers and lowering bridges, not a canonical surface contract. | none |

The policy implication is immediate: vector/map/gfx/SoA work should prefer
canonical non-`experimental` namespaces in docs and compiler authority, while
substrate helpers should be treated as explicit implementation namespaces
rather than as candidate public APIs.

### SoA Public Collection Contract
This section is the scope reference for the promoted `soa_vector<T>` public
surface. It remains separate from vector/map implementation details because SoA
uses a distinct structure-of-arrays substrate and field-view invalidation model.

- **Current status:** `soa_vector<T>` is a promoted stdlib-owned public
  collection surface. Ordinary public code should use the canonical SoA
  namespaces below instead of direct experimental imports; retained
  experimental namespaces are compatibility shims for targeted tests and
  migration coverage.
- **Current user-facing surface:** `/std/collections/soa_vector/*` and
  `/std/collections/soa_vector_conversions/*` are the canonical public
  spellings. The canonical wrapper owns ordinary constructor, count/get/ref,
  reserve/push, field-view, and AoS conversion helper names. The canonical
  conversion helpers use `SoaVector<T>` and `Reference<SoaVector<T>>` receiver
  spellings while routing through canonical `/std/collections/soa_vector/*`
  helper paths. One source-locked wildcard canonical parity program runs
  across C++ emitter, VM, and native for construction/read/ref/mutator,
  field-view, and conversion behavior without direct experimental SoA imports
  in the test source.
- **Accepted compatibility seams:** `/std/collections/experimental_soa_vector/*`
  and `/std/collections/experimental_soa_vector_conversions/*` remain importable
  only for targeted compatibility and conformance coverage. Existing C++/VM/native
  direct-import tests are the contract for these seams. They are not
  user-facing contracts, and ordinary public examples should continue to use
  `/std/collections/soa_vector/*` and
  `/std/collections/soa_vector_conversions/*`.
- **Internal substrate namespaces:** `/std/collections/internal_soa_vector/*`
  owns ordinary canonical wrapper implementation forwarding,
  `/std/collections/internal_soa_vector_conversions/*` owns ordinary canonical
  conversion implementation forwarding, while
  `/std/collections/internal_soa_storage/*` stays implementation-facing
  storage/layout plumbing. None of these are public APIs. The internal wrapper
  adapter still preserves the compatibility `SoaVector<T>` type identity behind
  the promoted public wrapper surface. The inline-parameter and direct lowerer
  wrapper-dispatch bridges no longer use rooted `/soa_vector/*`, `/to_aos`, or
  `/to_aos_ref` spellings as hidden raw fallbacks.
- **Promoted contract:** public behavior is owned by canonical stdlib surfaces
  and the remaining compiler/runtime pieces are generic substrate rather than
  SoA-specific compatibility paths:
  - Construction, read/ref, mutator, and field-view helpers are spelled through
    `/std/collections/soa_vector/*` with `SoaVector<T>` and
    `Reference<SoaVector<T>>` receivers; method sugar lowers to those same
    canonical helpers and preserves deterministic user-helper shadowing.
  - AoS/SoA conversions are spelled through
    `/std/collections/soa_vector_conversions/*` and canonical
    `/std/collections/soa_vector/to_aos*` helpers; conversion receivers do not
    require direct imports of experimental conversion modules in ordinary code.
  - Borrowed `ref(...)` values, field views, structural mutation, explicit
    conversions, owner destroy, and scope exit share one documented
    invalidation model, and invalid escapes fail with deterministic diagnostics.
  - C++ emitter, VM, and native coverage exercise the same canonical
    construction/read/ref/mutator, field-view, and conversion programs where
    each backend supports the feature; unsupported paths reject explicitly
    instead of falling back to hidden raw-builtin behavior.
  - `/std/collections/internal_soa_vector/*` and
    `/std/collections/internal_soa_storage/*` remain implementation-only, and
    retained `/std/collections/experimental_soa_vector*/*` paths are documented
    compatibility shims with conformance coverage.

### Backend Profiles
- A definition is well-typed only with respect to a backend profile.
- Profiles include: `vm_native`, `glsl`, `cpp`.
- Each profile declares supported types, effects, and constructs.
- A definition may declare `[profile(vm_native, cpp)]`. If omitted, the compiler target profile is used.

### Typing Context
- Type checking runs after import expansion and namespace resolution.
- The typing context maps local bindings, parameters, signatures, struct layouts, and import aliases to resolved types.

### Expression Rules
- **Literals:** fixed type determined by suffix (`1i32`, `2u64`, `1.0f32`, `"hi"utf8`, `true`).
- **Bindings:** declared type in the transform list, or `auto` if omitted.
- **Calls:** well-typed if the callee resolves to a definition or builtin, and every argument matches the parameter
  type.
- **Constructors:** value construction uses brace forms (`Type{...}` and context-typed `{...}`), never call syntax.
  Struct constructors map positional/labeled entries to fields; all fields must be provided or defaulted.
- **`convert<T>` and `T{...}`:** explicit conversions; `T` must be a supported conversion target.
- **`assign(target, value)`:** allowed only when `target` is mutable and `value` has the exact same type.
- **Matrix/quaternion operators (draft):** matrix and quaternion arithmetic is explicit and shape-checked; no implicit
  widening or family conversion is allowed.
- **Control flow:** `if`, `while`, and `for` conditions must be `bool`. `loop(count)` requires an integer type.
- **Pointers/references:** targets are restricted as described in pointer rules; no implicit conversions.

### Definition Rules
- Parameters use binding syntax and must resolve to concrete types before lowering (per instantiation).
- Return types are explicit (`return<T>`) or inferred from `return(value)` sites when `return<auto>` is used.
- All `return(value)` statements in a definition must agree on the same type.
- `return()` is only valid for `void`.

### Inference Rules
- `auto` is allowed on bindings, parameters, and return transforms.
- Signature `auto` introduces implicit template parameters; template arguments are inferred per call site from
  parameter and return constraints.
- Binding `auto` is inferred from the initializer within the definition.
- Unresolved or conflicting `auto` is a diagnostic.

### Traits and Constraints
- Traits are explicit requirements over named functions (not operators).
- Trait constraints are explicit in signatures; no inference of trait bounds.
- A type satisfies a trait if all required functions resolve in the current program and backend profile.
- Minimal built-in traits:
  - `Additive<T>` requires `plus(T, T) -> T`.
  - `Multiplicative<T>` requires `multiply(T, T) -> T`.
  - `Comparable<T>` requires `equal(T, T) -> bool` and `less_than(T, T) -> bool`.
  - `Indexable<T, Elem>` requires `count(T) -> i32` and `at(T, i32) -> Elem`.
Example (trait constraints are transforms on definitions):
```
[return<i32> Additive<i32> Comparable<i32>]
sum([i32] left, [i32] right) {
  return(plus(left, right))
}
```

### Parametric Types
- Type parameters are explicit in signatures or implicit via `auto` in signatures.
- Type arguments must be fully concrete before lowering.
- Template argument inference is limited to local call sites.

### Algebraic Data Types (v1.1 target)
- `enum` introduces tagged union types.
- Enum syntax follows the standard envelope rules; the `enum` transform rewrites the declaration to an ordinary struct
  plus static bindings so the base language never needs a dedicated enum form.
- Enums use an integer backing type. The default is `i32`; `enum<i64>` and `enum<u64>` select other widths.
- Entries may specify explicit integer literal values (via `Name = 5i32` or `assign(Name, 5i32)`), and omitted values
  auto-increment from `0` or the previous entry. Values must fit in the backing type; unsigned enums reject negatives.
- Pattern matching must be exhaustive and type-checked.
- `match` expressions have a single result type.

Enum example (surface):
```
[enum]
Colors {
  Blue = 5
  Red
  Green
}
```

Enum example (desugared shape):
```
[struct]
Colors() {
  [i32] value{0i32}

  [public static Colors] Blue{Colors{[value] 5i32}}
  [public static Colors] Red{Colors{[value] 6i32}}
  [public static Colors] Green{Colors{[value] 7i32}}
}
```

Enum entry access uses static field syntax (`Colors.Blue`) and rewrites to brace construction (`Colors{[value] 5i32}`).

### Ownership and Mutability
- `mut` marks writeable bindings.
- `move(x)` consumes a binding and forbids use until reinitialized.
- References cannot be moved.
- **Borrow rules (safe scope):**
  - A `Reference<T>` is a borrow of a storage location.
  - Many immutable borrows are allowed, or one mutable borrow; they may not overlap.
  - Borrows can end at last use before lexical scope exit (non-lexical lifetimes), including alias tracking through
    `location(ref)`.
  - Last-use analysis is conservative across complex control flow: if later use cannot be proven absent, the borrow
    stays active.
  - Borrowing a field borrows the whole struct value (no field-splitting in v1).
  - Borrowed bindings cannot be reassigned or moved until all borrows end.
  - `return<Reference<T>>` accepts either a direct `Reference<T>` parameter (`return(paramRef)`) or a direct
    parameter-rooted borrowed carrier such as `return(borrow(dereference(slot)))` when the borrowed storage is
    rooted in parameter-owned `uninitialized<T>` storage; local and non-parameter-rooted reference escapes are still
    rejected.
- **Unsafe scopes:** `[unsafe]` on a definition allows aliasing within that body, and also allows pointer-to-reference
  initialization from pointer-like expressions when types match. References created there must not escape the unsafe
  scope. Unsafe scopes are aliasing barriers for optimization.
- **Unsafe calls:** unsafe definitions may be called from safe code; the call does not taint the caller as long as
  unsafe-created references do not escape.

### Layout and Struct Semantics
- Structs record layout manifests in IR.
- Recursive-by-value struct layouts are rejected.
- `[pod]` asserts trivially-copyable semantics and is validated.

### Diagnostics
- Diagnostics include expected type, actual type, source span, and backend profile.
- Diagnostic messages must be stable for snapshot testing.

### Conformance Tests (required)
- **Positive typing:** literals, calls, struct construction, pointers/references.
- **Negative typing:** return mismatch, invalid assign, invalid convert, invalid pointer target.
- **Inference:** binding inference, implicit-template inference, unresolved `auto`.
- **Traits:** satisfied, missing requirement, incorrect signature.

### Type Semantics (draft)
- **Nested generics:** template arguments may themselves be generic envelopes (`map<i32, array<i32>>`), and the parser
  preserves the nested envelope string for later lowering.
- **Field visibility:** stack-value declarations accept `[public]` or `[private]` transforms (default: public); they are
  mutually exclusive. The compiler records `visibility` metadata per field so tooling and backends enforce access rules
  consistently. Field visibility is in-language field-access metadata; it does not make a field a package-importable API
  symbol.
- **Static members:** add `[static]` to hoist storage to namespace scope while reusing the field’s visibility transform.
  Static fields still participate in the struct manifest so documentation and reflection stay aligned, but only one
  storage slot exists per struct definition.
- **Example:**
  ```
  import /std/math/*
  namespace demo {
    [struct]
    brush_settings() {
      [f32] size{12.0f32}
      [private f32] jitter{0.1f32}
      [private static handle<Texture>] palette{load_default_palette()}

      [public]
      Create() {
        assign(this.size, clamp(this.size, 1.0f32, 64.0f32))
      }
    }
  }
  ```
- **Constructor semantics:** struct constructors use field initializers as defaults; `Create`/`Destroy` remain optional
  hooks. Constant member behavior follows the normal `mut` rules (immutable unless declared `mut`).
  - **Zero-arg constructor exists when:** either (a) every field has an initializer, or (b) a `Create()` helper exists
    and initializes every field (including `uninitialized<T>` fields via `init`).
  - **Execution order:** field initializers run first, then `Create()` runs (if present) and may override field values.
  - **Effect interaction:** a zero-arg constructor is outside-effect-free only when both `Create()` (if present) and all
    field initializers are effect-free under the “no outside effects” rules.

## Move/Copy/Destroy
- **Lifecycle set:** structured types can define `Create`, `Move`, `Copy`, and `Destroy` helpers. `Create`/`Destroy`
  are optional hooks; `Move`/`Copy` must be nested inside the struct, return `void`, and accept exactly one parameter.
- **Copy signature:** the canonical copy constructor is `Copy([Reference<Self>] other) { ... }`. A shorthand
  `Copy(other) { ... }` desugars to the reference form.
- **Move-by-default:** assignments, argument passing, and returns consume values unless the type is `Copy` or the value
  is a `Reference<T>`.
- **`Copy` types (Rust-aligned):** values that can be duplicated by a simple bitwise copy with no custom destruction.
  - Built-in `Copy` types: `bool`, `i32`, `i64`, `u64`, `f32`, `f64`, `Pointer<T>`, `Reference<T>`.
  - Structs are `Copy` when they are `[pod]`, all fields are `Copy`, and they do **not** define `Destroy` or `Copy`.
  - Types with `handle`/`gpu_lane` fields are not `Copy` (they cannot be `[pod]`).
- **Copy helper use:** ownership-sensitive lowering may route aggregate duplication through `Copy` when one exists.
- **Move signature:** the canonical move constructor is `Move([Reference<Self>] other) { ... }`. A shorthand
  `Move(other) { ... }` desugars to the reference form.
- **Explicit move:** `move(value)` is the explicit consume helper. It requires a local binding or parameter name (no
  arbitrary expressions) and marks the source binding as moved-from while returning its value.
- **Use-after-move:** any use of a moved-from binding is a compile error until it is re-initialized (e.g.,
  `assign(value, ...)`). The analysis is conservative across control flow.
- **Pointer behavior:** pointers can be moved; moved-from pointers are treated as invalid without being auto-zeroed (no
  implicit `null` literal; `0x0` is just a numeric value).
- **References:** `move(...)` rejects `Reference<T>` bindings; references do not participate in move semantics.
- **Backend note:** `move(...)` is a semantic ownership marker. VM/native lower it as a passthrough; the C++ emitter
  emits `std::move`.

## Uninitialized Storage (draft)
- **Purpose:** model explicit, inline uninitialized storage without implicit construction (C-style tagged storage and
  optional values).
- **Envelope:** `uninitialized<T>` allocates space for `T` but does not construct a `T` value.
- **Allowed locations:** local bindings and struct fields only. `uninitialized<T>` is rejected for parameters, return
  types,
  collection elements, nested pointer/reference targets, or template arguments to user-defined types. Top-level
  `Pointer<uninitialized<T>>` and `Reference<uninitialized<T>>` wrappers are allowed as storage handles.
- **Initialization:** `init(storage, value)` constructs `T` in-place.
  - `storage` must be `uninitialized<T>` or `dereference(ptr)` where `ptr` is `Pointer<uninitialized<T>>` or
    `Reference<uninitialized<T>>`, and it must be definitely uninitialized at the call site.
  - `value` is consumed (move-by-default); use `clone(value)` to duplicate.
  - After `init`, the storage is definitely initialized.
- **Destruction:** `drop(storage)` destroys the in-place value and marks the storage uninitialized.
  - `storage` must be definitely initialized; otherwise a diagnostic is emitted.
- **Access:**
  - `take(storage)` moves the value out and leaves the storage uninitialized.
  - `borrow(storage)` now supports standalone `[Reference<T>]` bindings for direct local/field storage and
    pointer/reference-backed `borrow(dereference(slot))` storage surfaces, and helper
    `return<Reference<T>>` contracts now also accept direct borrowed carriers rooted in parameter-owned storage.
    Internal stdlib slot-borrow helpers now also validate through `[return<Reference<T>>]` with slot-pointer
    provenance preserved through local `slot` aliases, while public value-returning helpers such as
    `soaColumnRef<T>(...)`, `soaColumnRead<T>(...)`, `vectorAt<T>(...)`, and `vectorAtUnsafe<T>(...)` still
    intentionally dereference that borrowed carrier back to whole-element `T`. The storage must be initialized.
  - Any other use of an `uninitialized<T>` value is a type error.
- **Lifetime rules:** using a storage value after `drop`/`take` is a compile-time error until it is reinitialized.
- **`Destroy` handling:** structs owning `uninitialized<T>` fields must explicitly `drop` them when initialized.

## Constructor-Shaped Compatibility Inventory
- **Default rule:** value construction uses braces (`Type{...}` or a context-typed `{...}`). A call-shaped form that
  looks like construction is an ordinary helper call or a documented compatibility rewrite, not field-mapping
  construction.
- **File:** imported `File<Mode>(path)` is retained as compatibility helper syntax. It resolves through
  `/File/openRead(...)`, `/File/openWrite(...)`, or `/File/openAppend(...)` after `/std/file/*` is imported and remains
  subject to the same `file_read` / `file_write` effects. Lowerer inference coverage locks this as a fallible helper
  return, not as a `File<Mode>` value constructor.
- **Graphics:** imported `Window(...)`, `Device()`, and `Buffer<T>(count)` are retained compatibility entry points over
  `/std/gfx/*` and `/std/gfx/experimental/*`. `Window(...)` and `Device()` route through stdlib create helpers and keep
  their `Result`/`?` shape; `Buffer<T>(count)` routes through the gfx buffer helper rewrite. Gfx semantic tests also keep
  bare explicit bindings without `?` on the mismatch path, proving these are not plain value constructors.
- **Collections:** brace forms such as `array<T>{...}`, `vector<T>{...}`, and `map<K, V>{...}` are preferred
  construction syntax. Legacy call-shaped `array<T>(...)`, `vector<T>(...)`, `map<K, V>(...)`, canonical
  `/std/collections/vector/vector<T>(...)`, `/std/collections/map/map<K, V>(...)`, and `soa_vector<T>(...)` spellings
  remain compatibility helper families registered through stdlib surface metadata; semantic-product tests publish their
  constructor surface IDs as helper metadata rather than struct field construction.
- **Maybe/Result migration:** `Maybe<T>` is now a stdlib-owned sum with `none` and `some` variants. `Maybe<T>{}`
  defaults to `none`, `Maybe<T>{none}` is the explicit empty form, `some<T>(value)` is the named helper, and
  `Maybe<T>{value}` is accepted when the `some` payload is the only matching variant. The remaining Result
  propagation and legacy cleanup work is tracked by TODO-4266 and TODO-4267, not by file/gfx/collection compatibility.
- **Follow-up policy:** do not add new constructor-shaped compatibility surfaces. Existing retained forms must either
  route through named helpers with focused coverage or get a dedicated migration TODO with scope, acceptance, and
  stop_rule before their status changes.

## Optional Values (Maybe)
- **Purpose:** represent either "no value" or a value of `T` without heap allocation.
- **Naming:** `Maybe<T>` is the canonical optional type in PrimeStruct; there is no separate `Option<T>`.
- **Concrete representation:** `Maybe<T>` is a stdlib-owned generic sum type with a unit `none` variant followed by a
  payload-carrying `some` variant. The active variant is represented by the generic sum layout contract.
- **Ergonomic constructor surface:** `Maybe<T>{}` yields `none`, `Maybe<T>{none}` is the explicit empty form, and
  `some<T>(value)` constructs the present variant. Direct `Maybe<T>{value}` construction is also accepted when the
  payload uniquely matches the `some` variant; use `some<T>(value)` when call-site clarity matters.
- **Helper surface (stdlib):** `isEmpty()` / `isSome()` and compatibility wrappers `is_empty()` / `is_some()` are
  implemented as type-path helpers over `pick`. The old mutable struct helpers `set(value)`, `clear()`, and `take()`
  are retired for the sum-backed representation until a dedicated mutable active-variant contract lands.
- **Example shape:**
  ```
  [sum]
  Maybe<T> {
    none
    [T] some
  }

  [return<Maybe<T>>]
  some<T>([T] value) {
    [Maybe<T>] result{[some] value}
    return(result)
  }

  [return<Maybe<T>>]
  none<T>() {
    [Maybe<T>] result{}
    return(result)
  }

  [return<bool>]
  /Maybe/isSome<T>([Maybe<T>] self) {
    return(pick(self) {
      none {
        return(false)
      }
      some(value) {
        return(true)
      }
    })
  }
  ```

Draft tests (shape only):
```
// Positive: create empty, create filled, inspect with pick.
[return<i32>]
maybe_basic() {
  [Maybe<i32>] a{none<i32>()}
  [Maybe<i32>] b{some<i32>(1i32)}
  return(pick(b) {
    none {
      return(0i32)
    }
    some(value) {
      return(value)
    }
  })
}

// Negative: retired mutable helper.
[return<i32>]
bad_set() {
  [Maybe<i32> mut] value{none<i32>()}
  value.set(1i32) // error: no sum-backed Maybe set helper is defined yet
  return(0i32)
}
```

## Lambdas & Higher-Order Functions
- **Syntax mirrors definitions:** lambdas omit the identifier (`[captures] <T>(params){ body }`). The capture list is
  required but may be empty (`[]`). Template arguments are optional.
- **Capture semantics:** supported forms are `[]`, `[=]`, `[&]`, and explicit entries (`[name]`, `[value name]`, `[ref
  name]`). Entries are comma/semicolon separated. Explicit names must resolve to parameters or locals in the enclosing
  scope; duplicates are diagnostics. `=` or `&` capture-all tokens cannot be repeated. `ref` marks a by-reference
  capture; otherwise captures are by value.
- **Parameters:** lambda parameters must use binding syntax. Defaults are allowed but must be literal/pure expressions
  and may not use named arguments.
- **Backend support (current):** lambdas are supported only by the C++ emitter, which lowers them to native C++ lambdas
  with the same capture list. IR/VM/GLSL backends reject lambdas; use named definitions when targeting those backends.
- **Inlining transforms:** standard transforms may inline pure lambdas in C++ emission paths; non-pure lambdas remain as
  closures.

## Literals & Composite Construction
- **Numeric literals:** decimal, float, hexadecimal with optional width suffixes (`42i64`, `42u64`, `1.0f64`).
- Integer literals require explicit width suffixes (`42i32`, `42i64`, `42u64`) unless `implicit-i32` is enabled (on by
  default). Omit it from `--text-transforms` (or use `--no-text-transforms`) to require explicit suffixes.
- Float literals accept `f32` or `f64` suffixes; when omitted in surface syntax they default to `f32`. Canonical form
  requires `f32`/`f64`. Exponent notation (`1e-3`, `1.0e6f32`) is supported.
- Commas may appear between digits in the integer part as digit separators and are ignored for value (e.g., `1,000i32`,
  `12,345.0f32`). Commas are not allowed in the fractional or exponent parts, and `.` is the only decimal separator.
- **Strings:** double-quoted strings process escapes unless a raw suffix is used; single-quoted strings are raw and do
  not process escapes. `raw_utf8` / `raw_ascii` force raw mode on either quote style. Surface literals accept
  `utf8`/`ascii`/`raw_utf8`/`raw_ascii` suffixes; the canonical/bottom-level form uses double-quoted strings with
  normalized escapes and an explicit `utf8`/`ascii` suffix. `implicit-utf8` (enabled by default) appends `utf8` when
  omitted.
- **Boolean:** keywords `true`, `false` map to backend equivalents.
- **Composite constructors:** structured values are introduced through brace constructor forms only. Calls such as
  `ColorGrade(...)` are ordinary executions and do not construct values. Use `ColorGrade{[hue_shift] 0.1f32 [exposure]
  0.95f32}` or a context-typed binding initializer such as `[ColorGrade] grade{[hue_shift] 0.1f32 [exposure] 0.95f32}`.
  Labeled entries map to fields, and every field must have either an explicit entry or an envelope-provided default
  before validation.
  - **Defaults & validation:** struct constructors accept positional and labeled arguments. Missing fields are filled
    from their field initializers; if a field has no initializer, zero-arg construction requires a `Create()` helper to
    assign it. Extra arguments are a semantic error (`argument count mismatch`).
  - **Multi-expression blocks:** constructor braces hold constructor field entries. General binding initializer blocks
    still produce one value via `return(value)` or the final expression when they are not parsed as a constructor entry
    list.
- **Algebraic sum types (partial):** `[sum]` definitions declare named, field-like variants. Payload-carrying variants
  have one payload envelope; unit/no-payload variants use bare lowerCamelCase names such as `none`. The parser,
  validator, and semantic product preserve source-order tag metadata for both forms; unit variant metadata records
  `has_payload=false` and an empty payload type. Explicit `[variant] payload` construction is validated for typed
  bindings, return values, fields, and `auto` bindings when the sum type is named. Target-typed inferred construction
  is also validated for bindings, call/field arguments, and returns when exactly one payload-carrying variant accepts
  the payload. A sum value has exactly one active variant at runtime.
  ```prime
  [sum]
  Shape {
    none
    [Circle] circle
    [Rectangle] rectangle
    [Text] text
  }

  [Circle] circleA{[radius] 3.4}
  [Circle] circleB{3.4}
  [Circle] circleC{3.4}
  [Shape] a{[circle] circleA}
  [Shape] b{[circle] circleB}
  [Shape] c{circleC}
  ```
  The explicit `[circle]` form selects the variant directly. The inferred form (`[Shape] c{circleC}`) is valid only
  when exactly one `Shape` variant accepts a `Circle` payload. Zero matches are type errors and multiple matches are
  ambiguity errors requiring an explicit `[variant]` label.
  Unit variants can be constructed by default when the first declared variant is unit, or by naming the unit variant in
  a target-typed constructor:
  ```prime
  [sum]
  MaybeI32 {
    none
    [i32] some
  }

  [MaybeI32] a{}
  [MaybeI32] b{none}
  [MaybeI32] c{[some] 1i32}
  ```
  Generic sum declarations use the same template syntax as generic structs and helpers. Template parameters may appear
  in payload envelopes, and each concrete use is monomorphized before validation and semantic-product publication:
  ```prime
  [sum]
  Maybe<T> {
    none
    [T] some
  }

  [sum]
  Result<T, E> {
    [T] ok
    [E] err
  }

  [Maybe<i32>] a{}
  [Maybe<i32>] b{[some] 1i32}
  [Result<i32, string>] c{[ok] 2i32}
  ```
  Monomorphized sum metadata records the substituted payload type text, keeps source-order variant indices/tags, and
  rejects invalid template arity. Recursive inline payloads such as `Bad<T> { [Bad<T>] again }` are unsupported until
  recursive sum layout is designed. The default sum construction rule is valid only when the first declared variant is
  a unit variant; the default active variant is tag `0` in source order. Payload variants are not default-constructed
  implicitly, so `[Shape] value{}` is rejected when the first `Shape` variant carries a payload. Variant order is
  therefore layout/serialization-sensitive and should be treated as version-sensitive API surface. `Maybe<T>` uses this
  substrate, and imported value-carrying `Result<T, E>` construction is available through `/std/result/*`.
  Legacy `Result.ok(value)`, `Result.map(result, fn)`, `Result.and_then(result, fn)`, and
  `Result.map2(left, right, fn)` can initialize typed imported value-carrying sum locals/returns on IR-backed VM/native
  paths with local imported Result sum sources or direct calls returning them. `try(...)`, `Result.error(...)`, and
  `Result.why(...)` can inspect imported value-carrying sum values directly or through dereferenced local
  `Reference<Result<T, E>>` / `Pointer<Result<T, E>>` values. Imported status-only `Result<E>` values can be
  constructed, used as `pick` targets, and consumed by IR-backed `try(...)`, `Result.error(...)`, and
  `Result.why(...)` from locals, direct calls, and dereferenced local `Reference<Result<E>>` /
  `Pointer<Result<E>>` sources.
  `pick(value) { variant(payload) { ... } }` is the semantically validated exhaustive pattern form. Payload variants
  require binders such as `circle(c) { ... }`; missing variants, duplicate variants, unknown variants, and incompatible
  branch value types are diagnostics. Unit variants use binder-free arms such as `none { ... }`, and payload binders on
  unit variants are rejected. IR-backed VM, native, and exe lowering currently execute scalar, unit, and struct-payload
  sums with an inline aggregate convention: slot 0 stores the payload-slot header, slot 1 stores the active variant tag,
  and slot 2 starts the active payload storage. Unit variants carry only the tag. Scalar payloads occupy one slot; struct
  payloads occupy their ordinary struct slot layout inline and `pick` arms bind a
  branch-local view of the selected payload only. Aggregate-valued `pick(...)` expressions copy the selected active
  payload into stable result storage before the value can be bound, returned, or passed to a helper, so inactive payload
  storage stays unobserved by the escape path. Native constructor and
  initializer selection consumes published sum-variant metadata before choosing
  payload storage on semantic-product-backed paths. Native `try(...)` lowering
  consumes published Result sum-variant metadata before matching payload shape,
  loading payload slots, copying propagated errors, or branching on tags.
  Sum-to-sum `move(...)` construction copies the active tag and routes only
  the selected aggregate payload through its `Move`/`Copy` helper when one exists, falling back to slot copy for
  helper-free payloads. Explicit `drop(storage)` for `uninitialized<Sum>` storage routes only the active aggregate
  payload through `DestroyStack`/`Destroy` when a payload helper exists; inactive payload storage is never observed or
  destroyed. Nested sum payloads remain unsupported until recursive sum layout is designed, so public examples should
  still avoid ownership-heavy nested struct-payload sums.
- **Labeled arguments:** labeled arguments use a bracket prefix (`[name] value`) and may be reordered (including on
  executions). Positional arguments fill the remaining parameters in declaration order, skipping labeled entries.
  Builtin calls (operators, comparisons, clamp, convert, pointer helpers, collections) do not accept labeled arguments.
  - Example: `sum3(1i32 [c] 3i32 [b] 2i32)` is valid.
  - Example: `array<i32>{[first] 1i32}` is rejected unless the collection constructor defines a `first` entry.
  - Duplicate labeled arguments are rejected for definitions and executions (`execute_task([a] 1i32 [a] 2i32)`).
  - Variadic-pack interaction: if a definition ends in `[args<T>] values`, positional arguments fill the fixed
    parameters first and the remaining positional arguments bind to `values`; named arguments do not target the
    `args<T>` parameter directly. A spread argument (`values...` surface, `[spread] values` canonical) is only legal in
    call-argument position and expands into that trailing variadic slot when the callee actually has one, and forwarded
    `args<T>` values now participate in omitted-pack inference. The body-side read-only API (`count(values)`,
    `values.count()`, `values[index]`, `at(values, index)`, `values.at(index)`, `values.at_unsafe(index)`) is now
    available; the legacy C++ emitter executes that API for concrete packs, while IR-backed VM/native now covers the
    direct numeric/bool/string pack slice plus pure and mixed-prefix pack forwarding from known-size sources,
    struct-pack materialization plus indexed field/helper access across direct/pure/mixed forwarding paths, `Result<T,
    Error>` packs with indexed `Result.why(...)` / `?` behavior across the same forwarding modes, status-only
    `Result<Error>` packs with indexed `Result.error(...)` / `Result.why(...)` behavior across those same forwarding
    modes, `FileError` packs with indexed downstream `why()` mapping, `Reference<FileError>` packs with indexed
    downstream `dereference(...).why()` mapping, and `Pointer<FileError>` packs with indexed downstream
    `dereference(...).why()` mapping. `File<Mode>` packs with indexed downstream file-handle method access,
    `Reference<File<Mode>>` packs with indexed downstream file-handle method access alongside explicit
    `dereference(...).write*` / `flush()` access plus helper-style `at(values, i).write*()` / `values.at(i).flush()`
    receivers, canonical free-builtin `at([values] values, [index] i).write*()` / `.flush()` receivers, and direct
    indexed `readByte(...)` `?` inference plus canonical free-builtin `at([values] values, [index] i).readByte(...)`
    `?`, `Pointer<File<Mode>>` packs with indexed downstream file-handle method access alongside explicit
    `dereference(...).write*` / `flush()` access plus helper-style `at(values, i).write*()` / `values.at(i).flush()`
    receivers, canonical free-builtin `at([values] values, [index] i).write*()` / `.flush()` receivers, and direct
    indexed `readByte(...)` `?` inference plus canonical free-builtin `at([values] values, [index] i).readByte(...)`
    `?`, `Buffer<T>` packs with indexed downstream `buffer_load(...)` and `buffer_store(...)` on the IR/VM GPU path,
    `Reference<Buffer<T>>` packs with indexed downstream `buffer_load(dereference(...), ...)` and
    `buffer_store(dereference(...), ...)` on that same IR/VM GPU path, `Pointer<Buffer<T>>` packs with indexed
    downstream `buffer_load(dereference(...), ...)` and `buffer_store(dereference(...), ...)` on that same IR/VM GPU
    path, `array<T>`, `Reference<array<T>>`, `Pointer<array<T>>`, `vector<T>`, `Reference<vector<T>>`,
    `Pointer<vector<T>>`, empty/header-only `soa_vector<T>`, `Reference<soa_vector<T>>`, `Pointer<soa_vector<T>>`,
    `map<K, V>`, `Reference<map<K, V>>`, plus `Pointer<map<K, V>>` packs with indexed downstream `count()` resolution
    across direct/pure/mixed forwarding, `vector<T>` packs with indexed downstream `capacity()` resolution,
    borrowed/pointer array and vector packs with explicit indexed `dereference(...)` receiver wrappers for downstream
    checked/unchecked access, borrowed/pointer vector packs with that same indexed `capacity()` surface through explicit
    `dereference(...)` receiver wrappers, borrowed/pointer map packs with that same count and lookup surface through
    explicit indexed `dereference(...)` receiver wrappers, those same value, borrowed, and pointer map packs with
    indexed downstream `tryAt(...)` payload-kind inference for `auto` bindings, and `Pointer<map<K, V>>` packs with
    indexed downstream `contains()` / `at()` / `at_unsafe()` lookup access. Scalar `Pointer<T>` plus scalar
    `Reference<T>` packs with indexed downstream `dereference(...)`, and struct `Pointer<T>` plus struct `Reference<T>`
    packs with indexed downstream field/helper access. Newly discovered unsupported non-string pack gaps should be
    tracked as concrete TODOs only when backed by a reproducible semantics, lowering, or backend failure.
    Latest checkpoint: canonical free-builtin `at([values] values, [index] i)` on wrapped borrowed/pointer `File<Mode>`
    arg-packs now preserves `write*()` / `flush()` receivers plus `readByte(...)` `?` inference across direct calls
    plus pure/mixed spread forwarding, while wrapped `FileError` free-builtin named access remains on the existing
    named-argument rejection path.
- **Collections:** `array<Type>{ ... }` / `array<Type>[ ... ]`, `vector<Type>{ ... }` / `vector<Type>[ ... ]`,
  `map<Key,Value>{ ... }` / `map<Key,Value>[ ... ]` are brace-backed collection construction surfaces. The
  `collections` text transform normalizes bracket aliases to braces and map `key=value` pairs to alternating key/value
  entries; it no longer emits call-shaped collection construction text. Legacy call-shaped helpers such as
  `array<Type>(...)`, `vector<Type>(...)`, and `map<Key,Value>(key1, value1, key2, value2, ...)` are compatibility
  helper forms, not the preferred construction syntax. Map literals supply alternating key/value forms.
  - Requires the `collections` text transform (enabled by default in `--text-transforms`).
  - Map literal entries are read left-to-right as alternating key/value forms; an odd number of entries is a diagnostic.
  - String keys are allowed in map literals (e.g., `map<string, i32>{"a"utf8=1i32}`), and nested forms inside braces are
    rewritten as usual.
  - Collections can appear anywhere forms are allowed, including execution arguments.
  - Numeric/bool array literals (`array<i32>{...}`, `array<i64>{...}`, `array<u64>{...}`, `array<bool>{...}`) lower
    through IR/VM/native.
  - `array<T>` is a fixed-size contiguous value sequence once constructed (C++ `std::array`-like behavior). Arrays
    support read/write/index helpers but no growth helpers.
  - PrimeStruct keeps `array<T>` as a runtime-count contract; envelope-level length forms like `array<T, N>` are
    intentionally unsupported.
  - Array helpers: `value.count()`, `value.at(index)`, `value[index]`, `value.at_unsafe(index)` (canonical equivalents:
    `count(value)`, `at(value, index)`, `at_unsafe(value, index)`).
  - Ownership direction: keep `array<T>` as language-core substrate, move the
    public constructor/helper behavior of `vector<T>` and `map<K, V>` into
    stdlib `.prime` definitions, and treat promoted `soa_vector<T>` as the
    current stdlib-owned SoA collection surface over generic SoA substrate.
  - `vector<T>` is a C++-style resizable contiguous owning sequence. `vector<T>{...}` is the user-facing variadic
    constructor form (0..N); `vector<T>(...)` remains a legacy compatibility helper path. Growth
    operations require `effects(heap_alloc)` (or the active default effects set), and
    `push`/`reserve` may reallocate and invalidate references/pointers into vector storage.
  - Planned stdlib-owned constructor surface: the temporary experimental vector namespace now uses `vector(values...)`
    over trailing `[args<T>] values`, and the remaining migration work is to move the canonical imported surface off
    fixed-arity helper wrappers onto that same variadic shape.
  - Vector helpers: `value.count()`, `value.at(index)`, `value[index]`, `value.at_unsafe(index)`, `value.push(item)`,
    `value.pop()`, `value.reserve(capacity)`, `value.capacity()`, `value.clear()`, `value.remove_at(index)`,
    `value.remove_swap(index)` (canonical helper equivalents remain `count(value)`, `at(value, index)`, `push(value,
    item)`, etc.). Import `/std/collections/*` before using bare helper names or method-sugar examples.
    Exact `import /std/collections/vector` is part of the verified runnable surface for bare
    legacy `vector(...)` construction compatibility plus canonical `/std/collections/vector/*` helper forms. Concise
    binding initializers such as `[vector<T>] values{1, 2, 3}` are now also part of the verified
    wildcard-import surface for readable vector examples and loop/index code, while explicit
    `vector<T>{...}` construction remains available when examples want to emphasize constructor
    spelling.
  - Map helpers: `count(value)`, `contains(value, key)`, `value.at(key)`, `value[key]`, `value.at_unsafe(key)` plus
    stdlib wrapper imports such as `mapCount`, `mapContains`, `mapTryAt`, `mapAt`, and `mapAtUnsafe`. `mapTryAt` routes
    misses to `Result<ContainerError>` / `Result<T, ContainerError>` instead of the checked missing-key abort path and,
    on IR-backed backends, currently supports the same `i32`/`bool`/`f32`/`string` plus single-slot int-backed stdlib
    error-struct value subset as `Result.ok(value)`.
  - Planned stdlib-owned map constructor surface: the temporary experimental map namespace now uses `map(entries...)`
    where each item is an `Entry<K, V>`/`entry(key, value)` pair, and the remaining migration work is to move the
    canonical imported constructor surface off the fixed-arity wrapper helpers onto that same entry-based variadic
    shape. The corresponding literal rewrite target is therefore planned to become `map(entry(k1, v1), entry(k2, v2),
    ...)` for the user-defined `.prime` implementation.
  - Current discard contract: builtin `pop` and `clear` are only defined for drop-trivial element types while
    container-owned destruction is still being specified. Drop-trivial currently includes scalar primitives, `string`,
    `Pointer<T>`, `Reference<T>`, arrays of drop-trivial elements, and concrete structs that do not define `Destroy*`
    helpers and whose fields are also drop-trivial. Builtin `vector`/`map`/`soa_vector` elements and structs with
    `Destroy` hooks are rejected for builtin `pop`/`clear`.
  - Current indexed-removal contract: builtin `vector` `remove_swap` and builtin `vector` `remove_at` now support
    ownership-sensitive and relocation-sensitive element types on the capacity-guarded heap-backed builtin runtime
    path. The lowerer has a reusable lifecycle-aware destroy-at-pointer helper, a shared vector destroy-at-slot
    primitive, and shared survivor-motion helpers, so both builtin indexed-removal helpers now destroy the removed slot
    and move survivors without depending on relocation-trivial raw copies. Canonical `/std/collections/vector/*`
    indexed-removal helpers on explicit experimental `Vector<T>` bindings remain exempt from builtin-runtime constraints
    because they route onto the experimental `.prime` implementation instead. There is no corresponding builtin
    `soa_vector` indexed-removal surface yet.
  - Remaining live ownership/runtime migration work is now the builtin canonical `map<K, V>` growth
    surface follow-up beyond the current owning local numeric-map path. The lowerer now has a shared
    non-local grown-pointer write-back/repoint primitive for wrapper targets, and direct canonical
    `/std/collections/map/insert(...)` plus method-sugar `receiver.insert(...)` calls now also route
    map-returning helper receivers (including nested field-access helper returns) onto that same
    rewrite path. Direct canonical field-access non-local receivers (for example `holder.values`) now
    also route through that path when explicit helper template arguments or inferred canonical
    callee receiver typing provide map typing.
    Method-sugar field-access receivers now also route through that path by inferring receiver map
    typing from resolved canonical method-callee receiver parameter types when method template
    arguments are omitted.
    Compatibility helper alias `insert` surfaces (for example `mapInsert`) now share that same
    field-access receiver-typing inference when template arguments are omitted, so alias-resolved
    non-local receiver forms route through `insert_builtin` instead of falling back to pending
    runtime paths.
    Location-wrapped field-access and helper-return receivers (for example
    `location(holder.values)` and `location(makeValues())`) now also route through that path by
    peeling `location(...)` wrappers before canonical/helper-alias receiver-typing and helper-return
    collection inference when template arguments are omitted.
    Location-wrapped local-map receivers (for example `location(valuesLocal)`) now also route
    through that path by peeling `location(...)` wrappers before local map-kind probing so omitted
    template arguments can reuse local map key/value metadata.
    Dereference-wrapped helper-return receivers (for example
    `dereference(makeValuesRef())`) now also route through that path by inferring map typing from
    resolved helper return collection declarations when template arguments are omitted.
    Dereference-wrapped non-local field-access receivers (for example
    `dereference(holder.valuesRef)`) now also share that same canonical/helper-alias callee
    receiver-typing inference when template arguments are omitted, so those non-local borrowed
    receiver forms route through `insert_builtin` as well.
    Nested location+dereference receiver chains (for example
    `location(dereference(location(makeValuesRef())))` and
    `location(dereference(location(holder.valuesRef)))`) now route through that same path by
    peeling stacked wrappers before helper-return and field-access receiver typing inference when
    template arguments are omitted.
    Temporary helper-return value receivers that do not provide a stable writable lvalue target now
    have deterministic compile-time rejects in conformance coverage (direct canonical and method-sugar
    forms). The remaining public
    builtin borrowed/non-local mutation surfaces still need to route onto that path instead of the
    current pending runtime boundary.
  - Current relocation contract: builtin `push` and `reserve` are only defined for relocation-trivial element types
    while container move/reallocation semantics are still being specified. Relocation-trivial currently includes scalar
    primitives, `string`, `Pointer<T>`, `Reference<T>`, arrays of relocation-trivial elements, and concrete structs that
    do not define `Destroy*` or `Copy`/`Move` helpers and whose fields are also relocation-trivial. Builtin
    `vector`/`map`/`soa_vector` elements and structs with custom move/destroy hooks are rejected for builtin
    `push`/`reserve`.
  - Vector binding forms:
    - `[vector<T> mut] v{vector<T>{}}` and `[mut] v{vector<T>{}}` are both valid.
    - `[vector<T> mut] v{}` is shorthand for zero-arg construction and rewrites to `[vector<T> mut] v{vector<T>{}}`.
  - `soa_vector<T>` is an explicit structure-of-arrays container (SoA). It is separate from `vector<T>`; the compiler
    must not silently rewrite AoS (`vector`) to SoA (`soa_vector`).
  - Intended usage: data-oriented loops and ECS-style component storage where field-wise contiguous iteration is
    preferred.
  - Implementation model: the public `soa_vector` API lives in stdlib `.prime`
    files, with compiler/runtime code reduced to generic SoA substrate only
    (field-layout/codegen/introspection, column storage, field-view
    borrowing/invalidation, and allocation primitives). The end-state is that
    C++ source does not contain `soa_vector`-named collection semantics.
  - Public `soa_vector<T>` surface:
    - Construction/growth mirrors vector (`soa_vector<T>{}`, `push`, `reserve`, `count`) and allocation still requires
      `effects(heap_alloc)`.
    - Indexing/access is explicit and SoA-aware (`value.field()[i]`, `value.get(i)`, and optionally `value.ref(i)` proxy
      access).
    - Reallocation invalidates SoA field views/proxies the same way vector growth invalidates pointers/references.
  - Ownership/invalidation rules for ECS-style usage:
    - Treat `get(...)` as value-style element access.
    - Treat `ref(...)` and future field views as borrowed storage views that are invalid after structural mutation.
    - Structural mutations (`push`, `reserve`) and explicit AoS/SoA conversions (`to_soa`, `to_aos`) are mutation
      boundaries; any previously acquired SoA views/proxies are invalid after these operations.
    - Explicit lifecycle destroy calls (`Destroy`, `DestroyStack`, `DestroyHeap`, `DestroyBuffer`) on SoA owners are
      also mutation boundaries; any previously acquired SoA views/proxies are invalid after these operations.
    - Implicit owner drops at scope exit also invalidate outstanding SoA views/proxies; keeping a live `ref(...)`
      borrow past the end of the owner scope is rejected.
    - Preferred update pattern is two-phase: run a stable-size update pass first, then apply deferred structural
      changes.
    - Example (canonical surface): `while(less_than(i, particles.count())) { particles.get(i) ... }`
      followed by `particles.reserve(plus(particles.count(), spawnQueue.count()))`.
  - SoA eligibility (v1 draft): `T` must be a struct with SoA-safe fields (fixed-size,
    non-pointer/reference/string/template envelopes unless explicitly allowed by backend policy).
  - AoS/SoA conversions are explicit only (`to_soa(vector<T>)`, `to_aos(soa_vector<T>)`); no implicit interop.
  - Canonical example source lives at `examples/3.Surface/soa_vector_ecs.prime` and imports
    `/std/collections/soa_vector/*` plus `/std/collections/soa_vector_conversions/*`.
  - **Current implementation status:** `soa_vector<T>` surface parsing is recognized, and semantic validation now
    accepts `soa_vector` bindings/literals/returns when type constraints hold (`soa_vector` struct element requirement,
    template-arity checks, and deterministic element-field envelope diagnostics such as `soa_vector field envelope is
    unsupported on /Type/field/...: ...` for disallowed direct or nested fields). Builtin `count`/`get`/`ref` validation
    and current lowering behavior remain compatibility scaffolding around the
    stdlib-owned public wrapper surface. The canonical
    `/std/collections/soa_vector/*` wrapper owns public
    constructor/read/ref/mutator helper names and
    forwards through the internal `/std/collections/internal_soa_vector/*`
    adapter instead of directly importing the experimental compatibility
    module, while `/std/collections/soa_vector_conversions/*` owns the public
    conversion helper names and forwards through the internal
    `/std/collections/internal_soa_vector_conversions/*` adapter. Today,
    explicit AoS/SoA conversion helpers validate in both call and method form
    (`to_soa(vector<T>)`, `to_aos(soa_vector<T>)`, `vector<T>.to_soa()`, `soa_vector<T>.to_aos()`), method-form/call-form field-view names now route through the shared
    `/soa_vector/field_view/<field>` helper path onto `soaVectorFieldView<Struct, Field>` (or a same-path user helper
    when visible), returning `SoaFieldView` values that can be bound with a tracked borrow root instead of stopping on a
    pending diagnostic; direct call-argument and return escapes remain deterministic `field-view escapes ...`
    diagnostics. `count(...)` on `soa_vector` lowers through the native count path for current SoA bindings, empty
    `soa_vector<T>{}` literals lower to header-only storage, and builtin `ref(...)` now rejects direct and helper-return
    local binding persistence plus direct and helper-return call-argument/return escapes with
    `unknown method: /std/collections/soa_vector/ref`
    until the borrowed-view substrate exists, including old-explicit
    `/soa_vector/ref(...)` call-argument escapes.
    Non-empty SoA literals still emit the deterministic unsupported diagnostic `native
    backend does not support non-empty soa_vector literals`. Root builtin `push(...)`,
    `reserve(...)`, `values.push(...)`, `values.reserve(...)`, and old explicit
    `/soa_vector/push|reserve(...)` forms now route through the canonical stdlib helper
    surface instead of stopping on dedicated backend-specific `soa_vector helper`
    diagnostics, and imported raw-builtin bare/method `count/get/ref/push/reserve`
    forms now also clear semantics on that canonical surface while imported method
    `get(...).field` / `ref(...).field` now resolves the element struct during
    semantics. Helper-return builtin bare/method `count/get/ref` reads on global and
    explicit `/Type/helper` receivers now also clear semantics on that same canonical
    surface instead of degrading to `does not accept template arguments`, and
    helper-return bare/method `push/reserve` local binding plus call-argument and
    direct-return escapes on those same receivers now preserve same-path
    `/soa_vector/push|reserve` helpers instead of degrading to builtin
    statement-only semantics or synthetic template-argument errors. Vector-target
    wrong-receiver bare/method `count/get/ref` calls, plus old-explicit method
    `count/get/ref`,
    now also preserve visible
    same-path `/soa_vector/count|get|ref` user helpers instead of being pinned to the builtin
    `soa_vector` target-mismatch path.
    Root/imported builtin bare/direct `to_aos` forms on raw `soa_vector<T>`
    bindings now also clear the old builtin-versus-wrapper lowering mismatch through the
    canonical stdlib shim, and imported plus no-import root builtin bare/direct/method/slash-method
    `to_aos` forms now also materialize the canonical `/std/collections/soa_vector/to_aos__...`
    helper path and run through that same bridged substrate on native instead of trapping. These compiler-owned
    `soa_vector` paths are compatibility scaffolding around the canonical public
    surface. The inline-parameter and direct lowerer wrapper-dispatch bridges
    now require canonical wrapper paths.
  - **Compile-time schema substrate status:** the minimum field-schema introspection needed for a `.prime`
    `soa_vector<T>` implementation already exists through compile-time reflection metadata queries:
    `meta.field_count<T>()`, `meta.field_name<T>(i)`, `meta.field_type<T>(i)`, and
    `meta.field_visibility<T>(i)`. Those queries validate only on reflect-enabled structs and are eliminated before IR
    emission, so future SoA stdlib code can derive column schemas from `T` without adding new compiler-owned
    collection-specific reflection primitives.
  - **Current canonical SoA public surface:** public docs/examples should use
    `/std/collections/soa_vector/*` with `SoaVector<T>`, `soaVectorNew<T>()`,
    `soaVectorSingle<T>()`, `soaVectorFromAos<T>()`, `soaVectorCount<T>()`, `soaVectorGet<T>()`,
    `soaVectorReserve<T>()`, and `soaVectorPush<T>()`, plus wrapper method sugar for `.count()`,
    `.get(i)`, `.reserve(...)`, and `.push(...)`. The explicit AoS conversion surface now lives in
    the canonical `/std/collections/soa_vector_conversions/*` module with `soaVectorToAos<T>()`,
    canonical `SoaVector<T>` / `Reference<SoaVector<T>>` receiver spellings.
    `/std/collections/internal_soa_vector/*` is the internal adapter behind
    construction/read/ref/mutator and field-view wrappers,
    `/std/collections/internal_soa_vector_conversions/*` is the internal
    adapter behind canonical AoS conversion helpers,
    `/std/collections/experimental_soa_vector/*` is accepted only as a targeted
    compatibility/conformance seam behind that canonical public surface,
    and `/std/collections/experimental_soa_vector_conversions/*` is accepted
    only as a targeted direct-import conversion compatibility seam. Ordinary
    public code should not import either experimental SoA namespace for
    construction, read/ref, mutator, field-view, or conversion flows.
    The wrapper now stores real `.prime` `SoaColumn<T>` state rather than the old builtin header-only
    `soa_vector<T>` backing, and it currently requires `T` to be a reflect-enabled struct via
    `meta.field_count<T>()` so non-SoA-safe element types fail early. Today the first real single-column
    wrapper read/mutate path runs end-to-end across C++/native/VM: `soaVectorSingle<T>()` constructs a non-empty
    wrapper, `soaVectorGet<T>()` plus wrapper method-sugar `values.get(i)` read elements back from that
    column-backed state, and `soaVectorReserve<T>()` /
    `soaVectorPush<T>()` plus wrapper method-sugar `values.reserve(...)` / `values.push(...)` mutate that same
    wrapper-backed column state in place. Experimental wrapper field-view attempts such as `values.x()` now route
    through `soaVectorFieldView<Struct, Field>` and return `SoaFieldView<Field>` values across direct locals, borrowed
    locals, helper-return receivers, method-like helper-return receivers, and inline `location(...)`-wrapped receivers.
    Mutating standalone method/call attempts such as `assign(values.x(), next)` and `assign(x(values), next)` now
    lower through `soaFieldViewRef<T>(..., 0)` dereference writes on that same helper path,
    while indexed and explicit borrowed-slot writes such as `assign(values.y()[i], next)`,
    `assign(dereference(pickBorrowed(...)).y()[i], next)`,
    `assign(location(pickBorrowed(...)).y()[i], next)`,
    `assign(y(location(pickBorrowed(...)))[i], next)`,
    `assign(holder.pickBorrowed(...).y()[i], next)`,
    `assign(y(holder.pickBorrowed(...)))[i]`,
    `assign(location(holder.pickBorrowed(...)).y()[i], next)`,
    `assign(y(dereference(location(holder.pickBorrowed(...))))[i], next)`,
    `assign(ref(values, i).y, next)`, and `assign(values.ref(i).y, next)` now clear
    semantics/runtime through the existing `soaVectorRef<T>(..., i).field` substrate, and borrowed
    `Reference<SoaVector<T>>` read-only method sugar `borrowed.get(i)`, `borrowed.ref(i)`, and
    `borrowed.to_aos()` plus bare helper forms `count(...)`, `get(...)`, `ref(...)`, and
    `to_aos(...)` now also ride on the existing helper/conversion substrate for local,
    parameter, helper-return, inline `location(...)`, inline
    `dereference(location(...))`, inline location-wrapped borrowed helper-return, method-like
    struct-helper return, and inline location-wrapped method-like struct-helper return receivers
    instead of leaking through raw builtin target-mismatch diagnostics or the old helper-return
    conversion mismatch. Read-only field-view indexing now rides on that same helper
    substrate for reflected structs: both method-form `values.x()[i]` / `values.y()[i]` and
    call-form `x(values)[i]` / `y(values)[i]` reads plus
    borrowed local forms such as `borrowed.y()[i]`, `dereference(borrowed).y()[i]`,
    `location(values).y()[i]`, and `y(dereference(location(values)))[i]`,
    borrowed helper-return forms such as `pickBorrowed(...).y()[i]`,
    method-like struct-helper return forms such as
    `holder.pickBorrowed(...).y()[i]` and `y(holder.pickBorrowed(...))[i]`,
    inline location-wrapped borrowed helper-return forms such as
    `location(pickBorrowed(...)).y()[i]` and `y(location(pickBorrowed(...)))[i]`,
    inline location-wrapped method-like struct-helper return forms such as
    `location(holder.pickBorrowed(...)).y()[i]` and `y(location(holder.pickBorrowed(...)))[i]`,
    and explicitly dereferenced borrowed helper-return forms such as
    `dereference(pickBorrowed(...)).y()[i]` rewrite to the existing
    `soaVectorGet<T>(..., i).field` path, and direct bare helper-field reads such as
    `ref(values, i).y`, `get(values, i).y`, method-form helper-field reads such as
    `values.ref(i).y` / `values.get(i).y`, bound method-like helper-return forms such as
    `[i32] field{get(holder.pickBorrowed(...), i).y}` and
    `[i32] field{holder.pickBorrowed(...).ref(i).y}`, and bound inline location-wrapped
    method-like helper-return forms such as
    `[i32] field{get(location(holder.pickBorrowed(...)), i).y}` and
    `[i32] field{location(holder.pickBorrowed(...)).ref(i).y}` now ride that same generic
    struct-field access flow. Direct return/root-expression borrowed helper-return reads such as
    `return(pickBorrowed(...).count())`, `return(pickBorrowed(...).y()[i])`,
    `return(get(pickBorrowed(...), i).y)`, `return(pickBorrowed(...).get(i).y)`,
    `return(pickBorrowed(...).ref(i).y)`, direct return/root-expression method-like borrowed
    helper-return reads such as `return(holder.pickBorrowed(...).count())`,
    `return(holder.pickBorrowed(...).y()[i])`, `return(get(holder.pickBorrowed(...), i).y)`,
    `return(holder.pickBorrowed(...).get(i).y)`, `return(holder.pickBorrowed(...).ref(i).y)`,
    and inline `location(...)`-wrapped variants now ride that same helper/field-access flow too,
    so those read-only surfaces run across C++/native/VM for both single-field and multi-field
    wrappers while the remaining pending field-view surfaces are now narrowed to standalone
    borrowed reads, with borrowed-field-view lifetime/provenance rules now enforced.
    `soaVectorFromAos<T>()`
    already targets the same substrate semantically, but backend lowering still stops on the current `* backend
    requires typed bindings` boundary.
    `soaVectorToAos<T>()` now lives in the canonical `/std/collections/soa_vector_conversions/*` import
    surface so the core wrapper module stays usable for read/mutate paths. Wrapper method-sugar `values.to_aos()`
    now resolves onto that canonical helper surface cleanly while the compatibility namespace remains implementation-only. Shared lowering now treats declared
    `vector<Struct>` helper returns as opaque array-handle returns, so imported `soaVectorToAos<T>()`
    and wrapper method-sugar `values.to_aos()` both lower successfully across C++/native/VM for
    empty and non-empty wrapper state instead of stopping on the old `* backend does not support
    return type on /std/collections/soa_vector_conversions/soaVectorToAos__...`
    boundary.
    The first canonical bridges also exist now: explicit `/std/collections/soa_vector/count<T>(...)`,
    `/std/collections/soa_vector/get<T>(...)`, `/std/collections/soa_vector/ref<T>(...)`,
    `/std/collections/soa_vector/reserve<T>(...)`, and `/std/collections/soa_vector/push<T>(...)`
direct calls now stay on the canonical stdlib shim surface through `ast-semantic` and then run
end-to-end on the experimental wrapper path; wrong-receiver direct-call misuse now preserves the
same canonical `/std/collections/soa_vector/*` unknown-target diagnostic instead of degrading
into bare `/get` / `/ref`-style fallback names. Explicit canonical slash-method `get` attempts like
`values./std/collections/soa_vector/get(...)` still keep that same canonical unknown-method
contract, while `values./std/collections/soa_vector/to_aos()` now validates on the canonical
helper path and reaches the same current lowerer `struct parameter type mismatch` boundary as the
other routed `to_aos` forms instead of degrading to an unknown experimental-wrapper method path.
Wildcard-imported canonical
helper names for `count`, `get`, `ref`, `reserve`, `push`, and `to_aos` now keep mixed
`/std/collections/*` plus `/std/collections/soa_vector/*` imports on the receiver-matched
canonical shim surface instead of pinning every bare `count(...)` call to the SoA alias. Bare
wildcard-imported helper names now validate and lower successfully across C++/native/VM,
including helper-return `count(...)` and `get(...).field` receiver flows that previously
leaked through unresolved root helper names, and
explicit canonical direct-call `to_aos(...)` now runs end-to-end through that same stdlib shim
path as well once template arguments are present. Direct-canonical plus imported-helper `to_aos`
lowering also no longer depends on dedicated IR/emitter/backend/runtime conversion branches,
and valid root bare/direct/
method `to_aos` calls on builtin `soa_vector<T>` bindings now rewrite onto that same canonical
`/std/collections/soa_vector/to_aos` helper path when no visible user `/to_aos` helper shadows
them. Imported root bare/direct/method `to_aos` forms on builtin `soa_vector<T>` bindings now
also clear semantics on that same canonical helper path instead of being rewritten onto the
experimental wrapper conversion helper directly. Valid root bare/method/old-explicit `count`,
`get`, and `ref` calls on builtin
`soa_vector<T>` bindings likewise now rewrite onto `/std/collections/soa_vector/count`,
`/std/collections/soa_vector/get`, and `/std/collections/soa_vector/ref` when no visible root
or same-path user helper shadows them.
Valid root bare/method/old-explicit `push` and `reserve` calls on builtin `soa_vector<T>`
bindings now likewise rewrite onto `/std/collections/soa_vector/push` and
`/std/collections/soa_vector/reserve` when no visible root or same-path user helper shadows
them. Old-explicit builtin mutator spellings now also share the same
remaining lowerer `push` / `reserve requires mutable vector binding` contract on builtin
`soa_vector<T>` receivers instead of falling through to the generic unsupported-expression-call
path.
Vector-target root bare/method/old-explicit `get` and `ref` misuses now also keep those same
canonical `/std/collections/soa_vector/get` and `/std/collections/soa_vector/ref` reject
contracts instead of the old builtin `get requires soa_vector target` / `ref requires
soa_vector target` diagnostics.
Visible vector-target same-path `/soa_vector/push` / `/soa_vector/reserve` helper shadows now
also preserve plain method forms, and old-explicit slash-method forms rewrite to direct
`/soa_vector/push(values, ...)` / `/soa_vector/reserve(values, ...)` calls instead of degrading
to `unknown call target: /std/collections/soa_vector/push`.
Vector-target root bare/direct/method `to_aos` misuses now also preserve visible same-path
`/to_aos` user helpers through semantics and dump rewriting, and numeric builtin-vector
helper-shadow cases now also clear lowering and run across C++/native/VM instead of stopping
after rewrite. The no-shadow path still keeps that same canonical
`/std/collections/soa_vector/to_aos` reject contract instead of the old builtin
`to_aos requires soa_vector target` diagnostic, while struct-vector literal/runtime limits remain
tracked separately from that helper-routing boundary.
Direct lowerer wrapper dispatch no longer uses rooted `/soa_vector/*`,
`/to_aos`, or `/to_aos_ref` paths as hidden wrapper-family fallback probes; add
a separate concrete SoA cleanup TODO before changing behavior outside that
scope. Dedicated inline-dispatch builtin
`soa_vector` count/get/ref
helper bridging is now gone as well, so those helper shapes flow through the shared
definition-resolution plus count/access fallback path instead of a one-off SoA branch. Root
builtin `push` and `reserve` forms now rewrite earlier onto the canonical helper surface as
well, so the old backend-specific lowerer `soa_vector helper: push|reserve` rejection path is
gone, and old-explicit builtin mutator spellings still share the narrower remaining
`push|reserve requires mutable vector binding` contract. The lowerer count/access classifiers no longer treat raw `/soa_vector/count` as a
builtin count alias once semantics has canonicalized those old-surface forms. Imported raw-builtin
method `get/ref` forms now also reach the same canonical wrapper-mismatch lowerer
boundary as their bare imported counterparts, so the remaining helper-call cleanup is no longer
blocked on wildcard-imported access-method parity. The remaining inferred and
method-target fallback resolution for builtin `soa_vector` count receivers now also prefers the
canonical `/std/collections/soa_vector/count` helper path while still preserving same-path
`/soa_vector/count` user-helper shadowing on helper-return receivers, and bare helper-return
`count(...)` local binding, call-argument, and direct return escapes now also preserve that
same-path helper shadow instead of degrading to a synthetic template-argument error. Helper-return bare
wildcard-imported helper names now also canonicalize from the receiver family before the
experimental wrapper rewrite so `count(holder.cloneValues())` and
`get(holder.cloneValues(), i).field` stay on the same canonical helper surface. Helper-return
expression-position bare and method `push(...)` / `reserve(...)` now also preserve same-path
`/soa_vector/push` and `/soa_vector/reserve` user-helper shadowing instead of degrading to the
old builtin statement-only mutator contract, and helper-return experimental-wrapper method
`count/get/ref/push/reserve` now also rewrites to visible same-path `/soa_vector/*` helpers
before validation/lowering on both global helper-return and explicit `/Type/helper`
method-like struct-helper return receivers instead of leaking through wrapper methods in
compile-run paths. Nested struct-body helper returns that materialize wrapper values through
`return(soaVectorSingle<Particle>(Particle{...}))` now also clear that same direct/bound
helper/conversion substrate plus same-path `/soa_vector/*` and `/to_aos` helper-shadow method
surfaces instead of failing during specialization or later expression lowering. The equivalent
helper-return method/infer fallback for builtin `soa_vector` `get` receivers now prefers the
canonical `/std/collections/soa_vector/get` helper path while still preserving same-path
`/soa_vector/get` user-helper shadowing, and bare helper-return `get(...)` local binding,
call-argument, and direct return escapes now also preserve that same-path helper shadow instead
of degrading to a synthetic template-argument error. The equivalent helper-return method/infer fallback for
builtin `soa_vector` `ref` receivers now also prefers the canonical
`/std/collections/soa_vector/ref` helper path while still preserving same-path
`/soa_vector/ref` user-helper shadowing. Local `auto` inference on builtin and helper-return
`ref(...)` method sugar now also respects that same-path helper return type instead of collapsing
to the builtin element type, bare helper-return `ref(...)` local binding, call-argument, and
direct return escapes now also preserve that same-path helper shadow instead of degrading to a
synthetic template-argument error, and the builtin borrowed-view recognizers in
initializer/monomorph handling now accept that canonical resolved helper path too. The
equivalent helper-return method/infer fallback for builtin `soa_vector` `to_aos` receivers now
also prefers the canonical `/std/collections/soa_vector/to_aos` helper path, and vector-result
recognizers now accept that canonical resolved helper path too. Same-path `/to_aos`
user-helper shadowing now also survives helper-return builtin `to_aos` bare, direct-call,
method, and infer fallback instead of canonicalizing early onto the canonical helper path.
Experimental wrapper helper-return `to_aos()` method
rewrites now also stop short of forcing `soaVectorToAos(...)` when a same-path `/to_aos` helper
is visible, and that helper-preserving rewrite now also covers explicit `/Type/helper`
method-like wrapper receivers instead of leaving `holder.cloneValues().to_aos()` on the raw
wrapper-method path. Lowerer-side count target classification now also
treats direct canonical `/std/collections/soa_vector/to_aos` call results as vector targets for
nested count dispatch instead of depending on the old raw `/to_aos` call shape. C++ emitter
vector-target classification now also treats those same direct canonical
`/std/collections/soa_vector/to_aos` call results as vector targets for nested helper dispatch
instead of depending on bound vector temporaries, and the shared native/VM vector-capacity
classifier now does the same for nested backend helper dispatch. Imported root bare/direct
builtin `to_aos` forms on raw `soa_vector<T>` bindings therefore now reach the same canonical
semantic rewrite and lower past the old builtin-versus-wrapper parameter mismatch instead of
failing earlier against the experimental wrapper helper, and imported plus no-import root
bare/direct/method/slash-method `to_aos` now materialize
`/std/collections/soa_vector/to_aos__...` before lowering and now run on native through that same
bridged shim path.
Imported raw-builtin bare/method canonical `count/get/ref/push/reserve` forms now also clear
semantics on that same canonical helper surface, imported method `get(...).field` /
`ref(...).field` now resolves the element struct before lowering, and imported method
`push/reserve` now also reaches the same canonical wrapper-mismatch boundary instead of the
older mutable-vector-binding gap. Those raw-builtin imported helper forms still stop later on
the remaining lowering bridge from builtin `/soa_vector` values to experimental wrapper
`SoaVector<T>` parameters instead of on the older imported method `get/ref` unknown-target gap.
Conversion cleanup itself is now complete on that bridged bare/direct/method/slash-method path. Lowerer,
backend, runtime-code, and diagnostic/test helper-call/conversion/successful-field-view cleanup are also complete,
so borrowed-view lifetime/provenance behavior is now enforced, with layer-local routing cleanup complete
removal. Runtime-code helper-call cleanup is now complete because no production
runtime-side helper routing remains outside the semantics/lowering/emitter/backend layers, and
diagnostic/test helper-call cleanup is now complete as well because direct canonical helper
coverage plus helper-return bare `get(...)` dump coverage now lock those routing paths in the
test harnesses. Diagnostic/test conversion cleanup is now complete too because compile-run,
IR-lowerer, and semantic-dump coverage lock both direct-canonical and imported-helper wrapper
`to_aos` forms to the canonical `/std/collections/soa_vector/to_aos` path, and imported plus
no-import root raw-builtin bare/direct/method/slash-method `to_aos` now also lock to the canonical
`/std/collections/soa_vector/to_aos__...` helper path through lowering plus C++/VM execution.
Runtime-code helper/conversion/field-view cleanup and the matching diagnostic/test special-case cleanup are therefore
complete; borrowed-view lifetime/provenance behavior is now enforced, with no runtime-side routing removal left. The current
successful field-view indexing surface is already locked by compile-run, dump, and source-level coverage rather than
backend-local routing contracts, and representative wildcard canonical
helper/conversion flows now run across C++ emitter, VM, and native without
direct experimental imports in the source. The wrapper now also exposes `soaVectorRef<T>()`
plus `values.ref(i)` on top of the current single-column borrowed-slot substrate, and those
borrowed-return paths now run successfully across the current backends without depending on the
conversion helper surface. The pending `soa_vector` field-view and borrowed-view diagnostics now also route through shared
semantics helper functions instead of duplicated string assembly across validator,
mutation-borrow, and monomorph entrypoints, and the validator-side plus monomorph-side fallback
probes are gone entirely. The remaining compiler-owned
`/soa_vector/field_view/...` helper path now also routes through shared semantics helper
construction/parsing instead of open-coded literals across validator and monomorph fallback logic,
and the old resolved-helper `soa_vector field view requires soa_vector target` fallback is gone
too,
while direct monomorph field-view fallback rejects now also reuse the shared
helper-path unavailable-method diagnostic instead of building the pending string inline,
and the shared direct pending reporter now routes both field-view and `ref(...)`
rejects through those same helper-path/unavailable-method diagnostics,
with helper-return local-binding `ref(...)` fallback now reusing that same direct
pending reporter instead of a local string-rewrite branch,
and monomorph-side builtin `ref(...)` fallback now also reusing the shared
unavailable-method helper instead of returning the borrowed-view pending string directly,
while the shared direct pending reporter now likewise uses that same optional
unavailable-method helper for both field-view and `ref(...)` rejects,
and infer pre-dispatch plus late-fallback unavailable-method sites now also use
that same shared helper instead of the older field-view-only wrapper,
which is now gone entirely,
while validator-side fixed unavailable-method rejects now also route through one
shared visibility-aware helper instead of recomputing the `/soa_vector/ref`
visibility and pending/unavailable split inline,
with the validator-side direct builtin `ref(...)` visibility split now also
using that same shared helper-target predicate instead of a hardcoded
`/soa_vector/ref` probe,
while validator-side visible same-path `/soa_vector/count|get|push|reserve`
helper checks plus definition-return and collection-return same-path
`/soa_vector/<field>` helper visibility now also route through that same
shared definition-visibility helper instead of repeating import/declared
probes or local same-path wrappers across count/capacity builtin
validation, SoA builtin validation, method-target, infer-time
helper-shadow, vector-helper routing, vector-helper same-path
`/soa_vector/push|reserve` mutator-shadow checks, preferred same-path
versus canonical target selection in method-target, infer-time, and
vector-helper mutator routing, return-inference paths, and builtin SoA
access/count helper fallback
validation,
with the remaining vector-target and builtin count/get/ref same-path
probes now also using shared target helpers instead of direct
`/soa_vector/count|get|ref` checks,
while the remaining preferred same-path-versus-canonical SoA target chooser now
also uses that shared helper directly instead of a generic visible-path
wrapper,
while the remaining validator-side fixed unavailable-method rejects now call
`soaUnavailableMethodDiagnostic(...)` directly with that same shared `ref`
helper-target predicate instead of open-coded preferred-target comparisons,
while collection-return visible same-path `/soa_vector/get|ref` helper
detection now uses one shared helper-name path instead of split `get` / `ref`
branches, while preferred
same-path-versus-canonical `soa_vector`
helper target selection in method-target, infer-time, vector-helper
mutator routing, plus builtin `get/ref` call-shape detection in direct
validation and collection-return inference, plus direct same-path
`/soa_vector/<helper>` visibility checks in count/builtin/method/vector
routing, direct field-view helper detection, and return-inference
helper-shadow probes now all route through shared validator helpers,
while monomorph-side visible `/soa_vector/ref` fallback detection now also
uses that same shared definition-visibility helper directly instead of a
dedicated ref-specific wrapper or local visibility cache,
while monomorph-side old-surface `/soa_vector/ref` and `/soa_vector/<field>`
visibility now also uses direct `sourceDefs` / `helperOverloads` checks
inside monomorph implicit-template fallback instead of a local helper-target
probe or a monomorph-local visible-path wrapper,
while monomorph-side fixed method pending rejects now call the shared pending
helper directly instead of a monomorph-local wrapper,
while direct builtin field-view/ref pending reporters now also route through
one shared expr-to-path helper and the validator-local direct pending
reporter wrapper plus the thin `isBuiltinSoaRefExpr(...)` wrapper are gone,
while monomorph-side visible same-path `/soa_vector/<field>` helper checks now
also route through that same shared definition-visibility helper instead of
probing `sourceDefs` and `helperOverloads` inline,
while monomorph-side fixed method pending rejects now also route through one
shared visibility-aware helper instead of open-coding the optional pending
lookup around that same visibility probe,
and the low-level field-view and borrowed-view pending string builders are now
file-local to the shared helper implementation instead of part of the public
semantics-helper surface,
while direct collection-access, count/capacity builtin validation, SoA builtin
validation, collection dispatch inference, direct access helper routing, and
method-target/infer borrowed experimental receiver handling now also share one
borrowed experimental receiver probe end-to-end instead of keeping duplicated
inline lambdas, wrapper lambdas, direct receiver adapters, or
local borrowed-target wrappers,
while direct SoA builtin validation now also calls that shared borrowed
receiver probe directly instead of a local direct-receiver adapter,
and the post-`validateExpr(...)` binding/return/call-argument plus return-inference reprobes are
gone too. The current successful read-only `value.field()[i]` path likewise no longer depends on
lowerer/emitter/backend-local `field_view` or `soaVectorGet|soaVectorRef` routing branches, since
both direct wrapper reads and borrowed local shorthand plus explicitly dereferenced borrowed local
reads now run through the generic helper-call plus struct-field path end-to-end.
The remaining pending-diagnostic cleanup for builtin SoA field views is now complete:
standalone assign-target method/call writes like `assign(values.x(), next)` /
`assign(x(values), next)` now lower through `soaVectorFieldView(...)` plus
`soaFieldViewRef<T>(..., 0)` dereference writes for the same receiver families
as other wrapper helpers, while assign-target indexed field-view writes like
`assign(y(values)[i], next)` and explicit borrowed-slot writes like
`assign(ref(values, i).field, next)` already lower through the experimental
`soaVectorRef<T>(..., i).field` substrate instead of staying on the old
pending-only branch.
Standalone builtin field-view call forms now route through the shared synthetic
`/soa_vector/field_view/<field>` or same-path `/soa_vector/<field>` method-target
path instead of a dedicated `SemanticsValidatorExprMapSoaBuiltins.cpp` fallback,
and resolved helper-form field-view rejects now reuse the shared unavailable-method
helper path instead of an inline pending-diagnostic branch there.
The borrowed-view lifetime/provenance rules are now enforced on top of
the experimental substrate rather than more compiler-owned pending plumbing or another indexed-write
bridge. The broader experimental wrapper/helper surface through imported
`to_aos` helper and method routing is now in place across C++/native/VM for both empty and
non-empty wrapper state, and stale post-semantics bare `to_aos(...)` target classification is
now gone from the shared emitter/lowerer path as well. The remaining backend-side
conversion-result detection no longer lives in emitter/native/VM-local classifier branches
either, instead using one shared AST call-path helper for canonical
`/std/collections/soa_vector/to_aos...` matching. The remaining conversion-specific
compiler-owned code is therefore narrowed to that shared helper plus invalid-target/user-shadow
`to_aos` fallbacks rather than old root-builtin direct-call routing outside the stdlib helper
path. The remaining raw-builtin conversion-specific compiler-owned code is now reduced to the
shared canonical `to_aos` matcher plus invalid-target/user-shadow fallbacks rather than any native
runtime trap on already-canonicalized bare/direct/method/slash-method calls.
That single-column borrowed-slot substrate is the current completed foothold, and it now
includes direct whole-value `ref(...)` values across direct wrapper locals, borrowed locals,
explicit `dereference(...)` receivers, borrowed helper-return/method-like helper-return
receivers, and inline `location(...)`-wrapped borrowed receivers in addition to the
indexed/field-level borrowed projection surface (`ref(...).field`, `.ref(i).field`, and
`value.field()[i]`-style reads/writes). Those projection forms are still recomputed per use
through the existing `soaVectorGet(...).field` / `soaVectorRef(...).field` rewrite and
lowering path, so later invalidation work still begins at standalone borrowed values and
standalone borrowed field-view values rather than the already-completed per-use projection
surface.
The remaining borrowed-view work therefore starts after the now-completed
slot-backed borrowed-value carrier exposure through the stdlib-owned `soaColumnBorrowSlot<T>(...)` /
`vectorBorrowSlot<T>(...)` helpers. Those internal helpers now validate through
`[return<Reference<T>>]` with slot-pointer provenance preserved through local `slot` aliases, and public
`soaColumnRef<T>(...)` now also preserves that standalone borrowed carrier instead of collapsing
back to whole-element `T`, experimental helper `soaVectorRef<T>(...)` forwards that same carrier
for direct helper-call use, and the shared canonical/helper-method route for `ref(values, i)` /
`values.ref(i)` now validates through the same `Reference<T>` carrier instead of dereferencing it
back to whole-element `T`. Projected `.ref(i).field` reads/writes
already ride the existing per-use `soaVectorRef(...).field` rewrite and stay with the later
standalone borrowed field-view queue rather than this whole-value helper step.
Successful experimental
`value.field()[i]` indexing now has its first completed read-only reflected slices on top of
the current substrate for direct wrapper receivers, borrowed local shorthand, inline
`location(...)` borrow expressions, explicitly dereferenced borrowed local receivers, borrowed
helper-return receivers, and explicitly
dereferenced borrowed helper-return receivers, with both method-form `value.field()[i]` and
call-form `field(value)[i]` read-only syntax now working on those receivers, while those same borrowed helper-return
receivers now also share the completed read-only method surface for `get`, `ref`, and
`to_aos` with the other borrowed wrapper receivers. The remaining
field-view work now includes enforced borrowed-view lifetime/provenance rules rather than backend cleanup for that
read-only path.
  - **Borrowed-view invalidation contract:** the next language-level substrate slice should treat
    standalone `ref(...)` values and future standalone field-view borrows as ephemeral views over
    wrapper-owned SoA column storage rather than as independently owning proxies. The intended
    contract is: `get(...)` remains value-style and never carries invalidation state; standalone
    `ref(...)` values and future standalone borrowed field-view values remain valid only while the
    underlying wrapper stays at a stable layout and stable element count; any structural
    mutation (`push`, `reserve`, `remove_at`, `remove_swap`, `clear`, direct AoS/SoA conversion
    that reallocates, or wrapper destruction) invalidates all outstanding borrowed
    SoA views derived from that wrapper, including borrows reached through
    `location(...)`, helper-return receivers, and method-like helper-return receivers. The
    implementation target is explicit validation and runtime/provenance rules for that
    invalidation boundary, not another compiler-owned pending-diagnostic special case. The
    current completed foothold is indexed borrowed-slot read/write projections
    (`ref(...).field`, `.ref(i).field`, and `value.field()[i]`-style reads/writes), but those
    projections are recomputed per use through the helper rewrite path and do not yet
    materialize a standalone borrowed object that needs its own persisted invalidation state.
    The standalone `ref(...)` receiver families above are now in place, those
    whole-value carriers now also survive local binding, helper pass-through, and direct
    helper return surfaces, and live carriers now already reject later `push` /
    `reserve` / `remove_at` / `remove_swap` / `clear` / `to_aos` mutations on that same wrapper across direct local,
    direct borrowed/dereferenced receiver, helper-return, pass-through, and
    return-rooted surfaces. Standalone borrowed field-view values do not exist
    yet, but once they do they inherit this same invalidation contract from the
    richer field-view design note below. The remaining implementation work
    therefore starts by materializing richer standalone field-view values on
    those same receiver families, then layering later shrink/motion invalidation,
    storage-replacement/destruction invalidation, and provenance/escape rules on top.
    Standalone `ref(...)` bindings now also participate in borrow-conflict checks even
    when the receiver is location-wrapped or helper-returned, so those receiver
    families no longer bypass the existing borrow-conflict rules.
    Mutable standalone `ref(...)` bindings now also require a mutable root binding,
    matching the existing mutable-borrow expectations for other reference paths.
    Standalone `ref(...)` bindings now also require a resolvable borrow root, so
    location-wrapped and helper-return receiver flows no longer drop borrow
    provenance when binding a reference.
    Standalone `ref(...)` call-argument escapes derived from location-wrapped or
    helper-return receivers are now rejected, matching the same non-escaping
    borrow intent as other parameter-rooted reference rules.
    Standalone field-view call-argument escapes derived from location-wrapped or
    helper-return receivers are now rejected as well, keeping borrowed field views
    from outliving the owner they were derived from.
    Standalone `ref(...)` and field-view return escapes derived from location-wrapped
    or helper-return receivers are also rejected, so direct return expressions no
    longer bypass the same non-escaping borrow rules.
    Standalone field-view bindings derived from location-wrapped or helper-return
    receivers are now rejected as well, keeping borrowed field views from escaping
    through local bindings.
  - **Richer borrowed field-view contract:** locally bound field-view expressions are now
    first-class non-owning column views instead of compiler-owned pending diagnostics on
    the canonical wrapper path. Direct borrowed locals, explicit `dereference(...)`
    receivers, borrowed helper-return receivers, method-like struct-helper-return receivers,
    and inline `location(...)`-wrapped borrowed receivers all share the column-view surface
    used by the successful `value.field()[i]` rewrite when the result is captured in a
    binding. Those bound field-view values remain borrowed projections over wrapper-owned
    SoA storage, inherit the same invalidation rules as `ref(...)`, and report borrowed
    binding diagnostics when a live projection overlaps a later structural mutation. Direct
    pass/return escapes remain rejected until that provenance contract is promoted. The
    remaining implementation work is to thread the broader pass/return contract through the
    existing experimental wrapper helper/indexing substrate, not to add more builtin-only
    fallback diagnostics. Today those standalone borrowed field-view attempts already all
    funnel through the same synthetic `/soa_vector/field_view/<field>` helper path across
    direct borrowed locals, explicit dereference, borrowed helper returns, method-like
    helper returns, and inline `location(...)`-wrapped receivers. The implementation
    now has the reusable non-owning `SoaColumn<T>` borrowed-view helper substrate plus reflected layout facts for
    reflect-enabled structs. `SoaColumn<T>` still stores contiguous whole `T` elements,
    and the current `SoaSchema*` reflection helpers now expose validated field byte
    offsets and whole-element stride through `SoaSchemaFieldOffset(...)` /
    `SoaSchemaElementStride()`. Existing buffer helpers now expose byte-addressable
    reinterpretation and offsetting, and the stdlib builds
    `soaColumnFieldSlotUnsafe<Struct, Field>(...)` plus the strided `SoaFieldView<T>`
    carrier on top of those reflected layout facts. Field-view binding
    initializers now route through the shared `/soa_vector/field_view/<field>`
    helper path onto `soaVectorFieldView<...>` and return a non-owning strided
    view. Bound field-view values now preserve borrowed semantics by resolving
    their receiver roots through direct locals, `location(...)`/`dereference(...)`,
    and helper-return receivers; later structural mutation on that live root reports the
    same borrowed-binding invalidation diagnostic used by `ref(...)` carriers.
    Direct call-argument and return escapes remain rejected until the pass/return
    provenance contract is promoted.
  - **Standalone mutating field-view contract:** standalone method-form and call-form
    `assign(value.field(), next)` / `assign(field(value), next)` writes now lower through
    `soaVectorFieldView(...)` plus `soaFieldViewRef<T>(..., 0)` dereference writes for the
    same receiver families as the rest of the wrapper surface (direct locals, borrowed
    locals, borrowed helper-return receivers, method-like struct-helper-return receivers, and
    inline `location(...)`-wrapped variants). That keeps standalone field-view writes on the
    existing writable column-view substrate instead of mutating temporary values or
    reintroducing builtin-only mutation branches. The remaining invalidation work therefore
    starts at later whole-value `ref(...)` shrink/motion, storage-replacement/destruction,
    and provenance/escape rules once the next standalone borrowed field-view lifetime rules
    land.
  - **Experimental SoA storage substrate:** the completed fixed-width reusable `.prime` storage layer now exists at
    `/std/collections/internal_soa_storage/*` with single-column `SoaColumn<T>` helpers
    (`soaColumnNew<T>()`, `soaColumnCount<T>()`, `soaColumnCapacity<T>()`, `soaColumnReserve<T>()`,
    `soaColumnPush<T>()`, `soaColumnRead<T>()`, `soaColumnWrite<T>()`, `soaColumnClear<T>()`) plus
    reusable fixed-width multi-column primitives `SoaColumns2<T0, T1>`, `SoaColumns3<T0, T1, T2>`,
    `SoaColumns4<T0, T1, T2, T3>`, `SoaColumns5<T0, T1, T2, T3, T4>`,
    `SoaColumns6<T0, T1, T2, T3, T4, T5>`,
    `SoaColumns7<T0, T1, T2, T3, T4, T5, T6>`,
    `SoaColumns8<T0, T1, T2, T3, T4, T5, T6, T7>`, and
    `SoaColumns9<T0, T1, T2, T3, T4, T5, T6, T7, T8>`, and
    `SoaColumns10<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>`,
    `SoaColumns11<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>`,
    `SoaColumns12<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11>`, and
    `SoaColumns13<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12>`, and
    `SoaColumns14<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13>`,
    `SoaColumns15<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14>`, and
    `SoaColumns16<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15>` with matching `New` /
    `Count` / `Capacity` / `Reserve` / `Push` / `Read*` / `Write` / `Clear` helpers. These
    primitives are backed by checked buffer alloc/grow/free plus explicit `init(...)`, `drop(...)`,
    `take(...)`, and `borrow(...)` flows. Their current allocation-failure contract is deterministic
    and matches the existing vector reserve overflow path: oversized reserve requests report
    `vector reserve allocation failed (out of memory)`. Direct borrowed-view coverage for the
    current single-column substrate is now locked through `soaColumnRef<T>(...)`, and that public
    helper now preserves a standalone borrowed carrier, but the primitives are not yet threaded
    into richer wrapper field-view surfaces. Reflected structs can now expose
    generated `SoaSchema*` helpers plus chunked `SoaSchemaStorage` allocation/grow/clear/destroy helpers, so
    arbitrary-width reflected storage no longer stops at the old pending-width runtime gate.
  - **Current implementation status:** VM/native vector locals use a heap-backed `count/capacity/data_ptr` record
    layout. `push` and dynamic `reserve` growth allocate/reallocate backing storage and report deterministic runtime
    allocation failures (`vector push allocation failed (out of memory)` / `vector reserve allocation failed (out of
    memory)`) once the local dynamic-capacity limit (`256`) is exceeded. `vector<T>{...}` construction and legacy
    `vector<T>(...)` compatibility construction above `256` elements and out-of-range/negative folded `reserve`
    integer literal expressions are rejected at lowering time
    (`vector literal exceeds local capacity limit (256)` / `vector reserve exceeds local capacity limit (256)` /
    `vector reserve expects non-negative capacity` / `vector reserve literal expression overflow`). Folded literal
    support currently covers `plus`/`minus`/`negate` expression trees, and both signed and unsigned fold-overflow paths
    emit `vector reserve literal expression overflow`.
  - Mutation helpers (`push`, `pop`, `reserve`, `clear`, `remove_at`, `remove_swap`) are statement-only.
  - Current builtin map key contract: `K` must resolve to the builtin `Comparable` subset (`i32`, `i64`, `u64`, `f32`,
    `f64`, `bool`, or `string`). Generic `map<K, V>` helpers may defer that check until monomorphisation, but concrete
    user-defined `Comparable` structs are not accepted by the builtin map runtime yet.
  - Numeric/bool map literals (`map<i32, i32>{...}`, `map<u64, bool>{...}`) also lower through IR/VM/native
    (construction, `count`, `at`, and `at_unsafe`).
  - String-keyed map literals lower through VM/native when keys are string literals or bindings backed by literals;
    other string key expressions require the C++ emitter (which uses `std::string_view` keys).
- **Conversions:** no implicit coercions. Use explicit executions (`convert<f32>(value)` or `bool{value}`) or custom
  transforms. The builtin `convert<T>(value)` is the default cast helper and supports `i32/i64/u64/bool/f32/f64` in the
  minimal native subset (integer width conversions currently lower as no-ops in the VM/native backends, while the C++
  emitter uses `static_cast`; `convert<bool>` compares against zero, so any non-zero value—including negative
  integers—yields `true`). Float ↔ integer conversions lower to dedicated PSIR opcodes in VM/native, and converting
  NaN/Inf to an integer is a runtime error (stderr + exit code `3`).
  - **Convert constructor resolution (v1):**
    - Builtin fast-path: when `T` is `bool/i32/i64/u64/f32/f64` and `value` is numeric/bool, use the builtin conversion
      rules; user-defined conversions do not override this.
    - Otherwise, resolve `convert<T>(value)` as a call to `T.Convert(value)` in the struct method namespace
      (`/T/Convert`).
    - Signature must match exactly after monomorphisation: `[static return<T>] Convert([U] value)` (Convert helpers are
      static, no implicit `this`). No implicit conversions are applied to match the parameter type.
    - If no match exists, emit a `no conversion found` diagnostic. If multiple matches exist, emit an `ambiguous
      conversion` diagnostic with the candidate list.
    - Visibility follows import rules: only `[public]` `Convert` helpers are visible across imports.
    - `convert<T>(value)` does not fall back to `T(value)` or `T{...}`.
- **Float note:** VM/native lowering supports float literals, bindings, arithmetic, comparisons, numeric conversions,
  and `/std/math/*` helpers.
- **String note:** VM/native lowering supports string literals and string bindings in `print*`, plus `count`/indexing
  (`at`/`at_unsafe`) on string literals and string bindings that originate from literals; other string operations still
  require the C++ emitter for now.
  - **Struct note:** VM/native lowering supports struct values when fields are numeric/bool or other struct values
    (nested structs). Struct fields with strings or templated envelopes still require the C++ emitter.
  - `bool{...}` is valid for integer operands (including `u64`) and treats any non-zero value as `true`.
- **Mutability:** values immutable by default; include `mut` in the stack-value execution to opt-in (`[f32 mut]
  value{...}`).
- **Open design:** raw string semantics across backends.

## Pointers & References
- **Explicit envelopes:** `Pointer<T>`, `Reference<T>` mirror C++ semantics; no implicit conversions.
- **Qualifiers:** `restrict<T>` is allowed on bindings and parameters only; it must match the binding type (including
  template args) and acts as an explicit type constraint. There is no `readonly` qualifier yet; use `mut` to opt into
  mutation.
- **Target whitelist:** `Pointer<T>` targets always support primitive and struct pointees. Additional specialized
  pointer targets are accepted only where the language/runtime has explicit handling (for example the supported
  `array<T>`, `vector<T>`, header-only `soa_vector<T>`, `map<K, V>`, `File<Mode>`, `Result<T, Error>`, and `FileError`
  surfaces described elsewhere in this spec). `Reference<T>` targets always support primitive and struct pointees, and
  selected explicit wrapper cases such as `array<T>`, `vector<T>`, header-only `soa_vector<T>`, `map<K, V>`,
  `File<Mode>`, `Result<T, Error>`, and `FileError` where the runtime has dedicated handling. Heap intrinsics still
  reject `alloc<array<T>>` even though location-backed `Pointer<array<T>>` bindings are accepted.
- **Backend limits:** VM/native lowering rejects `Pointer<string>` / `Reference<string>` (string pointers are not
  supported). Entry-argument arrays are not addressable (`location(args)` is invalid).
- **Surface syntax:** canonical syntax uses explicit calls (`location`, `dereference`, `plus`/`minus`); the `operators`
  text transform rewrites `&name`/`*name` sugar into those calls.
- **Reference binding:** in safe scopes, `Reference<T>` bindings are initialized from `location(...)`. In `[unsafe]`
  scopes they may also be initialized from pointer-like expressions (`Pointer<T>`/`Reference<T>` values and pointer
  arithmetic results) when the target type matches. References behave like `*Pointer<T>` in use. Use `mut` on the
  reference binding to allow `assign(ref, value)`.
- **Array pointers:** `Pointer<array<T>>` is allowed for location-backed locals/parameters and participates in the same
  read-only array receiver API (`count`/`at`/indexing) that value and reference arrays use.
- **Array references:** `Reference<array<T>>` is allowed; treat the reference like an array value for `count`/`at` and
  other array operations while still using `location(...)` to form the reference.
- **Core pointer calls:** `location(value)` yields a pointer to a local binding or parameter (entry argument arrays are
  not addressable); `location(ref)` returns the pointer stored by a `Reference<T>` binding; `dereference(ptr)` reads
  through a pointer/reference form; `assign(dereference(ptr), value)` writes through the pointer. Pointer writes require
  the pointer binding to be declared `mut`; attempting to assign through an immutable pointer or reference is rejected.
- **Heap intrinsics (semantic contract):** `/std/intrinsics/memory/alloc<T>(count)` returns `Pointer<T>`,
  `/std/intrinsics/memory/realloc(ptr, count)` returns the same `Pointer<T>` target type as `ptr`, and
  `/std/intrinsics/memory/free(ptr)` releases a pointer allocation. These names are recognized only in qualified
  `/std/intrinsics/memory/*` form. All three require `effects(heap_alloc)` (or the active default effects set), reject
  named/block arguments, and use element counts rather than raw byte sizes. `alloc` requires exactly one template
  argument plus one integer count argument; `realloc` requires one pointer argument plus one integer count argument;
  `free` requires one pointer argument and takes no template arguments. Current implementation status: semantic
  validation recognizes and validates all three calls; VM/native/IR-to-C++ lowering now lowers `alloc` onto the shared
  `HeapAlloc` runtime path (including struct element-count expansion by slot width), `free` now lowers onto `HeapFree`,
  `realloc` now lowers onto `HeapRealloc` on VM/native/IR-to-C++ backends, VM/IR-to-C++ runtimes reject dereferences
  into freed heap ranges deterministically, and `realloc` preserves slot payloads across successful growth/shrink
  reallocation while treating counts as element counts rather than raw bytes.
- **Checked pointer element access:** `/std/intrinsics/memory/at(ptr, index, count)` returns the same `Pointer<T>`
  target type as `ptr` after checked element-wise pointer arithmetic. It is recognized only in qualified
  `/std/intrinsics/memory/*` form, takes no template arguments, rejects named/block arguments, requires `ptr` to be a
  `Pointer<T>`, and currently requires `index` and `count` to use the same integer kind (`i32`, `i64`, or `u64`). Signed
  negative `index`/`count` values and `index >= count` fail deterministically with `pointer index out of bounds`.
  Lowering scales the element offset by the pointee slot width, so struct pointers step by full struct slot counts
  rather than raw scalar slots.
- **Unchecked pointer element access:** `/std/intrinsics/memory/at_unsafe(ptr, index)` returns the same `Pointer<T>`
  target type as `ptr` after unchecked element-wise pointer arithmetic. It is recognized only in qualified
  `/std/intrinsics/memory/*` form, takes no template arguments, rejects named/block arguments, requires `ptr` to be a
  `Pointer<T>`, and requires `index` to be an integer (`i32`, `i64`, or `u64`). Lowering scales the element offset by
  the pointee slot width just like checked `at(...)`, but performs no bounds check; this is the intended primitive for
  relocation/growth code paths in future stdlib-owned containers.
- **Internal pointer-helper shims:** `/std/collections/internal_buffer_checked/*` and
  `/std/collections/internal_buffer_unchecked/*` are explicit internal `.prime` helper namespaces used for
  container-conformance work. They wrap the qualified memory intrinsics into small alloc/grow/free plus
  checked/unchecked offset, read, and write helpers so future stdlib `vector`/`map` implementations can be proven
  through import-driven VM/native/C++ tests without presenting those helpers as candidate public collection APIs.
  - When any `/std...` import is present, the stdlib now also provides canonical `.prime` wrappers at
    `/std/collections/vector/*` over the current stdlib `vectorNew` / `vectorCount` / `vectorPush` helper surface. That
    imported path is now the sole public namespaced vector contract; the experimental vector namespace remains a
    backing seam rather than a peer public API. Canonical
    `/std/collections/vector/vector(...)`, `/std/collections/vector/count(...)`,
    `/std/collections/vector/capacity(...)`, `/std/collections/vector/at(...)`,
    `/std/collections/vector/at_unsafe(...)`, `/std/collections/vector/push(...)`, `/std/collections/vector/pop(...)`,
    `/std/collections/vector/reserve(...)`, `/std/collections/vector/clear(...)`,
    `/std/collections/vector/remove_at(...)`, and `/std/collections/vector/remove_swap(...)` calls now require either
    that imported stdlib path or an explicit source definition, and explicit canonical method forms such as
    `values./std/collections/vector/push(...)`, `values./std/collections/vector/pop()`,
    `values./std/collections/vector/reserve(...)`, `values./std/collections/vector/clear()`,
    `values./std/collections/vector/remove_at(...)`, and `values./std/collections/vector/remove_swap(...)` now reject
    with same-path `unknown method` diagnostics when no imported or declared canonical helper exists. The same
    explicit-helper rule now also covers bare builtin-vector `count()` method sugar: local `vector<T>` bindings such as
    `values.count()` reject with same-path `unknown method: /vector/count` unless an imported canonical helper or
    explicit `/vector/count` definition exists. Local non-vector explicit canonical slash-method `count()` and
    `capacity()` also now stay on that same-path rule: bindings such as `mapValues./std/collections/vector/count()`,
    `items./std/collections/vector/capacity()`, and `text./std/collections/vector/capacity()` reject with same-path
    `unknown method: /std/collections/vector/count` or `/std/collections/vector/capacity` unless a same-path helper
    exists. Wrapper-returned non-vector collection receivers now follow that same explicit-helper rule on both direct
    and slash-method `count`/`capacity` edges: `wrapMap()./std/collections/vector/capacity()`,
    `wrapMap()./vector/capacity()`, and `wrapMap()./vector/count()` reject with same-path `unknown method` diagnostics
    unless a same-path helper is declared, while direct canonical `/std/collections/vector/count(wrapMap())`,
    `/std/collections/vector/count(wrapText())`, and `/std/collections/vector/count(wrapArray())` calls also reject with
    same-path `unknown call target` diagnostics unless a same-path helper exists. Those canonical namespaced helper
    declarations now use experimental `Vector<T>` receivers directly, imported wrapper helpers
    `vectorCount|vectorCapacity|vectorAt*|vectorPush|vectorPop|vectorReserve|vectorClear|vectorRemove*` now likewise
    accept explicit experimental `Vector<T>` receivers by forwarding through that canonical vector wrapper path while
    keeping builtin `vector<T>` behavior, imported wrapper constructor aliases
    `vectorNew|vectorSingle|vectorPair|vectorTriple|...` now also rewrite onto the experimental `.prime` constructor
    path when explicit `Vector<T>` bindings, explicit `return<Vector<T>>` definitions, explicit `Vector<T>`
    parameters/fields, or explicit assignment/init targets require that value type, now also infer experimental
    `Vector<T>` results for `[auto]` locals and `return<auto>` definitions, and now also rewrite nested direct
    helper-call plus method-call receiver expressions built from those aliases before canonical helper or method
    resolution. Imported constructor/count/capacity/access wrappers plus statement-position mutators still follow
    ordinary definition argument rules such as named-argument support.
  - When any `/std...` import is present, the stdlib also provides canonical `.prime` wrappers at
    `/std/collections/map/*` over the current stdlib `mapNew` / `mapCount` / `mapTryAt` helper surface. That imported
    path is now the sole public namespaced map contract; the experimental map namespace remains a backing seam rather
    than a peer public API. Imported
    `/std/collections/map/map(...)`, `/std/collections/map/count(...)`, `/std/collections/map/contains(...)`,
    `/std/collections/map/tryAt(...)`, `/std/collections/map/at(...)`, and `/std/collections/map/at_unsafe(...)`
    wrappers follow ordinary definition argument rules such as named-argument support. Canonical
    `/std/collections/map/map(...)` legacy constructor-shaped builder calls, `/std/collections/map/count(...)` count calls,
    `/std/collections/map/contains(...)` contains calls, `/std/collections/map/tryAt(...)` calls, and canonical
    `/std/collections/map/at(...)` plus `/std/collections/map/at_unsafe(...)` access calls now require those imported
    wrappers or an explicit source definition instead of falling back to builtin `/map/map`, `/map/count`,
    `/map/contains`, `/map/tryAt`, or `/map/at*` handling. Explicit removed compatibility `/map/count(...)`,
    `/map/contains(...)`, `/map/tryAt(...)`, `/map/at(...)`, and `/map/at_unsafe(...)` spellings also no longer inherit
    canonical `/std/collections/map/*` behavior and now require explicit `/map/count`, `/map/contains`, `/map/tryAt`,
    `/map/at`, or `/map/at_unsafe` definitions. Legacy compatibility or conformance imports of `/std/collections/experimental_map/*` now extend canonical
    namespaced helper calls in three distinct ways: value `Map<K, V>` receivers can use call-form
    `/std/collections/map/count|contains|tryAt|at|at_unsafe`, which rewrites directly onto the real experimental
    `.prime` helper implementation; wrapper-layer `/std/collections/mapCount|mapContains|mapTryAt|mapAt|mapAtUnsafe`
    calls on those same value receivers now rewrite onto the corresponding experimental `.prime` helpers as well, and
    direct canonical or wrapper-layer helper receivers built from canonical `/std/collections/map/map(...)` or
    wrapper-layer `/std/collections/mapNew|mapSingle|mapDouble|mapPair|...` constructor expressions now also rewrite
    those inner constructors onto `/std/collections/experimental_map/mapNew|mapSingle|mapDouble|mapPair|...`; direct
    method-call receivers built from those same constructor expressions now do the same before method lowering; explicit
    `[Map<K, V>]` bindings, explicit `return<Map<K, V>>` definitions, direct arguments flowing into explicit `[Map<K,
    V>]` parameters plus inferred `[auto]` parameters backed by experimental-map default initializers, helper-wrapped
    argument expressions flowing into those explicit or inferred parameter targets, those same explicit or inferred
    parameter targets reached through method-call sugar, explicit or inferred experimental `Map<K, V>` struct fields
    initialized through struct-constructor arguments, direct `assign(values, ...)` writes into explicit `[Map<K, V>
    mut]` locals or parameters plus explicit or inferred experimental `Map<K, V>` struct fields, both explicit-template
    plus implicit-template legacy constructor-shaped calls flowing through `[auto]` locals and `return<auto>` definitions,
    `return<auto>` definitions whose inferred result is an experimental `Map<K, V>`, block-bodied value returns inside
    those inferred-return definitions, helper-wrapped return-path expressions inside those explicit or inferred
    experimental-map definitions, `auto` bindings nested inside those returned blocks, and direct method-call receivers
    produced by those inferred experimental-map definitions can initialize from either canonical
    `/std/collections/map/map(...)` or wrapper-layer `/std/collections/mapNew|mapSingle|mapDouble|mapPair|...`
    constructor aliases, which rewrite onto or infer
    `/std/collections/experimental_map/mapNew|mapSingle|mapDouble|mapPair|...` plus the real experimental `Map<K, V>`
    type during template monomorphization; and borrowed `Reference<Map<K, V>>` receivers use the overload-free canonical
    spellings `/std/collections/map/count_ref|contains_ref|tryAt_ref|at_ref|at_unsafe_ref`. Wrapper-layer helper names
    on builtin `map<K, V>` receivers and broader inference-driven constructor routing outside those explicit or
    first-step inferred experimental destinations still stay on `collections.prime` until the broader
    constructor/type-surface migration is ready. Bare builtin map method sugar now follows the same import-driven path:
    `values.count()`, `values.contains(...)`, and `values.tryAt(...)` require imported canonical
    `/std/collections/map/count`, `/std/collections/map/contains`, and `/std/collections/map/tryAt` wrappers or explicit
    `/map/count`, `/map/contains`, and `/map/tryAt` definitions, while `values.at(...)` plus `values.at_unsafe(...)`
    require imported canonical `/std/collections/map/at*` wrappers or explicit `/map/at*` definitions, instead of
    falling back to compiler-owned builtin method semantics. Bare root `count(values)`, `contains(values, key)`,
    `at(values, key)`, and `at_unsafe(values, key)` calls on builtin `map<K, V>` targets now follow that same helper
    path: `count(values)` requires imported canonical `/std/collections/map/count` or explicit `/count` or `/map/count`
    definitions, while `contains(values, key)` and `at*(values, key)` require imported canonical
    `/std/collections/map/contains` or `/std/collections/map/at*` helpers, or explicit `/contains` or `/map/*`
    definitions, instead of falling back to compiler-owned builtin call semantics.
- **Pointer/reference helper returns:** explicit helper return transforms such as `return<Pointer<T>>` and
  `return<Reference<T>>` lower through the address-like `i64` return path on VM/native/IR-backed backends (unless they
  wrap one of the collection envelopes handled separately), so imported `.prime` helpers can pass checked-access
  pointers between allocation, growth, and indexing helpers without backend-specific special cases.
- **Pointer arithmetic:** `plus(ptr, offset)` and `minus(ptr, offset)` treat `offset` as a byte offset. VM/native frames
  currently space locals in 16-byte slots, so adding or subtracting `16` advances one local slot. Offsets accept `i32`,
  `i64`, or `u64` in the front-end; non-integer offsets are rejected, and the native backend lowers all three widths.
  - Pointer + pointer is rejected; only pointer ± integer offsets are allowed.
  - Offsets are interpreted as unsigned byte counts at runtime; negative offsets require signed operands (e.g.,
    `-16i64`).
  - Example: `plus(location(second), -16i64)` steps back one 16-byte slot.
  - Example: `minus(location(second), 16u64)` also steps back one 16-byte slot.
  - You can use `location(ref)` inside pointer arithmetic when starting from a `Reference<T>` binding.
  - The C++ emitter performs raw pointer arithmetic on actual addresses; offsets are only well-defined within the same
    allocated object.
  - Minimal runnable example:
    ```
    [return<i32>]
    main() {
      [i32 mut] value{2i32}
      [Pointer<i32> mut] ptr{location(value)}
      assign(dereference(ptr), 4i32)
      return(value)
    }
    ```
  - Expected IR (shape only):
    ```
    PushI32 2
    StoreLocal value
    AddressOfLocal value
    StoreLocal ptr
    LoadLocal ptr
    PushI32 4
    StoreIndirect
    Pop
    LoadLocal value
    ReturnI32
    ```
- **Reference example:** `Reference<T>` behaves like a dereferenced pointer in value positions while still storing the
  address.
  - Minimal runnable example:
    ```
    [return<i32>]
    main() {
      [i32 mut] value{2i32}
      [Reference<i32> mut] ref{location(value)}
      assign(ref, 4i32)
      return(ref)
    }
    ```
  - You can recover a raw pointer from a reference via `location(ref)` when needed for pointer arithmetic or passing
    into APIs.
  - Expected IR (shape only):
    ```
    PushI32 2
    StoreLocal value
    AddressOfLocal value
    StoreLocal ref
    LoadLocal ref
    PushI32 4
    StoreIndirect
    Pop
    LoadLocal ref
    LoadIndirect
    ReturnI32
    ```
  - References can be used directly in arithmetic (e.g., `plus(ref, 2i32)`), and `location(ref)` yields the underlying
    pointer.
- **Ownership:** references are non-owning, frame-bound views. Pointer ownership tags (`raw`, `unique`, `shared`) are
  reserved for future allocator integrations.
- **Raw memory:** `memory::load/store` primitives expose byte-level access; opt-in to highlight unsafe operations.
- **Layout control:** attributes like `[packed]` guarantee interop-friendly layouts for C++/GLSL.
- **Open design:** pointer qualifier syntax, aliasing rules (restrict/readonly), and GPU backend constraints remain TBD.

## VM Design
- **Instruction set:** stack-based ops covering control flow, stack manipulation, memory/pointer access, IO, and
  explicit conversions. No implicit conversions; opcodes mirror the canonical language surface.
- **PSIR opcode set (v20, VM/native):** `PushI32`, `PushI64`, `PushF32`, `PushF64`, `PushArgc`, `LoadLocal`,
  `StoreLocal`,
  `AddressOfLocal`, `LoadIndirect`, `StoreIndirect`, `Dup`, `Pop`, `AddI32`, `SubI32`, `MulI32`, `DivI32`, `NegI32`,
  `AddI64`, `SubI64`, `MulI64`, `DivI64`, `DivU64`, `NegI64`, `AddF32`, `SubF32`, `MulF32`, `DivF32`, `NegF32`,
  `AddF64`, `SubF64`, `MulF64`, `DivF64`, `NegF64`, `CmpEqI32`, `CmpNeI32`, `CmpLtI32`, `CmpLeI32`, `CmpGtI32`,
  `CmpGeI32`, `CmpEqI64`, `CmpNeI64`, `CmpLtI64`, `CmpLeI64`, `CmpGtI64`, `CmpGeI64`, `CmpLtU64`, `CmpLeU64`,
  `CmpGtU64`, `CmpGeU64`, `CmpEqF32`, `CmpNeF32`, `CmpLtF32`, `CmpLeF32`, `CmpGtF32`, `CmpGeF32`, `CmpEqF64`,
  `CmpNeF64`, `CmpLtF64`, `CmpLeF64`, `CmpGtF64`, `CmpGeF64`, `ConvertI32ToF32`, `ConvertI32ToF64`,
  `ConvertI64ToF32`, `ConvertI64ToF64`, `ConvertU64ToF32`, `ConvertU64ToF64`, `ConvertF32ToI32`, `ConvertF32ToI64`,
  `ConvertF32ToU64`, `ConvertF64ToI32`, `ConvertF64ToI64`, `ConvertF64ToU64`, `ConvertF32ToF64`, `ConvertF64ToF32`,
  `JumpIfZero`, `Jump`, `ReturnVoid`, `ReturnI32`, `ReturnI64`, `ReturnF32`, `ReturnF64`, `PrintI32`, `PrintI64`,
  `PrintU64`, `PrintString`, `PrintArgv`, `PrintArgvUnsafe`, `LoadStringByte`, `FileOpenRead`, `FileOpenWrite`,
  `FileOpenAppend`, `FileReadByte`, `FileClose`, `FileFlush`, `FileWriteI32`, `FileWriteI64`, `FileWriteU64`,
  `FileWriteString`, `FileWriteByte`, `FileWriteNewline`, `PrintStringDynamic`, `Call`, `CallVoid`, `HeapAlloc`,
  `HeapFree`, `HeapRealloc`.
- **Call-opcode status:** `Call` and `CallVoid` are serialized/validated and execute in both VM and native backends with
  frame/call-stack semantics. Current lowering still inlines source-level definition calls and does not emit call
  opcodes yet.
- **GLSL note:** GLSL/SPIR-V emission routes through canonical IR (`glsl-ir`/`spirv-ir`) and `IrValidationTarget::Glsl`;
  these modes emit backend output directly without requiring PSIR serialization.
- **PSIR versioning:** current portable IR is PSIR v21 (adds `HeapRealloc` on top of v20’s `FileReadByte` and
  `HeapFree`, v19’s per-instruction source-map metadata keyed by debug ID, v18’s instruction debug IDs, v17’s local
  debug slots, v16’s function-call opcodes `Call`/`CallVoid`, v15’s execution metadata, v14’s float return opcodes,
  v13’s float arithmetic/compare/convert opcodes, and v12’s struct field visibility/static metadata, `LoadStringByte`,
  `PrintArgvUnsafe`, `PrintArgv`, `PushArgc`, pointer helpers, `ReturnVoid`, and print opcode upgrades).
- **Frames & stack:** VM/native execution starts at the entry frame and pushes/pops frames for `Call`/`CallVoid`; each
  frame stores locals in 16-byte slots while the operand stack stores raw `u64` values interpreted by opcode (ints,
  floats as bits, and indices). Indirect addresses are byte offsets into the active frame’s local slot space and must be
  16-byte aligned.
- **Module layout:** `IrModule` bundles functions, string table, and struct layouts; lowering emits entry instructions
  plus reachable non-entry callable function bodies so function names/metadata and executable IR survive serialization.
  VM/native execution starts from `entryIndex`; lowering currently still inlines source-level calls, so recursion
  remains rejected in lowered source programs even though callable IR call opcodes support recursive execution.
- **Strings & IO:** string values are indices into the module string table; `PrintString`/`LoadStringByte` read from it.
  File operations use OS descriptors stored as `i64` values and must be explicitly closed or they close on scope end via
  lowering.
- **Memory/GC:** there is no GC in the VM today. Arrays are inline locals with
  count metadata plus contiguous element slots. VM/native vector locals use a
  heap-backed `count/capacity/data_ptr` record; push/reserve growth reallocates
  that backing storage and preserves existing elements, while the current
  `256` local dynamic-capacity limit remains in force. `TODO-4281` tracks
  lifting that limit once the allocator/runtime contract is widened. No
  reference counting is performed.
- **Errors:** guard rails emit errors by printing to stderr and returning error codes (e.g., bounds checks), while VM
  runtime faults (stack underflow, invalid addresses) surface as `VM error:` with exit code 3.
- **Deployment target:** the VM serves as the sandboxed runtime for user-supplied scripts (e.g., on iOS) where native
  code generation is unavailable. Effect masks and capabilities enforce per-platform restrictions.

## Examples (sketch)
```
// Hello world (native/VM-friendly)
[return<i32> effects(io_out)]
main() {
  print_line("Hello, world!"utf8)
  return(0i32)
}

// Pull std::io at version 1.2.0
import<"/std/io", version="1.2.0">

import /std/math/*

[operators return<i32>] add<i32>(a, b) { return(plus(a, b)) }

[operators] execute_add<i32>(x, y)

clamp_exposure(img) {
  if(
    greater_than(img.exposure, 1.0f32),
    then() { print_line_error("exposure too high"utf8) },
    else() { }
  )
}

tweak_color([copy mut restrict<Image>] img) {
  assign(img.exposure, clamp(img.exposure, 0.0f32, 1.0f32))
  assign(img.gamma, plus(img.gamma, 0.1f32))
  apply_grade(img)
}

restrict_demo() {
  [restrict<array<i32>>] ok{array<i32>{1i32, 2i32}}
  [restrict<array<i32>>] bad{array<u64>{1u64, 2u64}} // diagnostic: expected array<i32>
}

// Canonicalization example for parameters:
// Surface: f1([i32] a [f32] b) { ... }
// Canonical: f1([restrict<i32>] a [restrict<f32>] b) { ... }

[return<i32>]
convert_demo() {
  return(i32{1.5f32})
}

[return<i32>]
labeled_args_demo() {
  return(sum3(1i32 [c] 3i32 [b] 2i32))
}

[operators return<f32>]
blend([f32] a, [f32] b) {
  [f32 mut] result{(a + b) * 0.5f32}
  if (result > 1.0f32) {
    result = 1.0f32
  } else {
  }
  return(result)
}

// Compute shader sketch (GPU backend)
[compute workgroup_size(64, 1, 1)]
/add_one(
  [Buffer<i32>] input,
  [Buffer<i32>] output,
  [i32] count
) {
  [i32] x{ /std/gpu/global_id_x() }
  if (x >= count) {
    return()
  } else {
  }
  [i32] value{ /std/gpu/buffer_load(input, x) }
  /std/gpu/buffer_store(output, x, plus(value, 1i32))
}

[effects(gpu_dispatch) return<i32>]
main() {
  [array<i32>] values{array<i32>{1i32, 2i32, 3i32, 4i32}}
  [Buffer<i32>] input{ /std/gpu/upload(values) }
  [Buffer<i32>] output{ /std/gpu/buffer<i32>(4i32) }
  /std/gpu/dispatch(/add_one, 4i32, 1i32, 1i32, input, output, 4i32)
  [array<i32>] result{ /std/gpu/readback(output) }
  return(plus(plus(result[0i32], result[1i32]), plus(result[2i32], result[3i32])))
}

// Canonical, post-transform form
[return<f32>] blend([f32] a, [f32] b) {
  [f32 mut] result{multiply(plus(a, b), 0.5f32)}
  if(
    greater_than(result, 1.0f32),
    then() { assign(result, 1.0f32) },
    else() { }
  )
  return(result)
}

// IR sketch
module {
  def /convert_demo(): i32 {
    return i32{1.5f32}
  }
  def /labeled_args_demo(): i32 {
    return sum3(1, [c] 3, [b] 2)
  }
}
```

## Integration Points
- **Build system:** extend CMake/tooling to run `primec`, track dependency graphs, and support incremental builds.
- **Testing:** unit/looped regression suites verify backend parity (C++, VM, GLSL).
- **Diagnostics:** metrics/logs land under `diagnostics/PrimeStruct/*`. Effect annotations drive error messaging.

### Diagnostics & Tooling Roadmap
- **Phase 0 (now):** standardize diagnostic records (`severity`, `code`, `message`, `notes`, source span), include
  canonical call paths, and ensure CLI output is deterministic. Establish a stable JSON diagnostic export
  (`--emit-diagnostics`) for tooling/tests.
- **Phase 1 (source maps):** attach token spans to AST, keep span provenance through text/semantic transforms, and emit
  IR-to-source maps for VM/native/GLSL. Require every diagnostic to carry at least one source span.
- **Phase 2 (incremental):** content-addressed caches for AST/IR, dependency graph tracking for imports, and
  invalidation rules per transform phase. Add `--watch` to reuse caches and stream diagnostics.
- **Phase 3 (IDE/LSP):** go-to-definition, completion, and signature help using the same symbol tables as the compiler.
  Provide diagnostics in LSP format plus an editor adapter.
- **Phase 4 (runtime):** VM/native stack traces mapped via source maps, crash reports emitted with IR/AST hashes, and
  opt-in runtime tracing for effect/capability usage.

### VM Debug Event Ordering
- `VmDebugSession` hook callbacks are emitted in one total order with a monotonically increasing `sequence` value that
  starts at `0` on each `start(...)`.
- Per instruction, hooks fire in this order: `beforeInstruction` -> zero or more in-instruction call events (`callPush`
  / `callPop`) -> `afterInstruction`.
- Faulted instructions emit `beforeInstruction` and then `fault`; they do not emit `afterInstruction`.
- For identical IR + entry arguments + control flow (`step`/`continue` decisions), the emitted hook event stream is
  deterministic and replayable.
- `primevm --debug-json` emits one JSON object per line (`version`, `event`, and event-specific fields). Hook records
  include `sequence` and `snapshot`; stop records include `reason` and a terminal snapshot.
- `primevm --debug-json-snapshots=stop|all` adds `snapshot_payload` on demand; `stop` limits payloads to `stop` events,
  while `all` includes payloads on session/hook/stop events.
- `primevm --debug-trace <path>` records deterministic VM events to a file (NDJSON) including hook `sequence` ordering,
  snapshots, and snapshot payloads; repeated runs over identical inputs must produce byte-identical trace logs.
- `primevm --debug-replay <trace>` restores a deterministic checkpoint from a captured trace and emits a single
  `replay_checkpoint` record (`target_sequence`, resolved `checkpoint_sequence`, event/reason, snapshot,
  snapshot_payload).
- `primevm --debug-replay-sequence <n>` selects the latest checkpoint with `sequence <= n` for time-travel; without an
  explicit sequence, replay uses the terminal checkpoint and mirrors traced exit codes for terminal `stop`/`Exit`
  checkpoints.

### VM IR Breakpoints
- `VmDebugSession` supports IR-level breakpoints keyed by `(functionIndex, instructionPointer)` through `addBreakpoint`,
  `removeBreakpoint`, and `clearBreakpoints`.
- `resolveSourceBreakpoints(...)` maps source locations (`line`, optional `column`) to executable IR breakpoint
  locations via canonical source-map entries; line-only requests reject ambiguous multi-column matches with explicit
  diagnostics.
- `VmDebugSession::addSourceBreakpoint(line, column, resolvedCount, ...)` resolves and installs all matching IR
  breakpoints in one call; `resolvedCount` reports how many executable locations were mapped.
- Breakpoints are checked in `continueExecution` before instruction execution and stop with reason `Breakpoint`.
- Continuing from a breakpoint resumes past that same location once (prevents immediate repeat-stops at the same
  `(functionIndex, instructionPointer)`), then normal breakpoint checks resume.

### VM Fault Stack Traces
- `VmDebugSession` fault diagnostics append a deterministic mapped stack trace when source-map metadata is available.
- Stack frames are listed from faulting frame to caller frames and include function path, instruction pointer,
  instruction debug ID, and mapped source span (`line:column`) with provenance.
- Caller-frame mapping reports call-site instructions (the `Call`/`CallVoid` slot) rather than the post-call instruction
  pointer.

### VM Debug Adapter (MVP)
- `VmDebugAdapter` (`primec/VmDebugAdapter.h`) translates VM debug-session behavior into debugger-facing primitives:
  - execution control: `launch`, `continueExecution`, `step`, `pause`
  - debugger queries: `threads`, `stackTrace`, `scopes`, `variables`
  - breakpoints: `setInstructionBreakpoints`, `setSourceBreakpoints`
- Stack-frame responses use source-map metadata when available (`line`, `column`, provenance) and keep the same
  call-site mapping behavior used by VM fault stack traces.
- Adapter calls append deterministic transcript lines through `transcript()` to support protocol transcript tests and
  replay checks.
- Snapshot payloads now carry per-frame locals (`frame_locals`) so `scopes/variables` return concrete local values for
  non-top frames as well as the active frame.
- `primevm --debug-dap` adds stdio JSON-RPC (`Content-Length`) framing and a request router for `initialize`, `launch`,
  `setBreakpoints`, `setInstructionBreakpoints`, `threads`, `stackTrace`, `scopes`, `variables`, `continue`, `next`,
  `pause`, and `disconnect`.

### Semantics Parallelism (Investigation)
We plan to parallelize semantic validation across root functions using a
deterministic diagnostics pipeline. See
`docs/Semantics_Multithread_Design.md` for the canonical phase boundaries,
ownership model, and rollout plan.

### IDE/LSP Integration Plan
Goals:
- Deliver a first-class IDE/LSP experience powered by the compiler front end.
- Keep diagnostics and symbol resolution deterministic across CLI and LSP.
- Preserve transform provenance so hovers and completions map to source.

Scope (MVP):
- `primec-lsp` process that reuses the parser, transform pipeline, and semantic resolver.
- Diagnostics wired through the JSON export (`--emit-diagnostics`) for identical output.
- Core language intelligence: hover, go-to-definition, signature help, and completion.
- Workspace symbol search driven by import dependency graphs.

Architecture:
- Maintain a long-lived compiler service with per-file token/AST caches.
- Track a dependency graph keyed by canonical paths; invalidate in topological order.
- Store per-definition symbol tables and a reverse reference index for navigation.
- Use source maps (Phase 1) to map transformed diagnostics back to surface text.

Milestones:
- M1: LSP diagnostics and document symbols (requires Phase 0).
- M2: go-to-definition/hover/signature help with stable symbol tables.
- M3: completion and workspace symbols with incremental cache invalidation (Phase 2).
- M4: rename/code actions backed by reference indexing and stable IDs.

Out of scope (initial):
- Full editor integration packaging (for example VS Code extension manifests and launch profile templates).
- GPU-specific editor features beyond diagnostics and symbol navigation.

## Dependencies & Related Work
- Stable IR definition & serialization (std::serialization once available).
- Scene graph/rendering plans (`docs/finished/Plan_SceneGraph_Renderer_Finished.md`).

## Risks & Research Tasks
- **Memory model** – value/reference semantics, handle lifetimes, POD vs GPU staging rules.
- **Resource bindings** – unify descriptors across CPU/GPU backends.
- **Debugging** – source maps, stack inspection across VM/GPU.
- **Performance** – ensure parity with handwritten code; benchmark harnesses.
- **Adoption** – migration strategy from existing shaders/scripts.
- **Diagnostics** – map compile/runtime errors back to PrimeStruct source across backends.
- **Concurrency** – finalise coroutine/await integrations.
- **Security** – sandbox policy for transforms/archives.
- **Tooling** – IDE/LSP roadmap, incremental compilation caches, metadata outputs.

## Validation Strategy
- Golden tests comparing outputs from C++, VM, GLSL for shared modules.
- Performance benchmarks recorded under `benchmarks/`.
- Static analysis/lint integrated into CI to catch undefined constructs before codegen.

## Next Steps (Exploratory)
1. Draft detailed syntax/semantics spec and circulate for review. _(Draft captured in a separate syntax spec on
   2025-11-21; review/feedback tracking TBD.)_
2. Prototype parser + IR builder (Phase 0).
3. Evaluate reuse of existing shader toolchains (glslang, SPIRV-Cross) vs bespoke emitters.
4. Design import/package system (module syntax, search paths, visibility rules, transform distribution).
5. Define library/versioning strategy so import resolution enforces compatibility.
6. Flesh out stack/class specifications (calling convention, class sugar transforms, dispatch strategy) across backends.
7. Lock down composite literal syntax across backends and add conformance tests.
8. Decide machine-code strategy (C++ emission, third-party JIT) and prototype.
9. Define diagnostics/tooling plan (source maps, error reporting pipeline, incremental tooling, editor roadmap).
10. Document staffing/time requirements before promoting PrimeStruct onto the active roadmap.
