# Literals and Composite Construction

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **normative**.

## Literals & Composite Construction
- **Numeric literals:** decimal, float, hexadecimal with optional width suffixes (`42i64`, `42u64`, `1.0f64`).
- Integer literals require explicit width suffixes (`42i32`, `42i64`, `42u64`) unless `implicit-i32` is enabled (on by
  default). Omit it from `--text-transforms` (or use `--no-text-transforms`) to require explicit suffixes.
- Float literals accept `f32` or `f64` suffixes; when omitted in surface syntax they default to `f32`. Canonical form
  requires `f32`/`f64`. Exponent notation (`1e-3`, `1.0e6f32`) is supported.
- Commas may appear between digits in the integer part as digit separators and are ignored for value (e.g., `1,000i32`,
  `12,345.0f32`). Commas are not allowed in the fractional or exponent parts, and `.` is the only decimal separator.
- **Strings:** double-quoted strings process escapes unless a raw suffix is used; single-quoted strings are raw and do
  not process escapes. `raw_utf8` / `raw_ascii` force raw mode on either quote style. Surface literals accept
  `utf8`/`ascii`/`raw_utf8`/`raw_ascii` suffixes; the canonical/bottom-level form uses double-quoted strings with
  normalized escapes and an explicit `utf8`/`ascii` suffix. `implicit-utf8` (enabled by default) appends `utf8` when
  omitted.
- **Boolean:** keywords `true`, `false` map to backend equivalents.
- **Composite constructors:** structured values are introduced through brace constructor forms only. Calls such as
  `ColorGrade(...)` are ordinary executions and do not construct values. Use `ColorGrade{[hue_shift] 0.1f32 [exposure]
  0.95f32}` or a context-typed binding initializer such as `[ColorGrade] grade{[hue_shift] 0.1f32 [exposure] 0.95f32}`.
  Labeled entries map to fields, and every field must have either an explicit entry or an envelope-provided default
  before validation.
  - **Defaults & validation:** struct constructors accept positional and labeled arguments. Missing fields are filled
    from their field initializers; if a field has no initializer, zero-arg construction requires a `Create()` helper to
    assign it. Extra arguments are a semantic error (`argument count mismatch`).
  - **Multi-expression blocks:** constructor braces hold constructor field entries. General binding initializer blocks
    still produce one value via `return(value)` or the final expression when they are not parsed as a constructor entry
    list.
