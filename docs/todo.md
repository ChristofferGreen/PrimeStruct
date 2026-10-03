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
| TODO-5464 | Add an output sink to Vm::execute for capturing program output | deferred | test-matrix |
| TODO-5466 | Migrate duplicated vm/native compile-run cases to the program matrix | in_progress | test-matrix |
| TODO-5471 | Register form with promoted locals | deferred | opt-regform |
| TODO-5483 | Verify arm64 SextI32 and normalize the remaining i32 builtins | ready | ir-semantics |
| TODO-5478 | Remove the super-linear front-end cost on very large functions | deferred | compile-speed |
| TODO-5481 | Gate vm, native and optexe rows in the benchmark baseline | deferred | opt-bench |

### Ready Now

- TODO-5466 (track: test-matrix): Migrate duplicated vm/native compile-run cases to the program matrix (surface: tests/unit/compile_run, tests/unit/program_matrix, scripts/migrate_compile_run_cases.py).
- TODO-5483 (track: ir-semantics): Verify arm64 SextI32 and normalize the remaining i32 builtins (surface: src/ir_lowerer operator helpers, NativeEmitterInternalsArm64Arithmetic.h, tests/unit/program_matrix).

### Immediate Next 10

1. TODO-5466 - Migrate duplicated vm/native compile-run cases to the program matrix.
2. TODO-5483 - Verify arm64 SextI32 and normalize the remaining i32 builtins.

### Priority Lanes

- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix TODO-5466 (sink TODO-5464 deferred); i32 audit TODO-5483; VM speed ; passes ; optexe ; deferred: TODO-5471, TODO-5478

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

### Task Blocks

- [ ] TODO-5464: Add an output sink to Vm::execute for capturing program output
  - owner: ai
  - status: deferred
  - deferred_reason: the program matrix (TODO-5465) runs every config through the built primec/primevm, which needs no in-process capture; the sink only buys speed (an in-process VM run instead of a subprocess) and removes the dup2-based capture in test_ir_vm_run.h.
  - created_at: 2026-10-03
  - phase: Optimizing backends
  - parallel_track: test-matrix
  - scope: VM print and file-write-to-stdout opcodes write straight to fd 1/2 (`::write`) or `fwrite` in `VmIoHelpers.cpp`, so tests must shell out and redirect to read output. Add an optional `VmOutputSink` (stdout/stderr chunk callbacks) to the `Vm::execute` overload family and to `VmKernelHost`, defaulting to today's exact fd writes, so a test or embedder can run a module in process and capture output. Do the same for `VmDebugSession::start` only if it is a one-line change. This is the prerequisite for the in-process program matrix (TODO-5465). Design: docs/OptimizingBackendsPlan.md section 8.2.
  - implementation_notes: `scripts/check_no_direct_stdio.py` forbids std::cout/cerr in production sources; use the existing fd/`fwrite` helpers behind the sink rather than iostreams. `VmKernelHost` is virtual and has `handlePrintInstruction`; thread the sink through `RuntimeVmKernelHost` in `src/runtime/VmExecution.cpp`.
  - acceptance:
    - a unit test runs a module that prints to stdout and stderr and receives the exact bytes, in order, through the sink; default behavior is unchanged (existing vm compile-run suites green)
    - no measurable slowdown on `benchmarks/aggregate.prime` under `--emit=vm` (within noise of the 2026-10-03 baseline)
  - stop_rule: do not buffer or reorder output in the default path; do not touch native or wasm output.

- [~] TODO-5466: Migrate duplicated vm/native compile-run cases to the program matrix
  - owner: ai
  - status: in_progress
  - depends_on: TODO-5465 (done: tests/unit/program_matrix/program_matrix.h runs a ProgramCase through every config via the built primec/primevm)
  - created_at: 2026-10-03
  - phase: Optimizing backends
  - parallel_track: test-matrix
  - progress: batch 1 done - the 22 native control cases that only check an exit code and stdout now live only in tests/unit/program_matrix/test_program_matrix_control.cpp and run on vm-step, vm, native at -O0/-O2 (deleted from native_backend/control; shard count 31 -> 9). Survey of the rest (2026-10-03): vm/ has 961 cases and native_backend/ 926; only 284 and 262 of them have the plain "run, check exit code" shape, 125 program sources appear in both in that shape and 117 of those expect the same exit code with no extra flags. The other shapes check stdout/stderr files, diagnostics or go through helper functions (216 and 235 distinct tails).
  - scope: Write `scripts/migrate_compile_run_cases.py` that rewrites the mechanical shapes into `ProgramCase` declarations, preserving case names and order, and reports every case it cannot convert; convert the 117 plain duplicate pairs first (one program case on vm-O0/vm-O2/native-O0/native-O2 replaces two cases), then the plain single-backend shapes, one suite per commit, regenerating shard counts and `tests/TEST_INVENTORY.md`. Design: docs/OptimizingBackendsPlan.md section 8.3. Note that a matrix case runs a program on 4-5 configs, so migrate only programs whose extra configs are worth their run time (pairs, optimizer-sensitive programs); do not convert the diagnostic and stdout-file shapes.
  - acceptance:
    - the reconciliation count is printed and matches; no program source is lost (script self-test under tests/scripts)
    - full release gate green with total compile-run wall time not higher than before
  - stop_rule: convert in batches of one suite per commit; if a batch changes a test's verdict, revert that batch and record the case.

