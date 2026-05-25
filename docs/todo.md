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

- TODO-4565: Add minimal scene graph and camera data model | track: scene-renderer | primary surface: stdlib/std/scene scene model
- TODO-4572: Remove vector statement-helper compiler path | track: vector-special-case-deletion | primary surface: vector semantic/lowerer helpers
- TODO-4573: Remove compiler-owned map literal lowering | track: map-special-case-deletion | primary surface: map literal semantics/lowering
- TODO-4580: Replace private source-lock tests with public contracts | track: architecture-source-lock-contracts | primary surface: source-lock inventory and public contract tests

### Immediate Next 10

- TODO-4566: Render flat and rounded-rect scene primitives to BGRA8
- TODO-4590: Add international text shaping and glyph atlas path
- TODO-4567: Render first globally lit 3D SDF widget primitive
- TODO-4568: Emit scene nodes from the existing UI layout/widgets
- TODO-4569: Present scene-rendered UI through software surface bridge
- TODO-4574: Remove vector count/access compiler classifiers
- TODO-4575: Remove map helper/access compiler classifiers
- TODO-4576: Remove map backing-type compiler classification
- TODO-4593: Carry source-unit provenance into IR and VM debug maps

### Priority Lanes

- Source-unit provenance ledger: TODO-4592 completed parser/semantic
  diagnostic source-unit mapping. TODO-4583 added the IR schema/version
  contract that TODO-4593 must follow when it changes IR source-map metadata.
  TODO-4591 completed the inspectable expanded-source ledger that this lane
  builds on.
  This lane is intentionally separate from TODO-4581 provenance ownership and
  TODO-4586 diagnostic stability tiers: it adds the missing source-unit/file
  identity that those later contracts can consume without absorbing
  TODO-4583's schema/versioning work.
- Scene graph renderer and UI presentation: TODO-4565 -> TODO-4566 ->
  (TODO-4590 and TODO-4567) -> TODO-4568 -> TODO-4569
- Map/vector compiler-independence: TODO-4570 retired the duplicate `map2`
  surface and TODO-4571 added the compiler-knowledge inventory categories that
  guide deletion scope. Vector path TODO-4572 -> TODO-4574 ->
  TODO-4577; map path TODO-4573 -> TODO-4575 -> TODO-4576; join at
  TODO-4578 -> TODO-4579
- Architecture hardening backlog: TODO-4580, TODO-4581 -> TODO-4582,
  TODO-4584, TODO-4585, TODO-4586, TODO-4587, TODO-4588, TODO-4589

### Execution Queue

- TODO-4565: Add minimal scene graph and camera data model
- TODO-4572: Remove vector statement-helper compiler path
- TODO-4573: Remove compiler-owned map literal lowering
- TODO-4566: Render flat and rounded-rect scene primitives to BGRA8
- TODO-4590: Add international text shaping and glyph atlas path
- TODO-4567: Render first globally lit 3D SDF widget primitive
- TODO-4568: Emit scene nodes from the existing UI layout/widgets
- TODO-4569: Present scene-rendered UI through software surface bridge
- TODO-4574: Remove vector count/access compiler classifiers
- TODO-4575: Remove map helper/access compiler classifiers
- TODO-4576: Remove map backing-type compiler classification
- TODO-4577: Remove vector backing-type compiler classification
- TODO-4578: Generalize stdlib surface registry away from map/vector IDs
- TODO-4579: Enforce zero map/vector compiler-knowledge traces
- TODO-4580: Replace private source-lock tests with public contracts
- TODO-4581: Split lowerer meaning from syntax provenance
- TODO-4582: Add semantic-product consumer coverage matrix
- TODO-4593: Carry source-unit provenance into IR and VM debug maps
- TODO-4584: Generalize backend capability gating
- TODO-4585: Manifest-drive stdlib module inclusion
- TODO-4586: Define diagnostic stability tiers
- TODO-4587: Extract shared compile-time/runtime VM kernel boundary
- TODO-4588: Add pass/phase invalidation manifest beyond semantics
- TODO-4589: Add architecture health dashboard script

### Task Blocks