- **Algebraic sum types (partial):** `[sum]` definitions declare named, field-like variants. Payload-carrying variants
  have one payload envelope; unit/no-payload variants use bare lowerCamelCase names such as `none`. The parser,
  validator, and semantic product preserve source-order tag metadata for both forms; unit variant metadata records
  `has_payload=false` and an empty payload type. Explicit `[variant] payload` construction is validated for typed
  bindings, return values, fields, and `auto` bindings when the sum type is named. Target-typed inferred construction
  is also validated for bindings, call/field arguments, and returns when exactly one payload-carrying variant accepts
  the payload. A sum value has exactly one active variant at runtime.
  ```prime
  [sum]
  Shape {
    none
    [Circle] circle
    [Rectangle] rectangle
    [Text] text
  }

  [Circle] circleA{[radius] 3.4}
  [Circle] circleB{3.4}
  [Circle] circleC{3.4}
  [Shape] a{[circle] circleA}
  [Shape] b{[circle] circleB}
  [Shape] c{circleC}
  ```
  The explicit `[circle]` form selects the variant directly. The inferred form (`[Shape] c{circleC}`) is valid only
  when exactly one `Shape` variant accepts a `Circle` payload. Zero matches are type errors and multiple matches are
  ambiguity errors requiring an explicit `[variant]` label.
  Unit variants can be constructed by default when the first declared variant is unit, or by naming the unit variant in
  a target-typed constructor:
  ```prime
  [sum]
  MaybeI32 {
    none
    [i32] some
  }

  [MaybeI32] a{}
  [MaybeI32] b{none}
  [MaybeI32] c{[some] 1i32}
  ```
  Generic sum declarations use the same template syntax as generic structs and helpers. Template parameters may appear
  in payload envelopes, and each concrete use is monomorphized before validation and semantic-product publication:
  ```prime
  [sum]
  Maybe<T> {
    none
    [T] some
  }

  [sum]
  Result<T, E> {
    [T] ok
    [E] err
  }

  [Maybe<i32>] a{}
  [Maybe<i32>] b{[some] 1i32}
  [Result<i32, string>] c{[ok] 2i32}
  ```
  Monomorphized sum metadata records the substituted payload type text, keeps source-order variant indices/tags, and
  rejects invalid template arity. Recursive inline payloads such as `Bad<T> { [Bad<T>] again }` are unsupported until
  recursive sum layout is designed. The default sum construction rule is valid only when the first declared variant is
  a unit variant; the default active variant is tag `0` in source order. Payload variants are not default-constructed
  implicitly, so `[Shape] value{}` is rejected when the first `Shape` variant carries a payload. Variant order is
  therefore layout/serialization-sensitive and should be treated as version-sensitive API surface. `Maybe<T>` uses this
  substrate, and imported value-carrying `Result<T, E>` construction is available through `/std/result/*`.
  `Result.ok(value)`, `Result.map(result, fn)`, `Result.and_then(result, fn)`, and
  `Result.map2(left, right, fn)` compatibility helpers can initialize typed imported value-carrying sum locals/returns
  on IR-backed VM/native paths with local imported Result sum sources or direct calls returning them. `try(...)`,
  `Result.error(...)`, and `Result.why(...)` can inspect imported value-carrying sum values directly or through dereferenced local
  `Reference<Result<T, E>>` / `Pointer<Result<T, E>>` values. Imported status-only `Result<E>` values can be
  constructed, used as `pick` targets, and consumed by IR-backed `try(...)`, `Result.error(...)`, and
  `Result.why(...)` from locals, direct calls, and dereferenced local `Reference<Result<E>>` /
  `Pointer<Result<E>>` sources.
  `pick(value) { variant(payload) { ... } }` is the semantically validated exhaustive pattern form. Payload variants
  require binders such as `circle(c) { ... }`; missing variants, duplicate variants, unknown variants, and incompatible
  branch value types are diagnostics. Unit variants use binder-free arms such as `none { ... }`, and payload binders on
  unit variants are rejected. IR-backed VM, native, and exe lowering currently execute scalar, unit, and struct-payload
  sums with an inline aggregate convention: slot 0 stores the payload-slot header, slot 1 stores the active variant tag,
  and slot 2 starts the active payload storage. Unit variants carry only the tag. Scalar payloads occupy one slot; struct
  payloads occupy their ordinary struct slot layout inline and `pick` arms bind a
  branch-local view of the selected payload only. Aggregate-valued `pick(...)` expressions copy the selected active
  payload into stable result storage before the value can be bound, returned, or passed to a helper, so inactive payload
  storage stays unobserved by the escape path. Native constructor and
  initializer selection consumes published sum-variant metadata before choosing
  payload storage on semantic-product-backed paths. Native `try(...)` lowering
  consumes published Result sum-variant metadata before matching payload shape,
  loading payload slots, copying propagated errors, or branching on tags.
  Sum-to-sum `move(...)` construction copies the active tag and routes only
  the selected aggregate payload through its `Move`/`Copy` helper when one exists, falling back to slot copy for
  helper-free payloads. Explicit `drop(storage)` for `uninitialized<Sum>` storage routes only the active aggregate
  payload through `DestroyStack`/`Destroy` when a payload helper exists; inactive payload storage is never observed or
  destroyed. Nested sum payloads remain unsupported until recursive sum layout is designed, so public examples should
  still avoid ownership-heavy nested struct-payload sums.
