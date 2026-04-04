# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started
  ✓ Finished

Finished items are periodically moved to `docs/todo_finished.md`.

**Types & Semantics**
Roadmap note: completed map/vector migration and compatibility-cleanup slices now live in `docs/todo_finished.md`. Keep this live file focused on unfinished ownership/runtime substrate work, SoA bring-up, and the remaining graph/boundary items.

**Group 8 - SoA de-builtinization**
- ◐ Extend the borrowed-view substrate from the completed single-column borrowed-slot foothold to language-level invalidation and richer borrowed field-view semantics after the current compiler-owned builtin `soa_vector` paths are retired. Progress: the invalidation contract and richer borrowed field-view contract are now documented; the current completed foothold is indexed borrowed-slot read/write projection syntax, but those projections are recomputed per use through the existing helper rewrite/lowering path and do not yet materialize standalone borrowed values, so the remaining live invalidation work starts with later standalone `ref(...)` and standalone borrowed field-view surfaces plus their borrowed receiver families.
  - ◐ Implement language-level invalidation rules on top of the borrowed-view substrate now that the borrowed-view contract is documented. Progress: current indexed borrowed-slot projection syntax (`ref(...).field`, `.ref(i).field`, and `value.field()[i]`-style reads/writes) is recomputed per use through the existing `soaVectorGet(...).field` / `soaVectorRef(...).field` rewrite path rather than materializing a standalone borrowed value, so the remaining invalidation boundary starts with later standalone `ref(...)` and standalone borrowed field-view values plus their provenance/escape rules.
    - ○ Invalidate standalone `ref(...)` values on structural growth surfaces (`push`, `reserve`) once those values exist.
    - ○ Invalidate standalone borrowed field-view values on structural growth surfaces (`push`, `reserve`) once those values exist.
    - ○ Invalidate standalone `ref(...)` values on shrink/motion helpers (`remove_*`, `clear`, and later size-changing helpers) once those helpers exist.
    - ○ Invalidate standalone borrowed field-view values on shrink/motion helpers (`remove_*`, `clear`, and later size-changing helpers) once those helpers exist.
    - ○ Invalidate standalone `ref(...)` values on reallocating conversion and storage-replacement surfaces such as `to_aos`.
    - ○ Invalidate standalone borrowed field-view values on reallocating conversion and storage-replacement surfaces such as `to_aos`.
    - ○ Invalidate standalone `ref(...)` values on wrapper destruction and other end-of-owner lifetime boundaries.
    - ○ Invalidate standalone borrowed field-view values on wrapper destruction and other end-of-owner lifetime boundaries.
    - ○ Enforce provenance/escape rules for standalone `ref(...)` and standalone borrowed field-view values reached through `location(...)`-wrapped receivers.
    - ○ Enforce provenance/escape rules for standalone `ref(...)` and standalone borrowed field-view values reached through helper-return and method-like helper-return receivers.
- ◐ Extend experimental stdlib `/std/collections/experimental_soa_vector/*` to field-view indexing (`value.field()[i]`) once the experimental `.prime` implementation reaches that boundary and the next minimal substrate slice is clear. Progress: read-only reflected wrapper field indexing plus indexed/ref-field writes now clear semantics/runtime through the existing `soaVectorGet<T>(..., i).field` and `soaVectorRef<T>(..., i).field` substrate across direct, borrowed, helper-return, method-like helper-return, and inline location-wrapped receiver families. The richer borrowed-view contract and standalone mutating method/call write contract are now documented; remaining work is implementing those surfaces on top of the existing substrate.
  - ◐ Implement the documented richer borrowed field-view semantics on top of the existing wrapper substrate, extending standalone field-view values beyond the current read-only method/helper coverage and indexed access. Progress: this richer borrowed-view surface is now split by receiver family plus the remaining pass/return/local-binding preservation seam.
    - ○ Implement standalone borrowed field-view values for direct borrowed locals and explicitly dereferenced borrowed receivers.
    - ○ Implement standalone borrowed field-view values for borrowed helper-return and method-like struct-helper-return receivers.
    - ○ Implement standalone borrowed field-view values for inline `location(...)`-wrapped borrowed receivers.
    - ○ Preserve borrowed field-view semantics across local binding, pass-through, and return surfaces instead of materializing owning vector values.
  - ◐ Implement the documented standalone mutating method/call field-view write semantics, replacing the remaining pending contract (`assign(value.field(), next)` / `assign(field(value), next)`) with real field-write lowering on top of the existing writable substrate. Progress: this mutating write surface is now split by receiver family instead of one umbrella write item.
    - ○ Implement standalone mutating method/call field-view writes for direct wrapper locals and borrowed-local receivers.
    - ○ Implement standalone mutating method/call field-view writes for borrowed helper-return and method-like struct-helper-return receivers.
    - ○ Implement standalone mutating method/call field-view writes for inline `location(...)`-wrapped borrowed receivers.
