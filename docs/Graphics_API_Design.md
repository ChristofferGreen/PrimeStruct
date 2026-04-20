# PrimeStruct Graphics API Design

Status: Locked (v1 Core Contract + Spinning-Cube Mini-Spec)
Last updated: 2026-03-18

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
     - `LayoutTree`
     - `LoginFormNodes`
     - `LayoutTree.append_root_column(...)`
     - `LayoutTree.append_column(...)`
     - `LayoutTree.append_leaf(...)`
     - `LayoutTree.append_label(...)`
     - `LayoutTree.append_button(...)`
     - `LayoutTree.append_input(...)`
     - `LayoutTree.append_panel(...)`
     - `LayoutTree.append_login_form(...)`
     - `LayoutTree.measure()`
     - `LayoutTree.arrange(x, y, width, height)`
     - `LayoutTree.serialize() -> vector<i32>`
     - `CommandList.draw_label(...)`
     - `CommandList.draw_button(...)`
     - `CommandList.draw_input(...)`
     - `CommandList.begin_panel(...)`
     - `CommandList.end_panel()`
     - `CommandList.draw_login_form(...)`
     - `HtmlCommandList`
     - `HtmlCommandList.emit_panel(...)`
     - `HtmlCommandList.emit_label(...)`
     - `HtmlCommandList.emit_button(...)`
     - `HtmlCommandList.emit_input(...)`
     - `HtmlCommandList.bind_event(...)`
     - `HtmlCommandList.emit_login_form(...)`
     - `HtmlCommandList.serialize() -> vector<i32>`
     - `UiEventStream`
     - `UiEventStream.push_pointer_move(...)`
     - `UiEventStream.push_pointer_down(...)`
     - `UiEventStream.push_pointer_up(...)`
     - `UiEventStream.push_key_down(...)`
     - `UiEventStream.push_key_up(...)`
     - `UiEventStream.push_ime_preedit(...)`
     - `UiEventStream.push_ime_commit(...)`
     - `UiEventStream.push_resize(...)`
     - `UiEventStream.push_focus_gained(...)`
     - `UiEventStream.push_focus_lost(...)`
     - `UiEventStream.event_count()`
     - `UiEventStream.serialize() -> vector<i32>`
   - Rounded rectangles are expressed through SDF-style primitives.
   - Renders into a software color buffer (or shared surface view).
   - Current host bridge prototype: the native window host and macOS Metal host
     can upload a deterministic BGRA8 software surface into a shared Metal
     texture and blit it through their presenter/output paths via
     `--software-surface-demo`.
2. Layout layer:
   - Two-pass layout contract:
     - Bottom-up measure pass (children -> parent).
     - Top-down arrange pass (parent -> children).
   - Current prototype contract:
     - Single-root flat tree; nodes are appended in parent-before-child
       insertion order.
     - `append_root_column(...)` creates the root column at node `0` with
       parent index `-1`.
     - `append_column(...)` appends a vertical stack container child.
     - `append_leaf(...)` appends a leaf node with fixed minimum size.
     - `measure()` walks reverse insertion order, so children are always
       measured before parents.
     - Leaf measurement is exactly `(min_width, min_height)`.
     - Column measurement is:
       - width = `max(child.measured_width) + 2 * padding`, clamped by
         `min_width`
       - height = `sum(child.measured_height) + gap * (child_count - 1) +
         2 * padding`, clamped by `min_height`
     - `arrange(x, y, width, height)` assigns the root rectangle, then walks
       insertion order to place children.
     - Column arrange stretches each child to the parent inner width and uses
       the child measured height as the arranged height.
3. Basic widget layer:
   - Small primitive controls built on top of layout + draw commands.
   - Current prototype contract:
     - `append_label(...)` creates a leaf whose measured width is
       `count(text) * ceil(text_size / 2)` and whose measured height is
       `text_size`.
     - `append_button(...)` creates a leaf whose measured size is the label
       text metrics plus `2 * padding` in both dimensions.
     - `append_input(...)` creates a leaf whose measured height matches the
       button formula and whose measured width is `max(min_width,
       text_width + 2 * padding)`.
     - `draw_label(...)` emits one `draw_text` command at the arranged node
       origin.
     - `draw_button(...)` emits one rounded rect followed by one text command.
     - `draw_input(...)` emits one rounded rect followed by one text command.