- **Labeled arguments:** labeled arguments use a bracket prefix (`[name] value`) and may be reordered (including on
  executions). Positional arguments fill the remaining parameters in declaration order, skipping labeled entries.
  Builtin calls (operators, comparisons, clamp, convert, pointer helpers, collections) do not accept labeled arguments.
  - Example: `sum3(1i32 [c] 3i32 [b] 2i32)` is valid.
  - Example: `array<i32>{[first] 1i32}` is rejected unless the collection constructor defines a `first` entry.
  - Duplicate labeled arguments are rejected for definitions and executions (`execute_task([a] 1i32 [a] 2i32)`).
  - Variadic-pack interaction: if a definition ends in `[args<T>] values`, positional arguments fill the fixed
    parameters first and the remaining positional arguments bind to `values`; named arguments do not target the
    `args<T>` parameter directly. A spread argument (`values...` surface, `[spread] values` canonical) is only legal in
    call-argument position and expands into that trailing variadic slot when the callee actually has one, and forwarded
    `args<T>` values now participate in omitted-pack inference. The body-side read-only API (`count(values)`,
    `values.count()`, `values[index]`, `at(values, index)`, `values.at(index)`, `values.at_unsafe(index)`) is now
    available; the legacy C++ emitter executes that API for concrete packs, while IR-backed VM/native now covers the
    direct numeric/bool/string pack slice plus pure and mixed-prefix pack forwarding from known-size sources,
    struct-pack materialization plus indexed field/helper access across direct/pure/mixed forwarding paths, `Result<T,
    Error>` packs with indexed `Result.why(...)` / `?` behavior across the same forwarding modes, status-only
    `Result<Error>` packs with indexed `Result.error(...)` / `Result.why(...)` behavior across those same forwarding
    modes, `FileError` packs with indexed downstream `why()` mapping, `Reference<FileError>` packs with indexed
    downstream `dereference(...).why()` mapping, and `Pointer<FileError>` packs with indexed downstream
    `dereference(...).why()` mapping. `File<Mode>` packs with indexed downstream file-handle method access,
    `Reference<File<Mode>>` packs with indexed downstream file-handle method access alongside explicit
    `dereference(...).write*` / `flush()` access plus helper-style `at(values, i).write*()` / `values.at(i).flush()`
    receivers, canonical free-builtin `at([values] values, [index] i).write*()` / `.flush()` receivers, and direct
    indexed `readByte(...)` `?` inference plus canonical free-builtin `at([values] values, [index] i).readByte(...)`
    `?`, `Pointer<File<Mode>>` packs with indexed downstream file-handle method access alongside explicit
    `dereference(...).write*` / `flush()` access plus helper-style `at(values, i).write*()` / `values.at(i).flush()`
    receivers, canonical free-builtin `at([values] values, [index] i).write*()` / `.flush()` receivers, and direct
    indexed `readByte(...)` `?` inference plus canonical free-builtin `at([values] values, [index] i).readByte(...)`
    `?`, `Buffer<T>` packs with indexed downstream `buffer_load(...)` and `buffer_store(...)` on the IR/VM GPU path,
    `Reference<Buffer<T>>` packs with indexed downstream `buffer_load(dereference(...), ...)` and
    `buffer_store(dereference(...), ...)` on that same IR/VM GPU path, `Pointer<Buffer<T>>` packs with indexed
    downstream `buffer_load(dereference(...), ...)` and `buffer_store(dereference(...), ...)` on that same IR/VM GPU
    path, `array<T>`, `Reference<array<T>>`, `Pointer<array<T>>`, `vector<T>`, `Reference<vector<T>>`,
    `Pointer<vector<T>>`, empty/header-only `soa<T>`, `Reference<soa<T>>`, `Pointer<soa<T>>`,
    `map<K, V>`, `Reference<map<K, V>>`, plus `Pointer<map<K, V>>` packs with indexed downstream `count()` resolution
    across direct/pure/mixed forwarding, `vector<T>` packs with indexed downstream `capacity()` resolution,
    borrowed/pointer array and vector packs with explicit indexed `dereference(...)` receiver wrappers for downstream
    checked/unchecked access, borrowed/pointer vector packs with that same indexed `capacity()` surface through explicit
    `dereference(...)` receiver wrappers, borrowed/pointer map packs with that same count and lookup surface through
    explicit indexed `dereference(...)` receiver wrappers, those same value, borrowed, and pointer map packs with
    indexed downstream `tryAt(...)` payload-kind inference for `auto` bindings, and `Pointer<map<K, V>>` packs with
    indexed downstream `contains()` / `at()` / `at_unsafe()` lookup access. Scalar `Pointer<T>` plus scalar
    `Reference<T>` packs with indexed downstream `dereference(...)`, and struct `Pointer<T>` plus struct `Reference<T>`
    packs with indexed downstream field/helper access. Newly discovered unsupported non-string pack gaps should be
    tracked as concrete TODOs only when backed by a reproducible semantics, lowering, or backend failure.
    Latest checkpoint: canonical free-builtin `at([values] values, [index] i)` on wrapped borrowed/pointer `File<Mode>`
    arg-packs now preserves `write*()` / `flush()` receivers plus `readByte(...)` `?` inference across direct calls
    plus pure/mixed spread forwarding, while wrapped `FileError` free-builtin named access remains on the existing
    named-argument rejection path.
