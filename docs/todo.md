# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started
  ✓ Finished

Finished items are periodically moved to `docs/todo_finished.md`.

**Types & Semantics**
Roadmap note: completed map/vector migration, compatibility-cleanup, and graph perf slices now live in `docs/todo_finished.md`. Keep this live file focused on unfinished ownership/runtime substrate work, SoA bring-up, and the remaining graph/boundary items.
Sizing note: each leaf `○` item should fit in one code-affecting commit plus focused conformance updates for that slice. If a leaf needs multiple behavior changes, split it first.

**Architecture / Type-resolution graph**
**Group 11 - Near-term graph queue**
Blocked by Group 13 rollout constraints until the remaining collection-helper/runtime predecessor items are finished and the return-kind pilot path is stable enough to resume broader graph work.
- ◐ Implement the next non-template inference-island migrations now that the graph-backed cutover contract is documented.
  - ○ Migrate the next direct-call/callee non-template inference island onto the graph path.
    - ○ Select the first direct-call island and record the exact validator/lowerer touchpoints.
    - ○ Publish graph facts for that island behind the existing inference path (no behavior change).
    - ○ Switch production inference for that island to consume graph facts.
    - ○ Add positive + negative conformance fixtures for that island.
    - ○ Delete the replaced fallback branch for that island.
  - ○ Migrate the next receiver/method-call non-template inference island onto the graph path.
    - ○ Select the first receiver/method-call island and record the exact touchpoints.
    - ○ Publish graph facts for that island behind the existing inference path (no behavior change).
    - ○ Switch production inference for that island to consume graph facts.
    - ○ Add positive + negative conformance fixtures for that island.
    - ○ Delete the replaced fallback branch for that island.
  - ○ Migrate the next collection helper/access fallback non-template inference island onto the graph path.
    - ○ Select the first collection helper/access island and record the exact touchpoints.
    - ○ Publish graph facts for that island behind the existing inference path (no behavior change).
    - ○ Switch production inference for that island to consume graph facts.
    - ○ Add positive + negative conformance fixtures for that island.
    - ○ Delete the replaced fallback branch for that island.
- ◐ Implement the graph/CT-eval interaction contract now that the boundary is documented.
  - ○ Route graph-backed consumers onto shared dependency-state consumption where CT-eval and graph validation overlap.
    - ○ Identify one dependency state shared by CT-eval and graph validation.
    - ○ Publish that dependency state on the shared graph-backed surface.
    - ○ Migrate one graph consumer to the shared state.
    - ○ Remove the migrated consumer's duplicate local state path.
  - ○ Add one explicit adapter for the remaining CT-eval-only paths that still cannot consume shared graph dependency state directly.
    - ○ Select one CT-eval-only dependency state that needs an adapter.
    - ○ Implement a minimal adapter for that dependency state.
    - ○ Wire one CT-eval callsite through the adapter.
    - ○ Add boundary diagnostics for adapter/graph mismatch cases.
  - ○ Add conformance coverage for the graph/CT-eval handoff boundary.
    - ○ Add a positive fixture that exercises the adapter seam.
    - ○ Add a negative diagnostic fixture that proves boundary mismatch rejection.
- ◐ Implement the graph-backed explicit/implicit template-inference migrations now that the cutover contract is documented.
  - ○ Migrate the next explicit template-argument inference surface onto the graph path.
    - ○ Select one explicit template-argument surface.
    - ○ Publish graph facts for that surface behind the current path.
    - ○ Switch inference for that surface to graph-backed facts.
    - ○ Add positive + negative conformance fixtures for that surface.
    - ○ Delete the replaced fallback path for that surface.
  - ○ Migrate the next implicit template-inference surface onto the graph path.
    - ○ Select one implicit template-inference surface.
    - ○ Publish graph facts for that surface behind the current path.
    - ○ Switch inference for that surface to graph-backed facts.
    - ○ Add positive + negative conformance fixtures for that surface.
    - ○ Delete the replaced fallback path for that surface.
  - ○ Migrate the matching revalidation/monomorph follow-up for those template-inference surfaces onto the graph path.
    - ○ Identify the required revalidation/monomorph follow-up for the migrated surfaces.
    - ○ Move that follow-up onto graph-backed facts and add conformance coverage.
- ◐ Implement the next omitted-envelope and local-`auto` graph expansions now that the widening contract is documented.
  - ○ Materialize omitted-envelope graph facts for the next widening slice.
    - ○ Select the next omitted-envelope family to model in the graph (proposed: omitted struct initializer envelopes).
    - ○ Add graph facts and positive + negative coverage for that family.
  - ○ Expand local-`auto` graph support across the next initializer-family surface.
    - ○ Select the next initializer-family surface (proposed: `if`-branch local `auto` binds).
    - ○ Land the widening and lock behavior with coverage.
  - ○ Expand local-`auto` graph support across the next control-flow join surface.
    - ○ Select the next control-flow join surface (proposed: `if` join return inference).
    - ○ Land the widening and lock behavior with coverage.

**Group 12 - Semantics/lowering boundary**
Progress note: the first semantic-product publication, dump surface, and temporary lowerer adapter are finished. The remaining live boundary work is to make `SemanticProgram` the only lowering truth, retire the remaining AST-derived fallbacks, and finish the graph-backed fact handoff needed by later Group 11 migrations.
- ✓ Turn missing semantic-product facts into explicit conformance failures instead of silent AST fallbacks.
  - ✓ Add compile-pipeline/backend conformance checks that assert the required semantic-product fact families are present before lowering starts.
    - ✓ Add a per-fact-family completeness matrix for lowering-required facts.
    - ✓ Add negative fixtures for routing fact families (direct-call, method-call, bridge-path).
    - ✓ Add negative fixtures for type/shape fact families (bindings, local-`auto`, return metadata).
    - ✓ Add negative fixtures for result/control fact families (`query`, `try(...)`, `on_error`, entry args).
    - ✓ Add one aggregate fixture that validates deterministic first-failure ordering when multiple families are missing.
