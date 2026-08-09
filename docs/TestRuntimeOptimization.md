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
- **TODO-4709 — RESOLVED (audit) 2026-08-09; migration explicitly not
  pursued**: audited `compile_run` for cases whose assertions only check
  pass/fail rather than actual program output. 1,470 candidates
  identified (full list in `docs/TODO4709CompileRunAudit.md`). A pilot
  migration attempt found the classification isn't reliably automatable
  (a message that looks semantics-owned can still only manifest via a
  later pipeline stage in practice) and, more importantly, measured that
  the actual achievable win is under 10 seconds off the ~22-minute suite
  - these rejection-only tests never reach codegen, so there was no large
  per-test cost to save here to begin with. Decided not to pursue mass
  migration. See the 2026-08-09 log entries below for both the audit
  methodology and the pilot/measurement that closed this out.
- **TODO-4710** — check whether compile-pipeline test helpers redundantly
  re-parse the same stdlib `.prime` files per test case, and cache if so.
- **TODO-4711** — once the above land, tighten CTest `TIMEOUT` values
  suite-by-suite toward the 30s ceiling instead of the current 300s/600s
  defaults.
- **TODO-4712** — once TODO-4707 proves a suite pollution-free, grow its
  CTest shard size (fewer, bigger shards) so the many small per-shard
  fixed costs (binary launch, doctest registration) stop adding up across
  hundreds of tiny shards for the same total case count. **Deprioritized
  2026-08-08**: TODO-4708's measurement found per-shard fixed cost is only
  ~5-9ms, so this whole angle would save on the order of ~15-20s total,
  not a meaningful fraction of the ~4748s suite. Not closed outright,
  since TODO-4707's pollution-correctness motivation still stands
  independent of the (now much weaker) performance case.
- **TODO-4708** — **RESOLVED 2026-08-08**: measured at 5-9ms per binary
  invocation across all three major suites. Fixed overhead is negligible;
  see the 2026-08-08 log entry below for the full measurement and its
  implication for TODO-4712.
- **TODO-5220 — RESOLVED 2026-08-09**: fixed TODO-4901's exponential-blowup
  call site in `TemplateMonomorphExpressionRewrite.h`. Two earlier
  type-inference-based guard attempts were tried and reverted (each fixed
  the perf bug but regressed 17-30 tests); the working fix gates the
  redundant early receiver rewrite behind a name-based check instead: does
  the outer call's own path leaf match a known collection-helper name, or
  does a plain (non-rewriting) scan of the receiver's subtree find such a
  call anywhere in it. See `docs/todo.md`'s TODO-5220 resolution_summary
  for the full attempt history. Confirmed via direct timing (n=16-term
  repro: 8.19s → ~0.13s; scales linearly through n=48) and a full `ctest
  --parallel 8` run with zero new failures.
- **TODO-5221 — RESOLVED 2026-08-09**: re-measured the full cost
  distribution after TODO-5220. The quaternion-helpers outlier is
  confirmed gone from the top of the list. No new single-bug/multi-shard
  pattern surfaced - the new top offenders (`vm.collections` and
  `emitters.cpp` collection-conformance shards) are distributed real
  backend/toolchain cost across many genuine test cases, not one fixable
  bug. See the 2026-08-09 log entry below for the full measurement.
