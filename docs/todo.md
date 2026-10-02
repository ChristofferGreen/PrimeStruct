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
| TODO-5388 | Shared lowerer-callback test factories for ir_pipeline validation tests | ready | test-infrastructure |
| TODO-5389 | Convert ir_pipeline validation inference_expr_kind / call_return_setup / get_return_info tests | deferred | test-infrastructure |
| TODO-5390 | Convert ir_pipeline validation statement_call_helper_buffer_store tests | deferred | test-infrastructure |
| TODO-5391 | Convert ir_pipeline validation statement_binding / conversions / on_error tests | deferred | test-infrastructure |
| TODO-5392 | Convert ir_pipeline validation inline_param_helper variadic-pack tests | deferred | test-infrastructure |
| TODO-5393 | Convert ir_pipeline validation setup_type_helper tests | deferred | test-infrastructure |
| TODO-5394 | Convert ir_pipeline validation result_helpers / count_access / inline_struct_arg tests | deferred | test-infrastructure |
| TODO-5395 | Replace source-text delegation checks in ir_pipeline validation tests with a table | deferred | test-infrastructure |
| TODO-5348 | Verify the iOS embed build and XCFramework packaging on macOS | deferred | embedding-ios |

### Ready Now

- TODO-5388 (track: test-infrastructure): Shared lowerer-callback test factories for ir_pipeline validation tests.

### Immediate Next 10

1. TODO-5388 - Shared lowerer-callback test factories for ir_pipeline validation tests.

### Priority Lanes

- Embedding (must support iOS): TODO-5348 (needs macOS)
- Test infrastructure: TODO-5388 -> 5389..5395 (the 5395 table conversion is independent)
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

- [ ] TODO-5388: Shared lowerer-callback test factories for ir_pipeline validation tests
  - owner: ai
  - status: ready
  - created_at: 2026-10-02
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - scope: Measured by `scripts/measure_test_duplication.py` (26.5% duplicated windows), the top clusters are the repeated lowerer-helper callback lists (`[](const primec::Expr &) { return false; }, [](const primec::Expr &, const LocalMap &, LocalInfo::StringSource &, int32_t &, bool &) { return true; }, ...`, 100+ repeats in 11 files). Add one tests-local header of named factories for the common callback sets (always-false predicates, always-true string-source resolvers, no-op emitters) and convert exactly one file (`..._inference_expr_kind_call_base_setup_infers_try_from_indexed_borrowed_and_po.cpp`, the largest duplicator) as the pattern.
  - acceptance:
    - factories header committed with a doc comment per factory; the converted file passes unchanged assertions.
    - each converted file keeps its TEST_CASE count (or merges only identical-assertion cases), and a documented mutation check (a removed lowerer branch) still fails a case.
    - `python3 scripts/measure_test_duplication.py` excess for the converted files drops, with before/after in the result note; full release gate green.
  - stop_rule: stop and leave the file unconverted if a mutation check (remove one lowerer branch the file covers) no longer fails some case after conversion; record the file in the leaf note.

- [ ] TODO-5389: Convert ir_pipeline validation inference_expr_kind / call_return_setup / get_return_info tests
  - owner: ai
  - status: deferred
  - created_at: 2026-10-02
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - depends_on: TODO-5388
  - scope: Using the TODO-5388 factories, convert `..._inference_expr_kind_dispatch_infers_try_from_indexed_map_tryat_args_pack_lo.cpp`, `..._inference_call_return_setup_resolves_namespaced_capacity_definition_directl.cpp`, `..._inference_get_return_info_step_reports_missing_definitions.cpp` into table-driven cases that keep per-row failure messages.
  - acceptance:
    - each converted file keeps its TEST_CASE count (or merges only identical-assertion cases), and a documented mutation check (a removed lowerer branch) still fails a case.
    - `python3 scripts/measure_test_duplication.py` excess for the converted files drops, with before/after in the result note; full release gate green.
  - stop_rule: stop and leave the file unconverted if a mutation check (remove one lowerer branch the file covers) no longer fails some case after conversion; record the file in the leaf note.

- [ ] TODO-5390: Convert ir_pipeline validation statement_call_helper_buffer_store tests
  - owner: ai
  - status: deferred
  - created_at: 2026-10-02
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - depends_on: TODO-5388
  - scope: Using the TODO-5388 factories, convert the four `..._statement_call_helper_buffer_store_direct_calls_helper_lowerer_*.cpp` files and `..._statement_call_helper_validates_function_table_diagnostics.cpp` into table-driven cases that keep per-row failure messages.
  - acceptance:
    - each converted file keeps its TEST_CASE count (or merges only identical-assertion cases), and a documented mutation check (a removed lowerer branch) still fails a case.
    - `python3 scripts/measure_test_duplication.py` excess for the converted files drops, with before/after in the result note; full release gate green.
  - stop_rule: stop and leave the file unconverted if a mutation check (remove one lowerer branch the file covers) no longer fails some case after conversion; record the file in the leaf note.