- ✓ Move resolved call routing to semantic-product-only ownership.
  - ✓ Move receiver/method-call target routing to semantic-product-only ownership.
  - ✓ Move helper-vs-canonical bridge-path routing to semantic-product-only ownership, including same-path helper-shadow choices.
  - ✓ Remove any remaining routing helper that still probes scope/import fallback when semantic ids are present.
    - ✓ Inventory the remaining semantic-id-present fallback probes and map each to a caller.
      - ✓ `makeResolveCallPathFromScope`: semantic direct-call target branch falls back to AST rooted name when semantic target path is unresolved in `defMap`.
      - ✓ `makeResolveCallPathFromScope`: rooted rewritten direct-call branch without a semantic-product fact falls back through `resolveCallPathFromScopeWithoutImportAliases`.
      - ✓ `makeResolveCallPathFromScope`: semantic-product-present generic branch still calls `resolveCallPathFromScopeWithoutImportAliases`.
    - ✓ Switch one caller at a time to semantic-product-only routing.
      - ✓ Keep semantic-product direct-call targets authoritative even when rooted AST call names differ.
      - ✓ Keep rewritten rooted direct-call paths explicit instead of routing through scope-probing fallback helpers.
      - ✓ Keep semantic-product generic path canonical without scope/root fallback probes.
    - ✓ Delete the fallback probe helpers once all callers are migrated.
- ✓ Make graph-backed local-`auto`, query, `try(...)`, and `on_error` facts required lowering inputs.
  - ✓ Make graph-backed local-`auto` facts required for lowerer local type setup.
  - ✓ Make graph-backed query and `try(...)` facts required for lowerer call/result setup.
  - ✓ Make graph-backed `on_error` and entry-parameter facts required during lower entry setup.
- ✓ Reshape `SemanticProgram` into module-scoped resolved artifacts instead of one flat whole-program fact bag.
  - ✓ Define the module-scoped semantic-product container shape and deterministic module ordering contract.
    - ✓ Define the module identity key and sort order contract.
    - ✓ Define per-module ownership for callable summaries, bindings, and call-routing facts.
  - ✓ Move existing fact families under module-scoped resolved artifacts without changing their lowering-visible meaning.
    - ✓ Migrate callable + call-routing fact families behind a compatibility adapter.
    - ✓ Migrate binding/result/control-flow fact families behind the same adapter.
    - ✓ Remove the adapter once all families are module-scoped.
  - ✓ Update lowerer/testing consumers to read module-scoped semantic artifacts instead of flat whole-program vectors.
    - ✓ Migrate lowerer readers first.
    - ✓ Migrate semantic snapshot/testing helpers second.
    - ✓ Delete flat-vector readers once both migrations are complete.
- ◐ Add CI budget gates for graph invalidation and graph-build metrics using the `type-graph` output.
  - ○ Select the representative graph corpora and baseline thresholds for prepare/build time and invalidation fan-out.
  - ○ Add a local/CTest graph-budget checker target that fails on threshold regressions.
  - ○ Wire the graph-budget checker target into CI so regressions fail by default.
  - ○ Document the workflow for intentionally revising graph budgets and metric baselines.
- ✓ Retire AST-derived lowering fallbacks until the AST is provenance-only at the lowering boundary.
  - ✓ Remove the remaining transform-based binding-kind and helper-alias fallback paths once semantic-product fact coverage is complete.
    - ✓ Normalize canonical map helper/access alias families onto stdlib builtin surfaces across validate/infer/lowerer.
    - ✓ Keep value + borrowed helper families aligned for method/direct/args-pack map receiver resolution.
    - ✓ Align bare `contains` / `tryAt` visibility checks across late validation, infer late-fallback inference, and `try(...)` validation.
    - ✓ Land the final runtime parity follow-up for experimental-map receiver metadata in runtime/native execution paths.
    - ✓ Normalize diagnostics for runtime-parity mismatch cases so failures are deterministic.
    - ✓ Add focused conformance coverage that locks runtime parity and rejects regressions back to AST/import fallback behavior.
  - ✓ Remove the final AST-derived semantic fallback slices so the AST remains lowering-visible only for provenance/debug/source-map ownership.
    - ✓ Inventory each remaining AST/import fallback site used after the lowering boundary.
      - ✓ `resolveMethodCallDefinitionFromExpr`: semantic-mode method-call target miss in `defMap` previously fell through to receiver-probing helper alias remaps.
    - ✓ Replace fallback sites in validator-side binding-kind paths with semantic-product facts or explicit failures.
      - ✓ Route lowerer explicit-binding detection for semantic-node bindings through semantic binding/local-`auto` facts instead of AST transform fallback.
      - ✓ Remove remaining transform-driven binding-kind/value fallback decisions from inference/control-flow helpers when semantic facts are present.
    - ✓ Replace fallback sites in call/helper alias paths with semantic-product facts or explicit failures.
      - ✓ Route semantic-mode method-call path resolution through semantic-product method-call targets (no AST/import alias fallback).
      - ✓ Remove the remaining semantic-mode helper alias remaps that still receiver-probe when semantic targets are absent from the lowered def map.
    - ✓ Delete obsolete fallback codepaths and transitional adapters after coverage lands.