- **Collections:** `array<Type>{ ... }` / `array<Type>[ ... ]`, `vector<Type>{ ... }` / `vector<Type>[ ... ]`, and
  `soa<Type>{ ... }` are brace-backed collection construction surfaces. The `collections` text transform normalizes
  bracket aliases to braces; it no longer owns map construction text. Map construction resolves through ordinary
  stdlib helpers such as canonical `/std/collections/map/map<Key,Value>(key1, value1, key2, value2, ...)` or
  `/std/collections/map/map<Key,Value>(/std/collections/map/entry<Key,Value>(key, value))` calls.
  - Fully qualified `/std/collections/map/*` calls do not require an alias; bare `map(...)` and `entry(...)` helper
    names are available only from import surfaces that publish them.
  - Map constructor arguments are validated by the selected stdlib helper signature, so invalid arity and type errors
    are ordinary call-resolution or argument-type diagnostics.
  - String keys are allowed in map constructors (e.g.,
    `/std/collections/map/map<string, i32>("a"utf8, 1i32)`) when the selected backend supports the resulting helper calls.
  - Collections can appear anywhere forms are allowed, including execution arguments.
  - Numeric/bool array literals (`array<i32>{...}`, `array<i64>{...}`, `array<u64>{...}`, `array<bool>{...}`) lower
    through IR/VM/native.
  - `array<T>` is a fixed-size contiguous value sequence once constructed (C++ `std::array`-like behavior). Arrays
    support read/write/index helpers but no growth helpers.
  - PrimeStruct keeps `array<T>` as a runtime-count contract; envelope-level length forms like `array<T, N>` are
    intentionally unsupported.
  - Array helpers: `value.count()`, `value.at(index)`, `value[index]`, `value.at_unsafe(index)` (canonical equivalents:
    `count(value)`, `at(value, index)`, `at_unsafe(value, index)`).
  - Ownership direction: keep `array<T>` as language-core substrate, move the
    public constructor/helper behavior of `vector<T>` and `map<K, V>` into
    stdlib `.prime` definitions, and treat promoted `soa<T>` as the current
    stdlib-owned SoA collection surface over generic SoA substrate.
  - `vector<T>` is a C++-style resizable contiguous owning sequence. `vector<T>{...}` is the user-facing variadic
    constructor form (0..N); `vector<T>(...)` remains a legacy compatibility helper path. Growth
    operations require `effects(heap_alloc)` (or the active default effects set), and
    `push`/`reserve` may reallocate and invalidate references/pointers into vector storage.
  - Planned stdlib-owned constructor surface: the temporary experimental vector namespace now uses `vector(values...)`
    over trailing `[args<T>] values`, and the remaining migration work is to move the canonical imported surface off
    fixed-arity helper wrappers onto that same variadic shape.
  - Vector helpers: `value.count()`, `value.at(index)`, `value[index]`, `value.at_unsafe(index)`, `value.push(item)`,
    `value.pop()`, `value.reserve(capacity)`, `value.capacity()`, `value.clear()`, `value.remove_at(index)`,
    `value.remove_swap(index)` (canonical helper equivalents remain `count(value)`, `at(value, index)`, `push(value,
    item)`, etc.). Import `/std/collections/*` before using bare helper names or method-sugar examples.
    Exact `import /std/collections/vector` is part of the verified runnable surface for bare
    legacy `vector(...)` construction compatibility plus canonical `/std/collections/vector/*` helper forms. Concise
    binding initializers such as `[vector<T>] values{1, 2, 3}` are now also part of the verified
    wildcard-import surface for readable vector examples and loop/index code, while explicit
    `vector<T>{...}` construction remains available when examples want to emphasize constructor
    spelling.
  - Map helpers: `/std/collections/map/count(value)`, `/std/collections/map/contains(value, key)`,
    `/std/collections/map/tryAt(value, key)`, `value.at(key)`, `value[key]`, and `value.at_unsafe(key)`.
    `tryAt` routes misses to `Result<ContainerError>` / `Result<T, ContainerError>` instead of the checked
    missing-key abort path and, on IR-backed backends, currently supports the same `i32`/`bool`/`f32`/`string` plus
    single-slot int-backed stdlib error-struct value subset as `Result.ok(value)`.
  - Stdlib-owned map constructor surface: canonical map construction lives in
    `stdlib/std/collections/map.prime` as `map(...)`, `entry(key, value)`, and
    `/std/collections/map/map(...)` helpers over the internal `MapValue<K, V>`
    backing implementation. Future variadic entry work can extend that helper
    surface without reviving compiler-owned map literal lowering.
  - Public `Map<K, V>` wrapper (TODO-4751): `stdlib/std/collections/map.prime`
    defines `/std/collections/map/Map<K, V>`, a thin public struct that owns one
    `MapValue<K, V>` (`inner`), mirroring how `Vector<T>` is itself the canonical
    vector struct. It exposes `count()`, `contains(key)`, `tryAt(key)`,
    `at(key)`, `at_unsafe(key)`, and `insert(key, value)` methods, and is built
    with `mapSingle<K, V>(key, value)`, `mapPair<K, V>(k1, v1, k2, v2)`, or
    `Map<K, V>{}`. A bare `Map` spelling is an ordinary struct name resolved
    through imports and namespaces (a user `Map<K, V>` in its own namespace
    dispatches to its own methods); it is never the builtin key/value storage
    identity, and a templated `Map<...>` with no visible `Map` struct is a
    semantic error. Wrapper receivers route every helper form to the struct's
    own methods: `values.count()`, `values[key]`, bare `count(values)`,
    explicit `/std/collections/map/insert<K, V>(values, key, value)`, and the
    borrowed `/std/collections/map/<helper>_ref<K, V>(ref, ...)` forms on a
    `Reference<Map<K, V>>`. The canonical helper family is not overloaded for
    the wrapper, and `map<K, V>` / `MapValue<K, V>` values do not convert to
    `Map<K, V>` implicitly. Unlike builtin `map<K, V>`, the wrapper accepts user
    struct keys that define `equal`/`less_than`.

    ```prime
    import /std/collections/*
    import /std/collections/map/*

    [effects(heap_alloc), return<int>]
    main() {
      [Map<i32, i32> mut] values{mapSingle<i32, i32>(1i32, 4i32)}
      values.insert(2i32, 7i32)
      /std/collections/map/insert<i32, i32>(values, 1i32, 5i32)
      return(values.count() + values[2i32] + values.at(1i32))
    }
    ```

    The example exits with `14` (2 + 7 + 5) on vm, native, and exe. The
    `/main` semantic product publishes `/std/collections/map/Map__t<hash>/insert`,
    `.../count`, and `.../at` method targets (no IR-format change).
  - Current discard contract: builtin `pop` and `clear` are only defined for drop-trivial element types while
    container-owned destruction is still being specified. Drop-trivial currently includes scalar primitives, `string`,
    `Pointer<T>`, `Reference<T>`, arrays of drop-trivial elements, and concrete structs that do not define `Destroy*`
    helpers and whose fields are also drop-trivial. Builtin `vector`/`map`/`soa` elements and structs with
    `Destroy` hooks are rejected for builtin `pop`/`clear`.
  - Current indexed-removal contract: builtin `vector` `remove_swap` and builtin `vector` `remove_at` now support
    ownership-sensitive and relocation-sensitive element types on the capacity-guarded heap-backed builtin runtime
    path. The lowerer has a reusable lifecycle-aware destroy-at-pointer helper, a shared vector destroy-at-slot
    primitive, and shared survivor-motion helpers, so both builtin indexed-removal helpers now destroy the removed slot
    and move survivors without depending on relocation-trivial raw copies. Canonical `/std/collections/vector/*`
    indexed-removal helpers on explicit experimental `Vector<T>` bindings remain exempt from builtin-runtime constraints
    because they route onto the experimental `.prime` implementation instead. There is no corresponding
    SoA indexed-removal surface yet.
  - Remaining live ownership/runtime migration work is now the builtin canonical `map<K, V>` growth
    surface follow-up beyond the current owning local numeric-map path. The lowerer now has a shared
    non-local grown-pointer write-back/repoint primitive for wrapper targets, and direct canonical
    `/std/collections/map/insert(...)` plus method-sugar `receiver.insert(...)` calls now also route
    map-returning helper receivers (including nested field-access helper returns) onto that same
    rewrite path. Direct canonical field-access non-local receivers (for example `holder.values`) now
    also route through that path when explicit helper template arguments or inferred canonical
    callee receiver typing provide map typing.
    Method-sugar field-access receivers now also route through that path by inferring receiver map
    typing from resolved canonical method-callee receiver parameter types when method template
    arguments are omitted.
    Compatibility helper alias `insert` surfaces (for example `mapInsert`) now share that same
    field-access receiver-typing inference when template arguments are omitted, so alias-resolved
    non-local receiver forms retarget to the canonical `.prime` insert helper instead of falling
    back to pending runtime paths.
    Location-wrapped field-access and helper-return receivers (for example
    `location(holder.values)` and `location(makeValues())`) now also route through that path by
    peeling `location(...)` wrappers before canonical/helper-alias receiver-typing and helper-return
    collection inference when template arguments are omitted.
    Location-wrapped local-map receivers (for example `location(valuesLocal)`) now also route
    through that path by peeling `location(...)` wrappers before local map-kind probing so omitted
    template arguments can reuse local map key/value metadata.
    Dereference-wrapped helper-return receivers (for example
    `dereference(makeValuesRef())`) now also route through that path by inferring map typing from
    resolved helper return collection declarations when template arguments are omitted.
    Dereference-wrapped non-local field-access receivers (for example
    `dereference(holder.valuesRef)`) now also share that same canonical/helper-alias callee
    receiver-typing inference when template arguments are omitted, so those non-local borrowed
    receiver forms retarget to the canonical `.prime` insert helper as well.
    Nested location+dereference receiver chains (for example
    `location(dereference(location(makeValuesRef())))` and
    `location(dereference(location(holder.valuesRef)))`) now route through that same path by
    peeling stacked wrappers before helper-return and field-access receiver typing inference when
    template arguments are omitted.
    Temporary helper-return value receivers that do not provide a stable writable lvalue target now
    have deterministic compile-time rejects in conformance coverage (direct canonical and method-sugar
    forms). The remaining public
    builtin borrowed/non-local mutation surfaces still need to route onto that path instead of the
    current pending runtime boundary.
  - Current relocation contract: builtin `push` and `reserve` are only defined for relocation-trivial element types
    while container move/reallocation semantics are still being specified. Relocation-trivial currently includes scalar
    primitives, `string`, `Pointer<T>`, `Reference<T>`, arrays of relocation-trivial elements, and concrete structs that
    do not define `Destroy*` or `Copy`/`Move` helpers and whose fields are also relocation-trivial. Builtin
    `vector`/`map`/`soa` elements and structs with custom move/destroy hooks are rejected for builtin
    `push`/`reserve`.
  - Vector binding forms:
    - `[vector<T> mut] v{vector<T>{}}` and `[mut] v{vector<T>{}}` are both valid.
    - `[vector<T> mut] v{}` is shorthand for zero-arg construction and rewrites to `[vector<T> mut] v{vector<T>{}}`.
  - `soa<T>` is an explicit structure-of-arrays container (SoA). It is separate
    from `vector<T>`; the compiler must not silently rewrite AoS (`vector`) to
    SoA (`soa`). `soa_vector<T>` is now a rejected compatibility spelling.
  - Intended usage: data-oriented loops and ECS-style component storage where field-wise contiguous iteration is
    preferred.
  - Implementation model: the public `soa` API lives in stdlib `.prime`
    files, with compiler/runtime code reduced to generic SoA substrate only
    (field-layout/codegen/introspection, column storage, field-view
    borrowing/invalidation, and allocation primitives). The end-state is that
    C++ source does not contain public SoA collection semantics.
  - Public `soa<T>` surface:
    - Construction/growth mirrors vector (`soa<T>{}`, `push`, `reserve`, `count`) and allocation still requires
      `effects(heap_alloc)`.
    - Indexing/access is explicit and SoA-aware (`value.field()[i]`, `value.get(i)`, and optionally `value.ref(i)` proxy
      access).
    - Reallocation invalidates SoA field views/proxies the same way vector growth invalidates pointers/references.
  - Ownership/invalidation rules for ECS-style usage:
    - Treat `get(...)` as value-style element access.
    - Treat `ref(...)` and future field views as borrowed storage views that are invalid after structural mutation.
    - Structural mutations (`push`, `reserve`) and explicit AoS/SoA conversions (`to_soa`, `to_aos`) are mutation
      boundaries; any previously acquired SoA views/proxies are invalid after these operations.
    - Explicit lifecycle destroy calls (`Destroy`, `DestroyStack`, `DestroyHeap`, `DestroyBuffer`) on SoA owners are
      also mutation boundaries; any previously acquired SoA views/proxies are invalid after these operations.
    - Implicit owner drops at scope exit also invalidate outstanding SoA views/proxies; keeping a live `ref(...)`
      borrow past the end of the owner scope is rejected.
    - Preferred update pattern is two-phase: run a stable-size update pass first, then apply deferred structural
      changes.
    - Example (canonical surface): `while(less_than(i, particles.count())) { particles.get(i) ... }`
      followed by `particles.reserve(plus(particles.count(), spawnQueue.count()))`.
  - SoA eligibility (v1 draft): `T` must be a struct with SoA-safe fields (fixed-size,
    non-pointer/reference/string/template envelopes unless explicitly allowed by backend policy).
  - AoS/SoA conversions are explicit only (`to_soa(vector<T>)`, `to_aos(soa<T>)`); no implicit interop.
  - Canonical example source lives at `examples/3.Surface/soa_ecs.prime` and
    imports `/std/collections/soa/*`. Focused rejection tests retain old
    `soa_vector` imports only as intentional negative coverage.
  - **Current implementation status:** `soa<T>` surface parsing is recognized,
    canonical `/std/collections/soa/*` helper routes cover construction,
    read/ref, mutation, field-view, and AoS conversion behavior, and retired
    `soa_vector` type/module spellings reject with stable compatibility
    diagnostics. Remaining `SoaVector<T>` and `soaVector*` names are internal
    backing identity for the current stdlib adapter and generic substrate, not
    public collection spelling.
  - **Current implementation status:** VM/native vector locals use a heap-backed `count/capacity/data_ptr` record
    layout. `push` and dynamic `reserve` growth allocate/reallocate backing storage and report deterministic runtime
    allocation failures (`vector push allocation failed (out of memory)` / `vector reserve allocation failed (out of
    memory)`) once the local dynamic-capacity limit (`1024`) is exceeded. `vector<T>{...}` construction and legacy
    `vector<T>(...)` compatibility construction above `1024` elements and out-of-range/negative folded `reserve`
    integer literal expressions are rejected at lowering time
    (`collection literal exceeds local capacity limit (1024)` / `vector reserve exceeds local capacity limit (1024)` /
    `vector reserve expects non-negative capacity` / `vector reserve literal expression overflow`). Folded literal
    support currently covers `plus`/`minus`/`negate` expression trees, and both signed and unsigned fold-overflow paths
    emit `vector reserve literal expression overflow`.
  - Mutation helpers (`push`, `pop`, `reserve`, `clear`, `remove_at`, `remove_swap`) are statement-only.
  - Current builtin map key contract: `K` must resolve to the builtin `Comparable` subset (`i32`, `i64`, `u64`, `f32`,
    `f64`, `bool`, or `string`). Generic `map<K, V>` helpers may defer that check until monomorphisation, but concrete
    user-defined `Comparable` structs are not accepted by the builtin map runtime yet.
  - Numeric/bool map constructors lower through IR/VM/native via stdlib map helpers (construction, `count`, `at`, and
    `at_unsafe`).
  - String-keyed map constructors lower through VM/native when keys are string literals or bindings backed by literals;
    other string key expressions require the C++ emitter (which uses `std::string_view` keys).
