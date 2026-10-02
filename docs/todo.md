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
| TODO-5375 | Borrowed `Reference<vector<T>>` receivers reject every collection helper spelling | ready | collection-defects |
| TODO-5379 | Close the remaining gate-time gap after wildcard pruning | ready | test-infrastructure |
| TODO-5374 | Typed collection family/helper enum replacing string-tagged family checks | deferred | collection-resolution |
| TODO-5352 | Measure and cut semantics header fan-out (SemanticsValidator.h) | ready | semantics-structure |
| TODO-5353 | Split TemplateMonomorphExpressionRewrite.cpp into focused units | ready | semantics-structure |
| TODO-5356 | Collapse near-duplicate ir_pipeline validation tests into table-driven suites | deferred | test-infrastructure |
| TODO-5358 | Inventory compiler global state and design a per-compilation context | ready | compiler-state |
| TODO-5359 | Move compiler caches into a per-compilation context | blocked | compiler-state |
| TODO-5360 | Remove ScopedCompileArena reset callbacks and the arena magic-static workarounds | blocked | compiler-state |
| TODO-5361 | Single opcode descriptor table for IR (stack effect, targets, serialization) | ready | ir-vm-structure |
| TODO-5362 | Unify the VM execution kernel and the debug session interpreter | blocked | ir-vm-structure |
| TODO-5363 | Spec: VM-owned dynamic strings (design decision) | ready | vm-strings |
| TODO-5364 | VM string heap: dynamic string indices in the interpreter | blocked | vm-strings |
| TODO-5365 | Embed API: string results and indexable string arguments | blocked | vm-strings |
| TODO-5368 | Split docs/PrimeStruct.md into stable spec sections | deferred | docs-hygiene |
| TODO-5348 | Verify the iOS embed build and XCFramework packaging on macOS | deferred | embedding-ios |

### Ready Now

- TODO-5352 (track: semantics-structure): Measure and cut semantics header fan-out (SemanticsValidator.h).
- TODO-5353 (track: semantics-structure): Split TemplateMonomorphExpressionRewrite.cpp into focused units.
- TODO-5358 (track: compiler-state): Inventory compiler global state and design a per-compilation context.
- TODO-5361 (track: ir-vm-structure): Single opcode descriptor table for IR (stack effect, targets, serialization).
- TODO-5363 (track: vm-strings): Spec: VM-owned dynamic strings (design decision).
- TODO-5379 (track: test-infrastructure): Close the remaining gate-time gap after wildcard pruning.

### Immediate Next 10

1. TODO-5379 - Close the remaining gate-time gap after wildcard pruning.
2. TODO-5361 - Single opcode descriptor table for IR (stack effect, targets, serialization).
3. TODO-5358 - Inventory compiler global state and design a per-compilation context.
4. TODO-5352 - Measure and cut semantics header fan-out (SemanticsValidator.h).
5. TODO-5353 - Split TemplateMonomorphExpressionRewrite.cpp into focused units.
6. TODO-5363 - Spec: VM-owned dynamic strings (design decision).
7. TODO-5375 - Borrowed `Reference<vector<T>>` receivers reject every collection helper spelling.

### Priority Lanes

- Embedding (must support iOS): TODO-5348 (needs macOS)
- Collection resolution: defects 5375-5377; typed family enum TODO-5374 (deferred)
- Semantics structure: TODO-5352, TODO-5353
- Test infrastructure: TODO-5379, TODO-5356 (deferred)
- Compiler state: TODO-5358 -> 5359 -> 5360
- IR/VM structure: TODO-5361 -> 5362
- VM strings: TODO-5363 -> 5364 -> 5365
- Docs hygiene: TODO-5368 (deferred)

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight, TODO-5375 (a defect leaf) waits for a slot.

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

- [ ] TODO-5375: Borrowed `Reference<vector<T>>` receivers reject every collection helper spelling
  - owner: ai
  - status: ready
  - created_at: 2026-10-02
  - phase: Compiler structure
  - parallel_track: collection-defects
  - scope: the six `vector ... (Reference)` rows pin that `r.count()`, `count(r)`, `r.at(i)`, `at(r, i)`, `r.push(x)` and `push(r, x)` on a `[Reference<vector<i32>> mut] r{location(v)}` local all fail semantics (`unknown method target`, `at requires array, vector, map, or string target`, push argument mismatch), although `Reference<vector<T>>` is accepted as a parameter/field type. Decide whether borrowed vectors are a supported helper receiver (then resolve to the vector helpers) or document and diagnose it uniformly.
  - acceptance:
    - either the six rows run (`ok`) and publish the vector helpers, or docs state borrowed vectors are unsupported helper receivers and all spellings emit one consistent diagnostic; rows updated accordingly.
    - the parity suite and the full release gate stay green.
  - stop_rule: fix only the pinned rows' behavior; anything else found goes to its own leaf.

- [ ] TODO-5379: Close the remaining gate-time gap after wildcard pruning (478.7 s vs 434 s target)
  - owner: ai
  - status: ready
  - created_at: 2026-10-02
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - scope: after TODO-5378 the gate is 478.7 s; ~92 s is the serial semantic-memory
    benchmarks. Profile the remaining top shards (`imports_operations_and_collections`,
    `vm_collections_*`, `smoke_core_paths_*`) for per-invocation cost that is not
    stdlib import (text filter `applyPerEnvelope`, VM startup), and decide whether
    the serial benchmarks can be split or scheduled so they overlap without
    invalidating RSS/wall measurements.
  - acceptance:
    - gate `Total Test time` (relinked-primec dev loop, 4-core box) <= 434 s, before/after
      recorded in docs/TestRuntimeOptimization.md.
  - stop_rule: do not change `scripts/compile.sh`; if no further reduction is possible
    without changing what cases assert, record the measured floor and stop.

- [ ] TODO-5374: Typed collection family/helper enum replacing string-tagged family checks
  - owner: ai
  - status: deferred
  - created_at: 2026-10-01
  - phase: Compiler structure
  - parallel_track: collection-resolution
  - depends_on: TODO-5350
  - scope: TODO-5350 centralized every collection helper spelling and replaced
    the duplicated base-or-borrowed and rooted-prefix chains with named
    predicates (`include/primec/support/CollectionHelperNames.h`), but family
    identity is still a string tag compared in about 200 places
    (`x == collection_helpers::kRootedVector`, `/map`, `/soa`, `/array`,
    `/string`) and the receiver-type-dependent lowerer predicates in
    `IrLowererSetupTypeMethodCallResolution.cpp`
    (`routesExplicitVectorCountMethodThroughArgsPackCount`,
    `directTargetKeepsSyntheticCollectionFallback`,
    `allowsReceiverResolvedVectorMetadataFallback`) encode behavior that a
    string table cannot express. Needs scoping: introduce
    `enum class CollectionFamily` and `CollectionHelper` with a registry-backed
    parse/format API, migrate function signatures that pass family strings
    (start with `src/semantics` receiver-family helpers), and express the three
    lowerer predicates as rows (family x helper x receiver kind -> routing).
  - acceptance:
    - family tags are an enum in semantics and the lowerer; no `== kRooted<Family>` string comparisons remain outside the enum's parse/format functions (ctest audit).
    - the three named lowerer predicates are deleted or reduced to lookups into the family/helper table.
    - collection parity matrix and the full release gate unchanged.
  - stop_rule: if a predicate depends on state the table cannot carry, record it as a documented routing exception in docs/CollectionHelperTargets.md instead of keeping an undocumented predicate.

