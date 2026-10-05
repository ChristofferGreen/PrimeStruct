# Language Design Highlights

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **normative**.

## Language Design Highlights
- **Identifiers:** `[A-Za-z_][A-Za-z0-9_]*` plus the slash-prefixed form `/segment/segment/...` for fully-qualified
  paths. Unicode may arrive later, but identifiers are constrained to ASCII for predictable tooling and hashing. `auto`,
  `mut`, `return`, `import`, `namespace`, `true`, `false`, `if`, `else`, `loop`, `while`, and `for` are reserved
  keywords; any other identifier (including slash paths) can serve as a transform, path segment, parameter, or binding.
-- **String literals:** surface forms accept `"..."utf8` / `"..."ascii` with escape processing, or raw `'...'utf8` /
`'...'ascii` with no escape processing. The `implicit-utf8` text transform (enabled by default) appends `utf8` when
omitted in surface syntax. **Canonical/bottom-level form uses double-quoted strings with escapes normalized and an
explicit `utf8`/`ascii` suffix.** `ascii` enforces 7-bit ASCII (the compiler rejects non-ASCII bytes). Example surface:
`"hello"utf8`, `'moo'ascii`. Example canonical: `"hello"utf8`. Raw example: `'C:\temp'ascii` keeps backslashes verbatim.
- **Numeric envelopes:** fixed-width `i32`, `i64`, `u64`, `f32`, and `f64` map directly to hardware instructions and are
  the only numeric envelopes supported across all backends today. Software numeric envelopes `integer`, `decimal`, and
  `complex` are accepted by the parser/semantic validator (bindings, returns, collections, and `convert<T>` targets),
  but current backends reject them at lowering/emission time. Mixed software/fixed arithmetic is rejected, and mixed
  software categories or ordered comparisons on `complex` are also diagnostics today.
- **Core type set (v1):** the closed set of envelopes that every backend must understand and that the type system treats
  as cross-backend portable is:
  - `bool`, `i32`, `i64`, `u64`, `f32`, `f64`, `string`
  - `array<T>`, `vector<T>`, `map<K, V>`
  - `Pointer<T>`, `Reference<T>`
  - User-defined struct types (including `[struct]`-tagged definitions)
  - Draft math extension types (`Vec2`, `Vec3`, `Vec4`, `Mat2`, `Mat3`, `Mat4`, `Quat`) are currently backend-specific
    and are not part of this portable core set.
  Backends may accept additional types, but any type outside this core set is backend-specific and must be rejected by
  backends that do not explicitly support it. For collections, element/key/value types must themselves be in the core
  set unless a backend explicitly documents wider support.
- **Aliases:** none for numeric widths; use explicit `i32`, `i64`, `u64`, `f32`, `f64`.
- **`auto` (implicit templates + local inference):** `auto` may appear on binding envelopes, parameters, or return
  transforms. In parameter/return positions it introduces an implicit template parameter (equivalent to adding a fresh
  type parameter) and is inferred per call site; omitted parameter envelopes are treated as `auto`. In bindings, `auto`
  requests local inference from the initializer and must resolve to a concrete envelope; unresolved or conflicting local
  inference is a diagnostic. Return `auto` is constrained by return statements; if all constraints resolve to a concrete
  envelope the definition becomes monomorphic.
  - Float literals accept standard decimal forms, including optional fractional digits (e.g., `1.`, `1.0`, `1.f32`,
    `1.e2`).
