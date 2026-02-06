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

Run via:
- `./scripts/compile.sh --benchmark`

You can change the number of runs with `BENCH_RUNS` and control the output
folder with `BENCH_DIR`.

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
