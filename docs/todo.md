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
| TODO-5373 | Extend the collection parity matrix to Reference receivers and the borrowed `_ref` helpers | ready | collection-defects |
| TODO-5372 | `soa<T>` method spellings publish internal `soaVector*` helper targets instead of the public helper | ready | collection-defects |
| TODO-5371 | array `.at(i)` method resolves to the vector helper and fails argument type checking | ready | collection-defects |
| TODO-5370 | map bare `contains(m, k)` passes semantics but fails VM lowering while `m.contains(k)` works | ready | collection-defects |
| TODO-5369 | map `.at()` and `.at_unsafe()` method calls fail with `unknown call target /map/at` | ready | collection-defects |
| TODO-5350 | Route semantics and dump rewrites through one collection target table | ready | collection-resolution |
| TODO-5351 | Route lowerer builtin-classification exemptions through the collection target table | blocked | collection-resolution |
| TODO-5352 | Measure and cut semantics header fan-out (SemanticsValidator.h) | ready | semantics-structure |
| TODO-5353 | Split TemplateMonomorphExpressionRewrite.cpp into focused units | ready | semantics-structure |
| TODO-5354 | Guard test registration: generated shards and an unregistered-test check | ready | test-infrastructure |
| TODO-5355 | Ban wall-clock comparisons in tests | ready | test-infrastructure |
| TODO-5356 | Collapse near-duplicate ir_pipeline validation tests into table-driven suites | deferred | test-infrastructure |
| TODO-5357 | Cut the release gate time: re-shard from measured costs | blocked | test-infrastructure |
| TODO-5358 | Inventory compiler global state and design a per-compilation context | ready | compiler-state |
| TODO-5359 | Move compiler caches into a per-compilation context | blocked | compiler-state |
| TODO-5360 | Remove ScopedCompileArena reset callbacks and the arena magic-static workarounds | blocked | compiler-state |
| TODO-5361 | Single opcode descriptor table for IR (stack effect, targets, serialization) | ready | ir-vm-structure |
| TODO-5362 | Unify the VM execution kernel and the debug session interpreter | blocked | ir-vm-structure |
| TODO-5363 | Spec: VM-owned dynamic strings (design decision) | ready | vm-strings |
| TODO-5364 | VM string heap: dynamic string indices in the interpreter | blocked | vm-strings |
| TODO-5365 | Embed API: string results and indexable string arguments | blocked | vm-strings |
| TODO-5366 | Archive docs/todo_finished.md and keep a greppable index | ready | docs-hygiene |
| TODO-5367 | Trim docs/failing_tests.md to current failures only | ready | docs-hygiene |
| TODO-5368 | Split docs/PrimeStruct.md into stable spec sections | deferred | docs-hygiene |
| TODO-5348 | Verify the iOS embed build and XCFramework packaging on macOS | deferred | embedding-ios |

### Ready Now

- TODO-5350 (track: collection-resolution): Route semantics and dump rewrites through one collection target table.
- TODO-5352 (track: semantics-structure): Measure and cut semantics header fan-out (SemanticsValidator.h).
- TODO-5353 (track: semantics-structure): Split TemplateMonomorphExpressionRewrite.cpp into focused units.
- TODO-5354 (track: test-infrastructure): Guard test registration: generated shards and an unregistered-test check.
- TODO-5355 (track: test-infrastructure): Ban wall-clock comparisons in tests.
- TODO-5358 (track: compiler-state): Inventory compiler global state and design a per-compilation context.
- TODO-5361 (track: ir-vm-structure): Single opcode descriptor table for IR (stack effect, targets, serialization).
- TODO-5363 (track: vm-strings): Spec: VM-owned dynamic strings (design decision).

### Immediate Next 10

1. TODO-5350 - Route semantics and dump rewrites through one collection target table.
2. TODO-5354 - Guard test registration: generated shards and an unregistered-test check.
3. TODO-5355 - Ban wall-clock comparisons in tests.
4. TODO-5361 - Single opcode descriptor table for IR (stack effect, targets, serialization).
5. TODO-5358 - Inventory compiler global state and design a per-compilation context.
6. TODO-5352 - Measure and cut semantics header fan-out (SemanticsValidator.h).
7. TODO-5353 - Split TemplateMonomorphExpressionRewrite.cpp into focused units.
8. TODO-5363 - Spec: VM-owned dynamic strings (design decision).
9. TODO-5366 - Archive docs/todo_finished.md and keep a greppable index.
10. TODO-5367 - Trim docs/failing_tests.md to current failures only.