**Architecture / Type-resolution graph**
**Group 11 - Near-term graph queue**
Blocked by Group 13 rollout constraints until the remaining collection-helper/runtime predecessor items are finished and the return-kind pilot path is stable enough to resume broader graph work.
- ◐ Expand positive end-to-end graph conformance coverage for backend dispatch on C++/VM/native once the still-open query-shaped canonical helper/method resolution path moves past the current compile-pipeline `if branches must return compatible types` boundary. Progress: this backend-dispatch umbrella is now split into explicit compile-pipeline boundary lifts plus per-backend dispatch slices, so graph conformance can land incrementally once query-shaped canonical helper resolution clears the current branch-compatibility failure.
  - ○ Add positive compile-pipeline parity for query-shaped canonical helper direct-call forms once they move past the current `if branches must return compatible types` boundary.
  - ○ Add positive compile-pipeline parity for query-shaped canonical helper slash-method forms once they move past the current `if branches must return compatible types` boundary.
  - ○ Add C++ backend dispatch conformance for the query-shaped canonical helper/method path once compile-pipeline parity is positive.
  - ○ Add VM backend dispatch conformance for the query-shaped canonical helper/method path once compile-pipeline parity is positive.
  - ○ Add native backend dispatch conformance for the query-shaped canonical helper/method path once compile-pipeline parity is positive.
- ◐ Implement graph-backed query invalidation rules and coverage for intra-definition edits now that the intra-definition invalidation contract is documented. Progress: this invalidation surface is now split by edit family so local dataflow, control-flow, and initializer-shape invalidation can be implemented and pinned separately.
  - ○ Implement graph-backed query invalidation rules and coverage for local-binding edits.
  - ○ Implement graph-backed query invalidation rules and coverage for control-flow edits.
  - ○ Implement graph-backed query invalidation rules and coverage for initializer-shape edits.
- ◐ Implement graph-backed query invalidation rules and coverage for cross-definition edits now that the cross-definition invalidation contract is documented. Progress: this invalidation surface is now split by dependency family so signature, import-alias, and receiver-type invalidation can be implemented and pinned separately.
  - ○ Implement graph-backed query invalidation rules and coverage for definition-signature edits.
  - ○ Implement graph-backed query invalidation rules and coverage for import-alias edits.
  - ○ Implement graph-backed query invalidation rules and coverage for receiver-type edits.
- ◐ Implement the next non-template inference-island migrations now that the graph-backed cutover contract is documented. Progress: the remaining non-template cutover is now split into direct-call, receiver-call, and collection bridge inference seams instead of one umbrella migration bullet.
  - ○ Migrate the next direct-call/callee non-template inference island onto the graph path.
  - ○ Migrate the next receiver/method-call non-template inference island onto the graph path.
  - ○ Migrate the next collection helper/access fallback non-template inference island onto the graph path.
- ◐ Implement the graph/CT-eval interaction contract now that the boundary is documented. Progress: the remaining work is now split into shared dependency-state consumption, explicit adapter seams, and conformance coverage instead of one broad CT-eval handoff item.
  - ○ Route graph-backed consumers onto shared dependency-state consumption where CT-eval and graph validation overlap.
  - ○ Add one explicit adapter for the remaining CT-eval-only paths that still cannot consume shared graph dependency state directly.
  - ○ Add conformance coverage for the graph/CT-eval handoff boundary.
- ◐ Implement the graph-backed explicit/implicit template-inference migrations now that the cutover contract is documented. Progress: the remaining template cutover is now split into explicit template-argument, implicit inference, and revalidation/monomorph follow-up slices instead of one umbrella migration bullet.
  - ○ Migrate the next explicit template-argument inference surface onto the graph path.
  - ○ Migrate the next implicit template-inference surface onto the graph path.
  - ○ Migrate the matching revalidation/monomorph follow-up for those template-inference surfaces onto the graph path.
- ◐ Implement the next omitted-envelope and local-`auto` graph expansions now that the widening contract is documented. Progress: the widening work is now split into omitted-envelope facts, new initializer families, and control-flow join widening slices instead of one umbrella expansion bullet.
  - ○ Materialize omitted-envelope graph facts for the next widening slice.
  - ○ Expand local-`auto` graph support across the next initializer-family surface.
  - ○ Expand local-`auto` graph support across the next control-flow join surface.
