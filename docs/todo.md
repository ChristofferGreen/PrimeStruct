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

- TODO-4604: Specify requirement contract phase split

### Immediate Next 10

- TODO-4605: Specify non-null pointer optionality
- TODO-4606: Specify capability-parameterized views
- TODO-4607: Publish initial array extent facts
- TODO-4608: Add checked array slice construction
- TODO-4609: Reject escaping local array slices
- TODO-4610: Add forward cursor traversal API
- TODO-4611: Add reverse cursor traversal API

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
- Safe array extents and capability views: TODO-4604 through TODO-4611 capture
  the agreed backlog from `docs/SafeArrayExtentViews.md`: requirement
  contracts, non-null safe pointers, capability-parameterized references and
  slices, semantic extent facts, checked slices, conservative view escapes, and
  cursor traversal with `limit(...)` / `reverse_limit(...)` boundaries.

### Execution Queue

1. TODO-4604: Specify requirement contract phase split
2. TODO-4605: Specify non-null pointer optionality
3. TODO-4606: Specify capability-parameterized views
4. TODO-4607: Publish initial array extent facts
5. TODO-4608: Add checked array slice construction
6. TODO-4609: Reject escaping local array slices
7. TODO-4610: Add forward cursor traversal API
8. TODO-4611: Add reverse cursor traversal API

### Task Blocks

- [ ] TODO-4604: Specify requirement contract phase split
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - parallel_track: requirement-contracts
  - scope: Update the normative language docs and example-style guidance so
    `require<...>` is the forced compile-time requirement form and
    `require(...)` is the contract form that is proven statically when
    possible and otherwise emits a runtime precondition check when pure and
    runtime-checkable.
  - implementation_notes: Start with `docs/PrimeStruct.md`,
    `docs/CodeExamples.md`, and the docs/source-lock tests that currently
    describe `require(...)` as compile-time-only syntax. Keep
    `docs/SafeArrayExtentViews.md` aligned with the final wording.
  - acceptance:
    - Docs clearly distinguish compile-time-only requirement viability from
      runtime-capable contracts.
    - Existing compile-time requirement examples are migrated or explicitly
      marked as legacy/proposed-transition syntax.
    - Source-lock coverage proves `docs/PrimeStruct.md`,
      `docs/CodeExamples.md`, and `docs/SafeArrayExtentViews.md` agree on the
      phase split.
  - stop_rule: Stop once the docs and source locks describe the phase split
    consistently; do not implement parser or semantic behavior in this leaf.

- [ ] TODO-4605: Specify non-null pointer optionality
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - depends_on: TODO-4604
  - scope: Update the pointer/reference and memory-capability design docs so
    safe `Pointer<T>` means a valid non-null pointer, optional or fallible
    pointer production uses `Maybe<Pointer<T>>` or
    `Result<Pointer<T>, ErrorT>`, and raw/foreign nullable addresses remain an
    unsafe/FFI adapter concern.
  - implementation_notes: Start with `docs/PrimeStruct.md`,
    `docs/MemoryCapabilities.md`, and `docs/SafeArrayExtentViews.md`. Keep
    current implementation limits clearly marked if heap intrinsics still
    return bare `Pointer<T>`.
  - acceptance:
    - Docs no longer imply that safe `Pointer<T>` has null as an ordinary
      inhabitant.
    - Allocation and FFI design examples use `Maybe<Pointer<T>>` or
      `Result<Pointer<T>, ErrorT>` when a valid pointer may not be produced.
    - Source-lock tests cover the non-null safe pointer invariant and the
      optional-pointer spelling.
  - stop_rule: Stop after the documented safe pointer model is consistent;
    leave heap intrinsic signature changes and FFI adapter implementation to
    later code leaves.

- [ ] TODO-4606: Specify capability-parameterized views
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
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
