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
  - ◐ Migrate Family S2 canonical `to_aos(_ref)` bridge-shim dependence onto shared stdlib helper/conversion paths. Progress: canonical `/std/collections/soa_vector/to_aos` now lowers through a self-contained canonical helper loop and no longer forwards into experimental conversion signatures.
    - ○ Remove remaining compiler-owned inline-parameter SoA bridge shim dependence for canonical `/std/collections/soa_vector/to_aos_ref` and direct experimental conversion helper callees (`/std/collections/experimental_soa_vector_conversions/soaVectorToAos(_Ref)`).
  - ○ Migrate Family S3 pending borrowed field-view diagnostics onto finalized stdlib helper contracts.

**Group 15 - Semantic memory footprint and multithread compile substrate**
Order rule: execute leaves top-to-bottom. Each leaf below is scoped to one code-affecting commit.

P0 - Reproducible measurement and attribution

P1 - Immediate peak-RSS reductions in existing pipeline

P2 - Traversal and allocation churn reductions