- ◐ Implement graph performance guardrails and sustained perf coverage now that the regression-budget contract is documented. Progress: the performance work is now split into baseline metrics, regression thresholds/reporting, and sustained perf coverage instead of one umbrella guardrail bullet.
  - ○ Add baseline graph timing and invalidation metrics for the current stabilized graph surface.
  - ○ Add regression thresholds and reporting for those graph metrics.
  - ○ Add sustained graph performance coverage over the stabilized graph surface.
- ◐ Prototype optional parallel solve now that its execution and merge contract is documented. Progress: the parallel-solve work is now split into execution partitioning, deterministic merge ordering, and parity coverage instead of one umbrella prototype bullet.
  - ○ Prototype the partition/execution layer for optional parallel solve.
  - ○ Implement deterministic merge ordering for optional parallel solve results.
  - ○ Add parity coverage between optional parallel solve and the existing single-threaded graph solve.


**Group 12 - Semantics/lowering boundary**
Boundary note: this group is now split into semantic-product creation, pipeline plumbing, lowering cutover, and cleanup so it can be worked incrementally instead of as one flat migration queue.

Semantic product creation:
- ◐ Implement the first semantic-product builder slice now that its scope is documented. Progress: the documented scope is now split into explicit fact-family slices so the initial publication pass can land incrementally without mixing unrelated lowering facts into one change.
  - ◐ Materialize resolved call targets and helper-vs-canonical path choices into the semantic product. Progress: this call-target surface is now split into explicit direct-call, receiver-call, and helper-routing slices so lowering-facing semantic publication can follow the existing resolution families instead of one broad target bucket.
- ◐ Implement the second semantic-product builder slice now that its scope is documented. Progress: the documented graph-backed snapshot surface is now split into explicit fact-family slices so test-only metadata can move over incrementally instead of as one broad builder cut.

Pipeline plumbing:
- ◐ Implement the CLI/runtime plumbing cutover for the semantic product now that the end-to-end handoff contract is documented. Progress: this plumbing cut is now split into explicit CLI, runtime/backend, and failure/report seams instead of one umbrella handoff item.
  - ✓ Thread the semantic product through `primec` CLI and dump-stage entrypoints. Completed: successful `primec` compile/emit entrypoints now thread the published semantic product through the shared IR preparation handoff, the `semantic-product` dump stage is available from both `primec` and `primevm`, and the CLI help text documents that lowering-facing stage between `ast-semantic` and `ir`.
  - ✓ Thread the semantic product through runtime/backend consumer entrypoints (`primevm`, backend dispatch, and related handoff glue). Completed: `primevm`, backend dispatch, and the shared runtime/backend handoff path now thread published semantic-product success artifacts through `prepareIrModule(...)` / `IrLowerer::lower(...)` instead of dropping back to raw AST-only lowering entrypoints.
  - ◐ Thread the semantic product through failure/report plumbing without creating parallel semantic state. Progress: compile-pipeline result plumbing now preserves semantic-product-aware failure objects on post-semantics failures, and the remaining work is limited to user-visible diagnostic ordering/reporting seams.
    - ✓ Thread semantic-product-aware dump/report plumbing without duplicating semantic state. Completed: the compile pipeline now emits `semantic-product` dump output directly from the already-published `CompilePipelineOutput.semanticProgram` surface instead of constructing any parallel semantic-report state.
    - ✓ Thread semantic-product-aware user-visible diagnostic ordering/reporting through the remaining CLI/runtime consumers. Completed: `CliDriver` now formats compile-pipeline failures directly from `CompilePipelineOutput.failure`, and post-semantics CLI diagnostics explicitly report when the semantic product remains available.
- ◐ Implement the temporary migration adapter now that its cutover/removal contract is documented. Progress: the remaining adapter work is now split into lowering-facing fact families instead of one umbrella shim item.
  - ◐ Implement the migration adapter for lowering-facing resolved target, binding, effect, and layout facts. Progress: this lowering-facing adapter is now split into direct target, binding/type, and effect/layout seams instead of one broad fact-family bucket.
    - ✓ Implement the migration adapter for resolved direct-call, receiver-call, and helper-routing target facts. Completed: lowerer entry/setup call resolution now snapshots semantic-product direct-call targets, receiver/method-call targets, and helper-routing choices into a temporary adapter and prefers those facts before any AST-side compatibility fallback.
    - ○ Implement the migration adapter for lowering-facing binding/result type facts.
    - ○ Implement the migration adapter for lowering-facing effect/capability and layout facts.
  - ◐ Implement the migration adapter for graph-backed query/local-`auto`/`try(...)`/`on_error` facts that are still read from legacy validator state. Progress: this graph-facing adapter is now split into per-fact-family seams so the remaining legacy reads can be retired incrementally.
    - ○ Implement the migration adapter for graph-backed local-`auto` facts.
    - ○ Implement the migration adapter for graph-backed query facts.
    - ○ Implement the migration adapter for graph-backed `try(...)` facts.
    - ○ Implement the migration adapter for graph-backed `on_error` facts.