### Priority Lanes

- Embedding (must support iOS): TODO-5348 (needs macOS)
- Collection resolution: TODO-5350 -> 5351; defects 5369-5373
- Semantics structure: TODO-5352, TODO-5353
- Test infrastructure: TODO-5354, TODO-5355, TODO-5357 (after 5354), TODO-5356 (deferred)
- Compiler state: TODO-5358 -> 5359 -> 5360
- IR/VM structure: TODO-5361 -> 5362
- VM strings: TODO-5363 -> 5364 -> 5365
- Docs hygiene: TODO-5366, TODO-5367, TODO-5368 (deferred)

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight, so the two `ready` leaves not listed there (TODO-5366, TODO-5367 docs hygiene) wait for a slot.

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

- [ ] TODO-5350: Route semantics and dump rewrites through one collection target table
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: collection-resolution
  - scope: Replace the scattered literal path and suffix checks in semantics (method-
    target resolution, same-path rewrites, template monomorphization, ast-
    semantic dump rewrite) with lookups into a single
    `CollectionHelperTable` (public header under
    `include/primec/semantics/`) whose rows come from the TODO-5349
    inventory. Delete the duplicated checks as each call site moves.
  - acceptance:
    - no `src/semantics` file contains a hard-coded
      `/std/collections/<folder>/<helper>` or `_ref` helper-suffix
      comparison outside the table (enforced by an audit script in ctest
      like the existing map-surface audits).
    - the TODO-5349 parity suite and the full release gate stay green.
    - net deleted lines recorded in the result note.
  - stop_rule: move call sites in batches of at most 10 files per commit; if a batch needs
    a behavior change, split it out as its own leaf.

- [ ] TODO-5351: Route lowerer builtin-classification exemptions through the collection target table
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5350
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: collection-resolution
  - scope: Finish what TODO-4737 started: the remaining receiver-type-dependent
    exemptions in `IrLowererSetupTypeMethodCallResolution.cpp` and
    `IrLowererInlineNativeCallDispatch.cpp`
    (`routesExplicitVectorCountMethodThroughArgsPackCount`,
    `directTargetKeepsSyntheticCollectionFallback`,
    `allowsReceiverResolvedVectorMetadataFallback`, ...) become table
    lookups, and a module-wide lowered-module invariant pass (test flag)
    asserts every published method-call target has a lowered definition or a
    table classification.
  - acceptance:
    - the three named predicates and the string-literal exemptions are deleted
      or reduced to table lookups.
    - invariant pass runs under a test flag in `ir.pipeline.validation`; re-
      introducing the gap (c) class of bug trips it.
    - before/after diff of the `ir.pipeline.validation` suite results shows no
      unintended acceptance change.
  - stop_rule: if a predicate encodes behavior the table cannot express, record it as a
    table row type instead of keeping the predicate.

- [ ] TODO-5352: Measure and cut semantics header fan-out (SemanticsValidator.h)
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: semantics-structure
  - scope: `src/semantics` is now 197 translation units (the include-only fragment
    migration noted in AGENTS.md is mostly done - update that note), but
    `SemanticsValidator.h` is included by about 300 includes and is a very
    large class header, so touching it rebuilds most of semantics. Measure:
    time `touch SemanticsValidator.h` + incremental release build
    before/after. Then split the class into focused headers (per-pass
    interfaces) so each translation unit includes only what it uses.
  - acceptance:
    - baseline and result incremental-rebuild times recorded in the result
      note (target: at least 40% faster after touching the main
      validator header).
    - no behavior change: full release gate green, `Semantics::validate` pass
      manifest unchanged.
    - AGENTS.md 'Implementation layout' note corrected to the real state.
  - stop_rule: stop after two attempts if the measured improvement is under 20%, and record
    why (this leaf is archived as low-value per the TODO rules).

