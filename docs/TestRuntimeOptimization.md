# Test Runtime Optimization

Status: investigation started, not yet a plan. This doc exists to track
findings and decisions as we work toward a fast, hang-proof test suite.
Prerequisite: land the current `primestruct.semantics.type_resolution_graph`
green-out sweep first (see `docs/failing_tests.md` for that work's status)
before investing heavily here, since a moving test surface makes timing
data stale.

## Goals

- **Hard ceiling: no test should ever take more than 30 seconds, on any
  hardware.** A test exceeding this is either doing something it shouldn't
  (real I/O, an unbounded loop, waiting on external state) or has hung
  outright, and either way should fail fast and loud rather than stall a
  run.
- **Typical target: most tests should run in under 5 seconds.** The bulk
  of this suite is single parse-then-validate semantics unit tests with no
  business taking longer than that. Suites that are consistently much
  slower than this are a signal worth investigating, not an accepted
  baseline.
- Corollary: every CTest entry should have an enforced timeout well under
  the 30s ceiling, so a regression that introduces a hang shows up as a
  fast, obvious CI failure instead of a stalled job.

## Why this matters now

While investigating unrelated `type_resolution_graph` test fixes, an
unsharded, ad-hoc invocation of
`./PrimeStruct_semantics_tests --test-suite="primestruct.semantics.calls_flow.collections"`
was left running as a background task and was discovered still spinning
(100% CPU, not blocked) after **2 hours 13 minutes** — nowhere near the
~20-30 minute range this suite has historically taken end-to-end (see the
"Methodology note" in `docs/failing_tests.md`). This is a genuine hang,
not just a slow suite, and it went undetected for that long specifically
*because* the ad-hoc invocation had no timeout.

Re-running the same suite through CTest's actual sharded configuration
(10-case shards, `TIMEOUT 300` each, see `CTestTestfile.cmake`) immediately
did its job: most shards complete in seconds, and shard
`calls_flow_collections_181_190` stood out, taking 3+ minutes on its own
while sibling shards launched around the same time finished and were
replaced by new ones. This matches an existing historical note referencing
that same shard range as previously timeout-prone
(`docs/failing_tests.md` / earlier session history around shards
`181_190`, `191_200`, `201_210`).

**Working conclusion so far:** always drive suite runs through CTest's own
sharded `add_test` commands (or `ctest -R <suite>`), never through a raw,
unsharded `--test-suite=...` binary invocation left unattended. The
unsharded form has no per-shard timeout and can silently run for hours;
the sharded form fails fast and pinpoints the offending shard.

## Test-pyramid shape: too much weight on full compile-and-execute

A second, independent angle (not related to the hang above): a large
fraction of the suite reaches for the heaviest possible test shape —
compile the full pipeline down to a binary and execute it — for things
that may only need semantics validation or IR-level checks.

Rough counts as of this doc's creation:

- **9,443** total `TEST_CASE` entries across `tests/unit/`.
- **3,935** of those (~42%) live under `tests/unit/compile_run/`, whose
  own helper name is `validateProgramThroughCompilePipeline` /
  `runCompilePipeline` — i.e. real import resolution, full codegen, and
  (for most of these) actually spawning and running the produced binary
  to check its output, rather than a raw in-memory
  `Semantics::validate()` call.
- 10 test files outside `compile_run` also reach for
  `validateProgramThroughCompilePipeline`/`runCompilePipeline` directly,
  presumably for cases that need real import resolution (like this
  session's fix to the qualified-Result-spelling try-fact test) without
  needing execution.

Compile-and-execute is the right tool when the thing under test is
runtime *behavior* (does the emitted code actually produce correct
output across VM/native/C++ backends). It's the wrong tool when the
thing under test is a *compile-time* property (does this construct parse,
does this call resolve to the right target, does this diagnostic fire) —
those only need `Semantics::validate()` or a type-resolution-graph
snapshot, both of which skip codegen and process-spawn entirely and
should be one to two orders of magnitude cheaper per case.

**This needs a real audit, not a guess**: many `compile_run` cases likely
*do* need to be compile-and-execute (that's the suite's whole purpose —
end-to-end backend parity). The task is finding the ones that don't:
cases duplicating a semantics-level check that's already covered
elsewhere, or checking something that a snapshot/AST-level assertion
could confirm without ever emitting or running a binary. Candidate
starting point: any `compile_run` case whose assertions only check
`ok`/`error` (pass/fail) rather than inspecting actual program output —
those are strong candidates for downgrading to
`Semantics::validate()`-only, since they're not actually exercising
runtime behavior at all.

## Tracked work

All actionable next steps here are now tracked as leaf TODOs in
`docs/todo.md` (phase "Test runtime optimization") so they get picked up
by the normal queue rather than living only as prose in this doc:

- **TODO-4706** — root-cause the `calls_flow_collections_181_190` shard
  slowness/hang found above: isolate the specific case(s) via
  `--test-case=` bisection, determine real-cost vs. hang vs. environment
  artifact, fix or file the follow-up.
- **TODO-4707** — fix the cross-test-case pollution documented in
  `docs/failing_tests.md` (the ~114-case spurious-failure artifact in
  `calls_flow.collections` and the `imports` wildcard-surface case) that's
  the reason suites are sharded into small 10-case chunks at all; a
  smaller shard count once this is proven clean is a natural follow-up,
  not part of this leaf.
- **TODO-4708** — measure fixed per-invocation binary
  startup/doctest-registration cost, independent of which cases actually
  run, and estimate its share of total suite runtime.
- **TODO-4709** — audit `compile_run` for cases whose assertions only
  check pass/fail rather than actual program output; these are the
  candidates for downgrading off the full compile-and-execute path (see
  "Test-pyramid shape" above). Audit only — no migrations in that leaf.
- **TODO-4710** — check whether compile-pipeline test helpers redundantly
  re-parse the same stdlib `.prime` files per test case, and cache if so.
- **TODO-4711** — once the above land, tighten CTest `TIMEOUT` values
  suite-by-suite toward the 30s ceiling instead of the current 300s/600s
  defaults.
- **TODO-4712** — once TODO-4707 proves a suite pollution-free, grow its
  CTest shard size (fewer, bigger shards) so the many small per-shard
  fixed costs (binary launch, doctest registration) stop adding up across
  hundreds of tiny shards for the same total case count.

This doc stays the narrative/findings log; `docs/todo.md` is the
execution queue — keep them in sync when a TODO's scope or status changes.

## Log

- 2026-07-15: Discovered and killed a 2h13m runaway unsharded
  `calls_flow.collections` run (CPU-bound, not idle/blocked). Re-ran via
  proper CTest sharding.
- 2026-07-15: The sharded run (78 shards, `--parallel 4`, 1305 cases total)
  finished in 1790s wall-clock and immediately paid for itself twice over:
  - **Caught a real regression within minutes** instead of it going
    unnoticed: an unrelated same-session fix to
    `TemplateMonomorphExpressionRewrite.h` (a "shadow precedence" guard on
    vector-count alias fallback) broke 4 test cases across
    `test_semantics_calls_and_flow_collections_vector_alias_unknown_expected_forwarding.cpp`
    and `test_semantics_calls_and_flow_collections_namespaced_collection_statement_body_args.cpp`
    (shards `451_460` and `741_750`). Reverted the guard and fixed the
    actual stale test that had misled the original diagnosis. This is the
    concrete case for why suite-wide sharded runs need to be part of the
    normal verification loop for changes to shared resolution code, not
    just the narrow test file being edited.
  - **Confirmed the shard-181_190-family timeout is real, not a fluke**:
    shards `181_190`, `191_200`, and `201_210` *all three* hit the full
    300s `TIMEOUT` and were killed by CTest. This exactly matches an
    earlier-session historical note about this same shard range being
    previously timeout-prone. TODO-4706 owns root-causing this.
  - All other 73 shards passed in well under a few seconds each (many
    under half a second), reinforcing that this is a localized problem in
    a specific shard range, not a systemic slowness issue.
- 2026-07-15: Root-caused the `181_190`/`191_200`/`201_210` timeout
  cluster (TODO-4706). All three shards fall inside
  `test_semantics_calls_and_flow_collections_container_error_and_result_helpers.cpp`'s
  `SoaColumnsN` coverage (N = 2 through 16 type parameters, exercising the
  real `stdlib/std/collections/soa_storage.prime` templates via
  `validateProgramThroughCompilePipeline`). Per-case wall time scales
  sharply non-linearly with column count, measured standalone/serial (no
  parallel contention, so not a resource-contention artifact):
  - 2-column: ~12s
  - 4-column: ~13s
  - 8-column: ~15s
  - 12-column: ~41s
  - 16-column: **426s** (measured directly; not a hang, the case passes)
  This is genuine template-monomorphization cost on real production
  stdlib code with up to 16 type parameters — not a bug in the test, and
  not an infinite loop. Applied a pragmatic near-term fix: overrode this
  suite's CTest `TIMEOUT` from the shared 300s default to 1200s in
  `cmake/PrimeStructManagedSemanticsSuites.cmake`. That was enough for
  shards `181_190` (221s) and `191_200` (275s), but **not** for
  `201_210`, which stacks the 13-through-16-column cases together and
  still timed out at 1200s — that shard needs re-measurement with more
  headroom and likely either a larger override or splitting the expensive
  tail into smaller shards (tracked as follow-up under TODO-4706).
- 2026-07-15: Live-profiled the underlying monomorphization cost via gdb
  statistical sampling (`gdb -p <pid> -batch -ex "bt 12"`, repeated
  against the live, still-running 16-column case — no separate profiling
  run needed). Findings: deep self-recursion through `rewriteExpr`
  (`TemplateMonomorphExpressionRewrite.h:2099`) walking chained receiver
  expressions, `mapping`/`locals`/`params` already passed by
  const-reference (not the copy-cost trap it could have been), combined
  with a wide, diffuse set of small string-heavy compat/alias-resolution
  helpers invoked per expression node (`resolveCalleePath`,
  `resolveStdlibSurfaceCompatibilityAlias`, `matchesAny`,
  `stdlibSurfaceMatchesSpelling`, `canonicalizeLegacySoaToAosHelperPath`,
  `resolveStdlibSurfaceMemberName`). Live-inspected `ctx.sourceDefs.size()
  == 531`, `ctx.templateDefs.size() == 432` at one sample point. No single
  dominant hot function isolated — cost looks compounded across several
  sources, and the diffuse compat-helper fan-out matches the exact
  fragmented architecture `docs/CompatPathResolutionConsolidation.md`
  already documents as a maintainability problem, suggesting that
  consolidation would plausibly help here too as a side effect. Given this
  session already produced one real regression in this same subsystem
  (the vector-alias fix, reverted above), did not attempt a live
  speculative fix — filed **TODO-4713** with the full profiling writeup so
  a focused follow-up pass (starting with real `callgrind` profiling
  instead of statistical sampling) doesn't have to re-derive this.
- 2026-07-15: Measured shard `201_210`'s full 10-case run standalone with
  a 2400s budget: **1762s total, all 1305 assertions passing** (genuinely
  correct, just slow — not a hang). Raised the suite's CTest `TIMEOUT` to
  2400s (comfortable margin over the measured worst case). Final
  verification via real `ctest -R
  "calls_flow_collections_(181_190|191_200|201_210)"`: all 3 pass —
  `181_190` in 227s, `191_200` in 279s, `201_210` in 1709s. **TODO-4706
  closed** (moved to `docs/todo_finished.md`). The blanket 2400s override
  now applies to all ~78 shards of this suite even though only a handful
  are actually slow — TODO-4712 (shrink shard count once TODO-4707's
  pollution fix lands) is the better long-term answer to that, and
  TODO-4713 (the real algorithmic fix) would shrink the timeout need
  itself.