- **Conversions:** no implicit coercions. Use explicit executions (`convert<f32>(value)` or `bool{value}`) or custom
  transforms. The builtin `convert<T>(value)` is the default cast helper and supports `i32/i64/u64/bool/f32/f64` in the
  minimal native subset (integer width conversions currently lower as no-ops in the VM/native backends, while the C++
  emitter uses `static_cast`; `convert<bool>` compares against zero, so any non-zero value—including negative
  integers—yields `true`). Float ↔ integer conversions lower to dedicated PSIR opcodes in VM/native, and converting
  NaN/Inf to an integer is a runtime error (stderr + exit code `3`).
  - **Convert constructor resolution (v1):**
    - Builtin fast-path: when `T` is `bool/i32/i64/u64/f32/f64` and `value` is numeric/bool, use the builtin conversion
      rules; user-defined conversions do not override this.
    - Otherwise, resolve `convert<T>(value)` as a call to `T.Convert(value)` in the struct method namespace
      (`/T/Convert`).
    - Signature must match exactly after monomorphisation: `[static return<T>] Convert([U] value)` (Convert helpers are
      static, no implicit `this`). No implicit conversions are applied to match the parameter type.
    - If no match exists, emit a `no conversion found` diagnostic. If multiple matches exist, emit an `ambiguous
      conversion` diagnostic with the candidate list.
    - Visibility follows import rules: only `[public]` `Convert` helpers are visible across imports.
    - `convert<T>(value)` does not fall back to `T(value)` or `T{...}`.
- **Float note:** VM/native lowering supports float literals, bindings, arithmetic, comparisons, numeric conversions,
  and `/std/math/*` helpers.
- **String note:** VM/native lowering supports string literals and string bindings in `print*`, plus `count`/indexing
  (`at`/`at_unsafe`) on string literals and string bindings that originate from literals; other string operations still
  require the C++ emitter for now.
  - **Struct note:** VM/native lowering supports struct values when fields are numeric/bool or other struct values
    (nested structs). Struct fields with strings or templated envelopes still require the C++ emitter.
  - `bool{...}` is valid for integer operands (including `u64`) and treats any non-zero value as `true`.
- **Mutability:** values immutable by default; include `mut` in the stack-value execution to opt-in (`[f32 mut]
  value{...}`).
- **Open design:** raw string semantics across backends.
