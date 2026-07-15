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

## Open questions / next steps

1. **Characterize the `181_190` shard hang**: is this the same
   environment-slowness explanation from the earlier session (confirmed
   real assertion failures were absent, timeout was purely capacity-driven
   at 1200s in that instance), or a genuine hang introduced by a recent
   change? Needs isolating with `--test-case=` on the specific case(s) in
   that shard range and profiling under `gdb`/`perf` if it reproduces
   consistently.
2. **Inventory suite-level runtimes**: get a clean timing breakdown per
   CTest entry (`ctest --output-junit` or `-T Test` timing report) to find
   every suite/shard that's an outlier before optimizing blindly.
3. **Separate "slow" from "hung"**: a shard that reliably takes 45s every
   run is a different problem (real algorithmic cost, or doctest/parse
   overhead multiplied across 10 cases) than one that only occasionally
   times out (flakiness, cross-test-case pollution, resource contention
   under parallel `ctest --parallel N`).
4. **Candidate root causes to check**, roughly in order of suspicion:
   - Whole-process doctest suites with hundreds-to-thousands of cases
     sharing one binary/process — per-case teardown/state reset issues
     already documented (`docs/failing_tests.md` "Flaky, not a real
     failure" section) suggest state leaks across cases that could also
     manifest as slowdowns, not just wrong results.
   - Full compile-pipeline test helpers (`validateProgramThroughCompilePipeline`
     et al.) that write a temp file and invoke the real import resolver —
     inherently heavier than a raw `Semantics::validate()` call; suites
     leaning on this pattern should be checked for whether they need to.
   - Template monomorphization cost on pathological/recursive template
     shapes exercised by generated test fixtures.
   - Parallel `ctest --parallel N` resource contention (CPU oversubscription
     skewing wall-clock time under load) vs. genuinely slow single-threaded
     work — needs a serial vs. parallel timing comparison to separate.
5. **Enforce the ceiling in CI, not just locally**: the managed semantics
   suite macro (`addPrimeStructManagedDoctestSuite`,
   `cmake/PrimeStructManagedSemanticsSuites.cmake`) currently sets
   `TIMEOUT 300` per 10-case shard by default; a separate, older macro path
   defaults to 600s (`PrimeStructSuite_TIMEOUT` in `CMakeLists.txt`). Once
   real per-shard timings are known, tighten these toward the 30s ceiling
   suite-by-suite rather than lowering the global default in one shot,
   since some suites may legitimately need more headroom until their own
   hangs/slowness are fixed.

## Log

- 2026-07-15: Discovered and killed a 2h13m runaway unsharded
  `calls_flow.collections` run (CPU-bound, not idle/blocked). Re-ran via
  proper CTest sharding; `calls_flow_collections_181_190` shard is the
  current outlier under investigation. No root cause identified yet.
