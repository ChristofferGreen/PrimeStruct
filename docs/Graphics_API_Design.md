# PrimeStruct Graphics API Design

Status: Locked (v1 Core Contract + Spinning-Cube Mini-Spec + Scene Renderer Boundary)
Last updated: 2026-05-25

This document defines the locked cross-backend graphics API contract for the
spinning-cube program family. The goal is to keep one portable language surface,
apply profile-gated validation at compile time, and avoid backend-specific
language namespaces in v1.

Implementation status note (2026-03-17): this document locks the source-level
contract. Current repo status:
- **Source surfaces:** canonical `/std/gfx/*` is now the authoritative public
  graphics surface. `/std/gfx/experimental/*` remains only as a legacy
  compatibility shim over that canonical helper layer for targeted migration
  coverage rather than as a peer public namespace.
- **Coverage breadth:** current coverage includes contract checks plus
  canonical public-surface coverage plus targeted compatibility-shim coverage.
- **Substrate boundaries:** the experimental path has an explicit `.prime`
  `GraphicsSubstrate` token/config boundary for create/acquire/submit/present
  operations. The canonical path also has private `.prime` substrate layers
  behind its public helpers:
  `GraphicsSubstrate.createWindow(...)`, `createDevice(...)`,
  `createQueue(...)`, `createSwapchain(...)`, `createMesh(...)`,
  `createPipeline(...)`, `acquireFrame(...)`, `openRenderPass(...)`,
  `drawMesh(...)`, `endRenderPass(...)`, `submitFrame(...)`, and
  `presentFrame(...)`.
- **Locked constructor/resource entrypoints:** constructor-shaped `Window(...)`,
  `Device()`, and `Buffer<T>(count)` now rewrite through dedicated stdlib
  helpers on both experimental and canonical paths. Fallible
  `Device.create_swapchain(...)`, `Device.create_mesh(...)`, and
  `Swapchain.frame()` wrapper paths are also in place, as is the type-valued
  `Device.create_pipeline([vertex_type] VertexColored, ...)` entrypoint for the
  locked v1 vertex wire type.
- **Render/submit/present behavior:** the non-Result `Frame.render_pass(...)`
  plus `RenderPass.draw_mesh(...)` / `RenderPass.end()` path preserves
  deterministic zero-token / no-op fallback on invalid handles, and the
  canonical path routes `render_pass(...)`, `draw_mesh(...)`, `end()`,
  `submit(...)`, and `present()` through the private `.prime` substrate layer.
- **Buffer helpers:** canonical and experimental `Buffer<T>` now expose
  `.prime`-authored `load(index)` / `store(index, value)` compute-storage
  wrappers alongside the existing `count()` / `empty()` / `is_valid()` /
  `readback()` / `allocate(...)` / `upload(...)` helper surface.
- **Real native-desktop path:** the first real native-desktop host/runtime path
  now consumes a deterministic canonical `/std/gfx/*` stream
  (`cubeStdGfxEmitFrameStream` via the macOS host `--gfx` mode) emitted by the
  shared spinning-cube `.prime` sample, so submit/present can drive one real
  macOS window host end-to-end. Launcher/docs/smoke coverage for that native
  window path no longer depend on the older `--cube-sim` host mode.
- **Shared host/sample helpers:** the native macOS launcher is now a thin
  wrapper over a shared canonical gfx launch helper, and the native macOS
  window host now binds its cube/software-surface callbacks onto a shared
  native Metal window presenter helper instead of owning the `NSWindow`,
  `CAMetalLayer`, timer, and first-frame/exit shell directly. The browser and
  Metal sample path also route through shared helpers: the offscreen Metal
  runtime shell lives in `examples/shared/metal_offscreen_host.h`, the Metal
  launcher delegates to `scripts/run_canonical_metal_host.sh` through
  `scripts/run_metal_spinning_cube.sh`, the Metal snapshot/parity helpers bind
  to `examples/shared/spinning_cube_simulation_reference.h`, the browser
  runtime shell lives in `examples/web/shared/browser_runtime_shared.js`, and
  the browser launcher delegates to `scripts/run_canonical_browser_sample.sh`
  through `scripts/run_browser_spinning_cube.sh`.
