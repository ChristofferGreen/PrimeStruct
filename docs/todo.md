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

- TODO-4578: Generalize stdlib surface registry away from map/vector IDs | track: stdlib-registry-generalization | primary surface: stdlib surface registry metadata

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
  TODO-4576 and TODO-4577 removed map/vector backing classifiers; join at
  TODO-4578 -> TODO-4579.
- Architecture hardening backlog: TODO-4586 completed parser diagnostic
  stability tiers. TODO-4587 completed the shared compile-time/runtime VM
  kernel boundary. TODO-4588 added the IR-preparation phase manifest.
  TODO-4589 added the architecture health dashboard. TODO-4594 completed the
  semantic unknown-call diagnostic stability slice.

### Execution Queue

- TODO-4578: Generalize stdlib surface registry away from map/vector IDs
- TODO-4579: Enforce zero map/vector compiler-knowledge traces

### Task Blocks

- [ ] TODO-4578: Generalize stdlib surface registry away from map/vector IDs
  - owner: ai
  - created_at: 2026-05-24
  - phase: Map/vector compiler-independence
  - parallel_track: stdlib-registry-generalization
  - depends_on: TODO-4576, TODO-4577
  - inventory_categories: `stdlib-bridge-key`, `stdlib-registry-id`
  - scope: Remove map/vector-specific C++ stdlib surface IDs, helper APIs,
    and bridge-key branches so C++ treats stdlib surface metadata as generic
    manifest data instead of named collection knowledge.
  - implementation_notes: Start with `include/primec/StdlibSurfaceRegistry.h`,
    `src/StdlibSurfaceRegistry.cpp`, `stdlib/std/collections/surfaces.psmeta`,
    `StdlibCollectionSurfaceHelpers.h`, and all callers of
    `findStdlibSurfaceMetadataByBridgeKey("collections.vector_helpers")` or
    `findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")`.
    Keep a generic manifest loader/resolver if compiler phases still need
    import/visibility metadata, but remove public enum constants and helper
    functions whose names encode map or vector.
  - acceptance:
    - `StdlibSurfaceId` and registry helper APIs no longer contain
      map/vector-specific names such as `CollectionsVector*` or
      `CollectionsMap*`.
    - Production C++ no longer hard-codes `collections.vector_helpers`,
      `collections.vector_constructors`, `collections.map_helpers`, or
      `collections.map_constructors`.
    - Map/vector import visibility and helper resolution still work through
      generic manifest data loaded from `.psmeta` or equivalent stdlib-owned
      files.
    - Focused tests prove adding or renaming a stdlib collection surface only
      requires stdlib metadata/test updates, not new C++ enum/API branches.
  - stop_rule: Stop once map/vector stdlib metadata is data-driven from the
    compiler's point of view and focused map/vector import/helper tests pass.

- [ ] TODO-4579: Enforce zero map/vector compiler-knowledge traces
  - owner: ai
  - created_at: 2026-05-24
  - phase: Map/vector compiler-independence
  - depends_on: TODO-4571, TODO-4578
  - inventory_categories: all categories reported by
    `scripts/check_map_vector_compiler_knowledge.py`
  - scope: Turn the broad compiler-knowledge inventory into the release-gate
    zero audit for PrimeStruct map/vector special-casing under production
    `src/` and `include/`.
  - implementation_notes: Start from the TODO-4571 audit script and wire its
    zero mode into CTest/CMake only after the deletion leaves have removed
    their categories. Keep existing narrow surface-trace scripts only if they
    still catch a distinct regression; otherwise replace them with the broader
    zero gate and self-tests.
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
