# Repository Memories

This file stores durable session-derived facts that are useful in later work. Keep it short, factual, and easy to diff.

## Active Memories

- `arg-pack-dereference-struct-inference`: IR struct-type inference now
  propagates through `dereference(...)`, so indexed borrowed arg-pack values
  (for example `args<Reference<Map<K, V>>>`) preserve their struct identity
  when forwarded directly into helper calls instead of failing with
  `<unknown>` struct-parameter mismatches.
- `experimental-vector-negative-index-guard`: `/std/collections/experimental_vector/*`
  checked access now rejects negative indices via `vectorCheckIndex` (same
  bounds-error path as high indices), and `vectorAtUnsafe` now routes through
  the same guard for both negative and positive out-of-range indices, so those
  accesses no longer fall through to invalid pointer arithmetic,
  uninitialized-slot reads, or indirect-address traps; the guard now triggers
  bounds errors through zero-length `array` indexing instead of constructing a
  canonical `/vector/*` value.
- `experimental-vector-push-growth-overflow-contract`:
  `/std/collections/experimental_vector/vectorPush` now diverts capacity-
  doubling overflow into the shared runtime-contract path instead of letting
  the doubled capacity wrap or continue into unchecked growth.
- `ir-lowerer-vector-mutator-call-form-expression-resolution`: IR lowering now
  probes bare vector mutator expression calls (`push`, `pop`, `reserve`,
  `clear`, `remove_at`, `remove_swap`) through the same receiver-based method
  resolution path as statement form, so VM-lowered user-defined `/vector/*`
  expression shadows no longer fall into backend-owned `vector helper`
  rejects.
- `ir-lowerer-query-local-collection-join-inference`: IR lowering now
  preserves collection binding metadata for local `auto` bindings sourced from
  resolved collection-returning calls and `if` branch joins, so query-local
  `/vector/count` direct-call and slash-method forms lower successfully across
  compile-pipeline, C++ emitter, VM, and native coverage instead of failing
  the old branch-compatibility boundary.
- `map-bare-access-canonical-helper-contract`: bare-root
  `contains(values, key)`, `at(values, key)`, and `at_unsafe(values, key)`
  on builtin `map<K, V>` now reject with canonical
  `unknown call target: /std/collections/map/*` diagnostics unless a
  canonical helper is imported or explicitly defined, and bare
  `values.at(...)` / `values.at_unsafe(...)` method sugar now follows the
  same-path reject contract instead of falling back to compiler-owned
  access behavior; the C++ emitter now matches that direct-call contract for
  bare `at` / `at_unsafe` calls instead of compiling them through the old
  builtin access fallback.
- `semantic-product-direct-call-routing`: production lowering now validates
  that every non-method direct call has a published semantic-product routing
  fact before lowering starts, and semantic-product-aware direct-call
  resolution no longer falls back to scope/import path recovery when those
  facts are absent.
- `semantic-product-method-call-routing`: production lowering now validates
  that every method call has a published semantic-product routing fact
  before lowering starts, and semantic-product-aware method resolution no
  longer falls back to receiver/helper inference when those facts are
  absent.
- `semantic-product-bridge-path-routing`: production lowering now requires
  published semantic-product bridge-path choices for collection-helper
  bridge calls, and semantic-aware direct-call resolution no longer routes
  same-path or canonical collection-helper bridge calls through duplicated
  direct-call targets when the bridge choice is absent.
- `semantic-product-binding-routing`: production lowering now validates
  that parameter/local binding facts are published before lowering starts,
  and semantic-aware binding classification no longer falls back to AST
  transforms for semantic-id-backed binding expressions.
- `semantic-product-local-auto-lowering-input`: production lowering now
  validates that semantic-id-backed implicit/`auto` local bindings have
  published semantic-product local-`auto` facts before lowering starts, and
  local binding setup now consumes the published local-`auto` binding type
  instead of re-inferring that metadata from AST initializer shape.
- `semantic-product-query-and-try-lowering-inputs`: semantic-id-backed
  generic call-result metadata now requires published semantic-product query
  facts, and semantic-id-backed `try(...)` lowering plus `try(...)`
  expression-kind inference now require published semantic-product `try(...)`
  facts instead of probing AST/result metadata as an optional fallback.
- `semantic-product-lowering-entrypoints`: `prepareIrModule(...)` and
  `IrLowerer::lower(...)` now reject null `SemanticProgram` inputs
  immediately, so production lowering only crosses the boundary with a
  published semantic product.