- ◐ Implement the deterministic semantic-product dump/formatter plus golden coverage now that its inspection contract is documented. Progress: the inspection-surface work is now split into formatter and coverage slices instead of one mixed dump item.
  - ✓ Implement the deterministic semantic-product dump stage and text formatter. Completed: `semantic-product` is now a real dump stage for `primec` and `primevm`, backed by a deterministic text formatter over the published `SemanticProgram` fact inventories.
  - ✓ Add golden coverage for the semantic-product dump surface. Completed: the semantic-product formatter now has an exact text golden that pins the full lowering-facing dump layout and field spelling, while the existing `primec`/`primevm` dump-stage tests continue to lock the runtime dump entrypoints and alias behavior against that formatter surface.
- ◐ Implement ownership-split tests for source spans, debug/source-map provenance, and syntax-faithful data using the documented test matrix. Progress: the documented matrix is now split into explicit ownership/parity slices so provenance coverage can land incrementally.
  - ○ Add source-span parity tests for semantic-product entries that feed lowering-facing facts.
  - ○ Add debug/source-map provenance parity tests for semantic-product-backed lowering/debug entrypoints.
  - ○ Add syntax-reproduction boundary tests that keep syntax-owned data on AST dumps and lowering-owned data on the semantic-product surface.
  - ○ Add deterministic ordering tests for semantic-product ownership/provenance surfaces.

Lowering cutover:
- ◐ Implement the `prepareIrModule` / `IrLowerer::lower` entrypoint cutover now that the handoff contract is documented. Progress: this entrypoint cutover is now split into IR preparation, lowerer entry, and raw-`Program` retirement seams instead of one umbrella handoff bullet.
  - ○ Make `prepareIrModule` consume the semantic product directly instead of re-reading lowering facts from raw `Program` state.
  - ○ Make `IrLowerer::lower` consume the semantic product directly at its main lowering entrypoint.
  - ○ Retire the raw-`Program` lowering entry path once the temporary adapter is no longer needed.
- ◐ Implement the lowerer effect/capability and struct-layout setup cutover now that the handoff contract is documented. Progress: the remaining lowerer metadata handoff is now split into effect/capability facts and struct/layout facts instead of one umbrella setup item.
  - ◐ Consume semantic-product effect/capability summaries in lowerer setup. Progress: entry effect/capability masks now consume semantic-product callable summaries; remaining work is execution-boundary metadata that still re-derives effect/capability facts from AST state.
    - ○ Consume semantic-product execution-boundary metadata that still re-derives effect/capability facts from AST state.
  - ◐ Consume semantic-product struct/enum/layout metadata in lowerer setup. Progress: this metadata family is now split into struct layout, enum metadata, and helper-owned layout classification seams instead of one broad layout bucket.
    - ○ Consume semantic-product struct/layout metadata in lowerer setup.
    - ○ Consume semantic-product enum metadata in lowerer setup.
    - ○ Consume semantic-product helper-owned layout/classification metadata in lowerer setup.

Coverage and migration cleanup:
- ◐ Implement the narrow semantic-product unit/golden suite now that its fact coverage and scope are documented. Progress: the exact semantic-product formatter golden now covers resolved call/helper targets, binding/result type facts, and effect/capability plus struct/layout metadata; remaining work is provenance-handle coverage.
  - ○ Add narrow semantic-product golden coverage for provenance handles back to AST-owned ids/spans.
- ◐ Add end-to-end compile-pipeline conformance cases that exercise the semantic-product boundary through C++/VM/native lowering paths. Progress: this conformance surface is now split into per-backend lowering consumers instead of one umbrella pipeline item.
  - ○ Add end-to-end semantic-product boundary conformance for the C++ lowering path.
  - ○ Add end-to-end semantic-product boundary conformance for the VM lowering path.
  - ○ Add end-to-end semantic-product boundary conformance for the native lowering path.