- [ ] TODO-4565: Add minimal scene graph and camera data model
  - owner: ai
  - created_at: 2026-05-24
  - phase: Scene graph renderer and UI presentation
  - depends_on: TODO-4564
  - scope: Add the first source-level scene graph data model with deterministic
    node ordering, local transforms, optional local z/order metadata,
    material/light handles, primitive handles, and a camera projection config
    whose only implemented render mode is orthographic.
  - implementation_notes: Prefer a narrow stdlib surface such as
    `stdlib/std/scene/scene.prime` plus focused compile-run tests. Keep any C++
    helper/parser/lowerer additions generic and avoid hard-coding UI widget
    names into the scene model. The scene graph should be serializable or
    otherwise inspectable enough for deterministic golden tests. Use `f32`
    scene units for transforms/camera/material parameters; the UI adapter maps
    existing `i32` layout rect pixels into those scene units.
  - acceptance:
    - A small PrimeStruct program can build a scene with parented nodes,
      transforms, one camera, one light rig, and one material.
    - Node traversal and serialized/inspected output are deterministic across
      repeated VM/native/C++ emitter runs.
    - Orthographic projection parameters are represented on `Camera` as a
      projection mode/config; perspective is either absent from construction or
      rejected with a deterministic unsupported diagnostic.
    - A default UI camera/viewport fixture proves one scene unit maps to one
      logical pixel with the documented top-left, y-down UI orientation.
    - Tests cover parent-before-child ordering, deterministic render-order/z
      metadata, local transform composition metadata, and stable
      material/light/primitive ids.
  - stop_rule: Stop once scene data can be authored and inspected
    deterministically; do not add pixel rendering in this slice.

- [ ] TODO-4566: Render flat and rounded-rect scene primitives to BGRA8
  - owner: ai
  - created_at: 2026-05-24
  - phase: Scene graph renderer and UI presentation
  - depends_on: TODO-4565
  - scope: Add a deterministic CPU BGRA8 renderer for the minimal scene graph,
    covering flat rect/plane primitives and 2D rounded-rect SDF coverage under
    the orthographic camera.
  - implementation_notes: Reuse `examples/shared/software_surface_bridge.h` for
    `SoftwareSurfaceFrame` validation and add a narrow renderer helper under
    `examples/shared/` or the smallest appropriate runtime/test helper area.
    Treat SDF distance as coverage for one primitive, then source-over blend
    the primitive material color; do not add smooth boolean composition between
    differently colored commands. Follow UI painter order as the primary
    default; do not add a global depth sort for UI widgets in this slice.
  - acceptance:
    - Renderer output for one flat rect and one rounded rect is stable through
      exact pixel checks or fixed hashes on small BGRA8 buffers.
    - Rounded-rect rendering uses an analytic 2D SDF or equivalent distance
      function for edge coverage and radius handling.
    - Painter order, local z/depth ties, and clipping/target bounds follow the
      deterministic scene ordering contract.
    - A fixture proves local z metadata does not accidentally reorder ordinary
      UI source-over painting outside the documented tie/depth rule.
    - Tests cover differently colored overlapping 2D primitives as explicit
      source-over painting, not SDF/material color blending.
    - Existing `/std/ui/CommandList` golden serialization tests remain
      unchanged.
  - stop_rule: Stop once deterministic 2D scene primitives render to BGRA8;
    leave 3D SDF widgets and presenter wiring to later slices.

- [ ] TODO-4590: Add international text shaping and glyph atlas path
  - owner: ai
  - created_at: 2026-05-24
  - phase: Scene graph renderer and UI presentation
  - depends_on: TODO-4566
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

- [ ] TODO-4567: Render first globally lit 3D SDF widget primitive
  - owner: ai
  - created_at: 2026-05-24
  - phase: Scene graph renderer and UI presentation
  - depends_on: TODO-4566
  - scope: Add the first 3D SDF scene primitive for UI, a single beveled
    button/slab rendered through the same orthographic camera and a
    deterministic global light rig.
  - implementation_notes: Model the primitive as a shallow 3D volume in the
    widget rect's local space. Derive normals from the SDF gradient or a
    documented analytic approximation, and use a fixed light/material model
    with no stochastic sampling. The pressed state should adjust depth or
    bevel/material parameters explicitly, without introducing implicit material
    interpolation between SDF fields. Start with the documented default global
    UI light rig and default material fields; for the first button primitive,
    prefer bevel radius 4 logical pixels, normal depth 3 logical pixels, and
    pressed depth 1 logical pixel unless the TODO-4564 contract documents a
    better reasoned default.
  - acceptance:
    - The renderer produces stable pixel/hash output for normal and pressed
      states of the 3D SDF button primitive.
    - Lighting is global and deterministic, with documented ambient and
      directional/area-light-like terms.
    - The default button fixture uses the documented initial bevel/depth
      defaults and proves pressed state changes geometry/depth rather than
      relying on material color blending.
    - Material color remains owned by the primitive/material and is not inferred
      from blending SDF fields across unrelated shapes.
    - Any 3D SDF blending is limited to geometry/depth/normals unless a later
      explicit material-composition rule is documented.
    - Tests prove the 3D primitive can share a scene with 2D primitives under
      the same camera and render order rules.
  - stop_rule: Stop after one globally lit 3D UI primitive is deterministic and
    covered; do not add general mesh assets, animation, or free-camera behavior.

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

