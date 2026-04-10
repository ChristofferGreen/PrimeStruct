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
  - ◐ Migrate Family S1 builtin `soa_vector` literal fallback onto shared stdlib helper/conversion paths. Progress: removed compiler-owned VM/native rejection of struct-element `soa_vector` literals above the former 256-element local-capacity ceiling.
    - ○ Migrate remaining struct-only non-empty builtin `soa_vector` literal fallback diagnostics off compiler-owned lowering checks.
  - ○ Migrate the next still-unhandled compiler-owned SoA fallback family (S2 storage-layout bridge shim or S3 pending field-view diagnostics) onto shared stdlib helper/conversion paths.

**Group 15 - Semantic memory footprint and multithread compile substrate**
Order rule: execute leaves top-to-bottom. Each leaf below is scoped to one code-affecting commit.

P0 - Reproducible measurement and attribution

P1 - Immediate peak-RSS reductions in existing pipeline

P2 - Traversal and allocation churn reductions
