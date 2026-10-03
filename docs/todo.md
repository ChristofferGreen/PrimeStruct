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
| TODO-5403 | Extend the source-file-size guard beyond src/semantics | deferred | lowerer-structure |
| TODO-5415 | Split stdlib/std/collections/soa_storage.prime by concern | deferred | stdlib |
| TODO-5348 | Verify the iOS embed build and XCFramework packaging on macOS | deferred | embedding-ios |

### Ready Now


### Immediate Next 10

1. TODO-5403 - Extend the source-file-size guard beyond src/semantics.

### Priority Lanes

- Embedding (must support iOS): TODO-5348 (needs macOS)
- Lowerer structure: TODO-5426 -> TODO-5403
- Collection resolution: TODO-5424, TODO-5425
- Tooling:  (needs approval)
- Stdlib: TODO-5415

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

### Task Blocks

- [ ] TODO-5426: Bring IrLowererCallHelpers.h under 100 std::function mentions
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Compiler structure
  - parallel_track: lowerer-structure
  - scope: after the native tail dispatch moved to `NativeCallTailDispatchHooks` (TODO-5402), `include/primec/ir_lowerer/IrLowererCallHelpers.h` still has about 195 `std::function` mentions; the big remaining ones are `emitBuiltinArrayAccess` (40), `emitArrayVectorIndexedAccess` (14), `tryEmitInlineCallWithCountFallbacks` (13), `tryEmitStringTableAccessLoad` (8) and the `emitKeyValueLookup*` family (about 40). Give each a small hooks struct the same way (designated-initializer call sites, one entry point each).
  - acceptance:
    - the header has fewer than 100 `std::function` mentions; no overload pair that differs only by omitted callbacks
    - validation tests converted with case counts unchanged; full gate green
  - stop_rule: a callback that is only ever a constant lambda becomes a struct default instead of a parameter.

- [ ] TODO-5403: Extend the source-file-size guard beyond src/semantics
  - owner: ai
  - status: ready
  - progress: the guard scans src/ and include/ with an allowlist (now 8 .cpp and 4 .h entries). Split so far: SemanticProduct, CountAccessHelpers, CompilePipeline, ResultHelpers, LowerInferenceBaseKindHelpers, AccessTargetResolution, LowerSumHelpers (three parts), and eight more via file-local helper headers (StatementBindingHelpers, CompileTimeEvaluation, SetupTypeReturnKindHelpers, LowerInferenceDispatchSetup, SetupTypeMethodCallResolution, OperatorCollectionMutationHelpers, AccessLoadHelpers, ResultMetadataHelpers). Remaining entries are in scripts/source_file_size_allowlist.txt; InlineNativeCallDispatch and LowerInlineCalls each hide one 1,200+ line function (phase extraction), BindingTypeHelpers has two anonymous namespaces, and the three LowerStatements*/LowerEmitExpr headers are fragment includes.
  - created_at: 2026-10-03
  - phase: Compiler structure
  - parallel_track: lowerer-structure
  - scope: `scripts/check_source_file_sizes.py` only scans src/semantics. Twenty files elsewhere exceed 1,200 lines (IrLowererLowerSumHelpers.cpp 2,949, IrLowererCountAccessHelpers.cpp 2,327, frontend/SemanticProduct.cpp 2,068, IrLowererInlineNativeCallDispatch.cpp 2,029, pipeline/CompilePipeline.cpp 1,987, ...). Scan src/ and include/ entirely, seed the allowlist with the current offenders, then split them one per commit using the phase/helper patterns from TODO-5384/5385.
  - acceptance:
    - allowlist seeded and only shrinking; the five largest files split under 1,200 lines
    - full gate green; dumps byte-identical
  - stop_rule: pure moves and phase extraction only; a split that needs a logic change gets its own leaf.

- [ ] TODO-5424: Collapse the removed vector/array/map call-form spelling diagnostics
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Compiler structure
  - parallel_track: collection-resolution
  - scope: `docs/CompatibilitySpellingInventory.md` shows `/vector/count(v)`, `/array/count(v)`, `v./vector/count()` and `/map/count(m)` are already rejected (`unknown call target` / `unknown method`). `explicitRemovedMethodPath`, `isRemovedVectorCompatibilityHelper`, `isRemovedKeyValueCompatibilityHelper` and the retired-maybe helpers (about 370 uses) exist to produce those rejections. Pin each rejection in the parity matrix, then let the generic unknown-target path produce the message and delete the classifiers.
  - acceptance:
    - rejection rows for every removed spelling in `docs/CollectionHelperTargets.md`; no `explicitRemoved*` / `isRemoved*CompatibilityHelper` identifiers left in src/
    - full gate green; diagnostics tests updated only where the message text legitimately changes
  - stop_rule: if a removed spelling still needs a tailored message for users, keep one table-driven function instead of the scattered predicates.

- [ ] TODO-5425: Probe and prune the legacy SoA helper path canonicalizers
  - owner: ai
  - status: deferred
  - created_at: 2026-10-03
  - phase: Compiler structure
  - parallel_track: collection-resolution
  - scope: `isLegacyOrCanonicalSoaHelperPath`, `canonicalizeLegacySoa{Ref,Get,ToAos}HelperPath` and `templateMonomorphCompatibilitySoaHelperPrefix` (about 290 uses) map root spellings such as `/to_aos` to `/std/collections/soa/...`, but the parity matrix pins only canonical SoA spellings. Add the legacy spellings as matrix rows first; for each one that is rejected, delete its canonicalizer branch and callers.
  - acceptance:
    - every legacy SoA spelling has a matrix row (ok or rejected); rejected ones no longer appear in src/
    - full gate green
  - stop_rule: a spelling the matrix shows as `ok` stays and gets a note, not a deletion.

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

