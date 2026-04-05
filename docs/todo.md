# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started
  ✓ Finished

Finished items are periodically moved to `docs/todo_finished.md`.

**Types & Semantics**
Roadmap note: completed map/vector migration and compatibility-cleanup slices now live in `docs/todo_finished.md`. Keep this live file focused on unfinished ownership/runtime substrate work, SoA bring-up, and the remaining graph/boundary items.

**Group 8 - SoA de-builtinization**
- ◐ Extend the borrowed-view substrate from the completed single-column borrowed-slot foothold to language-level invalidation and richer borrowed field-view semantics after the current compiler-owned builtin `soa_vector` paths are retired. Progress: the invalidation contract and richer borrowed field-view contract are now documented; the current completed foothold is indexed borrowed-slot read/write projection syntax, but those projections are recomputed per use through the existing helper rewrite/lowering path and do not yet materialize standalone borrowed field-view values, so the remaining live invalidation work starts with later standalone borrowed field-view surfaces plus their borrowed receiver families.
  - ✓ Materialize standalone borrowed-view values on top of the current single-column borrowed-slot substrate before the later invalidation rules can fire on anything persistent. Progress: the current completed foothold now includes direct whole-value `values.ref(i)` exposure through a standalone borrowed-view carrier across direct wrapper locals, borrowed locals, explicit dereference, borrowed helper-return/method-like helper-return receivers, inline `location(...)`-wrapped borrowed receivers, and the remaining local binding / pass-through / return surfaces. Projected `.ref(i).field` read/write forms still ride the existing per-use helper rewrite and stay with the later standalone borrowed field-view values queue.
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
    - ✓ Preserve standalone `ref(...)` values across local binding, pass-through, and return surfaces instead of collapsing them back into per-use projections. Completed: the shared canonical/experimental whole-value `ref(...)` carrier path now survives local `[Reference<T>]` binding, helper pass-through, and direct helper return without collapsing back into per-use projections; later `.ref(i).field` work stays with the standalone borrowed field-view queue.
  - ✓ Implement language-level invalidation rules on top of the borrowed-view substrate now that the borrowed-view contract is documented. Progress: current indexed borrowed-slot projection syntax (`ref(...).field`, `.ref(i).field`, and `value.field()[i]`-style reads/writes) is recomputed per use through the existing `soaVectorGet(...).field` / `soaVectorRef(...).field` rewrite path rather than materializing a standalone borrowed value, and standalone whole-value `ref(...)` carriers now invalidate correctly on `push` / `reserve` across direct local, direct borrowed/dereferenced receiver, helper-return, pass-through, and return-rooted carrier surfaces. Standalone borrowed field-view values do not exist yet and already inherit this same invalidation contract in the richer field-view design note, so the remaining live invalidation boundary is the later whole-value `ref(...)` lifetime/provenance work after those field-view surfaces land.
    - ✓ Invalidate standalone `ref(...)` values on structural growth surfaces (`push`, `reserve`) once those values exist. Completed: live whole-value `Reference<T>` carriers now reject later `push` / `reserve` across direct local, direct borrowed/dereferenced receiver, helper-return, pass-through, and return-rooted surfaces.
    - ✓ Invalidate standalone `ref(...)` values on shrink/motion helpers (`remove_*`, `clear`, and later size-changing helpers) once those helpers exist. Completed: standalone `Reference<T>` carriers now invalidate on `remove_at`, `remove_swap`, and `clear` for `soa_vector` receivers across direct, borrowed, helper-return, and inline `location(...)`-wrapped roots, matching the existing `push` / `reserve` growth rules.
    - ✓ Invalidate standalone `ref(...)` values on reallocating conversion and storage-replacement surfaces such as `to_aos`. Completed: standalone `Reference<T>` carriers now reject `to_aos` on the same wrapper across direct, borrowed, helper-return, and inline `location(...)`-wrapped roots.
    - ✓ Invalidate standalone `ref(...)` values on wrapper destruction and other end-of-owner lifetime boundaries.
      - ✓ Reject explicit `Destroy` / `DestroyStack` / `DestroyHeap` / `DestroyBuffer` calls on `soa_vector` and experimental `SoaVector` receivers while standalone `ref(...)` borrows are live.
      - ✓ Reject implicit owner drop / scope-exit with live standalone `ref(...)` borrows.
    - ✓ Enforce provenance/escape rules for standalone `ref(...)` and standalone borrowed field-view values reached through `location(...)`-wrapped receivers.
      - ✓ Apply borrow-conflict checks when binding standalone `ref(...)` values derived from location-wrapped receivers.
      - ✓ Require mutable roots when binding mutable standalone `ref(...)` values derived from location-wrapped receivers.
      - ✓ Require resolvable borrow roots when binding standalone `ref(...)` values derived from location-wrapped receivers.
      - ✓ Reject standalone `ref(...)` call-argument escapes derived from location-wrapped receivers.
      - ✓ Reject standalone field-view call-argument escapes derived from location-wrapped receivers.
      - ✓ Reject standalone `ref(...)` return escapes derived from location-wrapped receivers.
      - ✓ Reject standalone field-view return escapes derived from location-wrapped receivers.
      - ✓ Reject standalone field-view binding escapes derived from location-wrapped receivers.
      - ✓ Enforce full escape restrictions for standalone `ref(...)` and field-view values derived from location-wrapped receivers.
    - ✓ Enforce provenance/escape rules for standalone `ref(...)` and standalone borrowed field-view values reached through helper-return and method-like helper-return receivers.
      - ✓ Apply borrow-conflict checks when binding standalone `ref(...)` values derived from helper-return receivers.
      - ✓ Require mutable roots when binding mutable standalone `ref(...)` values derived from helper-return receivers.
      - ✓ Require resolvable borrow roots when binding standalone `ref(...)` values derived from helper-return receivers.
      - ✓ Reject standalone `ref(...)` call-argument escapes derived from helper-return and method-like helper-return receivers.
      - ✓ Reject standalone field-view call-argument escapes derived from helper-return and method-like helper-return receivers.
      - ✓ Reject standalone `ref(...)` return escapes derived from helper-return and method-like helper-return receivers.
      - ✓ Reject standalone field-view return escapes derived from helper-return and method-like helper-return receivers.
      - ✓ Reject standalone field-view binding escapes derived from helper-return and method-like helper-return receivers.
      - ✓ Enforce full escape restrictions for standalone `ref(...)` and field-view values derived from helper-return and method-like helper-return receivers.
