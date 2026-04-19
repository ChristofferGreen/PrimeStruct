# Benchmarks

## Aggregate (sum + sum of squares)

This benchmark computes the sum and sum of squares for the integer range
`[0, N)`. It is a simple, industry-standard style aggregation workload
(similar to basic analytics/ETL reductions) that exercises integer math,
looping, and mutable state.

Files:
- `benchmarks/aggregate.c`
- `benchmarks/aggregate.cpp`
- `benchmarks/aggregate.rs`
- `benchmarks/aggregate.prime`

Default N is 5,000,000 in all implementations.

Run after a release build via:
- `./scripts/compile.sh --release`
- `./scripts/benchmark.sh --build-dir ./build-release`
- `./scripts/benchmark.sh --build-dir ./build-release --report-json ./build-release/benchmarks/benchmark_report.json --baseline-json ./benchmarks/benchmark_baseline.json`

You can change the number of runs with `BENCH_RUNS` and control the output
folder with `BENCH_DIR`.

PrimeStruct benchmarks report two backends:
- `primestruct_cpp`: C++ emitter (`--emit=cpp`) compiled with `clang++ -O3`.
- `primestruct_native`: native codegen backend (`--emit=native`) when available.

## JSON Scan (token counting)

This benchmark scans a fixed JSON payload encoded as ASCII bytes and counts
structural tokens (`{ } [ ] : , "`). It is a lightweight proxy for JSON
tokenization that stays within the native PrimeStruct subset (integer math,
loops, arrays).

Files:
- `benchmarks/json_scan.c`
- `benchmarks/json_scan.cpp`
- `benchmarks/json_scan.rs`
- `benchmarks/json_scan.prime`

The payload is scanned 20,000 times in each implementation.

## JSON Parse (strings + integer parsing)

This benchmark performs a simple streaming JSON parse over a fixed payload:

- Skips strings correctly (handles `\\` escapes and `\\uXXXX` sequences).
- Parses signed base-10 integers and accumulates a checksum.
- Counts structural tokens (`{ } [ ] : ,`) outside of strings.

It is still small enough to keep implementations comparable, but is closer to a
real tokenizer/parser workload than `json_scan`.

Files:
- `benchmarks/json_parse.c`
- `benchmarks/json_parse.cpp`
- `benchmarks/json_parse.rs`
- `benchmarks/json_parse.prime`

## Compile Speed (large source)

This benchmark generates a large PrimeStruct source file (default 100,000 lines)
and measures how long `primec` takes to compile it.

Targets:
- `c`: `cc -O3` compile of the generated C source.
- `cpp`: `c++ -O3` compile of the generated C++ source.
- `rust`: `rustc -O` compile of the generated Rust source.
- `primestruct_cpp`: C++ emitter (`--emit=cpp`) output only.
- `primestruct_native`: native codegen backend (`--emit=native`) when available.

Controls:
- `BENCH_COMPILE_LINES` sets the source line count.
- `BENCH_COMPILE_RUNS` sets the number of timed runs (defaults to `BENCH_RUNS`).

The generated source is written to the benchmark output directory.

## Regression Harness

The benchmark runner can emit a deterministic report artifact:

- `./scripts/benchmark.sh --build-dir build-release --report-json build-release/benchmarks/benchmark_report.json`

To enforce baseline regression checks (used by CI), run:

- `./scripts/benchmark.sh --build-dir build-release --report-json build-release/benchmarks/benchmark_report.json --baseline-json benchmarks/benchmark_baseline.json`

Threshold checking is handled by:

- `scripts/check_benchmark_report.py --baseline benchmarks/benchmark_baseline.json --report build-release/benchmarks/benchmark_report.json`

Baseline thresholds are stored in:

- `benchmarks/benchmark_baseline.json`

## Semantic Memory Benchmark

Semantic memory benchmark fixtures live under:

- `benchmarks/semantic_memory/fixtures/`
- `math_star_repro.prime` is the primary high-RSS regression target: a
  minimized checked-in `/std/math/*` reproducer used for budget and phase-one
  success checks.
- `no_import.prime` is the baseline no-import fixture used for semantic-memory
  attribution runs.
- `math_vector.prime` uses explicit vector-surface imports (`/std/math/Vec2`).
- `math_vector_matrix.prime` uses explicit vector+matrix imports (`/std/math/Vec2 /std/math/Mat2`).
- `math_vector_matrix.prime` and `math_star_repro.prime` are paired
  attribution fixtures for vector+matrix explicit imports versus `/std/math/*`
  star imports.
- `non_math_large_include.prime` imports `/std/bench_non_math/*`, a non-math
  benchmark include surface that pulls `/std/collections/*` + `/std/file/*`
  and keeps top-level AST definition count comparable to `math_star_repro`.
