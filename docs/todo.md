# PrimeStruct TODO Log

## Purpose

This file is the live open-work queue for PrimeStruct.

- Keep only open work here: `[ ]` queued or `[~]` in progress.
- Move completed work to `docs/todo_finished.md`.
- Do not keep completed-task summaries, historical rollout notes, or closed
  coverage snapshots in this file.
- When this file has no task blocks, the tracked TODO queue is empty.

## Operating Rules

1. Use one task block per item with a stable `TODO-XXXX` ID.
2. Every active leaf must be implementable by someone arriving with no session
   context, including an AI agent.
3. Every active leaf must include `owner`, `created_at`, `scope`,
   `acceptance`, and `stop_rule`.
4. Prefer small, testable leaves over broad epics; split work before starting
   when acceptance cannot be verified in one bounded change.
5. Every active leaf must target at least one value outcome:
   - user-visible behavior change
   - measurable perf/memory improvement
   - deletion of a real compatibility subsystem
6. Avoid standalone micro-cleanups unless bundled into a value outcome.
7. If a leaf misses its value target after two attempts, archive it as
   low-value and replace it with a different hotspot.
8. Keep `Ready Now`, `Immediate Next 10`, `Priority Lanes`, `Execution Queue`,
   and task blocks synchronized when adding, splitting, completing, or deleting
   a task.
9. Keep `Ready Now` capped at eight active leaf tasks.
10. Keep active work leaf-shaped: queue sections must not contain umbrella,
    tracker, phase, research-shaped, or "continue with another slice" items.
11. For parallel work, each `Ready Now` item must name a `parallel_track` and a
    primary surface. Do not put two same-track successors in `Ready Now` unless
    their task blocks prove they touch different source/test surfaces.
12. Treat disabled tests as debt: each retained `doctest::skip(true)` cluster
    must map to an active TODO leaf with a re-enable-or-delete outcome, or be
    removed once proven stale.
13. When completing a task, mark it `[x]`, add `finished_at` plus a short
    evidence note, move the full block to `docs/todo_finished.md`, and remove
    it from this file.

## Task Template

```md
- [ ] TODO-<id>: Short title
  - owner: ai|human
  - created_at: YYYY-MM-DD
  - phase: Group/Phase name (optional)
  - parallel_track: short-track-name (required when listed in Ready Now)
  - depends_on: TODO-XXXX, TODO-YYYY (optional)
  - scope: ...
  - implementation_notes: optional, but required when source/test entry points are not obvious
  - acceptance:
    - ...
    - ...
  - stop_rule: ...
  - notes: optional
```

## Open Tasks

### Ready Now

- TODO-4613: Retire semantic-validator private source locks | track: semantic-source-lock-retirement | surface: semantic validator source-lock tests
- TODO-4606: Specify capability-parameterized views | track: capability-view-docs | surface: docs view model

### Immediate Next 10

- TODO-4607: Publish initial array extent facts
- TODO-4608: Add checked array slice construction
- TODO-4609: Reject escaping local array slices
- TODO-4610: Add forward cursor traversal API
- TODO-4611: Add reverse cursor traversal API
- TODO-4612: Add safe extent and cursor code examples
- TODO-4616: Make semantic validation manifest executable
- TODO-4617: Add semantic preflight missing-fact diagnostics
- TODO-4618: Fail closed on stale CT-eval requirement facts
- TODO-4619: Gate runtime reflection by backend profile

### Priority Lanes

- Scene graph renderer and UI presentation: TODO-4565 completed the data-only
  scene model and TODO-4566 completed the first BGRA8 2D primitive renderer;
  TODO-4567 completed the first globally lit 3D SDF widget primitive, and
  TODO-4595 completed deterministic shaped glyph runs. TODO-4596 completed
  deterministic text atlas/raster composition. TODO-4568 completed the first
  UI scene-record adapter, and TODO-4569 completed the software-surface UI
  presentation bridge.
- Map/vector compiler-independence: TODO-4570 retired the duplicate `map2`
  surface, TODO-4571 added the compiler-knowledge inventory categories that
  guide deletion scope, and TODO-4573 removed compiler-owned map literal
  lowering. TODO-4575 removed map helper/access classifiers, and vector path
  TODO-4572 and TODO-4574 completed the public helper classifier deletions.
  TODO-4576 and TODO-4577 removed map/vector backing classifiers. TODO-4578
  was split into TODO-4597 registry foundation plus TODO-4598, TODO-4599, and
  TODO-4600 subsystem migrations; TODO-4597 completed the generic registry
  IDs, TODO-4598 completed the semantics migration, TODO-4599 completed the
  emitter migration, TODO-4600 completed the IR-lowerer migration, and
  TODO-4601 removed the final map-helper classifier trace. TODO-4602 removed
  semantic vector-literal diagnostic traces, TODO-4603 completed the
  IR-lowerer vector-literal cleanup, and TODO-4579 wired the broad zero audit
  into release validation.
