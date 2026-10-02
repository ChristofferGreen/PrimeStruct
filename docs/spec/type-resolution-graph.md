# Planned Type-Resolution Graph

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **design direction**.

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
- Completed lowerer-side `FileError.why()` receiver slice: semantic-product-addressed
  base-kind inference now reads the receiver's published binding fact before lowerer-local
  `isFileError` metadata can classify `why()` as returning `string`. A published non-`FileError`
  binding fact suppresses stale local file-error metadata; syntax-only compatibility keeps the old
  local fallback.
- Completed lowerer-side file-handle method receiver slice: semantic-product-addressed
  base-kind inference now reads the receiver's published binding fact before lowerer-local
  `isFileHandle` metadata can classify `write`, `write_line`, `write_byte`, `read_byte`,
  `write_bytes`, `flush`, or `close` as returning `i32`. A published non-`File<Mode>`
  binding fact suppresses stale local file-handle metadata; syntax-only compatibility keeps the
  old local fallback.
- Completed lowerer-side method receiver query/local-auto slice: the same `FileError.why()`
  and file-handle method base-kind paths now read published receiver query and local-auto facts,
  not only binding facts, before indexed args-pack or named-local fallback can answer. Published
  non-file receiver facts suppress stale local metadata; no-fact syntax-only contexts keep the
  old local fallback.
- Completed lowerer-side base dereferenced FileError receiver slice: base-kind inference for
  `dereference(at(args<FileError>, i)).why()` now reads the indexed target's published query fact
  before borrowed/pointer args-pack fallback can classify the receiver. Published non-`FileError`
  target facts suppress stale args-pack metadata; syntax-only contexts keep the old fallback.
- Completed lowerer-side base dereferenced file-handle receiver slice: base-kind inference for
  `dereference(at(args<File<Mode>>, i)).flush()` and sibling file methods now reads the indexed
  target's published query fact before borrowed/pointer args-pack fallback can classify the
  receiver. Published non-`File<Mode>` target facts suppress stale args-pack metadata;
  syntax-only contexts keep the old fallback.
- Completed lowerer-side dispatch dereferenced file-handle receiver slice: dispatch inference for
  nested `try(dereference(at(args<File<Mode>>, i)).flush())` and sibling file methods now reads
  the indexed target's published query fact before borrowed/pointer args-pack fallback can
  classify the receiver. Published non-`File<Mode>` target facts suppress stale args-pack
  metadata; syntax-only contexts keep the old fallback.
- Completed lowerer-side dispatch dereferenced FileError receiver slice: dispatch inference for
  `dereference(at(args<FileError>, i)).why()` and nested `try(...)` forms now reads the indexed
  target's published query fact before the method receiver path can fall through to legacy
  dispatch fallback. Published non-`FileError` target facts suppress stale args-pack metadata.
- Completed lowerer-side direct file-call query slice: base-kind inference for direct
  `File<Mode>(...)` calls and nested `try(File<Mode>(...))` operands now reads the published query
  type before syntax-owned file-handle-call fallback can classify the call as `i64`. Published
  non-`File<Mode>` query facts suppress the legacy constructor-shaped fallback; no-fact
  syntax-only contexts keep it.
- Completed lowerer-side dispatch direct file-call query slice: dispatch inference for nested
  `try(File<Mode>(...))` operands now reads the published query type before syntax-owned
  file-handle-call fallback can classify the call as `i64`. Published non-`File<Mode>` query
  facts suppress the constructor-shaped fallback; no-fact syntax-only contexts keep it.
- Completed lowerer-side dispatch map receiver fact slice: dispatch inference for nested
  `try(map.contains(...))` and `try(map.tryAt(...))` operands now reads published receiver
  collection or binding facts before syntax, definition, or local-map fallback can classify the
  helper result. Published non-map receiver facts suppress stale local map metadata; no-fact
  syntax-only contexts keep the old fallback.
- Completed lowerer-side base map receiver fact slice: base-kind inference for nested
  `try(map.tryAt(...))` operands now reads published receiver collection or binding facts before
  local-map fallback can classify the helper result. Published non-map receiver facts suppress
  stale local map metadata; no-fact syntax-only contexts keep the old fallback.
- Completed lowerer-side dispatch count access fact slice: dispatch inference for
  `count(map.at(...))` now treats published indexed-access query facts as authoritative before
  local map metadata can classify the value as string-backed. Published non-string access facts
  suppress stale local string-map metadata; no-fact syntax-only contexts keep the old fallback.
- Completed lowerer-side dispatch count access-target fact slice: dispatch inference for
  `count(at(target, key))` now treats published map/non-map target facts as authoritative before
  stale local string-map metadata can classify target-owned access values as string-backed.
  Published non-string and non-map target facts suppress stale local metadata; no-fact
  syntax-only contexts keep the old fallback.
- Completed lowerer-side count-access string-map emission slice: native emission for
  `count(at(target, key))` now treats published indexed-access and target map/non-map facts as
  authoritative before stale local string-map metadata can choose `LoadStringLength`. Published
  non-string access or target facts suppress stale local metadata; no-fact syntax-only contexts
  keep the old fallback.
