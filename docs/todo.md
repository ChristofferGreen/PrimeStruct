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
| TODO-4710 | Cache stdlib parse results across compile-pipeline test runs | deferred | test-runtime-stdlib-cache |
| TODO-4712 | Grow CTest shard size once cross-test-case pollution is fixed | deferred | test-runtime-shard-consolidation |
| TODO-4732 | Cut compile-run test runtimes with semantic-product golden comparisons | deferred | (none) |
| TODO-4737 | Add a lowered-module invariant for method-call targets | deferred | (none) |
| TODO-4751 | Implement a real experimental `Map<K,V>` collection type | blocked | hidden-test-failures-imports-operations |
| TODO-5314 | Drop bare `Map` from IR lowerer, IR printer and emitter | blocked | (none) |
| TODO-5315 | Dispatch `values[key]` on user structs to their own `at` | ready | user-struct-indexing |
| TODO-5316 | Fix repeated user struct method calls on VM/native | ready | user-struct-method-inlining |
| TODO-4812 | Modern soa/SoaVector public-surface method-sugar gaps | ready | hidden-test-failures-text-filters |
| TODO-4800 | `args<T>` pack `.at()`/`.at_unsafe()` fails to lower on vm | ready | hidden-test-failures-emitters |
| TODO-4801 | Canonical map ref-form helper call fails to lower on vm | ready | hidden-test-failures-emitters |
| TODO-4806 | Chained `count(...)` off helper-return vector fails to lower | ready\* | hidden-test-failures-emitters |
| TODO-4807 | `resolveMethodCallPath` alias/canonical fallback regressions | ready\* | hidden-test-failures-emitters |
| TODO-5309 | Rename the soa `ref_ref` builtin to `ref_borrowed` | deferred | (none) |

\* held out of Ready Now this round. TODO-4806/4807 are the 3rd/4th
`ready` items on the `hidden-test-failures-emitters` track (rule 11 caps
concurrent same-track `Ready Now` items); pick them up once TODO-4800/4801
close. Neither is blocked.

### Ready Now

- TODO-4812 (track: hidden-test-failures-text-filters, surface: `stdlib/std/collections/soa`, `stdlib/std/collections/experimental_soa_vector*`): modern `soa<T>`/`SoaVector<T>` public-surface method-sugar/canonicalization gaps found re-pinning `test_compile_run_text_filters_dumps.cpp`'s soa dump cluster.
- TODO-4800 (track: hidden-test-failures-emitters, surface: vm lowering, `args<T>` variadic-pack access): `.at()`/`.at_unsafe()` method-call sugar (and bare `at(pack, N)`) on `args<T>` elements fails to lower on vm with "missing lowered definition: /array/at".
- TODO-4801 (track: hidden-test-failures-emitters, surface: vm lowering, canonical map ref-form helpers): a direct (non-method) call to a canonical map ref-form helper used in an expression fails to lower on vm.
- TODO-5315 (track: user-struct-indexing, surface: `SemanticsValidatorExprCollectionDispatchSetup.cpp` + semantic-product direct-call targets for bare `at`): `values[key]` on a user struct with its own `at` is rejected instead of dispatching to it.
- TODO-5316 (track: user-struct-method-inlining, surface: `src/ir_lowerer` inline struct-helper calls / `this` binding): calling the same user struct method twice fails VM/native lowering with "does not know identifier: this".

TODO-4751 is `blocked` on TODO-5315/TODO-5316 (TODO-5310 was split on 2026-09-24 into TODO-5312 -> TODO-5313 -> TODO-5314; TODO-5312 landed 2026-09-25; TODO-5313 hit its stop_rule on 2026-09-25 and its classifier removal was folded into TODO-4751; TODO-5314 is now `blocked` on TODO-4751). Held back from this round's Ready Now: TODO-4806/TODO-4807 (same `hidden-test-failures-emitters` track as TODO-4800/4801 - rule 11 caps concurrent same-track items; pick these up once one of the two above closes). TODO-4710/4712/4732/4737 are `deferred` (none are actually `blocked` on a still-open TODO as of the 2026-09-23 pass - see the Queue Summary table and each block's own `log:`) - unstarted scoping/design work or confirmed low-value, not `Ready Now` material this round.

### Immediate Next 10

### Priority Lanes

### Execution Queue

### Task Blocks

- [ ] TODO-4710: Cache stdlib .prime parse results across compile-pipeline test runs
  - owner: ai
  - status: deferred
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
  - status: deferred
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

