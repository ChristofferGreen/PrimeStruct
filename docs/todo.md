# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started
  ✓ Finished

Finished items are periodically moved to `docs/todo_finished.md`.

**Types & Semantics**
Roadmap note: completed map/vector migration and compatibility-cleanup slices now live in `docs/todo_finished.md`. Keep this live file focused on unfinished ownership/runtime substrate work, SoA bring-up, and the remaining graph/boundary items.

**Group 4 - Ownership and lifetime substrate**
Ownership/drop status note: completed guard and container-error-contract checkpoints are archived in `docs/todo_finished.md`. Keep the live file on the remaining non-trivial destruction and lifetime substrate work.
- ◐ Implement container element drop behavior in vector/map runtimes once the ownership contract is fixed. Progress: experimental `.prime` `Map<K, V>` destruction now reuses the experimental-vector drop/free steps for its key/payload storage so owned map buffers and element drops clean up on destroy, canonical `/std/collections/vector/*` `pop`/`clear` plus indexed-removal helpers now accept ownership-sensitive element types on explicit experimental `Vector<T>` bindings because they forward onto the experimental `.prime` discard/compaction implementation, canonical `/std/collections/map/insert` now forwards explicit experimental `Map<K, V>` bindings onto the `.prime` overwrite/growth path so ownership-sensitive value updates run through the same drop/reinit flow, canonical `/std/collections/map/insert_ref` now forwards borrowed `Reference<Map<K, V>>` bindings onto that same overwrite/growth path, explicit experimental `Map<K, V>` value method sugar now resolves through the `.prime` `/Map/*` method path so ownership-sensitive read/update flows stay on the same runtime drop/overwrite path, borrowed `Reference<Map<K, V>>` method sugar now resolves through the `.prime` borrowed-helper path so ownership-sensitive experimental-map overwrite/drop flows work through borrowed references, and builtin canonical `map<K, V>` inserts now perform real in-place overwrite when the numeric key already exists and grow through one generic arbitrary-`n` grow/copy/repoint path for owning local numeric maps instead of a count-by-count staircase.
  - ✓ Replace the temporary builtin canonical `map<K, V>` insert staircase with one generic arbitrary-`n` growth/drop implementation.


**Group 8 - SoA de-builtinization**
- ◐ `soa_vector<T>` freeze/stabilize slice: keep the current empty/header-only construction, `count`, validation, and existing helper-precedence behavior working while the new path lands, but do not expand compiler-owned `soa_vector` surface area unless it directly unblocks the items below.
- ✓ Lock the v1 `soa_vector` contract before more implementation work: construction forms, `count`, `get`, `ref`, field-view indexing shape, explicit `to_soa` / `to_aos`, and invalidation boundaries after structural mutation. Progress: semantic validation now emits deterministic indexing-shape diagnostics (`soa_vector field views require value.<field>()[index] syntax: <field>`) for invalid direct field-view argument forms such as `values.field(i)` and `field(values, i)` instead of a generic argument-reject message, unresolved call-form field-view index attempts (`field(values, i)`) now emit that same deterministic syntax diagnostic instead of `unknown call target`, builtin `get`/`ref` method forms now reject named arguments through the same deterministic `named arguments not supported for builtin calls` contract as call form, explicit `/soa_vector/get` / `/soa_vector/ref` direct calls now use that same builtin validation path instead of leaking into field-view fallback diagnostics, explicit `/soa_vector/count` direct-call and slash-method forms now normalize to the builtin `count` surface instead of doubling the helper path into `/soa_vector/soa_vector/count`, `to_soa` / `to_aos` method forms plus explicit slash-method forms now normalize to the builtin conversion surface instead of failing as unknown `/vector/to_soa` or `/soa_vector/to_aos` methods, builtin `soa_vector ref(...)` results now reject local binding persistence plus call-argument/return escapes with a deterministic `soa_vector borrowed views are not implemented yet: ref` diagnostic until the borrowed-view substrate exists, and builtin `soa_vector` field-view results now reject call-argument/return escapes with the same deterministic `soa_vector field views are not implemented yet: <field>` diagnostic until the field-view substrate exists.
  - ✓ Lock explicit `/soa_vector/get` and `/soa_vector/ref` direct-call builtin diagnostics/parity with bare and method forms.
  - ✓ Lock explicit `/soa_vector/count` direct-call and slash-method parity with bare and method forms.
  - ✓ Lock `to_soa` / `to_aos` method-form and slash-method parity with bare builtin calls.
  - ✓ Reject local bindings initialized from builtin `soa_vector ref(...)` until borrowed-view substrate exists.
  - ✓ Reject builtin `soa_vector ref(...)` escapes through call arguments and returns until borrowed-view substrate exists.
  - ✓ Reject builtin `soa_vector` field-view escapes through call arguments and returns until the field-view substrate exists.
