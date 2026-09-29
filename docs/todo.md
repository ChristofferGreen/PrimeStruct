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
| TODO-5316 | Fix repeated user struct method calls on VM/native | ready | user-struct-method-inlining |
| TODO-5320 | ast-semantic `.to_aos()` spelling vs resolved `/to_aos` shadow | deferred | hidden-test-failures-text-filters |
| TODO-5309 | Rename the soa `ref_ref` builtin to `ref_borrowed` | deferred | (none) |

### Ready Now

- TODO-5316 (track: user-struct-method-inlining, surface: `src/ir_lowerer` inline struct-helper calls / `this` binding): calling the same user struct method twice fails VM/native lowering with "does not know identifier: this".

TODO-4751 is `blocked` on TODO-5316 (TODO-5315 closed on 2026-09-29 and its `user-struct-indexing` slot stays empty - no other `ready` leaf remains outside `Ready Now`; TODO-5310 was split on 2026-09-24 into TODO-5312 -> TODO-5313 -> TODO-5314; TODO-5312 landed 2026-09-25; TODO-5313 hit its stop_rule on 2026-09-25 and its classifier removal was folded into TODO-4751; TODO-5314 is now `blocked` on TODO-4751). TODO-4800 closed on 2026-09-28 and TODO-4806 took its slot; TODO-4806 closed on 2026-09-28 and TODO-5322 (held on the same track, disjoint surface from TODO-4807) took its slot; TODO-4801 closed on 2026-09-28 and TODO-4807 (oldest held item on the track, disjoint surface) took its slot. TODO-4807 closed on 2026-09-28 as confirmed internal-only (the legacy AST `primec::Emitter` it lives in is not linked into `primec`; no end-to-end repro) and its slot stays empty (no other `ready` leaf remains outside `Ready Now`). TODO-5322 closed on 2026-09-28 (the last `hidden-test-failures-emitters` leaf) and its slot stays empty (no other `ready` leaf remains outside `Ready Now`). TODO-5320 is `deferred` (dump-spelling fidelity only; behaviour is already correct); TODO-5321 closed on 2026-09-28 and its `hidden-test-failures-text-filters` slot stays empty (no other `ready` leaf on that track). TODO-4710/4712/4732/4737 are `deferred` (none are actually `blocked` on a still-open TODO as of the 2026-09-23 pass - see the Queue Summary table and each block's own `log:`) - unstarted scoping/design work or confirmed low-value, not `Ready Now` material this round.

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
  - blocked_on: TODO-5316
  - created_at: 2026-07-29
  - phase: New feature (not a bug fix)
  - parallel_track: hidden-test-failures-imports-operations
  - depends_on: TODO-5316 (TODO-5315 closed 2026-09-29)
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
    repeated method calls (TODO-5316); `values[key]` dispatch to a user
    struct's own `at` landed with TODO-5315 (2026-09-29; a non-generic
    namespaced `/demo/Map` with its own `at` already indexes through it).
    Measured 2026-09-25 (TODO-5313): the classifier removal alone makes
    a namespaced user `Map<i32, i32>` with its own `count`/`insert` run
    through its own methods on vm/native/exe (exit 1) without any
    TODO-5314 backend edit, and regresses exactly 21 cases in 12 shards.
    18 are 9 TODO-4741 reject-placeholder sources (each pinned twice:
    `runs vm ...` and the vm-backed `... in C++ emitter` twin in
    `test_compile_run_imports_operations.cpp`) that spell `Map<string, V>`
    with the nonexistent `mapPair`/`mapSingle`; they still exit 2 but their
    pinned text changes to `unable to infer return type on /buildValues`,
    `unknown struct type for layout: Map` or `unknown call target:
    /std/collections/map/count`. This task restores them to runtime
    expectations, so re-pin or restore them here, not earlier. One of them
    (`scoreValues([Map<string, i32>] values)`,
    `expectCanonicalMapNamespaceExperimentalParameterConformance`) then
    passes semantics with an unresolved `Map` parameter and fails only in VM
    lowering ("missing semantic-product collection specialization") - close
    that hole (unknown `Map` must stay a semantic error when no `Map` struct
    is visible) before re-pinning. The other 3 (`experimental map custom
    comparable struct keys ...` in semantics, vm and C++/exe) test the
    builtin Comparable-key rule and keep their exact diagnostic when
    respelled `[map<Key, i32>] values{/std/collections/map/map<Key,
    i32>(...)}` - move them to that spelling. Full per-case list in
    `docs/todo_log.md` under `## TODO-4751`.

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

- [ ] TODO-5320: Make ast-semantic `.to_aos()` spelling match the resolved root `/to_aos` shadow
  - owner: ai
  - status: deferred
  - created_at: 2026-09-25
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-text-filters
  - depends_on: (none)
  - scope: split from TODO-4812 finding (2). Behaviour is correct: a root
    `/to_aos([soa<Particle>] values)` (or `[SoaVector<Particle>]`) user
    definition returning 7 is what `values.to_aos()` runs on vm and native
    (exit 7), including the helper-return receiver
    `holder.cloneValues().to_aos()` case. But for a `soa<Particle>`-typed
    local or a helper-return receiver, the ast-semantic dump spells the
    call as `/std/collections/soa/to_aos__t<hash>(values)` while the
    semantic product's `direct_call_targets` entry for it has
    `call_name=/std/collections/soa/to_aos__t<hash>`
    `resolved_path=/to_aos`. An explicit `[SoaVector<Particle>]` local
    dumps `/to_aos(values)` correctly. An AST call name that disagrees with
    the semantic-product target is the same shape as TODO-4756's root
    cause (a lowerer fast path trusting one over the other), so it is a
    latent wrong-target risk plus misleading dump output. A `/soa/to_aos`
    shadow is honoured and dumped correctly for method sugar, like its
    `/soa/count|get|ref|push|reserve` siblings (bare `to_aos(values)`
    does not route to `/soa/to_aos`, like bare get/ref/push/reserve).
  - implementation_notes: the pinned sites are "dump ast-semantic
    rewrites nested struct body soa method shadows" and "dump ast-semantic
    keeps helper-return experimental soa to_aos with same-path helper" in
    `test_compile_run_text_filters_dumps.cpp`; their `TODO-4756 (extends)`
    comments say the shadow is not honoured, which is wrong about
    behaviour - fix those comments when re-pinning.
  - acceptance:
    - the ast-semantic dump shows `/to_aos(values)` (not
      `/std/collections/soa/to_aos__...`) whenever the semantic product
      resolves the call to `/to_aos`, for `soa<T>` locals and helper-return
      receivers.
    - both pinned cases re-pinned with corrected comments;
      `./scripts/compile.sh --release` back at baseline.
  - stop_rule: if the AST rewrite cannot see the shadow without moving
    shadow resolution earlier in `semanticValidationPassManifest()`, stop
    and document the pass-order constraint instead of reordering passes.

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

