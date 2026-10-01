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
| TODO-5343 | iOS-safe embed build: no process spawning, bundled stdlib, cross-compile check | ready | embedding-ios |
| TODO-5347 | C++ to script string arguments for exported functions | ready | embedding-values |
| TODO-5341 | Embedding lifetime: arena scope, reentrancy, compile-once run-many | blocked | embedding-lifetime |

### Ready Now

- TODO-5347 (track: embedding-values, surface: `src/embed/`, reserved `__psarg_str`): string arguments to exports.
- TODO-5343 (track: embedding-ios, surface: `src/support/ProcessRunner.cpp`, `ImportResolver`, CMake option, iOS toolchain recipe): iOS-safe build.

### Immediate Next 10

1. TODO-5347 - strings into exports.
2. TODO-5341 - lifetime and reuse hardening.
3. TODO-5343 - iOS build recipe; needs a macOS runner to fully verify.

### Priority Lanes

- Embedding (top priority, user-set; must support iOS): TODO-5347 -> 5341

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10; 5340 follows 5345, 5341 follows 5340.

### Task Blocks

- [ ] TODO-5347: C++ to script string arguments for exported functions
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Embedding
  - parallel_track: embedding-values
  - depends_on: TODO-5346
  - scope: pass `std::string_view` arguments into exports (`fn([string]) -> i32`).
    Per call, run a copy of the export's module whose string table has the
    arguments appended; the wrapper fetches the index through the reserved
    `__psarg_str` host function (the only host function allowed to return
    `string`, reserved by the `__ps` prefix, which user host definitions may
    not use). String results are not supported.
  - acceptance:
    - `Script::call<int32_t>("count_chars", "hello")` style call returns the
      script's answer; embedded NUL and empty strings work; mismatch diagnosed.
  - stop_rule: no string returns; if the per-call module copy is too slow for
    large modules, record the cost and stop.

- [ ] TODO-5341: Embedding lifetime - arena scope, reentrancy, compile-once run-many
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5340
  - created_at: 2026-10-01
  - phase: Embedding
  - parallel_track: embedding-lifetime
  - scope: the CLI wraps compile+run in a process-wide `ScopedCompileArena`
    (see TODO-5233/5234/5235 notes in `docs/CompilerArenaAllocator.md`);
    an embedded engine must own its arena lifetime safely, support several
    engines and repeated `run`/`call` on one compiled `Script` without
    recompiling, and document thread-safety (one `Script` per thread, or
    guarded).
  - acceptance:
    - test compiles once and runs 1000 times with stable results and no
      memory growth (RSS or allocation-counter bound).
    - two engines alive at once on separate threads pass under TSAN smoke.
  - stop_rule: if the arena is inherently process-global, document the
    single-engine-per-process limit and stop rather than rewriting it.

- [ ] TODO-5343: iOS-safe embed build - no process spawning, bundled stdlib, cross-compile check
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Embedding
  - parallel_track: embedding-ios
  - scope: make `primec_embed_runtime_lib` (and, as a stretch, the full
    `primec_embed_lib` for on-device compiling of small scripts) build for
    iOS: gate `fork`/`exec`/`posix_spawn` users (`src/support/ProcessRunner.cpp`,
    `ImportResolver` archive roots, `TempPaths`) behind a platform option so
    iOS builds exclude them; no reliance on `/tmp` or the working
    directory; provide the stdlib as a bundle path or embedded blob for the
    full-compile variant; ensure no executable-memory allocation. Add a CMake
    toolchain recipe (`-DCMAKE_SYSTEM_NAME=iOS`) and an XCFramework packaging
    script.
  - implementation_notes: this Linux CI cannot build or run iOS. Verify with
    a macOS runner (or have the user run the recipe) and record the exact
    toolchain version; until then acceptance covers the portable parts
    (a Linux build with the process-spawning sources excluded must pass the
    embed tests).
  - acceptance:
    - Linux build with `PRIMESTRUCT_EMBED_NO_PROCESS=ON` links and passes the
      runtime-only bytecode test.
    - documented, reproducible iOS cross-compile recipe; compile of
      `primec_embed_runtime_lib` for iOS arm64 verified on macOS.
  - stop_rule: if the full compiler pipeline cannot drop process spawning
    cleanly, ship runtime-only for iOS and record the gap.

