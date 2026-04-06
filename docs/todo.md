# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started
  ✓ Finished

Finished items are periodically moved to `docs/todo_finished.md`.

**Types & Semantics**
Roadmap note: completed map/vector migration, compatibility-cleanup, and graph perf slices now live in `docs/todo_finished.md`. Keep this live file focused on unfinished ownership/runtime substrate work, SoA bring-up, and the remaining graph/boundary items.

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
