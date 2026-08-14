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
- 2026-08-09: User asked to attack the highest-taking tests directly and
  look for refactor opportunities, and separately flagged that tests
  showing very different costs under parallel load vs. standalone is
  itself a problem worth fixing (not just something to write off).
  Investigated the top ~20 entries of a fresh post-TODO-5220
  `CTestCostData.txt` one at a time, verifying each standalone before
  deciding whether it's a real target. Findings:
  - **`PrimeStruct_semantic_memory_definition_worker_parity` (128.68s)
    and `PrimeStruct_semantic_memory_benchmark` (42.98s) are deliberate
    performance-regression benchmarks** (history/trend/budget reports,
    `CMakeLists.txt:1657-1785`), not test-suite bloat - they're supposed
    to run realistic workloads to produce valid benchmark data. Not a
    refactor target.
  - **The entire `emitters_cpp_collection_access_and_alias_forwarding`
    cluster (6 of the original top-30 entries, summing to ~437s) was a
    measurement artifact, not real cost.** Every one of those shards
    ran in well under 1 second when re-run standalone. Root-caused the
    discrepancy: **`CTestCostData.txt` stores a rolling average across
    historical ctest invocations, not a fresh per-run measurement** -
    confirmed by the arithmetic after a second full run: shard `94_95`'s
    cost went from 127.73s (run 1) to 63.883s averaged over "2" runs;
    solving `(127.73 + x) / 2 = 63.883` gives `x ≈ 0.036s` for the
    second run's actual cost - matching the ~0.02-0.06s measured directly
    within a few percent. **One noisy run's numbers stay baked into the
    average for several subsequent runs** before washing out. The
    original 127.73s almost certainly came from residual load on this
    session's shared machine (this session ran a great many interactive
    `primec`/`ctest` commands directly beforehand, some overlapping in
    time) rather than anything intrinsic to the test. Same story for
    `imports_operations_and_collections_155_156` (56.09s recorded, 3.3s
    standalone) and several other entries in the original top-30 that
    didn't reproduce when re-measured directly.
  - **Directly tested whether these tests are unsafe to run alongside
    other heavy tests, since that's the more important question the
    user raised**: raced 3 genuinely heavy `vm.core` shards (each
    35-59s alone) concurrently against the "fast" `94_95` shard and it
    still completed in 0.059s - no slowdown at all. This is real,
    controlled evidence that this specific test is **not** parallel-
    unsafe or resource-starved by concurrent load; the original spike
    really was one-time noise, not a reproducible contention bug.
  - **Two clusters of genuinely, reproducibly slow tests were found**
    (confirmed standalone AND on a fresh full-suite re-run):
    `smoke_core_paths_newly_exposed_2026_07_16_113_122` (~95s,
    consistent across both measurements) and several `vm.core` shards
    testing `ImageError`/gfx helpers (35-59s). Traced both to the exact
    same root cause: **importing `/std/gfx/experimental/*` or
    `/std/image/*` costs several seconds per `primec` invocation just
    from the import itself**, confirmed via a minimal repro
    (`import /std/gfx/experimental/*` + an empty unused `main()`) taking
    ~5.2s at the `--dump-stage semantic-product` checkpoint, vs. ~0.25s
    with no import at all. Narrowing the import to a single symbol
    (`import /std/gfx/experimental/Device`) made no difference (~4.3s) -
    the cost isn't proportional to what's actually used, it's the whole
    module being processed regardless.
  - **This is not a new bug - it's a confirmed instance of TODO-4743's
    already-exhausted investigation.** TODO-4743 (still open, see
    `docs/todo.md`) already profiled the structurally identical
    `/std/image/*` case across 4 rounds (gdb statistical sampling twice,
    then real `callgrind` instrumentation), implemented every fix its
    own investigation surfaced (duplicate O(N)-scan reuse, stdlib-
    registry memoization, an `[=, this]` lambda-capture lifetime audit
    and fix, two additional micro-optimizations found via callgrind),
    took the cost from ~34.85s down to ~17.5s (2x), and then **formally
    invoked its own stop_rule**: the remaining cost is diffuse across
    dozens of small string/allocation/lookup operations integral to the
    compat-path-resolution architecture
    (`docs/CompatPathResolutionConsolidation.md`), not fixable at the
    leaf level without a larger, separately-scoped consolidation
    rewrite. Confirmed today's `smoke_core_paths`/`vm.core` measurements
    (14.7-15s per `ImageError` case, ~95s for a shard chaining 8 gfx
    tests each doing 3 backend invocations) are the same class of cost,
    not a new bug - re-chasing it here would duplicate already-exhausted
    work and violate that stop_rule.
  - **Net conclusion**: after this pass, there is no further
    actionable, safely-fixable single-bug slow-test target left in the
    suite that hasn't already been either fixed (TODO-5220) or
    exhaustively investigated and formally closed as an architectural
    limitation (TODO-4743, referencing TODO-4735's earlier finding on
    the same subject). The main practical takeaway for future sessions:
    **don't trust a single `CTestCostData.txt` snapshot for prioritizing
    work** - always re-verify a "slow" entry standalone before
    investing time in it, since one noisy run's numbers can masquerade
    as a real cost for several subsequent runs.