- [ ] TODO-4572: Remove vector statement-helper compiler path
  - owner: ai
  - created_at: 2026-05-24
  - phase: Map/vector compiler-independence
  - parallel_track: vector-special-case-deletion
  - inventory_categories: `vector-statement-helper`
  - scope: Delete the compiler-owned vector statement-helper validation and
    lowering path so `push`, `pop`, `reserve`, `clear`, `remove_at`, and
    `remove_swap` on vectors are resolved as ordinary imported `.prime`
    helper calls.
  - implementation_notes: Start with
    `src/semantics/SemanticsValidatorStatementVectorHelpers.cpp`,
    `src/semantics/SemanticsValidatorExprVectorHelpers.cpp`,
    `src/ir_lowerer/IrLowererFlowVectorHelpers.cpp`,
    `src/ir_lowerer/IrLowererFlowVectorResolutionHelpers.cpp`,
    `src/ir_lowerer/IrLowererLowerStatementsCallsStep.cpp`, and
    `src/ir_lowerer/IrLowererLowerInlineCalls.h`. Preserve ordinary
    parser/import resolution and the `.prime` implementations in
    `stdlib/std/collections/vector.prime` and
    `stdlib/std/collections/internal_vector.prime`.
  - acceptance:
    - Production C++ no longer declares or calls
      `validateVectorStatementHelper`, `tryEmitVectorStatementHelper`, or a
      vector-statement-only helper resolver.
    - Vector mutator calls compile and run through ordinary `.prime` helper
      definitions when those helpers are imported, with deterministic
      missing-import diagnostics when they are not.
    - Existing vector construction/count/access behavior remains covered by
      focused VM/native/C++ emitter tests.
    - The compiler-knowledge inventory from TODO-4571, if present, shows the
      vector statement-helper category at zero.
  - stop_rule: Stop once vector statement mutators are ordinary helper calls
    and focused vector tests pass; leave vector backing-layout classification
    and registry metadata cleanup to later leaves.

- [ ] TODO-4573: Remove compiler-owned map literal lowering
  - owner: ai
  - created_at: 2026-05-24
  - phase: Map/vector compiler-independence
  - parallel_track: map-special-case-deletion
  - inventory_categories: `map-literal-path`
  - scope: Remove C++ semantic and lowering branches that treat `map` literal
    construction as a compiler-owned collection literal instead of an ordinary
    call into `/std/collections/map/*` `.prime` code.
  - implementation_notes: Start with
    `src/semantics/SemanticsValidatorExprCollectionLiterals.cpp`,
    `src/semantics/SemanticsValidatorExprResolvedCallArguments.cpp`,
    `src/ir_lowerer/IrLowererOperatorCollectionMutationHelpers.cpp`, and
    compile-run tests that exercise `map<K, V>(...)`, `entry<K, V>(...)`, and
    assignment-pair syntax. Prefer making source examples call explicit
    `.prime` constructors/helpers over preserving a compiler-side map literal
    fast path.
  - acceptance:
    - Production C++ no longer has `collectMapLiteralEntries`, map-literal
      key/value validation diagnostics, or native lowering that constructs
      map storage slots directly.
    - Public map construction tests still pass through ordinary
      `/std/collections/map/map` and `/std/collections/map/entry`
      definitions.
    - Invalid map construction reports diagnostics from ordinary function
      resolution/type checking rather than map-literal-specific C++ branches.
    - The compiler-knowledge inventory from TODO-4571, if present, shows the
      map-literal category at zero.
  - stop_rule: Stop once map construction is no longer a compiler-owned
    literal/lowering path and focused map construction tests pass; leave map
    helper/access classifiers to TODO-4575 and map backing-type recognition
    cleanup to TODO-4576.

