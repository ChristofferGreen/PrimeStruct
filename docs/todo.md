# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started
  ✓ Finished

Finished items are periodically moved to `docs/todo_finished.md`.

**Types & Semantics**
Roadmap note: completed map/vector migration and compatibility-cleanup slices now live in `docs/todo_finished.md`. Keep this live file focused on unfinished ownership/runtime substrate work, SoA bring-up, and the remaining graph/boundary items.

**Group 8 - SoA de-builtinization**
- ◐ Extend the SoA storage substrate from the completed fixed-width single/two/three/four/five/six/seven/eight/nine/ten/eleven/twelve/thirteen/fourteen/fifteen-column primitives to reflected arbitrary-width schema allocation/grow/free. Progress: the remaining work is now split into explicit allocation, grow/realloc, and free/drop slices instead of one catch-all item.
  - ○ Add reflected arbitrary-width schema allocation on top of the fixed-width substrate and the existing deterministic pending-width gate.
  - ○ Add reflected arbitrary-width schema grow/realloc over that allocation path.
  - ○ Add reflected arbitrary-width schema free/drop cleanup over that allocation path.
- ◐ Extend the borrowed-view substrate from the completed single-column borrowed-slot foothold to language-level invalidation and richer borrowed field-view semantics after the current compiler-owned builtin `soa_vector` paths are retired. Progress: the remaining borrowed-view work is now split into explicit invalidation and borrowed field-view follow-ups instead of one broad parent item.
  - ○ Add language-level invalidation rules on top of the current single-column borrowed-slot substrate once the remaining compiler-owned builtin `soa_vector` paths are retired.
  - ○ Add richer borrowed field-view semantics on top of that substrate once the remaining compiler-owned builtin `soa_vector` paths are retired.
- ✓ Extend experimental stdlib `/std/collections/experimental_soa_vector/*` from the completed imported `to_aos` helper/method boundary to successful richer non-empty `to_aos` lowering. Completed: shared return inference now treats declared `vector<Struct>` helper returns as opaque array-handle returns instead of unsupported scalar returns, so imported `soaVectorToAos<T>()`, wrapper-method `values.to_aos()`, and direct canonical `/std/collections/soa_vector/to_aos<T>(...)` all lower successfully across C++/native/VM for both empty and non-empty experimental-wrapper state.
- ◐ Extend experimental stdlib `/std/collections/experimental_soa_vector/*` to field-view indexing (`value.field()[i]`) once the experimental `.prime` implementation reaches that boundary and the next minimal substrate slice is clear. Progress: direct semantic validation for experimental wrapper field-view attempts now fails with the same deterministic `soa_vector field views are not implemented yet: <field>` diagnostic as the builtin surface instead of leaking through `unknown call target`, and full compile-pipeline indexed reads now stop at that same deterministic pending diagnostic instead of degrading to the generic `at requires array, vector, map, or string target` boundary; the remaining successful field-view work is now split into an initial read-only single-column slice and the broader arbitrary-field/borrowed follow-up work.
  - ○ Add the first successful experimental wrapper field-view indexing slice for read-only access on top of the current single-column substrate.
  - ○ Extend experimental wrapper field-view indexing beyond that first slice to arbitrary reflected fields and borrowed/mutating field-view semantics.
  - ◐ Route wildcard-imported canonical `/std/collections/soa_vector/*` helper names (`count`, `get`, `ref`, `reserve`, `push`, and `to_aos`) through the experimental wrapper/conversion surfaces once bare canonical helper template inference reaches parity. Progress: bare wildcard-imported helper names still stop at `template arguments required for /std/collections/soa_vector/count`, and the remaining work is now tracked as explicit semantic-validation and compile-pipeline/backend follow-ups instead of pretending that path already works.
    - ○ Add semantic-validation parity for bare wildcard-imported canonical helper names once canonical helper template inference reaches parity.
    - ○ Route wildcard-imported canonical helper names through the full compile pipeline/backend path once that semantic-validation parity exists.