- **Envelope:** the canonical AST uses a single envelope form for definitions and executions: `[transform-list]
  identifier<template-list>(parameter-or-arg-list) {body-list}`. Surface definitions require an explicit `{...}` body
  and may spell transforms either in canonical prefix form (`[transform-list] name(...) { ... }`) or in surface
  post-parameter form (`name(...) [transform-list] { ... }`), which is normalized to the prefix form before semantic
  validation/lowering. The post-parameter form is definition-only and requires `[]` to be followed by `{...}`. `name[]()
  { ... }` is rejected to avoid indexing-like ambiguity. Surface executions are call-style
  (`identifier<template-list>(arg-list)`) and map to an envelope with an implicit empty body. **Definitions may omit an
  empty parameter list** (e.g., `Foo { ... }` is treated as `Foo() { ... }`), and this is accepted even at the concrete
  level; executions still require parentheses. Bindings use the form `[Envelope qualifiers…] name{initializer}`. In
  inference/surface levels, locals and struct fields may omit the envelope annotation when the initializer resolves to
  one concrete envelope; unresolved or ambiguous inference is a diagnostic. Struct field envelopes must be concrete
  before layout manifest emission. Parameters that omit an explicit envelope are treated as `auto` and become implicit
  template parameters inferred per call site. Lists recursively reuse whitespace-separated tokens.
  - Syntax markers: `[]` compile-time transforms, `<>` compile-time arguments/templates, `()` runtime
    parameters/calls, `{}` runtime code (definition bodies, binding initializers).
  - `[...]` enumerates metafunction transforms applied in order (see “Built-in transforms”).
  - **Bracket-list name binding rule:** `[...]` is contextual. Entries already known in the current syntactic position
    keep their transform, type-envelope, modifier, or label meaning. When the grammar expects a binding pattern, fresh
    identifiers inside the bracket list introduce new names. This preserves existing forms such as `[i32] count{0i32}`
    and `[mut] value{0i32}` while allowing tuple destructuring forms such as `[left right] pair`; mixed or ambiguous
    lists must be diagnosed deterministically rather than silently reinterpreted.
  - `<...>` is the compile-time argument channel. Existing source forms use it
    for type arguments and non-negative integer arguments; the AST preserves
    typed argument metadata and reserves symbol/future categories for later
    primitives such as `typeof<value>` rather than pretending those are runtime
    calls.
  - `(...)` lists runtime parameters; explicit calls always include `()` (even with no args), and `()` never appears on
    bindings. Bare zero-argument execution allows `name` to execute a unique visible zero-argument definition in
    command/value position when no stack value, import alias, or other visible callable makes the name ambiguous.
  - **Parameters:** use the same binding envelope as locals: `main([array<string>] args, [i32] limit{10i32})`.
    Qualifiers like `mut`/`copy`/`move` apply here as well and choose how the argument is passed (a read-only borrow
    by default; see Parameter Passing in `value-lifecycle.md`); defaults are optional and currently limited to literal/pure forms
    (no name references). The key/value collection constructors (`mapNew`, `map<K, V>(...)`) and the public
    `Map<K, V>` wrapper constructors (`Map<K, V>{}`, `mapSingle`, `mapPair`) are also accepted as defaults even though
    they allocate; other allocating calls are rejected.
  - `{...}` holds runtime code for definition bodies and value blocks for binding initializers. Binding initializers
    evaluate the block and use its resulting value (last item or `return(value)`); value construction uses brace
    constructor forms, including multi-field construction (e.g., `[T] name{T{arg1, arg2}}`). `Type(...)` is ordinary
    execution/call syntax and is not construction.
  - Bindings are only valid inside definition bodies or parameter lists; top-level bindings are rejected.