- Architecture hardening backlog: TODO-4586 completed parser diagnostic
  stability tiers. TODO-4587 completed the shared compile-time/runtime VM
  kernel boundary. TODO-4588 added the IR-preparation phase manifest.
  TODO-4589 added the architecture health dashboard. TODO-4594 completed the
  semantic unknown-call diagnostic stability slice.
- Architecture review hardening: TODO-4613 through TODO-4621 capture the
  concrete follow-ups from the latest architecture review: retire temporary
  private source locks, make the semantic validation manifest executable, add
  semantic-product stale/missing diagnostics, add a second backend capability
  gate, index expanded-source diagnostic lookup, and promote one lowerer or
  backend diagnostic into the stability-tier contract.
- Safe array extents and capability views: TODO-4604 completed the requirement
  contract phase split, and TODO-4605 completed the non-null safe pointer
  optionality model. TODO-4606 through TODO-4612 remain from the agreed
  backlog in `docs/SafeArrayExtentViews.md`: capability-parameterized
  references and slices, semantic extent facts, checked slices, conservative
  view escapes, cursor traversal with
  `limit(...)` / `reverse_limit(...)` boundaries, and style-aligned examples
  once the surface is specified.

### Execution Queue

1. TODO-4613: Retire semantic-validator private source locks
2. TODO-4606: Specify capability-parameterized views
3. TODO-4607: Publish initial array extent facts
4. TODO-4608: Add checked array slice construction
5. TODO-4609: Reject escaping local array slices
6. TODO-4610: Add forward cursor traversal API
7. TODO-4611: Add reverse cursor traversal API
8. TODO-4612: Add safe extent and cursor code examples
9. TODO-4616: Make semantic validation manifest executable
10. TODO-4617: Add semantic preflight missing-fact diagnostics
11. TODO-4618: Fail closed on stale CT-eval requirement facts
12. TODO-4619: Gate runtime reflection by backend profile
13. TODO-4620: Index expanded-source segments for diagnostics
14. TODO-4621: Classify unsupported variadic-pack diagnostics

### Task Blocks

- [ ] TODO-4606: Specify capability-parameterized views
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - parallel_track: capability-view-docs
  - depends_on: TODO-4605
  - scope: Promote the unified view model from `docs/SafeArrayExtentViews.md`
    into the normative design docs: `Reference<T, Capability>` is the
    single-element view, `Slice<T, Capability>` is the contiguous multi-element
    view, and both share a semantic `View<T, Capability>` model over valid
    pointer storage plus extent/provenance/capability facts.
  - implementation_notes: Start with `docs/PrimeStruct.md` and
    `docs/MemoryCapabilities.md`; keep the exact standard capability names
    open unless the docs already define stable names such as `Read` or `Write`.
  - acceptance:
    - Docs explain why `Reference<T, Capability>` is not a nullable pointer but
      a capability-carrying view with `count == 1`.
    - Docs explain that `Slice<T, Capability>` carries runtime count and shares
      borrow/provenance semantics with references.
    - Source-lock coverage proves the canonical docs and design note agree on
      the unified view model.
  - stop_rule: Stop after the view model is specified and source-locked; do not
    add parser support for capability-parameterized references or slices in
    this leaf.

- [ ] TODO-4607: Publish initial array extent facts
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - depends_on: TODO-4606
  - scope: Add the first semantic-product extent facts for existing
    `array<T>` values, `Reference<array<T>>` parameters, and `count(...)`
    expressions so downstream consumers can observe array element counts
    without reconstructing them from AST syntax.
  - implementation_notes: Start with `include/primec/SemanticProduct.h`,
    `src/SemanticProduct.cpp`, semantic publication builders, and the
    semantic-product dump/source-lock tests. Keep the first fact family limited
    to arrays and existing count surfaces.
  - acceptance:
    - `semantic-product` dumps expose deterministic extent facts for local
      array values and `Reference<array<T>>` parameters.
    - A focused stale/missing test proves an IR-preparation or lowerer
      consumer fails closed when an expected published extent fact is absent.
    - Existing array count/indexing coverage still passes.
  - stop_rule: Stop once array extent facts are published and covered; do not
    implement slices or cursor APIs in this leaf.