- **Compile-run conformance:** the repo now ships real compile-run conformance
  programs that import canonical `/std/gfx/*` and exercise `Window(...)`,
  `Device()`, `create_swapchain(...)`, `create_mesh(...)`,
  `create_pipeline(...)`, `frame()`, `render_pass(...)`, `draw_mesh(...)`,
  `submit(...)`, and `present()` across exe/vm/native instead of relying only
  on doc-lock coverage for that API surface. Separate compatibility-shim tests
  keep `/std/gfx/experimental/*` pinned for residual legacy imports.
- **Intentional rejects still in place:** source-level profile literals and
  unsupported `create_pipeline` vertex types are still intentionally rejected.
  Result-carrying method wrappers still reject bare explicit non-`Result`
  struct bindings during semantics. `/std/gfx/*` and
  `/std/gfx/experimental/*` imports now also reject deterministically on wasm
  (`wasm-browser`, `wasm-wasi`) and shader-only (`glsl`, `spirv`) emits because
  those targets still lack the required runtime substrate.
- **Remaining work:** reduce the remaining sample-owned
  compatibility/contract glue and keep the remaining public graphics API
  primarily in `.prime` files while leaving only minimal backend substrate in
  C++/host code.
- **Scene renderer boundary:** custom-drawn UI presentation targets a
  scene-graph producer contract rather than a special-purpose UI renderer:
  `/std/scene` or an equivalent scene surface owns the renderer-facing
  `Scene`, `Node`, `Transform`, `Camera`, `Material`, `Light`, and primitive
  descriptor model.

Status snapshot phrases kept contiguous for doc-lock checks:
- The non-Result `Frame.render_pass(...)` plus `RenderPass.draw_mesh(...)` / `RenderPass.end()` path now routes through
  minimal pass-encoding substrate helpers while preserving deterministic zero-token / no-op fallback on invalid handles.
- The native-desktop host/runtime path now consumes a deterministic canonical `/std/gfx/*` stream
  (`cubeStdGfxEmitFrameStream` via the macOS host `--gfx` mode).
- The native macOS launcher path is now a thin wrapper over a shared canonical gfx launch helper.
- The native macOS window host itself now binds its cube/software-surface callbacks onto a shared native Metal window
  presenter helper.
- The Metal sample path now also routes through shared helpers, including a shared metal launch helper and a shared
  spinning-cube simulation reference helper.
- The browser runtime shell now lives in `examples/web/shared/browser_runtime_shared.js`, and the browser sample
  launcher now delegates to `scripts/run_canonical_browser_sample.sh` through `scripts/run_browser_spinning_cube.sh`.

## Scope
- Covers the PrimeStruct language-facing graphics contract only.
- Defines locked Core/API constraints and profile-gating rules.
- Locks a concrete v1 mini-spec for spinning-cube style windowed rendering.
- Establishes conformance hooks that tests must exercise.

## v1 Principles
- One core graphics language surface under `/std/gfx/*`.
- No backend extension namespace in the language surface for v1.
- Profile differences are enforced with compile-time gating and deterministic
diagnostics.
- Browser, native desktop, and macOS Metal targets share one simulation/data
contract; only host/shader artifacts vary by profile.
- Source code should not hardcode backend profile names for device creation;
profile selection is compile-target driven.
- Treat `/std/gfx/*` as a hybrid surface: keep the public API contract and most
wrapper/helper behavior in stdlib `.prime`, while the minimal host/device/present
substrate remains runtime/backend-owned.

## Locked Core Surface (v1)
The locked core object model names are:
- `Window`
- `Device`
- `Queue`
- `Swapchain`
- `Frame`
- `RenderPass`
- `Buffer<T>`
- `Texture2D<T>`
- `Sampler`
- `ShaderModule`
- `Pipeline`
- `Material`
- `Mesh`
- `BindGroupLayout`
- `BindGroup`
- `CommandEncoder`
- `VertexColored`
- `GfxError`

These names are stable contract anchors. Implementation/lowering can be staged,
but source-level shape and diagnostics for this contract are locked.

## Locked Profile Set (v1)
The graphics profiles tracked by this contract are:
- `wasm-browser`
- `native-desktop`
- `metal-osx`

Current compiler flags may expose profile selectors differently (for example,
`--wasm-profile browser` with alias `wasm-browser`). The contract profile names
above remain the canonical design identifiers.

