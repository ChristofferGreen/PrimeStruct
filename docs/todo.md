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

- TODO-4595: Add deterministic scene text shaping runs | track: scene-text-shaping | primary surface: scene renderer text shaping/fallback metrics
- TODO-4578: Generalize stdlib surface registry away from map/vector IDs | track: stdlib-registry-generalization | primary surface: stdlib surface registry metadata

### Immediate Next 10

- TODO-4596: Rasterize shaped scene text through a glyph atlas
- TODO-4568: Emit scene nodes from the existing UI layout/widgets
- TODO-4569: Present scene-rendered UI through software surface bridge

### Priority Lanes

- Scene graph renderer and UI presentation: TODO-4565 completed the data-only
  scene model and TODO-4566 completed the first BGRA8 2D primitive renderer;
  TODO-4567 completed the first globally lit 3D SDF widget primitive.
  TODO-4595 -> TODO-4596 -> TODO-4568 -> TODO-4569
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

- TODO-4595: Add deterministic scene text shaping runs
- TODO-4578: Generalize stdlib surface registry away from map/vector IDs
- TODO-4596: Rasterize shaped scene text through a glyph atlas
- TODO-4568: Emit scene nodes from the existing UI layout/widgets
- TODO-4569: Present scene-rendered UI through software surface bridge
- TODO-4579: Enforce zero map/vector compiler-knowledge traces

### Task Blocks

- [ ] TODO-4595: Add deterministic scene text shaping runs
  - owner: ai
  - created_at: 2026-05-26
  - phase: Scene graph renderer and UI presentation
  - parallel_track: scene-text-shaping
  - scope: Add the renderer-owned front half of the international 2D text
    overlay pipeline: UTF-8 decoding, deterministic direction/script runs,
    combining-mark attachment, fallback fixture-font selection, and shaped glyph
    run metrics for scene text.
  - implementation_notes: Keep the API under the renderer/text layer and expose
    no third-party text-library types. A deterministic repo-local fixture
    backend is acceptable for this slice if the wrapper boundary leaves room for
    HarfBuzz-class shaping and ICU/FriBidi-class bidi/boundary backends later.
    Do not add glyph bitmap rasterization, atlas packing, BGRA8 composition,
    paragraph layout, color emoji, advanced OpenType controls, 3D SDF text, or
    mesh text in this slice.
  - acceptance:
    - A renderer-owned API converts UTF-8 text plus ordered fallback candidates
      into deterministic positioned glyph runs without exposing third-party
      library types through stdlib or compiler public APIs.
    - Tests cover Latin with combining marks, Cyrillic or Greek,
      right-to-left Arabic or Hebrew, and one complex-shaping fixture using
      checked-in deterministic fixture-font metadata.
    - Font fallback is deterministic: missing glyphs choose the first covering
      fixture font in a documented fallback list and emit a stable
      missing-glyph result when no fixture covers the code point.
    - Shaped glyph run metrics are stable across repeated runs for small scene
      text fixtures and include enough data for a later atlas/raster pass.
    - Docs state that glyph rasterization and atlas composition are deferred to
      TODO-4596, while the wrapper API remains ready for HarfBuzz-class and
      ICU/FriBidi-class backends.
  - stop_rule: Stop once international scene text can be decoded, segmented,
    shaped into deterministic glyph runs, and measured for renderer fixtures;
    leave glyph bitmap output and BGRA8 composition to TODO-4596.