- [ ] TODO-4574: Remove vector count/access compiler classifiers
  - owner: ai
  - created_at: 2026-05-24
  - phase: Map/vector compiler-independence
  - depends_on: TODO-4572
  - inventory_categories: `vector-helper-classifier`
  - scope: Remove compiler-owned vector count/capacity/access helper
    classifiers after vector mutator statements have been routed through
    ordinary `.prime` helper calls.
  - implementation_notes: Start with `isVectorBuiltinName` callers that handle
    count, capacity, `at`, and `at_unsafe` in `src/semantics`,
    `src/ir_lowerer`, and `src/emitter`, especially
    `SemanticsValidatorExprCountCapacityBuiltins.cpp`,
    `SemanticsValidatorExprCollectionCountCapacity.cpp`,
    `IrLowererCountAccessClassifiers.cpp`,
    `IrLowererCountAccessHelpers.cpp`, `IrLowererNativeTailDispatch.cpp`, and
    emitter count/access rewrite helpers. Do not touch vector backing-layout
    path builders in this slice unless they become dead after the classifier
    deletion.
  - acceptance:
    - Production C++ no longer uses `isVectorBuiltinName` or equivalent
      vector-specific classifier branches for count, capacity, `at`, or
      `at_unsafe`.
    - Vector count/capacity/access calls compile and run through ordinary
      imported `.prime` helper definitions, with deterministic missing-import
      diagnostics when helpers are unavailable.
    - Focused VM/native/C++ emitter tests cover vector construction, count,
      capacity, safe access, unsafe access, and statement mutators after the
      classifier deletion.
    - The compiler-knowledge inventory from TODO-4571, if present, shows the
      vector helper-classifier category at zero, excluding backing-layout
      classifiers left for TODO-4577.
  - stop_rule: Stop once vector helper names are no longer compiler builtin
    classifiers and focused vector behavior still passes; leave vector
    backing-type recognition to TODO-4577.

- [ ] TODO-4575: Remove map helper/access compiler classifiers
  - owner: ai
  - created_at: 2026-05-24
  - phase: Map/vector compiler-independence
  - depends_on: TODO-4573
  - inventory_categories: `map-helper-classifier`
  - scope: Remove compiler-owned map helper/access classifiers after map
    construction literals have been routed through ordinary `.prime`
    constructors and helpers.
  - implementation_notes: Start with `isKeyValueBuiltinName`,
    map-specific access/contains/tryAt/insert helper classifiers, map helper
    bridge-key dispatch checks, and map-specific access target resolution in
    `src/semantics`, `src/ir_lowerer`, and `src/emitter`. Keep generic
    key/value wording only when it applies to non-map collection-pair facts
    and does not recognize `/std/collections/map/*` as a compiler builtin.
  - acceptance:
    - Production C++ no longer uses `isKeyValueBuiltinName` or equivalent
      map-specific helper/access classifier branches for count, contains,
      tryAt, at, at_unsafe, insert, or their reference variants.
    - Public map helper calls compile and run through ordinary imported
      `.prime` definitions, with deterministic missing-import diagnostics when
      helpers are unavailable.
    - Focused VM/native/C++ emitter tests cover map construction, count,
      contains, tryAt, at, at_unsafe, insert, and reference helpers after the
      classifier deletion.
    - The compiler-knowledge inventory from TODO-4571, if present, shows the
      map helper/access-classifier category at zero, excluding backing-layout
      classifiers left for TODO-4576.
  - stop_rule: Stop once map helper names are no longer compiler builtin
    classifiers and focused map behavior still passes; leave map backing-type
    recognition to TODO-4576.

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