- Completed lowerer-side dereferenced count target slice: `count(dereference(target))`
  classification now reads the dereferenced target's published binding or query collection facts
  before stale reference/pointer or args-pack collection metadata can classify the count target.
  Published non-collection or missing semantic-id facts suppress stale local metadata; syntax-only
  contexts keep the old fallback.
- Completed lowerer-side indexed count target slice: `count(at(args, i))` classification now reads
  the indexed expression's published query collection fact before stale args-pack collection
  metadata can classify the count target. Published non-collection or missing semantic-id facts
  suppress stale local metadata; syntax-only contexts keep the old fallback.
- Completed lowerer-side runtime string count emission slice: `count(name)` native emission now
  reads the target's published string/non-string fact before stale runtime-string local metadata can
  choose `LoadStringLength`. Published non-string or missing semantic-id facts suppress stale local
  metadata; syntax-only contexts keep the old fallback.
- Completed lowerer-side try operand Result fact slice: base-kind inference for
  `try(operand)` now reads the operand's published binding, query, or local-auto Result fact
  before lowerer-local `isResult` and args-pack metadata can classify the value kind. Published
  non-Result operand facts suppress stale local Result metadata; no-fact syntax-only contexts
  keep the old fallback.
- Completed lowerer-side dispatch try operand Result fact slice: dispatch inference for
  `try(operand)` now reads the operand's published binding, query, or local-auto Result fact
  before lowerer-local `isResult` and args-pack metadata can classify the value kind. Published
  non-Result operand facts suppress stale local Result metadata; no-fact syntax-only contexts
  keep the old fallback.
- Completed lowerer-side base dereferenced Result operand slice: base-kind inference for
  `try(dereference(at(args<Result>, i)))` now reads the indexed target's published query Result
  fact before borrowed/pointer args-pack fallback can classify the value kind. Published
  non-Result target facts suppress stale args-pack metadata; syntax-only contexts keep the old
  fallback.
- Completed lowerer-side dispatch dereferenced Result operand slice: dispatch inference for
  `try(dereference(at(args<Result>, i)))` now reads the indexed target's published query Result
  fact before borrowed/pointer args-pack fallback can classify the value kind. Published
  non-Result target facts suppress stale args-pack metadata; syntax-only contexts keep the old
  fallback.
- Completed lowerer-side indexed Result source metadata slice: generic Result metadata resolution
  now reads published indexed-access query Result facts for `at(args<Result>, i)` and
  `dereference(at(args<Result>, i))` before args-pack Result metadata can classify downstream
  `Result.error(...)`, `Result.why(...)`, or combinator source values. Published non-Result
  target facts suppress stale args-pack metadata; syntax-only contexts keep the old fallback.
- Completed lowerer-side local Result source metadata slice: generic Result metadata resolution
  now reads published binding and local-auto Result facts for direct local Result sources plus
  pointer/reference-backed `dereference(local)` sources before lowerer-local Result metadata can
  classify downstream `Result.error(...)`, `Result.why(...)`, or combinator source values.
  Published non-Result facts suppress stale local metadata; syntax-only contexts keep fallback.
- Completed lowerer-side dispatch method receiver fact slice: dispatch inference for
  `FileError.why()` and nested `try(file.method())` file operations now reads the receiver's
  published binding, query, or local-auto fact before syntactic `FileError` spelling or
  lowerer-local `isFileHandle` metadata can classify the value kind. Published non-matching
  receiver facts suppress stale local metadata; no-fact syntax-only contexts keep the old
  fallback.
- Completed lowerer-side dispatch Result method fact slice: dispatch inference for
  `Result.ok(...)`, nested `try(Result.ok(...))`, `Result.error()`, and `Result.why()` now
  consumes published payload binding/query facts before legacy Result method arity fallback can
  classify the value kind. Missing semantic-product facts keep the dispatch value unresolved;
  no-fact syntax-only contexts keep the old fallback.
- Completed lowerer-side nested try file-method receiver slice: base-kind inference for
  `try(file.method())` now reads the file-method receiver's published binding, query, or
  local-auto fact before lowerer-local `isFileHandle` metadata can classify file operations as
  status `i32` results. Published non-file receiver facts suppress stale local file-handle
  metadata; no-fact syntax-only contexts keep the old fallback.
- Completed lowerer-side field receiver fact slice: base-kind inference for `receiver.field`
  now reads the receiver's published binding, query, or local-auto fact before lowerer-local
  struct-path inference can classify the field value kind. Published non-struct receiver facts
  suppress stale local struct metadata; no-fact syntax-only contexts keep the old fallback.
