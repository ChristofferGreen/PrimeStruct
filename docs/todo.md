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
| TODO-5493 | Inferred bindings in generic struct helpers leak a diagnostic span | ready | diagnostics |
| TODO-5498 | Destroy and copy single-field structs | ready | lifecycle |
| TODO-5494 | Local pointers cannot escape into containers or outer locals | ready | escapes |
| TODO-5483 | Verify arm64 SextI32 on a macOS machine | deferred | ir-semantics |

### Ready Now

- TODO-5493 (diagnostics): inferred bindings in generic struct helpers leak a diagnostic span
- TODO-5494 (escapes): local pointers cannot escape into containers or outer locals
- TODO-5498 (lifecycle): destroy and copy single-field structs

### Immediate Next 10

1. TODO-5493
2. TODO-5494
3. TODO-5498

### Priority Lanes

- Memory safety and parameter modes (docs/spec/value-lifecycle.md Parameter Passing; docs/spec/type-system.md Ownership and Mutability): TODO-5494, TODO-5498
- Diagnostics: TODO-5493
- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix; arm64 SextI32 TODO-5483 (needs macOS); VM speed ; passes ; optexe

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

### Task Blocks

- [ ] TODO-5493: Inferred bindings in generic struct helpers leak a diagnostic span
  - owner: ai
  - status: ready
  - created_at: 2026-10-05
  - phase: Diagnostics
  - parallel_track: diagnostics
  - scope: Validating an untyped binding inferred from a field of `other` in a generic struct helper such as `Copy([Reference<Self>] other)` (for example `[mut] allocCount{other.fieldCapacity}` in `Vector<T>.Copy`) sets the diagnostic primary span, so a later, unrelated semantic error in user code is reported at the stdlib helper's line instead of its own. The stdlib `Copy` helpers now declare those bindings' types (TODO-5487), which hides it. Find where the speculative inference captures the span without an error, and stop it.
  - acceptance:
    - a program that copies a `Vector` and then calls an unknown function reports the error at the call's own line, with the stdlib binding left untyped; regression test in the semantics diagnostics suite
    - full release gate green
  - stop_rule: diagnostics only; no inference behavior change.

- [ ] TODO-5494: Local pointers cannot escape into containers or outer locals
  - owner: ai
  - status: ready
  - created_at: 2026-10-05
  - phase: Memory safety
  - parallel_track: escapes
  - scope: TODO-5490 rejects a pointer rooted at a local when it is returned or assigned through a parameter. Also reject it when it is pushed or inserted into a container that outlives the local (a parameter's container, or a local declared in an enclosing scope), assigned to a binding declared in an enclosing block scope, or passed to a `move`/`copy` parameter whose callee keeps it; outside `[unsafe]` only.
  - acceptance:
    - negative tests for each route (`pointer escapes via argument to <callee>` / `via assignment to <target>`) and positive tests for same-scope use
    - full release gate and corpus scan green
  - stop_rule: compile-time only; heap pointers from `alloc` stay out of scope.

- [ ] TODO-5498: Destroy and copy single-field structs
  - owner: ai
  - status: ready
  - created_at: 2026-10-05
  - phase: Memory safety
  - parallel_track: lifecycle
  - scope: A struct with one scalar field still lowers as an array handle, so a `Destroy` or `Copy` helper on it never runs. Giving it the struct representation (as two-field uniform structs now get) breaks `try` on a `Result` of it: `try` treats a single-slot struct payload as packed into the scalar `Result`, while map `tryAt` and stdlib `Result` sums return it by pointer (map conformance `ownership-sensitive experimental map value methods` reads garbage). Make the packed and pointer conventions agree, then drop the single-field exception in `makeStructLayoutResolutionAdaptersWithOwnedSlotState`.
  - acceptance:
    - compile-run tests on VM, native and C++: a single-field struct with a counting `Destroy` is destroyed once at scope end and copied through `Copy`; `try` on a map `tryAt` of it returns the stored value
    - full release gate green
  - stop_rule: no change to the packed `Result` encoding of scalar payloads.

- [ ] TODO-5483: Verify arm64 SextI32 on a macOS machine
  - owner: ai
  - status: deferred
  - deferred_reason: needs an arm64 macOS machine; the Linux x86_64 session cannot run `Arm64Emitter` output.
  - created_at: 2026-10-03
  - phase: Optimizing backends
  - parallel_track: ir-semantics
  - scope: `Arm64Emitter::emitSextI32` (SXTW x0, w0, encoded 0x93407C00) was written from the encoding and never executed. The i32 builtins audit is done: increment, decrement, abs and pow emit SextI32, and integer lerp, saturate, clamp, min, max and sign already agree at the limits on every backend (matrix cases `i32_wrap_builtins` and `i32_limit_builtins`; lerp and clamp go through plus/minus/multiply, which wrap).
  - acceptance:
    - the `i32` matrix cases (`i32_wrap_basic`, `i32_wrap_loops`, `i32_wrap_builtins`, `i32_limit_builtins`) pass on arm64 macOS native; an encoding unit test pins the SXTW bytes (done: `primestruct.ir.native_codegen` checks SXTW and the float-compare branch conditions through `primec/testing/NativeEmitterEncodings.h`; the float compares now use MI/LS so NaN compares false, also unexecuted)
  - stop_rule: do not change i64/u64 behavior or the I32 arithmetic opcodes themselves; lowering also uses them for address arithmetic.