- [ ] TODO-4608: Add checked array slice construction
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - depends_on: TODO-4607
  - scope: Add the first checked `slice(values, start, end)` surface for
    `array<T>`/`Reference<array<T>>` inputs, publishing
    `count(result) == end - start` plus provenance facts and emitting one
    construction-time bounds check when the range is not statically proven.
  - implementation_notes: Start with parser/semantic recognition for the
    chosen slice helper path, semantic-product extent/view facts, and
    VM/native/IR-to-C++ lowering for read-only array slices.
  - acceptance:
    - Valid array slices can be indexed and counted through VM/native and C++
      emit paths.
    - Static invalid ranges produce deterministic diagnostics; runtime ranges
      emit deterministic construction-time bounds failures.
    - Lowering uses the published slice extent fact for indexed access instead
      of rechecking the original owner expression.
  - stop_rule: Stop once read-only array slices work end to end; leave mutable
    slices, vectors, and cursor traversal to later leaves.

- [ ] TODO-4609: Reject escaping local array slices
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - depends_on: TODO-4608
  - scope: Add the first conservative view lifetime diagnostic by rejecting a
    slice or reference view that escapes a local array owner through return,
    stored field, or longer-lived binding.
  - implementation_notes: Start with semantic validation around slice
    construction, return validation, and binding lifetime/provenance facts. Use
    lexical scope rather than non-lexical lifetime inference.
  - acceptance:
    - Returning a slice of a local array is rejected with a deterministic
      diagnostic naming the view and owner.
    - Passing a slice to a callee that does not store or return it remains
      accepted.
    - Tests document that the first checker is conservative and lexical.
  - stop_rule: Stop once local-owner slice escape is rejected and covered; do
    not add disjoint mutable slice analysis in this leaf.

- [ ] TODO-4610: Add forward cursor traversal API
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - depends_on: TODO-4608
  - scope: Add the first read-only forward cursor traversal API for arrays and
    vectors using `start(values)` as the first position and `limit(values)` as
    the one-past-final exclusive traversal boundary.
  - implementation_notes: Start with stdlib surface definitions or compiler
    intrinsics for `Cursor<T, Capability>`, `start`, `limit`, `read`, and
    `advance`; keep the first cursor category forward-only unless contiguous
    or random-access behavior is already needed by the tests.
  - acceptance:
    - A `while(it != limit(values))` loop over `vector<int>` compiles and runs
      without skipping the final element.
    - `read(limit(values))` is rejected or fails deterministically.
    - Cursor comparisons require compatible provenance and reject unrelated
      cursor comparisons.
  - stop_rule: Stop once read-only forward traversal works for arrays and
    vectors; leave reverse traversal and writable cursors to later leaves.

- [ ] TODO-4611: Add reverse cursor traversal API
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - depends_on: TODO-4610
  - scope: Add reverse read-only cursor traversal using
    `reverse_start(values)` as the last readable position or
    `reverse_limit(values)` when empty, and `reverse_limit(values)` as the
    one-before-first exclusive traversal boundary.
  - implementation_notes: Start with the cursor API from TODO-4610 and add
    `reverse_start`, `reverse_limit`, and `retreat` for arrays and vectors.
    Keep `last(values)` as an element-oriented helper returning
    `Maybe<Cursor<T, Capability>>` if it is exposed in this leaf.
  - acceptance:
    - A reverse `while(it != reverse_limit(values))` loop over `vector<int>`
      visits every element exactly once in reverse order.
    - Empty arrays/vectors produce `reverse_start(values) == reverse_limit(values)`
      and execute zero loop iterations.
    - `read(reverse_limit(values))` is rejected or fails deterministically.
  - stop_rule: Stop once read-only reverse traversal is implemented and
    covered for arrays and vectors; leave writable cursors and advanced
    iterator categories to later leaves.