- [ ] TODO-5353: Split TemplateMonomorphExpressionRewrite.cpp into focused units
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: semantics-structure
  - scope: `src/semantics/TemplateMonomorphExpressionRewrite.cpp` is 3,800 lines, the
    largest source file; `SemanticsValidatorSnapshots.cpp`,
    `...ExprMethodTargetResolution.cpp`, and `...StatementBindings.cpp` are
    2,200-2,600. Split each by responsibility into units of at most about
    1,000 lines with explicit headers, no logic changes.
  - acceptance:
    - no file under `src/semantics` exceeds 1,200 lines (a ctest script check,
      allowlisting any exception with a reason).
    - full release gate green; semantic-product goldens byte-identical.
    - commit per file split so each is bisectable.
  - stop_rule: pure moves only: if splitting needs a logic change, do it in a separate leaf
    first.

- [ ] TODO-5354: Guard test registration: generated shards and an unregistered-test check
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - scope: Shard ranges in `cmake/PrimeStructManaged*.cmake` are hand-maintained, and
    test files were once compiled but never registered with CTest (found
    only by accident during TODO-5320). Add a ctest check script (like
    `scripts/check_include_layers.py`) that fails when a `TEST_CASE` file
    under `tests/unit/` is not covered by a registered suite, or when a
    managed shard's `TOTAL_CASES` does not match the actual `TEST_CASE`
    count. Also generate `tests/TEST_INVENTORY.md` (11k lines, hand-
    maintained) from the sources instead.
  - acceptance:
    - the check fails on a deliberately added unregistered test file and on a
      wrong TOTAL_CASES (negative tests for the checker).
    - `TEST_INVENTORY.md` is generated by a script and verified up to date in
      ctest.
    - gate green with the checker enabled.
  - stop_rule: do not rewrite the shard mechanism here (see TODO-5357); only guard it.

- [ ] TODO-5355: Ban wall-clock comparisons in tests
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - scope: Two intermittent failures (type-graph dump comparisons including `*_ms`
    timings) came from tests comparing output that contains wall-clock
    values. Move the `stripDumpTimings` helper into the shared testing
    headers, use it for every dump comparison, and add an audit script that
    flags tests comparing two dump-stage outputs without normalizing, plus a
    doctest that proves the helper handles every metrics field.
  - acceptance:
    - one shared normalization helper under `include/primec/testing/`.
    - audit script in ctest fails if a `--dump-stage` output pair is compared
      raw (negative test included).
    - 10 consecutive release gates in CI-style repetition (`ctest --repeat
      until-fail:3` on the dump suites) pass.
  - stop_rule: if the audit has more than 20 false positives, narrow it to type-graph and
    metrics dumps and record the limit.

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

- [ ] TODO-5357: Cut the release gate time: re-shard from measured costs
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5354
  - created_at: 2026-10-01
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - scope: The full gate takes about 9-12 minutes. Use `CTestCostData.txt` to find
    shards over a time budget or badly unbalanced (a few shards dominate
    wall time under `--parallel 2N`), re-shard them, and replace hand-picked
    FIRST/LAST ranges with cost-based sharding computed at configure time.
  - acceptance:
    - gate wall time on the reference 4-core box reduced by at least 20%
      (before/after recorded).
    - no shard over 30 seconds without a recorded justification.
    - shard computation is deterministic and documented in
      `docs/TestRuntimeOptimization.md`.
  - stop_rule: do not change `scripts/compile.sh` (AGENTS rule); only CMake registration.

- [ ] TODO-5358: Inventory compiler global state and design a per-compilation context
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: compiler-state
  - scope: Process-global and thread-local state (compile arena, `thread_local` caches
    in `SourceLocationMapper`, `SemanticsBindingTypeHelpers`,
    `StdlibSurfaceRegistry`, function-local statics wrapped in
    `systemHeapValue`, arena reset callbacks) caused the TODO-5233/5234/5235
    hazards and, in embedding, the stale `SourceLocationMapper` cache
    (TODO-5340). Produce the inventory (every `thread_local`/non-trivial
    static under `src/`, who writes it, whether results depend on prior
    compiles) and a design for a `CompileContext` owned by one compilation,
    in `docs/CompilerArenaAllocator.md`.
  - acceptance:
    - inventory table committed with a per-item verdict: pure cache / stateful
      / must be process-global.
    - a property test compiles the same program set in different orders and
      thread layouts and asserts identical IR bytes, run for all
      fixtures in `tests/unit/embed/`.
    - design names the migration order and the first two items to move.
  - stop_rule: design and inventory only; no migration in this leaf.