- [ ] TODO-4732: Cut compile-run test runtimes with semantic-product golden comparisons
  - owner: ai
  - status: deferred
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
  - stop_rule: do not migrate a case without first timing it (in-process
    helper vs. current subprocess form) - the 2026-07-23 log entry already
    found most "obvious" reject-only candidates have no measurable win, so
    a blanket migration risks touching ~292 call sites for near-zero
    benefit; migrate only cases individually confirmed to save real time.

- [ ] TODO-4737: Add a lowered-module invariant - no published method-call target without a materialized definition or builtin classification
  - owner: ai
  - status: deferred
  - created_at: 2026-07-20
  - phase: Test infrastructure
  - scope: the TODO-4731 gap (c) class (semantic product publishes a
    method-call target the lowerer has no definition for) is invisible
    to semantic-product goldens and only surfaced case-by-case. A
    single validator pass over every lowered module asserting the
    invariant catches the whole class everywhere, replacing dozens of
    per-shape "does this lower" cases and guarding future
    materialization gaps by construction. The "materialized definition
    or builtin classification" check already exists but is scattered
    across 6 call sites in two files
    (`IrLowererInlineNativeCallDispatch.cpp:1605,1916` and
    `IrLowererSetupTypeMethodCallResolution.cpp:650,709,753,759`), each
    independently reimplementing a similar-but-not-identical exemption
    whitelist via hardcoded string-literal comparisons - see
    implementation_notes for the full exemption inventory and why a
    general invariant can't just be a static path allowlist.
  - implementation_notes: a genuinely correct, general invariant pass
    needs either (a) fully re-deriving and generalizing all 6 call
    sites' accumulated special-casing (real risk of shipping false
    positives, or missing a nuance and shipping a pass that doesn't
    actually catch anything), or (b) having each of the 6 call sites
    call OUT to one new shared `isBuiltinClassifiedMethodCallTarget(target,
    semanticProgram, callExpr)` helper instead of their own inline
    exemption list, then having the new invariant pass call that SAME
    shared helper - the safer design, provably consistent with existing
    lowering behavior by construction. Half of (b) already shipped (see
    log): `IrLowererInlineNativeCallDispatch.cpp`'s two call sites had a
    byte-identical 5-clause exemption predicate (`/string/count`,
    `/std/collections/vector/count`, `/vector/capacity`, `/soa/count`,
    `/vector/at`+`/at_unsafe`, `/soa/to_aos`), extracted into
    `isBuiltinClassifiedMethodCallTarget(target, callExpr)` in
    `IrLowererHelpers.h`/`.cpp`. `IrLowererSetupTypeMethodCallResolution.cpp`'s
    other 4 call sites remain unextracted: their exemptions
    (`routesExplicitVectorCountMethodThroughArgsPackCount`,
    `directTargetKeepsSyntheticCollectionFallback`,
    `allowsReceiverResolvedVectorMetadataFallback`) are woven into a
    local `resolveLoweredDefinitionPath` lambda doing receiver-type-
    dependent fuzzy path matching, not extractable into a target-string-
    only predicate without either unsafely re-deriving that logic or a
    dedicated decomposition pass over that function first (TODO-4724
    decomposed a *different* function - `SemanticsValidator::resolveMethodTarget`
    in `SemanticsValidatorExprMethodTargetResolution.cpp` - and closed
    without touching this one; it provides no seam here, so this is not
    `blocked_on` anything, just unstarted scoped work). Once a seam
    exists and both remaining files call the shared helper, the new
    invariant pass itself is comparatively small: iterate
    `semanticProgram->methodCallTargets`/`directCallTargets`, for each
    published target call `resolveLoweredDefinitionPath` and the shared
    helper, and error if neither the definition nor the
    builtin-classification check succeeds.
  - acceptance: invariant runs in the lowering pipeline under a test
    flag; deliberately re-introducing the gap (c) bug trips it.
  - stop_rule: do not re-derive `IrLowererSetupTypeMethodCallResolution.cpp`'s
    4 remaining fuzzy-path exemptions from scratch without a real
    before/after diff of the full `ir.pipeline.validation` suite (as the
    2026-07-23c log entry below did for the first 2 sites) - a subtly
    wrong re-derivation would silently narrow or widen which method-call
    targets require a materialized definition, exactly the class of bug
    this task exists to catch.