- `semantic-product-entry-params-and-on-error-bound-args`: lowering now
  requires published semantic-product entry parameter facts for entry
  count/argv setup, and semantic-product-backed `on_error` setup now
  consumes published `boundArgTexts` instead of falling back to AST
  parameters or reparsing `on_error` transforms.
- `semantic-product-required-callable-metadata`: once lowering is handed a
  `SemanticProgram`, entry effect/capability masks, callable effect validation,
  lowered callable metadata, entry return metadata, and `on_error` wiring now
  require published callable summaries plus the matching return/`on_error`
  facts instead of silently falling back to AST transforms.
- `semantic-product-return-metadata-consumption`: once lowering is handed a
  `SemanticProgram`, `getReturnInfo(...)` now precomputes callable return and
  result metadata from published callable summaries plus return facts instead
  of rebuilding that cache from AST/type-resolution inference, so downstream
  result/return consumers stay on semantic-product-owned metadata after the
  lowering boundary.
- `semantic-product-temporary-call-classification`: statement binding and
  default-parameter classification now thread the semantic-product target
  adapter through `inferStatementBindingTypeInfo(...)` and
  `inferCallParameterLocalInfo(...)`, so semantic-id-backed temporary calls
  prefer published binding/query facts over AST transform/collection-shape
  fallback when lowering classifies local/result metadata after the boundary.
- `semantic-product-provenance-handles`: lowering-consumed semantic-product
  fact families now carry explicit stable `provenanceHandle` values derived
  from the published semantic node ids, the semantic-product dump exposes
  `provenance_handle=<id> source="line:column"` for those facts, and the
  semantic-product snapshot suite now locks provenance-handle determinism
  alongside semantic-node id determinism across repeated runs plus unrelated
  helper-definition reorderings.
- `semantic-product-return-query-try-on-error-joins`: the temporary lowerer
  semantic-product adapter now keys return, local-`auto`, query, `try(...)`,
  and `on_error` facts by structural semantic ids, and semantic-product-backed
  entry return plus `on_error` lowering now join through definition semantic
  ids instead of definition-path lookups except for the narrow compatibility
  fallback still kept for unannotated manual fixtures.
- `semantic-product-structural-node-ids`: validated AST definitions,
  executions, and expressions now carry deterministic structural
  `semanticNodeId` values hashed from their resolved scope path plus traversal
  segment, and the lowerer semantic-product adapter now keys direct-call,
  method-call, bridge-path, and binding facts off those ids instead of the old
  `line:column:name` join plus duplicate-key collapse.
- `template-monomorph-core-utilities-header`:
  shared TemplateMonomorph core utility helpers now live in
  `src/semantics/TemplateMonomorphCoreUtilities.h` and are included by
  `TemplateMonomorph.cpp`, covering builtin type and collection
  classification plus overload path and import utility helpers.
- `uninitialized-type-helpers-split`: Group 10 reduced
  `src/ir_lowerer/IrLowererUninitializedTypeHelpers.cpp` to 578 lines by
  moving entry/runtime setup-builder orchestration into
  `src/ir_lowerer/IrLowererUninitializedSetupBuilders.cpp` and struct-return
  inference into `src/ir_lowerer/IrLowererUninitializedStructInference.cpp`,
  while leaving storage-access helpers in `IrLowererUninitializedTypeHelpers.cpp`
  so the existing helper API surface and validation coverage remain intact.

### template-monomorph-final-orchestration-header
- Updated: 2026-03-26
- Tags: semantics, template-monomorph, refactor
- Fact: Final monomorphization orchestration now routes import-alias setup and definition/execution rewrite passes
  through `src/semantics/TemplateMonomorphFinalOrchestration.h`.
- Evidence: Group 9 extracted `buildImportAliases(...)`, definition rewrite coordination, and execution rewrite
  coordination out of `monomorphizeTemplates(...)` into dedicated helper functions.

### template-monomorph-source-definition-setup-header
- Updated: 2026-03-26
- Tags: semantics, template-monomorph, refactor
- Fact: Source-definition collation, helper-overload family expansion, and template-root entry-path validation for
  monomorphization now run via `src/semantics/TemplateMonomorphSourceDefinitionSetup.h`.
- Evidence: Group 9 extracted the monomorph setup block from `monomorphizeTemplates(...)` into
  `initializeTemplateMonomorphSourceDefinitions(...)`.

## Maintenance Notes

- Keep entries sorted by slug within the section.
- Delete wrong entries instead of leaving contradictory facts behind.
- Prefer updating an existing entry over adding a near-duplicate.
- Avoid copying obvious facts from `AGENTS.md` or canonical design docs unless the shorter memory adds unique
  operational value.
