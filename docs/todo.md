# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started

Finished items are periodically moved to `docs/todo_finished.md`.

**Types & Semantics**
Roadmap note: completed map/vector migration, compatibility-cleanup, graph queue, and semantics/lowering boundary slices now live in `docs/todo_finished.md`.
Sizing note: each active leaf must fit in one code-affecting commit plus focused conformance updates.

**TODO Quality Bar (April 13, 2026)**
- Every active leaf must deliver one of:
  - user-visible behavior change,
  - measurable perf/memory improvement, or
  - deletion of a real compatibility subsystem.
- Every leaf must include explicit acceptance criteria and a stop rule.
- Avoid standalone micro-cleanups (alias renames, trivial bool rewrites, local dedup) unless bundled into a value outcome above.
- If a leaf misses its value target after 2 attempts, archive it as low-value and replace it with a different hotspot.
- Keep the active queue short: no more than 8 live leaves at once.

**Group 14 - SoA bring-up and end-state cleanup**
Goal: finish stdlib-authoritative `soa_vector` behavior and delete compatibility scaffolding.

- ○ [S4-01a] Enforce canonical helper authority for read helpers (`count`, `get`, `get_ref`).
  - Value: deterministic helper routing for the most-used read paths.
  - Acceptance criteria:
    1. Canonical `/std/collections/soa_vector/count|get|get_ref` paths pass semantics/IR/compile-run coverage.
    2. Old-surface `/soa_vector/count|get|get_ref` calls without compatibility import fail through one deterministic diagnostic family.
    3. Tests cover canonical success + old-surface reject for all three helpers.
  - Stop rule: if one commit cannot cover all three helpers, split into `{count}` then `{get,get_ref}`.

- ○ [S4-01b] Enforce canonical helper authority for borrowed helpers (`ref`, `ref_ref`).
  - Value: removes borrowed-helper ambiguity in fallback and diagnostics.
  - Acceptance criteria:
    1. Canonical `/std/collections/soa_vector/ref|ref_ref` paths pass semantics/IR/compile-run coverage.
    2. Old-surface `/soa_vector/ref|ref_ref` calls without compatibility import fail deterministically.
    3. Tests cover call-form and method-form receiver paths.
  - Stop rule: split into `{ref}` then `{ref_ref}` if needed.

- ○ [S4-01c] Enforce canonical helper authority for conversion helpers (`to_aos`, `to_aos_ref`).
  - Value: conversion behavior is canonical-only and predictable.
  - Acceptance criteria:
    1. Canonical `/std/collections/soa_vector/to_aos|to_aos_ref` paths pass semantics/IR/compile-run coverage.
    2. Old-surface conversion calls without compatibility import fail deterministically.
    3. Tests cover borrowed and non-borrowed receiver forms.
  - Stop rule: split into `{to_aos}` then `{to_aos_ref}` if needed.

- ◐ [S4-02] Remove canonical conversion fallback dependence on experimental conversion helpers.
  - Value: deletes duplicate substrate and fallback churn.
  - Acceptance criteria:
    1. Canonical conversion lowering has no dependency on `/std/collections/experimental_soa_vector_conversions/*`.
    2. Source-lock coverage asserts removed fallback wiring.
    3. Existing canonical conversion compile-run tests remain green.
  - Progress: experimental SoA method `to_aos` rewrite fallback in
    `SemanticsValidate.cpp` now rewrites to canonical
    `/std/collections/soa_vector/to_aos` instead of
    `/std/collections/experimental_soa_vector_conversions/soaVectorToAos`, and
    the same rewrite pass no longer checks
    `isExperimentalSoaVectorConversionFamilyPath(def.fullPath)` in its
    definition-skip gate (removing the last conversion-family dependency from
    canonical rewrite selection in that lowering stage).
  - Stop rule: if full family removal is too broad, remove one canonical conversion path end-to-end first.

- ○ [S4-03] Delete one full compiler-owned SoA compatibility island end-to-end.
  - Value: concrete maintenance-surface reduction.
  - Acceptance criteria:
    1. Remove one compatibility island (code path, tests, docs references).
    2. Add focused regression coverage proving canonical behavior is unchanged.
    3. Archive the deleted island in `docs/todo_finished.md` with affected files.
  - Stop rule: no valid completion if net behavior-deletion is zero.

S2 micro-slices `[S2-01]` through `[S2-05]` and S3 micro-slices `[S3-01]` through `[S3-132]` are archived in `docs/todo_finished.md` (April 12-13, 2026).

**Group 15 - Semantic memory footprint and multithread compile substrate**
Goal: ship measurable wins with hard regression control.

- ◐ [P1-01] Enforce benchmark regression gates against checked-in budget policy.
  - Value: prevents silent memory/runtime regressions.
  - Acceptance criteria:
    1. Checker fails on budget breach and reports offending fixture/phase.
    2. Policy doc defines pass/fail and exception flow.
    3. Tests lock checker pass/fail behavior.
  - Progress: `semantic_memory_ci_artifacts.py --mode benchmark` now runs the
    trend/budget checker by default (with explicit opt-out), CMake benchmark
    target/test now pass policy + trend report paths for that gate, and
    benchmark-harness wrapper coverage now includes benchmark-mode pass/fail
    cases that lock budget-gate execution and failure propagation.
  - Stop rule: if CI plumbing is too broad for one slice, land local checker + tests first.

- ◐ [P1-02] Deliver one measured peak-RSS reduction on a representative semantic fixture.
  - Value: tangible memory reduction, not stylistic refactor.
  - Acceptance criteria:
    1. Before/after benchmark report captured in commit notes.
    2. Median peak RSS improves by at least 5% on at least one representative fixture.
    3. Release compile/test gate remains green.
  - Progress: definition-validation worker scheduling now avoids eager
    full-index materialization outside fallback paths and moves partition index
    vectors directly into worker tasks instead of copying per worker launch;
    parallel validation now also iterates non-const partition chunks and moves
    owned index vectors into async workers, eliminating one extra
    `DefinitionPartitionChunk` vector copy layer in the launch loop.
  - Stop rule: if two attempts fail to reach 2% median improvement, archive approach as low-value and switch hotspot.

- ◐ [P1-03] Deliver one deterministic parallel-ready semantic partition behind a flag.
  - Value: validates multithread direction without destabilizing default path.
  - Acceptance criteria:
    1. One isolated semantic phase can run in partitioned mode under flag.
    2. Diagnostics/output order is byte-for-byte identical to single-thread mode.
    3. Focused parity tests lock deterministic behavior.
  - Progress: semantic-memory benchmark harness now supports
    `--definition-validation-workers 1|2|both`, emits worker-mode delta rows,
    records dump payload hashes per mode, and fails `both` mode when one-worker
    and two-worker dump hashes diverge for the same fixture/phase tuple; CMake
    now also exposes an expensive parity gate target/test
    (`primestruct_semantic_memory_definition_worker_parity`,
    `PrimeStruct_semantic_memory_definition_worker_parity`) that runs the same
    worker-mode parity contract through CI artifact capture.
  - Stop rule: if parity fails, land planner/scaffolding only and keep execution single-threaded.

P0 micro-slices `[P0-17]` through `[P0-28]` and P2 micro-slices `[P2-14]` through `[P2-42]` are archived in `docs/todo_finished.md` (April 12, 2026).
