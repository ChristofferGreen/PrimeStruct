# PrimeStruct Coding Guidelines

Status: Draft
Last updated: 2026-03-06

This document defines style conventions for user-facing PrimeStruct examples.
The goal is readability-first surface syntax while preserving deterministic
semantics after canonicalization.

## Core Style
- Prefer surface return annotations: `[T] name(...) { ... }`.
- Prefer inferred parameters in examples when the meaning is clear:
  `wrap_angle(angle)` instead of `wrap_angle([f32] angle)`.
- Prefer infix operators in surface code (`a + b`, `x >= y`) over canonical
  helper-call forms (`plus(a, b)`, `greater_equal(x, y)`).
- Prefer snake_case function names for user code and standard APIs.
- Prefer snake_case local binding names in surface examples.
- Prefer omitted local envelopes when initializer inference is unambiguous
  (`value{Type(...)}` instead of `[Type] value{Type(...)}`); use `[mut]` when
  a local must be writable.
  This is the default preferred local-binding style for surface examples.
  Example: `pass{RenderPass(...)}` is preferred over
  `[RenderPass] pass{RenderPass(...)}`.
- Prefer object-owned methods and constructors over free-function style APIs
  (`window.is_open()`, `pass.draw_mesh(...)`, `swapchain.present(...)`).
- For collection examples, prefer C++-style member/index syntax:
  `value.push(x)`, `value.at(i)`, `value[i]`, `value.count()`.
  Use free-function helper spellings (`push(value, x)`, `at(value, i)`) only when
  demonstrating canonical post-transform forms.
- Keep examples aligned with the ownership model documented in `docs/PrimeStruct.md`:
  treat `array<T>`, `string`, `Pointer<T>`, and `Reference<T>` as language-core;
  treat `Maybe<T>`, `vector<T>`, `map<K, V>`, and planned `soa_vector<T>` as
  stdlib-facing surfaces; and treat `Result<T, Error>`, `File<Mode>`, `Buffer<T>`,
  plus `/std/gfx/*` as hybrid surfaces with minimal builtin substrate.
- Prefer unsuffixed numeric literals in surface examples (`1280`, `0.0166667`).
- Prefer explicit math-family conversions; never rely on implicit scalar/vector/matrix/quaternion coercions.
- Prefer `mat * vec` ordering (column-vector convention) and keep transform-composition order explicit at call sites.
- Normalize quaternions before repeated composition or vector rotation (`q.toNormalized()` or `q.normalize()`).
- Prefer exact `==`/`!=` only for invariants and integer-like checks; use explicit tolerance helpers for
  float/vector/matrix/quaternion comparisons.
- Prefer typed enums/descriptors over stringly-typed GPU config
  (`PresentMode.Fifo` over `"fifo"`).
- Prefer labeled arguments on public API calls to keep call sites self-documenting,
  but omit labels when passing a variable with the same name
  (`draw_mesh(mesh, material)` not `draw_mesh([mesh] mesh, [material] material)`).
- `main` must return `int` or `void`; do not declare `main` with `Result<...>`
  return envelopes.
- Keep app asset data (for example cube vertices/indices) in app code, not in
  generic stdlib helpers.
- Prefer canonical graphics wire structs when available (for example
  `/std/gfx/VertexColored`) instead of ad-hoc per-example vertex structs.
- Top-level bindings are not allowed; represent reusable asset payloads as helper
  definitions (for example `cube_vertices()`) or initialize them inside `main`.
