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
  - ◐ Migrate Family S2 canonical `to_aos(_ref)` bridge-shim dependence onto shared stdlib helper/conversion paths. Progress: canonical `/std/collections/soa_vector/to_aos` now lowers through canonical `/std/collections/soa_vector/count|get` loop substrate instead of directly forwarding to `/std/collections/experimental_soa_vector_conversions/soaVectorToAos`, while `/std/collections/soa_vector/to_aos_ref` still routes through `to_aos(dereference(values))`.
    - ◐ Remove remaining compiler-owned inline-parameter SoA bridge shim dependence for direct experimental conversion helper callees (`/std/collections/experimental_soa_vector_conversions/soaVectorToAos(_Ref)`). Progress: direct experimental conversion-helper bridge matching is now removed from inline-parameter lowering; only canonical `/std/collections/soa_vector/count|get` bridge paths remain, with focused IR and compile-run coverage locking the reject contract for direct experimental helper calls on builtin `soa_vector`.
      - ◐ Repoint canonical `/std/collections/soa_vector/to_aos(_ref)` internals onto the canonical count/get substrate (or equivalent lowering substrate) so canonical conversion no longer depends on direct experimental helper callee bridge matching. Progress: `/std/collections/soa_vector/to_aos` now loops via canonical `/std/collections/soa_vector/count|get`; remaining `_ref` routing cleanup stays tracked under the next leaf.
      - ◐ Remove direct experimental helper callee bridge matching from inline-parameter lowering and lock the behavior with focused IR + compile-run coverage. Progress: inline-parameter bridge matching no longer treats `/std/collections/experimental_soa_vector_conversions/soaVectorToAos(_Ref)` as bridge-eligible for builtin `soa_vector`, and focused IR + compile-run coverage now lock the mismatch diagnostic.
  - ◐ Migrate Family S3 pending borrowed field-view diagnostics onto finalized stdlib helper contracts. Progress: direct pending borrowed/field-view diagnostics now canonicalize onto `unknown method: /std/collections/soa_vector/ref(_ref)` and `unknown method: /std/collections/soa_vector/field_view/<field>` across validator, return-inference, and monomorph fallback paths, with source-lock + semantic/IR validation coverage updated to assert the canonical contract; the dead optional pending-diagnostic helper path, direct pending unavailable-method wrapper, and unused borrowed-helper visibility probe argument are removed from semantics helpers, and callsites now route through single-argument `soaUnavailableMethodDiagnostic(...)` so these paths only use the canonical unknown-method contract.

**Group 15 - Semantic memory footprint and multithread compile substrate**
Order rule: execute leaves top-to-bottom. Each leaf below is scoped to one code-affecting commit.

P0 - Reproducible measurement and attribution

P1 - Immediate peak-RSS reductions in existing pipeline

P2 - Traversal and allocation churn reductions
