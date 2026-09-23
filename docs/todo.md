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

## Task Template

```md
- [ ] TODO-<id>: Short title
  - owner: ai|human
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

## Open Tasks

### Queue Summary

| ID | Title | Created | Track | Status |
| --- | --- | --- | --- | --- |
| TODO-4710 | Cache stdlib parse results across compile-pipeline test runs | 2026-07-15 | test-runtime-stdlib-cache | superseded by TODO-5230 (closed); low remaining value |
| TODO-4712 | Grow CTest shard size once cross-test-case pollution is fixed | 2026-07-15 | test-runtime-shard-consolidation | blocked on TODO-4707; noted low-value |
| TODO-4732 | Cut compile-run test runtimes with semantic-product golden comparisons | 2026-07-20 | (none) | blocked on TODO-4709's audit follow-through |
| TODO-4737 | Add a lowered-module invariant for method-call targets | 2026-07-20 | (none) | blocked on TODO-4724 decomposition seam |
| TODO-4751 | Implement a real experimental `Map<K,V>` collection type | 2026-07-29 | hidden-test-failures-imports-operations | ready |
| TODO-4752 | Fix struct field access on freshly-returned temporaries | 2026-07-29 | hidden-test-failures-imports-operations | ready |
| TODO-4812 | Modern soa/SoaVector public-surface method-sugar gaps | 2026-07-30 | hidden-test-failures-text-filters | ready |
| TODO-4809 | collect-diagnostics collapses mixed map/vector errors | 2026-07-30 | hidden-test-failures-text-filters | ready |
| TODO-4816 | `IrLowererHelpers.cpp` hardcodes vector-helper spellings | 2026-07-30 | hidden-test-failures-architecture-audits | ready |
| TODO-5295 | `/soa/ref_ref<T>` same-path shadow wrongly rejected | 2026-09-16 | hidden-test-failures-vm-collections | ready |
| TODO-4800 | `args<T>` pack `.at()`/`.at_unsafe()` fails to lower on vm | 2026-07-30 | hidden-test-failures-emitters | ready |
| TODO-4801 | Canonical map ref-form helper call fails to lower on vm | 2026-07-30 | hidden-test-failures-emitters | ready |
| TODO-4806 | Chained `count(...)` off helper-return vector fails to lower | 2026-07-30 | hidden-test-failures-emitters | ready, held off Ready Now (4th on this track) |
| TODO-4807 | `resolveMethodCallPath` alias/canonical fallback regressions | 2026-07-30 | hidden-test-failures-emitters | ready, held off Ready Now (4th on this track); acceptance needs scoping first |

### Ready Now

- TODO-4751 (track: hidden-test-failures-imports-operations, surface: `stdlib/std/collections` `Map<K,V>` type): implement the missing experimental `Map<K,V>` stdlib type - only the lowercase `map<K,V>` builtin and the underlying `MapValue<K,V>` struct exist today.
- TODO-4752 (track: hidden-test-failures-imports-operations, surface: `ContainerError::why()` / vm backend): a freshly-returned temporary's struct field access reads default/zeroed values instead of the real field on `--emit=vm`.
- TODO-4812 (track: hidden-test-failures-text-filters, surface: `stdlib/std/collections/soa`, `stdlib/std/collections/experimental_soa_vector*`): modern `soa<T>`/`SoaVector<T>` public-surface method-sugar/canonicalization gaps found re-pinning `test_compile_run_text_filters_dumps.cpp`'s soa dump cluster.
- TODO-4809 (track: hidden-test-failures-text-filters, surface: `--collect-diagnostics`/`--emit-diagnostics` pipeline): diagnostic collection collapses or corrupts messages when a definition mixes map- and vector-receiver errors.
- TODO-4816 (track: hidden-test-failures-architecture-audits, surface: `src/ir_lowerer/IrLowererHelpers.cpp`): `isBuiltinClassifiedMethodCallTarget` hardcodes canonical vector-helper path spellings as literal strings instead of routing through `CollectionSpellingClassifier`.
- TODO-5295 (track: hidden-test-failures-vm-collections, surface: semantics validation for `/std/collections/soa/ref_ref`): a same-path user shadow of `ref_ref<T>` is wrongly rejected with a template-arguments error instead of being invoked.
- TODO-4800 (track: hidden-test-failures-emitters, surface: vm lowering, `args<T>` variadic-pack access): `.at()`/`.at_unsafe()` method-call sugar (and bare `at(pack, N)`) on `args<T>` elements fails to lower on vm with "missing lowered definition: /array/at".
- TODO-4801 (track: hidden-test-failures-emitters, surface: vm lowering, canonical map ref-form helpers): a direct (non-method) call to a canonical map ref-form helper used in an expression fails to lower on vm.

Held back from this round's Ready Now: TODO-4806/TODO-4807 (same `hidden-test-failures-emitters` track as TODO-4800/4801 - rule 11 caps concurrent same-track items; pick these up once one of the two above closes). TODO-4710/4712/4732/4737 are blocked or low-value - see the Queue Summary table.

### Immediate Next 10

### Priority Lanes

### Execution Queue

### Task Blocks

- [ ] TODO-4710: Cache stdlib .prime parse results across compile-pipeline test runs
  - superseded_2026-08-13: this TODO's entire premise was moot. Every
    `compile_run` test spawns a fresh `./primec` subprocess (confirmed by
    TODO-4709's audit), so there is no shared process for a cross-test-run
    parse cache to live in - "process-local cache keyed on file path +
    mtime" has nothing to persist across, since each test gets a brand new
    process. While measuring this premise directly (`--dump-stage`
    breakdown on a minimal vector-importing compile), found the real,
    much bigger cost this TODO was gesturing at from the wrong angle: a
    SINGLE compile invocation that imports `/std/collections/vector/*`
    and uses it takes ~2.0-2.2s vs ~7-10ms for an otherwise-identical
    no-import compile - a ~250-300x difference, all CPU-bound (confirmed
    with `valgrind --tool=callgrind`), not I/O or cold-cache. The
    redundant work isn't stdlib text re-read across test PROCESSES, it's
    binding-type-name string parsing (`normalizeBindingTypeName`,
    `splitTemplateTypeName`, `splitTopLevelTemplateArgs`) re-deriving the
    same answers from scratch millions of times WITHIN a single process's
    one compile, with zero memoization. Real tracking entry is now
    TODO-5230, which fixed the memoizable part of this (verified: 99.99%
    cache hit rate, ~5.8% total retired-instruction reduction) and
    documented why the call-VOLUME itself (not the per-call string-parse
    cost) is the larger remaining piece, requiring deeper restructuring
    out of scope for a leaf-sized fix. Leaving this TODO open but pointing
    at TODO-5230 as the actual tracking entry, per the same
    superseded-but-not-duplicated pattern as TODO-4740 -> TODO-4804.
  - owner: ai
  - created_at: 2026-07-15
  - phase: Test runtime optimization
  - parallel_track: test-runtime-stdlib-cache
  - depends_on: (none)
  - scope: Determine whether `validateProgramThroughCompilePipeline`-style
    test helpers (and the underlying `ImportResolver`/`runCompilePipeline`
    machinery) re-read and re-parse the same unchanging stdlib `.prime`
    files from disk for every single test case that imports them. If so,
    add a process-local cache keyed on file path + mtime so repeated
    imports of the same stdlib module within one test binary process reuse
    already-parsed content.
  - implementation_notes: Confirm with a read syscall count or simple
    instrumentation before assuming this is real; don't add caching
    speculatively. Any cache must not change behavior for tests that
    intentionally write and import a modified stdlib file mid-run, if any
    exist.
  - acceptance:
    - Before/after wall-clock timing for one representative `compile_run`
      CTest shard is recorded in `docs/TestRuntimeOptimization.md`.
    - No test behavior changes (full affected suite still passes
      identically before and after).
  - stop_rule: Stop once caching is implemented and measured for one
    representative shard; broader rollout or cache-invalidation edge cases
    are follow-up work if the measured win is significant.

- [ ] TODO-4712: Grow CTest shard size once cross-test-case pollution is fixed
  - owner: ai
  - created_at: 2026-07-15
  - phase: Test runtime optimization
  - parallel_track: test-runtime-shard-consolidation
  - depends_on: TODO-4707, TODO-4708
  - scope: Managed doctest suites are currently sharded into small 10-case
    `add_test` chunks (`addPrimeStructManagedDoctestSuite`,
    `cmake/PrimeStructManagedSemanticsSuites.cmake`), which was necessary to
    dodge cross-test-case pollution (see TODO-4707) but means every one of
    the resulting hundreds of shards separately pays fixed binary-launch
    and doctest-registration overhead (see TODO-4708's measurement). Once
    TODO-4707 proves a suite pollution-free running as one process, raise
    that suite's `CASES_PER_SHARD` (or equivalent) toward the largest chunk
    size that still finishes comfortably under the 30s ceiling from
    `docs/TestRuntimeOptimization.md`, so the fixed per-shard cost stops
    being paid hundreds of times over for the same total case count.
  - implementation_notes: Shard size is a tradeoff, not a monotonic win:
    bigger shards amortize fixed overhead better but increase blast radius
    (one bad case can no longer be isolated as easily) and reduce
    parallelism granularity under `ctest --parallel N`. Pick a size using
    TODO-4708's measured overhead number and real per-case runtime, not a
    round number. Start with `calls_flow.collections` (the suite already
    under investigation) before generalizing to other managed suites.
  - acceptance:
    - `calls_flow.collections`'s shard count is reduced (larger
      `CASES_PER_SHARD`) with total wall-clock time for the full suite
      measurably lower than the current 10-case-shard baseline, and no
      shard exceeds the 30s ceiling.
    - The change is proven safe by confirming pass/fail results are
      identical to the pre-change baseline (no reintroduced pollution).
  - stop_rule: Stop once `calls_flow.collections` is re-sharded and
    verified; rolling the same change out to every other managed suite is
    follow-up work, not part of this leaf.
  - cross_reference_2026-08-08: TODO-4708's measurement (now resolved)
    found per-shard fixed overhead is ~5-9ms - negligible against the
    measured ~4748s total suite time. This TODO's whole premise (grow
    shard size to amortize that fixed cost) is real but now known to be
    **low-value**: even eliminating all fixed overhead from all 1954
    shards entirely would save on the order of ~15-20s, not a
    meaningful fraction of runtime. Deprioritized relative to the real
    cost drivers identified in `docs/TestRuntimeOptimization.md`'s
    2026-08-08 log entry (a handful of pathologically slow tests
    dominate total time; see TODO-5220/5221/5222 for the higher-ROI
    follow-up chain). Not closing this TODO outright since TODO-4707
    (cross-test-case pollution) is still open and independently worth
    fixing for correctness reasons even without the perf motivation -
    just noting the perf case for it is much weaker than originally
    assumed.

- [ ] TODO-4732: Cut compile-run test runtimes with semantic-product golden comparisons
  - owner: ai
  - created_at: 2026-07-20
  - phase: Test infrastructure
  - scope: many compile-run tests pay the full primec semantics + IR
    lowering + clang + link + run cost (~40-60s/case in Debug) only to
    assert an exit code that is a proxy for a routing decision. Idea
    (from the project owner): compare a stored artifact instead of
    running the full pipeline. Design sketch agreed in-session:
    prefer storing the SEMANTIC PRODUCT routing tables
    (direct_call_targets / method_call_targets) over lowest-level IR
    or generated C++ - it is tiny, stable across lowering refactors,
    available before clang, and pins exactly the decision under test;
    generated C++ churns cosmetically and IR goldens churn on slot or
    ordering refactors. Guard rails: goldens enshrine
    recording-day bugs (this session spent its bulk un-pinning ~200
    rotted contracts), so the refresh workflow must force human diff
    review, and a thin end-to-end tier that actually runs binaries
    must remain (only real runs catch miscompiles and VM/native
    divergence). Execution order across the test-runtime track: take
    the independent quick wins FIRST - TODO-4734 (RelWithDebInfo
    runner), TODO-4733 (vm-mode migration), TODO-4736 (runtime
    preamble prebuild), TODO-4735 (shared stdlib product) - plus the
    TODO-4737 lowering invariant and TODO-4738 duration telemetry;
    THIS golden-comparison item comes last, scoped to whatever is
    still slow once those land. Note the goldens also cannot see
    lowering-stage failures (the gap (c) class) - that is TODO-4737's
    job, not this item's.
  - acceptance: combined with the track's other items, emitters-suite
    wall time drops by an order of magnitude without losing the
    end-to-end miscompile net.
  - progress_2026-07-23: investigated with real measurements before
    attempting a migration, rather than guessing at candidates. Two
    findings, one very good and one that narrows the win:
    1. The infrastructure this TODO envisions ALREADY EXISTS and is
       proven at scale - it doesn't need to be built from scratch.
       `include/primec/testing/CompilePipelineDumpHelpers.h` provides
       `runCompilePipelineBackendConformanceForTesting`/
       `prepareCompilePipelineIr` (drives `primec::runCompilePipeline`
       IN-PROCESS, no subprocess, no clang) plus
       `CompilePipelineBackendConformance::findDirectCallTarget`/
       `findMethodCallTarget`/`resolvedDirectCallPath`/
       `resolvedMethodCallPath` for asserting directly on the semantic
       product's routing tables, and
       `captureSemanticBoundaryDumpsForTesting` for in-process
       ast-semantic/semantic-product/ir dump-stage text capture. This
       exact pattern is already load-bearing at scale in
       `tests/unit/semantics/test_semantics_type_resolution_graph_snapshots.cpp`
       (8722 lines). So "combine a stored artifact" doesn't need new
       golden-file tooling - it needs `compile_run` cases that are
       really routing-decision checks moved onto this existing
       in-process helper surface instead of shelling out to
       `./primec --emit=... ` + optionally running the binary.
    2. The "obvious" migration candidates (compile-time REJECT cases -
       diagnostic-only, no execution) mostly don't have cost left to
       save. Verified directly on
       `test_compile_run_emitters_wrapper_map_count_sugar.cpp`'s
       "C++ emitter keeps canonical map count diagnostics on wrapper
       slash return method sugar" case: its diagnostic ("argument type
       mismatch for /std/collections/map/count parameter marker")
       fires at the `semantic` stage (confirmed via matching
       `--dump-stage semantic-product` output, including exit code 2
       and the identical diagnostic text, against the full `--emit=exe`
       invocation) - i.e. the current subprocess already fails BEFORE
       reaching clang/link, same as a golden-comparison version would.
       Timed both forms directly: ~10-11ms either way, no measurable
       win. A `grep`-based sweep for the `compileCmd`-but-no-`exePath`
       shape (reject-only tests with no execution) found ~292 matches
       across `tests/unit/compile_run/*.cpp` - a large candidate pool,
       but this timing result means most of them likely have the same
       "already short-circuits before the expensive part" property and
       would need per-case verification (not a blanket migration) to
       confirm which ones are worth moving.
    - what still needs doing before a real migration: the ACCEPT-and-
       run cases are where the real clang+link+execute cost lives, but
       distinguishing "exit code is only a routing-decision proxy"
       from "exit code encodes real computed program output" (e.g.
       `bare map count through canonical helper in C++ emitter"`
       asserts the executed binary returns exactly 92, i.e. genuine
       runtime-behavior verification, not just routing) requires
       exactly the audit TODO-4709 already scoped and left undone
       ("audit only, no migrations"). That audit is the real
       prerequisite here, not new tooling. Also flagging a fidelity
       trap for whoever does the migration: the existing in-process
       helpers default to `emitKind = "native"`
       (`detail::captureCompilePipelineDumpStageFromPath`) while the
       `compile_run/*emitters*` test files are specifically exercising
       the C++ ("cpp"/exe) emitter by name - a migration must pass the
       matching `emitKind` explicitly rather than accept the default,
       or it silently tests a different backend than the original
       case intended (the same class of regression this session hit
       for real during TODO-4733's exe->vm migration, caught there via
       a before/after diff rather than assumed away).

- [ ] TODO-4737: Add a lowered-module invariant - no published method-call target without a materialized definition or builtin classification
  - owner: ai
  - created_at: 2026-07-20
  - phase: Test infrastructure
  - scope: the TODO-4731 gap (c) class (semantic product publishes a
    method-call target the lowerer has no definition for) is invisible
    to semantic-product goldens and only surfaced case-by-case. A
    single validator pass over every lowered module asserting the
    invariant catches the whole class everywhere, replacing dozens of
    per-shape "does this lower" cases and guarding future
    materialization gaps by construction.
  - acceptance: invariant runs in the lowering pipeline under a test
    flag; deliberately re-introducing the gap (c) bug trips it.
  - progress_2026-07-23: investigated before attempting an
    implementation, given this touches core ir_lowerer resolution
    logic (higher regression risk than the other test-runtime items,
    which were all pure test-harness or build-config changes). Found
    the "materialized definition or builtin classification" check
    already exists, but scattered across 6 call sites in two files
    (`IrLowererInlineNativeCallDispatch.cpp:1605,1916` and
    `IrLowererSetupTypeMethodCallResolution.cpp:650,709,753,759`),
    each independently reimplementing a similar-but-not-identical
    whitelist of "this target is builtin-classified, don't require a
    materialized definition" exemptions via hardcoded string-literal
    comparisons. Inventoried the exemption set across both files (the
    concrete seed list a consolidation would need to cover):
    `/string/count`, `/std/collections/vector/count`,
    `/std/collections/vector/capacity`, `/std/collections/soa/count`,
    `/std/collections/vector/at` and `/at_unsafe`,
    `/std/collections/soa/to_aos`, `/array/count`, anything matching
    `isBuiltinFileHandleMethodName()`, and anything under the `/file/`
    path prefix. `IrLowererSetupTypeMethodCallResolution.cpp`'s single
    `resolveMethodCallTargetDefinition`-shaped function (this is the
    2800+-line function TODO-4724 already tracks decomposing) also has
    several MORE nuanced exemptions beyond simple string equality -
    `routesExplicitVectorCountMethodThroughArgsPackCount`,
    `directTargetKeepsSyntheticCollectionFallback`,
    `allowsReceiverResolvedVectorMetadataFallback` - that depend on
    receiver-type inference, not just the target path string, meaning
    a general invariant can't just be a static path allowlist; it
    needs to replicate (or directly reuse) the same receiver-aware
    logic these call sites already run.
  - why not implemented yet: a genuinely correct, general invariant
    pass needs to either (a) fully re-derive and generalize all 6
    call sites' accumulated special-casing (real risk of missing a
    nuance and shipping false positives across the large compile_run
    suite, or missing a nuance the other direction and shipping a
    pass that doesn't actually catch anything), or (b) have each of
    the 6 call sites call OUT to one new shared
    `isBuiltinClassifiedMethodCallTarget(target, semanticProgram,
    callExpr)` helper instead of their own inline exemption list, then
    have the new invariant pass call that SAME shared helper - the
    safer design, since it's provably consistent with existing
    lowering behavior by construction, but is real refactoring work
    across `IrLowererSetupTypeMethodCallResolution.cpp` (itself
    already flagged as needing decomposition under TODO-4724) and
    `IrLowererInlineNativeCallDispatch.cpp`, not a green-field
    addition. Given this session's other test-runtime items were all
    lower-risk (test harness or build config only, verified via
    before/after diffs with zero blast radius on ir_lowerer), didn't
    attempt (b) without dedicated budget to do the consolidation
    properly and re-verify the full compile_run/emitters/semantics
    surface afterward - the same discipline this session applied
    throughout (see TODO-4739's stop_rule for the parallel case in the
    vector at/at_unsafe classification mess).
  - recommended next step: option (b) above, done as its own
    dedicated pass alongside (or as a natural side effect of)
    TODO-4724's `resolveMethodTarget` decomposition, since extracting
    the exemption-check logic into its own named helper is exactly the
    kind of seam that decomposition should produce anyway. Once that
    helper exists and both files call it, the new invariant pass
    itself is comparatively small: iterate
    `semanticProgram->methodCallTargets`/`directCallTargets`, for each
    published target call `resolveLoweredDefinitionPath` and the new
    shared helper, and error if neither the definition nor the
    builtin-classification check succeeds.
  - progress_2026-07-23c: implemented the safe, verifiable slice of
    option (b) - deferred the unsafe slice rather than force it.
    `IrLowererInlineNativeCallDispatch.cpp`'s two call sites
    (originally lines 1583-1599 and 1898-1913) had a BYTE-IDENTICAL
    5-clause exemption predicate (the `/string/count`,
    `/std/collections/vector/count`, `/vector/capacity`, `/soa/count`,
    `/vector/at`+`/at_unsafe`, `/soa/to_aos` set). Extracted this into
    one shared `isBuiltinClassifiedMethodCallTarget(target, callExpr)`
    in `IrLowererHelpers.h`/`.cpp` (also mirrored into
    `include/primec/testing/ir_lowerer_helpers/IrLowererHelpers.h` per
    this codebase's existing test-linkage convention for internal
    ir_lowerer headers) and repointed both call sites at it - a pure,
    mechanical dedup with the extracted body verbatim-identical to
    what was inline before. Added
    `tests/unit/ir_pipeline/test_ir_pipeline_validation_ir_lowerer_helpers_classifies_builtin_method_call_targets.cpp`,
    a direct unit test pinning the exact classification surface (7
    true cases covering every exemption, 6 false cases covering wrong
    arity/wrong call name/unknown target) - this is the guard that
    trips if the predicate is ever widened incorrectly, i.e. a scoped,
    machine-checked version of the gap (c) invariant for this specific
    duplication. Registered the new file in `CMakeLists.txt` and
    bumped `primestruct.ir.pipeline.validation`'s `TOTAL_CASES` 1387 ->
    1389 in `cmake/PrimeStructManagedUnitBackendSuites.cmake`.
    Verified with a real before/after diff, not just a green run:
    built and ran the full 1387/1389-case `ir.pipeline.validation`
    suite twice (once on the pre-change tree via `git stash`, once
    with the change restored), captured the full sorted list of
    failing `TEST CASE:` names from both untruncated runs, and diffed
    them - **byte-for-byte identical set of 40 pre-existing failures
    both times** (all in unrelated areas: struct layout, binding-type
    classification, reflection-query elimination, module-artifact
    ordering - none touch method-call-target exemption logic), with
    the new file's 2 cases / 14 assertions passing on top. Zero
    regressions, zero fixed-by-accident.
    `IrLowererSetupTypeMethodCallResolution.cpp`'s other 4 call sites
    were deliberately NOT touched this pass: read the surrounding
    ~250 lines in detail and confirmed the earlier assessment - its
    silent-skip exemptions (`routesExplicitVectorCountMethodThroughArgsPackCount`,
    `directTargetKeepsSyntheticCollectionFallback`,
    `allowsReceiverResolvedVectorMetadataFallback`) are woven into a
    local `resolveLoweredDefinitionPath` lambda that closes over
    `defMap`/`explicitMethodPath`/`callExpr` and does receiver-type-
    dependent fuzzy path matching (`buildReceiverMethodTargetPath`,
    `normalizeCollectionHelperPath`, generated-family-path matching) -
    genuinely not extractable into a target-string-only predicate
    without either unsafely re-deriving that logic or waiting on
    TODO-4724's decomposition to produce a reusable seam. A general
    "runs in the lowering pipeline, checks every published target"
    invariant pass therefore still isn't implemented - only the
    duplicated flat-string half of the exemption surface is now
    single-sourced and regression-tested. Remaining scope unchanged
    from the "why not implemented yet" / "recommended next step" notes
    above; this progress note narrows what's still open rather than
    closing the item.
  - note_2026-09-03: TODO-4724 has since closed (see
    `docs/todo_finished.md`) without a reusable extracted seam covering
    this specific fuzzy-path-matching lambda - it wasn't one of the
    seams landed. This item's blocker above is therefore still real;
    re-check TODO-4724's finished-task entry for what was and wasn't
    extracted before assuming a seam now exists.

- [ ] TODO-4751: (Optional/deferred) Implement a real, working experimental `Map<K,V>` collection type
  - owner: ai
  - created_at: 2026-07-29
  - phase: New feature (not a bug fix)
  - parallel_track: hidden-test-failures-imports-operations
  - depends_on: (none)
  - scope: TODO-4741's investigation found that the capitalized
    experimental `Map<K, V>` collection type (distinct from both the
    lowercase builtin `map<K, V>` and the underlying `MapValue<K, V>`
    struct that `stdlib/std/collections/map.prime` actually defines) does
    not exist anywhere - no stdlib struct named `Map`, no `mapSingle`
    function, and `mapPair` is only special-cased as a nested argument to
    `count`/`capacity`, not as a general constructor. Roughly 28+ test
    cases across `test_compile_run_imports_operations.cpp`,
    `test_compile_run_vm_collections_wrapper_temporaries_reject_count.cpp`,
    `test_compile_run_vm_collections_wrapper_temporaries_templated.cpp`,
    and others assume this type is meant to work (their TEST_CASE names
    literally say "runs vm experimental map ..."), and extensive
    supporting machinery already exists in the compiler for resolving
    `Map` as an alias-ish receiver
    (`TemplateMonomorphExperimentalCollectionReceiverResolution.h`'s
    `isUnspecializedExperimentalKeyValueBackingTypeForReceiverResolution`
    etc.) - suggesting this was a genuinely-planned feature whose stdlib
    half was never finished, not a typo or abandoned idea.
  - implementation_notes: decide (with the user, this is a design
    question, not purely mechanical) whether `Map<K,V>` should be (a) a
    thin struct wrapping `MapValue<K,V>` the way `Vector<T>` is itself
    the canonical struct (no separate `-Value` split for vectors), or (b)
    a true alias/rename. Then add `mapSingle<K,V>`/a general (non-nested)
    `mapPair<K,V>` constructor, and wire template-instantiation to
    recognize `Map` as templated (the root cause of "template arguments
    are only supported on templated definitions: /Map").
  - acceptance: this is scoped as OPTIONAL/deferred - only pursue if the
    experimental `Map<K,V>` surface is still wanted going forward; if the
    decision is "no, this experimental surface should be retired," the
    ~28+ tests re-pinned to reject by TODO-4741 stay as permanent
    rejection tests instead, and this TODO should be closed as "won't
    fix, surface retired" rather than implemented.
  - stop_rule: do not start implementing without confirming the design
    direction first (option (a) vs (b) above) - this is a multi-file
    stdlib + compiler feature addition with real design tradeoffs, not a
    mechanical fix, and guessing wrong risks a second round of rework.

- [ ] TODO-4752: Fix struct field access on freshly-returned temporaries reading default/zeroed values instead of the real field
  - owner: ai
  - created_at: 2026-07-29
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-imports-operations
  - depends_on: (none)
  - scope: found while triaging "container error contract conformance in
    C++ emitter"
    (`tests/unit/compile_run/test_compile_run_container_error_conformance_helpers.h`).
    Minimal repro on `--emit=vm`:
    `print_line(/ContainerError/why(/ContainerError/missing_key()))`
    prints the wrong ("container error", the why() fallback) instead of
    the correct ("container missing key") text - `missing_key()` returns
    a `ContainerError{1i32}` struct temporary directly into the `why(...)`
    call. Binding the SAME call to a local first works correctly:
    `[ContainerError] err{/ContainerError/missing_key()}; print_line(/ContainerError/why(err))`
    prints "container missing key" as expected. Isolated further:
    `[ContainerError] err{...}; print_line(err.code)` (bound) correctly
    prints `1`, so the struct literal and field itself are fine - the bug
    is specifically about a struct value returned directly from one call
    and immediately passed as an argument to another call (or having a
    field read off it inline) without an intervening local binding. The
    full test source's `total` sum (built from four `.code` field reads
    directly off inline call results, e.g.
    `/ContainerError/missing_key().code`) also comes out as `0` instead
    of the correct `10`, consistent with the same root cause.
  - implementation_notes: this smells like a temporary-value lifetime or
    calling-convention bug - the callee likely receives/reads the struct
    before it's fully materialized, or the field-read path assumes the
    receiver is an addressable local (has a stack slot) and silently
    reads garbage/zero for a bare call-result temporary that doesn't have
    one yet. Compare how struct-returning call results are lowered/passed
    when used as a bare local's initializer (works) vs. passed straight
    into another call's argument position or dotted into for a field read
    (broken). Since ARM64/x86_64 native backends ALSO showed a
    (different) `ContainerError`-related bug in this exact test (every
    `print_line(string)` call truncated to one character on native, "c"
    instead of the real string, exit code 10 - i.e. the field-read part
    may actually be fine on native but plain string printing is broken)
    - investigate that natively-specific truncation separately, it may or
    may not share a root cause with the vm-side temporary bug.
  - acceptance: `test_compile_run_container_error_conformance_helpers.h`'s
    `expectContainerErrorConformance` reverts to the fully-correct pinned
    values for both vm (exit 10, "container missing key" x8 then
    "container error") and native (same text, exit 10, no truncation)
    once both bugs are fixed - re-pinned in the meantime to the verified
    current (buggy) output so the suite stays green without hiding this.
  - stop_rule: don't assume the vm-side "temporary field access" bug and
    the native-side "string truncation" bug are the same root cause just
    because they show up in the same test - verify independently (the vm
    repro above never touches native, and the native truncation affects
    literal-string print_line calls that don't involve field access at
    all, e.g. print_line of already-correct string content), and confirm
    the fix for one doesn't mask investigating the other.
  - progress_2026-08-05: **the vm-side bug is confirmed fixed** - it was
    the same root cause as TODO-4757 (the `hasScalarOrVoidReturn`
    real-call-eligibility fix in `IrLowererRecursionAnalysis.cpp`
    already excludes `ContainerError` from real-call treatment). Both
    `expectContainerErrorConformance`'s `vm` branch and all 3
    conformance TEST_CASEs (`container error contract conformance in C++
    emitter`, `native imported container error contract conformance`,
    `runs vm imported container error contract conformance`) pass
    currently. The **native-side truncation bug is still open and is
    broader than originally scoped** - it is NOT specific to `why()`,
    `ContainerError`, or unbound temporaries: `[return<string>]
    makeMsg() { return("hello world"raw_utf8) }` then `[string]
    msg{makeMsg()}; print_line(msg)` (fully bound, no field access, no
    error-struct types involved at all) still prints only `h` on
    `--emit=native`, while the identical source prints the full string
    correctly on `--emit=vm`. A literal bound directly (`[string]
    msg{"hello world"raw_utf8}`, no function call) prints correctly on
    native too - so the truncation is specific to a `string` value that
    crossed a real (non-inlined) native function-call return boundary.
    Since `"string"` is not in `isSupportedScalarTypeName`
    (`IrLowererRecursionAnalysis.cpp:14-26`), it should already be
    ineligible for real-call treatment and forced to inline the same way
    the VM path now does for the four packed-error-struct types - the
    fact that native still truncates suggests the native/ARM64/x86_64
    emitter has its own, separate real-call/struct-return-ABI path that
    doesn't consult (or isn't governed by) this same eligibility
    analysis, and that path's handling of a struct-shaped return value
    (likely a `{Pointer<u8>, i32 length}`-shaped `string`) truncates the
    length to 1 when actually going through a real native call. This is
    a materially different, native-emitter-specific investigation from
    anything already traced for TODO-4757 - needs its own gdb/trace pass
    into the native/ARM64/x86_64 backend's call-emission code (not
    `IrLowererRecursionAnalysis.cpp`, which VM already correctly
    respects) before any fix. Not fixed this session; the acceptance
    criterion's native half remains unmet, so leaving this TODO open
    despite the vm half now being correct.

- [ ] TODO-4812: Modern soa<T>/SoaVector<T> public-surface method-sugar and canonicalization gaps found sweeping text_filters dumps
  - owner: ai
  - created_at: 2026-07-30
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-text-filters
  - depends_on: (none)
  - scope: a catch-all for several distinct drifts found re-pinning
    `test_compile_run_text_filters_dumps.cpp`'s large soa/SoaVector
    ast-semantic dump cluster (~30 cases), after modernizing those tests
    off the now-hard-rejected `import /std/collections/internal_soa(_conversions)/*`
    spelling (see the "direct import of retired soa compatibility modules
    is not supported" rejection, a deliberate TODO-4633-era removal, not
    itself a bug). Distinct findings once the retired imports were
    dropped:
    1. `.push(...)` method-call sugar on a `[soa<Particle>, mut]` or
       `[auto mut]`-typed local fails with `unknown call target: push`
       when no `import /std/collections/*` is present (or, for `[auto
       mut]`, even when the generic import IS present - the `auto`
       inference apparently isn't complete by the time `.push()` is
       resolved). Explicitly `[SoaVector<Particle> mut]`-typed locals
       with `import /std/collections/*` present are unaffected.
    2. Root-level same-path shadow definitions (`/to_aos`, not
       `/soa/to_aos`) are not honored for `SoaVector<Particle>`/public
       `soa<Particle>` receivers the way sibling shadows (`/soa/count`,
       `/soa/get`, `/soa/ref`, `/soa/push`, `/soa/reserve`) are - `.to_aos()`
       method-call sugar resolves straight to the canonical
       `/std/collections/soa/to_aos__` builtin instead, an asymmetry
       between `to_aos` and its siblings.
    3. `count()` can no longer be used inside an expression (only as a
       bare statement) - `plus(count(values), ...)` now rejects with
       `count is only supported as a statement`.
    4. Field-index-view mutation syntax (`values.y()[i]`,
       `y(values)[i]`) no longer routes through a dedicated
       `soaVectorRef__`/`experimental_soa/soaVectorRef__` column-view
       helper - it now lowers to plain per-element
       `ref__(values, i).y`/`ref_ref__(...).y` forms instead. Likely an
       intentional simplification, not a regression.
    5. By-value (non-borrowed) `get`/`count` helper-return receivers now
       canonicalize to the plain `get__`/`count__` forms instead of the
       `_ref` borrowed-reference variants, even when reached through
       `location(...)`/`dereference(...)` wrapper syntax - also likely
       an intentional simplification.
    6. `to_aos__`'s own body no longer directly contains
       `count__`/`get__` calls - the loop was factored into a separate
       `soaVectorToAos__` implementation helper (defined earlier in the
       dump) that uses internal `soaVectorCount__`/`soaVectorGet__`
       names instead of the public spellings.
    7. `soaVectorSingle`/`soaVectorNew`-family helpers now canonicalize
       under `/std/collections/soa/...` instead of the old
       `/std/collections/experimental_soa/...` namespace (consistent with
       the TODO-4633 `soa`/`experimental_soa` merge - not itself a bug).
    Each affected case was re-pinned individually to its exact verified
    current behavior; see the `TODO-4812` comments left at each site in
    `test_compile_run_text_filters_dumps.cpp` for the specific repro and
    message.
  - implementation_notes: (1) and (2) look like the highest-value real
    bugs here (broken/asymmetric method-call-sugar resolution); (3)-(7)
    are more likely intentional simplifications from ongoing soa
    modernization work and may not need code changes, just confirmation.
    Start with (1)'s `auto`-typed-local push failure (narrowest, clearest
    repro) and (2)'s `to_aos` same-path-shadow asymmetry (directly
    parallels the already-tracked TODO-4756 `ref_ref` gap) before the
    rest.
  - acceptance: split into properly-scoped sub-TODOs once triaged - this
    entry's job is first to determine which of the 7 findings above are
    genuine bugs (fix) vs. intentional (just confirm and close).
  - stop_rule: do not attempt to fix all 7 findings under one change -
    they very likely have different root causes (mixing method-sugar
    resolution, template/type inference timing, and IR-lowering loop
    factoring); triage into separate leaves before writing any code.
  - investigated_2026-08-07: triaged finding (1) (`.push()` sugar without
    import). Confirmed the exact asymmetry: `values.count()` on a
    `[soa<Particle> mut]` local resolves and runs fine with NO import at
    all, while `values.push(...)` on the identical receiver rejects with
    "unknown call target: push" - traced to
    `resolveMethodCallPath`/`matchesBuiltinSoaCollectionHelper`
    (`SemanticsValidatorExprMethodTargetResolution.cpp`, ~line 1947),
    whose always-visible-without-import allowlist covers
    `count`/`count_ref`/`get`/`get_ref`/`to_aos`/`to_aos_ref`/ref-like
    helpers but has no `push`/`reserve` entries at all - contrasted with
    this same TODO's own cross-reference elsewhere in this file
    describing `count`/`count_ref`/`get`/`get_ref`/`ref`/`ref_ref`/
    `to_aos`/`to_aos_ref`/`push`/`reserve` as one unified "same-path
    shadow family" for OTHER purposes, suggesting push/reserve's
    exclusion here specifically could be either an oversight or a
    deliberate "read methods always visible, write methods need explicit
    import" design choice - genuinely ambiguous either way from code
    alone. Went one step further and found this isn't a clean binary
    "bug or not": testing the explicitly-typed `[SoaVector<Particle> mut]`
    sibling (not `soa<Particle>`) with the identical no-import `push`
    call produces a THIRD, different behavior - it passes semantic
    validation cleanly (no "unknown call target" at all) but then fails
    at IR LOWERING with `"vm backend only supports arithmetic/.../
    increment/decrement calls in expressions (call=/std/collections/soa/push,
    ...)"`, an entirely different rejection class. So the three receiver
    spellings (`soa<Particle>` without import, `SoaVector<Particle>`
    without import, either with import) each hit a different code path
    with different behavior for the exact same logical operation - this
    is more tangled than a single allowlist gap and needs a design
    decision (should push/reserve require import like write-mutators
    elsewhere, or be uniformly visible like their sibling family members)
    before a fix should be attempted; not fixed this session, leaving
    for a session that can get that design question answered first
    rather than guess.

- [ ] TODO-4809: collect-diagnostics collection-helper (count/capacity) diagnostic collection collapses or corrupts messages when a definition mixes map- and vector-receiver errors
  - owner: ai
  - created_at: 2026-07-30
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-text-filters
  - depends_on: (none)
  - scope: found sweeping the ~150-case `--collect-diagnostics`/
    `--emit-diagnostics` cluster across
    `test_compile_run_text_filters_diagnostics_*.cpp`. Three related
    diagnostic-collection bugs, all in the same subsystem:
    1. **Mixed map/vector collection-helper diagnostic collapse.** When
       a single definition contains two separate erroring
       collection-helper calls where one resolves through the `/map/...`
       namespace and the other through `/vector/...` (e.g. `count(m)`
       with a wrong arg count, then `capacity(v, true)` with a wrong arg
       type), only ONE of the two diagnostics survives in
       `--collect-diagnostics` output - never both - regardless of
       source order. Minimal repro:
       ```
       [return<i32>]
       /map/count([map<i32, i32>] values, [i32] marker) {
         return(marker)
       }
       [effects(heap_alloc), return<i32>]
       /vector/capacity([vector<i32>] values, [i32] marker) {
         return(marker)
       }
       [return<i32>]
       bad() {
         [map<i32, i32>] m{map<i32, i32>(1i32, 2i32)}
         [vector<i32>] v{vector<i32>(3i32, 4i32)}
         count(m)
         capacity(v, true)
         return(0i32)
       }
       [return<i32>]
       main() {
         return(0i32)
       }
       ```
       Two identically-named-builtin calls to the SAME namespace (e.g.
       two `/vector/capacity` calls) both collect correctly - only the
       map/vector *mix* triggers the collapse. In some variants the
       surviving diagnostic's message text itself is wrong for its
       reported source position (e.g. "unknown call target: count"
       pointing at a line containing an unrelated `m[true]` expression),
       suggesting the two candidate diagnostics share a single
       overwritten scratch slot rather than each being independently
       collected.
       2. **Multi-diagnostic collection drops all-but-first for
       unresolved imports.** `import /missing_alpha` followed by `import
       /missing_beta` used to collect one "unknown import path: X/*"
       diagnostic per bad import (2 total); it now collects only the
       first (`/missing_alpha`), and that diagnostic's message also lost
       its "/*" suffix (now "unknown import path: /missing_alpha" instead
       of ".../missing_alpha/*").
       3. **Duplicate-definition report picks the last group, not the
       first.** Two duplicate-definition groups in one file (`dup`
       defined twice, then `other` defined twice) used to report the
       FIRST group encountered in source order (`/dup`); it now reports
       the LAST (`/other`) instead - still only one diagnostic total
       (`semanticCount == 1` still holds), just the wrong one relative to
       the "keeps first duplicate-definition payload" test's original
       name/intent.
       Roughly 140+ TEST_CASE assertions across 20 files were re-pinned
       to their exact verified current messages (see the `TODO-4809`
       references left at the individual fix sites, mostly the count/
       capacity call-pair message swaps and the two duplicate-definition/
       import tests).
  - implementation_notes: start with (1) - it's the most reproducible and
    has the clearest minimal repro. Check whatever code path collects
    diagnostics from collection-helper (`count`/`capacity`/`at`/etc.)
    resolution attempts within a single definition - likely a shared
    per-definition (not per-statement) scratch/pending-diagnostic slot
    that gets overwritten by each subsequent collection-helper candidate
    check instead of appended to a list. (2) and (3) may share the same
    root cause (a general "only the last thing written to a shared slot
    survives" pattern) or may be independent - verify before assuming.
  - acceptance: the minimal repro in (1) above collects BOTH the
    `/map/count` arg-count-mismatch and `/vector/capacity`
    arg-type-mismatch diagnostics (2 entries, not 1); the two-bad-import
    repro in (2) collects both diagnostics with the "/*" suffix restored;
    the duplicate-definition repro in (3) reports `/dup` (first group)
    again. All ~140+ re-pinned test cases should revert to checking for
    the multi-diagnostic/first-occurrence forms once fixed - this is a
    large but mechanical re-pin-back pass once the underlying collection
    bug(s) are fixed.
  - stop_rule: do not fix (1)/(2)/(3) as one patch without first
    confirming (via minimal repros, same as above) whether they share a
    root cause - if they turn out to be unrelated, split into separate
    TODOs rather than one combined fix that's hard to verify
    independently.
  - investigated_2026-08-06: root-caused sub-bug (1) via temporary
    instrumentation (added then reverted) in
    `collectDefinitionIntraBodyCallDiagnostics`'s `scanExpr` lambda
    (`SemanticsValidatorPassesDiagnostics.cpp`). It is NOT a shared-
    scratch-slot overwrite as originally hypothesized - the two
    candidate calls are handled by genuinely asymmetric code paths.
    For the repro's `capacity(v, true)` call, some earlier resolution
    pass has already rewritten `expr.name` from the bare `"capacity"`
    to the fully-qualified `"/vector/capacity"` (since `v`'s type
    resolves unambiguously to vector), so `isBuiltinCall(expr)` returns
    false for it and it correctly flows into
    `collectResolvedCallArgumentDiagnostic`, producing the observed
    "argument type mismatch for /vector/capacity ..." diagnostic. For
    the repro's `count(m)` call, no equivalent rewrite ever happens -
    `expr.name` stays the bare, unqualified `"count"` even though `m`
    is a map with a same-path user override at `/map/count`. Because
    `isSimpleCallName(expr, "count")` matches on the bare name alone
    (see `isCollectionHelperBuiltin` in this file), `isBuiltinCall`
    unconditionally classifies bare `count(...)` calls as a generic
    builtin collection helper regardless of any user override, so the
    `!isBuiltinCall(expr)` guard skips it entirely and no diagnostic is
    ever produced for it - not overwritten by capacity's diagnostic,
    simply never generated. This is the same map/vs/vector same-path-
    shadow resolution asymmetry documented as the still-unresolved
    TODO-4756 (bare-name collection-helper calls resolve/rewrite
    correctly for vector receivers but not consistently for map
    receivers) - TODO-4756 was investigated to exhaustion earlier in
    this epic with 4 ruled-out hypotheses and no interception point
    found; fixing sub-bug (1) here requires the same fix as TODO-4756
    and should not be attempted independently of it. Did not investigate
    sub-bugs (2)/(3) further this pass since (1) turned out to depend on
    TODO-4756 rather than being independently tractable; left open.

- [ ] TODO-4816: `IrLowererHelpers.cpp` duplicates canonical vector-helper spellings as literal strings instead of routing through `CollectionSpellingClassifier`
  - owner: ai
  - created_at: 2026-07-30
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-architecture-audits
  - depends_on: (none)
  - scope: found while fixing the `check_vector_surface_traces.py` /
    `check_map_surface_strict_audit.py` / `check_soa_surface_trace_
    inventory.py` governance audits (4 top-level CTest failures outside
    the compile_run test binary). `isBuiltinClassifiedMethodCallTarget`
    in `src/ir_lowerer/IrLowererHelpers.cpp` (around lines 311-339)
    hardcodes the canonical vector/soa helper path spellings
    (`"/std/collections/vector/count"`, `"/std/collections/vector/
    capacity"`, `"/std/collections/vector/at"`, `"/std/collections/
    vector/at_unsafe"`, `"/std/collections/soa/count"`, `"/std/
    collections/soa/to_aos"`) as string literals compared directly
    against `semanticTarget`, rather than asking
    `primec::CollectionSpellingClassifier` (specifically
    `classifyCollectionHelperSpelling` /
    `isResolutionStageCollectionSpellingPrefix`, already the canonical
    owner of collection-path-spelling knowledge per
    `docs/CompatPathResolutionConsolidation.md`) whether a given path is
    a recognized canonical collection-helper spelling. This is genuine
    literal-duplication debt in real code, distinct from the false
    positives elsewhere in this audit sweep, which were unrelated
    production files whose exemption comment used an audit-specific
    marker (`soa-surface-audit: exempt`) instead of the shared
    `collection-surface-audit: exempt` marker all three scripts also
    accept - those were fixed by updating the marker text, not by
    changing any logic.
  - implementation_notes: the compat/lowering-spelling migration epic
    (see the pre-existing "Step 2a/2b/2c: migrate ... to classifier"
    steps earlier in this document) intentionally left call sites like
    this one unmigrated in earlier phases; this is a leftover, not a new
    regression. A migration here would replace each hardcoded
    `semanticTarget == "/std/collections/.../X"` comparison with a
    classifier call that both confirms canonical-collection-domain
    membership and extracts the leaf helper name, then compare the leaf
    name (`count`/`capacity`/`at`/`at_unsafe`/`to_aos`) instead of the
    full path - reads the same but stops literal-duplicating the
    canonical prefix strings.
  - acceptance: `isBuiltinClassifiedMethodCallTarget` no longer contains
    literal `"/std/collections/..."` path strings; behavior is unchanged
    (same builtin-classification decisions) verified by the full
    `PrimeStruct_compile_run_tests` binary staying 100% green before and
    after.
  - stop_rule: do not widen this into a general refactor of
    `IrLowererHelpers.cpp` beyond `isBuiltinClassifiedMethodCallTarget` -
    scope is exactly the literal-duplicated spellings found by this
    audit sweep, not a broader cleanup pass.

- [ ] TODO-5295: Fix `/std/collections/soa/ref_ref<T>(...)` same-path user shadow rejected with "template arguments required" instead of being invoked
  - owner: ai
  - created_at: 2026-09-16
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-vm-collections
  - depends_on: (none)
  - scope: split out of TODO-4756 (closed) once that TODO's count/get/ref
    same-path-shadow-bypass sub-cluster was fixed and this sub-cluster was
    confirmed to be a genuinely distinct, unrelated bug. Repro: "vm runs
    builtin helper-return soa ref_ref same-path helper" in
    `test_compile_run_vm_collections_wrapper_temporaries_reject_count_soa_experimental_runs_borrowed.cpp`
    - a user-defined `/soa/ref_ref([soa<Particle>] values, [vector<i32>] index)`
    same-path shadow is rejected with `Semantic error: template arguments
    required for /std/collections/soa/ref_ref` (exit 2) instead of being
    invoked, for all three call forms in the repro (bare
    `ref_ref(values, idx)`, method-sugar `values.ref_ref(idx)`, and
    `ref_ref(cloneValues(), idx)`). Currently pinned to the verified
    current (buggy) exit code 2 with a matching stderr substring check.
  - implementation_notes: unlike TODO-4756's count/get/ref cluster (an
    IR-lowering-stage wrong-VALUE bug with no diagnostic), this is a
    semantic-validation-stage REJECTION - the error site is wherever
    `/std/collections/soa/ref_ref` gets template-argument-arity-checked
    against a same-path shadow target; compare against
    `/std/collections/soa/count`/`get`/`ref`'s (correctly non-erroring)
    same-path shadow resolution in semantics to find what's different
    about `ref_ref` specifically (it may be the only one of this family
    that's itself declared with template arguments in the canonical
    stdlib, and the user shadow's non-templated signature isn't being
    reconciled correctly).
  - acceptance: the repro's three call forms compile and run, invoking the
    user's `/soa/ref_ref` shadow (each returning 17i32 per the repro) -
    expected sum 17+17+17=51 - instead of rejecting with the template-
    arguments error; the stderr CHECK in the repro test is removed/
    replaced with the correct runtime assertion.
  - stop_rule: do not assume this shares a root cause with TODO-4756's
    now-fixed count/get/ref cluster - they were confirmed to be separate
    bugs (different failure mode: compile-time rejection vs. silent wrong
    value) via this session's investigation; verify independently.

- [ ] TODO-4800: Fix `.at()`/`.at_unsafe()` method-call sugar (and bare `at(pack, N)`) on `args<T>` variadic-pack elements failing to lower on vm with "missing lowered definition: /array/at"
  - owner: ai
  - created_at: 2026-07-30
  - phase: Hidden test failure remediation (emitters cluster)
  - parallel_track: hidden-test-failures-emitters
  - depends_on: (none)
  - scope: found while triaging `primestruct.compile.run.emitters.cpp`.
    Minimal repro on `--emit=vm`:
    ```
    [return<int>]
    packScore([args<string>] values) {
      return(values.at(1i32).count())
    }
    [return<int>]
    main() {
      return(packScore("ab"utf8, "cde"utf8, "fghi"utf8))
    }
    ```
    fails with `VM lowering error: semantic-product method-call target
    missing lowered definition: /array/at` (exit 2) instead of compiling
    and running. Confirmed to reproduce identically across every element
    type tried: `args<string>`, `args<i32>`, `args<Reference<i32>>`,
    `args<Reference<Struct>>`, `args<Pointer<i32>>`,
    `args<Pointer<Struct>>`, and `args<Reference<uninitialized<i32>>>` -
    both the bare `at(values, N)` call form and the `.at(N)`/
    `.at_unsafe(N)` method-call-sugar forms trigger it identically. This
    is the single largest root cause found this session, accounting for
    14 of the 35 `primestruct.compile.run.emitters.cpp` failures re-pinned
    in this pass, spanning
    `test_compile_run_emitters_variadic_pointer_pack_access.cpp` (all 8
    cases), 4 cases in
    `test_compile_run_emitters_variadic_reference_pack_access.cpp`, and 2
    cases in `test_compile_run_emitters_loop_sugar_runtime.cpp`. All
    re-pinned to the verified current rejection (exit 2, this exact
    message) rather than silently papered over.
  - implementation_notes: `/array/at` looks like an internal semantic-
    product target name synthesized for indexed access into a variadic
    args pack (which is represented/lowered similarly to an array), but
    whatever VM-lowering stage is supposed to provide its definition no
    longer does so - contrast with plain indexed access
    (`values[0i32]`), which still works fine in the same sources (only
    `.at(N)`/`at(values, N)` sugar on the pack fails). Likely a
    registration gap in the same "semantic-product method-call target"
    dispatch table implicated by TODO-4753's `remove_at`/`remove_swap`
    gap and TODO-4756's soa `ref_ref` gap - check whether `/array/at`'s
    lowered-definition synthesis was dropped or renamed during a related
    refactor.
  - acceptance: the minimal repro above compiles and runs on `--emit=vm`
    (and exe/native, not independently checked this session); all 14
    re-pinned cases above revert to their original "runs and returns N"
    expectations once fixed.
  - investigated_2026-08-06: attempted a fix by loosening
    `emitVectorIndexedAccessBeforeInline`'s
    (`IrLowererLowerEmitExprTailDispatch.h`, ~line 1142) receiver-type
    gate from requiring `targetInfo.isVectorTarget` to also accept
    `targetInfo.isArgsPackTarget`, on the theory that `.at()`/`at()`
    method-call-sugar dispatch simply wasn't reaching the same
    `emitArrayVectorIndexedAccess` machinery that already correctly
    handles args-pack targets for plain bracket-index access (confirmed
    via code reading that `validateArrayVectorAccessTargetInfo` already
    explicitly permits `isStructArgsPackTarget`/`isMapArgsPackTarget`/
    `isVectorArgsPackTarget`/etc). This did NOT fix the repro - added a
    temporary debug print (reverted) right after the gate and it never
    fired for either the minimal repro's `.at(1i32)` inner call or the
    outer `.count()` chain, meaning `emitVectorIndexedAccessBeforeInline`
    is never even reached for this call shape - some earlier guard
    (`inlineDispatchExpr.kind != Expr::Kind::Call`, the `args.size()!=2`
    check, or the `getBuiltinArrayAccessName`/`resolveVectorHelperAliasName`
    resolution at the top of the lambda) must already be diverting this
    exact case elsewhere before this function's body ever runs, or this
    whole `IrLowererLowerEmitExprTailDispatch.h` code path is only
    reachable from a different call context than the one this repro
    exercises (it's plausible "TailDispatch" is specific to certain
    positions, e.g. return-statement tails, not the general nested
    method-chain-argument position `values.at(1i32).count()` puts the
    `.at()` call in). Reverted cleanly (verified via `git diff`). Next
    step for a future session: trace with a debug print or gdb
    breakpoint starting from the OUTER `.count()` call's dispatch (since
    that's what actually fails) to find where it tries to emit its
    receiver expression (`values.at(1i32)`) and thus discover which
    actual function handles (or fails to handle) `.at()` sugar on an
    args-pack in this nested-receiver position - `IrLowererLowerEmitExprTailDispatch.h`
    may simply be the wrong file for this repro shape entirely.
  - stop_rule: verify the fix doesn't only cover the specific element
    types listed above - reproduce with at least one more untried
    `args<T>` shape (e.g. `args<map<K,V>>` or `args<vector<T>>`) before
    closing, since the bug appears to be about the pack-indexing
    mechanism itself, not any specific element type.
  - investigated_2026-08-08: traced one layer further using `gdb -batch
    -ex "break ... -ex run -ex bt"` on a fresh, simpler repro
    (`args<Pointer<uninitialized<i32>>>`, cross-referenced from
    TODO-4760(b) - see that TODO's own note for the exact source) that,
    unlike the 2026-08-06 attempt's `.count()`-chained repro, DOES reach
    `emitVectorIndexedAccessBeforeInline`
    (`IrLowererLowerEmitExprTailDispatch.h`, ~line 1117) - confirming the
    earlier session's hypothesis that reachability of this function
    depends on the call's syntactic position (this repro's `.at()` calls
    sit inside `init(dereference(values.at(1i32)), 2i32)` /
    `take(dereference(...))` statements, not chained after `.count()`).
    Re-tried the same fix the 2026-08-06 attempt proposed (loosening the
    `targetInfo.isVectorTarget`-only gate at ~line 1146 to also accept
    `targetInfo.isArgsPackTarget`) - this time the function IS reached,
    but the fix still didn't help: added debug prints and found
    `emitVectorIndexedAccessBeforeInline` bails out even EARLIER than the
    `targetInfo` gate, at the accessName-resolution step itself (~line
    1124): for the method-call form (`values.at(1i32)`,
    `resolvedAccessPath` reported as `/array/at`), `getBuiltinArrayAccessName`
    returns FALSE - so does the equivalent bracket-index-sugar form
    (`resolvedAccessPath` `/at`), meaning accessName must be getting set
    via `resolveVectorHelperAliasName` for whichever of the two actually
    works (not confirmed which, or whether the print's "isMethodCall=0"
    line really was the bracket form and not a coincidental substring
    match against an unrelated node also containing "at", since the
    debug filter used a loose `name.find("at") != npos` check). Reverted
    both the loosened gate and all debug prints cleanly (verified via
    `git diff`) rather than land a fix that doesn't actually work. Next
    step for a future session: instrument `resolveVectorHelperAliasName`
    itself (not just `getBuiltinArrayAccessName`) to find which
    resolution path the WORKING bracket-index form actually takes, then
    check why that same path doesn't also match the method-call form -
    the two forms clearly diverge before `emitVectorIndexedAccessBeforeInline`'s
    `targetInfo`/`isMethodCall` gates are ever reached, so fixing those
    gates (as both this and the 2026-08-06 attempt did) treats a symptom
    one layer too late.

- [ ] TODO-4801: Direct (non-method) call to a canonical map ref-form helper (e.g. `/std/collections/map/count_ref<K,V>(...)`) used in an expression fails to lower on vm
  - owner: ai
  - created_at: 2026-07-30
  - phase: Hidden test failure remediation (emitters cluster)
  - parallel_track: hidden-test-failures-emitters
  - depends_on: (none)
  - scope: found via "C++ emitter materializes variadic borrowed map
    packs with indexed count_ref calls" in
    `test_compile_run_emitters_variadic_file_packs.cpp`. Minimal repro on
    `--emit=vm`:
    ```
    import /std/collections/map/*
    [return<int> effects(heap_alloc)]
    main() {
      [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
      return(/std/collections/map/count_ref<i32, i32>(location(values)))
    }
    ```
    fails with `VM lowering error: vm backend only supports arithmetic/
    comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/
    increment/decrement calls in expressions (call=/std/collections/map/
    count_ref, name=/std/collections/map/count_ref__<mangled>, args=1,
    method=false)` (exit 2) - the "vm backend" wording is produced by a
    `native backend` -> `vm backend` string substitution applied to a
    shared lowering-error message (see `IrBackendProfiles.cpp`'s
    `replaceAll(error, "native backend", "vm backend")`), so this is
    really the same shared "unhandled call shape in expression position"
    fallback used across both backends. Re-pinned the one affected
    TEST_CASE to this exact verified rejection.
  - implementation_notes: this is the map-side sibling of TODO-4756's
    soa `ref_ref`/`to_aos_ref`/`count_ref` gaps - compare how
    `/std/collections/soa/count_ref` and other `_ref`-suffixed soa
    helpers get (or don't get) registered for inline-call-in-expression
    dispatch versus how `/std/collections/map/count_ref` should be
    registered analogously. The failing call here is a fully-qualified,
    explicitly-templated, non-method direct call - check whether
    method-call-sugar form (`values.count_ref()`, if that spelling even
    exists for map) resolves differently before assuming this is purely
    a registration-table gap.
  - acceptance: the minimal repro above runs and returns 2 (the map's
    element count) instead of rejecting; the re-pinned TEST_CASE reverts
    to its original "runs and returns 11" expectation once fixed.
  - stop_rule: do not conflate this with TODO-4800 above just because
    both are variadic-args-pack-adjacent findings from the same session -
    TODO-4800's repro reproduces with zero use of `map` or `count_ref`
    at all (plain `args<string>`), so verify independently before
    assuming a shared fix.
  - investigated_2026-08-07: confirmed the bare method-call-sugar form
    (`count_ref(location(values))`) doesn't even exist as a callable
    spelling (`unknown call target: count_ref` at the semantic layer),
    and that the direct-call rejection also fires identically when the
    call result is first bound to a local rather than used inline in
    `return(...)` - ruling out both alternate framings the
    implementation_notes suggested checking. Found the dispatch chain in
    `IrLowererLowerStatementsExpr.h` (~line 1718) that handles this exact
    call shape for `count` specifically: it checks
    `resolveSameFamilyKeyValueHelperMemberName(...) == "count"` gated by
    `hasSemanticKeyValueHelperDefinition("count")`, then rewrites to the
    canonical helper path and emits an inline definition call - `count_ref`
    is never included in this check anywhere in the file (confirmed via
    grep - only bare `"count"` string-literal comparisons exist, no
    `"count_ref"` ones in any of the several map/vector count-dispatch
    blocks in this file). Attempted the obvious fix (add
    `|| keyValueCountHelperName == "count_ref"` alongside the existing
    `"count"` check at that site) and rebuilt/retested - it made no
    difference at all to the observed rejection, meaning this exact
    block either never gets reached for `count_ref` (some earlier guard
    in the same large `if`, e.g. the `keyValueHelperMetadata() != nullptr`
    check a few lines up, may already fail before this point) or
    `hasSemanticKeyValueHelperDefinition("count_ref")` itself returns
    false (i.e. `count_ref` may not be registered as a recognized stdlib
    surface member for maps at all, unlike vector's `count_ref`/`at_ref`
    family) - not disambiguated this session. Reverted the one-line
    attempt cleanly (verified via `git diff`) rather than land a no-op
    change. This has the same "single guarded dispatch site with several
    plausible failure points, none individually confirmed" shape as the
    exhausted TODO-4756 investigation - next session should add a debug
    print at the `keyValueHelperMetadata()`/`hasSemanticKeyValueHelperDefinition`
    checks specifically (not just the leaf-name comparison this session
    tried) to see which one actually rejects `count_ref` before
    attempting another fix.

- [ ] TODO-4806: Slash-method-call chained off a helper-return vector temporary into `count(...)` fails to lower with "struct parameter type mismatch"
  - owner: ai
  - created_at: 2026-07-30
  - phase: Hidden test failure remediation (emitters cluster)
  - parallel_track: hidden-test-failures-emitters
  - depends_on: (none)
  - scope: found via "C++ emitter keeps slash-method vector access count
    through builtin string length" in
    `test_compile_run_emitters_wrapper_map_count_and_string_fallback.cpp`.
    Minimal repro on `--emit=vm`:
    ```
    [return<string>]
    /vector/at([vector<i32>] values, [i32] index) {
      return("abc"raw_utf8)
    }
    [effects(heap_alloc), return<vector<i32>>]
    wrapValues() {
      return(vector<i32>(1i32))
    }
    [effects(heap_alloc), return<int>]
    main() {
      return(count(wrapValues()./vector/at(0i32)))
    }
    ```
    fails with `VM lowering error: struct parameter type mismatch` (exit
    2) instead of running and returning 3 (the "abc" string's length).
    The equivalent DIRECT-call form (`count(/vector/at(wrapValues(),
    0i32))`, no slash-method-call chaining) was not independently
    re-tested this session - only the slash-method-call receiver form
    (`wrapValues()./vector/at(0i32)`) was confirmed broken. Re-pinned to
    the verified current rejection.
  - implementation_notes: "struct parameter type mismatch" suggests the
    lowering path is trying to pass the `wrapValues()` result (a
    `vector<i32>`) into `/vector/at`'s first parameter using a struct-
    by-value calling convention that doesn't match what `/vector/at`'s
    actual parameter slot expects when reached via slash-method-call
    syntax on a non-local (helper-return) receiver - compare IR
    generation for this receiver shape against the working local-
    variable-receiver case (`values./vector/at(0i32)` where `values` is
    a bound local, covered by passing sibling tests in the same file).
  - acceptance: the minimal repro above runs and returns 3; the re-pinned
    TEST_CASE reverts to its original "runs and returns 6" expectation
    once fixed (the original test summed two such calls).
  - stop_rule: reproduce the direct-call (non-slash-method) form too
    before closing, to confirm the bug is specifically about
    slash-method-call syntax on a helper-return receiver and not a
    broader "any call forwarding a helper-return vector into
    /vector/at" gap.
  - investigated_2026-08-06: per the stop_rule, reproduced the direct-
    call form (`count(/vector/at(wrapValues(), 0i32))`, no slash-method
    chaining) - it fails identically with the same "struct parameter
    type mismatch" message, so this is NOT slash-method-call-specific;
    the bug is the broader "any call forwarding a helper-return vector
    into /vector/at inside count(...)" gap the stop_rule warned about.
    Root-caused via code reading (no instrumentation needed once the
    right function was found): `isWrapperReturnedKeyValueAccessCall` in
    `IrLowererLowerEmitExpr.h` (checked unconditionally at the very top
    of `emitExpr`'s `Expr::Kind::Call` case, before any other dispatch)
    unconditionally emits `"struct parameter type mismatch"` whenever
    `count(...)`'s single argument is itself a call whose resolved path
    or namespace-scoped name's trailing segment is `at`/`at_unsafe`/
    `at_ref`/`at_unsafe_ref` AND that call's own first argument is itself
    a call (the helper-return receiver, e.g. `wrapValues()`). The
    intended scope (per the name) is presumably map/key-value `at()`
    receivers whose element type is a struct, where wrapping a struct
    value in `count(...)` really is a mismatch - but the actual
    implementation has no such scoping: it fires for ANY `at`/`at_unsafe`
    leaf name regardless of receiver collection kind or actual return
    type, including a plain vector `/vector/at` returning `string` (a
    perfectly valid `count()` target, i.e. string length). Confirmed the
    intended narrower gate, `resolveKeyValueHelperAliasName` (the
    overload actually linked into this translation unit, in
    `IrLowererSetupTypeCollectionHelpers.cpp`), is a permanent stub that
    always returns `false` - so the only thing actually gating this
    check today is the unscoped leaf-name match. Attempted a narrow fix
    (skip the "struct parameter type mismatch" verdict when
    `getBuiltinArrayAccessName` recognizes the candidate as a genuine
    vector access) but verified via debug print that
    `getBuiltinArrayAccessName` itself returns `false` for a bare
    user-defined `/vector/at` path (it only recognizes canonical stdlib-
    registered spellings, by design, per its own "explicitly excluded"
    canonical-path branch found during TODO-4803's investigation) - so
    that exclusion never fires for this repro and doesn't fix it.
    Reverted the attempt (verified clean via `git diff`). A correct fix
    needs to positively determine the receiver `at()` call's actual
    return type (struct vs plain scalar) rather than pattern-matching on
    path shape, and `Definition` has no direct return-type field - the
    return type lives in `Definition::transforms` (e.g. `return<string>`)
    and would need whatever helper this codebase already uses elsewhere
    to extract a definition's declared return type from its transform
    list (not identified this session) before this heuristic can be
    made type-aware. Left open, not fixed.

- [ ] TODO-4807: `resolveMethodCallPath`'s alias<->canonical cross-path fallback broke for several bare-alias vector/map receiver shapes (emitter-internal unit-test regressions, not yet observed end-to-end)
  - owner: ai
  - created_at: 2026-07-30
  - phase: Hidden test failure remediation (emitters cluster)
  - parallel_track: hidden-test-failures-emitters
  - depends_on: (none)
  - scope: found via several `resolveMethodCallPath(...)` unit tests in
    `test_compile_run_emitters_vector_receiver_metadata_resolution.cpp`
    and `test_compile_run_emitters_map_metadata_resolution.cpp` that
    exercise the emitter's internal C++ helper directly (no `.prime`
    source involved, so no end-to-end repro is confirmed to be affected
    yet - see stop_rule). Concretely, given only ONE of a
    alias-path/canonical-path pair has return-kind/return-struct
    metadata registered (e.g. only `/std/collections/vector/at` has
    metadata, not `/vector/at`, or vice versa), `resolveMethodCallPath`
    used to fall back across the pair to find it; this cross-path
    fallback now fails (returns unresolved) specifically when the
    receiver is (a) a plain non-method `Call` node spelled with the
    ALIAS path (`/vector/at`, not `/std/collections/vector/at`), or (b)
    an `isMethodCall=true` node whose `name` is literally the bare alias
    string `/vector/at` (with no `namespacePrefix`) - the equivalent
    canonical-path and parser-shaped (`name="at"` +
    `namespacePrefix="/std/collections/vector"`) spellings both still
    resolve correctly in the same scenarios. Conversely, two DIFFERENT
    resolution branches (rooted non-method-call receivers spelled as
    bare map alias paths like `/map/contains(values, key)`, and bare
    map method-call-sugar `values.at(key)`/`values.at_unsafe(key)`) now
    resolve successfully where they previously (per the pre-existing
    test expectations) did not - i.e. this isn't a uniform "aliases got
    stricter" change, some alias-receiver shapes got MORE permissive and
    others got LESS. All affected TEST_CASEs re-pinned to their exact
    current verified behavior (5 across the two files).
  - implementation_notes: the resolution behavior differs by which of
    the several receiver-shape branches in
    `src/emitter/EmitterBuiltinMethodResolutionHelpers.cpp`'s
    `resolveMethodCallPath` a given call takes (`receiver.kind==Name`,
    `receiver.kind==Call && !isMethodCall` non-method branch, or the
    generic `else` branch reached for `isMethodCall==true` Call
    receivers) - build a small table of (receiver shape, alias vs
    canonical spelling, has-metadata-on-which-path) x (old expected
    result, new actual result) from the re-pinned tests in both files
    before attempting a fix, since a naive "restore the old fallback
    everywhere" change would likely re-break the cases that got MORE
    permissive (which have their own now-passing sibling tests
    elsewhere in the same files that must not regress).
  - acceptance: not yet scoped to specific target behavior - first pass
    should determine whether the pre-change or post-change behavior is
    actually intended for each of the 5 re-pinned assertions (this may
    require asking the user, since both directions are plausible
    deliberate refactor outcomes), then fix `resolveMethodCallPath`
    accordingly and flip the corresponding re-pinned tests back.
  - stop_rule: before spending time on a code fix, try to construct at
    least one real `.prime` source (not a direct C++ unit test) that
    actually observably depends on this fallback behavior end-to-end -
    if none of this session's 35 fixed emitters failures needed it
    (TODO-4800 through 4806 above cover the ones that were end-to-end
    reproducible), this may be purely a metadata-plumbing internal
    inconsistency that never surfaces in real compiled programs, which
    would change this TODO's priority significantly.

