# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started

Finished items are periodically moved to `docs/todo_finished.md`.

**Types & Semantics**
Roadmap note: completed map/vector migration, compatibility-cleanup, graph queue, and semantics/lowering boundary slices now live in `docs/todo_finished.md`.
Sizing note: each leaf `○` item should fit in one code-affecting commit plus focused conformance updates for that slice. If a leaf needs multiple behavior changes, split it first.

**Group 14 - SoA bring-up and end-state cleanup**
- ◐ Retire remaining compiler-owned builtin `soa_vector` semantics/lowering/backend scaffolding as the stdlib `.prime` substrate becomes authoritative.
  - ◐ Migrate the next compiler-owned SoA fallback family onto shared stdlib helper/conversion paths.
    - ◐ Refresh section 4 of `docs/ownership_runtime_soa_touchpoints.md` with concrete still-unhandled compiler-owned SoA fallback families (one entry per family with helper shape + owning files). Progress: section 4 now includes an explicit 2026-04 still-unhandled-family inventory (S1/S2/S3) covering struct-only non-empty `soa_vector` literal lowering, compiler-owned `to_aos` storage-layout bridge glue, and pending field-view diagnostic fallback paths with owning files.
    - ◐ Add one failing focused semantics/backend parity test for the highest-priority unhandled SoA fallback family. Progress: added a focused compile-run parity reject that locks root non-struct non-empty `soa_vector` literal behavior across semantic dump (`--dump-stage ast-semantic`) and emit (`--emit=exe`) paths with matching semantic-stage diagnostics.
    - ◐ Migrate that single highest-priority SoA fallback family onto shared stdlib helper/conversion paths and flip the new parity test to passing. Progress: canonical `to_aos_ref` now reuses the same SoA bridge-compat parameter matching as `to_aos` in inline parameter lowering (including specialized helper-path variants), and focused compile-run coverage now passes for root `soa_vector` receiver-location form `/std/collections/soa_vector/to_aos_ref<T>(location(values))` in the C++ emitter.
  - ◐ Remove one no-longer-needed compiler-owned SoA fallback branch for the current highest-priority migrated family. Progress: removed the inline-parameter `calleePath.empty()` acceptance branch from builtin `soa_vector` -> experimental `SoaVector__*` bridge matching so S2 bridge compatibility now requires canonical helper-path classification.
  - ◐ Add a guard test that fails if that removed branch behavior reappears. Progress: added a focused inline-parameter helper regression that asserts empty-callee-path calls reject builtin `soa_vector` -> `SoaVector__*` bridge matching while canonical `/std/collections/soa_vector/to_aos_ref` still succeeds.

**Group 15 - Semantic memory footprint and multithread compile substrate**
Order rule: execute leaves top-to-bottom. Each leaf below is scoped to one code-affecting commit.

P0 - Reproducible measurement and attribution

P1 - Immediate peak-RSS reductions in existing pipeline

P2 - Traversal and allocation churn reductions
