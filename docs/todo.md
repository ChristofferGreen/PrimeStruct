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

Lowering cutover:
- ◐ Implement the `prepareIrModule` / `IrLowerer::lower` entrypoint cutover now that the handoff contract is documented. Progress: this entrypoint cutover is now split into IR preparation, lowerer entry, and raw-`Program` retirement seams instead of one umbrella handoff bullet.
  - ◐ Make `IrLowerer::lower` consume the semantic product directly at its main lowering entrypoint. Progress: this lowerer entry seam is now split into lowerer-wide validation/effect scans, import/layout setup, and helper/local setup instead of one broad cutover item.
    - ◐ Replace lowerer-wide entry/effect/runtime-reflection validation scans with semantic-product-backed entry and callable facts. Progress: top-level definition/execution effect validation now prefers semantic-product callable summaries, while the still-separate backend policy scans for software numeric and runtime-reflection rejection remain live.
      - ○ Decide and pin the long-term surface for native-backend software-numeric rejection scans.
      - ○ Decide and pin the long-term surface for native-backend runtime-reflection rejection scans.
    - ◐ Replace lowerer import/layout setup reads of raw `Program` state with semantic-product-backed inventories plus syntax-owned import aliases only. Progress: field ordering/type/envelope metadata, struct-like classification, explicit alignment, and struct-name inventory already come from the semantic product; the remaining live raw-`Program` seam is struct-layout iteration/provenance ownership. The lowerer-owned `Definition*` inventory that remains is explicitly pinned as an AST-owned provenance/body map, and the import-alias table is now explicitly pinned as a syntax-owned shorthand layer built from spelled `import` directives plus wildcard expansion rather than as semantic-product data.
      - ○ Replace struct-layout iteration over raw definitions with semantic-product-backed type inventories plus AST-owned provenance only.
    - ○ Replace lowerer helper/local setup reads of raw `Program` state with semantic-product-backed callable, binding, return, and `on_error` facts.
  - ○ Retire the raw-`Program` lowering entry path once the temporary adapter is no longer needed.

Coverage and migration cleanup:
- ◐ Add end-to-end compile-pipeline conformance cases that exercise the semantic-product boundary through C++/VM/native lowering paths. Progress: this conformance surface is now split into per-backend lowering consumers instead of one umbrella pipeline item.
  - ○ Add end-to-end semantic-product boundary conformance for the C++ lowering path.
  - ○ Add end-to-end semantic-product boundary conformance for the VM lowering path.
  - ○ Add end-to-end semantic-product boundary conformance for the native lowering path.
- ◐ Migrate tests and public testing helpers from `primec/testing/SemanticsValidationHelpers.h` now that their boundary/migration contract is documented, moving lowering-facing assertions onto the semantic-product inspection surface. Progress: this migration is now split into explicit helper-surface and assertion-migration slices instead of one broad cleanup item.
  - ◐ Move lowering-facing assertions onto semantic-product dump helpers or pipeline-facing conformance helpers. Progress: this assertion migration is now split into dump-based, pipeline-based, and backend-facing assertion seams instead of one broad assertion bucket.
    - ✓ Move lowering-facing dump assertions onto semantic-product dump helpers. Completed: lowering-facing semantic-product dump assertions now use `primec/testing/CompilePipelineDumpHelpers.h`; the remaining CLI dump tests only pin CLI alias/parity behavior while content assertions moved onto helper-driven compile-pipeline capture.
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
- ◐ Implement pipeline-facing tests that exercise the semantic-product inspection surface and its relation to existing AST/IR dumps once that stage exists, now that the test matrix is documented. Progress: inspection-order and consistency coverage now pins `ast-semantic`, `semantic-product`, and `ir` through the compile pipeline; remaining work is backend-consumer coverage per lowering path.
  - ◐ Add pipeline-facing tests that pin lowering consumption of semantic-product facts across C++/VM/native. Progress: this backend-consumer coverage is now split into per-backend lowering consumers instead of one broad backend bucket.
    - ○ Add pipeline-facing tests that pin lowering consumption of semantic-product facts on the C++ backend.
    - ○ Add pipeline-facing tests that pin lowering consumption of semantic-product facts on the VM backend.
    - ○ Add pipeline-facing tests that pin lowering consumption of semantic-product facts on the native backend.
