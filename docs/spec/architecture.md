# Goals and Proposed Architecture

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **implementation note**.

## Goals
- Single authoring language spanning gameplay/domain scripting, UI logic, automation, and rendering shaders.
- Emit high-performance C++ for engine integration, optional GLSL/SPIR-V targets via external toolchains, and bytecode
  for an embedded VM without diverging semantics.
- Share a consistent standard library (math, texture IO, resource bindings) across backends while preserving determinism
  for replay/testing.

## Proposed Architecture
- **Front-end parser:** C/TypeScript-inspired surface syntax with explicit envelope annotations, deterministic control
  flow, and explicit resource usage.
- **Transform pipeline:** ordered text transforms rewrite raw tokens before the AST exists; semantic transforms annotate
  the parsed AST before lowering. The compiler can auto-inject transforms per definition/execution (e.g., attach
  `operators` to every function) with optional path filters (`/std/math/*`, recurse or not) so common rewrites don’t
  have to be annotated manually. Transforms may also rewrite a definition’s own transform list (for example,
  `single_type_to_return`). The default text chain desugars infix operators, control-flow, assignment, etc.; the default
  semantic chain enables `single_type_to_return`. Projects can override via `--text-transforms` /
  `--semantic-transforms` or the auto-deducing `--transform-list`.
- **Intermediate representation:** envelope-tagged SSA-style IR shared by every backend (C++, GLSL, VM). Normalisation
  happens once; backends never see syntactic sugar.
- **Graphics contract:** the windowed graphics language surface and locked spinning-cube v1 mini-spec are defined in
  `docs/Graphics_API_Design.md` (`/std/gfx/*`, profile deduction, `VertexColored` wire layout, deterministic `GfxError`
  codes). Current implementation status: the contract and host/sample coverage exist, canonical `/std/gfx/*` is now the
  authoritative public gfx surface, and `/std/gfx/experimental/*` remains only as a legacy compatibility shim over
  that canonical helper layer rather than as part of the public gfx contract. The experimental path still keeps an explicit `.prime` `GraphicsSubstrate` token/config
  boundary for its legacy wrapper types, the legacy constructor-shaped experimental and canonical `Window(...)`,
  `Device()`, and `Buffer<T>(count)` compatibility entry points now rewrite onto matching stdlib helpers, canonical
  `Window(...)` and `Device()`
  now also route through a private `.prime` `GraphicsSubstrate.createWindow(...)` / `createDevice(...)` /
  `createQueue(...)` boundary inside `/std/gfx/*`, the experimental and canonical `create_swapchain(...)`,
  `create_mesh(...)`, `frame()`, and `Device.create_pipeline([vertex_type] VertexColored, ...)` wrapper paths now run
  through the same proven first-slice logic in `.prime`, canonical `/std/gfx/*` now also routes those fallible
  resource/frame/pipeline helpers through the same private `GraphicsSubstrate.createSwapchain(...)` / `createMesh(...)`
  / `createPipeline(...)` / `acquireFrame(...)` layer, the non-Result `render_pass(...)` / `draw_mesh(...)` / `end()`
  path now routes through minimal pass-encoding helpers with deterministic zero-token / no-op fallback on invalid
  handles, canonical `/std/gfx/*` now also routes `render_pass(...)`, `draw_mesh(...)`, `end()`, `submit(...)`, and
  `present()` through the matching private `GraphicsSubstrate.openRenderPass(...)` / `drawMesh(...)` /
  `endRenderPass(...)` / `submitFrame(...)` / `presentFrame(...)` layer, canonical and experimental `Buffer<T>` now
  also expose `.prime`-authored `count()`,
  `empty()`, `is_valid()`, `readback()`, compute-only `load(index)`, and compute-only `store(index, value)` plus the
  legacy constructor-shaped `Buffer<T>(count)` compatibility entry point and explicit slash-call `allocate<T>(count)` /
  `upload(...)` / `load(...)` / `store(...)` helpers so public buffer inspection, host-side allocation/readback/upload,
  and compute storage access no longer have to route directly through raw fields or builtin `/std/gpu/buffer(...)` /
  `/std/gpu/readback(...)` / `/std/gpu/upload(...)` / `/std/gpu/buffer_load(...)` / `/std/gpu/buffer_store(...)` call
  sites, status-only experimental and canonical gfx flows now use the same stdlib-owned `GfxError.status(err)` helper
  layer as other `Result<Error>` surfaces instead of hand-packing `err.code`, the shared spinning-cube native-window
  sample path now emits a deterministic canonical `/std/gfx/*` stream (`cubeStdGfxEmitFrameStream`) that the macOS host
  and launcher consume via `--gfx` so submit/present can drive one real window end-to-end, the native launcher script
  itself is now only a thin wrapper over a shared canonical gfx launch helper, the native window host runtime shell now
  also lives in one shared presenter helper instead of staying embedded in the spinning-cube sample file, the Metal
  sample launcher now also delegates to one shared metal launch helper while its offscreen runtime shell lives in one
  shared helper instead of staying embedded in `metal_host.mm`, the Metal sample’s snapshot/parity helper modes now also
  bind to one shared spinning-cube simulation reference helper instead of carrying their own inline fixed-step copy, the
  browser sample launcher now also delegates to one shared browser launch helper while its bootstrap/runtime shell now
  lives in `examples/web/shared/browser_runtime_shared.js` instead of staying embedded in `main.js`, real compile-run
  conformance now imports canonical `/std/gfx/*` and exercises that end-to-end wrapper path across exe/vm/native while
  separate compatibility-shim tests keep `/std/gfx/experimental/*` pinned for residual legacy imports, canonical and
  experimental gfx imports now reject deterministically on wasm (`wasm-browser`,
  `wasm-wasi`) and shader-only (`glsl`, `spirv`) emits until those targets gain runtime substrate, host-side sample
  `GfxError` mapping plus locked `VertexColored` upload layout definitions now live in one shared example header instead
  of being duplicated per macOS host, bare explicit bindings of Result-returning gfx wrappers still fail
  deterministically during semantics, unsupported `create_pipeline` vertex types now reject deterministically instead of
  degrading into generic compiler errors. No active TODO currently tracks broader backend/runtime package-path cleanup.
  Add a concrete TODO before changing that graphics backend/runtime seam; current spinning-cube execution
  therefore still mixes shared `.prime` simulation with browser/native/Metal host glue outside the fully canonical
  package path.