- ✓ Land the minimum compile-time struct-field introspection/codegen needed for one `.prime` `soa_vector` implementation to derive SoA column schemas from `T`.
- ○ Land the minimum column-storage substrate needed for one `.prime` `soa_vector` implementation: multi-column alloc/grow/free/read/write primitives with deterministic allocation-failure behavior.
- ○ Land the minimum borrowed-view / invalidation substrate needed for one `.prime` `soa_vector` implementation so `ref(...)` and field views have language-level semantics independent of compiler-owned `soa_vector` paths.
- ○ Add experimental stdlib `/std/collections/experimental_soa_vector/*` early with empty construction, `count`, SoA-safe type validation, and the smallest `.prime` storage/runtime path that exercises the new substrate.
- ○ Extend experimental stdlib `/std/collections/experimental_soa_vector/*` to non-empty literal construction, `push`, `reserve`, `get`, `ref`, and explicit `to_soa` / `to_aos`, adding only the next substrate pieces that the `.prime` implementation proves it needs.
- ○ Extend experimental stdlib `/std/collections/experimental_soa_vector/*` to field-view indexing (`value.field()[i]`) once the experimental `.prime` implementation reaches that boundary and the next minimal substrate slice is clear.
- ○ Route canonical `/std/collections/soa_vector/*` names through the real experimental `.prime` implementation once parity is proven.
- ○ Delete compiler-owned `soa_vector` helper routing and backend special cases from semantics, IR lowering, emitters, and runtime code until no C++ source mentions `soa_vector`.


**Architecture / Type-resolution graph**
**Group 11 - Near-term graph queue**
Blocked by Group 13 rollout constraints until the remaining collection-helper/runtime predecessor items are finished and the return-kind pilot path is stable enough to resume broader graph work.
- ○ Expand end-to-end graph conformance coverage for local `auto`, query, and `try(...)` consumers on the single graph-resolver path so the current snapshot-heavy checks are backed by real compile-pipeline scenarios.
- ○ Expand end-to-end graph conformance coverage for backend dispatch, deleted-path diagnostics, and canonical helper/method resolution on C++/VM/native so graph-solved metadata is exercised beyond the snapshot harness.
- ○ Land graph-backed query invalidation rules and coverage for local-binding, control-flow, and initializer-shape edits so cached query/binding/result metadata has one explicit contract for intra-definition churn.
- ○ Land graph-backed query invalidation rules and coverage for definition-signature, import-alias, and receiver-type edits before more inference consumers depend on the cache.
- ○ Migrate the next remaining non-template inference islands onto graph-backed query/binding state instead of leaving mixed ad hoc inference paths around the stabilized return/query/local pilot.
- ○ Define and validate graph interaction with CT-eval consumers so compile-time evaluation either consumes the shared dependency state directly or keeps one explicit boundary instead of implicit drift.
- ○ Migrate explicit and implicit template inference dependencies onto the graph-backed path once the non-template inference islands and invalidation rules are pinned, so template solving stops being a deferred side-system.
- ○ Broaden omitted-envelope and local-auto inference beyond the current first-step slices only after the next inference-island migrations prove stable on the graph path.
- ○ Add graph performance guardrails and sustained perf coverage before optional parallel solve work so future graph expansion has a deterministic regression budget.
- ○ Prototype optional parallel solve only after the single-threaded graph path, invalidation rules, CT-eval boundary, and performance guardrails are all stable.