- ✓ Extend experimental stdlib `/std/collections/experimental_soa_vector/*` to field-view indexing (`value.field()[i]`) once the experimental `.prime` implementation reaches that boundary and the next minimal substrate slice is clear. Completed: read-only reflected wrapper field indexing plus indexed/ref-field writes clear semantics/runtime through the existing `soaVectorGet<T>(..., i).field` and `soaVectorRef<T>(..., i).field` substrate across direct, borrowed, helper-return, method-like helper-return, and inline location-wrapped receiver families, while standalone `borrowed.field()` / `field(borrowed)` calls now return `SoaFieldView` carriers that can be bound, passed, returned, and written via the shared `soaFieldViewRead/Ref` helpers.
  - ✓ Implement the documented richer borrowed field-view semantics on top of the existing wrapper substrate, extending standalone field-view values beyond the current read-only method/helper coverage and indexed access. Completed: standalone `borrowed.field()` / `field(borrowed)` values now route through `soaVectorFieldView<...>` onto a non-owning `SoaFieldView` carrier for direct, borrowed, helper-return, method-like, and inline `location(...)`-wrapped receivers; bound/passed/returned field-view values preserve borrowed semantics by rewriting indexed access through `soaFieldViewRead/Ref`; and standalone method-form/call-form `assign(value.field(), next)` writes lower through `soaFieldViewRef<T>(..., 0)` dereference writes on that same substrate.
    - ✓ Publish validated reflected field byte offsets for reflect-enabled structs as reusable layout facts. Completed: the reflection metadata context now computes validated non-static field byte offsets for struct layouts and reuses them during `SoaSchema` helper generation.
    - ✓ Publish reflected whole-element storage stride for reflect-enabled structs as reusable layout facts. Completed: the reflection metadata context now computes validated whole-element stride bytes for struct layouts and reuses them during `SoaSchema` helper generation.
    - ✓ Emit generated `SoaSchemaFieldOffset(...)`-style helpers on top of those reflected layout facts. Completed: `generate(SoaSchema)` now emits deterministic indexed `SoaSchemaFieldOffset(...)` helpers alongside the existing name/type/visibility/chunk helpers.
    - ✓ Emit generated `SoaSchemaElementStride()`-style helpers on top of those reflected layout facts. Completed: `generate(SoaSchema)` now emits `SoaSchemaElementStride()` on top of the shared reflected stride fact.
    - ✓ Expose a low-level pointer reinterpret primitive that can cast whole-element storage pointers to raw byte-addressable pointers without changing the address. Completed: `/std/intrinsics/memory/reinterpret<T>(ptr)` now provides an address-preserving pointer cast and the experimental buffer helpers expose `bufferReinterpret<T, U>(...)` wrappers.
    - ✓ Expose typed reinterpretation from whole-element storage pointers to raw byte-addressable pointers on top of that primitive so reflected field byte offsets can start from a real byte base rather than an already-typed element pointer. Completed: the experimental buffer helpers now provide `bufferReinterpretBytes<T>(...)` to reinterpret typed storage pointers as raw byte-addressable `Pointer<i32>` bases.
    - ✓ Expose byte-addressable pointer offsetting over that raw storage so a byte-addressed slot can be recovered from a base element slot and reflected field byte offset. Completed: the experimental buffer helpers now provide `bufferOffsetBytesChecked(...)` / `bufferOffsetBytesUnsafe(...)` to advance raw byte-addressed pointers by explicit byte offsets.
    - ✓ Expose typed reinterpretation from that recovered byte-addressed slot to a field pointer so one named field can be retyped without pretending the base storage is already `Pointer<uninitialized<Field>>`. Completed: the experimental buffer helpers now provide `bufferReinterpretFromBytes<T>(...)` to cast byte-addressed slots back into typed pointers.
    - ✓ Introduce a reusable reflected field-slot pointer helper on top of those offset/stride helpers plus byte-addressable offset/reinterpretation primitives so one named field can be addressed as a pointer into whole-element `SoaColumn<T>` storage. Completed: `soaColumnFieldSlotUnsafe<Struct, Field>(...)` now uses `SoaSchemaFieldOffset(...)`, `SoaSchemaElementStride()`, and byte-addressable buffer helpers to recover a typed field pointer within whole-element `SoaColumn<Struct>` storage.
    - ✓ Introduce a reusable non-owning strided field-view carrier on top of that field-slot pointer helper so one named field can project as a standalone indexed borrowed view without pretending it is plain contiguous `SoaColumn<Field>` storage. Completed: `SoaFieldView<T>` and `soaFieldView*` helpers now model strided borrowed views over field storage.
    - ✓ Route the shared synthetic `/soa_vector/field_view/<field>` helper path onto that strided field-view carrier instead of the current pending diagnostic. Completed: standalone field-view calls now rewrite onto `soaVectorFieldView<...>` which returns the strided `SoaFieldView` carrier.
    - ✓ Preserve borrowed field-view semantics across local binding, pass-through, and return surfaces instead of materializing owning vector values. Completed: bound/passed/returned `SoaFieldView<T>` values now rewrite indexed access through the strided `soaFieldViewRead/Ref` helpers.