4. Basic widget container layer:
   - Current prototype contract:
     - `append_panel(...)` creates a column-backed container node using the
       existing layout measure/arrange rules.
     - `begin_panel(...)` emits one rounded rect for the panel background,
       then emits one `push_clip(...)` for the panel inner content rect.
     - The inner content rect is `panel_rect` inset by the layout padding on
       every side, with width/height clamped to `>= 0`.
     - `end_panel()` emits one balancing `pop_clip()`.
     - Child widget draw calls for that panel must appear between
       `begin_panel(...)` and `end_panel()` in command order.
5. Composite widget layer:
   - Higher-level widgets composed from basic widgets only.
   - Current prototype contract:
     - `append_login_form(...) -> LoginFormNodes` is the first composite widget
       helper and expands only through `append_panel`, `append_label`,
       `append_input`, and `append_button`.
     - `draw_login_form(...)` emits only through `begin_panel`, `draw_label`,
       `draw_input`, `draw_button`, and `end_panel`.
     - Composite widget helpers in this prototype must not call raw
       `draw_text`, `draw_rounded_rect`, `push_clip`, `pop_clip`, `append_leaf`,
       or `append_column` directly.
6. Presentation and backend adapters:
   - Native path: software-rendered/shared buffer can be presented through the
     platform presenter (for example Metal).
   - Web path: shared widget/layout model can also emit deterministic
     HTML/backend adapter records that a DOM/CSS/event layer can consume.
   - Current prototype contract:
     - `emit_panel(...)` emits one panel DOM/CSS record for the arranged node.
     - `emit_label(...)` emits one label DOM/CSS record for the arranged node.
     - `emit_button(...)` emits one button DOM/CSS record followed by one
       `bind_event(...)` click record.
     - `emit_input(...)` emits one input DOM/CSS record followed by one
       `bind_event(...)` input record.
     - `emit_login_form(...)` emits only through `emit_panel`, `emit_label`,
       `emit_input`, and `emit_button`.
     - Composite HTML adapter helpers in this prototype must not call raw
       `append_word`, `append_color`, or `append_string` directly.
7. Platform input adapter:
   - Normalizes OS/web input (pointer, keyboard, IME, resize, focus) into one
     UI event stream consumed by the UI runtime.
   - Current prototype contract:
     - `push_pointer_move(...)`, `push_pointer_down(...)`, and `push_pointer_up(...)` normalize through one pointer
       event record shape: target node id, pointer id, button, x, y.
     - `push_pointer_move(...)` uses button `-1` to mark non-button pointer
       motion while preserving the same payload layout as button transitions.
     - `push_key_down(...)` and `push_key_up(...)` normalize through one key event record shape: target node id, key
       code, modifier mask, is-repeat.
     - `push_ime_preedit(...)` and `push_ime_commit(...)` normalize through one IME event record shape: target node id,
       selection start, selection end, text.
     - `push_ime_commit(...)` uses selection start/end `-1` to mark committed text that no longer carries a live
       composition range.
     - `push_resize(...)`, `push_focus_gained(...)`, and `push_focus_lost(...)` normalize through one view event record
       shape: target node id, arg0, arg1.
     - `push_resize(...)` uses `arg0 = width` and `arg1 = height`.
     - `push_focus_gained(...)` and `push_focus_lost(...)` use `arg0 = 0` and `arg1 = 0`.
     - Current modifier mask bits are `1` = `shift`, `2` = `control`, `4` = `alt`, `8` = `meta`.

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

Current prototype serialization format for `/std/ui/HtmlCommandList.serialize()`:

- First word: format version (`1`)
- Second word: total command count
- Then, for each command in insertion order:
  - opcode
  - payload word count
  - payload words
- Current opcodes:
  - `1` = `emit_panel`
  - `2` = `emit_label`
  - `3` = `emit_button`
  - `4` = `emit_input`
  - `5` = `bind_event`