## Spinning-Cube Mini-Spec (v1)

### Locked Resource API Shape
The v1 spinning-cube path locks the following language-level call shape under
`/std/gfx/*` (surface syntax may use method sugar):

```text
Window([title] string, [width] i32, [height] i32)
  -> Result<Window, GfxError>

Device()
  -> Result<Device, GfxError>

Device.default_queue(self)
  -> Queue

Device.create_swapchain(
  self,
  [window] Window,
  [color_format] ColorFormat,
  [depth_format] DepthFormat,
  [present_mode] PresentMode
) -> Result<Swapchain, GfxError>

Device.create_mesh(
  self,
  [vertices] array<VertexColored>,
  [indices] array<i32>
) -> Result<Mesh, GfxError>

Device.create_pipeline(
  self,
  [shader] ShaderLibrary,
  [vertex_type] type,
  [color_format] ColorFormat,
  [depth_format] DepthFormat
) -> Result<Pipeline, GfxError>

Pipeline.material(self)
  -> Result<Material, GfxError>

Window.is_open(self) -> bool
Window.poll_events(self) -> void
Window.aspect_ratio(self) -> f32

Swapchain.frame(self)
  -> Result<Frame, GfxError>

Frame.render_pass(self, [clear_color] ColorRGBA, [clear_depth] f32)
  -> RenderPass

RenderPass.draw_mesh(self, [mesh] Mesh, [material] Material) -> void
RenderPass.end(self) -> void

Frame.submit(self, [queue] Queue) -> Result<void, GfxError>
Frame.present(self) -> Result<void, GfxError>

Buffer<T>(count) -> Buffer<T>
Buffer.count(self) -> i32
Buffer.empty(self) -> bool
Buffer.is_valid(self) -> bool
Buffer.readback(self) -> array<T>
Buffer.load(self, [index] i32) -> T
Buffer.store(self, [index] i32, [value] T) -> void
/std/gfx/Buffer/allocate<T>(count) -> Buffer<T>
/std/gfx/Buffer/upload(values) -> Buffer<T>
/std/gfx/Buffer/load(self, index) -> T
/std/gfx/Buffer/store(self, index, value) -> void
```

### Locked Profile-Deduction Rule
- Device creation must not require source-level profile literals.
- Profile selection comes from compiler target/profile flags.
- Source forms like `Device([profile] "metal-osx")` are out of contract for v1
portable graphics code.

### Locked Vertex Wire Layout: `VertexColored`
`/std/gfx/VertexColored` is a canonical GPU upload wire type with deterministic
field layout:

| Field | Type | Offset (bytes) |
| --- | --- | --- |
| `px` | `f32` | `0` |
| `py` | `f32` | `4` |
| `pz` | `f32` | `8` |
| `pw` | `f32` | `12` |
| `r` | `f32` | `16` |
| `g` | `f32` | `20` |
| `b` | `f32` | `24` |
| `a` | `f32` | `28` |

Locked layout properties:
- Total size: `32` bytes.
- Alignment: `4` bytes.
- No implicit backend-added padding.
- Upload of `array<VertexColored>` must preserve this byte layout across
native/VM/Wasm host-side upload paths that claim v1 graphics support.
- Sample-owned native/Metal glue that needs to upload this data should consume
  a shared host definition rather than duplicate ad-hoc structs per sample.

### Locked Error Surface: `GfxError`
`GfxError` must expose deterministic machine-stable codes and human-readable
`why()` text. For the v1 spinning-cube path, these code identifiers are locked:

- `window_create_failed`
- `device_create_failed`
- `swapchain_create_failed`
- `mesh_create_failed`
- `pipeline_create_failed`
- `material_create_failed`
- `frame_acquire_failed`
- `queue_submit_failed`
- `frame_present_failed`

Sample-owned native/Metal hosts should route any host-side diagnostic emission
through one shared mapping of these identifiers rather than re-declare them in
each sample host.

### Locked Fallible Usage Rule
Examples and conformance for this mini-spec must use `Result` propagation (`?`)
plus `on_error<...>` handlers. `.expect(...)`-style unwrap helpers are not part
of this v1 contract.

## Locked Scene Renderer and UI Producer Contract (vNext)
This contract locks the renderer boundary before runtime scene-renderer code
exists. A custom-drawn UI is a scene producer, not a special-purpose renderer:
it owns rect/layout/state/event behavior, while `/std/scene` or an equivalent
scene surface owns the renderer-facing object model.