**Architecture / Type-resolution graph**
**Group 11 - Near-term graph queue**
Blocked by Group 13 rollout constraints until the remaining collection-helper/runtime predecessor items are finished and the return-kind pilot path is stable enough to resume broader graph work.
- ◐ Expand positive end-to-end graph conformance coverage for backend dispatch on C++/VM/native once the still-open query-shaped canonical helper/method resolution path moves past the current compile-pipeline `if branches must return compatible types` boundary. Progress: this backend-dispatch umbrella is now split into explicit compile-pipeline boundary lifts plus per-backend dispatch slices, so graph conformance can land incrementally once query-shaped canonical helper resolution clears the current branch-compatibility failure.
  - ○ Add positive compile-pipeline parity for query-shaped canonical helper direct-call forms once they move past the current `if branches must return compatible types` boundary.
    - ○ Identify the direct-call helper/query path that crosses the branch-compatibility boundary.
    - ○ Add compile-pipeline parity coverage for that direct-call helper/query path.
  - ○ Add positive compile-pipeline parity for query-shaped canonical helper slash-method forms once they move past the current `if branches must return compatible types` boundary.
    - ○ Identify the slash-method helper/query path that crosses the branch-compatibility boundary.
    - ○ Add compile-pipeline parity coverage for that slash-method helper/query path.
  - ○ Add C++ backend dispatch conformance for the query-shaped canonical helper/method path once compile-pipeline parity is positive.
    - ○ Select the C++ backend fixture that exercises the helper/method query path.
    - ○ Add backend conformance coverage for that fixture.
  - ○ Add VM backend dispatch conformance for the query-shaped canonical helper/method path once compile-pipeline parity is positive.
    - ○ Select the VM backend fixture that exercises the helper/method query path.
    - ○ Add backend conformance coverage for that fixture.
  - ○ Add native backend dispatch conformance for the query-shaped canonical helper/method path once compile-pipeline parity is positive.
    - ○ Select the native backend fixture that exercises the helper/method query path.
    - ○ Add backend conformance coverage for that fixture.
- ◐ Implement the next non-template inference-island migrations now that the graph-backed cutover contract is documented. Progress: the remaining non-template cutover is now split into direct-call, receiver-call, and collection bridge inference seams instead of one umbrella migration bullet.
  - ○ Migrate the next direct-call/callee non-template inference island onto the graph path.
    - ○ Select the next direct-call inference island to move onto the graph path.
    - ○ Land the migration plus matching graph conformance coverage for that island.
  - ○ Migrate the next receiver/method-call non-template inference island onto the graph path.
    - ○ Select the next receiver/method-call inference island to move onto the graph path.
    - ○ Land the migration plus matching graph conformance coverage for that island.
  - ○ Migrate the next collection helper/access fallback non-template inference island onto the graph path.
    - ○ Select the next collection helper/access fallback inference island to move onto the graph path.
    - ○ Land the migration plus matching graph conformance coverage for that island.
