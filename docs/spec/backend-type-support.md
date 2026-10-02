# Backend Type Support

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **implementation note**.

### Backend Type Support (v1)
- **VM/native:** scalar `i32`, `i64`, `u64`, `bool`, `f32`, `f64`. `array`/`vector`/`map` support numeric/bool values;
  map string keys must be string literals or literal-backed bindings. Strings are limited to literals/literal-backed
  bindings for print/map contexts; string returns are supported for literal-backed values, while string arrays and
  string pointers/references are rejected. `convert<T>` supports `i32`, `i64`, `u64`, `bool`, `f32`, `f64`.
- **VM/native emitter restrictions (current):** recursive calls are rejected; lambdas are rejected (use the C++
  emitter); `equal`/`not_equal` on two strings lower to a real byte-by-byte comparison (reusing the same count()/at()
  builtins as the stdlib `/string/equal` helper), but ordered string comparisons (`less_than`, `greater_than`,
  `less_equal`, `greater_equal`) are still rejected, as is any comparison mixing a string operand with a non-string
  one; string literals are otherwise limited to print/count/index/map contexts; string
  array returns and string pointer/reference bindings are rejected; block arguments on non-control-flow calls and
  arguments on `if` branch blocks are rejected; `print*` and vector helper calls are statement-only; `File<Mode>(path)`
  requires a string literal or literal-backed binding; `Result.ok(value)` plus `Result.map(...)`,
  `Result.and_then(...)`, and `Result.map2(...)` currently accept `i32`, `bool`, `f32`, literal-backed `string`,
  ordinary user structs whose fields lower through the existing stack-backed struct path, the current single-slot
  int-backed stdlib error structs (`FileError`, `ImageError`, `ContainerError`, `GfxError`), packed `File<Mode>`
  handles, and `Buffer<T>` handles when downstream `try(...)` consumers are explicitly typed. Downstream `try(...)`
  preserves those handle/error-struct payloads alongside `array<T>` / `vector<T>` and `map<K, V>` handles whose element
  or key/value kinds already fit the current collection contract. IR-backed `[args<Result<T, Error>>]`,
  `[args<Reference<Result<T, Error>>>]`, and `[args<Pointer<Result<T, Error>>>]` packs now preserve indexed `try(...)`,
  `Result.error(...)`, and `Result.why(...)` access across direct, pure-spread, and mixed variadic forwarding when `T`
  already fits that same payload contract, and native executable `Result<Buffer<T>, GfxError>` values now preserve
  `try(...)`, `Result.error(...)`, and success/error `Result.why(...)` on that same explicit typed path. Unsupported
  math or GPU builtins fail.
- **GLSL:** numeric/bool scalar locals (`i32`, `i64`, `u64`, `bool`, `f32`, `f64`) plus nominal `Vec2`, `Vec3`, `Vec4`,
  `Quat`, `Mat2`, `Mat3`, and `Mat4` bindings; string literals and other non-supported composites are rejected, and
  entry definitions must return `void`. `convert<T>` targets match the numeric/bool list above.
- **GLSL emitter restrictions (current):** at most one `return()` statement; static bindings are rejected;
  assign/increment/decrement require local mutable targets; control flow must use canonical forms (`if(cond, then() {
  ... }, else() { ... })`, `loop(count, body() { ... })`, `while(cond, body() { ... })`, `for(init, cond, step, body() {
  ... })`); builtins require positional args with no template/block arguments, and unsupported builtins fail.
- **GLSL type support (current):** scalar `bool`, `i32`, `u32`, `i64`, `u64`, `f32`, `f64` plus nominal `Vec2`, `Vec3`,
  `Vec4`, `Quat`, `Mat2`, `Mat3`, and `Mat4`. Using `i64`/`u64` or `f64` emits
  `GL_ARB_gpu_shader_int64`/`GL_ARB_gpu_shader_fp64` requirements. Arrays, strings, general structs,
  pointers/references, maps, and other unsupported composites are rejected.
- **Matrix/quaternion status (draft):** VM/native, Wasm, and the C++ emitter support stdlib matrix/quaternion nominal
  values, conversion helpers, component-wise `Mat2`/`Mat3`/`Mat4` and `Quat` `plus` + `minus`, matrix/quaternion scalar
  scaling + divide, matrix-vector multiply, matching matrix-matrix multiply, quaternion Hamilton products, and
  quaternion-`Vec3` rotation. GLSL now lowers nominal `Vec2`/`Vec3`/`Vec4`, `Quat`, `Mat2`, `Mat3`, and `Mat4` values,
  direct vector/quaternion/matrix field access, component-wise vector/quaternion `plus`/`minus`, vector/quaternion
  scalar scale/divide, `MatN * VecN` interop, matching matrix-matrix multiply, quaternion Hamilton products,
  quaternion-`Vec3` rotation, and the explicit quaternion conversion helpers `quat_to_mat3`, `quat_to_mat4`, and
  `mat3_to_quat`.
- **GLSL effects/capabilities (current):** `io_out`, `io_err`, and `pathspace_*` metadata entries are accepted; other
  effects/capabilities are rejected. `print*` calls are accepted but emitted as no-op expressions.
- **GLSL determinism (current):** only local scalar plus nominal vector/matrix bindings are allowed; no static storage
  or heap/placement transforms. GPU backends are treated as deterministic with no external I/O.
