# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started
  ✓ Finished

Finished items are periodically moved to `docs/todo_finished.md`.

**Types & Semantics**
Roadmap note: completed map/vector migration and compatibility-cleanup slices now live in `docs/todo_finished.md`. Keep this live file focused on unfinished ownership/runtime substrate work, SoA bring-up, and the remaining graph/boundary items.

**Group 4 - Ownership and lifetime substrate**
Ownership/drop status note: completed guard and container-error-contract checkpoints are archived in `docs/todo_finished.md`. Keep the live file on the remaining non-trivial destruction and lifetime substrate work.
- ◐ Implement container element drop behavior in vector/map runtimes once the ownership contract is fixed. Progress: experimental `.prime` `Map<K, V>` destruction now reuses the experimental-vector drop/free steps for its key/payload storage so owned map buffers and element drops clean up on destroy, canonical `/std/collections/vector/*` `pop`/`clear` plus indexed-removal helpers now accept ownership-sensitive element types on explicit experimental `Vector<T>` bindings because they forward onto the experimental `.prime` discard/compaction implementation, canonical `/std/collections/map/insert` now forwards explicit experimental `Map<K, V>` bindings onto the `.prime` overwrite/growth path so ownership-sensitive value updates run through the same drop/reinit flow, canonical `/std/collections/map/insert_ref` now forwards borrowed `Reference<Map<K, V>>` bindings onto that same overwrite/growth path, explicit experimental `Map<K, V>` value method sugar now resolves through the `.prime` `/Map/*` method path so ownership-sensitive read/update flows stay on the same runtime drop/overwrite path, borrowed `Reference<Map<K, V>>` method sugar now resolves through the `.prime` borrowed-helper path so ownership-sensitive experimental-map overwrite/drop flows work through borrowed references, and builtin canonical `map<K, V>` inserts now perform real in-place overwrite when the numeric key already exists and grow through one generic arbitrary-`n` grow/copy/repoint path for owning local numeric maps without the old dead count-by-count lowerer staircase. The remaining live ownership/drop work is now split into explicit builtin vector discard/removal and wider builtin-map growth/drop slices.
  - ○ Lift the builtin `vector` / `soa_vector` `pop` / `clear` drop-trivial restriction once container-owned destruction semantics are implemented instead of deferred.
  - ○ Lift the builtin `vector` / `soa_vector` `remove_at` / `remove_swap` drop-trivial and relocation-trivial restriction once removed-element destruction and survivor compaction semantics are implemented.
  - ○ Extend builtin canonical `map<K, V>` growth/drop beyond the current owning local numeric-map path once the remaining ownership-sensitive runtime substrate is defined.


**Group 8 - SoA de-builtinization**
- ◐ Extend the SoA storage substrate from the completed fixed-width single/two/three/four/five/six/seven/eight/nine/ten/eleven/twelve/thirteen/fourteen/fifteen-column primitives to reflected arbitrary-width schema allocation/grow/free. Progress: the remaining work is now split into explicit allocation, grow/realloc, and free/drop slices instead of one catch-all item.
  - ○ Add reflected arbitrary-width schema allocation on top of the fixed-width substrate and the existing deterministic pending-width gate.
  - ○ Add reflected arbitrary-width schema grow/realloc over that allocation path.
  - ○ Add reflected arbitrary-width schema free/drop cleanup over that allocation path.
- ◐ Extend the borrowed-view substrate from the completed single-column borrowed-slot foothold to language-level invalidation and richer borrowed field-view semantics after the current compiler-owned builtin `soa_vector` paths are retired. Progress: the remaining borrowed-view work is now split into explicit invalidation and borrowed field-view follow-ups instead of one broad parent item.
  - ○ Add language-level invalidation rules on top of the current single-column borrowed-slot substrate once the remaining compiler-owned builtin `soa_vector` paths are retired.
  - ○ Add richer borrowed field-view semantics on top of that substrate once the remaining compiler-owned builtin `soa_vector` paths are retired.
- ◐ Extend experimental stdlib `/std/collections/experimental_soa_vector/*` to field-view indexing (`value.field()[i]`) once the experimental `.prime` implementation reaches that boundary and the next minimal substrate slice is clear. Progress: read-only reflected wrapper field indexing plus indexed/ref-field writes now clear semantics/runtime through the existing `soaVectorGet<T>(..., i).field` and `soaVectorRef<T>(..., i).field` substrate across direct, borrowed, helper-return, method-like helper-return, and inline location-wrapped receiver families. Remaining work is now split into richer borrowed-view semantics and the still-pending standalone mutating method/call writes.
  - ○ Extend experimental wrapper field-view semantics to richer borrowed-view surfaces beyond the current read-only method/helper coverage and indexed access on direct, local shorthand, explicitly dereferenced borrowed, borrowed helper-return, method-like struct-helper return, and inline location-wrapped wrapper receivers.
  - ○ Replace the remaining standalone mutating method/call field-view pending contract (`assign(value.field(), next)` / `assign(field(value), next)`) with real field-write lowering on direct, borrowed-local, borrowed helper-return, method-like struct-helper return, and inline location-wrapped wrapper receivers.