- Event kinds used by `bind_event(...)`:
  - `1` = `click`
  - `2` = `input`

Current prototype serialization format for `/std/ui/UiEventStream.serialize()`:

- First word: format version (`1`)
- Second word: total event count
- Then, for each event in insertion order:
  - event kind
  - payload word count
  - payload words
- Current event kinds:
  - `1` = `pointer_move`
  - `2` = `pointer_down`
  - `3` = `pointer_up`
  - `4` = `key_down`
  - `5` = `key_up`
  - `6` = `ime_preedit`
  - `7` = `ime_commit`
  - `8` = `resize`
  - `9` = `focus_gained`
  - `10` = `focus_lost`

Current prototype serialization format for `/std/ui/LayoutTree.serialize()`:

- First word: format version (`1`)
- Second word: total node count
- Then, for each node in insertion order:
  - kind
  - parent index
  - measured width
  - measured height
  - arranged x
  - arranged y
  - arranged width
  - arranged height
- Current node kinds:
  - `1` = `leaf`
  - `2` = `column`

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

Layout example:

```prime
import /std/ui/*

[effects(heap_alloc), return<int>]
main() {
  [LayoutTree mut] tree{LayoutTree()}
  [i32] root{tree.append_root_column(2i32, 3i32, 10i32, 4i32)}
  [i32] header{tree.append_leaf(root, 20i32, 5i32)}
  [i32] body{tree.append_column(root, 1i32, 2i32, 12i32, 8i32)}
  [i32] badge{tree.append_leaf(body, 8i32, 4i32)}
  [i32] details{tree.append_leaf(body, 10i32, 6i32)}
  [i32] footer{tree.append_leaf(root, 6i32, 7i32)}
  tree.measure()
  tree.arrange(10i32, 20i32, 50i32, 40i32)
  [vector<i32>] words{tree.serialize()}
  return(tree.node_count())
}
```

Basic control example:

```prime
import /std/ui/*

[effects(heap_alloc), return<int>]
main() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 2i32, 0i32, 0i32)}
  [i32] title{layout.append_label(root, 10i32, "Hi"utf8)}
  [i32] action{layout.append_button(root, 10i32, 3i32, "Go"utf8)}
  [i32] field{layout.append_input(root, 10i32, 2i32, 18i32, "abc"utf8)}
  layout.measure()
  layout.arrange(5i32, 6i32, 30i32, 46i32)

  [CommandList mut] commands{CommandList()}
  commands.draw_label(layout, title, 10i32, Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32), "Hi"utf8)
  commands.draw_button(
    layout,
    action,
    10i32,
    3i32,
    4i32,
    Rgba8([r] 10i32, [g] 20i32, [b] 30i32, [a] 255i32),
    Rgba8([r] 250i32, [g] 251i32, [b] 252i32, [a] 255i32),
    "Go"utf8
  )
  commands.draw_input(
    layout,
    field,
    10i32,
    2i32,
    3i32,
    Rgba8([r] 40i32, [g] 50i32, [b] 60i32, [a] 255i32),
    Rgba8([r] 200i32, [g] 210i32, [b] 220i32, [a] 255i32),
    "abc"utf8
  )
  return(commands.command_count())
}
```

Basic container example:

```prime
import /std/ui/*

[effects(heap_alloc), return<int>]
main() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 2i32, 0i32, 0i32)}
  [i32] panel{layout.append_panel(root, 2i32, 1i32, 20i32, 12i32)}
  [i32] action{layout.append_button(panel, 10i32, 2i32, "Go"utf8)}
  [i32] field{layout.append_input(panel, 10i32, 1i32, 16i32, "abc"utf8)}
  layout.measure()
  layout.arrange(4i32, 5i32, 28i32, 60i32)

  [CommandList mut] commands{CommandList()}
  commands.begin_panel(layout, panel, 4i32, Rgba8([r] 8i32, [g] 9i32, [b] 10i32, [a] 255i32))
  commands.draw_button(
    layout,
    action,
    10i32,
    2i32,
    3i32,
    Rgba8([r] 20i32, [g] 30i32, [b] 40i32, [a] 255i32),
    Rgba8([r] 200i32, [g] 201i32, [b] 202i32, [a] 255i32),
    "Go"utf8
  )
  commands.draw_input(
    layout,
    field,
    10i32,
    1i32,
    2i32,
    Rgba8([r] 50i32, [g] 60i32, [b] 70i32, [a] 255i32),
    Rgba8([r] 210i32, [g] 211i32, [b] 212i32, [a] 255i32),
    "abc"utf8
  )
  commands.end_panel()
  return(commands.command_count())
}
```