- Completed lowerer-side take/borrow query fact slice: base-kind inference for `take(storage)`
  and `borrow(storage)` now reads the call's published query fact before lowerer-local
  uninitialized-storage metadata can classify the value kind. Published non-scalar or missing
  semantic call facts suppress stale storage metadata; no-fact syntax-only contexts keep the old
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
- Completed lowerer-side statement binding args-pack initializer fallback slice: statement binding
  struct materialization now passes graph-backed array/vector target facts into the initializer
  access classifier before stale local args-pack metadata can choose struct-copy materialization.
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
- Completed routing coverage map-authority slice: direct-call, method-call, and bridge-path
  coverage validators now require the published routing lookup maps before raw routing entries can
  satisfy coverage or trigger stale-metadata checks. Raw routing entries without the published map
  no longer mask missing semantic-product routing facts.
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
- Completed lowerer-side indexed-access index-kind slice: native/string/array
  access validation now prefers semantic-product query, binding, and local-auto
  type facts for index expressions before using legacy expression inference.
- Completed lowerer-side `buffer_store` index-kind slice: GPU buffer stores now
  use the same graph-backed semantic-product index facts before legacy
  expression inference, preserving the existing integer-index diagnostic for
  syntax-only compatibility.
- Completed lowerer-side `buffer_store` target-kind slice: GPU buffer stores
  now classify the target buffer element kind from published binding,
  local-auto, and query facts before stale local buffer metadata can answer.
  Published non-buffer facts suppress stale buffer metadata while syntax-only
  compatibility keeps the legacy local and args-pack fallback.
- Completed lowerer-side dispatch dimension slice: GPU dispatch statement
  validation now classifies the three dimension expressions from published
  binding, local-auto, and query facts before legacy expression inference.
  Published non-`i32` facts suppress stale expression inference while
  syntax-only compatibility keeps the old fallback.
- Completed statement-call map insert receiver slice: builtin
  `map.insert(...)` rewriting now classifies the receiver from published
  binding, local-auto, and query map facts before local map metadata can
  supply key/value kinds. Published non-map facts suppress stale local map
  metadata while syntax-only compatibility keeps the old local fallback.
- Completed statement-call map-insert canonical receiver fallback slice:
  wrapped helper receiver classification now passes graph-backed map/non-map
  facts into the peeled receiver resolver before stale local map metadata can
  classify wrapped map helper forms.
- Completed statement-call vector mutator receiver slice: builtin
  `vector.push(...)` / `receiver.push(...)` rewriting now classifies the
  receiver from published binding, local-auto, and query vector facts before
  local vector metadata can select the builtin path. Published non-vector
  facts suppress stale local vector metadata while syntax-only compatibility
  keeps the old local fallback.
- Completed statement-call semantic adapter wiring slice: statement-call
  lowering now passes the published semantic program and product index into
  `runLowerStatementsCallsStep`, so graph-backed receiver gates cannot be
  disabled by null step inputs before local fallback metadata is consulted.
- Completed statement direct-call vector target-resolver fallback slice:
  explicit helper and builtin receiver probes now pass graph-backed vector
  facts into residual array/vector target resolution before stale local
  vector metadata can classify those receivers.
- Completed lowerer-side count-of-map-access slice: native count-kind
  inference for `count(map[key])` now asks the indexed access expression for
  its published query value kind before consulting local map metadata. Stale
  local map value kinds can no longer override a graph-backed `string` access
  result; local and literal map reconstruction remains the explicit
  syntax-only/no-fact compatibility fallback.
- Completed native count-of-map-access emission slice: native count emission
  for `count(map[key])` now uses the indexed access expression's inferred
  graph-backed value kind before local map metadata can select string-length
  emission. Known non-string graph facts suppress stale local string metadata;
  the local/literal map branch remains the compatibility fallback when no
  published access fact is available.
- Completed native runtime-string count emission slice: native count emission
  for `count(name)` now asks graph-backed value-kind inference before local
  string metadata can select string-length emission. Known non-string graph
  facts suppress stale local string metadata; local string metadata remains the
  syntax-only/no-fact compatibility fallback.
- Completed native literal-backed string count slice: native `count(value)`
  static string count emission now asks graph-backed value-kind inference
  before literal-backed string metadata can select string-table length
  emission. Known non-string graph facts suppress stale string-table metadata;
  no-fact literal-backed metadata remains the syntax-only compatibility
  fallback.
- Completed native string-call binding slice: shared native string argument
  emission now asks graph-backed value-kind inference before local string
  binding metadata can select table/runtime/argv string dispatch for named
  arguments. Known non-string graph facts suppress stale string bindings;
  no-fact local string metadata remains the syntax-only compatibility
  fallback.
- Completed native print string binding slice: native `print(name)` and
  `print_line(name)` emission now asks graph-backed value-kind inference before
  local string binding metadata can select static, argv, or dynamic string
  printing for named arguments. Known non-string graph facts fall through to
  ordinary numeric/bool printing; no-fact local string metadata remains the
  syntax-only compatibility fallback.
- Completed native File constructor dynamic-path slice: native
  `File<Mode>(path)` emission now asks graph-backed value-kind inference before
  runtime-string local metadata can select dynamic file-open emission. Known
  non-string graph facts suppress stale local string metadata; runtime local
  metadata remains the syntax-only/no-fact compatibility fallback.