- [ ] TODO-5391: Convert ir_pipeline validation statement_binding / conversions / on_error tests
  - owner: ai
  - status: deferred
  - created_at: 2026-10-02
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - depends_on: TODO-5388
  - scope: Using the TODO-5388 factories, convert `..._statement_binding_helper_classifies_variadic_struct_reference_parameters.cpp`, `..._conversions_helper_rejects_immutable_assign_target.cpp`, `..._on_error_helpers_wire_definition_handlers.cpp` into table-driven cases that keep per-row failure messages.
  - acceptance:
    - each converted file keeps its TEST_CASE count (or merges only identical-assertion cases), and a documented mutation check (a removed lowerer branch) still fails a case.
    - `python3 scripts/measure_test_duplication.py` excess for the converted files drops, with before/after in the result note; full release gate green.
  - stop_rule: stop and leave the file unconverted if a mutation check (remove one lowerer branch the file covers) no longer fails some case after conversion; record the file in the leaf note.

- [ ] TODO-5392: Convert ir_pipeline validation inline_param_helper variadic-pack tests
  - owner: ai
  - status: deferred
  - created_at: 2026-10-02
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - depends_on: TODO-5388
  - scope: Using the TODO-5388 factories, convert the `..._inline_param_helper_*variadic*.cpp` files (borrowed/pointer vector, array, result, fileerror and map packs): these are near-identical per carrier type and fit one table keyed by carrier into table-driven cases that keep per-row failure messages.
  - acceptance:
    - each converted file keeps its TEST_CASE count (or merges only identical-assertion cases), and a documented mutation check (a removed lowerer branch) still fails a case.
    - `python3 scripts/measure_test_duplication.py` excess for the converted files drops, with before/after in the result note; full release gate green.
  - stop_rule: stop and leave the file unconverted if a mutation check (remove one lowerer branch the file covers) no longer fails some case after conversion; record the file in the leaf note.

- [ ] TODO-5393: Convert ir_pipeline validation setup_type_helper tests
  - owner: ai
  - status: deferred
  - created_at: 2026-10-02
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - depends_on: TODO-5388
  - scope: Using the TODO-5388 factories, convert the `..._setup_type_helper_*.cpp` files (count/capacity probing, wrapper string slash access, canonical map helper rejects, indexed args-pack pointer map receivers) into table-driven cases that keep per-row failure messages.
  - acceptance:
    - each converted file keeps its TEST_CASE count (or merges only identical-assertion cases), and a documented mutation check (a removed lowerer branch) still fails a case.
    - `python3 scripts/measure_test_duplication.py` excess for the converted files drops, with before/after in the result note; full release gate green.
  - stop_rule: stop and leave the file unconverted if a mutation check (remove one lowerer branch the file covers) no longer fails some case after conversion; record the file in the leaf note.

- [ ] TODO-5394: Convert ir_pipeline validation result_helpers / count_access / inline_struct_arg tests
  - owner: ai
  - status: deferred
  - created_at: 2026-10-02
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - depends_on: TODO-5388
  - scope: Using the TODO-5388 factories, convert `..._result_helpers_*.cpp`, `..._count_access_helpers_emit_count_access_calls.cpp`, `..._inline_struct_arg_helper_reports_diagnostics.cpp` into table-driven cases that keep per-row failure messages.
  - acceptance:
    - each converted file keeps its TEST_CASE count (or merges only identical-assertion cases), and a documented mutation check (a removed lowerer branch) still fails a case.
    - `python3 scripts/measure_test_duplication.py` excess for the converted files drops, with before/after in the result note; full release gate green.
  - stop_rule: stop and leave the file unconverted if a mutation check (remove one lowerer branch the file covers) no longer fails some case after conversion; record the file in the leaf note.

- [ ] TODO-5395: Replace source-text delegation checks in ir_pipeline validation tests with a table
  - owner: ai
  - status: deferred
  - created_at: 2026-10-02
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - scope: Convert `..._ir_lowerer_call_helpers_source_delegation_stays_stable.cpp` (3,755 lines) and `..._ir_validator_accepts_lowered_canonical_module.cpp` (3,080 lines): turn repeated `source.find(...)` blocks into (file, required snippets, forbidden snippets) rows with per-row failure messages into table-driven cases that keep per-row failure messages.
  - acceptance:
    - each converted file keeps its TEST_CASE count (or merges only identical-assertion cases), and a documented mutation check (a removed lowerer branch) still fails a case.
    - `python3 scripts/measure_test_duplication.py` excess for the converted files drops, with before/after in the result note; full release gate green.
  - stop_rule: stop and leave the file unconverted if a mutation check (remove one lowerer branch the file covers) no longer fails some case after conversion; record the file in the leaf note.