- [ ] TODO-4577: Remove vector backing-type compiler classification
  - owner: ai
  - created_at: 2026-05-24
  - phase: Map/vector compiler-independence
  - depends_on: TODO-4574
  - inventory_categories: `vector-backing-classifier`, `vector-literal-path`
  - scope: Delete C++ recognition of vector backing structs and specialized
    `Vector` storage paths, replacing it with generic struct layout,
    semantic product, and ordinary helper-call facts.
  - implementation_notes: Start with
    `specializedExperimentalVectorStructPathForElementType`,
    `experimentalCollectionTypePath("vector", "Vector")`,
    `experimentalCollectionMemberRoot("vector")`,
    vector local capacity diagnostics, and vector backing predicates in
    `src/ir_lowerer`, `src/semantics`, and `src/emitter`. Do this only after
    vector helper classifiers are gone, so remaining matches are
    backing/layout responsibilities rather than public helper dispatch.
    Preserve ordinary C++ `std::vector` container use.
  - acceptance:
    - Production C++ no longer checks for PrimeStruct `Vector` backing paths,
      specialized vector struct path builders, or vector-only layout branches.
    - Vector construction, count, capacity, access, and mutation tests still
      pass through `.prime` definitions and generic struct/layout facts.
    - Vector capacity/allocation diagnostics either come from `.prime` code or
      from generic runtime/allocation diagnostics, not vector-specific C++
      branches.
    - The compiler-knowledge inventory from TODO-4571 shows the vector
      backing-classifier category at zero.
  - stop_rule: Stop once vector backing identity is no longer recognized by
    compiler-specific C++ branches and focused vector runtime tests pass.

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

- [ ] TODO-4580: Replace private source-lock tests with public contracts
  - owner: ai
  - created_at: 2026-05-24
  - phase: Architecture hardening
  - parallel_track: architecture-source-lock-contracts
  - scope: Replace one cluster of temporary private-source architecture locks
    with public semantic-product, IR, diagnostic, or testing-helper contracts.
  - implementation_notes: Start from `docs/source_lock_inventory.md`,
    `tests/unit/test_ir_pipeline_backends_graph_contexts.h`, and the matching
    source-lock shard. Pick one bounded cluster, add public contract coverage
    first, then remove or narrow the private-source string assertions and update
    the inventory.
  - acceptance:
    - One inventory row classified as a temporary migration lock is replaced or
      materially reduced by public API, semantic-product dump, IR, or diagnostic
      contract coverage.
    - The replacement test fails on the intended behavior regression without
      reading private `src/` files as the primary assertion surface.
    - `docs/source_lock_inventory.md` records the retired lock or the remaining
      reduced private-source dependency.
    - Focused release-mode tests covering the changed contract pass.
  - stop_rule: Stop after one source-lock cluster is replaced or narrowed; do
    not attempt a repo-wide source-lock migration in this slice.

- [ ] TODO-4581: Split lowerer meaning from syntax provenance
  - owner: ai
  - created_at: 2026-05-24
  - phase: Architecture hardening
  - scope: Introduce an explicit lowering context that separates
    semantic-product meaning from AST-owned syntax provenance for one lowerer
    stage or helper family.
  - implementation_notes: Start from `src/IrPreparation.cpp`,
    `include/primec/IrLowerer.h`, `src/ir_lowerer/IrLowererLower.cpp`, and
    `src/ir_lowerer/IrLowererSemanticProductTargetAdapters.*`. Choose a narrow
    consumer where both `Program` and `SemanticProgram` are currently consulted,
    and make the semantic-product lookup the only authority for meaning while
    retaining AST spans/source-map data only as provenance.
  - acceptance:
    - The selected lowering helper receives a context or inputs that make
      meaning-vs-provenance ownership explicit.
    - Missing semantic-product meaning for the selected helper fails closed with
      a deterministic diagnostic instead of silently re-deriving from AST state.
    - Syntax spans, source-map data, or debug provenance still come from the AST
      where needed.
    - Focused IR/lowering tests cover both positive lowering and stale/missing
      semantic-product facts for the selected helper.
  - stop_rule: Stop after one lowerer helper family has the explicit split and
    focused coverage; leave broader lowerer entrypoint reshaping to follow-up
    slices.

- [ ] TODO-4582: Add semantic-product consumer coverage matrix
  - owner: ai
  - created_at: 2026-05-24
  - phase: Architecture hardening
  - depends_on: TODO-4581
  - scope: Add a checked inventory mapping semantic-product fact families to
    production consumers and coverage for positive plus stale/missing-fact
    behavior.
  - implementation_notes: Start from `include/primec/SemanticProduct.h`,
    `src/SemanticProduct.cpp`, `src/ir_lowerer/`, and existing negative
    semantic-product tests in `tests/unit/test_ir_pipeline_backends_*`.
    Prefer a small checked document or script output that can be source-locked
    until a richer manifest exists.
  - acceptance:
    - Each published semantic-product fact family has an inventory row naming
      known production consumers or explicitly stating no production consumer.
    - At least three fact families have positive consumer coverage and
      stale/missing-fact coverage referenced from the inventory.
    - The inventory identifies gaps as concrete follow-up candidates without
      adding broad umbrella TODOs.
    - Focused backend IR or semantic-product tests pass.
  - stop_rule: Stop once the first consumer matrix is checked and validates real
    coverage for at least three representative fact families.

