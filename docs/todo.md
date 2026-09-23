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
| TODO-4751 | Implement a real experimental `Map<K,V>` collection type | ready | hidden-test-failures-imports-operations |
| TODO-4752 | Fix struct field access on freshly-returned temporaries | ready | hidden-test-failures-imports-operations |
| TODO-4812 | Modern soa/SoaVector public-surface method-sugar gaps | ready | hidden-test-failures-text-filters |
| TODO-4809 | collect-diagnostics drops bare map `count(m)` diagnostic | deferred | hidden-test-failures-text-filters |
| TODO-5305 | collect-diagnostics keeps only the first unresolved import | ready | hidden-test-failures-text-filters |
| TODO-5306 | collect-diagnostics reports last duplicate-definition group | ready\* | hidden-test-failures-text-filters |
| TODO-4816 | `IrLowererHelpers.cpp` hardcodes vector-helper spellings | ready | hidden-test-failures-architecture-audits |
| TODO-5295 | `/soa/ref_ref<T>` same-path shadow wrongly rejected | ready | hidden-test-failures-vm-collections |
| TODO-4800 | `args<T>` pack `.at()`/`.at_unsafe()` fails to lower on vm | ready | hidden-test-failures-emitters |
| TODO-4801 | Canonical map ref-form helper call fails to lower on vm | ready | hidden-test-failures-emitters |
| TODO-4806 | Chained `count(...)` off helper-return vector fails to lower | ready\* | hidden-test-failures-emitters |
| TODO-4807 | `resolveMethodCallPath` alias/canonical fallback regressions | ready\* | hidden-test-failures-emitters |

\* held out of Ready Now this round. TODO-4806/4807 are the 3rd/4th
`ready` items on the `hidden-test-failures-emitters` track (rule 11 caps
concurrent same-track `Ready Now` items); pick them up once TODO-4800/4801
close. TODO-5306 would exceed the eight-item `Ready Now` cap (rule 9); pick
it up once any `Ready Now` item closes. None of them is blocked.

### Ready Now

- TODO-4751 (track: hidden-test-failures-imports-operations, surface: `stdlib/std/collections` `Map<K,V>` type): implement the missing experimental `Map<K,V>` stdlib type - only the lowercase `map<K,V>` builtin and the underlying `MapValue<K,V>` struct exist today.
- TODO-4752 (track: hidden-test-failures-imports-operations, surface: `ContainerError::why()` / vm backend): a freshly-returned temporary's struct field access reads default/zeroed values instead of the real field on `--emit=vm`.
- TODO-4812 (track: hidden-test-failures-text-filters, surface: `stdlib/std/collections/soa`, `stdlib/std/collections/experimental_soa_vector*`): modern `soa<T>`/`SoaVector<T>` public-surface method-sugar/canonicalization gaps found re-pinning `test_compile_run_text_filters_dumps.cpp`'s soa dump cluster.
- TODO-5305 (track: hidden-test-failures-text-filters, surface: import resolution collect-mode diagnostics): `--collect-diagnostics` keeps only the first unresolved import and drops the "/*" message suffix.
- TODO-4816 (track: hidden-test-failures-architecture-audits, surface: `src/ir_lowerer/IrLowererHelpers.cpp`): `isBuiltinClassifiedMethodCallTarget` hardcodes canonical vector-helper path spellings as literal strings instead of routing through `CollectionSpellingClassifier`.
- TODO-5295 (track: hidden-test-failures-vm-collections, surface: semantics validation for `/std/collections/soa/ref_ref`): a same-path user shadow of `ref_ref<T>` is wrongly rejected with a template-arguments error instead of being invoked.
- TODO-4800 (track: hidden-test-failures-emitters, surface: vm lowering, `args<T>` variadic-pack access): `.at()`/`.at_unsafe()` method-call sugar (and bare `at(pack, N)`) on `args<T>` elements fails to lower on vm with "missing lowered definition: /array/at".
- TODO-4801 (track: hidden-test-failures-emitters, surface: vm lowering, canonical map ref-form helpers): a direct (non-method) call to a canonical map ref-form helper used in an expression fails to lower on vm.

