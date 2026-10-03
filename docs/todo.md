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
| TODO-5398 | Archive or fold the orphaned long-form docs | deferred | docs-hygiene |
| TODO-5400 | Table-drive the builtin math-name classifiers | ready | lowerer-structure |
| TODO-5401 | Collapse duplicate std::function callback aliases | ready | lowerer-structure |
| TODO-5402 | Replace the 17-callback native tail dispatch signatures with a hooks struct | deferred | lowerer-structure |
| TODO-5403 | Extend the source-file-size guard beyond src/semantics | deferred | lowerer-structure |
| TODO-5404 | Remove the hand-maintained src/ir_lowerer header mirrors | deferred | lowerer-structure |
| TODO-5405 | Inventory and schedule deletion of compatibility spellings | deferred | collection-resolution |
| TODO-5406 | Delete or justify the legacy collection branch counters | deferred | collection-resolution |
| TODO-5407 | Route benchmark instrumentation through one sink | ready | diagnostics |
| TODO-5408 | Turn stdlib registry startup throws into diagnostics | deferred | diagnostics |
| TODO-5409 | Add clang-format configuration and a changed-files format check | deferred | tooling |
| TODO-5410 | Add self-tests for the nine unguarded check scripts | ready | tooling |
| TODO-5411 | Add a CI workflow for the release gate | deferred | tooling |
| TODO-5412 | Share CLI argument parsing between primec and primevm | deferred | tooling |
| TODO-5413 | Re-baseline benchmarks after the structural refactors | ready | performance |
| TODO-5414 | Split the test files over 3,000 lines | deferred | test-infrastructure |
| TODO-5415 | Split stdlib/std/collections/soa_storage.prime by concern | deferred | stdlib |
| TODO-5348 | Verify the iOS embed build and XCFramework packaging on macOS | deferred | embedding-ios |

### Ready Now

- TODO-5400 (track: lowerer-structure): Table-drive the builtin math-name classifiers (surface: IrLowererBuiltinNameHelpers.cpp).
- TODO-5401 (track: lowerer-structure): Collapse duplicate std::function callback aliases (surface: *Fn alias headers).
- TODO-5407 (track: diagnostics): Route benchmark instrumentation through one sink.
- TODO-5410 (track: tooling): Add self-tests for the nine unguarded check scripts.
- TODO-5413 (track: performance): Re-baseline benchmarks after the structural refactors.

### Immediate Next 10

4. TODO-5400 - Table-drive the builtin math-name classifiers.
5. TODO-5401 - Collapse duplicate std::function callback aliases.
6. TODO-5407 - Route benchmark instrumentation through one sink.
7. TODO-5410 - Add self-tests for the nine unguarded check scripts.
8. TODO-5413 - Re-baseline benchmarks after the structural refactors.
9. TODO-5402 - Replace the 17-callback native tail dispatch signatures with a hooks struct.
10. TODO-5403 - Extend the source-file-size guard beyond src/semantics.

### Priority Lanes

- Embedding (must support iOS): TODO-5348 (needs macOS)
- Docs hygiene: TODO-5398
- Lowerer structure: TODO-5400, TODO-5401 -> TODO-5402 -> TODO-5403 -> TODO-5404
- Collection resolution: TODO-5405 -> TODO-5406
- Diagnostics: TODO-5407 -> TODO-5408
- Tooling: TODO-5410 -> TODO-5409, TODO-5411, TODO-5412
- Performance: TODO-5413
- Test infrastructure: TODO-5414
- Stdlib: TODO-5415

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

### Task Blocks

- [ ] TODO-5398: Archive or fold the orphaned long-form docs
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Documentation
  - parallel_track: docs-hygiene
  - scope: `docs/memories.md` (1,812 lines, 69 entries, last updated 2026-05-28, referenced by nothing), `docs/ReceiverTargetResolutionConsolidation.md` (7,524 lines, a Step-1/Step-2 migration log) and `docs/TODO4709CompileRunAudit.md` (4,009 lines, cited only from the todo archive) are history, not reference. Classify each entry: still-true rule -> AGENTS.md or the right `docs/spec/*.md` part; completed plan -> `docs/todo_archive/`; otherwise delete.
  - acceptance:
    - the three files are gone or reduced to a short pointer; every surviving fact lives in a document the index or AGENTS links to
    - `scripts/check_spec_docs.py` and the todo index ctests pass
  - stop_rule: if an entry's truth cannot be verified against the code, drop it rather than carry it forward.