- Completed native File constructor literal-backed path slice: native
  `File<Mode>(path)` emission now asks graph-backed value-kind inference before
  literal-backed local string metadata can select static file-open emission for
  name paths. Known non-string graph facts suppress stale table-backed local
  metadata; literal-backed local metadata remains the syntax-only/no-fact
  compatibility fallback, and string literals keep the direct static path.
- Completed native file-write literal-backed argument slice: native
  `file.write(value)` emission now asks graph-backed value-kind inference
  before literal-backed string metadata can select static string-write
  emission for non-literal arguments. Known numeric graph facts suppress stale
  table-backed string metadata; string literals and no-fact literal-backed
  bindings keep the direct static string-write path.
- Completed native dynamic string access target slice: native `text[index]`
  dynamic string access now asks graph-backed value-kind inference before
  runtime-string local metadata can select dynamic string-byte dispatch for
  name targets. Known non-string graph facts suppress stale local string
  metadata; runtime local metadata remains the syntax-only/no-fact
  compatibility fallback.
- Completed lowerer setup access element-kind slice: array/map/string access
  element-kind inference now asks graph-backed value-kind inference before
  local string receiver metadata can classify named indexed targets as byte
  elements. Known non-string graph facts suppress stale local string receiver
  metadata; no-fact local metadata remains the syntax-only compatibility
  fallback.
- Completed lowerer setup indexed receiver-method slice: setup-type method
  receiver inference for `text[index].method()` now asks graph-backed
  value-kind inference before local string receiver metadata can classify
  indexed named targets as `i32` receivers. Known non-string graph facts
  suppress stale local string receiver metadata; no-fact local metadata
  remains the syntax-only compatibility fallback.
- Completed lowerer setup reordered access return-kind slice: setup-type
  return-kind inference for reordered `at(index, values)` calls now asks
  graph-backed value-kind inference before local string metadata can decide
  whether a named positional argument is already the collection receiver.
  Known non-string graph facts let the reordered receiver probe continue;
  no-fact local metadata preserves the existing syntax-only leading-receiver
  compatibility behavior.
- Completed native string-key map lookup slice: native map lookup key emission
  now asks graph-backed value-kind inference before literal-backed string
  metadata can select the static string-table key path for string-key maps.
  Known non-string graph facts suppress stale string-table metadata; no-fact
  literal-backed key metadata remains the syntax-only compatibility fallback.
- Completed native non-literal string access guard slice: native indexed
  access validation now asks graph-backed value-kind inference before local
  string metadata can reject a named non-literal target. Known non-string
  graph facts continue to ordinary array/vector access; graph-known strings
  and no-fact local string bindings keep the unsupported string-index
  diagnostic.
- Completed native indexed-access string-target classifier slice: native
  indexed-access emission now classifies string targets from semantic-product
  collection, binding, local-auto, and query facts before dynamic string
  dispatch or non-literal string validation can consult local string metadata.
  Known non-string graph facts suppress stale runtime/string-binding access
  paths; graph-known strings keep the existing string-index diagnostic.
- Completed native collection literal string slice: native array/vector literals
  and then-active map constructor string-element emission now ask graph-backed value-kind
  inference before literal-backed string metadata can select string-table
  storage. Known non-string graph facts suppress stale string-table metadata;
  no-fact literal-backed metadata remains the syntax-only compatibility
  fallback.
- Completed native dereferenced mutation target slice: native
  `increment(dereference(...))` / `decrement(dereference(...))` inference now
  asks graph-backed pointer/reference facts before stale dereference
  value-kind inference can classify the mutable target. Known numeric pointer
  facts suppress stale local string metadata; no-fact contexts keep the
  existing inference fallback.
- Completed native indexed-access array/vector target slice: native
  `target[index]` load emission now asks graph-backed collection, binding,
  local-auto, and query facts before stale local array/vector metadata can
  classify named targets. Known scalar facts suppress stale vector locals;
  no-fact contexts keep the existing local collection fallback.
- Completed native indexed-access map target slice: native `target[index]`
  map lookup emission now asks graph-backed collection, binding, local-auto,
  and query facts before stale local map metadata can classify named targets.
  Known non-map facts suppress stale map locals; no-fact contexts keep the
  existing local map fallback.
- Completed lowerer setup method map-receiver probe slice: setup-type method
  resolution for `at(target, key).method()` and `tryAt(target, key).method()`
  now asks graph-backed map/non-map facts before local map metadata can block
  primitive receiver fallback. Known map facts suppress stale scalar locals;
  known non-map facts suppress stale map locals.
- Completed lowerer setup method vector gate slice: setup-type method
  resolution for bare vector `at`/`at_unsafe` access and vector mutator
  methods now asks graph-backed vector/non-vector facts before local vector
  metadata can decide whether to suppress builtin fallback. Known vector
  facts suppress stale scalar locals; known non-vector facts suppress stale
  vector locals.
