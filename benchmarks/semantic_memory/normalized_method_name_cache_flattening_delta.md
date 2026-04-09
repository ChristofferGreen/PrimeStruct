# Normalized Method-Name Cache Flattening Delta (P2-12)

- Date: 2026-04-09
- Fixture: `math_star_repro`
- Phase: `semantic-product`
- Runs: `3`
- Command: `./scripts/semantic_memory_benchmark.sh --fixtures math_star_repro --phases semantic-product --runs 3 --report-json build-release/benchmarks/semantic_memory_p2_12_report.json`

## Baseline vs Current

| Metric | Baseline | Current | Delta |
|---|---:|---:|---:|
| median wall (s) | 5.204086 | 5.996594 | +0.792508 (+15.23%) |
| worst wall (s) | 5.206857 | 6.204024 | +0.997168 (+19.15%) |
| median peak RSS (bytes) | 5926502400 | 2919170048 | -3007332352 (-50.74%) |
| worst peak RSS (bytes) | 5927174144 | 3206725632 | -2720448512 (-45.90%) |

## RSS Delta (MiB)

- median peak RSS: 5651.95 MiB -> 2783.94 MiB (-2868.02 MiB)
- worst peak RSS: 5652.59 MiB -> 3058.17 MiB (-2594.42 MiB)

## Sources

- Baseline report: `benchmarks/semantic_memory_baseline_report.json`
- Current report: `build-release/benchmarks/semantic_memory_p2_12_report.json`
