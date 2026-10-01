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
13. Treat failing release-test cases as the top priority queue item: before
    starting new implementation work, update `docs/failing_tests.md`, fix the
    oldest reproducible failure first, and keep `docs/todo.md` aligned with the
    active test-fix work.
14. Every release test run must record any failing cases in
    `docs/failing_tests.md` before broader work continues.
15. When completing a task, mark it `[x]`, add `finished_at` plus a short
    evidence note, move the full block to `docs/todo_finished.md`, and remove
    it from this file.
16. This file is read fresh by an LLM agent each session, not browsed by a
    human - optimize for grep-ability over prose. Keep each task block to
    its current state (scope/acceptance/stop_rule), not a narrative history.
    Dated investigation/progress notes go in `docs/todo_log.md` instead,
    under that task's own `## TODO-XXXX` heading - when a task closes, fold
    whatever's still relevant into its `docs/todo_finished.md` resolution
    note and delete its `docs/todo_log.md` section.
17. Every active leaf must set `status` to exactly one of `ready` (its own
    repro/acceptance gap is confirmed and nothing external blocks starting),
    `blocked` (a specific, currently-`[ ]` `TODO-XXXX` must close first -
    name it in `blocked_on`; if that TODO is actually closed, the leaf is
    not blocked - fix the status instead of leaving it stale), or `deferred`
    (deprioritized, needs further scoping, or of confirmed-low value, with
    no single external blocker). Only `ready` leaves belong in `Ready Now`.

## Task Template

```md
- [ ] TODO-<id>: Short title
  - owner: ai|human
  - status: ready|blocked|deferred
  - blocked_on: TODO-XXXX (required when status: blocked; omit otherwise)
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

Dated investigation history for this task goes in `docs/todo_log.md` under
a matching `## TODO-<id>` heading, not inline here.

## Open Tasks

### Queue Summary

Generated from each task block's own `status`/`parallel_track` fields -
re-derive after editing any block rather than hand-editing this table out
of sync with them.

| ID | Title | Status | Track |
| --- | --- | --- | --- |
| TODO-4737 | Add a lowered-module invariant for method-call targets | ready | lowered-module-invariant |

### Ready Now

- TODO-4737 (track: lowered-module-invariant, surface: `src/ir_lowerer/IrLowererSetupTypeMethodCallResolution.cpp` and the shared `isBuiltinClassifiedMethodCallTarget` helper in `IrLowererHelpers.{h,cpp}`): share one builtin-classification predicate across all method-call-target sites, then add the lowered-module invariant pass.

### Immediate Next 10

1. TODO-4737 - largest; needs a before/after diff of the full `ir.pipeline.validation` suite, so take it last.

### Priority Lanes

- Lowering correctness tooling: TODO-4737

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10; only TODO-4737 remains.

### Task Blocks




- [ ] TODO-4737: Add a lowered-module invariant - no published method-call target without a materialized definition or builtin classification
  - owner: ai
  - status: ready
  - created_at: 2026-07-20
  - phase: Test infrastructure
  - parallel_track: lowered-module-invariant
  - scope: the TODO-4731 gap (c) class (semantic product publishes a
    method-call target the lowerer has no definition for) is invisible
    to semantic-product goldens and only surfaced case-by-case. A
    single validator pass over every lowered module asserting the
    invariant catches the whole class everywhere, replacing dozens of
    per-shape "does this lower" cases and guarding future
    materialization gaps by construction. The "materialized definition
    or builtin classification" check already exists but is scattered
    across 6 call sites in two files
    (`IrLowererInlineNativeCallDispatch.cpp:1605,1916` and
    `IrLowererSetupTypeMethodCallResolution.cpp:650,709,753,759`), each
    independently reimplementing a similar-but-not-identical exemption
    whitelist via hardcoded string-literal comparisons - see
    implementation_notes for the full exemption inventory and why a
    general invariant can't just be a static path allowlist.
  - implementation_notes: a genuinely correct, general invariant pass
    needs either (a) fully re-deriving and generalizing all 6 call
    sites' accumulated special-casing (real risk of shipping false
    positives, or missing a nuance and shipping a pass that doesn't
    actually catch anything), or (b) having each of the 6 call sites
    call OUT to one new shared `isBuiltinClassifiedMethodCallTarget(target,
    semanticProgram, callExpr)` helper instead of their own inline
    exemption list, then having the new invariant pass call that SAME
    shared helper - the safer design, provably consistent with existing
    lowering behavior by construction. Half of (b) already shipped (see
    log): `IrLowererInlineNativeCallDispatch.cpp`'s two call sites had a
    byte-identical 5-clause exemption predicate (`/string/count`,
    `/std/collections/vector/count`, `/vector/capacity`, `/soa/count`,
    `/vector/at`+`/at_unsafe`, `/soa/to_aos`), extracted into
    `isBuiltinClassifiedMethodCallTarget(target, callExpr)` in
    `IrLowererHelpers.h`/`.cpp`. `IrLowererSetupTypeMethodCallResolution.cpp`'s
    other 4 call sites remain unextracted: their exemptions
    (`routesExplicitVectorCountMethodThroughArgsPackCount`,
    `directTargetKeepsSyntheticCollectionFallback`,
    `allowsReceiverResolvedVectorMetadataFallback`) are woven into a
    local `resolveLoweredDefinitionPath` lambda doing receiver-type-
    dependent fuzzy path matching, not extractable into a target-string-
    only predicate without either unsafely re-deriving that logic or a
    dedicated decomposition pass over that function first (TODO-4724
    decomposed a *different* function - `SemanticsValidator::resolveMethodTarget`
    in `SemanticsValidatorExprMethodTargetResolution.cpp` - and closed
    without touching this one; it provides no seam here, so this is not
    `blocked_on` anything, just unstarted scoped work). Once a seam
    exists and both remaining files call the shared helper, the new
    invariant pass itself is comparatively small: iterate
    `semanticProgram->methodCallTargets`/`directCallTargets`, for each
    published target call `resolveLoweredDefinitionPath` and the shared
    helper, and error if neither the definition nor the
    builtin-classification check succeeds.
  - acceptance: invariant runs in the lowering pipeline under a test
    flag; deliberately re-introducing the gap (c) bug trips it.
  - stop_rule: do not re-derive `IrLowererSetupTypeMethodCallResolution.cpp`'s
    4 remaining fuzzy-path exemptions from scratch without a real
    before/after diff of the full `ir.pipeline.validation` suite (as the
    2026-07-23c log entry below did for the first 2 sites) - a subtly
    wrong re-derivation would silently narrow or widen which method-call
    targets require a materialized definition, exactly the class of bug
    this task exists to catch.
