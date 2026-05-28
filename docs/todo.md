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

- TODO-4609: Reject escaping local array slices | track: array-slice-escape-diagnostics | surface: slice view lifetime diagnostics
- TODO-4610: Add forward cursor traversal API | track: cursor-forward-traversal | surface: forward cursor traversal

### Immediate Next 10

- TODO-4611: Add reverse cursor traversal API
- TODO-4612: Add safe extent and cursor code examples

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
  semantic unknown-call diagnostic stability slice. TODO-4616 made the
  semantic validation manifest executable. TODO-4619 completed the runtime
  reflection backend-profile capability gate. TODO-4620 completed indexed
  expanded-source diagnostic lookup.
- Architecture review hardening: TODO-4613 through TODO-4616 retired the
  temporary semantic/lowerer/emitter source locks and made the semantic
  validation manifest executable. TODO-4619 completed the second backend
  capability gate, and TODO-4620 completed deterministic indexed
  expanded-source diagnostic lookup. TODO-4617 completed preflight
  stale/missing diagnostics, TODO-4618 completed CT-eval
  requirement-predicate fail-closed coverage, and TODO-4621 completed one
  lowerer/backend variadic diagnostic stability-tier promotion.
- Safe array extents and capability views: TODO-4604 completed the requirement
  contract phase split, and TODO-4605 completed the non-null safe pointer
  optionality model. TODO-4606 specified the capability-parameterized
  reference/slice view model in the normative docs. TODO-4607 published the
  initial semantic-product array extent facts, and TODO-4608 added the first
  checked read-only array slice construction surface. TODO-4609 through
  TODO-4612 remain from the agreed backlog in `docs/SafeArrayExtentViews.md`:
  conservative view escapes, cursor traversal with
  `limit(...)` / `reverse_limit(...)` boundaries, and style-aligned examples
  once the surface is specified.

### Execution Queue

1. TODO-4609: Reject escaping local array slices
2. TODO-4610: Add forward cursor traversal API
3. TODO-4611: Add reverse cursor traversal API
4. TODO-4612: Add safe extent and cursor code examples

### Task Blocks

- [ ] TODO-4609: Reject escaping local array slices
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - parallel_track: array-slice-escape-diagnostics
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
  - parallel_track: cursor-forward-traversal
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
