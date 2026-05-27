# PrimeStruct TODO Log

## Purpose

This file is the live open-work queue for PrimeStruct.

- Keep only open work here: `[ ]` queued or `[~]` in progress.
- Move completed work to `docs/todo_finished.md`.
- Do not keep completed-task summaries, historical rollout notes, or closed
  coverage snapshots in this file.
- When this file has no task blocks, the tracked TODO queue is empty.

## Operating Rules

1. Use one task block per item with a stable `TODO-XXXX` ID.
2. Every active leaf must be implementable by someone arriving with no session
   context, including an AI agent.
3. Every active leaf must include `owner`, `created_at`, `scope`,
   `acceptance`, and `stop_rule`.
4. Prefer small, testable leaves over broad epics; split work before starting
   when acceptance cannot be verified in one bounded change.
5. Every active leaf must target at least one value outcome:
   - user-visible behavior change
   - measurable perf/memory improvement
   - deletion of a real compatibility subsystem
6. Avoid standalone micro-cleanups unless bundled into a value outcome.
7. If a leaf misses its value target after two attempts, archive it as
   low-value and replace it with a different hotspot.
8. Keep `Ready Now`, `Immediate Next 10`, `Priority Lanes`, `Execution Queue`,
   and task blocks synchronized when adding, splitting, completing, or deleting
   a task.
9. Keep `Ready Now` capped at eight active leaf tasks.
10. Keep active work leaf-shaped: queue sections must not contain umbrella,
    tracker, phase, research-shaped, or "continue with another slice" items.
11. For parallel work, each `Ready Now` item must name a `parallel_track` and a
    primary surface. Do not put two same-track successors in `Ready Now` unless
    their task blocks prove they touch different source/test surfaces.
12. Treat disabled tests as debt: each retained `doctest::skip(true)` cluster
    must map to an active TODO leaf with a re-enable-or-delete outcome, or be
    removed once proven stale.
13. When completing a task, mark it `[x]`, add `finished_at` plus a short
    evidence note, move the full block to `docs/todo_finished.md`, and remove
    it from this file.

## Task Template

```md
- [ ] TODO-<id>: Short title
  - owner: ai|human
  - created_at: YYYY-MM-DD
  - phase: Group/Phase name (optional)
  - parallel_track: short-track-name (required when listed in Ready Now)
  - depends_on: TODO-XXXX, TODO-YYYY (optional)
  - scope: ...
  - implementation_notes: optional, but required when source/test entry points are not obvious
  - acceptance:
    - ...
    - ...
  - stop_rule: ...
  - notes: optional
```

## Open Tasks

### Ready Now

- TODO-4601: Remove final map helper classifier trace | track: map-helper-zero | primary surface: IR-lowerer map constructor metadata lookup
- TODO-4602: Remove semantic vector-literal compiler traces | track: semantic-vector-literal-zero | primary surface: semantic collection literal diagnostics
- TODO-4603: Remove IR-lowerer vector-literal compiler traces | track: lowerer-vector-literal-zero | primary surface: native vector literal lowering diagnostics

### Immediate Next 10

- TODO-4579: Enforce zero map/vector compiler-knowledge traces

### Priority Lanes

- Scene graph renderer and UI presentation: TODO-4565 completed the data-only
  scene model and TODO-4566 completed the first BGRA8 2D primitive renderer;
  TODO-4567 completed the first globally lit 3D SDF widget primitive, and
  TODO-4595 completed deterministic shaped glyph runs. TODO-4596 completed
  deterministic text atlas/raster composition. TODO-4568 completed the first
  UI scene-record adapter, and TODO-4569 completed the software-surface UI
  presentation bridge.
- Map/vector compiler-independence: TODO-4570 retired the duplicate `map2`
  surface, TODO-4571 added the compiler-knowledge inventory categories that
  guide deletion scope, and TODO-4573 removed compiler-owned map literal
  lowering. TODO-4575 removed map helper/access classifiers, and vector path
  TODO-4572 and TODO-4574 completed the public helper classifier deletions.
  TODO-4576 and TODO-4577 removed map/vector backing classifiers. TODO-4578
  was split into TODO-4597 registry foundation plus TODO-4598, TODO-4599, and
  TODO-4600 subsystem migrations; TODO-4597 completed the generic registry
  IDs, TODO-4598 completed the semantics migration, TODO-4599 completed the
  emitter migration, TODO-4600 completed the IR-lowerer migration, and the
  final TODO-4579 zero gate was split into concrete trace-deletion leaves
  TODO-4601 through TODO-4603 before the release-gate switch lands.
- Architecture hardening backlog: TODO-4586 completed parser diagnostic
  stability tiers. TODO-4587 completed the shared compile-time/runtime VM
  kernel boundary. TODO-4588 added the IR-preparation phase manifest.
  TODO-4589 added the architecture health dashboard. TODO-4594 completed the
  semantic unknown-call diagnostic stability slice.

### Execution Queue

- TODO-4601: Remove final map helper classifier trace
- TODO-4602: Remove semantic vector-literal compiler traces
- TODO-4603: Remove IR-lowerer vector-literal compiler traces
- TODO-4579: Enforce zero map/vector compiler-knowledge traces

### Task Blocks