- [ ] TODO-4751: Implement a real, working experimental `Map<K,V>` collection type
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5315, TODO-5316
  - created_at: 2026-07-29
  - phase: New feature (not a bug fix)
  - parallel_track: hidden-test-failures-imports-operations
  - depends_on: TODO-5315, TODO-5316
  - scope: add the capitalized public `Map<K, V>` collection type, which
    does not exist anywhere today (only the lowercase builtin `map<K, V>`
    and the `MapValue<K, V>` backing struct in
    `stdlib/std/collections/map.prime`), plus `mapSingle<K, V>` and a
    general (non-nested) `mapPair<K, V>` constructor under
    `/std/collections/map/`, and restore the ~28+ compile-run cases that
    TODO-4741 re-pinned to reject (`expect*ExperimentalMap*Conformance` /
    `expectCanonicalMapNamespace*` in
    `tests/unit/compile_run/map_conformance/*expectations.h`, plus
    `test_compile_run_imports_operations.cpp` and the
    `test_compile_run_vm_collections_wrapper_temporaries_*` files).
    Also owns the semantics/monomorph bare-`Map` classifier removal
    folded in from TODO-5313 (stop_rule, 2026-09-25), which must land in
    the same change as the wrapper: the bare `typeName == backingTypeName`
    match of `isExperimentalCollectionBackingTypeName` for `Map`
    (`src/semantics/StdlibCollectionSurfaceHelpers.h`),
    `isBareExperimentalKeyValueBackingTypeName` (same file; one caller in
    `SemanticsValidatorInferCollectionReturnInference.cpp`), the
    `base == typeName` arm of
    `isUnspecializedExperimentalCollectionTypeBaseLocal`
    (`SemanticsBindingTypeHelpers.cpp`), the `base == "Map"` /
    `normalizedType == "Map"` arms of `normalizeCollectionTypePath`
    (`SemanticsValidatorInferCollectionCompatibility.cpp`), the `"Map"`
    arm of `resolveBuiltinKeyValueResultType`
    (`SemanticsValidatorResultHelpers.cpp`) and the bare/generated-bare
    `Map` arm of `normalizeCollectionReceiverTypeName`
    (`TemplateMonomorphCollectionCompatibilityPaths.cpp`).
  - implementation_notes: design decided by the user (2026-09-24): option
    (a) - `Map<K, V>` is a thin public struct owning one `MapValue<K, V>`
    (mirroring how `Vector<T>` is itself the canonical struct), with
    `count/contains/tryAt/at/at_unsafe/insert` methods, not an alias.
    Blocked because the compiler still classifies a bare `Map` spelling as
    the builtin key/value storage type (TODO-5310, split into
    TODO-5312/5313/5314 on 2026-09-24), so a `Map<K, V>`
    wrapper binding is routed onto the `MapValue` helper family and its
    own methods are never selected. Prototype findings and the approaches
    already ruled out are in `docs/todo_log.md` under `## TODO-4751`.
  - acceptance:
    - `[Map<i32, i32> mut] values{mapSingle<i32, i32>(1i32, 4i32)}` with
      `values.insert(...)`, `values.count()`, `values[key]`,
      `count(values)`, explicit `/std/collections/map/insert<K, V>(values,
      ...)` and `Reference<Map<K, V>>` `*_ref` helpers compiles and runs
      on vm/native/exe (i32 keys) with no change to existing lowercase
      `map<K, V>` diagnostics or behavior.
    - every TODO-4741 reject pin whose source uses non-string keys is
      restored to its originally intended runtime expectation (the six
      string-key `map<string, V>` sources were already restored by
      TODO-5311 on 2026-09-25).
    - one positive and one negative (e.g. key-type mismatch) test for the
      new surface; `./scripts/compile.sh --release` at baseline.
    - a user `Map<K, V>` declared in its own namespace publishes
      `/<ns>/Map__t<hash>/<method>` targets for `values.count()` /
      `values.insert(...)`, not `/std/collections/map/*` (semantic-product
      test; the TODO-5313 prototype passed one next to "semantic product
      method-call targets stay separated by receiver type").
  - stop_rule: do not add same-arity `Map`/`MapValue` overloads to the
    canonical helper family (`count`, `at`, `insert`, ...) - that was
    prototyped on 2026-09-24 and regressed 14-19 existing
    `primestruct.semantics.calls_flow.collections` cases because
    downstream code keys on the un-suffixed `/std/collections/map/<helper>`
    paths; route wrapper receivers to the struct's own methods instead
    once the classifier removal above lands. The wrapper also needs
    `values[key]` dispatch (TODO-5315) and repeated method calls
    (TODO-5316). Measured 2026-09-25 (TODO-5313): the classifier removal
    alone makes a namespaced user `Map<i32, i32>` with its own
    `count`/`insert` run through its own methods on vm/native/exe (exit
    1) without any TODO-5314 backend edit, and regresses exactly 21
    cases in 12 shards. 18 are 9 TODO-4741 reject-placeholder sources
    (each pinned twice: `runs vm ...` and the vm-backed `... in C++
    emitter` twin in `test_compile_run_imports_operations.cpp`) that
    spell `Map<string, V>` with the nonexistent `mapPair`/`mapSingle`;
    they still exit 2 but their pinned text changes to `unable to infer
    return type on /buildValues`, `unknown struct type for layout: Map`
    or `unknown call target: /std/collections/map/count`. This task
    restores them to runtime expectations, so re-pin or restore them
    here, not earlier. One of them (`scoreValues([Map<string, i32>]
    values)`, `expectCanonicalMapNamespaceExperimentalParameterConformance`)
    then passes semantics with an unresolved `Map` parameter and fails
    only in VM lowering ("missing semantic-product collection
    specialization") - close that hole (unknown `Map` must stay a
    semantic error when no `Map` struct is visible) before re-pinning.
    The other 3 (`experimental map custom comparable struct keys ...` in
    semantics, vm and C++/exe) test the builtin Comparable-key rule and
    keep their exact diagnostic when respelled `[map<Key, i32>]
    values{/std/collections/map/map<Key, i32>(...)}` - move them to that
    spelling. Full per-case list in `docs/todo_log.md` under
    `## TODO-4751`.

- [ ] TODO-5314: Drop bare `Map` from IR lowerer, IR printer and emitter
  - owner: ai
  - status: blocked
  - blocked_on: TODO-4751
  - created_at: 2026-09-24
  - phase: Follow-up cleanup after TODO-4751 (split from TODO-5310)
  - depends_on: TODO-4751
  - scope: stop the backend-side classifiers from matching a bare `Map`:
    the `normalized == raw || raw + "<"` arm of
    `isExperimentalCollectionTypeName` (`IrLowererSetupTypeCollectionHelpers.cpp`)
    for `Map` (its ~14 `(..., "map", "Map")` callers then only match the
    retired rooted `experimental_map/Map` path and can be deleted), the
    `isExperimentalCollectionTypeBase(base, "map", "Map")` arm of
    `returnKindForTypeName` (`src/ir/IrPrinterHelpers.cpp`), and the
    `normalized == "Map"` arms of `isKeyValueCompatibilityStorageBase`
    (`src/emitter/EmitterHelpersTypes.cpp`) and
    `isKeyValueCollectionTypeNameLocal`
    (`src/emitter/EmitterBuiltinCallPathHelpers.cpp`).
  - implementation_notes: part of the 2026-09-24 combined attempt (see
    TODO-5313 in `docs/todo_finished.md`). Blocked on TODO-4751 because
    the semantics classifier removal it depends on was folded into
    TODO-4751 on 2026-09-25. Not a prerequisite for the wrapper: with
    only that semantics change, a user `Map<i32, i32>` with its own
    `count`/`insert` already ran on vm/native/exe (exit 1), and the
    2026-09-24 combined prototype (these edits included) ran one with
    `count`/`insert`/`at` too (exit 111), so these edits are expected to
    be behavior-neutral once nothing upstream produces bare `Map`. The
    `scripts/check_map_*` audits pass with them, but avoid the literal
    `Map__` text in new comments (it trips `map-backing-type-symbol`).
    The rooted `experimental_map/Map` spellings are still exercised
    directly by classifier unit tests
    (`test_semantics_builtin_array_access_name_classifier.cpp`,
    `test_stdlib_map_ownership_*`), so deleting those arms is a separate
    decision from this leaf.
  - acceptance:
    - a user `Map<K, V>` struct with its own `count()`/`insert()` runs on
      vm/native/exe through its own methods.
    - no `map<K, V>` compile-run/IR test changes result.
    - `./scripts/compile.sh --release` back at baseline.
  - stop_rule: if any `map<K, V>` lowering test changes, stop and record
    which classifier still carries the builtin identity.

- [ ] TODO-5315: Dispatch `values[key]` on user structs to their own `at`
  - owner: ai
  - status: ready
  - created_at: 2026-09-24
  - phase: User struct method dispatch
  - parallel_track: user-struct-indexing
  - depends_on: (none)
  - scope: docs/PrimeStruct.md ("Method calls & indexing") says
    `value[index]` rewrites to `at(value, index)` and is equivalent to
    `value.at(index)`. For a user struct that declares its own `at`,
    `values.at(k)` works but `values[k]` / `at(values, k)` is rejected in
    semantics with `unknown method: /<ns>/<Struct>/at`. The rejection
    comes from `prepareExprCollectionDispatchSetup`
    (`SemanticsValidatorExprCollectionDispatchSetup.cpp`), which fails via
    `resolveLeadingNonCollectionAccessReceiverPath` even when that path is
    a real definition. Make the bare form dispatch to the struct's own
    access helper on vm/native/exe.
  - implementation_notes: 2026-09-24 prototype (not landed): skipping
    that diagnostic when `defMap_` has the path lets semantics pass, but
    the semantic product still publishes `/at` (or
    `/std/collections/map/at` when `/std/collections/*` is imported) as
    the direct-call target, so lowering reaches the builtin
    `emitBuiltinArrayAccess`. The receiver local is `LocalInfo::Kind::Array`
    with the struct's `structTypeName`. Routing
    `IrLowererLowerEmitExprTailDispatch.h` to
    `emitInlineDefinitionCall(expr, <struct>/at)` for such locals worked
    inside `plus(...)` (correct result) but miscompiled `return(values[k])`
    and `[i32] r{values[k]}` (VM "unaligned indirect address"). Struct
    return-path inference (`IrLowererStructReturnPathHelpers.cpp`) likely
    treats `at(structLocal, k)` as struct-valued. The real fix should have
    semantics publish the struct method as the direct-call target, so the
    lowerer needs no receiver heuristics.
  - acceptance:
    - `values[k]`, `at(values, k)` and `values.at(k)` on a user struct
      with an `at` method give the same result in expression, `return`,
      and binding-initializer positions on vm and native.
    - a user struct without `at` still rejects `values[k]` with
      `unknown method: /<ns>/<Struct>/at`.
    - `./scripts/compile.sh --release` back at baseline.
  - stop_rule: if publishing the struct method as the direct-call target
    changes any existing collection (`vector`/`map`/`soa`/`string`)
    indexing test, stop and record which receiver classifier claimed the
    struct.

- [ ] TODO-5316: Fix repeated user struct method calls on VM/native
  - owner: ai
  - status: ready
  - created_at: 2026-09-24
  - phase: User struct method dispatch
  - parallel_track: user-struct-method-inlining
  - depends_on: (none)
  - scope: calling the same user struct method twice in one definition
    fails lowering with `vm backend does not know identifier: this`
    (native: same message). Minimal repro, confirmed on the unmodified
    2026-09-24 baseline: a root-level `[struct] Bag() { [i32 mut]
    total{0i32} [return<i32>] size() { return(plus(this.total, 100i32)) }
    }` with `main` doing `[Bag mut] values{Bag{}}` then
    `return(plus(values.size(), values.size()))`. The same failure occurs
    for two `values.addPair(...)` statements or
    `[i32] a{values.size()} [i32] b{values.size()}`, for generic and
    non-generic structs, and in or out of a namespace. A single call
    works.
  - implementation_notes: suspect the inline-call path
    (`emitInlineDefinitionCall`, `buildInlineCallParameterList` /
    `makeStructHelperThisParam` in `IrLowererCallHelpers.cpp`) caches the
    first inlined body or its `this` local and reuses it without
    rebinding. Start with `--dump-stage ir` for both calls. This blocks
    any realistic struct-backed collection wrapper (TODO-4751 calls
    `insert` repeatedly).
  - acceptance:
    - the repros above run on vm and native (e.g. `plus(values.size(),
      values.size())` exits 200).
    - one new compile-run case pins it.
    - `./scripts/compile.sh --release` back at baseline.
  - stop_rule: if the fix needs a change to the inlining recursion or
    real-call eligibility model, stop and split that out with evidence.

- [ ] TODO-4812: Modern soa<T>/SoaVector<T> public-surface method-sugar and canonicalization gaps found sweeping text_filters dumps
  - owner: ai
  - status: ready
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

- [ ] TODO-4800: Fix `.at()`/`.at_unsafe()` method-call sugar (and bare `at(pack, N)`) on `args<T>` variadic-pack elements failing to lower on vm with "missing lowered definition: /array/at"
  - owner: ai
  - status: ready
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
  - stop_rule: verify the fix doesn't only cover the specific element
    types listed above - reproduce with at least one more untried
    `args<T>` shape (e.g. `args<map<K,V>>` or `args<vector<T>>`) before
    closing, since the bug appears to be about the pack-indexing
    mechanism itself, not any specific element type.

- [ ] TODO-4801: Direct (non-method) call to a canonical map ref-form helper (e.g. `/std/collections/map/count_ref<K,V>(...)`) used in an expression fails to lower on vm
  - owner: ai
  - status: ready
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

- [ ] TODO-4806: Slash-method-call chained off a helper-return vector temporary into `count(...)` fails to lower with "struct parameter type mismatch"
  - owner: ai
  - status: ready
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

- [ ] TODO-4807: `resolveMethodCallPath`'s alias<->canonical cross-path fallback broke for several bare-alias vector/map receiver shapes (emitter-internal unit-test regressions, not yet observed end-to-end)
  - owner: ai
  - status: ready
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

- [ ] TODO-5309: Rename the soa `ref_ref` builtin to `ref_borrowed`
  - owner: ai
  - status: deferred
  - created_at: 2026-09-24
  - phase: Naming/API clarity
  - parallel_track: (none)
  - depends_on: (none)
  - scope: `/std/collections/soa/ref_ref<T>([Reference<SoaVector<T>>] values,
    [i32] index)` (`stdlib/std/collections/soa.prime:230-233`) is the one
    member of the soa accessor family (`count`/`count_ref`/`get`/`get_ref`/
    `ref`/`ref_ref`) whose name doubles a suffix instead of composing two
    distinct axes: which value it returns (`get` = value, `ref` =
    `Reference<T>`) and whether the receiver is borrowed (bare name = by
    value `SoaVector<T>`, `_ref` suffix = `Reference<SoaVector<T>>`).
    `ref_ref` collapses "returns a reference" and "receiver is borrowed"
    into one doubled token, which reads like a typo and was genuinely
    confusing enough to prompt a user question outside any specific bug
    investigation. Rename to `ref_borrowed` (or another name that keeps
    `ref`'s existing "returns a reference" meaning and makes "receiver is
    borrowed" explicit rather than doubling the suffix - confirm exact
    spelling before implementing, this scope intentionally doesn't lock it
    in). This TODO was filed immediately after TODO-5295/5307/5308 (closed
    2026-09-23/24) all landed real bug fixes in this exact accessor's
    same-path-shadow resolution - deliberately deferred rather than
    started immediately, to let that code settle first and avoid
    colliding with any follow-up fixes in the same area.
  - implementation_notes: this is a public stdlib rename, not a local
    refactor - `/std/collections/soa/ref_ref` is `[public]` and callable
    by name from user `.prime` code, and its rooted spelling
    (`/std/collections/soa/ref_ref`) plus the same-path-shadow spelling
    (`/soa/ref_ref`) both appear throughout `tests/unit/` (several dozen
    sites, many added/touched by TODO-5295/5307/5308's fixes literally
    today). A safe migration needs: (1) add the new name as the real
    implementation, (2) decide whether the old name stays as a
    deprecated/compatibility alias or is deleted outright (check this
    repo's usual policy for renaming public stdlib symbols - search
    `docs/PrimeStruct.md`/`docs/CompatPathResolutionConsolidation.md` for
    precedent), (3) update every call site across `stdlib/`, `tests/`, and
    any docs that reference `ref_ref` by name.
  - acceptance:
    - The soa accessor family's naming consistently encodes "returns a
      reference" and "receiver is borrowed" as two separable axes, not a
      doubled suffix.
    - Every test and stdlib call site is updated to the new name (or the
      old name is kept working as a documented compatibility alias, per
      whatever migration policy step (2) above settles on).
    - `docs/PrimeStruct.md` (or wherever this accessor family is
      documented) reflects the new name.
  - stop_rule: do not start this while any of TODO-5295/5307/5308's
    immediate follow-up work is still active in this same file
    (`src/semantics/TemplateMonomorphExpressionRewrite.cpp`,
    `stdlib/std/collections/soa.prime`) - confirm no other in-flight task
    touches soa same-path-shadow resolution first, to avoid a rename
    landing on top of a still-moving target.