- ◐ Delete compiler-owned `soa_vector` helper routing and backend special cases from semantics, IR lowering, emitters, and runtime code until no C++ source mentions `soa_vector`. Progress: direct-call canonical `/std/collections/soa_vector/*` helper calls now stay on the canonical stdlib shim surface through `ast-semantic`/monomorphization instead of being rewritten immediately to `/std/collections/experimental_soa_vector/*`, wrong-receiver direct canonical helper misuse now keeps the canonical `/std/collections/soa_vector/*` reject contract instead of degrading into bare `/get` / `/ref` / `/push` style unknown-target errors, canonical slash-method `to_aos` attempts now also keep that same canonical unknown-method contract instead of being rewritten through the builtin conversion path, and the remaining compiler-owned `soa_vector` cleanup work is now split into explicit semantics/lowering/C++/native/VM/runtime follow-ups instead of one ambiguous catch-all bullet.
  - ◐ Delete the remaining builtin `soa_vector` fallback handling in semantics once the stdlib shim fully owns those call surfaces. Progress: the remaining builtin fallback work is now split into explicit root `get`, root `ref`, root conversion, and field-view follow-ups instead of one mixed bucket.
    - ○ Delete the remaining root builtin `get` fallback handling in semantics once equivalent stdlib-owned helper routing exists for the old `soa_vector<T>` surface.
    - ○ Delete the remaining root builtin `ref` fallback handling in semantics once equivalent stdlib-owned helper routing exists for the old `soa_vector<T>` surface.
    - ○ Delete the remaining root builtin `to_aos` conversion fallback handling in semantics once equivalent stdlib-owned conversion routing exists for the old `soa_vector<T>` surface.
    - ○ Delete the remaining builtin `soa_vector` field-view diagnostic fallback handling in semantics once field-view indexing moves fully onto the experimental/stdlib path.
  - ◐ Delete the remaining IR-lowerer `soa_vector` special cases once lowering consumes only the stdlib/experimental helper surfaces. Progress: the remaining lowering cleanup is now split into explicit helper-call, conversion, and field-view follow-ups instead of one broad item.
    - ◐ Delete the remaining IR-lowerer helper-call `soa_vector` special cases once helper calls lower only through stdlib/experimental surfaces. Progress: the remaining lowerer helper-call cleanup is now split into explicit direct-call and wildcard-imported follow-ups instead of one broad helper bucket.
      - ○ Delete the remaining IR-lowerer direct-call `soa_vector` helper special cases once direct canonical helper calls lower only through stdlib/experimental surfaces.
      - ○ Delete the remaining IR-lowerer wildcard-imported `soa_vector` helper special cases once bare canonical helper template inference reaches parity.
    - ◐ Delete the remaining IR-lowerer conversion `soa_vector` special cases once `to_aos` lowering consumes only stdlib/experimental helper surfaces. Progress: the remaining lowerer conversion cleanup is now split into explicit direct-canonical and imported-helper follow-ups instead of one broad conversion bucket.
      - ○ Delete the remaining IR-lowerer direct-canonical `soa_vector/to_aos` special cases once direct canonical conversion calls lower only through stdlib/experimental surfaces.
      - ○ Delete the remaining IR-lowerer imported-helper `soa_vector` conversion special cases once imported `to_aos` helper/method lowering consumes only stdlib/experimental surfaces.
    - ◐ Delete the remaining IR-lowerer field-view `soa_vector` special cases once field-view indexing lowers only through the experimental substrate. Progress: the remaining lowerer field-view cleanup is now split into explicit pending-diagnostic and successful-indexing follow-ups instead of one broad field-view bucket.
      - ○ Delete the remaining IR-lowerer successful field-view indexing special cases once read-only and richer field-view lowering run entirely through the experimental substrate.
  - ◐ Delete the remaining emitter/backend `soa_vector` special cases once C++/native/VM all route exclusively through stdlib helpers. Progress: the remaining backend cleanup is now split into explicit C++, native, and VM follow-ups instead of one combined item.
    - ◐ Delete the remaining C++ emitter `soa_vector` special cases once it routes exclusively through stdlib helpers. Progress: the remaining C++ backend cleanup is now split into explicit helper-call, conversion, and field-view follow-ups instead of one broad C++ item.
      - ◐ Delete the remaining C++ emitter helper-call `soa_vector` special cases once helper calls route exclusively through stdlib helpers. Progress: the remaining C++ helper cleanup is now split into explicit direct-call and wildcard-imported follow-ups instead of one broad helper bucket.
        - ○ Delete the remaining C++ emitter direct-call `soa_vector` helper special cases once direct canonical helper calls route exclusively through stdlib helpers.
        - ○ Delete the remaining C++ emitter wildcard-imported `soa_vector` helper special cases once bare canonical helper template inference reaches parity.
      - ◐ Delete the remaining C++ emitter conversion `soa_vector` special cases once `to_aos` routing is fully stdlib-owned. Progress: the remaining C++ conversion cleanup is now split into explicit direct-canonical and imported-helper follow-ups instead of one broad conversion bucket.
        - ○ Delete the remaining C++ emitter direct-canonical `soa_vector/to_aos` special cases once direct canonical conversion calls route exclusively through stdlib helpers.
        - ○ Delete the remaining C++ emitter imported-helper `soa_vector` conversion special cases once imported `to_aos` helper/method routing is fully stdlib-owned.
      - ◐ Delete the remaining C++ emitter field-view `soa_vector` special cases once field-view indexing routes through the experimental substrate. Progress: the remaining C++ field-view cleanup is now split into explicit pending-diagnostic and successful-indexing follow-ups instead of one broad field-view bucket.
        - ○ Delete the remaining C++ emitter successful field-view indexing special cases once read-only and richer field-view lowering run entirely through the experimental substrate.
    - ◐ Delete the remaining native-backend `soa_vector` special cases once it routes exclusively through stdlib helpers. Progress: the remaining native backend cleanup is now split into explicit helper-call, conversion, and field-view follow-ups instead of one broad native item.
      - ◐ Delete the remaining native helper-call `soa_vector` special cases once helper calls route exclusively through stdlib helpers. Progress: the remaining native helper cleanup is now split into explicit direct-call and wildcard-imported follow-ups instead of one broad helper bucket.
        - ○ Delete the remaining native direct-call `soa_vector` helper special cases once direct canonical helper calls route exclusively through stdlib helpers.
        - ○ Delete the remaining native wildcard-imported `soa_vector` helper special cases once bare canonical helper template inference reaches parity.
      - ◐ Delete the remaining native conversion `soa_vector` special cases once `to_aos` routing is fully stdlib-owned. Progress: the remaining native conversion cleanup is now split into explicit direct-canonical and imported-helper follow-ups instead of one broad conversion bucket.
        - ○ Delete the remaining native direct-canonical `soa_vector/to_aos` special cases once direct canonical conversion calls route exclusively through stdlib helpers.
        - ○ Delete the remaining native imported-helper `soa_vector` conversion special cases once imported `to_aos` helper/method routing is fully stdlib-owned.
      - ◐ Delete the remaining native field-view `soa_vector` special cases once field-view indexing routes through the experimental substrate. Progress: the remaining native field-view cleanup is now split into explicit pending-diagnostic and successful-indexing follow-ups instead of one broad field-view bucket.
        - ○ Delete the remaining native successful field-view indexing special cases once read-only and richer field-view lowering run entirely through the experimental substrate.
    - ◐ Delete the remaining VM-backend `soa_vector` special cases once it routes exclusively through stdlib helpers. Progress: the remaining VM backend cleanup is now split into explicit helper-call, conversion, and field-view follow-ups instead of one broad VM item.
      - ◐ Delete the remaining VM helper-call `soa_vector` special cases once helper calls route exclusively through stdlib helpers. Progress: the remaining VM helper cleanup is now split into explicit direct-call and wildcard-imported follow-ups instead of one broad helper bucket.
        - ○ Delete the remaining VM direct-call `soa_vector` helper special cases once direct canonical helper calls route exclusively through stdlib helpers.
        - ○ Delete the remaining VM wildcard-imported `soa_vector` helper special cases once bare canonical helper template inference reaches parity.
      - ◐ Delete the remaining VM conversion `soa_vector` special cases once `to_aos` routing is fully stdlib-owned. Progress: the remaining VM conversion cleanup is now split into explicit direct-canonical and imported-helper follow-ups instead of one broad conversion bucket.
        - ○ Delete the remaining VM direct-canonical `soa_vector/to_aos` special cases once direct canonical conversion calls route exclusively through stdlib helpers.
        - ○ Delete the remaining VM imported-helper `soa_vector` conversion special cases once imported `to_aos` helper/method routing is fully stdlib-owned.
      - ◐ Delete the remaining VM field-view `soa_vector` special cases once field-view indexing routes through the experimental substrate. Progress: the remaining VM field-view cleanup is now split into explicit pending-diagnostic and successful-indexing follow-ups instead of one broad field-view bucket.
        - ○ Delete the remaining VM successful field-view indexing special cases once read-only and richer field-view lowering run entirely through the experimental substrate.
  - ◐ Delete the remaining runtime/diagnostic `soa_vector` special cases and tests once no production C++ source mentions `soa_vector`. Progress: the remaining runtime cleanup is now split into explicit runtime-code and diagnostic/test follow-ups instead of one combined item, and both the runtime-code and diagnostic/test cleanup are now further split into helper-call, conversion, and field-view slices.
    - ◐ Delete the remaining runtime-code `soa_vector` special cases once no production C++ source mentions `soa_vector`. Progress: the remaining runtime-code cleanup is now split into explicit helper-call, conversion, and field-view follow-ups instead of one broad runtime-code item.
      - ◐ Delete the remaining runtime-code helper-call `soa_vector` special cases once helper calls route exclusively through stdlib helpers. Progress: the remaining runtime-code helper cleanup is now split into explicit direct-call and wildcard-imported follow-ups instead of one broad helper bucket.
        - ○ Delete the remaining runtime-code direct-call `soa_vector` helper special cases once direct canonical helper calls route exclusively through stdlib helpers.
        - ○ Delete the remaining runtime-code wildcard-imported `soa_vector` helper special cases once bare canonical helper template inference reaches parity.
      - ◐ Delete the remaining runtime-code conversion `soa_vector` special cases once `to_aos` routing is fully stdlib-owned. Progress: the remaining runtime-code conversion cleanup is now split into explicit direct-canonical and imported-helper follow-ups instead of one broad conversion bucket.
        - ○ Delete the remaining runtime-code direct-canonical `soa_vector/to_aos` special cases once direct canonical conversion calls route exclusively through stdlib helpers.
        - ○ Delete the remaining runtime-code imported-helper `soa_vector` conversion special cases once imported `to_aos` helper/method routing is fully stdlib-owned.
      - ◐ Delete the remaining runtime-code field-view `soa_vector` special cases once field-view indexing routes through the experimental substrate. Progress: the remaining runtime-code field-view cleanup is now split into explicit pending-diagnostic and successful-indexing follow-ups instead of one broad field-view bucket.
        - ○ Delete the remaining runtime-code field-view pending-diagnostic special cases once field-view attempts no longer depend on compiler-owned fallback plumbing.
        - ○ Delete the remaining runtime-code successful field-view indexing special cases once read-only and richer field-view lowering run entirely through the experimental substrate.
    - ◐ Delete the remaining diagnostic/test `soa_vector` special cases once runtime-code cleanup is complete. Progress: the remaining diagnostic/test cleanup is now split into explicit helper-call, conversion, and field-view follow-ups instead of one broad diagnostic/test item.
      - ◐ Delete the remaining diagnostic/test helper-call `soa_vector` special cases once helper-call runtime cleanup is complete. Progress: the remaining diagnostic/test helper cleanup is now split into explicit direct-call and wildcard-imported follow-ups instead of one broad helper bucket.
        - ○ Delete the remaining diagnostic/test direct-call `soa_vector` helper special cases once direct canonical helper runtime cleanup is complete.
        - ○ Delete the remaining diagnostic/test wildcard-imported `soa_vector` helper special cases once bare canonical helper template inference reaches parity.
      - ◐ Delete the remaining diagnostic/test conversion `soa_vector` special cases once conversion runtime cleanup is complete. Progress: the remaining diagnostic/test conversion cleanup is now split into explicit direct-canonical and imported-helper follow-ups instead of one broad conversion bucket.
        - ○ Delete the remaining diagnostic/test direct-canonical `soa_vector/to_aos` special cases once direct canonical conversion runtime cleanup is complete.
        - ○ Delete the remaining diagnostic/test imported-helper `soa_vector` conversion special cases once imported `to_aos` runtime cleanup is complete.
      - ◐ Delete the remaining diagnostic/test field-view `soa_vector` special cases once field-view runtime cleanup is complete. Progress: the remaining diagnostic/test field-view cleanup is now split into explicit pending-diagnostic and successful-indexing follow-ups instead of one broad field-view bucket, and the successful-indexing work remains isolated from the already-locked pending-diagnostic contract.
        - ○ Delete the remaining diagnostic/test successful field-view indexing special cases once read-only and richer field-view lowering run entirely through the experimental substrate.
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
    - ○ Materialize resolved direct-call targets and canonical callee paths into the semantic product.
    - ○ Materialize resolved receiver/method-call targets and receiver-side helper-family choices into the semantic product.
    - ○ Materialize helper-vs-canonical path choices for collection/builtin bridge surfaces into the semantic product.
  - ○ Materialize final binding/result type facts for parameters, locals, temporaries, and returns into the semantic product.
  - ○ Materialize effect/capability summaries needed by IR preparation into the semantic product.
  - ○ Materialize struct/enum/layout metadata already computed during semantic validation into the semantic product.