- Prefer compile-target profile deduction over source-level profile literals.
  Canonical `/std/gfx/*` contract example: `device{Device()?}` is preferred
  over `device{Device([profile] "...")?}`.
  Treat `/std/gfx/*` as a hybrid surface: keep the public API shape in `.prime`
  wherever practical, while device/window/present substrate stays in host/runtime
  code.
  Current `/std/gfx/experimental/*` status: it is now a legacy compatibility
  shim over canonical `/std/gfx/*`, so the legacy wrapper imports remain
  available only for targeted compatibility coverage while new public gfx
  authority lives under the canonical namespace. The
  constructor-shaped `Window(...)` and `Device()` entry points now rewrite through
  substrate-backed helpers there, the fallible `create_swapchain(...)`,
  `create_mesh(...)`, and `frame()` wrapper paths now route through
  substrate-backed configs/helpers, the non-Result `render_pass(...)` /
  `draw_mesh(...)` / `end()` path now routes through minimal pass-encoding
  substrate helpers with deterministic zero-token / no-op fallback on invalid
  handles, the shared spinning-cube native-window sample path now emits and
  consumes a deterministic canonical `/std/gfx/*` stream
  (`cubeStdGfxEmitFrameStream` via `--gfx`), real compile-run conformance now
  imports canonical `/std/gfx/*` and exercises that public path across
  exe/vm/native, while separate compatibility-shim coverage keeps
  `/std/gfx/experimental/*` pinned for residual legacy imports, and
  `Device.create_pipeline([vertex_type] VertexColored, ...)` now rewrites
  through the matching pipeline helper. The canonical `/std/gfx/*` entry
  points now mirror that same helper-backed slice in `.prime`, with the
  constructor/resource/frame/pass/submit/present wrappers now routing through
  a private canonical `GraphicsSubstrate` helper layer there, and also have
  real compile-run conformance across exe/vm/native. Canonical and
  experimental gfx imports now reject deterministically on wasm
  (`wasm-browser`, `wasm-wasi`) and shader-only (`glsl`, `spirv`) emits until
  those targets gain runtime substrate, while source-level profile literals
  and unsupported pipeline vertex types are still intentionally rejected, the
  native/Metal sample hosts now share one canonical host-side `GfxError` +
  `VertexColored` contract header, the native window launcher now delegates to
  a shared canonical gfx launch helper, the native window host runtime shell
  now lives in a shared presenter helper, the Metal launcher now delegates to
  a shared metal launch helper, the Metal offscreen runtime shell now lives in
  a shared helper, the Metal snapshot/parity helper modes now also bind to a
  shared spinning-cube simulation reference helper, the browser launcher now
  delegates to a shared browser launch helper, the browser runtime shell now
  lives in a shared JS helper, and broader backend conformance remains staged.
- Status snapshot phrases kept contiguous for doc-lock checks:
  Current `/std/gfx/experimental/*` status: the constructor-shaped `Window(...)` and `Device()` entry points now rewrite
  through substrate-backed helpers there, the fallible `create_swapchain(...)`, `create_mesh(...)`, and `frame()`
  wrapper paths now route through substrate-backed configs/helpers, the non-Result `render_pass(...)` / `draw_mesh(...)`
  / `end()` path now routes through minimal pass-encoding substrate helpers with deterministic zero-token / no-op
  fallback on invalid handles, the shared spinning-cube native-window sample path now emits and consumes a deterministic
  canonical `/std/gfx/*` stream (`cubeStdGfxEmitFrameStream` via `--gfx`), real compile-run conformance now imports
  canonical `/std/gfx/*` while separate compatibility-shim coverage keeps `/std/gfx/experimental/*` pinned for residual
  legacy imports, and
  `Device.create_pipeline([vertex_type] VertexColored, ...)` now rewrites through the matching pipeline helper.
  The canonical `/std/gfx/*` entry points now mirror that same helper-backed slice in `.prime` and also have real
  compile-run conformance across exe/vm/native.
  The native/Metal sample hosts now share one canonical host-side `GfxError` + `VertexColored` contract header, the
  native window launcher now delegates to a shared canonical gfx launch helper, the native window host runtime shell now
  lives in a shared presenter helper, the Metal launcher now delegates to a shared metal launch helper, the Metal
  snapshot/parity helper modes now also bind to a shared spinning-cube simulation reference helper, the browser launcher
  now delegates to a shared browser launch helper, and the browser runtime shell now lives in a shared JS helper.
- Prefer `Result` propagation with `?` plus `on_error<...>` handlers over
  ad-hoc unwrap helpers.
  Canonical `/std/gfx/*` contract example: `window{Window(...) ?}` with
  `on_error<GfxError, /log_gfx_error>`.
- Do not use `.expect(...)` in PrimeStruct examples; model fallible flows with
  `?` and explicit handlers.

## Collection Surface Example (Compilable)

```prime
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{1, 2}
  [map<i32, i32>] pairs{map<i32, i32>{7=10}}
  values.push(3)
  values.reserve(8)
  return(values[0] + values.at(2) + values.count() + at(pairs, 7))
}
```

Use the wildcard collections import when an example depends on bare helper
names or method sugar. Concise vector bindings such as
`[vector<T>] values{1, 2}` are now part of the verified example surface, and
the explicit `vector<T>{...}` constructor form remains available when an
example wants to emphasize constructor spelling. Exact `import
/std/collections/vector` is acceptable for bare `vector(...)` construction plus
canonical `/std/collections/vector/*` helpers.

## Gold-Standard Surface Example (Pure PrimeStruct)

