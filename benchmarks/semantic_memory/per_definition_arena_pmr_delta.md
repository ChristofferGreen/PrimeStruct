# Per-Definition Arena/PMR Delta (P2-13)

- Date: 2026-04-09
- Fixture: `math_star_repro`
- Phase: `semantic-product`
- Runs: `3`
- Command: `./scripts/semantic_memory_benchmark.sh --fixtures math_star_repro --phases semantic-product --runs 3 --report-json build-release/benchmarks/semantic_memory_p2_13_report.json`

## Baseline vs Current

| Metric | Baseline | Current | Delta |
|---|---:|---:|---:|
| median wall (s) | 5.204086 | 5.850800 | +0.646714 (+12.43%) |
| worst wall (s) | 5.206857 | 5.880637 | +0.673780 (+12.94%) |
| median peak RSS (bytes) | 5926502400 | 3671654400 | -2254848000 (-38.05%) |
| worst peak RSS (bytes) | 5927174144 | 3942252544 | -1984921600 (-33.49%) |

## RSS Delta (MiB)

- median peak RSS: 5651.95 MiB -> 3501.56 MiB (-2150.39 MiB)
- worst peak RSS: 5652.59 MiB -> 3759.62 MiB (-1892.97 MiB)

## Sources

- Baseline report: `benchmarks/semantic_memory_baseline_report.json`
- Current report: `build-release/benchmarks/semantic_memory_p2_13_report.json`