- ◐ Implement the second semantic-product builder slice now that its scope is documented. Progress: the documented graph-backed snapshot surface is now split into explicit fact-family slices so test-only metadata can move over incrementally instead of as one broad builder cut.
  - ○ Materialize graph-backed local `auto` facts into the semantic product.
  - ○ Materialize graph-backed query metadata into the semantic product.
  - ○ Materialize graph-backed `try(...)` metadata into the semantic product.
  - ○ Materialize graph-backed `on_error` metadata into the semantic product.

Pipeline plumbing:
- ◐ Implement the CLI/runtime plumbing cutover for the semantic product now that the end-to-end handoff contract is documented. Progress: this plumbing cut is now split into explicit CLI, runtime/backend, and failure/report seams instead of one umbrella handoff item.
  - ◐ Thread the semantic product through `primec` CLI and dump-stage entrypoints. Progress: this CLI surface is now split into parse/validate success handling, dump-stage selection, and CLI-facing reporting seams so the user-facing handoff can land incrementally.
    - ○ Thread semantic-product success results through `primec` compile/emit entrypoints.
    - ○ Thread semantic-product success results through `primec` dump-stage selection and emission entrypoints.
    - ○ Thread semantic-product-aware reporting through `primec` user-facing CLI diagnostics/help text where needed.
  - ◐ Thread the semantic product through runtime/backend consumer entrypoints (`primevm`, backend dispatch, and related handoff glue). Progress: this consumer surface is now split into `primevm`, backend registry/dispatch, and shared runtime/backend handoff seams.
    - ○ Thread semantic-product success results through `primevm` entrypoints.
    - ○ Thread semantic-product success results through backend registry/dispatch glue.
    - ○ Thread semantic-product success results through shared runtime/backend handoff glue used by both CLI consumers.
  - ◐ Thread the semantic product through failure/report plumbing without creating parallel semantic state. Progress: this reporting surface is now split into pipeline failure objects, dump/report surfaces, and user-visible diagnostic ordering/reporting seams.
    - ○ Thread semantic-product-aware failure objects through compile-pipeline result plumbing.
    - ○ Thread semantic-product-aware dump/report plumbing without duplicating semantic state.
    - ○ Thread semantic-product-aware user-visible diagnostic ordering/reporting through the remaining CLI/runtime consumers.