- **TODO-5222 — RESOLVED 2026-08-09**: investigated all 3 candidate
  levers. `-O0` and a precompiled header for the fixed stdlib includes
  were already implemented and are the dominant win (66% reduction
  measured: 1.168s → 0.390s on a representative generated `.cpp`). Tested
  adding `-fuse-ld=lld` on top and found only a ~3% further improvement -
  not worth the portability risk, so not adopted. `--emit=native` doesn't
  invoke an external toolchain at all, so this whole cost class is
  specific to `--emit=cpp`/`exe`. TODO-4709's audit (downgrading
  pass/fail-only cases off exe/native to vm) remains its own separate,
  still-open TODO rather than folded in here. See the 2026-08-09 log
  entry for the full measurement.

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
- 2026-08-08: Full data-driven pass at "make `./scripts/compile.sh
  --release`'s full suite run faster," covering parallelism and real cost
  drivers, using measured data rather than assumptions. Findings:
  - **CTest parallelism was already correctly wired in `scripts/
    compile.sh`** via its `detect_jobs()` helper (portable core-count
    detection: `getconf _NPROCESSORS_ONLN` → `nproc` → `sysctl -n
    hw.ncpu` → fallback 4) feeding `ctest --parallel "$CTEST_JOBS"`. The
    gap was ad-hoc/manual `ctest` invocations (IDE, direct shell) that
    default to **serial** execution. Empirically disproved the common
    assumption that `ctest --parallel 0` means "auto-detect cores": on a
    16-test slice of `calls_flow_control`, `--parallel 1` took 25.04s,
    `--parallel 0` took 23.01s (essentially serial), `--parallel 4` took
    14.52s, and `--parallel 8` (oversubscribed on a 4-core box) took
    15.14s with no regression. Fixed by adding `CMakePresets.json`
    (`debug`/`release`/`relwithdebinfo` presets, each wiring `ctest
    --preset <name>` to `--parallel 8 --output-on-failure` automatically)
    and a new **hard rule in `AGENTS.md`**: never invoke a bare `ctest`
    with no `--parallel <N>` and no preset.
  - **Cost-distribution analysis** of a full run's `CTestCostData.txt`
    (1954 total tests, ~4748.4s total serial-equivalent time): 1499 tests
    (77%) run in ≤1s (sum 90.7s total — nearly free); 327 tests (1-10s
    band) sum to 1213.73s; **128 tests (6.5% of all tests, >10s each)
    consume 3443.93s — 72.5% of total suite time.** This confirms the
    suite's cost is concentrated in a small tail, not spread evenly — the
    highest-leverage work is fixing/trimming that tail, not broad
    per-test micro-optimization.
  - **Registration/startup overhead measured directly** (TODO-4708):
    `time ./<binary> --list-test-cases` and `time ./<binary>
    --test-case="__no_such_test_case_xyz__"` isolate pure process-start +
    doctest-registration cost from actual test execution.  Measured
    **5-9 milliseconds** across all three major binaries
    (`PrimeStruct_semantics_tests`, 2940 cases; `PrimeStruct_backend_ir_tests`,
    1741 cases; `PrimeStruct_compile_run_tests`, 2940 cases). Across all
    1954 shards this is a ~15-20s fixed-cost ceiling total — a rounding
    error against the ~4748s measured total. **This falsifies the premise
    behind TODO-4712** (grow shard size to amortize per-shard fixed cost):
    even eliminating all fixed overhead from every shard would not
    meaningfully reduce runtime. TODO-4708 closed as resolved; TODO-4712
    deprioritized (not closed — TODO-4707's correctness motivation for
    small shards stands on its own).
  - **Traced the actual #1 and #2 slowest shards** (256s and 200s) via
    `ps aux` while they ran live, and found both were spawning `primec
    --emit=wasm`/`--emit=vm` on the exact same pathological "quaternion
    arithmetic helpers with tolerance" source already documented under
    **TODO-4901** (exponential O(2^depth) blowup in
    `TemplateMonomorphExpressionRewrite.h`'s `rewriteExpr`, root-caused
    but not yet fixed earlier this session — see that TODO's entry in
    `docs/todo.md` for the full investigation). That single source is
    referenced from 3 separate test files
    (`test_compile_run_emitters_matrix_quaternion_support.cpp`,
    `test_compile_run_smoke_core_wasm_core.cpp`,
    `test_compile_run_vm_math.cpp`), so one unfixed bug is directly
    multiplying its cost across several of the very slowest shards in the
    entire suite. This makes **TODO-4901's proper fix the single
    highest-ROI item** currently identified for suite runtime — filed as
    **TODO-5220** (narrow fix: gate the one unconditionally-firing call
    site the same way its 3 correctly-gated siblings already are, rather
    than removing it outright, which was tried earlier this session and
    reverted for breaking ~25-30 collection tests).
  - **Action plan / new TODO chain** (see `docs/todo.md`): **TODO-5220**
    (fix TODO-4901 narrowly — top priority) → **TODO-5221** (`depends_on:
    TODO-5220`; re-measure the full cost distribution post-fix and triage
    any newly-surfaced slow outliers for the same
    one-source-multiple-files multiplier pattern) → **TODO-5222**
    (independent of the above two; reduce real C++ toolchain compile+link
    cost for `compile_run`'s `--emit=cpp`/`exe`/`native` cases —
    optimization level, faster linker, completing TODO-4709's existing
    audit of pass/fail-only cases as downgrade candidates).
- 2026-08-09: **TODO-5220 landed** (see `docs/todo.md` for the fix and its
  attempt history) and **TODO-5221's re-measurement** immediately
  followed. Fresh `CTestCostData.txt` from a `ctest --parallel 8` run:
  1953 tests, 8903.78s total serial-equivalent time (1430 tests ≤1s, sum
  108.23s; 300 tests 1-10s, sum 1347.93s; 223 tests >10s, sum 7447.61s).
  - The quaternion-helpers outlier (previously #1/#2 at 256s/200s) is
    **gone** - not present anywhere near the top of the new sorted list,
    confirming TODO-5220's fix actually eliminated the cost, not just
    moved it.
  - New top offenders are `compile_run_vm_collections_collections_newly_exposed_2026_07_16_*`
    shards (220s, 125s, 122s, 58s, 57s, 57s) and
    `compile_run_emitters_cpp_collection_access_and_alias_forwarding_*`/
    `compile_run_emitters_cpp_map_wrapper_and_fallback_inference_*` shards
    (213s, 201s, 187s, 106s, 97s). Investigated the worst one directly:
    re-ran shard `593_602` standalone (no `--parallel 8` contention) and
    it took ~111s serially for its actual 10 test cases (confirmed via
    `--list-test-cases`; doctest's own summary line is misleading here -
    "682 passed" refers to the whole suite's registered-case total, not
    this shard). All 10 are genuine map/vector conformance and
    growth-limit tests (e.g. "runs vm shared stdlib vector conformance
    harness") - broad harnesses legitimately exercising many real backend
    operations, not one fixable bug like quaternion was. **No new
    single-bug/multi-shard pattern found** this time; this cost is
    distributed real backend/toolchain work, which is exactly TODO-5222's
    existing scope rather than a new leaf.
  - Total suite time in this run (8903.78s) is roughly 2x the original
    2026-08-08 baseline (4748.4s) for a similar test count. Cross-checked
    via the same shard's serial-vs-parallel timing (111s serial vs. ~220s
    reported under 8-way parallel contention in the full run, roughly
    2x) - this points to general machine/environment load during the
    parallel run as the likely explanation, not a regression from
    TODO-5220 (which only removes redundant work; it cannot make an
    already-passing case's own logic slower). Worth re-confirming on a
    quieter run before drawing conclusions from absolute totals, but the
    *relative* finding (quaternion gone, no new single-bug pattern) holds
    regardless of the noise on the totals.
  - **Both TODO-5220 and TODO-5221 are now resolved.** TODO-5222 (real
    C++ toolchain compile+link cost reduction) is the only remaining item
    in this doc's tracked chain.
- 2026-08-09: **TODO-5222 investigated and resolved** - no code change
  landed, because the two big, safe wins were already in place and the
  one untried lever wasn't worth its risk. Details:
  - `src/ExternalTooling.cpp`'s `compileCppExecutable` (the `--emit=exe`/
    `cpp` toolchain call) already passes `-O0` to `clang++`, and already
    wires in a precompiled header (`PRIMEC_GENERATED_CPP_PCH_PATH`) for
    the fixed stdlib `#include`s every generated `.cpp` carries. Measured
    the PCH's actual impact directly: compiling a representative
    generated `.cpp` (69KB) with `-O0` alone took 1.168s; with the PCH
    added, 0.390s - a 66% reduction, already captured before this
    investigation.
  - `--emit=native` doesn't invoke an external toolchain at all - it's
    `src/native_emitter/`'s own direct machine-code emitter (confirmed by
    grep: no `clang`/`g++`/`ld`/`ProcessRunner` references in that
    directory) - so the toolchain-cost premise only applies to
    `--emit=cpp`/`exe`.
  - Tried the one remaining candidate lever, `-fuse-ld=lld`: confirmed
    available and functional in this environment, but measured only a
    ~3% further improvement on top of the PCH'd baseline (0.390s →
    0.377s) - a single-TU test executable has almost nothing to link, so
    the linker choice barely matters once the PCH has already removed the
    dominant cost (parsing/instantiating the stdlib includes). Decided
    not to adopt it: a 3% gain doesn't clear the bar against this TODO's
    own stop_rule concern about portability risk (lld/mold availability
    isn't guaranteed everywhere this suite runs).
  - TODO-4709's audit (downgrading pass/fail-only `compile_run` cases off
    `--emit=exe`/`native` to `--emit=vm`) remains open as its own
    separate TODO, not folded into this one, since it has its own "audit
    only" stop rule.
  - **This closes out every TODO in this doc's tracked chain**
    (TODO-4708, TODO-5220, TODO-5221, TODO-5222 all resolved; TODO-4712
    deprioritized with its rationale documented). The remaining
    identified cost in the suite is genuine, distributed per-test
    compile+link/backend work across hundreds of real test cases, not a
    single further fixable inefficiency - see TODO-4709 (separately
    tracked) for the next real lever if suite runtime becomes a priority
    again.