- [ ] TODO-5359: Move compiler caches into a per-compilation context
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5358
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: compiler-state
  - scope: Implement the TODO-5358 design for the stateful items: caches become members
    of a context passed through the pipeline (or owned by it), so nothing
    leaks between compiles or threads and no reset callbacks are needed.
  - acceptance:
    - each migrated cache's `thread_local`/static is deleted and its arena
      reset callback removed.
    - order-independence property test from TODO-5358 passes; TSAN embed smoke
      stays clean.
    - compile-time benchmarks (`scripts/benchmark.sh`) show no regression
      beyond noise.
  - stop_rule: migrate at most three caches per commit; if benchmarks regress more than 5%,
    stop and record the cache as 'must stay process-global'.

- [ ] TODO-5360: Remove ScopedCompileArena reset callbacks and the arena magic-static workarounds
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5359
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: compiler-state
  - scope: Once state is per-compilation (TODO-5359), delete
    `registerArenaResetCallback`, the `systemHeapValue` wrappers on magic
    statics, and the poison-audit plumbing that exist only to make resets
    safe; keep the arena allocator itself if benchmarks justify it.
  - acceptance:
    - `registerArenaResetCallback` and `systemHeapValue` have no remaining
      callers and are deleted.
    - embed API can optionally use a per-compile arena scope without dangling
      results (test).
    - docs/CompilerArenaAllocator.md updated to the final design.
  - stop_rule: if the arena cannot be made safe to reset per compile without the
    workarounds, record that and stop.

- [ ] TODO-5361: Single opcode descriptor table for IR (stack effect, targets, serialization)
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: ir-vm-structure
  - scope: Adding `CallHost` required edits in the enum, validator allowlists,
    serializer bounds (whose upper bound was wrong and could not load
    `FileWriteStringDynamic`), VM kernel, debug session, and about a dozen
    switches. Introduce one table in `include/primec/ir/` describing each
    opcode: name, immediate kind, stack pops/pushes, allowed validation
    targets, and whether the VM kernel handles it. Generate or check the
    validator, serializer range, and VM dispatch coverage from it.
  - acceptance:
    - a ctest check proves every opcode has a table row and that
      serializer/validator/VM coverage matches its row (negative test:
      add an opcode without a row fails).
    - the validator per-target allowlists and `MinOpcode/MaxOpcode` constants
      are derived from the table.
    - no serialized-IR change (golden fixture byte-identical).
  - stop_rule: do not change opcode numbering or semantics; if a switch cannot be derived,
    check it against the table instead.

- [ ] TODO-5362: Unify the VM execution kernel and the debug session interpreter
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5361
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: ir-vm-structure
  - scope: `VmExecutionKernel` and `VmDebugSession::stepInstruction` interpret the same
    opcodes twice (`src/runtime/VmDebugSessionInstruction.cpp` repeats cases
    the kernel has), so every opcode and host-call change must be made
    twice, and debug sessions already lack host calls. Make the debug
    session drive the kernel one instruction at a time through the existing
    `VmKernelHost` boundary with debug hooks.
  - acceptance:
    - debug session and normal run share one dispatch implementation; the
      duplicated opcode cases are deleted.
    - DAP, breakpoint, and step tests unchanged and green; host calls work in
      debug sessions with bindings supplied.
    - `VmDebugSessionInstruction.cpp` reduced by at least half.
  - stop_rule: if single-step cost makes `Vm::execute` more than 5% slower on the VM
    benchmarks, keep a fast path for non-debug runs.

- [ ] TODO-5363: Spec: VM-owned dynamic strings (design decision)
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Language/VM
  - parallel_track: vm-strings
  - scope: The VM cannot create strings (string values are indices into a module
    table), which limits embedding (`Script::call` cannot return strings or
    index argument strings, host functions cannot return strings) and blocks
    any dynamic text feature. Write the design in `docs/PrimeStruct.md`: a
    VM string heap with an index space above the module table, how
    `LoadStringByte` (currently a compile-time string index immediate)
    handles dynamic indices, lifetime/ownership, interaction with
    native/wasm backends (reject or support), and the PSIR version impact.
  - acceptance:
    - design section committed with a decision on each question above and an
      IR snippet.
    - a prototype diff or estimate of the touched sites (VM string lookups,
      print/file ops, host calls) with a recommended leaf split appended
      here.
    - docs-only change with an explicit 'docs-only/no TODO' note aside from
      the leaves it spawns.
  - stop_rule: docs and sizing only; do not implement.