### Scene Boundary
The initial scene surface is defined by these stable concepts:
- `Scene`: insertion-ordered node storage, root/child relationships, one active
  camera, the fixed first UI light rig, and deterministic traversal metadata.
- `Node`: stable node id, parent id, local `Transform`, painter-order ordinal,
  optional local-z/depth metadata, primitive descriptor reference, and material
  reference.
- `Transform`: local translation/scale/rotation metadata; the first UI path
  consumes translation in logical-pixel scene units and leaves full transform
  composition to later renderer slices.
- `Camera`: a projection mode plus projection config. Orthographic projection
  is the first supported mode, represented as `Camera` config rather than a
  separate public `OrthographicCamera` type.
- `Material`: owns base color and opacity for a primitive. SDF geometry and
  coverage do not carry implicit colors, and differently colored commands do
  not smooth-blend implicit materials.
- `Light`: renderer-owned lighting data. The first UI scene uses only the fixed
  ambient-plus-key rig described below.
- Primitive descriptors: flat rects, rounded rects, text overlays, 2D SDF
  coverage primitives, explicit-material 3D SDF primitives, and future mesh or
  image descriptors.

The first source-level implementation lives in `stdlib/std/scene/scene.prime`.
It is an authoring and inspection model only: `Scene.serialize()` emits a stable
integer record stream for golden tests, `Camera` stores orthographic projection
config and UI pixel mapping, and materials, lights, primitives, nodes, local
`z`, painter order, and transforms receive stable ids without producing pixels.
The first renderer helper lives in `examples/shared/scene_bgra8_renderer.h` and
consumes that serialized `/std/scene` record stream directly. It renders flat
rect/plane records, rounded-rect 2D SDF coverage, and the first raised 3D SDF
button/slab primitive into a validated `SoftwareSurfaceFrame` BGRA8 buffer with
deterministic source-over composition, target-bound clipping, fixed global
lighting, and the documented painter-order/local-z/stable-node ordering.

### UI Coordinate and Camera Defaults
The first UI camera is orthographic. Its default viewport maps one scene unit to
one logical pixel. UI scene coordinates use a top-left origin, `+x` points
right, `+y` points down, the UI base plane is `z=0`, and positive local `z`
raises visual geometry toward the viewer. UI hit testing remains rect/layout
based; visual rendering may use 2D or 3D SDF scene primitives without changing
the event/hit-test contract.

### Material, SDF, Text, and Light Defaults
- Materials own color. Initial material defaults are: base color from the
  primitive/UI state, opacity `1.0`, shade strength `1.0`, no texture slots, and
  no implicit material interpolation across SDF blends.
- 2D SDFs produce coverage masks painted source-over in deterministic scene
  order. 3D SDFs may blend geometry, depth, and normals only when each SDF has
  an explicit material assignment.
- The first 3D SDF widget primitive is `primitive_sdf_button()`: a shallow
  beveled button/slab in widget-local rect space. Its default fixture uses a
  `4` logical-pixel bevel radius, `3` logical-pixel normal depth, and `1`
  logical-pixel pressed depth. Pressed state changes explicit depth through the
  node transform rather than blending material colors.
- Text remains a 2D overlay/primitive in the first UI scene path, not a 3D SDF
  or mesh problem. The renderer helper now exposes a deterministic shaped glyph
  run path for UTF-8 decoding, script/direction runs, combining-mark
  attachment, fallback fixture fonts, stable metrics, fixture glyph coverage,
  deterministic atlas packing, and BGRA8 source-over composition. The first
  presented UI text path keeps international shaping, bidi ordering, fallback
  fonts, glyph metrics, atlas coverage, and raster output behind that wrapper
  boundary.
- Native text dependencies stay behind renderer-owned wrapper APIs:
  HarfBuzz-class shaping, FreeType-class glyph loading/rasterization, and an
  ICU/FriBidi-class Unicode bidi/boundary service. PrimeStruct source APIs must
  not expose third-party text-library types.
- The first global UI light rig is fixed: ambient weight `0.55`, key weight
  `0.45`, key direction from upper-left/front, no shadows, no stochastic
  sampling, and no author lights.