- Completed inline canonical map helper gate slice: inline dispatch for
  canonical `/std/collections/map/*` helper calls now asks graph-backed
  map/non-map receiver facts before local map metadata can decide whether to
  defer to builtin/native map handling. Known map facts suppress stale scalar
  locals; known non-map facts suppress stale map locals.
- Completed late statement-expression canonical map helper gate slice: late
  expression helper dispatch now passes graph-backed map/non-map receiver facts
  into canonical `/std/collections/map/*` deferral checks before local map
  metadata can decide whether to keep the call on builtin/native handling.
- Completed inline collection target-resolver fallback slice: inline native
  collection fallback now passes graph-backed map and array/vector facts into
  the residual access-target resolvers before builtin collection spelling or
  direct-return reconstruction can classify receivers.
- Completed tail explicit map helper gate slice: tail dispatch for explicit
  `/std/collections/map/*` helper calls now asks graph-backed map/non-map
  receiver facts before local map metadata can decide whether to rewrite into
  builtin native helper forms. Known map facts suppress stale scalar locals;
  known non-map facts suppress stale map locals.
- Completed tail canonical experimental map helper gate slice: tail dispatch
  for canonical map helpers on experimental map receivers now asks
  graph-backed map/non-map receiver facts before local experimental-map
  struct metadata can choose direct experimental helper rewrites. Known map
  facts suppress stale scalar locals; known non-map facts suppress stale map
  locals.
- Completed tail borrowed map receiver rewrite slice: tail dispatch for
  borrowed or pointer map receivers now asks graph-backed wrapped-map/non-map
  receiver facts before local reference/pointer map metadata can inject
  implicit `dereference(...)` receivers. Known wrapped-map facts suppress stale
  scalar locals; known non-map facts suppress stale borrowed-map locals.
- Completed lowerer setup receiver-target map probe slice: setup-type
  receiver-target inference for bare `at(target, key)` and `tryAt(target, key)`
  receiver probes now asks graph-backed map/non-map facts before local map
  metadata can suppress fallback receiver-kind inference. Known map facts
  suppress stale scalar locals; known non-map facts suppress stale map locals.
- Completed late materialized collection receiver slice: late collection
  helper receiver materialization now asks graph-backed map, wrapped-map, and
  array/vector facts before stale local collection metadata can choose helper
  receiver materialization or classify nested args-pack access. Syntax-only
  local/direct-return inference remains the compatibility fallback.
- Completed lowerer-side base map result helper slice: base-kind result
  inference for `tryAt(...)` and `contains(...)` now passes graph-backed
  map/non-map receiver facts into helper result classifiers before stale local
  map metadata can decide result value kinds.
- Completed lowerer string-table target resolver slice: native string-table
  target resolution now asks graph-backed binding, local-auto, and query
  string/non-string facts before stale local string-table metadata can resolve
  named string indices. Known non-string facts suppress stale string-table
  locals; no-fact contexts keep the existing local table-index fallback.
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

Procedural compile-time genericity contract:
- Existing template syntax remains source-compatible, but its architectural
  interpretation is "`<...>` is the compile-time argument channel" rather than
  "templates are a separate language inside the language."
- Bare zero-argument execution, compile-time type locals, `typeof<symbol>`,
  and local generated type definitions must all reuse ordinary name resolution
  and produce deterministic ambiguity diagnostics instead of inventing a second
  lookup model.
- Compile-time locals, runtime locals, and visible definitions share the same
  local namespace for bare names; duplicate or ambiguous names are errors.
- `[type]` locals are semantic compile-time facts. They may be bound from a
  concrete type name or from `typeof<symbol>`, and later local binding or
  struct-field type envelopes may consume earlier type locals as concrete
  types. They must not survive into backend-facing IR.
- `typeof<symbol>` uses the compile-time argument channel. It resolves an
  unambiguous parameter, local value, or earlier `[type]` local in the current
  definition specialization and binds the concrete type fact for a `[type]`
  local. Runtime-call spelling such as `typeof(value)` is deliberately not the
  primitive, because parentheses remain the runtime argument channel.
- Current implementation boundary: direct definition-body
  `[type] Name { ConcreteType }` and `[type] Name { typeof<value> }`
  statements validate as semantic-only type facts, share the local
  duplicate-name namespace, can annotate later local binding and struct-field
  envelopes, and are erased before semantic-product publication and IR
  lowering. Consumed type-local facts publish as concrete binding, direct-call,
  type, and struct-field facts in the semantic product; lowerers must consume
  those facts instead of reconstructing local generated type paths from source
  spelling.
- Local generated structs may be defined inside a function body with
  field-only struct syntax such as `[struct] PairT { [LeftT] first ... }`.
  They are nominal per enclosing definition specialization, may consume
  earlier enclosing `[type]` facts in field envelopes, publish ordinary struct
  and field metadata for lowering, and are implementation-local. Their
  generated paths, semantic-product provenance, and IR names must be stable
  across repeated builds and import order, but the types cannot escape as
  externally named return or parameter types unless a later feature defines an
  explicit export/name mechanism.