- [ ] TODO-4612: Add safe extent and cursor code examples
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - depends_on: TODO-4604, TODO-4605, TODO-4606, TODO-4608, TODO-4610,
    TODO-4611
  - scope: Add runnable, style-aligned examples to `docs/CodeExamples.md` for
    the agreed safe-array extent and cursor surfaces after their normative
    spelling is specified.
  - implementation_notes: Cover the smallest useful set: a contract-form
    `require(...)` example that proves-or-checks an extent relationship,
    `Maybe<Pointer<T>>` optional-pointer handling, a
    `Reference<T, Capability>`/`Slice<T, Capability>` example, a checked slice
    loop, a forward cursor loop using `limit(values)`, and a reverse cursor
    loop using `reverse_limit(values)`. Keep examples minimal and runnable
    with the current compiler before treating them as style guidance.
  - acceptance:
    - `docs/CodeExamples.md` contains user-facing examples for safe extent
      contracts, optional pointers, capability views, checked slices, and
      forward/reverse cursor loops.
    - Examples use the readable surface form and naming guidance from
      `docs/CodeExamples.md`.
    - Source-lock coverage proves the examples stay aligned with
      `docs/SafeArrayExtentViews.md` and the relevant normative
      `docs/PrimeStruct.md` sections.
    - The new examples compile or are explicitly marked as proposed syntax
      until the corresponding implementation leaves land.
  - stop_rule: Stop once the example guide and source-lock coverage are
    updated; do not implement missing language features in this leaf.

- [ ] TODO-4613: Retire semantic-validator private source locks
  - owner: ai
  - created_at: 2026-05-27
  - phase: Architecture hardening
  - parallel_track: semantic-source-lock-retirement
  - scope: Replace the temporary semantic-validator delegation source locks
    with public semantic-product, type-resolution graph, or deterministic
    diagnostic contract coverage so active tests no longer read private
    `src/semantics/*.h` or `src/semantics/*.cpp` fragments just to prove
    validator split points.
  - implementation_notes: Start with the semantic-validator rows in
    `docs/source_lock_inventory.md`, the matching tests under `tests/unit/`,
    and any existing public helpers under `include/primec/testing/`.
  - acceptance:
    - The semantic-validator row in `docs/source_lock_inventory.md` is
      removed or reclassified because delegation coverage no longer depends on
      private semantics source text.
    - Replacement coverage proves one observable public contract: a published
      semantic fact, type-resolution graph shape, or stable diagnostic.
    - Include-layer validation passes without adding a new allowlist entry.
  - stop_rule: Stop once the validator source-delegation source lock is
    retired and covered; do not split or reorganize semantic validator
    fragments in this leaf.

- [ ] TODO-4616: Make semantic validation manifest executable
  - owner: ai
  - created_at: 2026-05-27
  - phase: Architecture hardening
  - scope: Make `semanticValidationPassManifest()` the executable authority for
    semantic validation pass dispatch instead of keeping the manifest as
    metadata beside a separate string-name runner.
  - implementation_notes: Start with
    `include/primec/SemanticValidationPlan.h`,
    `src/semantics/SemanticValidationPlan.cpp`, and the
    `Semantics::validate` pass loop in `src/semantics/SemanticsValidate.cpp`.
    A typed pass id, runner callback, or checked switch is acceptable if the
    manifest remains the single order/ownership source.
  - acceptance:
    - Semantic validation order and execution are derived from the manifest,
      and the ad hoc `pass.name == ...` dispatch chain is deleted or reduced to
      a checked compatibility shim with no independent ordering authority.
    - Adding a pass to the manifest without runner coverage fails a focused
      test or compile-time contract.
    - Existing manifest source-lock coverage still proves AST mutation
      ownership and semantic-product publication boundaries.
  - stop_rule: Stop once the manifest drives pass execution; do not split pass
    implementations or change pass order except where required by the manifest
    handoff.

- [ ] TODO-4617: Add semantic preflight missing-fact diagnostics
  - owner: ai
  - created_at: 2026-05-27
  - phase: Architecture hardening
  - scope: Add deterministic missing/stale diagnostics for
    `publishedLowererPreflightFacts` so lowerer effect setup fails closed when
    software numeric type or runtime reflection preflight ids are absent,
    incomplete, or stale.
  - implementation_notes: Start with
    `docs/SemanticProductConsumerMatrix.md`,
    `include/primec/SemanticProduct.h`, `src/SemanticProduct.cpp`, and
    `src/ir_lowerer/IrLowererLowerEffects.cpp`.
  - acceptance:
    - A focused stale/missing test removes or corrupts one software numeric
      preflight fact and gets a deterministic diagnostic instead of fallback
      AST reconstruction.
    - A focused stale/missing test covers runtime reflection preflight ids at
      the consumer boundary.
    - The consumer matrix row is updated with the new stale/missing coverage
      names.
  - stop_rule: Stop once preflight facts fail closed with stable diagnostics;
    do not add new semantic-product fact families in this leaf.