### Deterministic Ordering
For UI presentation, painter order emitted by the UI producer is primary. Local
`z` is deterministic tie/depth metadata unless a future explicit depth group
opts in to different behavior. Within the same painter-order group, lower local
`z` is behind higher local `z`; equal-depth ties resolve by stable node id, then
primitive sub-order. Source-over 2D coverage, 3D SDF evaluation, and text
overlay emission must all preserve this total order.

### UI Producers
The first `/std/ui` prototype (`stdlib/std/ui/ui.prime`: a command list, a two-pass layout tree, basic widgets, an HTML
adapter, an input event stream and a login form) had no user and was retired in TODO-5711. Native windows use
platform widgets through `/std/ui/native` and its handlers (docs/NativeUiPlan.md), and web pages use `/std/web`
(docs/WebAppsPlan.md); neither goes through the scene renderer. A custom-drawn UI producer for this contract has yet to
be written. The `--software-surface-ui-demo` mode of the native window host and the macOS Metal host still renders a
fixed scene-record fixture through `examples/shared/ui_scene_surface_bridge.h` into the same BGRA8 upload and
presenter path as `--software-surface-demo`.

## Locked Constraints and Conformance Hooks

- `GFX-CORE-API-NAMESPACE`: Graphics API surface is rooted at `/std/gfx/*` and does not use
  backend-specific language namespaces. Conformance hook:
  `graphics_api_contract_doc_linked_constraints` (doc lock + unsupported backend emit check).
- `GFX-CORE-SURFACE-V1`: The v1 core object model names listed in this document are locked.
  Conformance hook: `graphics_api_contract_doc_linked_constraints` (doc lock checks).
- `GFX-CORE-NO-EXT-NS`: `/std/gfx/ext/*` is forbidden in v1. Conformance hook:
  `graphics_api_contract_doc_linked_constraints` (compile diagnostic check).
- `GFX-PROFILE-IDENTIFIERS`: Profile identifiers are locked to `wasm-browser`,
  `native-desktop`, `metal-osx`. Conformance hook:
  `graphics_api_contract_doc_linked_constraints` (profile value rejection check).
- `GFX-PROFILE-GATING`: Unsupported profile features fail at compile time before backend
  emission. Conformance hook:
  `graphics_api_contract_doc_linked_constraints` (wasm-browser reject check).
- `GFX-DIAG-PROFILE-CONTEXT`: Profile-gated failures must include deterministic profile/backend
  context in diagnostics. Conformance hook:
  `graphics_api_contract_doc_linked_constraints` (diagnostic content check).
- `GFX-V1-MINISPEC-SIGNATURES`: Spinning-cube mini-spec function/method signatures and object
  ownership shape are locked. Conformance hook:
  `graphics_api_contract_doc_linked_constraints` (doc lock checks).
- `GFX-V1-PROFILE-DEDUCTION`: Device creation is compile-target/profile deduced (no source
  profile literals required). Conformance hook:
  `graphics_api_contract_doc_linked_constraints` (profile-literal rejection/absence checks).
- `GFX-V1-VERTEXCOLORED-LAYOUT`: `/std/gfx/VertexColored` wire layout
  (offsets/size/alignment) is locked. Conformance hook:
  `graphics_api_contract_doc_linked_constraints` (layout lock checks).
- `GFX-V1-ERROR-CODES`: `GfxError` code identifiers for v1 spinning-cube stages are
  deterministic and locked. Conformance hook:
  `graphics_api_contract_doc_linked_constraints` (diagnostic code set checks).
- `GFX-V1-RESULT-PROPAGATION`: v1 fallible graphics examples/conformance use `?` +
  `on_error<...>` handler flow. Conformance hook:
  `graphics_api_contract_doc_linked_constraints` (doc lock checks).

## Non-goals (v1)
- No `/std/gfx/ext/*` backend extension namespace.
- No requirement for parity with advanced backend-specific features.
- No expansion to additional backend profile names without contract update and
conformance tests.
- No guarantee of advanced material/shader reflection in v1.

## Change Control
Any change to this contract requires:
1. Update this document (including locked IDs or rationale).
2. Update doc-linked conformance tests for every affected locked constraint.
3. Update `docs/todo.md` with explicit follow-up work if implementation is
split across multiple PRs.
