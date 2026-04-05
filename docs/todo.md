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
  - ◐ Materialize standalone borrowed-view values on top of the current single-column borrowed-slot substrate before the later invalidation rules can fire on anything persistent. Progress: the current completed foothold now includes direct whole-value `values.ref(i)` exposure through a standalone borrowed-view carrier across direct wrapper locals, borrowed locals, explicit dereference, borrowed helper-return/method-like helper-return receivers, and inline `location(...)`-wrapped borrowed receivers. Projected `.ref(i).field` read/write forms still ride the existing per-use helper rewrite and stay with the later standalone borrowed field-view values queue, so the remaining live work here is preserving standalone `ref(...)` values across local binding, pass-through, and return surfaces instead of collapsing them back into per-use projections.
    - ✓ Implement standalone `ref(...)` values for direct borrowed locals, explicitly dereferenced borrowed receivers, borrowed helper-return/method-like struct-helper-return receivers, and inline `location(...)`-wrapped borrowed receivers. Completed: slot-borrow helpers, public `soaColumnRef<T>(...)`, experimental helper `soaVectorRef<T>(...)`, and the shared canonical/experimental whole-value `ref(...)` helper-method route now all validate through `Reference<T>` carriers across those receiver families; projected `.ref(i).field` reads/writes remain in the later standalone borrowed field-view queue.
      - ✓ Introduce a standalone borrowed element-view carrier for experimental-wrapper `ref(...)` instead of returning whole-element `T`. Completed: slot-borrow helpers, public `soaColumnRef<T>(...)`, experimental helper `soaVectorRef<T>(...)`, and the shared canonical/experimental whole-value `ref(...)` helper-method route now all validate through `Reference<T>` carriers; projected `.ref(i).field` reads/writes remain in the later standalone borrowed field-view queue.
        - ✓ Add a reusable slot-borrow borrowed-value carrier so `borrow(dereference(slot))` no longer collapses back to whole-element `T`. Completed: direct `borrow(storage)` and pointer/reference-backed `borrow(dereference(slot))` now validate for standalone `[Reference<T>]` bindings over local and field storage, including block/if binding sites, and the stdlib helper exposure slices now validate through `Reference<T>` as well.
          - ✓ Introduce a language-level standalone borrowed-value carrier for direct `borrow(storage)` before extending it to `borrow(dereference(slot))`. Completed: standalone borrowed-value typing now covers both direct local/field storage and pointer/reference-backed `dereference(slot)` storage for regular bindings plus block/if binding sites.
            - ✓ Introduce direct-storage standalone borrowed-value typing so `[Reference<T>] ref{borrow(storage)}` no longer requires `location(...)`.
            - ✓ Extend that standalone borrowed-value typing to `borrow(dereference(slot))` over pointer/reference-backed storage.
          - ✓ Route `soaColumnBorrowSlot<T>(...)` and `vectorBorrowSlot<T>(...)` onto that standalone borrowed-value carrier instead of `[return<T>]`. Completed: helper `return<Reference<T>>` now accepts parameter-rooted borrow carriers, helper-returned slot pointers preserve borrowed-root provenance through local `slot` aliases, and the internal stdlib slot-borrow helpers now validate through `[return<Reference<T>>]` while public `soaColumnRef<T>(...)`, `soaColumnRead<T>(...)`, `vectorAt<T>(...)`, and `vectorAtUnsafe<T>(...)` still intentionally dereference back to whole-element `T`.
            - ✓ Allow helper `return<Reference<T>>` contracts to return borrowed-value carrier expressions rooted in parameter-owned storage.
            - ✓ Preserve borrowed-root provenance through helper-returned slot pointers such as `soaColumnSlotUnsafe<T>(...)` and `vectorSlotUnsafe<T>(...)`.
            - ✓ Route `soaColumnBorrowSlot<T>(...)` and `vectorBorrowSlot<T>(...)` helper contracts onto that standalone borrowed-value carrier instead of `[return<T>]`.
        - ✓ Route public `soaColumnRef<T>(...)` onto a standalone borrowed element-view carrier instead of dereferencing `soaColumnBorrowSlot<T>(...)`. Completed: `soaColumnRef<T>(...)` now validates through `[public return<Reference<T>>]`, and direct semantics/C++/native/VM coverage now binds and dereferences that standalone borrowed carrier explicitly instead of collapsing back to whole-element `T`.
        - ✓ Route experimental helper `soaVectorRef<T>(...)` onto `soaColumnRef<T>(...)`'s standalone borrowed element-view carrier instead of returning whole-element `T`. Completed: direct helper calls now validate through `[public return<Reference<T>>]`, while wrapper `SoaVector<T>.ref(i)` and canonical `/std/collections/soa_vector/ref` still intentionally dereference that helper back to whole-element `T`.
        - ✓ Route canonical `/std/collections/soa_vector/ref` plus same-path `/soa_vector/ref` whole-value helper/method sugar onto `soaVectorRef<T>(...)`'s standalone borrowed element-view carrier instead of returning whole-element `T`. Completed: canonical helper `ref(values, i)` and experimental-wrapper `values.ref(i)` now both validate through `Reference<T>` carriers instead of collapsing back to whole-element `T`, while projected `.ref(i).field` reads/writes stay with the later standalone borrowed field-view queue.
    - ○ Preserve standalone `ref(...)` values across local binding, pass-through, and return surfaces instead of collapsing them back into per-use projections.
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
- ✓ Implement the `prepareIrModule` / `IrLowerer::lower` entrypoint cutover now that the handoff contract is documented. Completed: production entrypoints now pass the semantic product into `prepareIrModule(...)` and the main `IrLowerer::lower(...)` path; lowerer-wide effect validation, import/layout setup, and helper/local setup now prefer semantic-product-backed lowering facts, and the legacy raw-`Program` compatibility overload is now retired under helper-surface cleanup.

Coverage and migration cleanup:
- ✓ Migrate tests and public testing helpers from `primec/testing/SemanticsValidationHelpers.h` now that their boundary/migration contract is documented, moving lowering-facing assertions onto the semantic-product inspection surface. Completed: lowering-facing dump, compile-pipeline, and backend-facing assertions now route through semantic-product-aware helper surfaces, graph/type-resolution testing snapshots now live in `primec/testing/SemanticsGraphHelpers.h`, `primec/testing/SemanticsValidationHelpers.h` is reduced to syntax-owned canonicalization/assertion helpers, the legacy raw-`Program` `IrLowerer::lower(...)` compatibility overload plus its final fallback parity case are gone, the public prepared-IR pipeline helper is internalized, no redundant public backend helper entrypoints or backend-facing compatibility shims remain, and the testing-only semantic snapshot transport layer is fully deleted.