- A returnable pair helper should instead return a caller-visible generic type
  such as `Pair<LeftT, RightT>` or another explicitly named public shape.
  Function-local generated types are for implementation-local storage.
- Requirements and contracts are definition transforms, not body statements or
  standalone compile-time commands. Keeping them in transform position makes
  boundary obligations part of the callable signature and lets overload
  filtering, semantic validation, and lowering consume the facts at the right
  phase.
- `require<...>` is the forced compile-time requirement form. It uses the
  `<...>` compile-time argument channel, participates in specialization and
  overload viability, publishes compile-time requirement facts during semantic
  validation, and never emits runtime code. If a `require<...>` predicate is
  not knowable at compile time, the constrained definition is non-viable or the
  call site gets a compile-time diagnostic.
- `require(...)` is the runtime-capable contract form. The compiler first tries
  to prove the predicate from semantic facts. If it cannot prove the predicate
  but the expression is pure and runtime-checkable, lowering emits a runtime
  precondition check. If the expression is neither statically provable nor
  runtime-checkable, semantic validation rejects it. Whether proven or checked,
  a successful contract publishes an assumed body fact for downstream semantic
  and lowering consumers.
- Compile-time requirement transforms should use one readable
  comma-separated predicate list, for example
  `[require<typeof<left> == i32, typeof<right> == i32>]`. The readable
  expression form rewrites into builtin compile-time predicates, such as
  `/std/meta/type_equals<typeof<left>, i32>()`, before semantic validation
  publishes requirement facts. `typeof<left>` is a compile-time query because
  it uses the compile-time argument channel; `typeof(left)` remains an ordinary
  runtime call shape and is never compile-time requirement syntax.
- Contract transforms also use one readable comma-separated predicate list, for
  example `[require(count(dst) == count(src))]`. They may read parameters,
  constants, `count(...)`, and other safe value facts, but may not allocate,
  mutate, perform IO, spawn tasks, or depend on backend-only effects. Runtime
  precondition failure must be deterministic; the exact failure transport is a
  runtime contract detail outside this compile-time requirement split.
- A definition may carry at most one `require<...>` transform and at most one
  `require(...)` transform. Multiple predicates for the same phase are written
  as comma-separated entries inside the matching transform, not as repeated
  transforms.
- Current implementation boundary: the parser and semantic validator still
  accept legacy `[require(...)]` as transition syntax for compile-time generic
  requirements. That spelling has no runtime fallback today and should be read
  as the old compile-time requirement form in checked-in executable examples
  until parser support for `require<...>` lands. New specification prose should
  use `require<...>` for forced compile-time requirements and reserve
  `require(...)` for runtime-capable contracts.
- The first implemented runtime-contract slice covers `require(...)` value
  comparisons whose operands are integer literals, integer parameters, or
  `count(parameter)` over array, vector, and string parameters. Predicates the
  compile-time evaluator can fully resolve keep their compile-time outcome;
  predicates over runtime parameter facts lower to one deterministic
  call-boundary precondition check that names the definition and predicate and
  fails with the runtime-error exit path. Runtime contracts stay rejected on
  the program entry definition, in `restrict<...>` position, and in `ct_if`
  conditions, and non-checkable operands such as float parameters remain
  compile-time diagnostics.
- The v1 compile-time requirement grammar should stay intentionally small:
  equality, inequality, comma-separated conjunction through `require<...>`,
  builtin predicates, user-defined compile-time predicates, and simple
  comparisons over compile-time values such as `N > 0`. Contract-form
  `require(...)` is likewise restricted to pure, deterministic, runtime-safe
  predicates. Avoid arbitrary boolean algebra until the semantic-product
  representation is proven stable.
- The initial builtin predicate families are:
  - `/std/meta/type_equals<A, B>()` and
    `/std/meta/type_not_equals<A, B>()` for type equality and inequality.
  - `/std/meta/has_trait<T>(Trait)` and
    `/std/meta/supports_call<Args..., ResultT>(name)` for trait and named
    operation/capability support.
  - `/std/meta/can_construct<T, Args...>()`, `/std/meta/can_copy<T>()`, and
    `/std/meta/can_move<T>()` for construction and lifecycle availability.
  - `/std/meta/has_field<T>(name)`, `/std/meta/field_type_equals<T, name, U>()`,
    and `/std/meta/has_member<T>(name)` for field/member queries.
  - `/std/meta/value_equals<A, B>()`, `/std/meta/value_not_equals<A, B>()`,
    `/std/meta/value_less<A, B>()`, `/std/meta/value_less_equal<A, B>()`,
    `/std/meta/value_greater<A, B>()`, and
    `/std/meta/value_greater_equal<A, B>()` for compile-time integer/value
    relations such as `N > 0`.
  The implemented capability slice evaluates `has_trait` for
  `Additive`/`Multiplicative`/`Comparable`/`Indexable`, named
  `supports_call`, exact positional `can_construct`, explicit lifecycle
  `can_copy`/`can_move`, visibility-aware `has_field`/`has_member`, and
  compile-time integer `value_*` equality and ordering predicates.
  The compile-time host can answer these canonical `/std/meta/*` predicates
  from published semantic requirement facts with deterministic success,
  unsatisfied, and invalid-evaluation results. `field_type_equals` remains
  deferred.