- [ ] TODO-5483: Verify arm64 SextI32 and normalize the remaining i32 builtins
  - owner: ai
  - status: ready
  - created_at: 2026-10-03
  - phase: Optimizing backends
  - parallel_track: ir-semantics
  - scope: TODO-5477 made user-level i32 `plus`/`minus`/`multiply`/`divide`/`negate` wrap through the `SextI32` opcode. Two gaps remain. (1) `Arm64Emitter::emitSextI32` (SXTW x0, w0) was written from the encoding and never executed: run the native conformance and matrix cases on an arm64 macOS machine and add an encoding test. (2) Builtins that compute i32 values in lowering without going through the arithmetic helper (`abs`, `pow`, `clamp`, `saturate`, `round`, `increment`/`decrement` helpers, `sign`) still leave unnormalized slots on overflow; emit SextI32 where an i32 result can leave the 32-bit range and add a matrix case per builtin.
  - acceptance:
    - the `i32 arithmetic wraps` matrix cases pass on arm64 macOS native; an encoding unit test pins the SXTW bytes
    - each audited builtin either has a matrix case showing identical output on vm, native, optexe and exe for overflowing input, or a comment explaining why its result cannot overflow
  - stop_rule: do not change i64/u64 behavior or the I32 arithmetic opcodes themselves; lowering also uses them for address arithmetic.

- [ ] TODO-5471: Register form with promoted locals
  - owner: ai
  - status: deferred
  - deferred_reason: optexe (TODO-5472..5475) works directly on the stack form with the shared CFG and lets the host compiler allocate registers, so the register form is only needed by the native code generator (docs/OptimizingBackendsPlan.md Phase 3); reopen when that phase is filed.
  - depends_on: TODO-5470, TODO-5467
  - created_at: 2026-10-03
  - phase: Optimizing backends
  - parallel_track: opt-regform
  - scope: Extend the block virtual-register form so every non-pinned local becomes a virtual register: `LoadLocal`/`StoreLocal` of a promoted slot become register uses/defs, and block edges carry moves for live promoted locals as they already do for stack values. Pinned slots keep their memory instructions. Update the verifier and liveness for the new defs. No code generator consumes this yet except tests. Design: docs/OptimizingBackendsPlan.md Phase 2.1. Depends on the CFG utilities of TODO-5467.
  - acceptance:
    - a round-trip test (lower to register form, lift back to stack IR) leaves VM results identical for the ir-pipeline corpus with promotion on
    - verifier rejects a promoted local that is read before any def on some path (negative test)
  - stop_rule: do not run any optimization on the register form here; if lifting back cannot preserve behavior for loops, record it and keep promotion behind a test-only switch.

- [ ] TODO-5478: Remove the super-linear front-end cost on very large functions
  - owner: ai
  - status: deferred
  - deferred_reason: pre-existing and outside the optimizer programme; recorded so the compile-time gate of TODO-5476 is read correctly.
  - created_at: 2026-10-03
  - phase: Optimizing backends
  - parallel_track: compile-speed
  - scope: `primec`/`primevm` take 3.3 s for a 2,000-statement `main`, 6.5 s for 4,000, 17 s for 8,000 and 84 s for 20,000 (185,739 IR instructions), all before any emitter or the host compiler runs (host clang is ~1 s of that at every size for foldable programs). Profile semantics validation and lowering on that shape and remove the super-linear step. Reproducer generator: docs/OptimizingBackendsPlan.md section 9.1.
  - acceptance:
    - the 20,000-statement reproducer compiles at least 4x faster with identical IR output
  - stop_rule: if the cost is inherent to a data structure shared with the semantic product, record the profile and stop.

- [ ] TODO-5481: Gate vm, native and optexe rows in the benchmark baseline
  - owner: ai
  - status: deferred
  - deferred_reason: numbers are recorded by scripts/benchmark_backends.py (TODO-5463); gating needs a stable-timing environment decision (CI runners are noisy) and the 25% ratio policy applied to rows that move 5x between releases.
  - created_at: 2026-10-03
  - phase: Optimizing backends
  - parallel_track: opt-bench
  - scope: Feed the `vm-O2`, `native-O2` and `optexe-O2` rows of `scripts/benchmark_backends.py --json` into `scripts/benchmark.sh --report-json`/`--baseline-json` and `scripts/check_benchmark_report.py`, with baseline entries per backend and level, so a regression in the VM kernel, the native emitter or the optimizer fails the benchmark gate. Fix `benchmarks/README.md`, which still calls `--emit=exe` a speed baseline (it is the old C++ emitter at `clang++ -O0`; the C/C++ reference programs are the baseline). Design: docs/OptimizingBackendsPlan.md Phase 5.3.
  - acceptance:
    - the gate fails when a row regresses past the ratio and passes on the recorded baseline; the benchmark-harness test covers the new entries
  - stop_rule: do not tighten existing cpp thresholds; new entries use the existing 25% ratio.