- 2026-08-09: **TODO-4709 audit completed** (audit only, no migrations -
  per its own stop_rule). Scanned all 3,967 `TEST_CASE` bodies across
  `tests/unit/compile_run/`'s 207 files with a mechanical, heuristic
  classifier (script not preserved - one-off scratch analysis) looking
  for the actual discriminator between "compile-time-only" and
  "needs real execution": **not** whether a test passes `--emit=`
  (nearly every file in the directory uses `--emit=` somewhere, so that
  signal turned out to be useless - an earlier, discarded draft of this
  audit used it and was wrong), but whether the test's `runCommand(...)`
  check(s) ever get past the *compile* step at all. This codebase
  consistently uses exit code `2` for compile/semantic rejection
  (verified across dozens of samples) - a test whose entire body is one
  `runCommand(...) == 2` check never produces any runtime behavior
  regardless of which backend was targeted, since the compile itself
  failed. An earlier heuristic attempt bucketed exit codes `{1, 2, 3}`
  together as "rejection-like" and was **wrong**: manually verified
  counter-examples showed exit code `1` is frequently a genuine *computed*
  program result (e.g. `return(plus(count(values), 1i32))` legitimately
  evaluating to `1`), and exit code `3` is this codebase's VM
  runtime-error convention (e.g. an out-of-bounds panic *during*
  execution, not a compile-time rejection) - both require the full
  pipeline, they just happen to use small integers. Final classification:
  - **1,470 SAFE_TO_DOWNGRADE candidates** - single `runCommand(...) == 2`
    check, nothing else.
  - **1,727 NEEDS_FULL_PIPELINE** - checks a computed/executed result,
    multiple `runCommand` calls, or a known program-output-checking
    helper (`*ProgramRuns`, `runVmCommandOrExpectUnsupported`, etc.).
  - **767 AMBIGUOUS** - no literal `runCommand(...) == N` pattern found
    (variable-based comparisons, `CHECK_MESSAGE`, or not really a
    compile/execute behavior test at all - e.g. several
    `test_compile_run_examples_docs_locks.cpp` source-inventory-lock
    cases that don't fit this axis).
  Full file:line lists for all three buckets are in the new
  `docs/TODO4709CompileRunAudit.md`. **This is a heuristic triage list,
  not a certified-safe migration list** - every SAFE-bucket entry still
  needs an individual read before migrating, since a single-exit-code
  pattern match can't fully rule out an edge case the heuristic didn't
  anticipate (as the discarded {1,2,3} attempt demonstrated). No
  migrations were performed in this leaf, per TODO-4709's stop_rule -
  that's real, separate follow-up work (each migration its own leaf, to
  keep correctness verifiable per file).