- Compile-time integer `value_*` predicates execute their pure comparison
  opcode through the shared `primec/VmKernelBoundary.h` API. The same API is
  used by the runtime VM numeric path, while compile-time evaluation still
  rejects runtime-only values, IO/file effects, heap/argv access, final backend
  IR, and `primevm` launches.
- Builtin requirement predicates live under `/std/meta/*`; readable predicate
  syntax may rewrite to these builtin helpers or compiler-recognized facts, but
  user helpers should not collide with the builtin namespace. Public
  user-authored predicates should use project or package namespaces outside
  `/std/meta/*`.
- Reflection predicates such as field/member queries obey normal visibility by
  default. Private fields are not visible to external requirements unless a
  future privileged reflection mode explicitly says otherwise.
- User-defined predicates distinguish an ordinary `false` result from an
  invalid predicate evaluation. `false` means the requirement is not satisfied;
  an invalid predicate body, unsupported operation, or missing compile-time fact
  is a hard diagnostic, not a non-viable candidate.
- User-defined requirement predicates return ordinary `bool` at the source
  level. The compile-time evaluation layer wraps `true` and `false` into typed
  requirement facts; evaluation faults remain hard diagnostics.
- The first implemented user-predicate slice evaluates pure zero-runtime-argument
  predicates whose bodies return a literal source `bool`. A `true` result
  publishes a satisfied requirement fact, a `false` result publishes an
  unsatisfied fact and fails the constrained definition, and unsupported bodies,
  runtime parameters, denied effects, unknown predicate definitions, or missing
  facts remain invalid-evaluation diagnostics.
- The implemented compile-time branch slice adds
  `ct_if(predicate()) { ... } else { ... }` for predicate conditions that can
  be evaluated from concrete type facts before ordinary validation. Statement
  branches contribute selected runtime statements, and expression-position
  branches such as `return(ct_if(...) { value } else { fallback })` or
  `[T] local{ct_if(...) { value } else { fallback }}` contribute the selected
  value. Generic-specialized definitions may also use `ct_if` over type facts
  after template monomorphization selects concrete parameter types. Only the
  selected branch contributes runtime code or diagnostics; the discarded branch
  is parsed but pruned before validation and lowering. Local generated structs
  introduced by the selected statement branch receive a deterministic
  branch-scoped identity such as `/pick/PairT__ct_if_then_5_5`; selected-branch
  uses are rewritten to that identity, while discarded-branch generated
  definitions and type facts stay out of semantic products and IR. Invalid
  `ct_if` predicates report the branch site, predicate source, predicate path,
  concrete compile-time facts, and the evaluation result that prevented branch
  selection. Branch-local generated type escape diagnostics report the selected
  branch, generated path, type-local fact provenance, and a short fix hint.
- Compile-time predicate and helper execution should run through a
  compiler-hosted compile-time VM facade. That facade may share the runtime VM
  interpreter core for arithmetic, calls, branching, and frame mechanics, but
  it has a separate `CompileTimeHost` with typed compile-time values, semantic
  facts, `/std/meta/*` intrinsics, provenance, budgets, caches, and
  phase-qualified compile-time effects. The shared VM execution kernel exposes
  an explicit host boundary for runtime-only behavior such as argv, heap
  allocation, file IO, and print IO so compile-time evaluation can reuse the
  deterministic interpreter mechanics without depending on `primevm` runtime
  state. The public compiler-internal facade reports stable categories for
  success, unsatisfied predicates, invalid evaluation, denied effects, budget
  exhaustion, and internal compiler errors; diagnostics format those categories
  with the definition, predicate, and source provenance supplied by semantic
  validation.
- Compile-time values are typed facts, not raw VM slots. The compiler-internal
  value model represents `bool`, signed and unsigned integer constants, string
  literals, type facts, symbols, and requirement outcomes with stable equality,
  hashing, debug formatting, and provenance handles. Runtime-only values such
  as buffers, heap addresses, or backend stack slots are rejected as invalid
  compile-time evaluation instead of being encoded as `uint64_t` VM slots.
- Restricted compile-time callable preparation currently turns published
  semantic requirement predicate facts for builtin `/std/meta/*` predicates
  into typed CT callable descriptors. Preparation records the definition path,
  predicate identity, source text, typed operands, budget, and provenance, and
  it rejects missing facts, unsupported predicate families, runtime-only
  operands, unsupported operand shapes, and exhausted preparation budgets before
  execution. Prepared CT callables are deliberately independent of final
  backend IR, native/C++ emission, and `primevm` runtime launch.