- ◐ Implement the graph/CT-eval interaction contract now that the boundary is documented. Progress: the remaining work is now split into shared dependency-state consumption, explicit adapter seams, and conformance coverage instead of one broad CT-eval handoff item.
  - ○ Route graph-backed consumers onto shared dependency-state consumption where CT-eval and graph validation overlap.
    - ○ Identify the next shared dependency state consumed by both CT-eval and the graph.
    - ○ Move that dependency state onto the shared graph-backed surface.
  - ○ Add one explicit adapter for the remaining CT-eval-only paths that still cannot consume shared graph dependency state directly.
    - ○ Select the CT-eval-only dependency state that needs an adapter.
    - ○ Implement the adapter and wire it into the graph/CT-eval boundary.
  - ○ Add conformance coverage for the graph/CT-eval handoff boundary.
    - ○ Add a positive conformance fixture that exercises the CT-eval adapter seam.
    - ○ Add a negative diagnostic fixture that proves the adapter boundary rejects mismatches.
- ◐ Implement the graph-backed explicit/implicit template-inference migrations now that the cutover contract is documented. Progress: the remaining template cutover is now split into explicit template-argument, implicit inference, and revalidation/monomorph follow-up slices instead of one umbrella migration bullet.
  - ○ Migrate the next explicit template-argument inference surface onto the graph path.
    - ○ Select the explicit template-argument inference surface to migrate.
    - ○ Land the migration plus matching graph conformance coverage for that surface.
  - ○ Migrate the next implicit template-inference surface onto the graph path.
    - ○ Select the implicit template-inference surface to migrate.
    - ○ Land the migration plus matching graph conformance coverage for that surface.
  - ○ Migrate the matching revalidation/monomorph follow-up for those template-inference surfaces onto the graph path.
    - ○ Identify the revalidation/monomorph follow-up needed for the migrated surfaces.
    - ○ Land the follow-up and conformance coverage.
- ◐ Implement the next omitted-envelope and local-`auto` graph expansions now that the widening contract is documented. Progress: the widening work is now split into omitted-envelope facts, new initializer families, and control-flow join widening slices instead of one umbrella expansion bullet.
  - ○ Materialize omitted-envelope graph facts for the next widening slice.
    - ○ Select the next omitted-envelope family to model in the graph (proposed: omitted struct initializer envelopes).
    - ○ Add graph facts and coverage for that omitted-envelope family.
  - ○ Expand local-`auto` graph support across the next initializer-family surface.
    - ○ Select the next initializer-family surface to widen in the graph (proposed: `if`-branch local `auto` binds).
    - ○ Land the widening plus matching coverage for that initializer-family surface.
  - ○ Expand local-`auto` graph support across the next control-flow join surface.
    - ○ Select the next control-flow join surface to widen in the graph (proposed: `if` join return inference).
    - ○ Land the widening plus matching coverage for that join surface.
- ✓ Implement graph performance guardrails and sustained perf coverage now that the regression-budget contract is documented. Completed: baseline metrics, regression thresholds/reporting, and sustained perf coverage are now in place.
- ✓ Add baseline graph timing and invalidation metrics for the current stabilized graph surface. Completed: the type-resolution graph dump now includes prepare/build timing, node/edge counts, per-kind node/edge totals, SCC counts, and invalidation fan-out counters, while the graph snapshot helper now surfaces prepare/build timing, node/edge totals, and the same invalidation counters.
  - ✓ Add regression thresholds and reporting for those graph metrics. Completed: type-graph dumps now report optional prepare/build budget caps and over-budget flags via `PRIMESTRUCT_GRAPH_PREPARE_MS_MAX` and `PRIMESTRUCT_GRAPH_BUILD_MS_MAX`, and the graph snapshot helper now surfaces the same budget/over-budget data.
- ✓ Add sustained graph performance coverage over the stabilized graph surface. Completed: baseline timing/budget metrics now exist; a lightweight graph perf snapshot test records prepare/build timings; budgeted prep/build coverage is wired; and a long-running graph perf soak is available behind `PRIMESTRUCT_GRAPH_SOAK`.
    - ✓ Add a lightweight graph perf snapshot test that records prepare/build timings for a fixed corpus.
    - ✓ Add budgeted perf coverage for graph prep/build on a representative compile-pipeline fixture.
    - ✓ Add a long-running graph perf soak (disabled by default) for larger program suites.
- ✓ Prototype optional parallel solve now that its execution and merge contract is documented. Completed: a first partition/execution layer now groups condensation DAG components into stable layers with deterministic merge ordering, and parity coverage asserts the layer-order toggle matches the default solve.
