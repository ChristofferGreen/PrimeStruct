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

- TODO-4590: Add international text shaping and glyph atlas path | track: scene-text-renderer | primary surface: scene renderer text/glyph output
- TODO-4576: Remove map backing-type compiler classification | track: map-backing-classifier-deletion | primary surface: map backing/layout classifiers

### Immediate Next 10

- TODO-4568: Emit scene nodes from the existing UI layout/widgets
- TODO-4569: Present scene-rendered UI through software surface bridge

### Priority Lanes

- Scene graph renderer and UI presentation: TODO-4565 completed the data-only
  scene model and TODO-4566 completed the first BGRA8 2D primitive renderer;
  TODO-4567 completed the first globally lit 3D SDF widget primitive.
  TODO-4590 -> TODO-4568 -> TODO-4569
- Map/vector compiler-independence: TODO-4570 retired the duplicate `map2`
  surface, TODO-4571 added the compiler-knowledge inventory categories that
  guide deletion scope, and TODO-4573 removed compiler-owned map literal
  lowering. TODO-4575 removed map helper/access classifiers, and vector path
  TODO-4572 and TODO-4574 completed the public helper classifier deletions.
  TODO-4577 removed vector backing classifiers; map path TODO-4576 remains
  before the join at TODO-4578 -> TODO-4579
- Architecture hardening backlog: TODO-4586 completed parser diagnostic
  stability tiers. TODO-4587 completed the shared compile-time/runtime VM
  kernel boundary. TODO-4588 added the IR-preparation phase manifest.
  TODO-4589 added the architecture health dashboard. TODO-4594 completed the
  semantic unknown-call diagnostic stability slice.

### Execution Queue

- TODO-4590: Add international text shaping and glyph atlas path
- TODO-4576: Remove map backing-type compiler classification
- TODO-4568: Emit scene nodes from the existing UI layout/widgets
- TODO-4569: Present scene-rendered UI through software surface bridge
- TODO-4578: Generalize stdlib surface registry away from map/vector IDs
- TODO-4579: Enforce zero map/vector compiler-knowledge traces

### Task Blocks

- [ ] TODO-4590: Add international text shaping and glyph atlas path
  - owner: ai
  - created_at: 2026-05-24
  - phase: Scene graph renderer and UI presentation
  - parallel_track: scene-text-renderer
  - scope: Add the first renderer-private international text pipeline for 2D
    overlay scene text: Unicode text segmentation/bidi handling, shaped glyph
    runs, font fallback, glyph rasterization, glyph atlas packing, and
    deterministic BGRA8 composition under the orthographic UI camera.
  - implementation_notes: Use wrapper APIs owned by the renderer/text layer so
    third-party C/C++ types do not leak into PrimeStruct public APIs. Target a
    HarfBuzz-class shaping backend, FreeType-class font loading/rasterization
    backend, and ICU/FriBidi-class Unicode bidi/boundary backend. Do not depend
    on system fonts for tests; add small checked-in fixture fonts with explicit
    license notes or a documented repo-local font-fixture strategy. Keep text as
    2D overlay coverage/atlas rendering; do not add 3D SDF text, mesh text, font
    editing, emoji color-font rendering, or arbitrary OpenType feature UI in
    this slice.
  - acceptance:
    - A renderer-owned API converts UTF-8 text plus font fallback candidates
      into deterministic positioned glyph runs without exposing third-party
      library types through stdlib or compiler public APIs.
    - Tests cover at least Latin with combining marks, Cyrillic or Greek,
      right-to-left Arabic or Hebrew, and one complex-shaping script fixture
      using checked-in deterministic fonts.
    - Font fallback is deterministic: missing glyphs choose the first covering
      fixture font in a documented fallback list and emit a stable missing-glyph
      result when no fixture covers the code point.
    - Glyph rasterization and atlas placement produce stable metrics and
      pixel/hash output across repeated runs for small BGRA8 buffers.
    - The build integration documents whether the text libraries are vendored,
      pinned FetchContent dependencies, or required system packages, and keeps
      their warnings/includes isolated from PrimeStruct production targets.
  - stop_rule: Stop once international 2D text can be shaped, rasterized, and
    composited deterministically for scene text fixtures; leave paragraph layout,
    color emoji, advanced OpenType controls, and 3D text to later leaves.

- [ ] TODO-4568: Emit scene nodes from the existing UI layout/widgets
  - owner: ai
  - created_at: 2026-05-24
  - phase: Scene graph renderer and UI presentation
  - depends_on: TODO-4567, TODO-4590
  - scope: Add an adapter from the current `/std/ui` layout/widget layer to the
    scene graph so UI rects and widget state produce scene nodes for flat and
    raised presentation.
  - implementation_notes: Keep the existing `CommandList`, `HtmlCommandList`,
    `UiEventStream`, and `LayoutTree` contracts stable. Add adapter helpers
    rather than replacing command serialization. Start with labels/panels as
    flat primitives and buttons as the first raised 3D SDF primitive; hit
    testing, focus, and events remain layout-node based. Label/text output
    should emit shaped international 2D overlay scene primitives through the
    TODO-4590 text path; do not make text a 3D SDF primitive in this slice.
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

- [ ] TODO-4576: Remove map backing-type compiler classification
  - owner: ai
  - created_at: 2026-05-24
  - phase: Map/vector compiler-independence
  - depends_on: TODO-4575
  - inventory_categories: `map-backing-classifier`
  - scope: Delete C++ recognition of map backing structs and `MapValue`
    shapes, replacing it with generic struct layout, semantic product, and
    ordinary helper-call facts.
  - implementation_notes: Start with `isExperimentalMapStructTypePath`,
    `inferPublishedExperimentalMapStructPathFromConstructorPath`,
    `resolveExperimentalMapValueTarget`, `isMapValue`, and any
    `MapValue`-specific paths in `src/ir_lowerer`, `src/semantics`, and
    `src/emitter`. Do this only after map helper/access classifiers are gone,
    so the remaining matches are backing/layout responsibilities rather than
    public helper dispatch.
  - acceptance:
    - Production C++ no longer checks for `MapValue`, experimental map backing
      paths, `isExperimentalMapStructTypePath`,
      `resolveExperimentalMapValueTarget`, or `isMapValue`.
    - Map helper calls, returns, access, and insertion behavior still compile
      through `.prime` definitions and generic struct/layout facts.
    - Existing map backing/source trace tests are updated or replaced so they
      assert zero compiler backing knowledge instead of maintaining an
      allowance inventory.
    - The compiler-knowledge inventory from TODO-4571 shows the map
      backing-classifier category at zero.
  - stop_rule: Stop once map backing identity is no longer recognized by
    compiler-specific C++ branches and focused map runtime tests pass.

- [ ] TODO-4578: Generalize stdlib surface registry away from map/vector IDs
  - owner: ai
  - created_at: 2026-05-24
  - phase: Map/vector compiler-independence
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
