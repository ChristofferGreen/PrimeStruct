# PrimeStruct TODO Log

## Purpose

This file is the live open-work queue for PrimeStruct.

- Keep only open work here: `[ ]` queued or `[~]` in progress.
- Move completed work to `docs/todo_finished.md` (below its marker), then run
  `python3 scripts/archive_todo_finished.py`: it files the block verbatim under
  `docs/todo_archive/` and regenerates the index (`grep TODO-NNNN
  docs/todo_finished.md` finds the archive file).
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
    evidence note, move the full block below the marker in `docs/todo_finished.md`,
    run `scripts/archive_todo_finished.py` (moves it into
    `docs/todo_archive/<YYYY-MM>.md` and regenerates the index), and remove
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
| TODO-5356 | Collapse near-duplicate ir_pipeline validation tests into table-driven suites | deferred | test-infrastructure |
| TODO-5348 | Verify the iOS embed build and XCFramework packaging on macOS | deferred | embedding-ios |

### Ready Now

- TODO-5385 (track: semantics-structure): Decompose the ~3,500-line rewriteExpr in TemplateMonomorphExpressionRewrite.cpp.

### Immediate Next 10

1. TODO-5385 - Decompose the ~3,500-line rewriteExpr in TemplateMonomorphExpressionRewrite.cpp.

### Priority Lanes

- Embedding (must support iOS): TODO-5348 (needs macOS)
- Semantics structure: TODO-5385
- Test infrastructure: TODO-5356 (deferred)
- IR/VM structure: TODO-5361 -> 5362

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

### Task Blocks

- [ ] TODO-5348: Verify the iOS embed build and XCFramework packaging on macOS
  - owner: human
  - status: deferred
  - created_at: 2026-10-01
  - phase: Embedding
  - parallel_track: embedding-ios
  - scope: split from TODO-5343, which finished everything verifiable on Linux
    (process spawning compiled out via `PRIMESTRUCT_EMBED_NO_PROCESS`,
    `PRIMESTRUCT_EMBED_ONLY`, forbidden-symbol scan, `scripts/check_embed_no_process.sh`,
    iOS section in docs/Embedding.md). What remains needs macOS + Xcode, which
    this repository's CI and agent sandboxes do not have: run
    `scripts/build_ios_embed.sh`, fix any toolchain errors it exposes (iOS SDK
    availability of APIs used under `__APPLE__`, e.g. `mach/mach.h` task_info in
    the semantics validator, `std::filesystem` deployment target), link the
    runtime XCFramework into a sample iOS app target that loads bytecode and
    calls a bound host function on device and simulator, and add a macOS CI job
    running the script.
  - acceptance:
    - `scripts/build_ios_embed.sh` succeeds on macOS for device and simulator
      and produces both XCFrameworks.
    - a sample app runs `Script::loadBytecode` + `bind` + `run` on the simulator.
    - result (Xcode version, deployment target, library sizes) recorded here.
  - stop_rule: if an API used by the full compiler is unavailable on iOS, ship
    the runtime-only XCFramework and record the gap rather than widening the
    scope.

- [ ] TODO-5385: Decompose the ~3,500-line rewriteExpr in TemplateMonomorphExpressionRewrite.cpp
  - owner: ai
  - status: ready
  - created_at: 2026-10-02
  - phase: Compiler structure
  - parallel_track: semantics-structure
  - scope: (also covers the other single-giant-function files left on the allowlist: `SemanticsValidatorStatementBindings.cpp` validateBindingStatement ~2,150 lines, `SemanticsValidatorExpr.cpp` validateExpr ~1,870, `TemplateMonomorphImplicitTemplateInference.cpp` inferImplicitTemplateArgs ~1,430. `SemanticsValidatorStatementReturns.cpp`, `...InferMethodResolution.cpp` and `...InferCollectionReturnInference.cpp` were brought under 1,200 lines by moving helpers and sibling functions out. Measured: lambdas are only ~500/3,500 lines of rewriteExpr and ~0 of validateExpr, so lambda extraction alone is not enough; the straight-line if-chains need an explicit shared-state struct and phase functions.) `rewriteExpr` is one function (lines ~319-3876) whose branches share many local lambdas; it cannot be split by moving code. Extract the branches (name-expression rewrites, method-call rewrites, bare-call rewrites, collection helper rewrites) into functions over an explicit shared-state struct, then move them into focused units.
  - acceptance:
    - `TemplateMonomorphExpressionRewrite.cpp` and the other files named above are each split under 1,200 lines and the size allowlist is empty.
    - full release gate green; semantic-product dumps byte-identical (compare old/new `primec` on the examples).
  - stop_rule: behavior-preserving extraction only; stop and record if a branch depends on control flow that cannot be expressed without a logic change.

- [ ] TODO-5356: Collapse near-duplicate ir_pipeline validation tests into table-driven suites
  - owner: ai
  - status: deferred
  - created_at: 2026-10-01
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - scope: `tests/unit/ir_pipeline/validation/` has hundreds of single-purpose files
    with 100+ character names (several over 3,000 lines, e.g.
    `..._call_helpers_source_delegation_stays_stable.cpp`). Needs scoping:
    measure duplication (shared setup blocks), then convert groups to table-
    driven cases that keep per-row failure messages, keeping test count and
    coverage (mutation check: delete a lowerer branch and confirm some row
    fails).
  - acceptance:
    - measured duplication report committed first, listing the groups to
      convert.
    - each converted group keeps coverage: a documented mutation (lowerer
      branch removed) still fails a row.
    - files and lines removed, suite runtime before/after in the result note.
  - stop_rule: stop if coverage cannot be shown equivalent for a group; leave that group as
    is.