- ◐ Implement the temporary migration adapter now that its cutover/removal contract is documented. Progress: the remaining adapter work is now split into lowering-facing fact families instead of one umbrella shim item.
  - ◐ Implement the migration adapter for lowering-facing resolved target, binding, effect, and layout facts. Progress: this lowering-facing adapter is now split into direct target, binding/type, and effect/layout seams instead of one broad fact-family bucket.
    - ○ Implement the migration adapter for resolved direct-call, receiver-call, and helper-routing target facts.
    - ○ Implement the migration adapter for lowering-facing binding/result type facts.
    - ○ Implement the migration adapter for lowering-facing effect/capability and layout facts.
  - ◐ Implement the migration adapter for graph-backed query/local-`auto`/`try(...)`/`on_error` facts that are still read from legacy validator state. Progress: this graph-facing adapter is now split into per-fact-family seams so the remaining legacy reads can be retired incrementally.
    - ○ Implement the migration adapter for graph-backed local-`auto` facts.
    - ○ Implement the migration adapter for graph-backed query facts.
    - ○ Implement the migration adapter for graph-backed `try(...)` facts.
    - ○ Implement the migration adapter for graph-backed `on_error` facts.
- ◐ Implement semantic-product publication through `CompilePipelineOutput` and `Semantics::validate` now that the success-artifact contract is documented. Progress: this publication cut is now split into explicit producer and pipeline-surface slices instead of one mixed handoff item.
  - ○ Make `Semantics::validate` produce the semantic product as its canonical successful post-semantics result.
  - ○ Publish that semantic product on successful compile-pipeline results through `CompilePipelineOutput`.