- The compiler must not depend on launching the normal runtime VM or final
  backend IR to evaluate requirements. Requirements run during semantic
  validation, before final lowering is complete, so the CT evaluator needs a
  restricted compile-time lowering path or a small CT bytecode/fact evaluator
  that shares VM pieces without coupling semantics to `primevm`.
- Implementation should stage the evaluator conservatively: builtin
  `/std/meta/*` facts first, then a typed compile-time value layer, then pure
  user-defined predicate execution, and only then effectful compile-time
  execution gated by `effects<compiletime>(...)`.
- Compile-time flow is pure by default. Without an explicit phase-qualified
  effect, compile-time requirement and helper evaluation may only read semantic
  facts, compile-time arguments, template arguments, type/symbol metadata,
  literal-backed strings, and deterministic imported source/module metadata
  that semantic validation already loaded. It may execute arithmetic,
  comparisons, boolean logic, deterministic branches, and calls to other pure
  compile-time helpers over typed compile-time values.
- `effects<compiletime>(...)` uses the same effect vocabulary as runtime
  `effects(...)`, but it authorizes only the compile-time phase and does not
  imply any runtime permission. Runtime `effects(...)` does not authorize
  compile-time host access. Effectful compile-time helpers must name the
  phase-qualified effects on the enclosing definition before they can consult
  deterministic host services such as source-file reads within the import
  graph, package metadata, or explicit compile-time diagnostics/tracing.
  Host services must publish stable provenance and cache fingerprints.
- Some operations are always invalid at compile time, even with an effect:
  launching `primevm` or final backend IR, depending on native/C++/GPU backend
  behavior, allocating runtime heap objects, observing addresses or stack
  slots, reading argv/stdin/stdout/stderr as runtime streams, using debugger
  state, reading wall-clock time, randomness, environment variables, process
  state, network state, or unordered host iteration whose order can affect a
  result.
- Compile-time termination is budgeted in categories that TODO-4358 must
  enforce independently: preparation steps while turning semantic facts into
  callable descriptors, call depth and recursion edges between compile-time
  helpers, executed CT instructions or evaluator steps, typed value/storage
  size, imported host bytes consulted by effectful helpers, and emitted
  diagnostic/provenance payload size. Budget exhaustion is a deterministic
  invalid-evaluation category, not a crash or fallback to runtime execution.
- Compile-time caches are semantic caches, not backend caches. A cache key must
  include the language/semantic-product version, predicate or helper identity,
  normalized compile-time arguments, visible import set sorted by canonical
  path, semantic facts read by the helper with provenance handles, active
  compile-time effects, host-service fingerprints for effectful reads, and the
  evaluator policy version. Import order and unordered map iteration must not
  affect the key.
- Compile-time diagnostic categories are stable: `satisfied`, `unsatisfied`,
  `invalid-evaluation`, `denied-effect`, `budget-exhausted`,
  `cache-corrupt-or-version-mismatch`, and `internal-compiler-error`. Each
  diagnostic includes the predicate/helper path, source span, selected
  specialization if any, effects consulted, budget category when relevant, and
  the semantic facts or host fingerprints that caused the result.
- Failed requirements on direct calls are diagnostics, not C++-style
  substitution failure by accident. Overload filtering may reject non-viable
  candidates only when it also preserves failed-requirement diagnostics.
- Requirement-constrained overloads are only automatically selected when
  exactly one candidate is viable. Requirements should not rank candidates by
  specificity; ambiguous viable candidates require clearer names or imports.
- Diagnostics for this feature should lead with the concrete call site, show
  the failed requirement site, list the relevant compile-time facts, and end
  with a short actionable hint.
- Generic public examples should use consistent parameter names such as `T`,
  `ElemT`, `LeftT`, `RightT`, and value-level names such as `N`.
- High-level generic examples should follow the style section in
  `docs/CodeExamples.md`: use plain inference for pass-through helpers,
  `require<...>` for forced compile-time obligations, `require(...)` for pure
  runtime-capable contracts that may be proven or checked, compile-time value
  facts for values such as lengths, `ct_if(...)` only when a branch must be
  pruned before validation, local generated structs for non-escaping storage,
  and explicit template arguments when the call site intentionally selects a
  public type or compile-time value. The current checked-in generic
  requirement example still uses legacy transition `require(...)` because this
  TODO specifies the phase split without implementing parser behavior. Runnable
  examples live in
  `examples/2.Inference/generic_identity.prime`,
  `examples/2.Inference/generic_pair_design.prime`, and
  `examples/2.Inference/generic_requirements_design.prime`.
- The implementation path should first document and parse the compile-time
  argument channel, then add bare zero-argument name resolution, then add type
  locals and `typeof<symbol>`, and only then lower local generated types through
  the existing monomorphization/semantic-product boundary.
- These features are blocked on the same graph-backed ownership discipline as
  template migration: compile-time generic facts must be published or adapted at
  an explicit semantic-product boundary, not reconstructed independently by
  lowerers or CT-eval helpers.