- [ ] TODO-4596: Rasterize shaped scene text through a glyph atlas
  - owner: ai
  - created_at: 2026-05-26
  - phase: Scene graph renderer and UI presentation
  - depends_on: TODO-4595
  - scope: Add the renderer-owned back half of the international 2D text overlay
    pipeline: deterministic glyph bitmap rasterization, atlas packing, and BGRA8
    source-over composition for shaped scene text runs.
  - implementation_notes: Consume the shaped glyph run API from TODO-4595. Keep
    rasterization and atlas internals behind renderer-owned wrappers so a
    FreeType-class backend can be substituted without exposing third-party
    types. Use checked-in deterministic fixture glyphs or fixture fonts; do not
    depend on system fonts. Keep text a 2D overlay primitive and do not add
    paragraph layout, color emoji, advanced OpenType controls, 3D SDF text, or
    mesh text in this slice.
  - acceptance:
    - A renderer-owned raster/atlas API consumes shaped glyph runs and produces
      deterministic atlas placements and coverage masks for small fixtures.
    - Tests cover repeated atlas packing stability, missing-glyph coverage, and
      at least one shaped bidirectional or complex-script fixture from
      TODO-4595.
    - Scene BGRA8 renderer tests prove text overlay composition is deterministic
      and source-over ordered with flat, rounded, and 3D SDF primitives.
    - Docs describe the path from shaped glyph runs to atlas coverage to BGRA8
      composition and keep native text dependencies behind renderer wrappers.
  - stop_rule: Stop once shaped scene text fixtures rasterize through a
    deterministic glyph atlas and compose into BGRA8 scene output; leave
    paragraph layout, color emoji, advanced OpenType controls, and host UI
    adapter emission to later leaves.

- [ ] TODO-4568: Emit scene nodes from the existing UI layout/widgets
  - owner: ai
  - created_at: 2026-05-24
  - phase: Scene graph renderer and UI presentation
  - depends_on: TODO-4567, TODO-4596
  - scope: Add an adapter from the current `/std/ui` layout/widget layer to the
    scene graph so UI rects and widget state produce scene nodes for flat and
    raised presentation.
  - implementation_notes: Keep the existing `CommandList`, `HtmlCommandList`,
    `UiEventStream`, and `LayoutTree` contracts stable. Add adapter helpers
    rather than replacing command serialization. Start with labels/panels as
    flat primitives and buttons as the first raised 3D SDF primitive; hit
    testing, focus, and events remain layout-node based. Label/text output
    should emit shaped international 2D overlay scene primitives through the
    TODO-4596 atlas/raster path; do not make text a 3D SDF primitive in this
    slice.
  - acceptance:
    - A checked-in PrimeStruct fixture builds a small UI layout and emits a
      deterministic scene representation.
    - Existing command-list, HTML adapter, event-stream, and layout golden tests
      continue to pass unchanged.
    - Scene emission preserves stable node ids or a documented mapping from UI
      layout nodes to scene nodes.
    - Docs and tests distinguish logical UI rects/state/events from scene
      presentation primitives, including 3D-looking widgets.
    - Label/text scene emission follows the documented international 2D overlay
      policy, uses shaped glyph runs, and has deterministic ordering over
      panels/buttons.
  - stop_rule: Stop once the UI-to-scene adapter covers one small panel/button
    fixture; leave host presentation and broader controls to later slices.

- [ ] TODO-4569: Present scene-rendered UI through software surface bridge
  - owner: ai
  - created_at: 2026-05-24
  - phase: Scene graph renderer and UI presentation
  - depends_on: TODO-4568
  - scope: Wire the scene renderer output into the existing BGRA8 software
    surface bridge so a real PrimeStruct-authored UI scene can be presented
    through the native/Metal host path.
  - implementation_notes: Start from `examples/shared/software_surface_bridge.h`,
    `examples/native/spinning_cube/window_host.mm`,
    `examples/metal/spinning_cube/metal_host.mm`, and the existing
    `--software-surface-demo` tests. Keep the deterministic demo frame as a
    fallback or separate mode, but add a mode that renders the UI scene output
    instead of the checker/gradient buffer.
  - acceptance:
    - A host/demo mode renders a PrimeStruct-authored UI scene into a
      `SoftwareSurfaceFrame` and uploads/presents that BGRA8 output through the
      existing bridge.
    - Tests cover command-line mode validation and deterministic renderer output
      before any GUI-dependent smoke step.
    - macOS/Metal visual smoke remains explicitly skippable on unsupported
      runners while source-level and pixel/hash coverage still run where
      possible.
    - Docs describe the path from `/std/ui` layout to scene graph to BGRA8
      surface to presenter without making UI events depend on rendered pixels
      or 3D ray tests.
    - The demo fixture uses the default orthographic UI coordinate mapping,
      painter-order policy, global light rig, and international 2D text overlay
      policy.
  - stop_rule: Stop once one PrimeStruct UI scene reaches the software-surface
    presenter path with deterministic non-GUI coverage.

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