- ◐ Implement the deterministic semantic-product dump/formatter plus golden coverage now that its inspection contract is documented. Progress: the inspection-surface work is now split into formatter and coverage slices instead of one mixed dump item.
  - ○ Implement the deterministic semantic-product dump stage and text formatter.
  - ○ Add golden coverage for the semantic-product dump surface.
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
- ◐ Implement the `IrLowerer` entry-setup cutover now that the handoff contract is documented. Progress: the lowering entry setup is now split into explicit direct-call target, receiver-call target, and helper-routing slices instead of one umbrella target handoff item.
  - ○ Consume semantic-product direct-call targets in lowerer entry setup instead of re-deriving canonical callees from AST state.
  - ○ Consume semantic-product receiver/method-call targets in lowerer entry setup instead of re-deriving them from AST state.
  - ○ Consume semantic-product helper-routing choices in lowerer entry setup instead of re-deriving helper-vs-canonical paths from AST state.
- ◐ Implement the lowerer type/binding setup cutover now that the handoff contract is documented. Progress: the lowerer binding cutover is now split into parameters/locals, temporaries/returns, and helper-owned binding metadata instead of one umbrella type handoff item.
  - ◐ Consume semantic-product parameter and local binding metadata in lowerer setup. Progress: this binding family is now split into parameter, local, and lowered entry-argument seams instead of one broad parameter/local bucket.
    - ○ Consume semantic-product parameter binding metadata in lowerer setup.
    - ○ Consume semantic-product local binding metadata in lowerer setup.
    - ○ Consume semantic-product lowered entry-argument binding metadata in lowerer setup.
  - ◐ Consume semantic-product temporary and return binding metadata in lowerer setup. Progress: this binding family is now split into temporaries, implicit helper temporaries, and returns instead of one broad temporary/return bucket.
    - ○ Consume semantic-product temporary binding metadata in lowerer setup.
    - ○ Consume semantic-product implicit helper-temporary metadata in lowerer setup.
    - ○ Consume semantic-product return binding/result metadata in lowerer setup.
  - ◐ Consume semantic-product helper-owned binding/type metadata in lowerer setup. Progress: this helper-owned family is now split into helper parameters, helper results, and helper-local synthetic metadata instead of one broad helper-owned bucket.
    - ○ Consume semantic-product helper parameter/type metadata in lowerer setup.
    - ○ Consume semantic-product helper result/type metadata in lowerer setup.
    - ○ Consume semantic-product helper-local synthetic binding/type metadata in lowerer setup.