- [ ] TODO-5364: VM string heap: dynamic string indices in the interpreter
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5363
  - created_at: 2026-10-01
  - phase: Language/VM
  - parallel_track: vm-strings
  - scope: Implement the TODO-5363 design in the VM kernel: strings created at run time
    (from host calls first) get indices above the module table;
    `LoadStringLength`, `LoadStringByte` (dynamic-index variant), print and
    file-open opcodes resolve both spaces through one lookup helper.
  - acceptance:
    - every VM string lookup goes through one helper (grep audit in ctest).
    - a hand-built IR test creates a host string and indexes, measures, and
      prints it; out-of-range and use-after-free indices fault cleanly.
    - PSIR version bumped and documented if the format changed.
  - stop_rule: VM only; native/wasm backends keep rejecting dynamic strings with a
    diagnostic.

- [ ] TODO-5365: Embed API: string results and indexable string arguments
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5364
  - created_at: 2026-10-01
  - phase: Language/VM
  - parallel_track: vm-strings
  - scope: Use the VM string heap in the embedding layer: host functions may return
    `std::string`, exported functions may return strings, `text.at(i)` works
    on argument strings, and the per-call module copy in `Script::call` goes
    away.
  - acceptance:
    - `CallResult<std::string>` and host callables returning strings work,
      with tests for empty/NUL/UTF-8/large strings.
    - `first_byte`-style exports (indexing an argument string) compile and
      run; the `__psarg_string` special case and per-call module copy
      are deleted.
    - lifetime test: no growth over 1000 string-returning calls.
  - stop_rule: strings only; structs and arrays across the boundary remain out of scope.

- [ ] TODO-5366: Archive docs/todo_finished.md and keep a greppable index
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Documentation
  - parallel_track: docs-hygiene
  - scope: `docs/todo_finished.md` is 57k lines, which every agent session pays to
    search. Split it into per-quarter archive files under
    `docs/todo_archive/` and leave `todo_finished.md` as a short index (ID
    -> title -> archive file -> finished_at) generated by a script, so `grep
    TODO-NNNN` still finds the block with one extra hop.
  - acceptance:
    - `scripts/archive_todo_finished.py` splits and regenerates the index
      idempotently.
    - every existing TODO ID is findable from the index (script checks round
      trip: no block lost).
    - `docs/todo.md` operating rule 15 updated to say where finished blocks
      go.
  - stop_rule: move text verbatim; never rewrite finished blocks.

- [ ] TODO-5367: Trim docs/failing_tests.md to current failures only
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Documentation
  - parallel_track: docs-hygiene
  - scope: `docs/failing_tests.md` is about 2,100 lines of historical triage while
    `compile.sh` already rewrites a generated section with the latest gate
    result. Move history to the archive, keep only open failures plus the
    generated section, and make the policy checkable (a ctest script fails
    if the file lists a test that currently passes).
  - acceptance:
    - file under 150 lines with only open entries and the generated section.
    - history preserved under `docs/todo_archive/`.
    - checker script in ctest, with a negative test.
  - stop_rule: do not change `scripts/compile.sh` (AGENTS rule); only the doc and a
    checker.

- [ ] TODO-5368: Split docs/PrimeStruct.md into stable spec sections
  - owner: ai
  - status: deferred
  - created_at: 2026-10-01
  - phase: Documentation
  - parallel_track: docs-hygiene
  - scope: `docs/PrimeStruct.md` (5.9k lines) mixes the normative language spec with
    roadmap, rollout notes, and implementation status. Needs scoping:
    classify each section (normative / implementation note / history), then
    split into `docs/spec/*.md` with stable anchors, leaving PrimeStruct.md
    as the index. Keep `AGENTS.md` references and the ownership matrix
    location intact.
  - acceptance:
    - classification table committed first.
    - spec files split with a link checker in ctest (no broken intra-doc links
      or anchors).
    - AGENTS.md and todo.md references updated; normative text unchanged
      (diff-checked).
  - stop_rule: no semantic edits to spec text; if a section's classification is unclear,
    leave it in the index file.