- **Draft variadic argument packs (parser + call semantics + read-only body API + spread call-lowering landed;
  backend/runtime materialization is now partial):** to support stdlib-owned `vector`/`map` implementations without
  hand-written `Single/Pair/Triple/...` constructor ladders, the surface syntax now parses rest parameters and spread
  calls while keeping the canonical meaning inside the envelope system.
  - Surface parameter sugar: `collect(values...) { ... }` now desugars during parsing to
    `collect<__pack_T>([args<__pack_T>] values) { ... }`.
  - Typed surface parameter sugar: `collect([string] values...) { ... }` now desugars during parsing to
    `collect([args<string>] values) { ... }`.
  - Surface call sugar: `build(values...)` inside a call now desugars to `[spread] values` on that argument node, and
    explicit canonical `[spread] values` is accepted directly in call-argument position.
  - Canonical parser form therefore uses a real pack envelope plus an explicit spread marker instead of storing syntax
    in bare identifier spelling. Call semantics now bind trailing positional arguments into that trailing pack, infer
    omitted pack element types homogeneously across packed values, reject named arguments targeting the variadic
    parameter directly, and allow a final `[spread] values` argument to forward an existing `args<T>` pack into another
    trailing variadic slot with the same omitted-pack inference path. The read-only body API is now available, while
    backend/runtime materialization remains partial and should get a new explicit TODO before further implementation
    work starts.
  - After monomorphisation, bottom-level form contains no templates. Example:
    - Surface:
      ```text
      collect(values...) { return(vector(values...)) }
      main() { [auto] xs{collect(1i32, 2i32, 3i32)} }
      ```
    - Canonical envelope form:
      ```text
      [return<vector<i32>>]
      collect__i32([args<i32>] values) {
        return(vector__i32([spread] values))
      }

      [return<void>]
      main() {
        [vector<i32>] xs{collect__i32(1i32, 2i32, 3i32)}
      }
      ```
  - Current v1 constraints: one `args<T>` parameter per definition, it must be last, it is homogeneous, named arguments
    bind only fixed parameters, and `[spread]` is only valid in call-argument position.
  - Current body API: `count(values)`, `values.count()`, `values[index]`, `at(values, index)`, `values.at(index)`, and
    `values.at_unsafe(index)` work on `args<T>` parameters.
  - Runtime status: the legacy C++ emitter now materializes concrete `args<T>` parameters for direct variadic calls plus
    mixed explicit-prefix + final-spread forwarding, and that emitted path executes the full read-only body API
    including downstream method resolution on indexed values. IR-backed VM/native lowering now covers direct
    numeric/bool/string variadic calls, pure final-spread forwarding of an existing pack, and mixed explicit-prefix +
    final-spread forwarding rebuilt from known-size numeric/bool/string packs through the same array-like body API,
    including indexed downstream string helpers. Struct packs now also materialize for direct calls plus pure/mixed
    forwarding across `count(...)`, checked/unchecked access, and downstream indexed field/helper resolution. `Result<T,
    Error>` packs now use the same IR storage across direct/pure/mixed forwarding and preserve indexed `Result.why(...)`
    / `?` behavior on VM/native, status-only `Result<Error>` packs preserve indexed `Result.error(...)` /
    `Result.why(...)` behavior across those same forwarding modes, `FileError` packs preserve indexed downstream `why()`
    mapping, `Reference<FileError>` packs preserve indexed downstream `dereference(...).why()` mapping,
    `Pointer<FileError>` packs preserve indexed downstream `dereference(...).why()` mapping, `Reference<Result<T,
    Error>>` packs preserve indexed downstream `dereference(...)`, `try(...)`, and `Result.why(...)` access, status-only
    `Reference<Result<Error>>` packs preserve indexed downstream `dereference(...)`, `Result.error(...)`, and
    `Result.why(...)` access, `Pointer<Result<T, Error>>` packs preserve indexed downstream `dereference(...)`,
    `try(...)`, and `Result.why(...)` access including payload-kind inference for `auto` bindings on indexed `try(...)`
    results, and status-only `Pointer<Result<Error>>` packs preserve indexed downstream `dereference(...)`,
    `Result.error(...)`, and `Result.why(...)` access. `File<Mode>` packs preserve indexed downstream file-handle method
    access, `Reference<File<Mode>>` packs preserve indexed downstream file-handle method access alongside explicit
    `dereference(...).write*` / `flush()` receiver forms plus helper-style `at(values, i).write*()` /
    `values.at(i).flush()` receivers, canonical free-builtin `at([values] values, [index] i).write*()` / `.flush()`
    receivers, and direct indexed `readByte(...)` `?` inference plus canonical free-builtin `at([values] values,
    [index] i).readByte(...)` `?`, `Pointer<File<Mode>>` packs preserve indexed downstream file-handle method access
    alongside explicit `dereference(...).write*` / `flush()` receiver forms plus helper-style `at(values, i).write*()` /
    `values.at(i).flush()` receivers, canonical free-builtin `at([values] values, [index] i).write*()` / `.flush()`
    receivers, and direct indexed `readByte(...)` `?` inference plus canonical free-builtin `at([values] values,
    [index] i).readByte(...)` `?`, `Buffer<T>` packs preserve indexed downstream `buffer_load(...)` and
    `buffer_store(...)` on the IR/VM GPU path, `Reference<Buffer<T>>` packs preserve indexed downstream
    `buffer_load(dereference(...), ...)` and `buffer_store(dereference(...), ...)` on that same IR/VM GPU path,
    `Pointer<Buffer<T>>` packs preserve indexed downstream `buffer_load(dereference(...), ...)` and
    `buffer_store(dereference(...), ...)` on that same IR/VM GPU path, `array<T>`, `Reference<array<T>>`,
    `Pointer<array<T>>`, `vector<T>`, `Reference<vector<T>>`, `Pointer<vector<T>>`, empty/header-only `soa<T>`,
    `Reference<soa<T>>`, `Pointer<soa<T>>`, `map<K, V>`, `Reference<map<K, V>>`, plus `Pointer<map<K, V>>`
    packs now preserve indexed downstream `count()` resolution across the same forwarding modes, `vector<T>` packs also
    preserve indexed downstream `capacity()` and statement-mutator access, borrowed/pointer array and vector packs
    preserve explicit indexed `dereference(...)` receiver wrappers for downstream checked/unchecked access,
    borrowed/pointer vector packs preserve that same indexed `capacity()` and statement-mutator surface through explicit
    `dereference(...)` receiver wrappers, borrowed/pointer map packs preserve that same count and lookup surface through
    explicit indexed `dereference(...)` receiver wrappers, those same value, borrowed, and pointer map packs preserve
    indexed downstream `tryAt(...)` payload-kind inference for `auto` bindings, and `Pointer<map<K, V>>` packs preserve
    indexed downstream `contains()` / `at()` / `at_unsafe()` lookup access. Scalar `Pointer<T>` plus scalar
    `Reference<T>` packs now preserve indexed downstream `dereference(...)`, and struct `Pointer<T>` plus struct
    `Reference<T>` packs now preserve indexed downstream field/helper access. Any newly discovered unsupported
    non-string pack element should get a concrete TODO only after a reproducible semantics, lowering, or backend
    failure is identified.
    Latest checkpoint: canonical free-builtin `at([values] values, [index] i)` on wrapped borrowed/pointer `File<Mode>`
    arg-packs now preserves `write*()` / `flush()` receivers plus `readByte(...)` `?` inference across direct calls
    plus pure/mixed spread forwarding, while wrapped `FileError` free-builtin named access remains on the existing
    named-argument rejection path.