- 2026-08-10: User asked to fix TODO-4743 directly and explicitly
  authorized the large compat-path-resolution consolidation rewrite if
  it was needed. Checking `docs/CompatPathResolutionConsolidation.md`
  first found **that rewrite had already landed** (all steps through 2c
  marked Complete, real commits) in a separate work stream after
  TODO-4743's own 2026-08-05 stop_rule invocation - but it targeted
  compat-spelling *classification* specifically, not the general
  call-path-resolution cost the gfx/image import symptom actually comes
  from, so it hadn't fixed this. Fresh `gdb` sampling on the gfx-import
  repro (the same investigation as the previous log entry) found a real,
  previously-undiagnosed bug: `resolveCalleePath`'s
  `hasDefinitionFamilyPath` cache lived in a per-definition arena that
  gets destroyed and rebuilt every time validation moves to a new
  definition, even though its answer only depends on `defMap_` /
  `definitionFamilyPathIndex_` - both immutable for the whole validation
  pass. Every new definition in a large imported module was rebuilding
  identical answers from scratch. Fixed by making the cache persist for
  the whole `SemanticsValidator` instance instead (verified safe under
  the opt-in parallel-worker path: each worker gets its own separate
  validator instance, confirmed by reading
  `SemanticsValidatorPassesDefinitions.cpp`, so no cross-thread state to
  race on). Also memoized 4 zero-argument SOA-path-string helpers with
  `static const` locals (same pattern as this TODO's earlier fixes).
  Measured: the gfx-import repro went from ~5.2s to ~3.2s (~38%); the
  TODO's own original `/std/image/*` case (`--emit=vm`, matching its
  historical measurement) went from ~17.5s to ~12.0s (~31% further, on
  top of the already-landed 5x from TODO-4742/4743's earlier work).
  **Still short of the ~2-5s acceptance target** - a fresh profile after
  the fix shows the same diffuse pattern already characterized (no
  single dominant function), confirming this was a real, distinct,
  previously-missed inefficiency rather than duplicate work, but the
  underlying diffuse-cost diagnosis and its stop_rule still stand: no
  further leaf-level fix without a much larger rewrite of the
  whole-program validation model itself (a different, bigger scope than
  the compat-path consolidation that already landed). Verified via a
  full `ctest --parallel 4` run: zero new failures beyond the
  pre-existing `PrimeStruct_vector_surface_traces` gate-script failure.
- 2026-08-10: User asked for sub-1-second import cost, which TODO-4743's
  further micro-optimization couldn't reach (confirmed via
  `--benchmark-semantic-phase-counters`: `import /std/image/*` with an
  unused `main()` visits 4,885 calls and produces 26,862 facts - the
  entire imported module gets validated regardless of usage, since
  `appendStdlibModuleSources` splices whole `.prime` files as text before
  a single parse+validate pass). A follow-up memoization attempt (caching
  more `soa_paths::collectionPath`-family string-building) regressed 16
  tests and was reverted cleanly before committing - not root-caused
  further, given the modest ~12% win wasn't worth the debugging risk.
  Agreed on the real architectural fix with the user: per-module symbol
  manifests (auto-generated, not hand-authored) plus lazy, iterative
  import expansion (linker-style: resolve a reference, pull in only that
  symbol's source, discover new unresolved references, repeat to a fixed
  point) instead of today's whole-file text-splicing. Full plan,
  design rationale, risks, and a 4-phase TODO chain
  (TODO-5223 through TODO-5226) are in the new
  `docs/LibrarySymbolManifestLazyImports.md` - this is a genuinely large
  compiler feature (new manifest format + generator + restructuring the
  import-resolution loop from one-shot to iterative), not a leaf-sized
  fix, and follows the same characterize-first discipline that made
  `docs/CompatPathResolutionConsolidation.md` succeed.
- 2026-08-13: Re-examined TODO-4710's premise (cache stdlib `.prime`
  parse results across compile-pipeline test runs) and confirmed it's
  moot: every `compile_run` test spawns a fresh `./primec` subprocess
  (per TODO-4709's audit), so there's no shared process for a
  cross-test-run cache to live in. While measuring this, found a much
  bigger, real issue: a single compile invocation that imports and uses
  a collection type is dramatically slower than one that doesn't, purely
  in CPU time. Repro (`build-release/`):
  ```
  # no-import baseline
  time ./primec --emit=vm mini_novec.prime --entry /main   # ~7-10ms
  # imports /std/collections/vector/*, calls count(v)
  time ./primec --emit=vm mini_vec.prime --entry /main     # ~2.0-2.2s
  ```
  Reran multiple times, consistent; `valgrind --tool=callgrind` confirmed
  ~100% CPU-bound (13.64B retired instructions), not I/O or cold-cache.
  Top non-libc self-time entries: `primec::semantics::
  splitTopLevelTemplateArgs` (~4.3-5.0%), `normalizeBindingTypeName`
  (~2.9-3.3%), `splitTemplateTypeName` (~2.8-3.2%), `parseBindingInfo`
  plus several lambda-closure invoke entries (~7-8% combined),
  `envelope_internal::findNextEnvelopeStart`/`parseNamespaceBlock`
  (parse-stage text scanning, ~6.5% combined). libc malloc/free/memcpy/
  strlen/memcmp churn collectively ~25-30% of total instructions.
  Verified `normalizeBindingTypeName`, `splitTemplateTypeName`, and
  `splitTopLevelTemplateArgs` (`src/semantics/
  SemanticsBindingTypeHelpers.cpp`) are pure functions of their string
  input with no external mutable state - safe to memoize. Added
  `thread_local unordered_map` caches (thread_local because definition
  validation runs multiple `SemanticsValidator` instances concurrently
  via `std::async` for large definition sets - a shared `static` cache
  would need locking, thread_local needs none and is trivially safe for
  pure functions). Instrumented with temporary debug counters (removed
  before landing) to verify the hypothesis directly rather than just
  assuming: for the `mini_vec.prime` repro, `normalizeBindingTypeName`
  is called ~2.53 **million** times but only 329 are unique inputs (hit
  rate 99.99%); `splitTemplateTypeName` ~3.13 million calls / 316
  unique; `splitTopLevelTemplateArgs` ~971 thousand calls / 97 unique -
  confirming the redundant-re-derivation hypothesis with real numbers,
  not guesswork.
  **Measured result**: `valgrind --tool=callgrind` on byte-identical
  before/after binaries (same build flags/input/machine) shows total
  retired instructions dropped from 13,641,584,142 to 12,853,913,354
  (**-5.78%**), with `splitTopLevelTemplateArgs`'s self-time dropping
  580.6M -> 166.8M (-71%) and `normalizeBindingTypeName`'s 392.0M ->
  207.0M (-47%). `splitTemplateTypeName`'s own self-time barely moved
  (374.9M -> 374.3M) despite a near-100% call-count hit rate - its
  savings show up as reduced downstream malloc/free/memcpy churn
  elsewhere in the profile rather than in its own line, suggesting its
  *inputs* vary more per-call than the other two even though the
  *outputs* repeat.
  **Wall-clock did not move measurably** on this session's sandboxed
  4-core VM (~2.0-2.25s both before and after - within normal ~10%
  run-to-run noise on this box). The instruction-count win is real,
  reproducible, and deterministic (callgrind counts instructions, immune
  to scheduler noise) but too small a share of the ~13B-instruction
  total to surface above wall-clock variance here. **This falls well
  short of the hoped-for outcome** (get close to the ~7-10ms no-import
  baseline) - documenting the honest result rather than a forced number,
  per this doc's established discipline (see the 2026-08-08 log entry's
  own finding that a "plausible" perf target can turn out to be much
  smaller than hoped once actually measured).
  **Why the win is small despite huge call counts**: each memoized call
  is already cheap after the fix (one `thread_local unordered_map`
  lookup, no allocation for short SSO-sized strings) - the dominant
  remaining cost is the sheer CALL VOLUME itself (2.5+ million calls to
  derive facts about a 361-line stdlib file plus a 5-line user program),
  not per-call string-parsing cost. That volume traces to
  `parseBindingInfo` (`src/semantics/SemanticsHelpersCore.cpp`) being
  called from ~50 distinct validator-pass call sites across
  `src/semantics/` (confirmed via `grep -rn "parseBindingInfo("`), each
  re-deriving `BindingInfo` for the same AST node from scratch across
  different passes (uninitialized-locals, effect-free, struct-field
  building, return-kind inference, printability, etc.), compounded by at
  least one whole-program fixed-point graph-inference loop
  (`inferUnknownReturnKindsGraph`'s `do { ... } while (changed)` in
  `SemanticsValidatorInferGraph.cpp`) that re-scans unresolved
  definitions to a fixed point. Getting materially closer to the
  no-import baseline would require caching or restructuring at the
  `parseBindingInfo` level (or higher) - e.g. compute `BindingInfo` once
  per statement and thread it through instead of re-deriving it at each
  of ~50 call sites - which is a substantially larger, riskier change
  (parseBindingInfo takes an `Expr&` plus several context maps/sets by
  reference, not just a string, so caching by `Expr*` alone risks
  returning stale results if callers legitimately pass different
  context) than a leaf-sized memoization, and was left as explicit
  follow-up rather than attempted this session. Tracked as TODO-5230
  (done, resolved to this documented limit); TODO-4710 marked
  superseded pointing here, same pattern as TODO-4740 -> TODO-4804.
  Verified zero regressions: full `./scripts/compile.sh --release`,
  1881/1881 passing, wall-clock 1971.99s (within the documented
  ~1165-1340s baseline range given this session's heavier machine
  load/oversubscription contention on a 4-core sandbox).