- [ ] TODO-4584: Generalize backend capability gating
  - owner: ai
  - created_at: 2026-05-24
  - phase: Architecture hardening
  - scope: Replace one ad hoc backend/profile support check with a generic
    backend capability registry used by diagnostics and tests.
  - implementation_notes: Start from graphics substrate target checks in
    `src/CompilePipeline.cpp`, backend profile helpers, and
    `include/primec/IrBackends.h`. The first registry should cover one
    capability family such as graphics runtime substrate availability without
    changing supported targets.
  - acceptance:
    - A backend/profile capability table or API describes the selected
      capability for VM, native, Wasm, GLSL/SPIR-V, and C++/IR aliases as
      applicable.
    - The selected ad hoc check is routed through the registry and still emits
      the same deterministic unsupported-target diagnostic.
    - Focused tests cover at least one supported target and one unsupported
      target for the capability.
    - Docs or code comments identify how future capability gates should use the
      registry.
  - stop_rule: Stop once one backend capability family is registry-driven and
    covered; leave broader backend policy migration to later slices.

- [ ] TODO-4585: Manifest-drive stdlib module inclusion
  - owner: ai
  - created_at: 2026-05-24
  - phase: Architecture hardening
  - scope: Replace one stdlib auto-include heuristic with checked manifest data
    for module roots, dependencies, public imports, or backend support.
  - implementation_notes: Start from `appendStdlibModuleSources` and
    `collectStdlibAutoIncludeKeys` in `src/CompilePipeline.cpp`, plus existing
    `.psmeta` stdlib metadata. Keep the first manifest field small and
    deterministic, and preserve current import behavior.
  - acceptance:
    - One currently hard-coded stdlib inclusion rule is represented in a checked
      manifest or manifest-like metadata file.
    - The compile pipeline reads that metadata deterministically and preserves
      current successful imports and diagnostics.
    - Tests cover direct import, wildcard import, and missing/invalid metadata
      behavior for the selected rule.
    - Docs or source-lock coverage describe the manifest field so future stdlib
      modules can follow it.
  - stop_rule: Stop after one inclusion rule is manifest-driven and covered; do
    not convert the entire stdlib loader in this slice.

- [ ] TODO-4586: Define diagnostic stability tiers
  - owner: ai
  - created_at: 2026-05-24
  - phase: Architecture hardening
  - scope: Classify diagnostics into stable user-facing contracts and
    implementation diagnostics for one compiler phase, then lock the stable tier
    with code/message/span coverage.
  - implementation_notes: Start from `include/primec/Diagnostics.h`,
    semantic/parser diagnostic tests, and `--collect-diagnostics` behavior.
    Choose one phase such as parser errors, import errors, or semantic
    call-resolution diagnostics.
  - acceptance:
    - The selected diagnostic phase has documented stability tiers for code,
      message text, primary span, and notes.
    - Stable-tier diagnostics have focused tests that assert code and span
      behavior, not only raw message substrings.
    - Implementation-tier diagnostics are clearly allowed to change without
      implying a public contract break.
    - Existing diagnostics for the selected phase remain deterministic.
  - stop_rule: Stop after one diagnostic phase has tier docs and focused
    coverage; leave other phases to future slices.

- [ ] TODO-4587: Extract shared compile-time/runtime VM kernel boundary
  - owner: ai
  - created_at: 2026-05-24
  - phase: Architecture hardening
  - scope: Make one compile-time evaluation path and one `primevm` runtime path
    share a documented VM-kernel API instead of incidental runtime state.
  - implementation_notes: Start from `include/primec/CompileTimeEvaluation.h`,
    `src/CompileTimeEvaluation.cpp`, `src/VmExecutionKernel.cpp`, and
    `tests/unit/test_compile_time_evaluation_facade.cpp`. Keep the first slice
    to arithmetic/call/frame mechanics or another small common kernel surface.
  - acceptance:
    - A named shared kernel API is used by both compile-time evaluation and
      runtime VM execution for the selected behavior.
    - Compile-time evaluation remains unable to perform runtime-only effects or
      launch final backend IR.
    - Runtime VM behavior and debug execution for the selected behavior remain
      covered.
    - Focused CT-eval and VM tests prove parity where the kernel is shared and
      rejection where compile-time effects are not allowed.
  - stop_rule: Stop once one shared kernel behavior is extracted and covered;
    do not merge compile-time and runtime hosts broadly.