Note: this example locks a proposed API shape for graphics/math naming. Current
stdlib in this repo ships vector/color/matrix/quaternion nominal math types,
and the documented matrix/quaternion operator/runtime subset is now lowered on
VM/native/Wasm/C++ plus GLSL. The `/std/gfx/*` rendering surface shown below
(including `VertexColored`) now has an initial helper-backed `.prime`
implementation, but still needs broader backend/runtime coverage on top of the
current unsupported-backend diagnostics.

```prime
import /std/math/*
import /std/gfx/*

[f32]
wrap_angle(angle) {
  [mut] wrapped{angle}
  tau{6.2831853}

  while(wrapped >= tau) {
    wrapped = wrapped - tau
  }

  while(wrapped < 0.0) {
    wrapped = wrapped + tau
  }

  return(wrapped)
}

[Mat4]
cube_mvp(angle, aspect) {
  model{mat4_from_axis_angle(Vec3(0.0, 1.0, 0.0), angle)}
  view{
    mat4_look_at(
      [eye] Vec3(2.4, 1.7, 2.7),
      [target] Vec3(0.0, 0.0, 0.0),
      [up] Vec3(0.0, 1.0, 0.0)
    )
  }
  proj{mat4_perspective(0.7853982, aspect, 0.01, 100.0)}
  return(mat4_mul(proj, mat4_mul(view, model)))
}

[array<VertexColored>]
cube_vertices() {
  return(array<VertexColored>{
    VertexColored([position] Vec4(-1.0, -1.0, -1.0, 1.0), [color] ColorRGBA(0.0, 0.0, 0.0, 1.0)),
    VertexColored([position] Vec4( 1.0, -1.0, -1.0, 1.0), [color] ColorRGBA(1.0, 0.0, 0.0, 1.0)),
    VertexColored([position] Vec4( 1.0,  1.0, -1.0, 1.0), [color] ColorRGBA(1.0, 1.0, 0.0, 1.0)),
    VertexColored([position] Vec4(-1.0,  1.0, -1.0, 1.0), [color] ColorRGBA(0.0, 1.0, 0.0, 1.0)),
    VertexColored([position] Vec4(-1.0, -1.0,  1.0, 1.0), [color] ColorRGBA(0.0, 0.0, 1.0, 1.0)),
    VertexColored([position] Vec4( 1.0, -1.0,  1.0, 1.0), [color] ColorRGBA(1.0, 0.0, 1.0, 1.0)),
    VertexColored([position] Vec4( 1.0,  1.0,  1.0, 1.0), [color] ColorRGBA(1.0, 1.0, 1.0, 1.0)),
    VertexColored([position] Vec4(-1.0,  1.0,  1.0, 1.0), [color] ColorRGBA(0.0, 1.0, 1.0, 1.0))
  })
}

[array<i32>]
cube_indices() {
  return(array<i32>{
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4,
    0, 4, 7, 7, 3, 0,
    1, 5, 6, 6, 2, 1,
    3, 2, 6, 6, 7, 3,
    0, 1, 5, 5, 4, 0
  })
}

[effects(io_err)]
/log_gfx_error([GfxError] err) {
  print_line_error(err.why())
}

[return<int> effects(gpu_dispatch, io_err)
 on_error<GfxError, /log_gfx_error>]
main() {
  window{
    Window(
      [title] "PrimeStruct Spinning Cube",
      [width] 1280,
      [height] 720
    )?
  }

  device{Device()?}
  queue{device.default_queue()}

  swapchain{
    device.create_swapchain(
      window,
      [color_format] ColorFormat.Bgra8Unorm,
      [depth_format] DepthFormat.Depth32F,
      [present_mode] PresentMode.Fifo
    )?
  }

  mesh{
    device.create_mesh(
      [vertices] cube_vertices(),
      [indices] cube_indices()
    )?
  }

  pipeline{
    device.create_pipeline(
      [shader] ShaderLibrary.CubeBasic,
      [vertex_type] VertexColored,
      [color_format] ColorFormat.Bgra8Unorm,
      [depth_format] DepthFormat.Depth32F
    )?
  }

  material{pipeline.material()?}

  [mut] angle{0.0}
  dt{0.0166667}

  while(window.is_open()) {
    window.poll_events()

    frame{swapchain.frame()?}
    aspect{window.aspect_ratio()}
    mvp{cube_mvp(angle, aspect)}
    material.set_mvp(mvp)

    pass{
      frame.render_pass(
        [clear_color] ColorRGBA(0.05, 0.07, 0.10, 1.0),
        [clear_depth] 1.0
      )
    }

    pass.draw_mesh(mesh, material)
    pass.end()

    frame.submit(queue)?
    frame.present()?

    angle = wrap_angle(angle + (1.1 * dt))
  }

  return(0)
}
```
