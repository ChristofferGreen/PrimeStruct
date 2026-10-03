# Benchmark re-baseline, 2026-10

Run: `./scripts/benchmark.sh --build-dir "$PWD/build-release" --report-json build-release/benchmarks/benchmark_report.json --baseline-json benchmarks/benchmark_baseline.json`
(absolute paths are required; a relative `--build-dir` breaks the compile-speed step).

| entry | before fixes | after fixes | baseline limit |
| --- | --- | --- | --- |
| aggregate primestruct_cpp (runtime) | 1.67s | 1.70s | within baseline |
| compile_speed primestruct_cpp (100k lines) | > 25 min (killed) | 11.2s | 12.0s |

The compile-speed regression was real: semantic validation had picked up several
quadratic loops in the definition count. Fixed in this change:

- `validateRequirementPredicates` built a full fact context for definitions with no
  `require`/`restrict` transform.
- Four linear scans over `program_.definitions` (return inference, struct return
  inference) now use a sorted path index (`definitionsMatchingPathOrSpecialization`).
- Local-aware call refinement erased from the shared collector vectors once per call;
  replacements are now batched.
- AST-hook alias collection walked every definition per definition.

Synthetic scaling (N empty functions, `--emit=cpp`): 8000 functions 38s -> 2.5s,
16000 functions 6.2s. Scaling is still mildly superlinear; the remaining cost is spread
across call inference rather than one hot spot. The baseline limits were left unchanged
because the fixed run passes them; the 11.2s result is close to the 12.0s limit, so a
future change should look at the remaining inference cost before raising it.