- [ ] TODO-4618: Fail closed on stale CT-eval requirement facts
  - owner: ai
  - created_at: 2026-05-27
  - phase: Architecture hardening
  - scope: Make compile-time requirement evaluation reject incomplete, missing,
    or stale `requirementPredicateFacts` from the semantic product instead of
    degrading into ordinary no-match or syntax-derived behavior.
  - implementation_notes: Start with
    `docs/SemanticProductConsumerMatrix.md`,
    `include/primec/CompileTimeEvaluation.h`,
    `src/CompileTimeEvaluation.cpp`, and
    `src/semantics/RequirementPredicateFacts.cpp`.
  - acceptance:
    - A missing published requirement-predicate fact produces a deterministic
      CT-eval diagnostic naming the missing semantic fact family.
    - A stale predicate fact with mismatched callable or argument identity is
      rejected before evaluation.
    - The consumer matrix row records the new stale/missing coverage and the
      positive CT-eval coverage still passes.
  - stop_rule: Stop once CT-eval fails closed on requirement-predicate fact
    completeness; do not implement new requirement syntax in this leaf.

- [ ] TODO-4619: Gate runtime reflection by backend profile
  - owner: ai
  - created_at: 2026-05-27
  - phase: Architecture hardening
  - scope: Add one non-graphics backend capability gate for runtime reflection
    support, advertise it through `IrBackendProfiles`, and route an existing
    runtime-reflection preflight path through the profile check before
    lowering.
  - implementation_notes: Start with `include/primec/IrBackendProfiles.h`,
    `src/IrBackendProfiles.cpp`, `src/IrPreparation.cpp`, and any lowerer
    preflight checks that consume runtime reflection ids.
  - acceptance:
    - Backend profiles expose a named runtime-reflection capability in addition
      to the existing graphics-runtime substrate capability.
    - A backend without the capability rejects a runtime-reflection-dependent
      program with a deterministic diagnostic before backend-specific lowering.
    - A backend with the capability keeps the existing runtime-reflection
      coverage passing.
  - stop_rule: Stop after one real runtime-reflection capability gate is
    enforced; do not expand the profile model to every documented construct in
    this leaf.

- [ ] TODO-4620: Index expanded-source segments for diagnostics
  - owner: ai
  - created_at: 2026-05-27
  - phase: Architecture hardening
  - scope: Replace the linear expanded-source segment lookup used for
    diagnostic source mapping with a deterministic indexed lookup so large
    generated or expanded sources avoid repeated full segment scans.
  - implementation_notes: Start with `include/primec/SourceLocationMapper.h`,
    `src/SourceLocationMapper.cpp`, `include/primec/ExpandedSource.h`, and the
    expanded-source provenance tests. Keep the index deterministic and local to
    source mapping.
  - acceptance:
    - Source mapping results remain byte-for-byte stable for existing
      diagnostics and expanded-source tests.
    - A focused synthetic many-segment test proves lookup uses the indexed
      path and does not scan every segment for each mapped diagnostic.
    - The architecture health dashboard or focused benchmark notes the lookup
      budget improvement or guards against an obvious regression.
  - stop_rule: Stop once expanded-source diagnostic lookup is indexed and
    covered; do not redesign expanded-source provenance records in this leaf.

- [ ] TODO-4621: Classify unsupported variadic-pack diagnostics
  - owner: ai
  - created_at: 2026-05-27
  - phase: Architecture hardening
  - scope: Promote one existing lowerer/backend unsupported variadic-pack
    diagnostic into the diagnostic stability-tier contract, including stable
    message text, primary span, and notes where applicable.
  - implementation_notes: Start with `include/primec/Diagnostics.h`,
    `src/Diagnostics.cpp`, and the current variadic `args<T>` rejection tests
    for unsupported string pointer/reference pack elements or unsupported
    forwarding shapes.
  - acceptance:
    - The selected unsupported variadic-pack diagnostic has an explicit
      `DiagnosticContract` tier and stable message/span expectations.
    - A focused negative test asserts the stable diagnostic contract through
      the public diagnostics path.
    - Existing variadic positive coverage stays unchanged, and the task does
      not open a new materialization path without a reproduced unsupported
      non-string pack element.
  - stop_rule: Stop after exactly one lowerer/backend variadic diagnostic is
    tiered and source-locked; leave additional diagnostic families to later
    leaves.