- [ ] TODO-5400: Table-drive the builtin math-name classifiers
  - owner: ai
  - status: ready
  - created_at: 2026-10-03
  - phase: Compiler structure
  - parallel_track: lowerer-structure
  - scope: `src/ir_lowerer/IrLowererBuiltinNameHelpers.cpp` repeats the prologue `if (expr.kind != Call || name.empty()) return false; if (!parseMathName(resolveMathExprName(expr), out, allowBare)) ...` 19 times, once per math builtin family. Replace with one classifier over a (family, accepted names) table.
  - acceptance:
    - the 19 near-identical functions become table rows plus one lookup; IR dumps for `examples/` are byte-identical before/after
    - full release gate green
  - stop_rule: if two families differ in more than the name set, keep them as separate rows with an explicit predicate column, not separate functions.

- [ ] TODO-5401: Collapse duplicate std::function callback aliases
  - owner: ai
  - status: ready
  - created_at: 2026-10-03
  - phase: Compiler structure
  - parallel_track: lowerer-structure
  - scope: src/ and include/ define 230 `using XxxFn = std::function<...>` aliases; 66 share the signature `bool(const Expr &)`, 47 `std::string(const Expr &)`, 45 `bool(const Expr &, const LocalMap &)`, 21 `int32_t()`, 20 `void(const Expr &, LocalInfo &)`, 20 `ValueKind(const Expr &, const LocalMap &)`. Define one canonical alias per signature in `include/primec/ir_lowerer/CallbackTypes.h`, make the old names aliases of it, then delete the old names file by file; add a ratchet on the alias count.
  - acceptance:
    - alias count ratchet committed and falling; the six top signatures have exactly one name each
    - full release gate green; the testing mirror check passes
  - stop_rule: keep a distinct name when it carries meaning the signature does not (e.g. a classifier vs. a predicate); record those in the header comment.

- [ ] TODO-5402: Replace the 17-callback native tail dispatch signatures with a hooks struct
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Compiler structure
  - parallel_track: lowerer-structure
  - scope: `src/ir_lowerer/IrLowererCallHelpers.h` holds 306 `std::function` parameters; `tryEmitNativeCallTailDispatch` and `...WithLocals` each take 17 callbacks across 4 overloads. Introduce `NativeCallTailDispatchHooks` (the pattern `CallResolutionAdapters` already uses), pass it by const reference, and delete the overloads that only differ by omitted callbacks.
  - acceptance:
    - one struct, one function per entry point; `IrLowererCallHelpers.h` std::function count below 100
    - validation tests converted with case counts unchanged; full gate green
  - stop_rule: if a callback is only ever passed as a constant lambda, make it a struct default rather than a parameter.

- [ ] TODO-5403: Extend the source-file-size guard beyond src/semantics
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Compiler structure
  - parallel_track: lowerer-structure
  - scope: `scripts/check_source_file_sizes.py` only scans src/semantics. Twenty files elsewhere exceed 1,200 lines (IrLowererLowerSumHelpers.cpp 2,949, IrLowererCountAccessHelpers.cpp 2,327, frontend/SemanticProduct.cpp 2,068, IrLowererInlineNativeCallDispatch.cpp 2,029, pipeline/CompilePipeline.cpp 1,987, ...). Scan src/ and include/ entirely, seed the allowlist with the current offenders, then split them one per commit using the phase/helper patterns from TODO-5384/5385.
  - acceptance:
    - allowlist seeded and only shrinking; the five largest files split under 1,200 lines
    - full gate green; dumps byte-identical
  - stop_rule: pure moves and phase extraction only; a split that needs a logic change gets its own leaf.

- [ ] TODO-5404: Remove the hand-maintained src/ir_lowerer header mirrors
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Compiler structure
  - parallel_track: lowerer-structure
  - scope: 40 headers exist twice: `src/ir_lowerer/X.h` and `include/primec/testing/ir_lowerer_helpers/X.h`, kept in sync by hand and guarded by `check_testing_mirror_structs.py` after a real ODR bug (TODO-5235). Make the testing umbrella include the one real declaration (move shared declarations to `include/primec/ir_lowerer/` or install the src headers for tests) and delete the mirrors and the mirror check.
  - acceptance:
    - zero duplicated header basenames; `check_testing_mirror_structs.py` deleted or reduced to a no-duplicates assertion
    - full gate green; include-layer check passes without new allowlist entries
  - stop_rule: if a mirror exists to hide private members from tests, split the header into public/private parts instead of keeping two copies.

