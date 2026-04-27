# Semantic Memory Benchmark Policy

## Scope

This policy defines the checked-in regression budget contract for semantic
memory benchmarking (`scripts/semantic_memory_benchmark.py`).

Canonical policy file:

- `benchmarks/semantic_memory_budget_policy.json`

Canonical baseline source:

- `benchmarks/semantic_memory_baseline_report.json`

## Budget Model

Budgets are defined per `(fixture, phase)` pair where phase is one of:

- `ast-semantic`
- `semantic-product`

Each policy entry contains:

- `baseline_worst_peak_rss_bytes`
- `soft_max_worst_peak_rss_bytes`
- `max_worst_peak_rss_bytes`

Interpretation:

- `max_*` values are hard caps for single-run gating.
- `soft_max_*` values are early-warning sustained-regression thresholds.
- Budget coverage is strict: every `(fixture, phase)` row emitted in a report
  must have a matching policy entry.

## Sustained-Window Rule

Policy-level sustained-window settings are kept in:

- `sustained_window.window_size`
- `sustained_window.minimum_regressions`

Current rule:

- Window size: `3` reports
- Sustained regression trigger: `2` or more soft-threshold regressions in that
  3-report window for the same `(fixture, phase)` RSS metric

This sustained rule exists to avoid fail/noise on one-off machine variance while
still catching trend regressions.

## Parallel Benchmark Targets

Semantic-memory benchmark and trend targets run in parallel with the wider test
suite. Budget policy gating is RSS-only so concurrency noise does not trip
runtime-based false regressions.

## Update Workflow

When semantic behavior changes intentionally shift memory usage:

1. Produce a fresh report with `scripts/semantic_memory_benchmark.py`.
2. Compare report results against current policy entries.
3. Update only the affected `(fixture, phase)` budgets in
   `benchmarks/semantic_memory_budget_policy.json`.
4. Include rationale in the same change that modified semantic behavior.
5. Keep this policy note and benchmark README references aligned.

### Current Budget Notes

- `math_vector:ast-semantic` keeps a hard cap of `22140518` bytes after the
  April 26, 2026 release artifact observed `21086208` bytes on the release
  runner. The cap preserves roughly 5% headroom over that observed value while
  leaving the baseline row unchanged for future comparison.
- `imported_math_body:ast-semantic` keeps a hard cap of `21108326` bytes after
  the April 27, 2026 release artifact observed `20103168` bytes on the release
  runner. The cap preserves roughly 5% headroom over that observed value while
  leaving the baseline row unchanged for future comparison.
- `math_vector_matrix:ast-semantic` keeps a hard cap of `20953497` bytes after
  the April 27, 2026 release artifact observed `19955712` bytes on the release
  runner. The cap preserves roughly 5% headroom over that observed value while
  leaving the baseline row unchanged for future comparison.

### Canonical Regeneration Command and Expected Rows

Use this canonical regeneration command when refreshing the benchmark report used
for policy updates:

```bash
python3 scripts/semantic_memory_benchmark.py \
  --repo-root . \
  --primec build-release/primec \
  --runs 3 \
  --definition-validation-workers both \
  --phases ast-semantic,semantic-product \
  --fixtures math_star_repro,no_import,math_vector,math_vector_matrix,non_math_large_include,inline_math_body,imported_math_body,scale_1x,scale_2x,scale_4x \
  --report-json build-release/benchmarks/semantic_memory_report.json
```

This mode records worker-mode deltas and fails when one-worker and two-worker
runs emit different dump payload hashes for the same `(fixture, phase, mode)`
tuple.

## Unified Semantic-Product Index Evidence

Current checked-in evidence for the unified lowerer `SemanticProductIndex`
builder is split across the baseline report and the worker-parity benchmark
mode:

- `benchmarks/semantic_memory_baseline_report.json` now contains both
  definition-validation worker rows across all `10` fixtures and both semantic
  phases (`40` total rows), and all `20` `semantic-product` rows carry both
  `key_cardinality` metrics and `semantic_product_index_family_counts`.
- The refreshed baseline report also carries matching
  `definition_validation_worker_mode_deltas[*].semantic_product_index_family_counts_*`
  fields for the unified semantic-product adapter path.
- `benchmarks/semantic_memory/semantic_product_index_parity_evidence.json`
  is the paired checked-in evidence artifact that locks those family-count and
  worker-parity field shapes alongside the canonical baseline refresh.
