# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started
  ✓ Finished

Finished items are periodically moved to `docs/todo_finished.md`.

**Types & Semantics**
Roadmap note: completed map/vector migration, compatibility-cleanup, graph queue, and semantics/lowering boundary slices now live in `docs/todo_finished.md`. This live file now tracks the next unfinished ownership/runtime substrate and SoA bring-up queues.
Sizing note: each leaf `○` item should fit in one code-affecting commit plus focused conformance updates for that slice. If a leaf needs multiple behavior changes, split it first.

- ✓ Seed the next live ownership/runtime and SoA bring-up queues from unresolved spec notes (April 8, 2026).
  - ✓ Inventory unresolved ownership/runtime migration notes in `docs/PrimeStruct.md` (vector/map constructor migration and builtin map growth routing follow-up).
  - ✓ Inventory unresolved SoA bring-up notes in `docs/PrimeStruct.md` (remaining compiler-owned builtin scaffolding and non-empty literal runtime gap).
  - ✓ Publish decomposed live checklist items below.

**Group 13 - Ownership/runtime substrate**
- ◐ Move the canonical imported vector constructor surface from fixed-arity wrapper helpers to variadic `vector(values...)`.
  - ✓ Inventory remaining fixed-arity vector constructor wrappers and callsites (see `docs/ownership_runtime_soa_touchpoints.md`, section 1).
  - ✓ Route canonical imported constructor calls and literal rewrite targets to the variadic helper surface (`stdlib/std/collections/vector.prime` now forwards canonical constructor overloads to `/std/collections/experimental_vector/vector<T>(...)`).
  - ✓ Add focused conformance coverage for constructor/literal parity across direct and imported forms (`tests/unit/test_compile_run_native_backend_map_and_vector_variadics.h`: `native keeps vector constructor and literal parity across direct and canonical forms`).
  - ○ Delete the replaced fixed-arity wrapper constructor surface after parity is locked.
- ◐ Move the canonical imported map constructor surface from fixed-arity wrapper helpers to variadic `map(entry(...), ...)`.
  - ✓ Inventory remaining fixed-arity map constructor wrappers and callsites (see `docs/ownership_runtime_soa_touchpoints.md`, section 2).
  - ✓ Route canonical imported constructor calls and literal rewrite targets to the variadic entry-based helper surface (`stdlib/std/collections/map.prime` canonical constructor overloads now build `/std/collections/experimental_map/entry<K, V>(...)` tuples and forward to variadic `/std/collections/experimental_map/map<K, V>(...)`).
  - ✓ Add focused conformance coverage for constructor/literal parity across direct and imported forms (`tests/unit/test_compile_run_native_backend_map_and_vector_variadics.h`: `native keeps map constructor and literal parity across direct and canonical forms`).
  - ○ Delete the replaced fixed-arity wrapper constructor surface after parity is locked.
- ○ Route the remaining builtin canonical `map<K, V>` borrowed/non-local growth mutation surfaces through the shared grown-pointer write-back/repoint path.
  - ✓ Inventory each remaining borrowed/non-local mutation receiver family still pinned to the pending runtime boundary (see `docs/ownership_runtime_soa_touchpoints.md`, section 3).
  - ○ Migrate one receiver family at a time onto the shared grown-pointer write-back/repoint primitive.
  - ○ Add focused runtime + conformance coverage that locks growth behavior and deterministic failure diagnostics.
  - ○ Remove the migrated pending runtime fallback branches after coverage lands.

**Group 14 - SoA bring-up and end-state cleanup**
- ○ Retire remaining compiler-owned builtin `soa_vector` semantics/lowering/backend scaffolding as the stdlib `.prime` substrate becomes authoritative.
  - ✓ Inventory the remaining compiler-owned fallback/special-case sites by layer (semantics, lowerer, emitter/backend/runtime helpers) (see `docs/ownership_runtime_soa_touchpoints.md`, section 4).
  - ○ Migrate one fallback family at a time onto shared stdlib helper/conversion paths.
  - ○ Add focused parity coverage for direct/imported/helper-return receiver forms on each migrated family.
  - ○ Delete migrated fallback branches after parity coverage is in place.
- ○ Replace the current non-empty `soa_vector` literal runtime rejection with a deterministic substrate-backed lowering/runtime path.
  - ○ Route non-empty literal lowering through the current stdlib `soa_vector` helper substrate instead of the direct unsupported diagnostic path.
  - ○ Add runtime coverage for successful non-empty literal materialization on supported backends.
  - ○ Add deterministic diagnostic coverage for unsupported element envelopes and allocation-failure boundaries.