- **Draft heterogeneous type packs:** generic definitions may declare a final
  type-pack parameter such as `Tuple<Ts...>`. Monomorphized specializations
  expand `[Ts...] values` into deterministic fields named
  `__pack_values_0`, `__pack_values_1`, and so on. A specialized helper can
  write `Ts[I]` in type positions to select the `I`th concrete type from the
  bound pack, and `pack_at<I, values>(receiver)` rewrites during
  monomorphization to the matching generated field access. `I` must resolve to
  a non-negative integer template argument; out-of-range indexes and scalar
  non-pack operands are semantic diagnostics. This substrate is intentionally
  lowerer-neutral: after specialization, ordinary field access and field-borrow
  paths carry the selected storage slot.
- **Stdlib tuple:** `/std/tuple/*` exposes the public lower-case
  `tuple<Ts...>` type as a stdlib-owned heterogeneous product. The
  implementation is a single `.prime` generic struct with `[Ts...] values`,
  not a generated or hand-written fixed-arity tuple family. Imported
  `tuple<>{}`, `tuple<T>{...}`, and `tuple<T0, T1, ...>{...}` use ordinary
  brace construction and field lifecycle rules. Imported
  `get<I, Ts...>(value)` returns the precise `I`th element type by rewriting
  through generic `pack_at<I, values>(value)` during monomorphization.
  Tuple bracket indexing, such as `pair[0]`, is accepted as tuple-specific
  compile-time sugar over the same stdlib `get<I, Ts...>` helper path; runtime
  index variables still belong to ordinary collection indexing, not tuple
  element selection. Imported `make_tuple(...)` infers a heterogeneous
  `Ts...` pack from positional values and constructs the same ordinary
  `tuple<Ts...>` storage, including `make_tuple()` as `tuple<>`; named pack
  arguments and homogeneous `args<T>` spread forwarding are rejected.
  Tuple destructuring, such as `[left right] pair`, is accepted for named
  ordinary tuple values and lowers to the same `get<I, Ts...>` helper path in
  tuple order. Destructuring rejects non-tuple operands, arity mismatches,
  duplicate names, mixed type/modifier entries, and borrowed tuple operands.
  Imported `get_ref<I, Ts...>(location(value))` type-checks as
  `Reference<TI>` when borrowed field access is available, but direct native
  runtime dereference of returned tuple element references is still treated as
  a backend support boundary rather than a tuple-specific opcode. Tuple arity
  and element order are available through the same generated field/reflection
  metadata used by pack-expanded structs. Multi-wait integration remains
  follow-up sugar over this stdlib tuple surface.