- `benchmarks/semantic_memory/semantic_product_index_math_star_repro_report.json`
  is the checked-in measured report for the primary `math_star_repro`
  `semantic-product` fixture, generated with
  `--definition-validation-workers both` so the worker-parity family-count
  payload also exists in a focused one-fixture artifact.
- Use those family-count fields to confirm the direct-call, method-call,
  bridge-path, binding, return, local-auto, query, try, and `on_error` index
  families remain populated and worker-parity-identical while comparing RSS and
  wall-time deltas.

The checked-in primary-fixture measured artifact was generated with:

```bash
python3 scripts/semantic_memory_benchmark.py --repo-root . \
  --primec build-release/primec --runs 3 --fixtures math_star_repro \
  --phases semantic-product --definition-validation-workers both \
  --report-json benchmarks/semantic_memory/semantic_product_index_math_star_repro_report.json
```

The CI artifact wrapper forwards this mode via:

```bash
python3 scripts/semantic_memory_ci_artifacts.py --mode benchmark \
  --benchmark-definition-validation-workers both ...
```

Expected `(fixture, phase)` report rows for that canonical run:

| Fixture | Phase |
| --- | --- |
| `math_star_repro` | `ast-semantic` |
| `math_star_repro` | `semantic-product` |
| `no_import` | `ast-semantic` |
| `no_import` | `semantic-product` |
| `math_vector` | `ast-semantic` |
| `math_vector` | `semantic-product` |
| `math_vector_matrix` | `ast-semantic` |
| `math_vector_matrix` | `semantic-product` |
| `non_math_large_include` | `ast-semantic` |
| `non_math_large_include` | `semantic-product` |
| `inline_math_body` | `ast-semantic` |
| `inline_math_body` | `semantic-product` |
| `imported_math_body` | `ast-semantic` |
| `imported_math_body` | `semantic-product` |
| `scale_1x` | `ast-semantic` |
| `scale_1x` | `semantic-product` |
| `scale_2x` | `ast-semantic` |
| `scale_2x` | `semantic-product` |
| `scale_4x` | `ast-semantic` |
| `scale_4x` | `semantic-product` |

## Validation Tooling

The policy can be checked locally with:

- `python3 scripts/check_semantic_memory_budget.py --policy benchmarks/semantic_memory_budget_policy.json --report build-release/benchmarks/semantic_memory_report.json`

Optional sustained-window input can be provided by adding one or more prior
reports through repeated `--history-report <path>` flags.

For CI trend checks that discover prior reports from a directory, use:

- `python3 scripts/check_semantic_memory_trend.py --policy benchmarks/semantic_memory_budget_policy.json --report build-release/benchmarks/semantic_memory_report.json --history-dir build-release/benchmarks/semantic_memory_history --history-limit 2`
- Add `--trend-report-json <path>` when CI needs a machine-readable trend
  summary that includes selected history inputs and checker status.

For CI artifact capture across benchmark + trend runs (including failures), use:

- `python3 scripts/semantic_memory_ci_artifacts.py --mode full --repo-root . --primec build-release/primec --benchmark-report build-release/benchmarks/semantic_memory_report.json --budget-report build-release/benchmarks/semantic_memory_budget_check_report.json --trend-report build-release/benchmarks/semantic_memory_trend_report.json --history-dir build-release/benchmarks/semantic_memory_history --artifacts-dir build-release/benchmarks/semantic_memory_artifacts`

Benchmark-only CI runs now enforce policy checks by default:

- `python3 scripts/semantic_memory_ci_artifacts.py --mode benchmark ...`

This mode runs benchmark collection and then runs the trend/budget checker against
the generated report and discovered history. Use
`--skip-budget-check-in-benchmark` for collector-only runs that intentionally
separate artifact capture or parity checks from the sustained RSS budget gate.

The release CMake/CTest collector targets intentionally use that skip flag:

- `PrimeStruct_semantic_memory_benchmark` records the fresh benchmark report,
  artifact bundle, and history input for the current build.
- `PrimeStruct_semantic_memory_definition_worker_parity` records the
  worker-mode parity report/artifact bundle and still fails benchmark
  collection on dump-hash mismatches, but it does not also own the RSS budget
  gate.
- `PrimeStruct_semantic_memory_trend` depends on that collector target and owns
  the sustained policy gate for the normal release path.

For phase-one target checks tied to the baseline-derived criteria, use:

- `python3 scripts/check_semantic_memory_phase_one_success.py --criteria benchmarks/semantic_memory_phase_one_success_criteria.json --report build-release/benchmarks/semantic_memory_report.json --history-dir build-release/benchmarks/semantic_memory_history --history-limit 2`