Held back from this round's Ready Now: TODO-4806/TODO-4807 (same `hidden-test-failures-emitters` track as TODO-4800/4801 - rule 11 caps concurrent same-track items; pick these up once one of the two above closes) and TODO-5306 (would exceed the eight-item cap). TODO-4809 is `deferred` pending a map-alias policy decision (see its block). TODO-4710/4712/4732/4737 are `deferred` (none are actually `blocked` on a still-open TODO as of the 2026-09-23 pass - see the Queue Summary table and each block's own `log:`) - unstarted scoping/design work or confirmed low-value, not `Ready Now` material this round.

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

- [ ] TODO-4751: (Optional/deferred) Implement a real, working experimental `Map<K,V>` collection type
  - owner: ai
  - status: ready
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
  - status: ready
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

- [ ] TODO-4809: collect-diagnostics drops the bare map `count(m)` diagnostic when a definition also has a scanner-detected helper error
  - owner: ai
  - status: deferred
  - created_at: 2026-07-30
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-text-filters
  - depends_on: (none)
  - scope: originally three `--collect-diagnostics` bugs; sub-bugs (2)
    (unresolved-import collection) and (3) (duplicate-definition report)
    were split out on 2026-09-23 as TODO-5305 and TODO-5306. What remains
    is sub-bug (1): with a user `/map/count` and `/vector/capacity`
    same-path shadow, a definition containing `count(m)` (wrong arg count)
    and `capacity(v, true)` (wrong arg type) collects only the
    `/vector/capacity` diagnostic. Minimal repro:
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
    Root cause (verified 2026-09-23 with instrumentation, see
    `docs/todo_log.md`): template monomorphization rewrites bare
    `capacity(v, ...)` to `/vector/capacity` via the vector-only same-path
    branch in `preferCanonicalStdlibCollectionHelperPath`
    (`src/semantics/TemplateMonomorphExpressionRewrite.cpp`), but bare
    `count(m)` stays spelled `count`. The intra-body scanner
    (`collectDefinitionIntraBodyCallDiagnostics`,
    `src/semantics/SemanticsValidatorPassesDiagnostics.cpp`) treats
    bare `count` as a builtin and skips it. It records the capacity
    diagnostic, and `SemanticsValidatorPassesDefinitions.cpp` then skips
    full `validateDefinition` for `/bad`, which is the only pass that
    would have rejected `count(m)`.
  - implementation_notes: needs a design decision before any code change.
    The acceptance below (collect a `/map/count` arg-count mismatch)
    requires bare `count(m)` to route to a rooted `/map/count` user
    shadow. A working prototype of that (map mirror of the vector
    same-path monomorph branch plus a scanner exemption; 27 text_filters
    cases re-pinned; diff described in `docs/todo_log.md`) contradicts
    current, deliberately pinned map policy: "rejects vm user map count
    call shadow without imported canonical helper"
    (`test_compile_run_vm_collections_array_and_wrapper_shadows.cpp`),
    "rejects bare map count through compatibility alias when canonical
    helper is absent in C++ emitter"
    (`test_compile_run_emitters_canonical_map_helper_calls.cpp`), and
    "C++ emitter keeps canonical map sugar before compatibility aliases"
    (`test_compile_run_emitters_wrapper_map_count_sugar.cpp`), plus the
    `docs/PrimeStruct.md` note that rooted `/map/*` spellings are
    retiring compatibility seams. Options: (a) change policy so rooted
    `/map/count`/`count_ref` shadows win for bare calls like the vector
    same-path shadows do (the prototype), re-pinning those three tests
    and updating the spec; or (b) keep the policy and instead make
    collect mode also report the validator-only rejection of `count(m)`
    ("unknown call target: count") when the scanner has already
    recorded other diagnostics. (b) is a general change to the
    scanner-then-validator gating and interacts with other
    validator-only errors in the same definition (the repro's `/bad`
    also lacks `effects(heap_alloc)`, which the validator reports
    before `count(m)`).
  - acceptance:
    - A decision between (a) and (b) is recorded in this block.
    - The repro above collects two diagnostics in collect mode: under
      (a) `argument count mismatch for /map/count` plus the
      `/vector/capacity` arg-type mismatch; under (b) the policy
      rejection for `count(m)` plus the `/vector/capacity` mismatch.
    - Every affected text_filters collect-diagnostics case is re-pinned
      to verified output, and the full release gate stays green.
  - stop_rule: do not land option (a) without explicit sign-off; it
    reverses a pinned map-alias policy. Do not widen (b) beyond the
    scanner/validator gating for definitions without first measuring
    how many existing collect-diagnostics cases change.