**Architecture / Type-resolution graph**
**Group 11 - Near-term graph queue**
Blocked by Group 13 rollout constraints until the remaining collection-helper/runtime predecessor items are finished and the return-kind pilot path is stable enough to resume broader graph work.
- ○ Expand positive end-to-end graph conformance coverage for backend dispatch on C++/VM/native once the still-open query-shaped canonical helper/method resolution path moves past the current compile-pipeline `if branches must return compatible types` boundary.
- ○ Implement graph-backed query invalidation rules and coverage for local-binding, control-flow, and initializer-shape edits now that the intra-definition invalidation contract is documented.
- ○ Implement graph-backed query invalidation rules and coverage for definition-signature, import-alias, and receiver-type edits now that the cross-definition invalidation contract is documented.
- ○ Implement the next non-template inference-island migrations now that the graph-backed cutover contract is documented, so mixed ad hoc inference paths stop surrounding the stabilized return/query/local pilot.
- ○ Implement the graph/CT-eval interaction contract now that the boundary is documented, so compile-time evaluation either consumes shared dependency state directly or goes through one explicit tested adapter.
- ○ Implement the graph-backed explicit/implicit template-inference migrations now that the cutover contract is documented, once the non-template inference islands and invalidation rules are pinned.
- ○ Implement the next omitted-envelope and local-`auto` graph expansions now that the widening contract is documented, after the next inference-island migrations prove stable on the graph path.
- ○ Implement graph performance guardrails and sustained perf coverage now that the regression-budget contract is documented, before optional parallel solve work broadens the graph surface further.
- ○ Prototype optional parallel solve now that its execution and merge contract is documented, once the single-threaded graph path, invalidation rules, CT-eval boundary, and performance guardrails are stable.


**Group 12 - Semantics/lowering boundary**
Boundary note: this group is now split into semantic-product creation, pipeline plumbing, lowering cutover, and cleanup so it can be worked incrementally instead of as one flat migration queue.

Semantic product creation:
- ○ Implement the first semantic-product builder slice now that its scope is documented, materializing resolved call targets, binding types, effects/capabilities, and struct metadata from `SemanticsValidator`.
- ○ Implement the second semantic-product builder slice now that its scope is documented, moving graph-backed local `auto`, query, `try(...)`, and `on_error` metadata out of test-only snapshot plumbing and into the semantic product.

Pipeline plumbing:
- ○ Implement the CLI/runtime plumbing cutover for the semantic product now that the end-to-end handoff contract is documented.
- ○ Implement the temporary migration adapter now that its cutover/removal contract is documented.
- ○ Implement semantic-product publication through `CompilePipelineOutput` and `Semantics::validate` now that the success-artifact contract is documented.
- ○ Implement the deterministic semantic-product dump/formatter plus golden coverage now that its inspection contract is documented.
- ○ Implement ownership-split tests for source spans, debug/source-map provenance, and syntax-faithful data using the documented test matrix.

Lowering cutover:
- ○ Cut over `IrLowerer::lower` and `prepareIrModule` so IR preparation consumes the semantic product directly, then retire the raw-`Program` lowering path once the temporary adapter is removed.
- ○ Switch `IrLowerer` entry setup to consume resolved call targets from the semantic product instead of re-deriving them from AST state.
- ○ Switch lowerer type/binding setup to consume semantic-product binding metadata instead of repeating helper/type inference logic.
- ○ Switch lowerer effect/capability and struct-layout setup to consume semantic-product metadata instead of re-reading AST/transforms for those facts.

Coverage and migration cleanup:
- ○ Implement the narrow semantic-product unit/golden suite now that its fact coverage and scope are documented.
- ○ Add end-to-end compile-pipeline conformance cases that exercise the semantic-product boundary through C++/VM/native lowering paths.
- ○ Migrate tests and public testing helpers from `primec/testing/SemanticsValidationHelpers.h` snapshot surfaces onto the semantic-product inspection surface where they are asserting lowering-facing facts.
- ○ Delete redundant testing-only semantic snapshot plumbing once equivalent semantic-product dumps and conformance coverage exist, leaving one canonical inspection surface for lowering-facing semantic facts.
- ○ Delete lowerer-side stdlib/helper alias fallback paths once equivalent canonical resolution is provided by the semantic product.
- ○ Implement the structured diagnostic sink now that its single-threaded first-error contract is documented, replacing `SemanticsValidator`'s shared mutable `error_` flow without changing current failure policy.
- ○ Implement stable semantic-product node identities or sort keys for diagnostics now that the merge-order contract is documented, so later parallel validation can publish deterministic diagnostic order.
- ○ Implement the per-definition validation-state split now that the context/caching contract is documented, so semantic-product construction can use explicit contexts instead of validator-global mutable fields.
- ○ Implement pipeline-facing tests that exercise the semantic-product inspection surface and its relation to existing AST/IR dumps once that stage exists, now that the test matrix is documented.
