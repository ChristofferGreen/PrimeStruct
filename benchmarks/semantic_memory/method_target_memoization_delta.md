# Method-Target Memoization Delta (P2-10)

- Date: 2026-04-09
- Fixture: `math_star_repro`
- Phase: `semantic-product`
- Runs: `3`
- Command: `./scripts/semantic_memory_benchmark.sh --fixtures math_star_repro --phases semantic-product --runs 3 --report-json build-release/benchmarks/semantic_memory_p2_10_report.json`

## Baseline vs Current

| Metric | Baseline | Current | Delta |
|---|---:|---:|---:|
| median wall (s) | 5.204086 | 6.178043 | +0.973957 (+18.72%) |
| worst wall (s) | 5.206857 | 6.211852 | +1.004995 (+19.30%) |
| median peak RSS (bytes) | 5926502400 | 3363045376 | -2563457024 (-43.25%) |
| worst peak RSS (bytes) | 5927174144 | 3472621568 | -2454552576 (-41.41%) |

## RSS Delta (MiB)

- median peak RSS: 5651.95 MiB -> 3207.25 MiB (-2444.70 MiB)
- worst peak RSS: 5652.59 MiB -> 3311.75 MiB (-2340.84 MiB)

## Sources

- Baseline report: `benchmarks/semantic_memory_baseline_report.json`
- Current report: `build-release/benchmarks/semantic_memory_p2_10_report.json`