- **Layered UI/rendering roadmap:** the first `/std/ui/*` foundation now includes deterministic command-list rendering,
  a two-pass layout tree contract, basic control emission, a basic panel container primitive, and the first composite
  widget helper, plus deterministic HTML/backend adapter records and deterministic platform input records
  (`CommandList`, `draw_text`, `draw_rounded_rect`, `push_clip`, `pop_clip`, `draw_label`, `draw_button`, `draw_input`,
  `begin_panel`, `end_panel`, `draw_login_form`, `HtmlCommandList`, `emit_panel`, `emit_label`, `emit_button`,
  `emit_input`, `bind_event`, `emit_login_form`, `UiEventStream`, `push_pointer_move`, `push_pointer_down`,
  `push_pointer_up`, `push_key_down`, `push_key_up`, `push_ime_preedit`, `push_ime_commit`, `push_resize`,
  `push_focus_gained`, `push_focus_lost`, `LayoutTree`, `LoginFormNodes`, `UiScene`, `UiSceneNodes`,
  `UiSceneTextOverlays`, `append_root_column`, `append_column`, `append_leaf`, `append_label`, `append_button`,
  `append_input`, `append_panel`, `append_login_form`, `emit_scene_panel`, `emit_scene_label`, `emit_scene_button`,
  `emit_scene_panel_button`, `measure`, `arrange`, deterministic `serialize()` output), and the current host bridge can
  blit a deterministic BGRA8 software
  surface through the native window presenter and macOS Metal host paths. The `--software-surface-ui-demo` mode now
  renders the checked-in PrimeStruct-authored `UiScene` fixture through `ui_scene_surface_bridge.h` into the same BGRA8
  presenter path while the shared widget/layout model can also
  emit deterministic HTML/backend adapter records and normalize pointer, keyboard, IME, resize, and focus input into
  deterministic UI event-stream records. The scene renderer boundary is now locked as a UI producer contract rather
  than a UI-specific software renderer: `/std/ui/*` owns rect/layout/state/event logic and now emits deterministic
  `UiScene` scene records plus `UiSceneTextOverlays` records for panel, label, and raised button presentation,
  `/std/scene` owns renderer-facing `Scene`, `Node`, `Transform`, `Camera`, `Material`, `Light`, and
  primitive descriptor concepts, and `/std/ui/CommandList.serialize()` remains a stable adapter path. The first
  source-level `/std/scene` model is data-only: it authors and serializes stable ids, parent-before-child node order,
  painter order, local `z`, local transform metadata, orthographic camera config, materials, lights, and primitive
  descriptors. The shared CPU BGRA8 renderer consumes that serialized `/std/scene` record stream from
  `examples/shared/scene_bgra8_renderer.h` and emits validated `SoftwareSurfaceFrame` output for flat rect/plane
  primitives, rounded-rect 2D SDF coverage, and the first globally lit 3D SDF button/slab primitive with deterministic
  source-over composition, target-bound clipping, fixed global lighting, and documented painter-order/local-z/stable-node
  ordering. The first UI scene camera is an orthographic `Camera` projection config that maps one scene unit
  to one logical pixel with a top-left origin, `+x` right, `+y` down, base plane `z=0`, and positive local `z` toward
  the viewer; painter order is primary, then local `z`, then stable node id and primitive sub-order. Materials own
  color with initial defaults of base color from primitive/UI state, opacity `1.0`, shade strength `1.0`, no texture
  slots, and no implicit interpolation across SDF blends. 2D SDFs provide source-over coverage, 3D SDFs require explicit
  material assignment, and the initial 3D SDF widget is `primitive_sdf_button()` with a `4` logical-pixel bevel radius,
  `3` logical-pixel normal depth, and `1` logical-pixel pressed depth. Text stays a 2D international overlay/primitive
  with deterministic shaped glyph runs for UTF-8 decoding, script/direction segmentation, combining marks, fallback-font
  selection, fixture atlas coverage, and BGRA8 source-over composition behind renderer-owned HarfBuzz-class,
  FreeType-class, and ICU/FriBidi-class wrappers, and the first UI light
  rig is fixed ambient-plus-key (`0.55` ambient, `0.45` upper-left/front key, no shadows, no stochastic sampling, no
  author lights). No active TODO currently tracks platform/runtime consumption of that shared event stream. Add a
  concrete TODO before changing that UI runtime seam; composite-widget composition remains locked to the basic
  widget/container APIs rather than raw draw-command helpers or raw HTML record append helpers.
