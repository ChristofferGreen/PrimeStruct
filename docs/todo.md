# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started
  ✓ Finished

Finished items are periodically moved to `docs/todo_finished.md`.

**Types & Semantics**
Roadmap note: completed map/vector migration and compatibility-cleanup slices now live in `docs/todo_finished.md`. Keep this live file focused on unfinished ownership/runtime substrate work, SoA bring-up, and the remaining graph/boundary items.

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
  - ◐ Implement graph-backed query invalidation rules and coverage for local-binding edits. Progress: added dependency-chain coverage for local binding invalidation to establish the expected fan-out surface.
  - ◐ Implement graph-backed query invalidation rules and coverage for control-flow edits. Progress: added dependency-chain coverage for control-flow invalidation through conditional return paths.
  - ✓ Implement graph-backed query invalidation rules and coverage for initializer-shape edits.
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
- ✓ Implement graph performance guardrails and sustained perf coverage now that the regression-budget contract is documented. Completed: baseline metrics, regression thresholds/reporting, and sustained perf coverage are now in place.
- ✓ Add baseline graph timing and invalidation metrics for the current stabilized graph surface. Completed: the type-resolution graph dump now includes prepare/build timing, node/edge counts, per-kind node/edge totals, SCC counts, and invalidation fan-out counters, while the graph snapshot helper now surfaces prepare/build timing, node/edge totals, and the same invalidation counters.
  - ✓ Add regression thresholds and reporting for those graph metrics. Completed: type-graph dumps now report optional prepare/build budget caps and over-budget flags via `PRIMESTRUCT_GRAPH_PREPARE_MS_MAX` and `PRIMESTRUCT_GRAPH_BUILD_MS_MAX`, and the graph snapshot helper now surfaces the same budget/over-budget data.
- ✓ Add sustained graph performance coverage over the stabilized graph surface. Completed: baseline timing/budget metrics now exist; a lightweight graph perf snapshot test records prepare/build timings; budgeted prep/build coverage is wired; and a long-running graph perf soak is available behind `PRIMESTRUCT_GRAPH_SOAK`.
    - ✓ Add a lightweight graph perf snapshot test that records prepare/build timings for a fixed corpus.
    - ✓ Add budgeted perf coverage for graph prep/build on a representative compile-pipeline fixture.
    - ✓ Add a long-running graph perf soak (disabled by default) for larger program suites.
- ✓ Prototype optional parallel solve now that its execution and merge contract is documented. Completed: a first partition/execution layer now groups condensation DAG components into stable layers with deterministic merge ordering, and parity coverage asserts the layer-order toggle matches the default solve.