- [ ] TODO-5405: Inventory and schedule deletion of compatibility spellings
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Compiler structure
  - parallel_track: collection-resolution
  - scope: src/ uses Legacy/Compatibility/Removed/Retired-named identifiers 2,406 times (`isLegacyOrCanonicalSoaHelperPath` 168, `canonicalVectorCompatibilityHelperPathOrFallback` 102, `explicitRemovedMethodPath` 76, ...). Produce a table (identifier, what spelling it accepts, whether `docs/CollectionHelperTargets.md` still shows that spelling as `ok`), then one leaf per spelling that is no longer accepted to delete its helper and callers.
  - acceptance:
    - table committed under docs/; each deletable spelling has a child leaf
    - no behavior change in this leaf
  - stop_rule: stop at the table; deletion is per-child-leaf with the parity matrix as the gate.

- [ ] TODO-5406: Delete or justify the legacy collection branch counters
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Compiler structure
  - parallel_track: collection-resolution
  - scope: `IrLowererLegacyCollectionBranchCounters` keeps 9 env-var-gated hit counters that print JSON to std::cerr; one is still recorded from `IrLowererSetupTypeMethodCallResolution.cpp`. Run the full suite with the env var set; counters that read zero mark branches that can be deleted along with the counter; non-zero ones get a row in `docs/CollectionRoutingExceptions.md`.
  - acceptance:
    - every counter either deleted with its branch or documented; the std::cerr printing goes through TODO-5407's sink
    - full gate green
  - stop_rule: a branch with non-zero hits stays; document it, do not delete it.

- [ ] TODO-5407: Route benchmark instrumentation through one sink
  - owner: ai
  - status: ready
  - created_at: 2026-10-03
  - phase: Compiler structure
  - parallel_track: diagnostics
  - scope: Five production files write `[benchmark-...]` JSON lines straight to std::cerr (SemanticsValidator.cpp, SemanticsValidatorPassesDefinitions.cpp, CompilePipeline.cpp, SemanticsValidationBenchmarkOrchestration.cpp, IrLowererLegacyCollectionBranchCounters.cpp). AGENTS.md asks for a centralized logger `(to be defined)`. Define `primec::support::BenchmarkSink` (default: stderr; tests: capture) and make these the only callers; add a ctest that no non-bin src file names std::cerr/std::cout (the two IrToCpp emitter files generate C++ text and are allowlisted).
  - acceptance:
    - zero std::cerr/std::cout uses in production src outside the allowlist; audit script with self-test
    - `scripts/benchmark.sh` output unchanged; full gate green
  - stop_rule: do not change the JSON line format; downstream scripts parse it.

- [ ] TODO-5408: Turn stdlib registry startup throws into diagnostics
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Compiler structure
  - parallel_track: diagnostics
  - scope: `src/support/StdlibSurfaceRegistry.cpp:914` throws `std::runtime_error` when a known collection surface is missing at startup, against the `prefer explicit error types over exceptions` rule; `CompileArena` throws `std::bad_alloc` by design. Make registry construction return an Expected-style result that `runCompilePipeline` surfaces as a normal compiler diagnostic.
  - acceptance:
    - no `throw` outside CompileArena in production src; a test exercises the missing-surface path and gets a diagnostic, not a crash
    - full gate green
  - stop_rule: leave CompileArena's bad_alloc as is.

- [ ] TODO-5409: Add clang-format configuration and a changed-files format check
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Tooling
  - parallel_track: tooling
  - scope: There is no `.clang-format`; style is enforced only by review. Derive a config that reproduces the current layout (2-space indent, 120 columns, aligned parameters), verify it is a no-op on a sample of recently touched files, and add `scripts/check_format.py` that formats only files changed since `master` so the first run does not rewrite the tree.
  - acceptance:
    - .clang-format committed; the check passes on HEAD; AGENTS.md documents the command
    - no mass reformat in this leaf
  - stop_rule: if no config reproduces the current style within a few percent of lines, pick the closest and record the diff as a follow-up instead of reformatting everything.