Composite widget example:

```prime
import /std/ui/*

[effects(heap_alloc), return<int>]
main() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  [LoginFormNodes] login{layout.append_login_form(
    root,
    2i32,
    1i32,
    10i32,
    1i32,
    2i32,
    16i32,
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )}
  layout.measure()
  layout.arrange(6i32, 7i32, 40i32, 57i32)

  [CommandList mut] commands{CommandList()}
  commands.draw_login_form(
    layout,
    login,
    10i32,
    1i32,
    2i32,
    4i32,
    3i32,
    Rgba8([r] 9i32, [g] 8i32, [b] 7i32, [a] 255i32),
    Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32),
    Rgba8([r] 20i32, [g] 30i32, [b] 40i32, [a] 255i32),
    Rgba8([r] 200i32, [g] 201i32, [b] 202i32, [a] 255i32),
    Rgba8([r] 50i32, [g] 60i32, [b] 70i32, [a] 255i32),
    Rgba8([r] 250i32, [g] 251i32, [b] 252i32, [a] 255i32),
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )
  return(commands.command_count())
}
```

HTML adapter example:

```prime
import /std/ui/*

[effects(heap_alloc), return<int>]
main() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  [LoginFormNodes] login{layout.append_login_form(
    root,
    2i32,
    1i32,
    10i32,
    1i32,
    2i32,
    16i32,
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )}
  layout.measure()
  layout.arrange(6i32, 7i32, 40i32, 57i32)

  [HtmlCommandList mut] html{HtmlCommandList()}
  html.emit_login_form(
    layout,
    login,
    10i32,
    1i32,
    2i32,
    4i32,
    3i32,
    Rgba8([r] 9i32, [g] 8i32, [b] 7i32, [a] 255i32),
    Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32),
    Rgba8([r] 20i32, [g] 30i32, [b] 40i32, [a] 255i32),
    Rgba8([r] 200i32, [g] 201i32, [b] 202i32, [a] 255i32),
    Rgba8([r] 50i32, [g] 60i32, [b] 70i32, [a] 255i32),
    Rgba8([r] 250i32, [g] 251i32, [b] 252i32, [a] 255i32),
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8,
    "user_input"utf8,
    "pass_input"utf8,
    "submit_click"utf8
  )
  return(html.commandCount)
}
```

Input adapter example:

```prime
import /std/ui/*

[effects(heap_alloc), return<int>]
main() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  [LoginFormNodes] login{layout.append_login_form(
    root,
    2i32,
    1i32,
    10i32,
    1i32,
    2i32,
    16i32,
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )}

  [UiEventStream mut] events{UiEventStream()}
  events.push_pointer_move(login.usernameInput, 7i32, 20i32, 30i32)
  events.push_pointer_down(login.submitButton, 7i32, 1i32, 20i32, 30i32)
  events.push_pointer_up(login.submitButton, 7i32, 1i32, 21i32, 31i32)
  events.push_key_down(login.usernameInput, 13i32, 3i32, 1i32)
  events.push_key_up(login.usernameInput, 13i32, 1i32)
  events.push_ime_preedit(login.usernameInput, 1i32, 4i32, "al|"utf8)
  events.push_ime_commit(login.usernameInput, "alice"utf8)
  events.push_resize(login.panel, 40i32, 57i32)
  events.push_focus_gained(login.usernameInput)
  events.push_focus_lost(login.usernameInput)
  return(events.event_count())
}
```

This section still describes architecture direction first; the prototype API
above is intentionally narrow and will expand as clipping, layout, widgets, and
presentation layers are implemented.

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
