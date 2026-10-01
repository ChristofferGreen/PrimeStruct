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
| TODO-5327 | Allow field access on a method result whose receiver is a borrowed call | ready | borrowed-receiver-field-access |
| TODO-5328 | Keep the Result type for `tryAt` on a block-inferred wrapper call receiver | ready | inferred-call-receiver-try |
| TODO-5329 | Resolve calls inside an `[auto]`-parameter `/Type/method` correctly | ready | auto-param-method-resolution |
| TODO-5330 | Decide whether allocating wrapper constructors may be parameter defaults | ready | wrapper-default-parameters |
| TODO-5331 | Accept a builtin `map<K, V>` temporary in templated canonical `count<K, V>` | ready | canonical-map-count-temporaries |
| TODO-5332 | Resolve method-style `.at(...)` on a builtin `map<K, V>` temporary | ready | canonical-map-method-at |
| TODO-5333 | Index a builtin `map<K, V>` temporary | ready | canonical-map-index-temporary |
| TODO-5334 | Fix the vector `at` argument mismatch inside canonical map temporary lookup | ready | canonical-map-temporary-lookup-vector |
| TODO-5335 | Reject a value-type mismatch in `at<K, V>` on a builtin `map<K, V>` temporary | blocked | canonical-map-at-value-mismatch |
| TODO-5320 | ast-semantic `.to_aos()` spelling vs resolved `/to_aos` shadow | deferred | hidden-test-failures-text-filters |
| TODO-5309 | Rename the soa `ref_ref` builtin to `ref_borrowed` | deferred | (none) |

### Ready Now

- TODO-5327 (track: borrowed-receiver-field-access, surface: VM lowering of field access on a borrowed-call method result): allow chained field access on `borrow(location(v)).at(k).value`.
- TODO-5328 (track: inferred-call-receiver-try, surface: semantics `try` Result inference for `return<auto>` wrapper calls): keep the `Result` type for `tryAt` on an inferred wrapper call receiver.
- TODO-5329 (track: auto-param-method-resolution, surface: lowering of `[auto]`-parameter `/Type/method` bodies): stop resolving `print_line` as `/Holder/print_line`.
- TODO-5330 (track: wrapper-default-parameters, surface: parameter-default purity rule and its spec note): decide on allocating wrapper constructors as defaults.
- TODO-5331 (track: canonical-map-count-temporaries, surface: canonical `map<K, V>` temporary handling in semantics/VM lowering): accept a builtin `map<K, V>` temporary in templated canonical `count<K, V>`.
- TODO-5332 (track: canonical-map-method-at, surface: canonical `map<K, V>` temporary handling in semantics/VM lowering): resolve method-style `.at(...)` on a builtin `map<K, V>` temporary.
- TODO-5333 (track: canonical-map-index-temporary, surface: canonical `map<K, V>` temporary handling in semantics/VM lowering): index a builtin `map<K, V>` temporary.
- TODO-5334 (track: canonical-map-temporary-lookup-vector, surface: canonical `map<K, V>` temporary handling in semantics/VM lowering): fix the vector `at` argument mismatch inside canonical map temporary lookup.