- **Definitions vs executions:** definitions include a body (`{…}`) and optional transforms; executions are call-style
  (`execute_task<…>(args)`) with mandatory parentheses and no body, and map to an envelope with an implicit empty body.
  Calls always use `()`; the `name{...}` form is reserved for bindings so `execute_task{...}` is invalid.
  - Executions accept the same argument syntax as calls, including labeled arguments (`[param] value`).
  - Nested forms inside execution arguments still follow their own rules (e.g., labeled entries are valid in
    `array<i32>{[first] 1i32}` only if that collection form explicitly supports them).
  - Example: `execute_task([items] array<i32>{1i32, 2i32} [pairs] /std/collections/map/map<i32, i32>(1i32, 2i32))`.
  - **Definition order:** call sites may reference definitions that appear later in the same file or namespace. Name
    resolution runs after import expansion and namespace expansion; unresolved names remain diagnostics.
  - **Helper overloading:** non-struct definitions may reuse the same public helper path when their exact parameter
    counts differ. Call resolution picks the exact-arity match before template specialization and method-call lowering.
    Same-path same-arity definitions may also coexist when their parameter-type signatures are pairwise distinct
    (an unbound template parameter counts as one generic marker); the call site then selects the candidate whose
    parameter types match the argument types exactly, with a specificity tie-break when several candidates remain
    viable: a concrete parameter type beats an unbound template parameter in the same position, and a candidate wins
    only if it is at least as specific in every position and strictly more specific in at least one. No viable
    candidate is a compile-time `no viable overload for ...` diagnostic; incomparable viable candidates are a
    compile-time `ambiguous call to ...` diagnostic. Same-path same-arity definitions with the same parameter-type
    signature are still duplicates (unless disambiguated by `require<...>` constraints, which keep their existing
    behavior). Minimal examples:
    ```text
    [return<i32>] /helper/value<T>([T] value) { return(1i32) }
    [return<i32>] /helper/value<A, B>([A] first, [B] second) { return(2i32) }
    [return<i32>] /helper/insert([string] path) { return(3i32) }
    [return<i32>] /helper/insert([PathKey] key) { return(4i32) }
    [return<int> effects(io_out)]
    main() {
      print_line(/helper/value(7i32))
      print_line(/helper/value(7i32, true))
      print_line(/helper/insert("by-path"))
      return(0i32)
    }
    ```
  - Note: current VM/native/GLSL/C++ emitters only generate code for definitions; top-level executions are
    parsed/validated but not emitted (tooling-only for now).
- **Return annotation:** definitions declare return envelopes via transforms (e.g., `[return<f32>] blend<…>(…) { … }`).
  Definitions return values explicitly (`return(value)`); the desugared form is always canonical.
- **Surface vs canonical:** surface syntax may omit the return transform or use `return<auto>` (or `[auto]` with
  `single_type_to_return`) and rely on inference; canonical/bottom-level syntax always carries an explicit concrete
  `return<T>` after monomorphisation, and the base-level tree contains no templates or `auto`. Example surface: `main()
  { return(0) }` → canonical: `[return<i32>] main() { return(0i32) }`. If all return paths yield no value,
  `return<auto>` resolves to `return<void>`.
