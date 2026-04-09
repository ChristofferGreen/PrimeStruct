# Graph Local-Auto Structured Key Delta (P2-11)

- Date: 2026-04-09
- Fixture: `math_star_repro`
- Phase: `semantic-product`
- Runs: `3`
- Command: `./scripts/semantic_memory_benchmark.sh --fixtures math_star_repro --phases semantic-product --runs 3 --report-json build-release/benchmarks/semantic_memory_p2_11_report.json`

## Baseline vs Current

| Metric | Baseline | Current | Delta |
|---|---:|---:|---:|
| median wall (s) | 5.204086 | 5.999976 | +0.795890 (+15.29%) |
| worst wall (s) | 5.206857 | 6.164138 | +0.957281 (+18.39%) |
| median peak RSS (bytes) | 5926502400 | 3494428672 | -2432073728 (-41.04%) |
| worst peak RSS (bytes) | 5927174144 | 3607527424 | -2319646720 (-39.14%) |

## RSS Delta (MiB)

- median peak RSS: 5651.95 MiB -> 3332.55 MiB (-2319.41 MiB)
- worst peak RSS: 5652.59 MiB -> 3440.41 MiB (-2212.19 MiB)

## Sources

- Baseline report: `benchmarks/semantic_memory_baseline_report.json`
- Current report: `build-release/benchmarks/semantic_memory_p2_11_report.json`