TODO-4751 closed on 2026-09-29 (public `Map<K, V>` wrapper, semantics/monomorph bare-`Map` classifier removal and the TODO-4741 re-pins landed together), which unblocked TODO-5314 (closed 2026-10-01). Its follow-ups TODO-5323 (closed 2026-10-01)/5324 (closed 2026-10-01)/5325 (closed 2026-10-01; its unrunnable shapes became TODO-5327..5330)/5326 (closed 2026-10-01; its canonical-map gaps became TODO-5331..5335) were filed the same day on distinct tracks with disjoint surfaces (stdlib `MapValue` overwrite, a general semantics initializer check, the pinned wrapper conformance helpers, and canonical-map vm test pins). The TODO-5310 split chain is complete (TODO-5312 landed 2026-09-25, TODO-5313's classifier removal was folded into TODO-4751, TODO-5314 closed 2026-10-01). TODO-5320 is `deferred` (dump-spelling fidelity only; behaviour is already correct). TODO-4710/4712/4732/4737 are `deferred` (none are `blocked` on a still-open TODO) - unstarted scoping/design work or confirmed low-value, not `Ready Now` material this round.

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

- [ ] TODO-5327: Allow field access on a method result whose receiver is a borrowed call
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Map wrapper follow-up (split from TODO-5325)
  - parallel_track: borrowed-receiver-field-access
  - depends_on: (none)
  - scope: `borrowExperimentalMap(location(values)).at("left"raw_utf8).value` (and `.at_unsafe(...)`) on a `Map<string, Owned>` fails VM lowering with "struct parameter type mismatch"; binding the result to a local first (`[Owned] x{borrow(...).at(...)}`) then `x.value` works, as does `values.at(...).value` on a plain receiver. Pinned by `expectExperimentalMapReferenceMethodConformance`, which binds intermediates today.
  - acceptance:
    - the chained form runs on vm/native/exe and the conformance source is respelled back to chained access.
    - `./scripts/compile.sh --release` at baseline.
  - stop_rule: if the fix needs a design decision beyond this shape, stop and
    record it here instead of widening the change.

- [ ] TODO-5328: Keep the Result type for `tryAt` on a block-inferred wrapper call receiver
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Map wrapper follow-up (split from TODO-5325)
  - parallel_track: inferred-call-receiver-try
  - depends_on: (none)
  - scope: `[return<auto>] buildValues(...)` returning a `Map<string, i32>` from `if` branches, then `try(buildValues(true).tryAt("left"raw_utf8))`, fails semantics with "try requires Result argument". Pinned by `expectInferredExperimentalMapCallReceiverConformance`.
  - acceptance:
    - the shape runs on vm/native/exe and the pin moves from reject to run.
    - `./scripts/compile.sh --release` at baseline.
  - stop_rule: if the fix needs a design decision beyond this shape, stop and
    record it here instead of widening the change.

- [ ] TODO-5329: Resolve calls inside an `[auto]`-parameter `/Type/method` correctly
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Map wrapper follow-up (split from TODO-5325)
  - parallel_track: auto-param-method-resolution
  - depends_on: (none)
  - scope: `/Holder/score([Holder] self, [auto mut] values)` calling `print_line(...)` lowers `print_line` as `/Holder/print_line` ("vm backend only supports ... calls in expressions (call=/Holder/print_line ...)"). The same body as a free `[auto]` function runs. The method form was part of the original `expectInferredExperimentalMapParameterConformance` source.
  - acceptance:
    - a `/Holder/score` method with an `[auto mut]` wrapper parameter runs on vm/native/exe and is added back to the conformance source.
    - `./scripts/compile.sh --release` at baseline.
  - stop_rule: if the fix needs a design decision beyond this shape, stop and
    record it here instead of widening the change.

- [ ] TODO-5330: Decide whether allocating wrapper constructors may be parameter defaults
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Map wrapper follow-up (split from TODO-5325)
  - parallel_track: wrapper-default-parameters
  - depends_on: (none)
  - scope: `[Map<string, i32> mut] values{mapSingle<string, i32>(...)}` and `[auto mut] values{mapNew<string, i32>()}` style defaults are rejected with "parameter default must be a literal or pure expression" because the constructors carry `effects(heap_alloc)`. TODO-5325 treated that as by-design and passes the maps explicitly; this leaf records the open question.
  - acceptance:
    - either a documented decision (spec note, diagnostic test) that allocating defaults stay rejected, or a feature that allows them with tests.
    - `./scripts/compile.sh --release` at baseline.
  - stop_rule: if the fix needs a design decision beyond this shape, stop and
    record it here instead of widening the change.

- [ ] TODO-5331: Accept a builtin `map<K, V>` temporary in templated canonical `count<K, V>`
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Canonical map follow-up (split from TODO-5326)
  - parallel_track: canonical-map-count-temporaries
  - depends_on: (none)
  - scope: `/std/collections/map/count<string, i32>(wrapMap<string, i32>(...))` where `wrapMap` returns a builtin `map<K, V>` built with `/std/collections/map/map<K, V>(key, value)` fails with "argument type mismatch for /std/collections/map/count... parameter entries: expected .../MapValue__t... got /map". Pinned by "runs vm with templated stdlib wrapper temporary call forms", "...count capacity parity" and the `count key/value mismatch` rejects in `test_compile_run_vm_collections_wrapper_temporaries_*.cpp`.
  - acceptance:
    - the canonical count call and `.count()` method run on the temporary (vm) and the pins move to the real result; the key/value mismatch rejects check their intended mismatch diagnostics.
    - `./scripts/compile.sh --release` at baseline.
  - stop_rule: if the fix needs a design decision beyond this shape, stop and
    record it here instead of widening the change.

- [ ] TODO-5332: Resolve method-style `.at(...)` on a builtin `map<K, V>` temporary
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Canonical map follow-up (split from TODO-5326)
  - parallel_track: canonical-map-method-at
  - depends_on: (none)
  - scope: `wrapMap<string, i32>(...).at("only"raw_utf8)` (and `.at_unsafe`) on a builtin `map<K, V>` temporary fails with "unknown call target: /map/at". Pinned by "runs vm templated stdlib return wrapper temporaries in expressions", the `method arity/missing key` rejects and the unsafe-parity reject.
  - acceptance:
    - method-style `at`/`at_unsafe` run on the temporary and the pins move to real results or intended arity diagnostics.
    - `./scripts/compile.sh --release` at baseline.
  - stop_rule: if the fix needs a design decision beyond this shape, stop and
    record it here instead of widening the change.

- [ ] TODO-5333: Index a builtin `map<K, V>` temporary
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Canonical map follow-up (split from TODO-5326)
  - parallel_track: canonical-map-index-temporary
  - depends_on: (none)
  - scope: `wrapMap<string, i32>(...)["only"raw_utf8]` fails VM lowering with "struct parameter type mismatch: expected .../MapValue__t... got .../Vector__t...". Pinned by "runs vm with templated stdlib wrapper temporary index forms".
  - acceptance:
    - indexing a builtin `map<K, V>` temporary runs on the vm and the pin moves to the real result.
    - `./scripts/compile.sh --release` at baseline.
  - stop_rule: if the fix needs a design decision beyond this shape, stop and
    record it here instead of widening the change.

- [ ] TODO-5334: Fix the vector `at` argument mismatch inside canonical map temporary lookup
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Canonical map follow-up (split from TODO-5326)
  - parallel_track: canonical-map-temporary-lookup-vector
  - depends_on: (none)
  - scope: `/std/collections/map/at<string, i32>(wrapMap<string, i32>(...), "only"raw_utf8)` mixed with vector/index forms fails semantics inside `stdlib/std/collections/map.prime` (`findIndex`): "argument type mismatch for /std/collections/vector/at parameter values: expected .../Vector__t... got vector<string>". Pinned by "runs vm with templated stdlib wrapper temporary syntax parity".
  - acceptance:
    - the syntax-parity source runs on the vm and the pin moves to the real result.
    - `./scripts/compile.sh --release` at baseline.
  - stop_rule: if the fix needs a design decision beyond this shape, stop and
    record it here instead of widening the change.

- [ ] TODO-5335: Reject a value-type mismatch in `at<K, V>` on a builtin `map<K, V>` temporary
  - owner: ai
  - status: blocked
  - created_at: 2026-10-01
  - phase: Canonical map follow-up (split from TODO-5326)
  - parallel_track: canonical-map-at-value-mismatch
  - depends_on: TODO-5331
  - scope: `/std/collections/map/at<string, bool>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8)` is accepted and exits 4 on the vm instead of being rejected as a value-type mismatch (`map<string, i32>` vs `<string, bool>`). Pinned by "rejects vm templated stdlib map wrapper temporary call value mismatch" at exit 4.
  - acceptance:
    - the call is rejected in semantics with a specific type-mismatch diagnostic and the pin checks it. Sequenced after TODO-5331 (same canonical map call type-check path).
    - `./scripts/compile.sh --release` at baseline.
  - stop_rule: if the fix needs a design decision beyond this shape, stop and
    record it here instead of widening the change.

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