- **IR definition (stable, PSIR v26):**
  - **Module:** `{ string_table, struct_layouts, functions, instruction_source_map, host_imports, entry_index, version }`.
    The canonical contract constants live in `include/primec/Ir.h` as `IrSchemaMagic`,
    `IrSchemaVersion`, and the supported-version range; serializer implementations
    must use those constants rather than private version literals.
  - **Function:** `{ name, metadata, parameter_count, local_debug_slots, instructions }` where instructions are
    linear, stack-based ops with immediates and debug IDs. `parameter_count` declares how many leading local slots
    a callee expects the caller to have populated at call time; it exists for static-analysis passes (stack-depth
    checkers, the wasm function-type signature) that need to reason about a callee without executing it. Always 0
    today because lowering still inlines every call rather than emitting `Call`/`CallVoid` targets.
  - **Metadata:** `{ effect_mask, capability_mask, scheduling_scope, instrumentation_flags }` (see PSIR binary layout).
  - **Instruction:** `{ op, imm, debug_id }`; `op` is an `IrOpcode`, `imm` is a 64-bit immediate payload whose meaning
    depends on `op`, and `debug_id` is a deterministic per-instruction identifier used for source-map linkage.
  - **Instruction source map entry:** `{ debug_id, line, column, provenance, source_unit }`; entries map instruction
    debug IDs back to canonical AST statement/expression coordinates when available (including inlined callee
    statements), the source unit/file identity when compile-pipeline expanded-source metadata is available, and
    provenance tags (`canonical_ast` for direct statement/expression mappings, `synthetic_ir` for compiler-generated
    instructions). Instructions with no direct AST origin currently fall back to definition coordinates and omit
    `source_unit` only when no source-unit ledger was supplied.
  - **Host import:** `{ name, parameter_kinds, return_kind }` with kinds `void|i32|i64|u64|f32|f64|bool|string` (`string` for parameters only; the VM hands the host a pointer to the module's string). `CallHost`
    (`imm` = index into `host_imports`) pops the declared parameters, calls the host function an embedder bound to that
    name, and pushes the result unless it returns void. Only the VM target accepts `CallHost`; validation rejects it for
    native, wasm, GLSL, and C++ targets, and VM debug sessions fault with a diagnostic. An embedder must bind every
    import with a matching signature or execution fails before the first instruction (see `docs/Embedding.md`).
  - **Locals:** addressed by index; `LoadLocal`, `StoreLocal`, `AddressOfLocal` operate on the index encoded in `imm`.
  - **Strings:** string literals are interned in `string_table` and referenced by index in print ops (see PSIR
    versioning).
  - **Entry:** `entry_index` points to the entry function in `functions`; its signature is enforced by the front-end.
  - **PSIR binary layout (little-endian):**
    - `u32 magic` (`0x50534952` = `"PSIR"`), `u32 version`, `u32 function_count`, `u32 entry_index`, `u32 string_count`.
    - `string_count` entries: `u32 byte_len` + raw bytes.
    - `u32 struct_count`.
    - `struct_count` entries: `u32 name_len` + name bytes, `u32 total_size`, `u32 alignment`, `u32 field_count`, then
      `field_count` entries:
      `u32 field_name_len` + bytes, `u32 envelope_len` + bytes, `u32 offset`, `u32 size`, `u32 alignment`,
      `u32 padding_kind`, `u32 category`, `u32 visibility`, `u32 is_static`.
    - `function_count` entries: `u32 name_len` + name bytes, `u64 effect_mask`, `u64 capability_mask`,
      `u32 scheduling_scope`, `u32 instrumentation_flags`, `u32 parameter_count`, `u32 local_debug_count`, then
      `local_debug_count` entries: `u32 slot_index`, `u32 name_len` + name bytes, `u32 type_len` + type bytes, then
      `u32 instruction_count` and `instruction_count` entries: `u8 opcode` + `u64 imm` + `u32 debug_id`.
    - `u32 instruction_source_map_count`, then `instruction_source_map_count` entries:
      `u32 debug_id`, `u32 line`, `u32 column`, `u8 provenance`, `u32 source_unit_len` + source-unit bytes.
    - `u32 host_import_count`, then `host_import_count` entries: `u32 name_len` + name bytes, `u32 parameter_count`,
      `parameter_count` x `u8 kind`, `u8 return_kind`.
  - **PSIR opcode set:** see the `IrOpcode` enum and the “PSIR opcode set (v22, VM/native)” section below.
- **PSIR versioning:** serialized IR includes a version tag; v2 introduces `AddressOfLocal`, `LoadIndirect`, and
  `StoreIndirect` for pointer/reference lowering; v4 adds `ReturnVoid` to model implicit void returns in the VM/native
  backends; v5 adds a string table + print opcodes for stdout/stderr output; v6 extends print opcodes with
  newline/stdout/stderr flags to support `print`/`print_line`/`print_error`/`print_line_error`; v7 adds `PushArgc` for
  entry argument counts in VM/native execution; v8 adds `PrintArgv` for printing entry argument strings; v9 adds
  `PrintArgvUnsafe` to emit unchecked entry-arg prints for `at_unsafe`; v10 adds `LoadStringByte` for string indexing in
  VM/native backends; v11 adds struct layout manifests; v12 adds struct field visibility/static metadata; v13 adds float
  arithmetic/compare/convert opcodes; v14 adds float return opcodes (`ReturnF32`, `ReturnF64`); v15 adds per-function
  execution metadata (effect/capability masks plus scheduling/instrumentation fields); v16 adds `Call` and `CallVoid`
  function-call opcodes for callable IR; v17 adds per-function local debug slot metadata (`slot_index`, `name`, `type`)
  without runtime semantic changes; v18 adds per-instruction debug IDs for source-map linkage; v19 adds per-instruction
  source-map metadata entries keyed by instruction debug ID (`line`, `column`, `provenance`); v20 adds `FileReadByte`
  for deterministic single-byte file reads with explicit EOF mapping and `HeapFree` for `/std/intrinsics/memory/free`;
  v21 adds `HeapRealloc` for `/std/intrinsics/memory/realloc`; v22 adds per-instruction source-unit/file identity to
  source-map metadata so VM debug lookup can disambiguate identical line/column positions across source units; v23
  adds a per-function `parameter_count` field so static-analysis passes can reason about a callee's expected argument
  count without executing it, in preparation for lowering to emit real `Call`/`CallVoid` targets instead of always
  inlining (TODO-4747); it is a pure schema/no-op addition, always 0 until that lowering work lands; v24 adds the
  `CallHost` opcode and the module `host_imports` table (VM-only host function calls for embedding; v23 bytecode is
  rejected and must be recompiled); v25 adds the `string` host value kind for host function parameters. v26 appends the VM-only `LoadStringByteDynamic` opcode for run-time (VM-owned) strings (TODO-5364). The same change fixed the deserializer's opcode upper bound, which previously
  stopped at `HeapRealloc` and could not load `FileWriteStringDynamic`.
  - **PSIR v2:** adds pointer opcodes (`AddressOfLocal`, `LoadIndirect`, `StoreIndirect`) to support
    `location`/`dereference`.
  - **PSIR v4:** adds `ReturnVoid` so void definitions can omit explicit returns without losing a bytecode terminator.
  - **Versioning policy:** the `version` field is a single, monotonically increasing integer for incompatible changes.
    There is no forward/backward compatibility guarantee today; tools reject unknown versions and require recompilation.
    Migration tooling may be added later, but no automatic migrations exist yet. Any change to the binary layout,
    opcode numbering/meaning, metadata encoding, or required module fields must bump `IrSchemaVersion`, update this
    versioning note with the migration expectation, and refresh the representative golden serialization fixture.
- **Backends:**
  - **C++ emitter** – generates host code for native binaries.
  - **GLSL emitter** – produces shader code; SPIR-V output is available via `--emit=spirv`.
  - **VM bytecode** – compact instruction set executed by the embedded interpreter/JIT.
- **Tooling:** CLI compiler `primec`, plus the VM runner `primevm` and build/test helpers. The compiler accepts `--entry
  /path` to select the entry definition (default: `/main`). Import search roots are configured with `--import-path
  <dir>` (or `-I <dir>`). Entry definitions currently accept either no parameters or a single `[array<string>]`
  parameter for command-line arguments; `args.count()` and `count(args)` are supported, and checked indexing is
  available via either `args[index]` or `args.at(index)` (`at_unsafe(args, index)` / `args.at_unsafe(index)` skips
  checks). String bindings may be initialised from checked/unchecked entry-arg indexing (print-only). The C++ emitter
  maps the array argument to `argv` and otherwise uses the same restriction. The definition/execution split maps cleanly
  to future node-based editors; full IDE/LSP integration is deferred until the compiler stabilises.
- **AST/IR dumps:** the debug printers include executions with their argument lists so tooling can capture scheduling
  intent in snapshots.
  - Dumps show collection literals after text-transform rewriting while preserving brace construction (e.g.,
    `array<i32>{1i32,2i32}` remains `array<i32>{1i32,2i32}`).
  - Labeled execution arguments appear inline (e.g., `exec /execute_task([count] 2)`).