- [ ] TODO-5369: map `.at()` and `.at_unsafe()` method calls fail with `unknown call target /map/at`
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: collection-defects
  - scope: docs/PrimeStruct.md (stdlib reference, map section) lists
    `.at()`/`.at_unsafe()` as supported map methods, but `[map<i32, i32>]
    m{...}` `m.at(2i32)` fails semantics with `unknown call target: /map/at`
    while bare `at(m, 2i32)` works. Pinned by the `map at method` and `map
    at_unsafe method` rows of docs/CollectionHelperTargets.md.
  - acceptance:
    - either make the method forms resolve to the same published target as the
      bare forms (`/std/collections/map/at`,
      `/std/collections/map/at_unsafe`) and flip the two rows to `ok`,
      or, if the methods are intentionally unsupported, correct
      docs/PrimeStruct.md and replace the diagnostic with one that names
      the supported spelling.
    - the parity suite and the full release gate stay green.
  - stop_rule: fix only this row's behavior; anything else found goes to its own leaf.

- [ ] TODO-5370: map bare `contains(m, k)` passes semantics but fails VM lowering while `m.contains(k)` works
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: collection-defects
  - scope: bare `contains(m, 2i32)` publishes `/std/collections/map/contains` yet
    lowering fails with `only supports arithmetic/comparison/clamp/...`
    (call=/contains); the method form of the same helper lowers and runs.
    Pinned by the `map contains bare` row. This is the canonical 'published
    target has no lowered definition' class from TODO-4737.
  - acceptance:
    - `contains(m, k)` returns the same value as `m.contains(k)` in the VM and
      native backends; row flipped to `ok`.
    - the parity suite and the full release gate stay green.
  - stop_rule: fix only this row's behavior; anything else found goes to its own leaf.

- [ ] TODO-5371: array `.at(i)` method resolves to the vector helper and fails argument type checking
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: collection-defects
  - scope: `a.at(1i32)` on `array<i32>` fails with `argument type mismatch for
    /std/collections/vector/at`, while bare `at(a, 1i32)` publishes
    `/array/at` and works. The method form on an array should publish
    `/array/at` like the bare form. Pinned by the `array at method` row.
  - acceptance:
    - `a.at(i)` and `a.at_unsafe(i)` on arrays publish and lower `/array/at` /
      `/array/at_unsafe`; row flipped to `ok`.
    - the parity suite and the full release gate stay green.
  - stop_rule: fix only this row's behavior; anything else found goes to its own leaf.

- [ ] TODO-5372: `soa<T>` method spellings publish internal `soaVector*` helper targets instead of the public helper
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: collection-defects
  - scope: for a `soa<Particle>` receiver, `values.count()` and `values.get(i)` publish
    `/std/collections/soa/soaVectorCount` / `soaVectorGet`, while the bare
    spelling and the `SoaVector<T>` method spelling publish the public
    `/std/collections/soa/count` / `get`. Pinned by the `soa count
    method(soa<T>)` and `soa get method(soa<T>)` rows. Decide whether
    publishing the internal name is intended (then document it in the
    matrix) or a defect.
  - acceptance:
    - one published target per helper regardless of spelling, or an explicit
      documented reason in docs/CollectionHelperTargets.md for the
      difference.
    - the parity suite and the full release gate stay green.
  - stop_rule: fix only this row's behavior; anything else found goes to its own leaf.

- [ ] TODO-5373: Extend the collection parity matrix to Reference receivers and the borrowed `_ref` helpers
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: collection-defects
  - scope: docs/CollectionHelperTargets.md covers by-value receivers only. Add rows for
    `Reference<vector<T>>`, `Reference<map<K,V>>`,
    `Reference<soa<T>>`/`SoaVector<T>` receivers and the `*_ref` helpers
    (`count_ref`, `get_ref`, `ref_ref`, `at_ref`, ...) with their correct
    borrow syntax (take it from existing compile-run tests; a first probe
    with `location(v)` was rejected), so TODO-5350/5351 cannot regress them.
  - acceptance:
    - the matrix includes at least count/at/get/ref/push per family with
      borrowed receivers; new disagreements are filed as defects.
    - the parity suite and the full release gate stay green.
  - stop_rule: do not change resolution behavior; only add rows and file defects.
