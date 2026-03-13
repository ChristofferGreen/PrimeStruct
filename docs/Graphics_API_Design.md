# PrimeStruct Graphics API Design

Status: Locked (v1 Core Contract + Spinning-Cube Mini-Spec)
Last updated: 2026-03-06

This document defines the locked cross-backend graphics API contract for the
spinning-cube program family. The goal is to keep one portable language surface,
apply profile-gated validation at compile time, and avoid backend-specific
language namespaces in v1.

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

### Locked Fallible Usage Rule
Examples and conformance for this mini-spec must use `Result` propagation (`?`)
plus `on_error<...>` handlers. `.expect(...)`-style unwrap helpers are not part
of this v1 contract.

## Planned Layered UI/Software Renderer (vNext, Non-Locking)
The following architecture is planned but not locked as part of the v1 contract:

1. Base renderer layer:
   - Consumes a flat draw-command list (`draw_text`, `draw_rounded_rect`, etc.).
   - The initial stdlib prototype lives under `/std/ui/*` with:
     - `Rgba8`
     - `CommandList`
     - `CommandList.draw_text(...)`
     - `CommandList.draw_rounded_rect(...)`
     - `CommandList.push_clip(...)`
     - `CommandList.pop_clip()`
     - `CommandList.clip_depth()`
     - `CommandList.serialize() -> vector<i32>`
   - Rounded rectangles are expressed through SDF-style primitives.
   - Renders into a software color buffer (or shared surface view).
2. Layout layer:
   - Two-pass layout contract:
     - Bottom-up measure pass (children -> parent).
     - Top-down arrange pass (parent -> children).
3. Basic widget layer:
   - Small primitive controls built on top of layout + draw commands.
4. Composite widget layer:
   - Higher-level widgets composed from basic widgets only.
5. Presentation and backend adapters:
   - Native path: software-rendered/shared buffer can be presented through the
     platform presenter (for example Metal).
   - Web path: UI tree/layout can also be emitted as HTML/CSS + event bindings.
6. Platform input adapter:
   - Normalizes OS/web input (pointer, keyboard, IME, resize, focus) into one
     UI event stream consumed by the UI runtime.

Current prototype serialization format for `/std/ui/CommandList.serialize()`:

- First word: format version (`1`)
- Second word: total command count
- Then, for each command in insertion order:
  - opcode
  - payload word count
  - payload words
- Current opcodes:
  - `1` = `draw_text`
  - `2` = `draw_rounded_rect`
  - `3` = `push_clip`
  - `4` = `pop_clip`
- Clip-stack rule:
  - `push_clip(...)` increments `clip_depth()` and emits one command.
  - `pop_clip()` decrements `clip_depth()` and emits one command only when the
    depth is non-zero.
  - `pop_clip()` at depth `0` is a deterministic no-op: no command is emitted
    and `clip_depth()` remains `0`.

Example:

```prime
import /std/ui/*

[effects(heap_alloc), return<int>]
main() {
  [CommandList mut] commands{CommandList()}
  commands.draw_rounded_rect(
    2i32,
    4i32,
    30i32,
    40i32,
    6i32,
    Rgba8([r] 12i32, [g] 34i32, [b] 56i32, [a] 255i32)
  )
  commands.draw_text(
    7i32,
    9i32,
    14i32,
    Rgba8([r] 255i32, [g] 240i32, [b] 0i32, [a] 255i32),
    "Hi!"utf8
  )
  [vector<i32>] words{commands.serialize()}
  return(commands.command_count())
}
```

This section still describes architecture direction first; the prototype API
above is intentionally narrow and will expand as clipping, layout, widgets, and
presentation layers are implemented.

## Locked Constraints and Conformance Hooks

| ID | Locked Constraint | Conformance Hook |
| --- | --- | --- |
| `GFX-CORE-API-NAMESPACE` | Graphics API surface is rooted at `/std/gfx/*` and does not use backend-specific language namespaces. | `graphics_api_contract_doc_linked_constraints` (doc lock + unsupported backend emit check) |
| `GFX-CORE-SURFACE-V1` | The v1 core object model names listed in this document are locked. | `graphics_api_contract_doc_linked_constraints` (doc lock checks) |
| `GFX-CORE-NO-EXT-NS` | `/std/gfx/ext/*` is forbidden in v1. | `graphics_api_contract_doc_linked_constraints` (compile diagnostic check) |
| `GFX-PROFILE-IDENTIFIERS` | Profile identifiers are locked to `wasm-browser`, `native-desktop`, `metal-osx`. | `graphics_api_contract_doc_linked_constraints` (profile value rejection check) |
| `GFX-PROFILE-GATING` | Unsupported profile features fail at compile time before backend emission. | `graphics_api_contract_doc_linked_constraints` (wasm-browser reject check) |
| `GFX-DIAG-PROFILE-CONTEXT` | Profile-gated failures must include deterministic profile/backend context in diagnostics. | `graphics_api_contract_doc_linked_constraints` (diagnostic content check) |
| `GFX-V1-MINISPEC-SIGNATURES` | Spinning-cube mini-spec function/method signatures and object ownership shape are locked. | `graphics_api_contract_doc_linked_constraints` (doc lock checks) |
| `GFX-V1-PROFILE-DEDUCTION` | Device creation is compile-target/profile deduced (no source profile literals required). | `graphics_api_contract_doc_linked_constraints` (profile-literal rejection/absence checks) |
| `GFX-V1-VERTEXCOLORED-LAYOUT` | `/std/gfx/VertexColored` wire layout (offsets/size/alignment) is locked. | `graphics_api_contract_doc_linked_constraints` (layout lock checks) |
| `GFX-V1-ERROR-CODES` | `GfxError` code identifiers for v1 spinning-cube stages are deterministic and locked. | `graphics_api_contract_doc_linked_constraints` (diagnostic code set checks) |
| `GFX-V1-RESULT-PROPAGATION` | v1 fallible graphics examples/conformance use `?` + `on_error<...>` handler flow. | `graphics_api_contract_doc_linked_constraints` (doc lock checks) |

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