- ◐ Migrate tests and public testing helpers from `primec/testing/SemanticsValidationHelpers.h` now that their boundary/migration contract is documented, moving lowering-facing assertions onto the semantic-product inspection surface. Progress: this migration is now split into explicit helper-surface and assertion-migration slices instead of one broad cleanup item.
  - ◐ Move lowering-facing assertions onto semantic-product dump helpers or pipeline-facing conformance helpers. Progress: this assertion migration is now split into dump-based, pipeline-based, and backend-facing assertion seams instead of one broad assertion bucket.
    - ○ Move lowering-facing dump assertions onto semantic-product dump helpers.
    - ○ Move lowering-facing compile-pipeline assertions onto semantic-product pipeline conformance helpers.
    - ○ Move lowering-facing backend-facing assertions onto semantic-product-aware backend conformance helpers.
  - ◐ Narrow `primec/testing/SemanticsValidationHelpers.h` to syntax-owned and provenance-owned assertions only. Progress: this narrowing work is now split into syntax-owned, provenance-owned, and transitional compatibility seams instead of one broad header-cleanup item.
    - ○ Keep only syntax-owned assertions in `primec/testing/SemanticsValidationHelpers.h`.
    - ○ Keep only provenance-owned assertions in `primec/testing/SemanticsValidationHelpers.h`.
    - ○ Delete transitional compatibility assertions from `primec/testing/SemanticsValidationHelpers.h` once replacements exist.
  - ◐ Delete redundant lowering-facing public testing helper entrypoints once the semantic-product replacements exist. Progress: this removal work is now split into dump-helper, pipeline-helper, and backend-helper entrypoint cleanup seams instead of one broad deletion item.
    - ○ Delete redundant lowering-facing dump helper entrypoints.
    - ○ Delete redundant lowering-facing pipeline helper entrypoints.
    - ○ Delete redundant lowering-facing backend helper entrypoints.
- ◐ Delete redundant testing-only semantic snapshot plumbing now that its migration/removal contract is documented, leaving one canonical lowering-facing inspection surface once equivalent semantic-product dumps and conformance coverage exist. Progress: this removal is now split into explicit snapshot-transport and compatibility-helper cleanup slices instead of one broad deletion item.
  - ◐ Delete redundant testing-only semantic snapshot transport/plumbing once no lowering-facing test depends on it. Progress: this transport cleanup is now split into snapshot capture, snapshot serialization, and snapshot fixture seams instead of one broad transport bucket.
    - ○ Delete redundant testing-only semantic snapshot capture plumbing.
    - ○ Delete redundant testing-only semantic snapshot serialization/plumbing.
    - ○ Delete redundant testing-only semantic snapshot fixture/loader plumbing.
  - ◐ Delete compatibility helpers that only existed to expose lowering facts before the semantic product surface existed. Progress: this compatibility cleanup is now split into dump-facing, pipeline-facing, and backend-facing helper seams instead of one broad helper bucket.
    - ○ Delete dump-facing compatibility helpers that only exposed lowering facts before the semantic-product dump existed.
    - ○ Delete pipeline-facing compatibility helpers that only exposed lowering facts before semantic-product pipeline conformance existed.
    - ○ Delete backend-facing compatibility helpers that only exposed lowering facts before semantic-product backend conformance existed.
- ◐ Delete lowerer-side stdlib/helper alias fallback paths now that their semantic-product removal contract is documented, once equivalent canonical resolution is provided by the semantic product. Progress: this alias-fallback cleanup is now split into direct-call, receiver-call, and collection bridge fallback seams instead of one umbrella removal item.
  - ○ Delete lowerer-side direct-call stdlib/helper alias fallback paths once semantic-product direct-call targets exist.
  - ○ Delete lowerer-side receiver/method-call stdlib/helper alias fallback paths once semantic-product receiver-call targets exist.
  - ○ Delete lowerer-side collection/builtin bridge alias fallback paths once semantic-product helper-routing facts exist.
- ◐ Implement pipeline-facing tests that exercise the semantic-product inspection surface and its relation to existing AST/IR dumps once that stage exists, now that the test matrix is documented. Progress: the documented conformance matrix is now split into explicit inspection-order and backend-consumer slices instead of one broad pipeline test item.
  - ○ Add pipeline-facing tests that pin inspection-surface order and consistency across `ast-semantic`, semantic-product dump, and `ir`.
  - ◐ Add pipeline-facing tests that pin lowering consumption of semantic-product facts across C++/VM/native. Progress: this backend-consumer coverage is now split into per-backend lowering consumers instead of one broad backend bucket.
    - ○ Add pipeline-facing tests that pin lowering consumption of semantic-product facts on the C++ backend.
    - ○ Add pipeline-facing tests that pin lowering consumption of semantic-product facts on the VM backend.
    - ○ Add pipeline-facing tests that pin lowering consumption of semantic-product facts on the native backend.