- ◐ Implement the lowerer effect/capability and struct-layout setup cutover now that the handoff contract is documented. Progress: the remaining lowerer metadata handoff is now split into effect/capability facts and struct/layout facts instead of one umbrella setup item.
  - ◐ Consume semantic-product effect/capability summaries in lowerer setup. Progress: this metadata family is now split into effect summaries, capability summaries, and execution-boundary metadata instead of one broad effect/capability bucket.
    - ○ Consume semantic-product effect summaries in lowerer setup.
    - ○ Consume semantic-product capability summaries in lowerer setup.
    - ○ Consume semantic-product execution-boundary metadata that still re-derives effect/capability facts from AST state.
  - ◐ Consume semantic-product struct/enum/layout metadata in lowerer setup. Progress: this metadata family is now split into struct layout, enum metadata, and helper-owned layout classification seams instead of one broad layout bucket.
    - ○ Consume semantic-product struct/layout metadata in lowerer setup.
    - ○ Consume semantic-product enum metadata in lowerer setup.
    - ○ Consume semantic-product helper-owned layout/classification metadata in lowerer setup.

Coverage and migration cleanup:
- ◐ Implement the narrow semantic-product unit/golden suite now that its fact coverage and scope are documented. Progress: the documented fact families are now split into explicit suite slices so the semantic-product inspection surface can be pinned incrementally.
  - ○ Add narrow semantic-product golden coverage for resolved call/helper targets.
  - ○ Add narrow semantic-product golden coverage for binding/result type facts.
  - ○ Add narrow semantic-product golden coverage for effect/capability summaries and struct/layout metadata.
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
