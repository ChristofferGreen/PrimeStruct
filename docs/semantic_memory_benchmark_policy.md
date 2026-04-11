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
- `baseline_worst_wall_seconds`
- `soft_max_worst_peak_rss_bytes`
- `soft_max_worst_wall_seconds`
- `max_worst_peak_rss_bytes`
- `max_worst_wall_seconds`

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
  3-report window for the same `(fixture, phase, metric)` tuple

This sustained rule exists to avoid fail/noise on one-off machine variance while
still catching trend regressions.

## Expensive Test Classification

Semantic-memory benchmark and trend CTest entries are treated as expensive tests
and must stay serial when semantic-memory reports exceed expensive thresholds.

Current expensive thresholds (from benchmark report payload):

- max wall time: `3.0` seconds
- max peak RSS: `524288000` bytes

When `benchmarks/semantic_memory_baseline_report.json` reports one or more
`expensive_offenders` above either threshold, keep:

- `PrimeStruct_semantic_memory_benchmark` as `RUN_SERIAL` + `LABELS "expensive"`
- `PrimeStruct_semantic_memory_trend` as `RUN_SERIAL` + `LABELS "expensive"`

## Update Workflow

When semantic behavior changes intentionally shift memory/runtime:

1. Produce a fresh report with `scripts/semantic_memory_benchmark.py`.
2. Compare report results against current policy entries.
3. Update only the affected `(fixture, phase)` budgets in
   `benchmarks/semantic_memory_budget_policy.json`.
4. Include rationale in the same change that modified semantic behavior.
5. Keep this policy note and benchmark README references aligned.

### Canonical Regeneration Command and Expected Rows

Use this canonical regeneration command when refreshing the benchmark report used
for policy updates:

```bash
python3 scripts/semantic_memory_benchmark.py \
  --repo-root . \
  --primec build-release/primec \
  --runs 3 \
  --phases ast-semantic,semantic-product \
  --fixtures math_star_repro,no_import,math_vector,math_vector_matrix,non_math_large_include,inline_math_body,imported_math_body,scale_1x,scale_2x,scale_4x \
  --report-json build-release/benchmarks/semantic_memory_report.json
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

For phase-one target checks tied to the baseline-derived criteria, use:

- `python3 scripts/check_semantic_memory_phase_one_success.py --criteria benchmarks/semantic_memory_phase_one_success_criteria.json --report build-release/benchmarks/semantic_memory_report.json --history-dir build-release/benchmarks/semantic_memory_history --history-limit 2`