- [ ] TODO-5410: Add self-tests for the nine unguarded check scripts
  - owner: ai
  - status: ready
  - created_at: 2026-10-03
  - phase: Test infrastructure
  - parallel_track: tooling
  - scope: Nine `scripts/check_*.py` audits have no `tests/scripts/test_*.py` self-test (check_benchmark_report, check_graph_budget, check_include_layers, check_semantic_memory_budget, check_semantic_memory_phase_one_success, check_semantic_memory_trend, check_test_duration_budget, check_test_suite_naming, check_testing_mirror_structs), so a broken audit passes silently. Add one self-test each with a passing and a failing fixture and register them in CMakeLists.txt next to their audit.
  - acceptance:
    - each listed script has a registered self-test; `scripts/check_test_registration.py` passes
  - stop_rule: if an audit cannot be exercised with a small fixture, refactor it to expose a `find_problems(...)` function first.

- [ ] TODO-5411: Add a CI workflow for the release gate
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Tooling
  - parallel_track: tooling
  - scope: The repository has no `.github/workflows`. Add a workflow that runs `./scripts/compile.sh --release` on pushes to master and pull requests, caches `build-release/`, and uploads `LastTestsFailed.log` on failure.
  - acceptance:
    - workflow file committed; a green run on master
    - no change to compile.sh (stability rule)
  - stop_rule: if the full gate exceeds the runner limit, run configure+build+the parallel-safe label first and record the full gate as a nightly job.

- [ ] TODO-5412: Share CLI argument parsing between primec and primevm
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Tooling
  - parallel_track: tooling
  - scope: `src/bin/primevm_main.cpp` has a 402-line `main` and `src/bin/main.cpp` a 247-line one; `src/support/OptionsParser.cpp` already parses primec options. Move primevm's parsing into OptionsParser (or a sibling) so both binaries share flag syntax, help text and error wording.
  - acceptance:
    - each `main` under 80 lines; `--help` output covered by a compile-run test for both binaries
    - full gate green
  - stop_rule: do not change any flag's meaning; renames need a deprecation alias.

- [ ] TODO-5413: Re-baseline benchmarks after the structural refactors
  - owner: ai
  - status: ready
  - created_at: 2026-10-03
  - phase: Performance
  - parallel_track: performance
  - scope: `benchmarks/benchmark_baseline.json` dates from 2026-09-21, before the arena allocator changes, the semantics PCH, the VM kernel stepper, the dynamic string heap and the phase-split giant functions (which now build state structs per call). Run `./scripts/benchmark.sh --build-dir build-release --report-json ... --baseline-json ...`, investigate any regression over 5%, and refresh the baseline.
  - acceptance:
    - report committed under build artifacts or docs with before/after numbers; regressions over 5% either fixed or recorded as a leaf
    - baseline file updated
  - stop_rule: if a regression is in a phase-split function, fix it by hoisting the state struct, not by re-merging the function.

- [ ] TODO-5414: Split the test files over 3,000 lines
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Test infrastructure
  - parallel_track: test-infrastructure
  - scope: Four test files exceed 3,000 lines (test_compile_run_benchmark_harness.cpp 3,853; ..._call_helpers_source_delegation_stays_stable.cpp 3,755; test_compile_run_imports_operations.cpp 3,662; ..._ir_validator_accepts_lowered_canonical_module.cpp 3,080). Split along TEST_CASE groups into sibling files with the same TEST_SUITE, then run `scripts/generate_test_inventory.py` and fix shard ranges.
  - acceptance:
    - no test file over 2,000 lines; case counts unchanged; registration and inventory ctests pass
  - stop_rule: pure moves; no case is merged or dropped.

- [ ] TODO-5415: Split stdlib/std/collections/soa_storage.prime by concern
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Standard library
  - parallel_track: stdlib
  - scope: `soa_storage.prime` is 4,518 lines and ~1,200 definitions in one file, the internal SoA substrate. Split into storage, column, conversion and helper files under `stdlib/std/collections/internal_soa_*.prime` with the same namespace, keeping it classified as internal/bridge code per docs/CodeExamples.md.
  - acceptance:
    - no stdlib file over 1,500 lines; collection parity matrix and `docs/CollectionHelperTargets.md` byte-identical
    - full gate green
  - stop_rule: pure moves; if the import manifest needs new entries, update `docs/spec/type-system.md`'s inclusion manifest in the same commit.

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

