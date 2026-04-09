# Semantic Memory Phase-One Success Criteria

This note defines the phase-one memory target tied to the checked-in semantic
memory baseline.

Canonical machine-readable criteria:

- `benchmarks/semantic_memory_phase_one_success_criteria.json`

## Primary Goal

Target the primary high-RSS fixture:

- Fixture: `math_star_repro`
- Phase: `semantic-product`
- Metric: `worst_peak_rss_bytes`

Baseline value (from `benchmarks/semantic_memory_baseline_report.json`):

- `5927174144` bytes

Pass criterion:

- Reduce by at least `10%` versus baseline, or satisfy an absolute cap of
  `5 GiB`, whichever is stricter.
- Effective threshold from current baseline:
  `min(5927174144 * 0.9, 5368709120) = 5334456730` bytes.

## Sustained Rule

The phase-one goal is considered achieved when the primary criterion passes in
at least `2` of the most recent `3` semantic-memory benchmark reports.

This sustained window aligns with the policy rule in
`benchmarks/semantic_memory_budget_policy.json`.

## Validation Tooling

Check current + sustained phase-one success against criteria:

- `python3 scripts/check_semantic_memory_phase_one_success.py --criteria benchmarks/semantic_memory_phase_one_success_criteria.json --report build-release/benchmarks/semantic_memory_report.json --history-dir build-release/benchmarks/semantic_memory_history --history-limit 2`

Optional machine-readable checker summary:

- add `--report-json build-release/benchmarks/semantic_memory_phase_one_check_report.json`