- [ ] TODO-5305: collect-diagnostics keeps only the first unresolved import and drops its "/*" suffix
  - owner: ai
  - status: ready
  - created_at: 2026-09-23
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-text-filters
  - depends_on: (none)
  - scope: split out of TODO-4809 (was its sub-bug 2). With
    `import /missing_alpha` followed by `import /missing_beta`,
    `--collect-diagnostics` used to collect one
    `unknown import path: <path>/*` diagnostic per unresolved import (2
    total). It now collects only `unknown import path: /missing_alpha`,
    without the `/*` suffix. Re-confirmed 2026-09-23 against the current
    compiler (primec and primevm). Pinned to the current behavior in
    `test_compile_run_text_filters_diagnostics_stable_multi_parse.cpp`
    (two cases, marked `TODO-5305`).
  - implementation_notes: import resolution runs before semantics, so
    start from the import resolver's collect-mode error path, not the
    semantics intra-body scanner that TODO-4809 is about. Also decide
    whether the missing `/*` suffix is a deliberate message change or a
    regression before restoring it.
  - acceptance:
    - The two-bad-import repro collects both unresolved-import
      diagnostics in source order.
    - The message suffix matches whichever form is confirmed intended.
    - Both `TODO-5305` test sites are re-pinned to the fixed behavior.
  - stop_rule: do not combine with TODO-5306 unless a shared root cause
    is confirmed with a minimal repro for each.

- [ ] TODO-5306: collect-diagnostics reports the last duplicate-definition group instead of the first
  - owner: ai
  - status: ready
  - created_at: 2026-09-23
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-text-filters
  - depends_on: (none)
  - scope: split out of TODO-4809 (was its sub-bug 3). A file that
    defines `dup` twice and then `other` twice reports
    `duplicate definition: /other` (the last group) instead of
    `duplicate definition: /dup` (the first group in source order),
    which the "keeps first duplicate-definition payload" test name
    promises. It is still one diagnostic total. Re-confirmed 2026-09-23
    against the current compiler (primec and primevm). Pinned to the
    current behavior in
    `test_compile_run_text_filters_diagnostics_stable_multi_parse.cpp`
    (two cases, marked `TODO-5306`).
  - implementation_notes: look for the duplicate-definition check's
    iteration order (probably a map or set iterated in a non-source
    order, or a "last write wins" error slot). AGENTS.md requires
    deterministic, source-ordered diagnostics.
  - acceptance:
    - The duplicate-definition repro reports `/dup` again.
    - Both `TODO-5306` test sites are re-pinned to the first-group
      expectation.
  - stop_rule: do not change how many duplicate-definition diagnostics
    are collected (still one) in the same change. Collecting every group
    is a separate behavior decision.

- [ ] TODO-4816: `IrLowererHelpers.cpp` duplicates canonical vector-helper spellings as literal strings instead of routing through `CollectionSpellingClassifier`
  - owner: ai
  - status: ready
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
  - status: ready
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