- 2026-08-13 (TODO-5231, characterization): Instrumented `parseBindingInfo`
  (`src/semantics/SemanticsHelpersCore.cpp`) with a temporary
  `thread_local`-free debug counter (mutex-protected, since large
  definition sets validate via concurrent `std::async` workers) recording
  total call count, unique `Expr*` pointers touched, unique
  `(Expr*, namespacePrefix, structTypes.size(), importAliases.size(),
  additionalNominalTypes.size(), compileTimeTypeLocals.size())` context
  fingerprints, and `__builtin_return_address(0)` per call site (resolved
  via `dladdr`/`addr2line` against a `-rdynamic -g1` instrumented build),
  plus a separate counter in `inferUnknownReturnKindsGraph`'s
  `do { ... } while (changed)` loop (`SemanticsValidatorInferGraph.cpp`)
  counting fixed-point passes. Removed before landing (see TODO-5232's
  commit for the only surviving code change). For the `mini_vec.prime`
  repro (`import /std/collections/vector/*` + one `count(v)` call,
  reconstructed per this doc's 2026-08-13 entry above since the earlier
  session didn't check the repro file itself into the repo):
  **`inferUnknownReturnKindsGraph` runs only 4 fixed-point passes total**
  for this repro - nowhere near "millions" and not the dominant cost.
  **`parseBindingInfo` is called 804,142 times, touching only 3,211
  unique `Expr*` pointers (9,596 unique `Expr*`+context fingerprints)** -
  i.e. the average `Expr*` is re-parsed ~250 times. Call-site attribution
  (by caller return address) showed **two call sites alone account for
  744,672 of the 804,142 calls (92.6%)**: both inside
  `makeCompileTimeIfRequirementContext` in `SemanticsValidate.cpp`
  (`src/semantics/SemanticsValidate.cpp`, formerly lines 6882 and 6919).
  Reading the surrounding function found the root cause:
  `rewriteCompileTimeIfBranches` loops over every `Definition` in
  `program.definitions` (`N` definitions - stdlib pulls in ~50-100 for
  this repro) and, for **each one**, calls
  `makeCompileTimeIfRequirementContext(program, definition, ...)`, which
  **itself loops over all of `program.definitions` again** (`for (const
  Definition &candidate : program.definitions)`) to build `callables`/
  `structFields`/`structTraits` requirement-predicate facts - re-deriving
  `BindingInfo` for every parameter and struct-binding field of every
  candidate definition, from scratch, on every one of the `N` outer
  iterations. This is an **O(N^2) redundant-recomputation bug**, not a
  worklist-shaped fixed-point-loop problem and not diffuse ~50-call-site
  duplication - a single nested-loop structure inside one function
  accounts for the overwhelming majority of call volume. Conclusion for
  TODO-5232: fix the O(N^2) loop nesting (hoist the candidate-facts
  computation out of the per-definition loop), not the fixed-point loop
  (already cheap) and not a general `Expr*`-keyed cache across all ~50
  call sites (unnecessary given one nesting bug explains 92.6% of calls,
  and would carry the correctness risk TODO-5230 flagged without needing
  to be taken on).
- 2026-08-13 (TODO-5232, fix): Split
  `makeCompileTimeIfRequirementContext` into a new
  `buildCompileTimeIfCandidateFacts(program, structNames, importAliases)`
  (the expensive per-candidate loop, extracted verbatim) and a lean
  per-definition context builder that copies the precomputed facts and
  adds only the current definition's own `params`/`definitionPath`/
  `namespacePrefix`/`templateArgs`. `rewriteCompileTimeIfBranches` now
  calls `buildCompileTimeIfCandidateFacts` exactly once, before its
  per-definition loop, instead of once per definition.
  **Correctness argument** (required before shipping, per TODO-5232's
  stop_rule): the candidate-facts loop's only external mutable input is
  `structNames`, which *does* grow during the per-definition loop (ct_if
  branch resolution inserts freshly-generated, definition-nested struct
  paths via `structNames.insert(generated.fullPath)` in the branch-
  selection helper). This looked at first like it could make hoisting
  unsafe - a later definition's candidate facts might need to see structs
  generated while processing an earlier definition. Read every insertion
  site: every path added to `structNames` during the loop is a freshly
  synthesized *nested* path (`definitionPath + "/" + generatedName`,
  marked `isNested = true`), and generated definitions are only merged
  into `program.definitions` itself *after* the entire per-definition
  loop finishes (`program.definitions.insert(...)` runs once, at the very
  end of `rewriteCompileTimeIfBranches`). Since the candidate-facts loop
  iterates only over `program.definitions` and the sole place growth of
  `structNames` matters to that loop is the membership test
  `structNames.count(candidate.fullPath)` for `candidate` drawn from
  `program.definitions`, and no candidate's `fullPath` can ever equal a
  path inserted mid-loop (mid-loop insertions are synthesized nested paths
  that were never members of `program.definitions` to begin with, and the
  loop itself already rejects true duplicates via the `duplicate
  branch-local generated struct path` error before insertion) - the
  candidate-facts loop's output is **provably identical** regardless of
  whether it runs once before the per-definition loop or freshly inside
  every iteration. This is not "probably fine": every mutation site of the
  set this hoist depends on was read and traced to confirm it cannot alter
  the specific membership queries the hoisted computation performs.
  **Measured result**: `mini_vec.prime` repro - `parseBindingInfo` calls
  dropped 804,142 -> 60,972 (**-92.4%**, ~13.2x fewer calls); wall-clock
  (release build, `--emit=vm`, same machine, byte-identical binary
  before/after aside from this change) dropped from ~2.06-2.12s to
  ~1.26-1.28s (**~40% faster**); `valgrind --tool=callgrind` total
  retired instructions dropped from 13,641,584,142 (TODO-5230's baseline
  measurement, same repro, same machine class) to 7,896,698,452
  (**-42.1%**). A heavier, more realistic repro modeled on
  `tests/unit/compile_run/test_compile_run_vector_conformance_sources.h`'s
  `makeVectorHelperSurfaceConformanceSource` pattern but importing the
  whole `/std/collections/*` module (not just `vector`) went from
  ~2.39-2.44s to ~1.60-1.70s (**~31% faster**), confirming the fix
  generalizes beyond the minimal repro rather than being an artifact of
  its specific shape.
  **Falls short of the <100ms target.** A fresh `callgrind_annotate` after
  the fix shows the profile is now dominated by costs unrelated to
  `parseBindingInfo`/requirement-predicate context building: libc
  malloc/free/malloc_consolidate collectively ~26% of retired
  instructions, and parse-stage whole-file text scanning
  (`envelope_internal::findNextEnvelopeStart`,
  `envelope_internal::parseNamespaceBlock`,
  `findMatchingCloseWithComments`) collectively ~13% - both tied to the
  already-documented whole-file text-splicing import architecture
  (`appendStdlibModuleSources` splices entire `.prime` files as text
  before a single parse+validate pass, so the whole stdlib gets
  parsed/scanned regardless of how much of it is actually used) that the
  2026-08-10 log entry above already scoped as a separate, larger
  architectural fix (`docs/LibrarySymbolManifestLazyImports.md`,
  TODO-5223 through TODO-5226 - lazy, iterative import expansion). That
  work is out of scope for TODO-5232, which targeted specifically the
  `parseBindingInfo`/requirement-predicate-context redundancy TODO-5230
  identified and TODO-5231 characterized - and that specific redundancy
  is now fixed to the extent this leaf's stop_rule calls for (a
  provably-correct hoist of the one nested-loop bug that accounted for
  92.6% of the call volume TODO-5231 measured), with the remainder
  honestly attributed to the separately-tracked import architecture
  rather than forced further within this leaf.
  Verified via `./scripts/compile.sh --release` (single invocation, this
  repo's convention): **1881/1881 tests passing, 0 regressions**,
  full-suite wall-clock 1102.55s (within the documented baseline range
  given normal machine-load variance on this session's 4-core sandbox).
  A mid-session run surfaced one transient failure
  (`...native_window_launcher_and_preflight_56_56`, a docs-content-lock
  test) caused by TODO-5231/5232 themselves being listed in `### Ready
  Now` in `docs/todo.md` before this entry moved them to
  `docs/todo_finished.md` - not a code regression; it passed again once
  `docs/todo.md` was updated to its final state below, and the clean
  1881/1881 run above was taken after that update.
- 2026-08-13 (TODO-5233/5234, compiler arena allocator): Followed up on the
  above entry's observation that libc malloc/free-family functions were
  still ~26-33% of retired instructions post-TODO-5232. Surveyed with
  `valgrind --tool=dhat` (allocation count, not just instruction share) on
  the `mini_vec.prime` repro: **9,756,990 total heap blocks** for one
  compile, 754MB total bytes allocated vs. only ~23MB live at peak -
  diffuse, high-count, short-lived churn dominated by
  `RequirementPredicateDefinitionContext`'s copy constructor
  (`SemanticsValidate.cpp`), not one fixable function. Designed and
  implemented a scoped arena allocator (`src/CompileArena.{h,cpp}`):
  global `operator new`/`delete` overrides, size-classed free lists (not
  naive bump-and-never-free - the 754MB-vs-23MB gap ruled that out as a
  peak-memory risk even for a "safe to leak until exit" CLI process), and
  a uniform allocation header so `operator delete` never guesses from
  ambient thread state. The original design (reset the arena at every new
  compile scope, including one scope per doctest `TEST_CASE`) reproduced a
  **real, deterministic memory-corruption bug** on the full `semantics`
  suite: process-lifetime "magic static" values (`static const
  std::string`/`std::vector` computed once on first call, a pattern used
  dozens of times across `src/semantics/`, distinct from and more numerous
  than the `thread_local` caches the design had already planned to
  invalidate) get arena-allocated if first constructed during an active
  compile scope, then silently corrupted when the *next* scope's reset
  hands the same bytes to a new object while the static is still alive.
  Fell back to the narrower variant TODO-5234's own stop_rule
  pre-authorized: the arena never resets, is entered exactly once per CLI
  process, and the doctest test binaries never construct it at all (stay
  entirely on the system allocator, unaffected by this work). Full
  mechanism, safety argument, and this pivot's rationale are in the new
  `docs/CompilerArenaAllocator.md`.
  **Measured result** (release build, same machine before/after via `git
  stash`): `mini_vec.prime` repro wall-clock ~0.90-0.92s -> ~0.66s
  (**~27% faster**), peak RSS (`getrusage` `ru_maxrss`) ~36MB -> ~28.5MB
  (no regression). Heavier `import /std/collections/*` repro: ~0.89-0.94s
  -> ~0.64-0.68s (**~28% faster**), peak RSS ~43.6MB. Falls short of the
  sub-100ms directional target - consistent with the 2026-08-13
  TODO-5232 entry's own finding that the remaining floor is the
  whole-file text-splicing import architecture
  (`docs/LibrarySymbolManifestLazyImports.md`), not allocator overhead.
  **Long-lived-process memory-growth check** (the most important
  verification per TODO-5234's stop_rule): sampled `/proc/<pid>/status`
  `VmRSS`/`VmHWM` every 15s across a full `PrimeStruct_semantics_tests`
  run (2940 `TEST_CASE`s, ~460s wall-clock) - `VmHWM` went from ~50MB to
  ~56MB over the entire run, essentially flat with no growth trend
  (expected: the test binaries never touch the arena, so behave
  identically to before this work). Verified via
  `./scripts/compile.sh --release` (single invocation, this repo's
  convention): **1881/1881 tests passing, 0 regressions**.
  **One legitimate, small budget-policy update was required** to get
  there: `PrimeStruct_semantic_memory_trend` failed with a "sustained RSS
  regression" for `imported_math_body`/`math_vector`/`math_vector_matrix`
  at the `ast-semantic` phase - real and reproducible (three consecutive
  reruns, same result), not noise. Compared against the last pre-arena
  history report (`semantic_memory_report_..._20260813T130155Z.json`,
  captured earlier the same day after TODO-5230-5232 landed but before
  this work): those fixtures' `ast-semantic` RSS was already ~22.7-23.0MB
  then (the policy's own `soft_max_worst_peak_rss_bytes`, ~24.6-25.4MB,
  had only ~1.5-2MB of headroom left before this work even started), and
  the arena's per-allocation 16-byte header adds a further, modest
  ~2.7MB (~11-12%) specifically for these three fixtures (the ones that
  import stdlib math/vector modules and so make meaningfully more
  allocations than `inline_math_body`/`math_star_repro`/`no_import`,
  which stayed flat and were not flagged) - exactly the kind of small,
  understood, deliberate memory-for-speed cost this design's own
  size-classed free lists are meant to keep bounded rather than
  unbounded. Updated `benchmarks/semantic_memory_budget_policy.json`'s
  three affected `ast-semantic` entries' `baseline`/`soft_max`/`max` to
  the new measured values with a fresh ~13-20% margin (not just raised
  enough to pass - resized to still catch a *further* regression from
  here), rather than leaving a stale pre-TODO-5230 baseline in place or
  silently loosening the check indefinitely.
- 2026-08-13: TODO-5235 (left open, partial progress) + TODO-5236
  (resolved). TODO-5235 built a general escape hatch
  (`primec::SystemHeapScope`/`systemHeapValue()`/
  `registerArenaResetCallback()` in `primec/CompileArena.h`) for the
  magic-static/arena-reset hazard TODO-5234 hit, fixed every magic static
  and thread_local cache found by it, and re-attempted TODO-5234's
  reset-per-`TEST_CASE` design under it. Three consecutive
  fix-rebuild-rerun-the-full-suite rounds each found a *different* class
  or location of the same hazard (see `docs/CompilerArenaAllocator.md`'s
  new "TODO-5235" section for the full account), the last of which
  surfaced more unverified candidates than it resolved - the opposite of
  convergence. Per this leaf's own stop_rule, reverted the reset wiring
  and left `tests/unit/test_main.cpp` on the system allocator exactly as
  TODO-5234 shipped it (no wall-clock change for `semantics`/
  `ir_pipeline`); the escape hatch and every fix found along the way
  remain in place as they're unconditionally safe.
  **TODO-5236** profiled `RequirementPredicateDefinitionContext`
  specifically (the dominant allocation-count site TODO-5233's dhat
  survey found) and found the actual redundancy: `rewriteCompileTimeIfBranches`'s
  `RequirementPredicateDefinitionContext` was being passed **by value**
  into `rewriteCompileTimeIfStatements`/`rewriteCompileTimeIfStatement`,
  which are called once per AST statement/expression node across the
  *entire* recursive tree walk that looks for `ct_if` envelopes anywhere
  in a definition's body - not just where one is found. Since the context's
  contents are read-only for that walk except at the single point a `ct_if`
  actually resolves (where `structNames` needs updating for the selected
  branch's nested statements), the fix was to take `context` by `const&` in
  both functions and make exactly one explicit local copy, only inside the
  `ct_if`-decided branch, right before the mutation that needs it -
  eliminating a full-struct copy (several strings/vectors/hash-sets) per
  AST node system-wide down to one copy per actually-resolved compile-time
  conditional. This is the same "eliminate the redundant work upstream
  instead of caching a moving target" shape TODO-5232's own stop_rule
  required, not a memoization band-aid - the context's field values
  genuinely don't need copying for the vast majority of the tree walk.
  **Measured result** (release build, same machine before/after via `git
  stash` of just this fix, arena from TODO-5234/5235 present and identical
  in both configurations so it isolates this fix's own effect):
  `mini_vec.prime` repro - **allocation count 9,756,990 -> 5,579,238 blocks
  (-42.8%)**, **total bytes allocated 754.9MB -> 376.7MB (-50.1%)**
  (`valgrind --tool=dhat`, matching TODO-5233's exact baseline numbers on
  the "before" build, confirming the repro and methodology line up);
  wall-clock ~0.82-0.89s -> ~0.74-0.77s (**~10-12% faster**). Heavier
  `import /std/collections/*` repro: wall-clock ~0.855-0.867s ->
  ~0.79-0.84s (**~5-8% faster** - a smaller relative share since this
  repro's total compile does proportionally more work outside the
  compile-time-if tree walk). Peak live memory (dhat's `t-gmax`) was
  unchanged (22.96MB in both configurations) since this fix only reduces
  *transient* allocation churn, not the live working set - consistent with
  TODO-5233's own finding that the 754MB-vs-23MB gap was almost entirely
  short-lived churn rather than a large working set. Verified via
  `./scripts/compile.sh --release` (single invocation, this repo's
  convention): **1881/1881 tests passing, 0 regressions** (the same
  pre-existing `docs/todo.md` content-lock staleness pattern TODO-5232/
  5234's own resolution notes already documented showed up during an
  interim run, resolved by this leaf's own `docs/todo.md` update).
- 2026-08-13 (TODO-5237, mimalloc evaluation): Followed up on TODO-5234's
  own open question - was the ~26-33% malloc/free instruction share
  TODO-5233 found "glibc ptmalloc is slow for this workload" or
  "allocations need arena/reset semantics"? Checked this environment for
  a clean way to add a fast general allocator before writing any code, per
  this leaf's own `implementation_notes`: general internet access for
  CMake `FetchContent` is not reliable here (`curl https://github.com`
  through this environment's proxy returns HTTP 400), but system packages
  are (`apt-get install -y libmimalloc-dev` succeeded cleanly, and
  crucially ships its own upstream CMake config package - `find_package
  (mimalloc CONFIG QUIET)` resolves it with zero hand-written vendoring).
  Wired `CMakeLists.txt` with a `PRIMESTRUCT_USE_MIMALLOC` option
  (default `ON`, auto-detected, degrades to the unchanged system
  allocator with just a `STATUS` message on any environment without the
  package) and conditionally linked the resulting `mimalloc` target into
  `primec`/`primevm` only - not the test binaries, matching the TODO's
  scope. Measured all four points of the requested three(-plus)-way
  comparison on the standard `mini_vec.prime`/`heavy_collections.prime`
  repros (release build, `--emit=vm`, same machine, 5 runs each,
  wall-clock via shell `time`): **(a) shipped state** (system-malloc
  fallback + TODO-5234 arena): 0.820s / 0.762s. **(b) system malloc only,
  arena disabled** (allocator-only baseline - built via a temporary
  `src/main.cpp` edit commenting out `ScopedCompileArena`'s construction,
  reverted via `git checkout` before anything was committed): 0.931s /
  0.919s. **(c) mimalloc + arena composed**: **0.772s / 0.746s**. **(c')
  mimalloc alone, arena disabled**: 0.821s / 0.765s.
  **Key finding**: mimalloc alone (c') nearly exactly matches the arena
  alone's (a) own win over system malloc (~12% faster on `mini_vec`, ~17%
  faster on `heavy_collections`, both configurations) despite the two
  mechanisms working completely differently (bump/free-list arena bypassing
  glibc entirely for the hot path, vs. still calling `malloc`/`free` but
  through a faster implementation) - real evidence that a meaningful share
  of the original malloc/free cost genuinely was "the allocator
  implementation," not solely "needs arena/reset semantics." **Composing
  both (c) beats either alone**: ~17-19% faster than system malloc, a
  further ~6% (`mini_vec`) / ~2% (`heavy_collections`) faster than the
  arena alone - expected once traced through `src/CompileArena.cpp`'s own
  code: the arena calls `std::malloc`/`std::free` directly for chunk
  sourcing and its large/over-aligned/cross-thread fallback path, and
  mimalloc's shared-library symbol interposition makes both of those
  mimalloc-backed automatically (no `CompileArena.cpp` source changes
  needed) - the two mechanisms don't compete for the same allocations, so
  they add rather than trade off. **Recommendation** (full reasoning and
  numbers in `docs/CompilerArenaAllocator.md`'s new "TODO-5237" section):
  keep the TODO-5234 arena and *add* mimalloc alongside it - this is what
  shipped. As with TODO-5232/5233/5234 before it, this still falls well
  short of the sub-100ms directional target - the remaining floor is
  still the whole-file text-splicing import architecture
  (`docs/LibrarySymbolManifestLazyImports.md`), unchanged by any allocator
  work. Verified Debug and RelWithDebInfo presets both configure cleanly
  and resolve the same `find_package` path (link-only change, no source
  `#include` of any mimalloc header, so no compile-flag-specific risk).
  Verified via `./scripts/compile.sh --release` (single invocation, this
  repo's convention): **1881/1881 tests passing, 0 regressions**. Two
  interim runs surfaced `native_window_launcher_and_preflight_56_56`
  (`test_compile_run_examples_docs_locks.cpp`'s todo-queue lock case)
  failing - not the usual transient `docs/todo.md`-staleness pattern
  TODO-5232/5234/5236 describe, but a pre-existing bug already present in
  this leaf's starting state: the lock test hard-codes the `### Ready
  Now` bullet list, and the commit that added TODO-5237/5238 to that list
  (before this leaf started) never updated the lock to match. Fixed the
  lock test's hard-coded bullet list and split its single literal `CHECK`
  so it no longer also pins the free-text `Note (...)` paragraph
  `docs/todo.md` keeps between the bullets and the next heading (full
  reasoning in `docs/todo_finished.md`'s TODO-5237 entry). A third clean
  run confirmed `100% tests passed, 0 tests failed out of 1881`.
- 2026-08-14 (TODO-5238, continued allocation-redundancy mining): Re-profiled
  `mini_vec.prime` with `valgrind --tool=dhat` on a build with TODO-5236's fix
  present, to find the next-largest remaining allocation-count contributor.
  The single dominant site was still inside `rewriteCompileTimeIfBranches`
  (several `dhat` entries totaling ~600K+ of the run's 5.66M blocks) - but
  this time one level shallower than TODO-5236's own fix: `makeCompileTimeIfRequirementContext`
  deep-copies `structNames`/`sumNames`/`importAliases` (`unordered_set`/`unordered_map`,
  real node-allocation-per-element churn) plus `candidateFacts.callables`/
  `structFields`/`structTraits` into a fresh `RequirementPredicateDefinitionContext`
  - **once per `Definition` in the whole program**, unconditionally, before
  even looking at that definition's body. Traced every consumer of the
  resulting `context` (TODO-5236 already made the recursive walk take it by
  `const&`) and confirmed it is read in exactly one place:
  `evaluateCompileTimeIfDecision`, called only when a `ct_if` envelope is
  actually found while walking a definition's statements/expressions. The
  entire `/std/collections/*` stdlib module contains **zero** `ct_if` usages
  (confirmed via `grep -rn ct_if stdlib/std/collections/`), so for
  `mini_vec.prime` and `heavy_stdlib.prime` alike, every one of these
  per-definition context builds across the hundreds of spliced-in stdlib
  definitions was pure waste - the context was built, then walked past
  without ever being read, for every single definition.
  **Fix**: added `LazyCompileTimeIfContext` (`src/semantics/SemanticsValidate.cpp`,
  next to `makeCompileTimeIfRequirementContext`) - a small wrapper that
  stores only pointers to the build inputs (`definition`, `structNames`,
  `sumNames`, `importAliases`, `candidateFacts`; no copies) until `.get()`
  is called, which performs the exact same `makeCompileTimeIfRequirementContext`
  build as before, cached for the lifetime of that one wrapper instance.
  Threaded this type through `rewriteCompileTimeIfStatements`/
  `rewriteCompileTimeIfStatement`/`rewriteCompileTimeIfExpression`/
  `evaluateCompileTimeIfDecision` in place of the eager
  `const RequirementPredicateDefinitionContext&`, calling `.get()` only at
  the two `evaluateCompileTimeIfDecision` call sites (the only two places
  that ever read it). The already-resolved-branch path
  (`rewriteCompileTimeIfStatement`'s `updatedContext`, which mutates
  `structNames` for the selected branch's nested statements) uses a second
  constructor that wraps an already-built context directly, so that path's
  cost is unchanged from TODO-5236's fix - this only removes the *eager,
  unconditional* build for definitions that turn out to contain no `ct_if`
  at all, same shape as TODO-5232's stop_rule required ("eliminate the
  redundant work upstream," not caching a moving target - nothing here is
  memoized across different definitions or repeated calls, each definition
  that does need a context still gets exactly one freshly-built one, same
  as before).
  **Correctness argument**: `LazyCompileTimeIfContext::get()` calls the
  exact same `makeCompileTimeIfRequirementContext` function with the exact
  same arguments that were passed eagerly before, and is guaranteed to be
  called before `context` is ever dereferenced (both call sites are inside
  `evaluateCompileTimeIfDecision`, which receives `lazyContext.get()`
  directly, not the wrapper). No control-flow path reads `context`'s fields
  without going through `.get()` first - verified by grepping every use of
  the `context`/`updatedContext` identifiers in the touched functions.
  **Measured result**: isolated clean-checkout before/after binaries (base
  commit `4aabeb8` vs. this fix, both built `-DPRIMESTRUCT_USE_MIMALLOC=OFF`
  to isolate this fix's own effect from TODO-5237's unrelated allocator
  work landing in parallel) via `valgrind --tool=dhat`: `mini_vec.prime` -
  **5,658,083 -> 4,983,933 blocks (-11.9%)**, **380.1MB -> 319.2MB bytes
  (-16.0%)**. `heavy_stdlib.prime` (`/std/collections/*` import, matching
  the `makeVectorHelperSurfaceConformanceSource` heavier-repro shape used
  throughout this chain) - **9,273,056 -> 8,591,848 blocks (-7.3%)**,
  539.1MB -> 477.6MB bytes (-11.4%) - a smaller relative share than
  `mini_vec`, consistent with TODO-5236's own finding that the heavier
  repro spends proportionally more of its total compile outside the
  compile-time-if tree walk this fix touches. `valgrind --tool=callgrind`
  total retired instructions for `mini_vec.prime`: 4,939,848,139 ->
  4,814,403,312 (**-2.5%**). Wall-clock for both repros was **within
  measurement noise** (~0.6-0.9s either way, repeated interleaved runs) -
  expected and worth stating plainly rather than papering over: TODO-5234's
  arena (and now TODO-5237's mimalloc, composed with it) already made
  individual allocations cheap, so a further allocation-*count* reduction
  of this size no longer moves wall-clock measurably, even though the
  underlying redundant work (and its real, measured instruction-count cost)
  is genuinely gone. This mirrors TODO-5232's own honest attribution
  pattern of the remaining floor being elsewhere, just one layer further:
  first "the allocator absorbs it," now "the allocator absorbs it so
  completely that the count itself stops being the wall-clock bottleneck."
  **Round 2 check** (dhat on the post-fix build): re-ran the allocation-site
  breakdown and found the profile is now genuinely diffuse - the former
  single dominant site is gone, and the largest remaining individual sites
  (`envelope_internal::parseIdentifier`/`findNextEnvelopeStart` text
  scanning, `hasExperimentalGfxImportedDefinitionPath`'s per-call string
  copy, `SemanticsValidator::inferQueryExprTypeText`'s string
  concatenation) each account for well under 1% of the remaining 4.98M
  blocks (~15K-35K blocks apiece). Bucketing the full post-fix `mini_vec.prime`
  dhat profile by subsystem: `src/semantics` still dominates at **87.9%** of
  blocks, `parser`/lexer/envelope-splicing at 3.7%, `src/ir_lowerer` at
  4.2% - so per the task's own instruction to let the data decide rather
  than assume, `ir_lowerer` and `parser` were checked and are genuinely not
  where the remaining volume lives for this repro; `semantics` still is,
  just no longer concentrated in one fixable call path. **Stopping here for
  this session**: chasing any of these sub-1%-each sites individually would
  be materially higher effort (each is a different function/subsystem, no
  shared root cause the way `RequirementPredicateDefinitionContext` was
  across two rounds) for a fraction of this round's already-modest
  wall-clock return, and multiplies the surface area for a subtle
  correctness mistake - against the discipline this whole chain has held to.
  This is a documented diminishing-returns judgment call per the task's own
  acceptance criteria, not a silent stop. Verified via
  `./scripts/compile.sh --release` (single invocation, this repo's
  convention, run after TODO-5237's mimalloc-by-default change had already
  landed on this branch): **100% tests passed, 0 tests failed out of 1881**.
- 2026-08-14 (TODO-5239, characterize envelope-parsing/text-transform cost):
  Re-profiled `mini_vec.prime` with `valgrind --tool=callgrind` at the
  post-TODO-5238 baseline (4,770,140,374 retired instructions this session -
  matches the previously-recorded 4,791,446,809 within normal run-to-run
  noise) and found the hot-spot landscape had shifted entirely out of
  `src/semantics` and into PARSING: `primec::envelope_internal::findNextEnvelopeStart`
  (8.72%), `primec::envelope_internal::(anonymous namespace)::parseNamespaceBlock`
  (8.67%), `primec::findMatchingCloseWithComments` (4.32%), plus generic
  string scanning (`memcmp`, `memcpy`) plausibly downstream of the same
  pattern - roughly 20%+ combined. Added temporary debug counters
  (`g_filterPassCalls`/`g_findNextEnvelopeStartCalls`, guarded by a
  `PRIMESTRUCT_TODO5239_DEBUG_COUNTERS` macro, removed before landing) around
  `applyPerEnvelope`'s per-filter driving loop in
  `src/text_filter/TextFilterPipelineEnvelope.cpp` and re-ran `mini_vec.prime`.
  **Result: exactly four identical full-tree passes**, one per default text
  filter (`collections`, `operators`, `implicit-utf8`, `implicit-i32` -
  `include/primec/Options.h`'s `Options::textFilters` default), each
  independently re-walking the same (~279-285KB, post-import-splicing)
  spliced source text from scratch:
  ```
  [TODO-5239] top-level pass for filter='collections'    (input len=278816): applyPerEnvelopeFilterPass calls=1663, findNextEnvelopeStart calls=6048
  [TODO-5239] top-level pass for filter='operators'      (input len=278816): applyPerEnvelopeFilterPass calls=1663, findNextEnvelopeStart calls=6048
  [TODO-5239] top-level pass for filter='implicit-utf8'  (input len=284651): applyPerEnvelopeFilterPass calls=1663, findNextEnvelopeStart calls=6048
  [TODO-5239] top-level pass for filter='implicit-i32'   (input len=284651): applyPerEnvelopeFilterPass calls=1663, findNextEnvelopeStart calls=6048
  ```
  Read every structural-scan function involved
  (`findNextEnvelopeStart`/`parseNamespaceBlock`/`parseDefinitionBlock`/
  `advanceNamespaceScan`/`advanceDefinitionScan`, all in
  `src/text_filter/TextFilterPipelineEnvelopeHelpers.cpp`) and confirmed
  **none of them take a filter-identity parameter or consult which filter is
  active** - envelope/namespace/definition boundary discovery is a pure
  function of the source text and body-depth, entirely independent of which
  of the 4 filters is currently being driven. The recursion-decision logic
  inside `applyPerEnvelopeFilterPass` (`inheritsFilters =
  filtersEqual(envelopeFilters, activeFilters)`) is likewise
  filter-identity-independent, since `activeFilters` (the *full* configured
  list) is passed unchanged on every outer-loop iteration - so the entire
  recursive tree shape (which envelope bodies inherit the active filter set
  vs. divert through explicit-filter recursion) is identical across all 4
  passes too, not just the leaf-level structural scanning. The only
  filter-dependent step is the leaf-chunk text transform
  (`applyFilterToChunk` -> `applyPass`), and `applyPass` itself already
  supports combining *all* enabled filters into a single linear character
  scan (it checks `options.hasFilter("collections")`,
  `options.hasFilter("operators")`, `options.hasFilter("implicit-utf8")`,
  `options.hasFilter("implicit-i32")` independently within one pass) - a
  capability that turned out to be dead code in production, since
  `applyFilterToChunk` always constructed `passOptions.enabledFilters =
  {*filter}` (exactly one filter) and nothing else in the codebase ever
  called `applyPass` with more than one filter enabled at once. **Root
  cause, matching this chain's established "same shape of bug in a
  different subsystem" pattern (TODO-5232, TODO-5236, TODO-5238)**:
  `applyPerEnvelope`'s "apply each configured filter" loop treats *one
  filter* as the unit of *one structural walk*, so N configured filters (4
  by default) means N full top-to-bottom re-walks of the entire spliced
  source text, even though the walk's own control flow never depends on
  which filter is active - a genuinely redundant re-scan, not necessary
  work. This directly informed TODO-5240's fix: fold the "one filter" loop
  down into the (per-leaf-chunk, cheap) text-transform step instead of
  looping the whole structural walk once per filter.
- 2026-08-14 (TODO-5240, fix the envelope-parsing/text-transform
  redundancy): Restructured `src/text_filter/TextFilterPipelineEnvelope.cpp`
  so the envelope/namespace/definition structural walk happens once per
  distinct *filter set* instead of once per individual filter.
  `applyPerEnvelopeFilterPass`'s single `filter` (`std::string`) parameter
  became `filterSequence` (`std::vector<std::string>`), and its leaf-chunk
  call site now goes through a new `applyFiltersToChunk` helper that applies
  every filter in `filterSequence` to that one chunk, in order, threading
  each filter's output into the next filter's input as its input - exactly
  the same per-chunk filter-application order the old code produced by
  re-walking the whole tree once per filter, just computed without
  re-running the (filter-independent) structural scan each time.
  `applyPerEnvelope`'s outer driver changed from a strict "one filter per
  round" loop to an "every not-yet-applied filter per round" loop (tracked
  via the same pre-existing `applied` set), so the pre-existing (rare)
  dynamic filter-discovery mechanism - a leading `[filterName]<...>`
  transform-list annotation at the very start of the file revealing a
  filter not in the originally configured set - still works exactly as
  before: already-applied filters are never reapplied, and only genuinely
  new filters get folded into a further round. **Correctness argument (a
  control-flow restructuring, not a cache - no scanned input is assumed
  stable and reused across calls)**: a leaf chunk's filtered output never
  depends on any other chunk's content (each `applyPass` call is chunk-local
  with no shared/global state), so the final concatenated output is
  invariant under reordering *which chunk* is processed first; the only
  order that can affect correctness is *which filter* is applied first *to
  a given chunk*, and that per-chunk filter order (`activeFilters`' original
  order, e.g. `collections` then `operators` then `implicit-utf8` then
  `implicit-i32`) is completely unchanged by this restructuring - each chunk
  still sees filter[0] applied, then filter[1] applied to filter[0]'s
  output, then filter[2], etc., byte-for-byte identically to before, just
  without 3 other filters' worth of *other chunks* and structural-scan work
  happening in between. The envelope-recursion decision logic itself
  (`inheritsFilters`, `filtersEqual(envelopeFilters, activeFilters)`) was
  already proven filter-identity-independent in TODO-5239's own
  investigation, so recursing with the full `filterSequence` in one
  combined pass instead of one filter per recursive-descent round changes
  nothing about which envelopes inherit the active filter set vs. divert
  through explicit-filter (rule-selected or transform-list-declared)
  recursion. Verified both standard repros produce identical VM execution
  results before and after the fix (`mini_vec.prime` and
  `heavy_collections.prime` both `--emit=vm --entry /main`, no `-o`, so the
  VM actually runs `main` end to end: both exit with code 3 -
  `vectorCount<i32>` of the 3-element `vector<i32>(1i32, 2i32, 3i32)` -
  matching before and after in both configurations).
  **Measured result** (release build, same machine before/after via `git
  stash` of just this fix against this session's starting commit `fe3e5df`,
  mimalloc from TODO-5237 and the arena from TODO-5234 present and identical
  in both configurations so they isolate this fix's own effect;
  `valgrind --tool=callgrind`): `mini_vec.prime` - **4,770,140,374 ->
  3,539,199,913 retired instructions (-25.8%)**; `heavy_collections.prime`
  (`import /std/collections/*`) - **4,650,139,744 -> 3,420,156,943
  (-26.4%)**, a slightly larger relative win, consistent with this fix
  scaling with the amount of spliced-in stdlib source text (more text ->
  proportionally more structural-scan work eliminated per filter no longer
  re-walked). Wall-clock (5 interleaved runs each, `./primec --emit=vm
  <file> --entry /main`, no `-o` so the VM actually executes): `mini_vec.prime`
  **~0.744-0.79s -> ~0.58-0.60s (~22% faster)**; `heavy_collections.prime`
  **~0.717-0.77s -> ~0.57-0.62s (~20% faster)**. `callgrind_annotate`
  confirms the two named hot functions collapsed roughly to the expected
  ~1/4 share (4 passes -> 1 pass): `findNextEnvelopeStart` 8.72% -> 2.88%,
  `parseNamespaceBlock` 8.67% -> 2.86% of total post-fix instructions - the
  redundant re-scan this leaf targeted is gone. The post-fix profile's new
  top contributors (`memcpy`/`operator new`/`memcmp`/string `append`,
  `soa_paths::collectionPath`, `applyPass` itself) are unrelated,
  already-legitimate work spread across the rest of the pipeline, not a new
  concentration this leaf could have also fixed cheaply. **Cumulative chain
  progress** (session start -> now, `mini_vec.prime`, `valgrind --tool=callgrind`
  retired instructions): 13,641,584,142 -> **3,539,199,913 (-74.1% from
  session start)**; wall-clock ~2.0-2.2s -> **~0.58-0.60s**. This remains
  well above the ~14-60ms no-import-baseline-plus-linear-increment
  directional target this chain has carried since TODO-5232 - stated
  honestly per this chain's own precedent, not glossed over. The remaining
  floor is most plausibly the whole-file text-splicing import architecture
  itself (`docs/LibrarySymbolManifestLazyImports.md`, unchanged by any leaf
  in this chain so far), plus whatever costs the post-fix profile's new,
  more-diffuse top contributors represent - a natural target for a further
  round of profiling in a future session, per this chain's own repeated
  "measure, then decide" pattern. Verified via `./scripts/compile.sh
  --release` (single invocation, this repo's convention): **100% tests
  passed, 0 tests failed out of 1881**.