- 2026-08-09: **Attempted, then explicitly abandoned, migrating TODO-4709's
  1,470 SAFE candidates.** Piloted on
  `test_compile_run_native_backend_core_vector_and_experimental_map_variadics.cpp`
  (12 rejection-only TEST_CASEs) and found two independent reasons this
  isn't safely automatable at scale, plus a measurement that closes the
  question regardless:
  1. **Stage classification by text alone is unreliable.** A message can
     sound semantics-owned (grep confirms "template arguments are only
     supported on templated definitions" originates in
     `src/semantics/TemplateMonomorph*`) yet still only be reachable via
     a later pipeline stage in practice - empirically, this pilot's soa
     test source passes cleanly through `--dump-stage ast-semantic`,
     `--dump-stage semantic-product`, AND `--dump-stage ir` (all exit 0)
     and only fails at the real `--emit=native` compile step. Reliably
     telling these apart needs per-test empirical verification (running
     each candidate through the actual stages and comparing), which is
     itself process-spawning work - eroding the point of the migration.
  2. **A chunk of the audited directory can't be verified here at all.**
     The pilot file (and the whole `test_compile_run_native_backend_core_*`
     family, by the same gate) is compiled only when
     `PRIMESTRUCT_NATIVE_CORE_ENABLED` is `1`, which requires
     `__APPLE__ && __arm64__` - on this Linux x86_64 build it's `0`, so
     these TEST_CASEs never run here and any migration to them can't be
     validated by this session's normal build+test loop.
  3. **The actual achievable win is small.** Direct timing: 20
     invocations of `./primec` on a trivial rejection source averaged
     ~5.9ms each (full attempt) / ~3ms each (`--list-transforms`,
     minimal) - the same order of magnitude as TODO-4708's already-measured
     ~5-9ms test-binary startup cost, not the expensive compile+link cost
     this TODO originally worried about (rejection-only tests never reach
     codegen, so there was no large per-test cost here to begin with).
     Even a perfect migration of all 1,470 candidates would save on the
     order of **under 10 seconds** off the ~22-minute suite.
  **Decision**: do not pursue the mass migration - same shape of finding
  as TODO-4708/TODO-4712 (a plausible-sounding optimization target that,
  once actually measured, turns out to be a rounding error), compounded
  here by real reliability risk in the automated classification itself.
  The audit (`docs/TODO4709CompileRunAudit.md`) stays useful as a
  reference for anyone who wants to hand-migrate a handful of specific
  tests for non-performance reasons (e.g. reducing external-process
  flakiness), but this is closed out as a runtime-optimization lever.