- **Default convenience:** the `single_type_to_return` transform rewrites a single bare envelope in the transform list
  into `return<envelope>` (e.g., `[i32] main()` → `[return<i32>] main()`), and it is enabled by default (disable via
  `--no-semantic-transforms` or override the semantic transform list). If the bare envelope is `auto`, the transform
  yields `return<auto>` and inference resolves it later.
Array returns use `return<array<T>>` (or `[array<T>]` with `single_type_to_return`) and surface as `array` in the IR.
Struct returns use `return<StructName>` (or inference when the body returns a struct constructor/value); they surface as
`array` in the IR with the struct layout manifest attached.

Example:
```
[return<array<i32>>]
make_pair() {
  return(array<i32>{1i32, 2i32})
}
```

Expected IR (shape only):
```
module {
  def /make_pair(): array {
    return array(1, 2)
  }
}
```
- **Diverging definitions (`return<never>`):** a definition annotated `[return<never>]` promises it never returns to
  its caller (it aborts execution or loops forever). `never` is valid only as a `return<...>` argument — never in
  bindings, parameters, or template arguments. A `never` definition must not contain any `return` statement (including
  bare `return()`), and calling one as a statement terminates that control path for all-paths-return analysis: a
  non-void definition may end a branch (or its body, after at least one `return` elsewhere) with a call to a diverging
  helper instead of a `return`. The stdlib entry point is `/std/panic/panic([i32] code)`, which traps through the
  runtime bounds check and surfaces `code` in the trap diagnostics:
  ```
  import /std/panic/*

  [return<i32>]
  pick([i32] value) {
    if(value > 0) {
      return(value)
    }
    panic(value)
  }
  ```
  Backends lower `never` like `void` (no value is ever produced). v1 limitations: the compiler does not verify that a
  `never` body actually diverges (like C `_Noreturn`/C++ `[[noreturn]]`, that is the author's contract), divergence is
  not tracked through expression position (`[i32] x{panic(0)}` is invalid), and a non-void definition whose body
  contains no `return` statement at all is still rejected at parse time even if it ends in a diverging call.
- **Effects:** by default, definitions/executions start with `io_out` enabled so logging works without explicit
  annotations. Authors can override with `[effects(...)]` (e.g., `[effects(global_write, io_out)]`) or tighten to pure
  behavior by passing `primec --default-effects=none`. Standard library routines permit stdout/stderr logging via
  `io_out`/`io_err`; backends reject unsupported effects (e.g., GPU code requesting filesystem access). `primec
  --default-effects <list>` supplies the default effect set for any definition/execution that omits `[effects]`
  (comma-separated list; `default` and `none` are supported tokens). If `[capabilities(...)]` is present it must be a
  subset of the active effects (explicit or default). VM/native accept `io_out`, `io_err`, `heap_alloc`, `file_read`,
  `file_write`, `gpu_dispatch`, `task`, and `pathspace_*` effects (`pathspace_notify`, `pathspace_insert`,
  `pathspace_take`, `pathspace_bind`, `pathspace_schedule`); `file_write` also implies `file_read` for compatibility.
  GLSL accepts
  `io_out`, `io_err`, plus `pathspace_*` metadata effects/capabilities.
- **Execution effects:** executions may also carry `[effects(...)]`. The execution’s effects must be a subset of the
  enclosing definition’s active effects; otherwise it is a diagnostic. The default set is controlled by
  `--default-effects` in the compiler/VM.
- **Task effect prototype:** the first structured-concurrency surface uses `[effects(task)]` on definitions that request
  source-visible task work. The parser accepts `[spawn] f(...)` as an execution transform on call envelopes and parses
  `wait(task)` as the first task-handle join expression. The semantic pass publishes `Task<T>` binding facts for
  `[spawn] f(...)`, infers `wait(Task<T>) -> T` and multi-task `wait(...) -> tuple<...>`, requires the `task` effect
  for both operations, and rejects live task handles on return, double waits, task-handle escapes, and mutable/reference
  captures. VM/native lower the structured runtime slice by storing spawned call results in task handle bindings,
  lowering `wait(handle)` to return that stored result, and lowering multi-wait to ordinary stdlib tuple construction.
  Detached tasks, task groups, channels, scheduler controls, and true parallel scheduling remain future work.
