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

## Update Workflow

When semantic behavior changes intentionally shift memory/runtime:

1. Produce a fresh report with `scripts/semantic_memory_benchmark.py`.
2. Compare report results against current policy entries.
3. Update only the affected `(fixture, phase)` budgets in
   `benchmarks/semantic_memory_budget_policy.json`.
4. Include rationale in the same change that modified semantic behavior.
5. Keep this policy note and benchmark README references aligned.

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