- **GPU compute (draft):**
  - A definition tagged with `[compute]` is lowered as a GPU kernel. Kernels are `void` and write outputs via buffer
    parameters rather than return values.
  - `workgroup_size(x, y, z)` fixes the local group size for the kernel; only valid alongside `[compute]`.
  - Kernel bodies are restricted to the GPU-safe subset (POD/`gpu_lane` types, fixed-width numeric envelopes, no IO, no
    heap, no strings, no recursion). Backends reject unsupported features early with diagnostics.
  - Host-side submission uses `/std/gpu/dispatch(kernel, gx, gy, gz, args...)` and requires `effects(gpu_dispatch)`.
  - VM/native fallback currently requires `/std/gpu/buffer<T>(count)` to use a constant `i32` literal size.
  - GPU builtins live under `/std/gpu/*` (see Core library surface).
  - GPU ID helpers are scalar: `/std/gpu/global_id_x()`, `/std/gpu/global_id_y()`, `/std/gpu/global_id_z()` return
    `i32`.
- **Capability taxonomy (v1):**
  - **IO:** `io_out` (stdout), `io_err` (stderr), `file_read` (filesystem input), `file_write` (filesystem output; also
    implies `file_read`).
  - **Memory:** `heap_alloc` (dynamic allocation), `global_write` (mutating global state).
  - **Assets:** `asset_read`, `asset_write` (asset/database I/O).
  - **GPU:** `gpu_dispatch` (host-side GPU submission/dispatch).
  - **PathSpace (internal):** `pathspace_notify`, `pathspace_insert`, `pathspace_take`, `pathspace_bind`,
    `pathspace_schedule` (host metadata/event hooks; currently treated as backend metadata/no-op operations).
  - Unknown capability names are errors; capability identifiers are `lower_snake_case`.
- **Tooling vs runtime visibility:**
  - **Tooling surfaces:** declared effects/capabilities, resolved defaults, entry defaults, and backend allowlist
    violations (diagnostics).
  - **Runtime-only logs:** resolved effect/capability masks and execution identifiers (path hashes) for tracing; source
    spans are optional/debug-only.
- **Paths & imports:** every definition/execution lives at a canonical path (`/ui/widgets/log_button_press`). Authors
  can spell the path inline or rely on `namespace foo { ... }` blocks to prepend `/foo` automatically. Import expansion
  produces a single compilation unit; implicit-template inference may use call sites anywhere in that unit; there are no
  module boundaries. Import paths are parsed before text transforms, so they remain quoted without literal suffixes.
  - **Source imports:** `import<"/std/io", version="1.2.0">` resolves packages from the import path (zipped archive or
    plain directory) whose layout mirrors `/version/first_namespace/second_namespace/...`. The angle-bracket list may
    contain multiple quoted string paths—`import<"/std/io", "./local/io/helpers", version="1.2.0">`—and the resolver
    applies the same version selector to each path; mismatched archives raise an error before expansion. Versions live
    in the leading segment (e.g., `1.2/std/io/*.prime` or `1/std/io/*.prime`). If the version attribute provides one or
    two numbers (`1` or `1.2`), the newest matching archive is selected; three-part versions (`1.2.0`) require an exact
    match. Each `.prime` source file is expanded exactly once and registered under its namespace/path (e.g., `/std/io`);
    duplicate imports are ignored. Folders prefixed with `_` remain private.
    Legacy `include<...>` source imports are removed; use `import<...>` only.
    Legacy `--include-path` is also removed; configure import roots with
    `--import-path` (or `-I`).
  - **Namespace imports:** `import /foo/*` brings the immediate **public** children of `/foo` into the root namespace.
    `import /foo/bar` brings a single **public** definition (or builtin) by its final segment; importing a non-public
    definition is a diagnostic. `import /foo` is shorthand for `import /foo/*` (except `/std/math`, which is unsupported
    without `/*` or an explicit name). `import /std/math/*` brings all math builtins into the root namespace, or import
    a subset via `import /std/math/sin /std/math/pi`; `import /std/math` without a wildcard or explicit name is not
    supported. Imports are resolved after source imports and can be listed as `import /std/math/*, /util/*` or
    whitespace-separated paths.
  - **Exports:** definitions are private by default. Add `[public]` to a definition (function, struct, method) to make
    it importable; `[private]` explicitly marks it as non-exported. Private definitions are still callable within the
    same compilation unit; visibility only affects imports.
- **Transform-driven control flow:** control structures desugar into prefix calls that accept envelope arguments.
  Surface `if(condition) { … } else { … }` rewrites into `if(condition, then() { … }, else() { … })`. Surface
  `if(condition) { … }` (without `else`) is statement-only sugar and rewrites into `if(condition, then() { … }, else() {
  })`. `loop(count) { … }`, `while(condition) { … }`, and `for(init cond step) { … }` rewrite into `loop(count, do() { …
  })`, `while(condition, do() { … })`, and `for(init, cond, step, do() { … })`. `match(condition) { … } else { … }`
  rewrites into `match(condition, then() { … }, else() { … })` and behaves like `if` for boolean conditions. Value
  matching uses the canonical call form `match(value, case(pattern) { … }, case(other) { … }, else() { … })`; cases
  compare with `equal(...)`, and a final `else` block is required. The envelope names (`do`, `then`, `else`, `case`) are
  for readability only; any name is accepted and ignored by the compiler. Infix operators (`a + b`) become canonical
  calls (`plus(a, b)`), ensuring IR/backends see a small, predictable surface.
- **Mutability:** bindings are immutable by default. Opt into mutation by placing `mut` inside the stack-value execution
  or helper (`[Integer mut] exposure{42}`, `[mut] Create()`). Transforms enforce that only mutable bindings can serve as
  `assign` or pointer-write targets.
