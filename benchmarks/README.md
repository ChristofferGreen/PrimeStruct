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
