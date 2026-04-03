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
- ◐ Implement container element drop behavior in vector/map runtimes once the ownership contract is fixed. Progress: experimental `.prime` `Map<K, V>` destruction now reuses the experimental-vector drop/free steps for its key/payload storage so owned map buffers and element drops clean up on destroy, canonical `/std/collections/vector/*` `pop`/`clear` plus indexed-removal helpers now accept ownership-sensitive element types on explicit experimental `Vector<T>` bindings because they forward onto the experimental `.prime` discard/compaction implementation, canonical `/std/collections/map/insert` now forwards explicit experimental `Map<K, V>` bindings onto the `.prime` overwrite/growth path so ownership-sensitive value updates run through the same drop/reinit flow, canonical `/std/collections/map/insert_ref` now forwards borrowed `Reference<Map<K, V>>` bindings onto that same overwrite/growth path, explicit experimental `Map<K, V>` value method sugar now resolves through the `.prime` `/Map/*` method path so ownership-sensitive read/update flows stay on the same runtime drop/overwrite path, borrowed `Reference<Map<K, V>>` method sugar now resolves through the `.prime` borrowed-helper path so ownership-sensitive experimental-map overwrite/drop flows work through borrowed references, and builtin canonical `map<K, V>` inserts now perform real in-place overwrite when the numeric key already exists and grow through one generic arbitrary-`n` grow/copy/repoint path for owning local numeric maps without the old dead count-by-count lowerer staircase. The remaining live ownership/drop work is now split into explicit builtin vector indexed-removal destruction/compaction and wider builtin-map growth/drop slices.
  - ○ Implement builtin `vector` indexed-removal removed-element destruction so `remove_at` / `remove_swap` can stop rejecting ownership-sensitive elements solely because removed values are never explicitly dropped on the fixed-capacity runtime path.
  - ○ Implement builtin `vector` indexed-removal survivor compaction/swap semantics so `remove_at` / `remove_swap` can stop requiring relocation-trivial elements on the fixed-capacity runtime path.
  - ○ Extend builtin canonical `map<K, V>` growth/drop beyond the current owning local numeric-map path once the remaining ownership-sensitive runtime substrate is defined.


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
- ○ Implement the `prepareIrModule` / `IrLowerer::lower` entrypoint cutover now that the handoff contract is documented, so IR preparation consumes the semantic product directly and the raw-`Program` lowering path can be retired once the temporary adapter is removed.
- ○ Implement the `IrLowerer` entry-setup cutover now that the handoff contract is documented, consuming resolved call targets from the semantic product instead of re-deriving them from AST state.
- ○ Implement the lowerer type/binding setup cutover now that the handoff contract is documented, consuming semantic-product binding metadata instead of repeating helper/type inference logic.
- ○ Implement the lowerer effect/capability and struct-layout setup cutover now that the handoff contract is documented, consuming semantic-product metadata instead of re-reading AST/transforms for those facts.

Coverage and migration cleanup:
- ○ Implement the narrow semantic-product unit/golden suite now that its fact coverage and scope are documented.
- ○ Add end-to-end compile-pipeline conformance cases that exercise the semantic-product boundary through C++/VM/native lowering paths.
- ○ Migrate tests and public testing helpers from `primec/testing/SemanticsValidationHelpers.h` now that their boundary/migration contract is documented, moving lowering-facing assertions onto the semantic-product inspection surface.
- ○ Delete redundant testing-only semantic snapshot plumbing now that its migration/removal contract is documented, leaving one canonical lowering-facing inspection surface once equivalent semantic-product dumps and conformance coverage exist.
- ○ Delete lowerer-side stdlib/helper alias fallback paths now that their semantic-product removal contract is documented, once equivalent canonical resolution is provided by the semantic product.
- ○ Implement pipeline-facing tests that exercise the semantic-product inspection surface and its relation to existing AST/IR dumps once that stage exists, now that the test matrix is documented.