**Group 12 - Semantics/lowering boundary**
- ○ Define a first-class `SemanticProgram` or `ResolvedModule` boundary type that captures all lowering-required semantic facts and document its ownership/lifetime contract.
- ○ Add a first semantic-product builder slice that materializes resolved call targets, binding types, effects/capabilities, and struct metadata from `SemanticsValidator`.
- ○ Add a second semantic-product builder slice that moves graph-backed local `auto`, query, `try(...)`, and `on_error` metadata out of test-only snapshot plumbing and into the semantic product.
- ○ Thread the semantic product through CLI/runtime plumbing (`primec`, `primevm`, dump-stage handling, and failure/report paths) so the new semantics boundary is carried end-to-end outside the core compile/lower APIs.
- ○ Add a temporary migration adapter that can derive lowerer input from either raw `Program` or the semantic product during the cutover, with explicit removal criteria once all lowering entrypoints use the new boundary.
- ○ Cut over `CompilePipelineOutput` and `Semantics::validate` so the compile pipeline publishes the semantic product as its post-semantics success artifact instead of only a mutated raw `Program`.
- ○ Cut over `IrLowerer::lower` and `prepareIrModule` so IR preparation consumes the semantic product directly, then retire the raw-`Program` lowering path once the temporary adapter is removed.
- ○ Add a deterministic semantic-product dump/formatter plus golden coverage so the new boundary is inspectable in the same way as AST/IR stages.
- ○ Document and test the ownership split between raw AST and semantic product, especially for source spans, debug/source-map provenance, and other syntax-faithful data that lowering/debuggers still need.
- ○ Switch `IrLowerer` entry setup to consume resolved call targets from the semantic product instead of re-deriving them from AST state.
- ○ Switch lowerer type/binding setup to consume semantic-product binding metadata instead of repeating helper/type inference logic.
- ○ Switch lowerer effect/capability and struct-layout setup to consume semantic-product metadata instead of re-reading AST/transforms for those facts.
- ○ Add a narrow semantic-product unit/golden suite that asserts exported lowering facts directly instead of only through snapshot helpers.
- ○ Add end-to-end compile-pipeline conformance cases that exercise the semantic-product boundary through C++/VM/native lowering paths.
- ○ Migrate tests and public testing helpers from `primec/testing/SemanticsValidationHelpers.h` snapshot surfaces onto the semantic-product inspection surface where they are asserting lowering-facing facts.
- ○ Delete redundant testing-only semantic snapshot plumbing once equivalent semantic-product dumps and conformance coverage exist, leaving one canonical inspection surface for lowering-facing semantic facts.
- ○ Delete lowerer-side stdlib/helper alias fallback paths once equivalent canonical resolution is provided by the semantic product.
- ○ Replace `SemanticsValidator`'s shared mutable `error_` flow with a structured diagnostic sink in the current single-threaded path while preserving deterministic first-error behavior.
- ○ Attach stable semantic-product node identities or sort keys to diagnostics so later parallel validation can merge and order them deterministically.
- ○ Split per-definition validation state from validator-global caches so semantic-product construction uses explicit contexts rather than shared mutable fields.
- ○ Update CLI help, dump-stage docs/spec text, and pipeline-facing tests to document the semantic-product inspection surface and its relation to existing AST/IR dumps.
- ○ Add a staged migration note in `docs/PrimeStruct.md` for the semantics-to-lowering boundary, including exit criteria for removing AST-dependent lowerer logic.
