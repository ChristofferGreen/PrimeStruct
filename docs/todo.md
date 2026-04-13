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

- ◐ [P1-02] Deliver one measured peak-RSS reduction on a representative semantic fixture.
  - Value: tangible memory reduction, not stylistic refactor.
  - Acceptance criteria:
    1. Before/after benchmark report captured in commit notes.
    2. Median peak RSS improves by at least 5% on at least one representative fixture.
    3. Release compile/test gate remains green.
  - Progress: definition-validation worker scheduling now avoids eager
    full-index materialization outside fallback paths and moves partition index
    vectors directly into worker tasks instead of copying per worker launch;
    partition planning now stores stable-order range metadata
    (`stableOrderOffset` + `stableOrderCount`) instead of per-partition index
    vectors, and worker chunks now materialize only bounded local ranges during
    execution, eliminating persistent partition index-vector allocation across
    all chunks in the launcher path.
  - Stop rule: if two attempts fail to reach 2% median improvement, archive approach as low-value and switch hotspot.

P0 micro-slices `[P0-17]` through `[P0-28]`, P1 slices `[P1-01]` and
`[P1-03]`, and P2 micro-slices `[P2-14]` through `[P2-42]` are archived in
`docs/todo_finished.md` (April 12-13, 2026).