- [ ] TODO-4601: Remove final map helper classifier trace
  - owner: ai
  - created_at: 2026-05-27
  - phase: Map/vector compiler-independence
  - parallel_track: map-helper-zero
  - depends_on: TODO-4597, TODO-4600
  - inventory_categories: `map-helper-classifier`
  - scope: Remove the remaining production map-helper classifier trace from
    IR-lowerer constructor metadata lookup without reintroducing map-specific
    helper path builders.
  - implementation_notes: Start with
    `src/ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp`, where the
    inventory currently reports `collectionMemberPath("map", "map")`.
    Prefer generic collection surface metadata or canonical-path helper APIs
    over spelling another map-specific path builder.
  - acceptance:
    - `python3 scripts/check_map_vector_compiler_knowledge.py --root . --require-zero-category map-helper-classifier`
      passes.
    - Existing IR-lowerer and map ownership source-lock coverage for
      collection constructor metadata still passes.
    - Production code does not add new `map-helper-classifier`,
      `stdlib-bridge-key`, or `stdlib-registry-id` traces.
  - stop_rule: Stop once the map-helper classifier category is zero and the
    focused source-lock/tests prove map constructor metadata still resolves.

- [ ] TODO-4602: Remove semantic vector-literal compiler traces
  - owner: ai
  - created_at: 2026-05-27
  - phase: Map/vector compiler-independence
  - parallel_track: semantic-vector-literal-zero
  - depends_on: TODO-4571
  - inventory_categories: `vector-literal-path`
  - scope: Remove vector-specific literal wording and identifiers from
    production semantic collection-literal validation while preserving the
    existing diagnostics and effect checks through generic collection-literal
    surfaces.
  - implementation_notes: Start with
    `src/semantics/SemanticsValidatorExprCollectionLiterals.cpp`, where the
    inventory currently reports vector-literal diagnostics.
  - acceptance:
    - `python3 scripts/check_map_vector_compiler_knowledge.py --root .` no
      longer reports `vector-literal-path` traces in `src/semantics/`.
    - Focused semantics or compile-run diagnostics for array/vector/soa
      literal validation still pass with the updated wording or source locks.
    - The change does not touch IR-lowerer vector-literal lowering.
  - stop_rule: Stop once semantic vector-literal traces are gone and the
    nearest collection-literal diagnostic tests pass.

- [ ] TODO-4603: Remove IR-lowerer vector-literal compiler traces
  - owner: ai
  - created_at: 2026-05-27
  - phase: Map/vector compiler-independence
  - parallel_track: lowerer-vector-literal-zero
  - depends_on: TODO-4571
  - inventory_categories: `vector-literal-path`
  - scope: Remove vector-specific literal wording and identifiers from
    production IR-lowerer/native vector literal support by routing the
    remaining logic through generic collection-literal names and helpers.
  - implementation_notes: Start with `src/ir_lowerer/IrLowererHelpers.cpp`
    and `src/ir_lowerer/IrLowererLowerInlineCalls.h`, where the inventory
    currently reports native vector-literal diagnostics and lowering traces.
  - acceptance:
    - `python3 scripts/check_map_vector_compiler_knowledge.py --root .` no
      longer reports `vector-literal-path` traces in `src/ir_lowerer/`.
    - Focused native/IR-lowerer vector literal tests still pass.
    - The change does not touch semantic collection-literal validation.
  - stop_rule: Stop once IR-lowerer vector-literal traces are gone and the
    nearest native/vector literal tests pass.

- [ ] TODO-4579: Enforce zero map/vector compiler-knowledge traces
  - owner: ai
  - created_at: 2026-05-24
  - phase: Map/vector compiler-independence
  - parallel_track: map-vector-zero-gate
  - depends_on: TODO-4571, TODO-4598, TODO-4599, TODO-4600, TODO-4601,
    TODO-4602, TODO-4603
  - inventory_categories: all categories reported by
    `scripts/check_map_vector_compiler_knowledge.py`
  - scope: Turn the broad compiler-knowledge inventory into the final
    release-gate zero audit after the remaining map-helper and vector-literal
    trace deletion leaves have landed.
  - implementation_notes: Start from the TODO-4571 audit script and wire its
    `--enforce-zero` mode into CTest/CMake once TODO-4601 through TODO-4603
    remove the current nonzero categories. Keep existing narrow surface-trace
    scripts only if they still catch a distinct regression; otherwise replace
    them with the broader zero gate and self-tests.
  - acceptance:
    - Routine release validation fails on any production C++ map/vector
      compiler-knowledge trace, including bridge keys, helper recognizers,
      literal paths, backing-layout classifiers, and map/vector-specific
      stdlib registry names.
    - The gate explicitly ignores ordinary C++ containers (`std::map`,
      `std::vector`), source-map terminology, tests, docs, and stdlib
      `.prime`/`.psmeta` files.
    - C++ tests may still mention map/vector to test stdlib behavior, but
      production compiler/runtime code no longer contains PrimeStruct
      map/vector special cases.
    - `docs/todo.md` and `docs/PrimeStruct.md` state the final invariant:
      map/vector semantics and implementation live in stdlib `.prime` and
      metadata files, while C++ treats them as ordinary included stdlib code.
  - stop_rule: Stop once the zero gate is wired into routine validation and
    focused map/vector stdlib tests plus the new audit pass.