- `inline_math_body.prime` and `imported_math_body.prime` are paired fixtures
  with identical non-import body text; the imported variant adds
  `/std/math/Vec2` to isolate stdlib-import overhead from body-shape effects.
- `scale_1x.prime`, `scale_2x.prime`, and `scale_4x.prime` are the scale-step
  attribution fixtures; they grow the same linear call-chain shape at `1x`,
  `2x`, and `4x` sizes for RSS/time slope reporting.
- The benchmark harness runs fixtures across both semantic phases by default:
  `ast-semantic` and `semantic-product`.

Run the semantic memory harness after building `primec`:

- `./scripts/semantic_memory_benchmark.sh`
- `python3 scripts/semantic_memory_benchmark.py --repo-root . --primec build-release/primec --runs 3 --report-json build-release/benchmarks/semantic_memory_report.json`
- `python3 scripts/semantic_memory_benchmark.py --repo-root . --primec build-release/primec --runs 3 --semantic-product-force on --report-json build-release/benchmarks/semantic_memory_force_on_report.json`
- `python3 scripts/semantic_memory_benchmark.py --repo-root . --primec build-release/primec --runs 3 --phases ast-semantic --semantic-product-force both --semantic-phase-counters --report-json build-release/benchmarks/semantic_memory_force_ab_report.json`
- `python3 scripts/semantic_memory_benchmark.py --repo-root . --primec build-release/primec --runs 3 --semantic-validation-without-fact-emission on --report-json build-release/benchmarks/semantic_memory_no_facts_report.json`
- `python3 scripts/semantic_memory_benchmark.py --repo-root . --primec build-release/primec --runs 3 --semantic-validation-without-fact-emission both --report-json build-release/benchmarks/semantic_memory_validation_vs_facts_ab_report.json`
- `python3 scripts/semantic_memory_benchmark.py --repo-root . --primec build-release/primec --runs 3 --fact-families callable_summaries --report-json build-release/benchmarks/semantic_memory_callable_only_report.json`

The semantic memory report schema is:

- `primestruct_semantic_memory_report_v1`

This report captures:

- Per-fixture wall time and peak RSS for both `ast-semantic` and `semantic-product` dump stages.
- Median and worst-case wall/RSS across 3 runs.
- Semantic-product key-cardinality metrics (distinct direct/method target keys and max key length).
- `1x/2x/4x` scale-fixture slope estimates for RSS/time.
- Threshold annotations (`3s` runtime, `500 MiB` peak RSS): per-row flags plus an `expensive_offenders` summary.

Benchmark-only collector controls are forwarded to `primec`:

- `--semantic-product-force auto|on|off|both` maps to compile-pipeline semantic-product gate override; `both` emits `semantic_product_force_deltas` for `ast-semantic` A/B runs.
- `--semantic-validation-without-fact-emission off|on|both` controls validation-only runs and emits validator-vs-fact A/B deltas in report output.
- `--no-fact-emission` remains as a compatibility alias for `--semantic-validation-without-fact-emission on`.
- `--fact-families <csv>` allows only the specified semantic collector families.

Initial checked-in baseline report:

- `benchmarks/semantic_memory_baseline_report.json`
  now records the full canonical fixture matrix with
  `--definition-validation-workers both`, so semantic-product rows carry
  `semantic_product_index_family_counts` and the report includes
  `definition_validation_worker_mode_deltas`.
- `benchmarks/semantic_memory_budget_policy.json` defines per-fixture per-phase
  soft/hard RSS budgets plus the sustained-window rule (`2` regressions in
  a `3`-report window).
- `docs/semantic_memory_benchmark_policy.md` documents how to update those
  budgets safely.
- `benchmarks/semantic_memory/semantic_product_index_parity_evidence.json`
  locks the checked-in semantic-product index-family count and
  definition-worker parity field shape until the canonical baseline report is
  refreshed with `--definition-validation-workers both`.
- `benchmarks/semantic_memory/semantic_product_index_math_star_repro_report.json`
  is the checked-in measured worker-parity report for the primary
  `math_star_repro` `semantic-product` fixture, generated with
  `--definition-validation-workers both` to record real
  `semantic_product_index_family_counts` and
  `definition_validation_worker_mode_deltas` payloads.
- `benchmarks/semantic_memory_phase_one_success_criteria.json` defines the
  phase-one success target tied to the primary `/std/math/*` fixture
  (`math_star_repro`, `semantic-product`) using a reduction-vs-cap rule.
- `docs/semantic_memory_phase_one_success_criteria.md` explains that phase-one
  target and its sustained-window pass condition.