- [ ] TODO-4588: Add pass/phase invalidation manifest beyond semantics
  - owner: ai
  - created_at: 2026-05-24
  - phase: Architecture hardening
  - scope: Extend the semantic validation manifest idea to one IR
    preparation, lowering, or emitter phase with explicit inputs, outputs,
    mutation, and invalidation rules.
  - implementation_notes: Start from `include/primec/SemanticValidationPlan.h`,
    `src/semantics/SemanticValidationPlan.cpp`, `src/IrPreparation.cpp`, and
    the selected phase implementation. Prefer a public header manifest rather
    than source-locking private control flow.
  - acceptance:
    - One non-semantics phase publishes a manifest entry or manifest family with
      phase name, input ownership, output ownership, mutation action, and
      invalidation/consumer notes.
    - Tests assert the manifest shape and at least one ordering or ownership
      invariant relevant to the selected phase.
    - Any existing source-lock assertion for the same phase is narrowed,
      replaced, or explicitly documented as still temporary.
    - Docs explain how future phase changes should update the manifest.
  - stop_rule: Stop after one non-semantics phase has a manifest-backed
    contract and focused tests.

- [ ] TODO-4589: Add architecture health dashboard script
  - owner: ai
  - created_at: 2026-05-24
  - phase: Architecture hardening
  - scope: Add a repo-local script that reports architecture health metrics
    without changing compiler behavior.
  - implementation_notes: Aggregate existing checks where possible:
    `docs/source_lock_inventory.md`, `scripts/include_layer_allowlist.txt`,
    semantic-product fact family metadata, top file sizes, and semantic memory
    or graph budget artifacts when present. The first version should report
    metrics and support an optional JSON output; only fail on script errors, not
    metric thresholds.
  - acceptance:
    - A script prints source-lock inventory count, include-layer allowlist count,
      semantic-product fact family count, largest subsystem files, and available
      graph/semantic-memory budget summary paths.
    - The script has self-tests for parsing representative inputs and for
      missing optional benchmark artifacts.
    - CMake/CTest wires the self-test, while the dashboard itself remains a
      developer helper unless a later TODO defines thresholds.
    - README or docs mention the helper as an architecture triage entrypoint.
  - stop_rule: Stop once the dashboard and self-tests land; do not add failing
    architecture thresholds in this slice.

- [ ] TODO-4593: Carry source-unit provenance into IR and VM debug maps
  - owner: ai
  - created_at: 2026-05-24
  - phase: Source-unit provenance ledger
  - depends_on: TODO-4592, TODO-4583
  - scope: Extend lowered IR source-map metadata and VM/debug lookup so
    instruction provenance can identify the source unit/file as well as line,
    column, and AST/synthetic provenance.
  - implementation_notes: Start from `IrInstructionSourceMapEntry`,
    `IrSerializer.cpp`, `IrLowererLowerStatementsSourceMapStep.*`,
    `VmDebugHelpers.cpp`, and
    `tests/unit/test_ir_pipeline_serialization_control_flow_metadata.h`.
    Coordinate with TODO-4583 because serialized IR source-map metadata changes
    must follow the version/schema contract; if TODO-4583 already landed,
    update that contract in the same slice.
  - acceptance:
    - IR source-map entries can carry a source-unit id or file identity in
      addition to debug id, line, column, and provenance.
    - IR serialization/deserialization and golden coverage include
      source-unit-aware metadata, with unknown/old metadata behavior handled
      according to the TODO-4583 schema contract.
    - VM breakpoint lookup can disambiguate identical line/column positions in
      different source units when file/module identity is supplied.
    - VM mapped stack traces include source file or source-unit display
      identity when available and keep deterministic fallback text when it is
      unavailable.
    - Focused IR serialization and VM debug tests pass for primary-source and
      imported-source instruction provenance.
  - stop_rule: Stop once source-unit identity survives lowering,
    serialization, and VM debug lookup for a focused imported-source fixture;
    do not redesign the debugger protocol or broaden diagnostic policy in this
    slice.
