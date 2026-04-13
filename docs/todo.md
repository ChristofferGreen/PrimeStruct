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

S2 micro-slices `[S2-01]` through `[S2-05]`, S3 micro-slices `[S3-01]` through
`[S3-132]`, and S4 slices `[S4-01a1]` through `[S4-03]` are archived in
`docs/todo_finished.md` (April 12-13, 2026).

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
    all chunks in the launcher path; definition validation now routes both full
    and chunked validation through a shared stable-index resolver path, removing
    transient full-range stable-index vector materialization in serial and
    single-partition fallback flows.
  - Stop rule: if two attempts fail to reach 2% median improvement, archive approach as low-value and switch hotspot.

P0 micro-slices `[P0-17]` through `[P0-28]`, P1 slices `[P1-01]` and
`[P1-03]`, and P2 micro-slices `[P2-14]` through `[P2-42]` are archived in
`docs/todo_finished.md` (April 12-13, 2026).