- `benchmarks/semantic_memory/method_target_memoization_delta.md` records the
  `P2-10` method-target memoization RSS/time delta on `math_star_repro`
  (`semantic-product`, 3 runs) against that baseline.
- `benchmarks/semantic_memory/graph_local_auto_structured_keys_delta.md`
  records the `P2-11` graph-local-auto structured-key RSS/time delta on
  `math_star_repro` (`semantic-product`, 3 runs) against that baseline.
- `benchmarks/semantic_memory/normalized_method_name_cache_flattening_delta.md`
  records the `P2-12` normalized-method-name cache flattening RSS/time delta
  on `math_star_repro` (`semantic-product`, 3 runs) against that baseline.
- `benchmarks/semantic_memory/per_definition_arena_pmr_delta.md` records the
  `P2-13` per-definition arena/PMR scratch-allocation RSS/time delta on
  `math_star_repro` (`semantic-product`, 3 runs) against that baseline.

Check semantic memory report budgets against policy:

- `python3 scripts/check_semantic_memory_budget.py --policy benchmarks/semantic_memory_budget_policy.json --report build-release/benchmarks/semantic_memory_report.json`
- `python3 scripts/check_semantic_memory_budget.py --policy benchmarks/semantic_memory_budget_policy.json --report build-release/benchmarks/semantic_memory_report.json --history-report <older1.json> --history-report <older2.json>`

Trend helper for CI/history-driven sustained checks:

- `python3 scripts/check_semantic_memory_trend.py --policy benchmarks/semantic_memory_budget_policy.json --report build-release/benchmarks/semantic_memory_report.json --history-dir build-release/benchmarks/semantic_memory_history --history-limit 2`
- Add `--trend-report-json <path>` to emit a machine-readable trend summary that
  records selected history reports and checker pass/fail status.

Phase-one success checker:

- `python3 scripts/check_semantic_memory_phase_one_success.py --criteria benchmarks/semantic_memory_phase_one_success_criteria.json --report build-release/benchmarks/semantic_memory_report.json --history-dir build-release/benchmarks/semantic_memory_history --history-limit 2`
- Add `--report-json <path>` to emit a machine-readable phase-one check summary.

CI artifact wrapper (always emits run manifest/log artifacts, including failures):

- `python3 scripts/semantic_memory_ci_artifacts.py --mode full --repo-root . --primec build-release/primec --benchmark-report build-release/benchmarks/semantic_memory_report.json --budget-report build-release/benchmarks/semantic_memory_budget_check_report.json --trend-report build-release/benchmarks/semantic_memory_trend_report.json --history-dir build-release/benchmarks/semantic_memory_history --artifacts-dir build-release/benchmarks/semantic_memory_artifacts`
- In `benchmark`/`full` modes, the wrapper clears prior report outputs first so
  failed runs cannot accidentally re-publish stale benchmark/budget JSON files
  from earlier successful runs.

CTest/CMake gates:

- `PrimeStruct_semantic_memory_trend` (depends on `PrimeStruct_semantic_memory_benchmark`)
- `cmake --build build-release --target primestruct_semantic_memory_trend_check`
- `PrimeStruct_semantic_memory_benchmark` runs the wrapper in collector-only mode (`--skip-budget-check-in-benchmark`) so the release path always captures a fresh report/history/artifact bundle before policy evaluation.
- `PrimeStruct_semantic_memory_trend` runs the sustained budget gate against that collected report/history set, so release-gate failures are attributed to the policy step instead of the collector step.
- Both targets still run through `scripts/semantic_memory_ci_artifacts.py`, so machine-readable report bundles are available in `build-release/benchmarks/semantic_memory_artifacts/` even when one step fails.

## Type-Graph Budget Gates

Type-resolution graph budget checks are enforced from a committed baseline file:

- `benchmarks/type_graph_budget_baseline.json`

Run the local checker directly after building `primec`:

- `python3 scripts/check_graph_budget.py --primec build-release/primec --repo-root . --baseline benchmarks/type_graph_budget_baseline.json`

Run it as a build target:

- `cmake --build build-release --target primestruct_graph_budget_check`

CTest includes a default gate named `PrimeStruct_graph_budget`, so CI/`ctest` runs
fail when graph budget thresholds regress.

If a graph change intentionally shifts budgets, revise the baseline with this flow:

1. Run the checker with a report artifact:
   - `python3 scripts/check_graph_budget.py --primec build-release/primec --repo-root . --baseline benchmarks/type_graph_budget_baseline.json --report-json build-release/graph_budget_report.json`
2. Compare `build-release/graph_budget_report.json` against the current baseline.
3. Update only the needed thresholds in `benchmarks/type_graph_budget_baseline.json`.
4. Include the baseline update and rationale in the same commit as the graph change.
