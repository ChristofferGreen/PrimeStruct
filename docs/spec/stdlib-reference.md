# Standard Library Reference

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **normative (draft)**.

### Standard Library Reference (v0)
- **Status:** stable snapshot of the current builtin surface (v0).
- **Versioning (planned):**
  - Each package declares a semantic version (e.g., `1.2.0`).
  - `import<..., version="1.2.0">` selects a specific package revision.
  - There is no compiler flag to pin the default package set yet; use explicit versions in `import` declarations.
- **Namespaces:**
  - `/std/math/*` is imported via `import /std/math/*` (or explicit names like `import /std/math/sin /std/math/pi`).
  - Core builtins (`assign`, `count`, `print*`, `convert`, etc.) live in the root namespace.
- **Conformance markers:**
  - **`C++`**: supported only by the C++ emitter (VM/native reject).
  - **`VM/native`**: supported by the VM + native backends.
  - **`VM/native (limited)`**: supported with the listed restrictions.
- **Conformance notes:**
  - String comparisons are **`C++`** only; VM/native reject them.
  - `map<K, V>` keys currently use the builtin ordered-map `Comparable` subset: `i32`, `i64`, `u64`, `f32`, `f64`,
    `bool`, or `string`. Template parameters defer this check until monomorphisation; user-defined `Comparable` keys
    remain blocked on the future stdlib-owned map runtime.
  - Builtin `map<K, V>` values must be numeric/bool for **`VM/native`**; string values remain **`C++`** only on the
    builtin map path, while the experimental stdlib map helper path now has VM/native `mapTryAt` coverage for string
    values.
  - String indexing in **`VM/native (limited)`** requires string literals or bindings backed by literals.
  - `vector<T>` is specified as a C++-style dynamic contiguous sequence
    (`push`/`reserve` may grow capacity). VM/native now use a heap-backed
    `count/capacity/data_ptr` record for vector locals, so push/reserve growth
    reallocates backing storage while preserving existing elements up to the
    current deterministic `1024` local dynamic-capacity limit.
  - Stdlib collection helpers now share `ContainerError` for deterministic error payloads: `containerMissingKey()`
    (`1`), `containerIndexOutOfBounds()` (`2`), `containerEmpty()` (`3`), and `containerCapacityExceeded()` (`4`).
    `containerErrorStatus(err)` packs a status-only `Result<ContainerError>`, `containerErrorResult<T>(err)` packs a
    `Result<T, ContainerError>` error value for the current IR-backed backends, the public
    `/ContainerError/why([ContainerError] err)` wrapper keeps explicit type-owned error strings on the stdlib surface,
    the canonical type-owned `ContainerError.missingKey()`, `ContainerError.indexOutOfBounds()`,
    `ContainerError.empty()`, and `ContainerError.capacityExceeded()` helpers keep the current constructor values on
    that same surface, the public camelCase root wrappers `/ContainerError/missingKey()`,
    `/ContainerError/indexOutOfBounds()`, `/ContainerError/empty()`, and `/ContainerError/capacityExceeded()` expose
    those same values for slash-call code, compatibility wrappers keep the older snake_case root spellings, and
    `Result.why(Result<ContainerError>)` maps container error codes to the same stable literal-backed messages.
    Builtin empty-vector `pop` runtime aborts and checked vector indexing/removal aborts now use the same `"container
    empty"` / `"container index out of bounds"` wording across VM/native/C++ flows.
  - Canonical `/std/collections/vector/*` is now the sole public namespaced vector contract. The
    `/std/collections/vector/*` family owns the internal backing adapter behind that
    public contract, while direct `/std/collections/experimental_vector/*`
    source imports are rejected. The legacy experimental shim remains only as
    forwarding storage identity behind the internal backing adapter. That
    backing adapter (`vector<T>(...)`, `vectorNew`, `vectorSingle`,
    `vectorPair`, `vectorTriple`, `vectorQuad`, `vectorQuint`, `vectorSext`, `vectorSept`, `vectorOct`, `vectorCount`,
    `vectorCapacity`, `vectorReserve`, `vectorPush`, `vectorPop`, `vectorClear`, `vectorRemoveAt`,
    `vectorRemoveSwap`, `vectorAt`, `vectorAtUnsafe`) returns the current `.prime` `Vector<T>` record backed by
    heap-pointer storage, but user-facing docs/examples should prefer the canonical wrappers in
    `stdlib/std/collections/vector.prime` instead of treating the experimental shim as a peer public API. The
    current slice includes a real variadic `.prime` constructor built on `[args<T>]` parameters, with the older
    fixed-arity helper names retained as backing compatibility forwarders, plus reserve/push and pop/clear/remove
    helpers on that pointer-backed storage. `Vector<T>` now carries `.prime` `Move` and `Destroy` hooks plus
    `Pointer<uninitialized<T>>` slot storage, so constructor materialization, growth, checked/unchecked access,
    removed-element destruction, survivor compaction/swap, and scope-exit cleanup all run through stdlib-owned
    `init(...)`, `borrow(...)`, `take(...)`, and `drop(...)` flows instead of reusing builtin vector ownership gates.
    Imported canonical `/std/collections/vector/*` helper spellings still
    rewrite onto that same backing implementation whenever their receiver is an
    experimental `Vector<T>` value. The old `/std/collections/vectorCount` /
    `vectorCapacity` / `vectorPush`-style wrapper helpers and fixed-arity
    `vectorSingle` / `vectorPair`-style constructor aliases are no longer
    published by the public `/std/collections` wrapper layer; user-facing code
    should use canonical `/std/collections/vector/*` paths or method sugar
    instead. Imported
    positional canonical `/std/collections/vector/vector<T>(...)` calls now prefer a real variadic canonical wrapper
    over that same backing constructor for `auto` bindings and temporary receiver flows, and imported named-argument
    legacy constructor-shaped canonical calls now resolve through same-path fixed-arity
    `/std/collections/vector/vector` overloads instead of helper-path rewrites for those same inferred and temporary
    receiver cases. Canonical and bare statement calls to vector mutators such
    as `push`, `pop`, `reserve`, `clear`, `remove_at`, and `remove_swap` now
    resolve through visible `.prime` helper definitions and deterministic
    missing-import diagnostics instead of a compiler-owned vector
    statement-helper emitter. Wrapper-layer
    `/std/collections/vectorPush`-style mutator aliases are removed from the
    public wrapper layer instead of rewriting back into raw builtin-vector
    emission.
    Direct `/std/collections/experimental_vector/vectorPush`-style source
    imports now fail with an import diagnostic; use canonical
    `/std/collections/vector/*` imports instead. Experimental
    `Vector<T>.set_field_count` and
    `set_field_capacity` statement calls plus `field_count`, `field_capacity`,
    `set_field_count`, and `set_field_capacity` inline helper calls now inline
    their visible `.prime` helper definitions instead of using C++ fast paths
    that write fixed count/capacity header slots directly. Vector metadata
    expression fallbacks and constructor/header materialization now route
    through visible `.prime` definitions or declarative layout facts. Canonical
    `/std/collections/vector/*` `pop` / `clear` helpers now reuse that same
    backing discard path for ownership-sensitive element types on explicit
    experimental `Vector<T>` bindings, and canonical
    `/std/collections/vector/*` `remove_at` / `remove_swap` helpers now reuse the backing indexed-removal path for
    those same explicit experimental `Vector<T>` bindings. Explicit canonical read/access calls to
    `/std/collections/vector/count`, `capacity`, `at`, and `at_unsafe` now fall through to visible `.prime` helper
    definitions instead of count/access classifier raw vector emitters; rooted
    `/vector/*` helper spellings are rejected unless an explicit same-path user
    definition exists, and direct `/std/collections/experimental_vector/*`
    source imports are rejected.
  - Canonical `/std/collections/map/*` is now the sole public namespaced map contract. The
    `/std/collections/internal_map/*` module now owns the map backing implementation behind that
    public contract while preserving the current compatibility `Map<K, V>` identity. The backing helpers
    (`Entry<K, V>`, `entry(key, value)`,
    `map<K, V>(entries...)`, `mapNew`, `mapSingle`, `mapDouble`, `mapPair`, `mapTriple`, `mapQuad`, `mapQuint`,
    `mapSext`, `mapSept`, `mapOct`, `insertImpl`, `countImpl`, `containsImpl`, `tryAtImpl`, `atImpl`, `atUnsafeImpl`,
    `insertRefImpl`, `countRefImpl`, `containsRefImpl`, `tryAtRefImpl`, `atRefImpl`, `atUnsafeRefImpl`) return the current
    `.prime` `Map<K, V>` struct backed by parallel vector storage, but user-facing
    docs/examples should prefer the canonical wrappers in `stdlib/std/collections/map.prime` instead of treating the
    experimental compatibility shim as a peer public API. The current constructor surface includes a real variadic `.prime`
    `map(entries...)` helper over trailing `[args<Entry<K, V>>]` parameters, with the older fixed-arity helper names
    retained as backing compatibility forwarders. Lookup follows `Comparable<K>` inside the internal `.prime`
    substrate: `findIndexImpl` and `borrowedFindIndexImpl` scan the parallel key vector, `containsImpl` compares the
    found index with the key count, `tryAtImpl` returns `Result<ContainerError>` with `containerMissingKey()` for misses,
    `atImpl` preserves the checked missing-key trap path, and `atUnsafeImpl` keeps the unchecked payload access path.
    `insertImpl` and `insertRefImpl` overwrite duplicate keys by `drop(...)`ing and `init(...)`ing the payload slot, or
    grow the parallel key/value vectors through internal vector pushes when the key is new. Constructor-backed
    canonical `map<K, V>` bindings and non-local map receivers now route direct
    `/std/collections/map/insert(...)`, `.insert(...)`, and
    `/std/collections/map/insert_ref(...)` through those `.prime` helper bodies
    on VM/native instead of the stale flat builtin insert rewrite. Borrowed
    `Reference<map<K, V>>` values now support
    distinct `*Ref` free-helper calls plus `.count()`/`.contains()`/`.tryAt()`/`.at()`/`.at_unsafe()`/`.insert()`
    method-call sugar through `.prime` `/Reference/*` helpers, and both value plus borrowed-reference experimental maps
    now participate in shared `value[key]` bracket access with the same key/type diagnostics as other map helper forms.
- **Core builtins (root namespace):**
  - **`assign(target, value)`** (statement): mutates a mutable binding or dereferenced pointer.
  - **`increment(target)` / `decrement(target)`** (statement): mutation helpers used by `++`/`--` desugaring.
  - **`if(cond, then() { ... }, else() { ... })`**: canonical conditional form after desugaring.
  - **`loop(count, do() { ... })`** (statement): loop helper with integer count.
  - **`while(cond, do() { ... })`** (statement): loop helper with bool condition.
  - **`for(init, cond, step, do() { ... })`** (statement): loop helper with init/cond/step.
  - **`return(value)`**: explicit return helper.
  - **`count(value)` / `value.count()`**: collection length. Vector forms currently require `import /std/collections/*`.
  - **`contains(value, key)`**: map key probe without triggering the checked missing-key abort path used by `at(value,
    key)`.
  - **`at(value, index)` / `value.at(index)` / `value[index]` / `at_unsafe(value, index)`**: bounds-checked and
    unchecked indexing (`value[index]` and `value.at(index)` are the same safe operation). Vector forms currently
    require `import /std/collections/*`.
  - **`print*`**: `print`, `print_line`, `print_error`, `print_line_error`.
  - **Collections:** `array<T>{...}`, `vector<T>{...}`, and stdlib map helpers such as
    `/std/collections/map/map<K, V>(key, value, ...)` or
    `/std/collections/map/map<K, V>(/std/collections/map/entry<K, V>(key, value))`. Legacy call-shaped collection helpers
    remain compatibility helper surfaces.
  - **Pointer helpers:** `location`, `dereference`.
  - **Ownership helpers:** `move`, `clone`.
  - **Uninitialized helpers (draft):** `init`, `drop`, `take`, `borrow`.
  - **GPU builtins (draft):**
    - `Buffer<T>` is a hybrid GPU resource-handle surface: allocation, upload/readback, dispatch, and storage access
      stay in language/runtime substrate, while higher-level wrappers should converge on stdlib `.prime` definitions.
      Canonical and experimental `/std/gfx/*` now provide `.prime`-authored `Buffer.count()`, `Buffer.empty()`,
      `Buffer.is_valid()`, `Buffer.readback()`, and compute-only `Buffer.load(index)` / `Buffer.store(index, value)`
      convenience helpers, the preferred constructor-shaped `Buffer<T>(count)` allocation surface, plus explicit
      slash-call `/std/gfx/Buffer/allocate<T>(count)`, `/std/gfx/Buffer/upload(...)`, `/std/gfx/Buffer/load(...)`, and
      `/std/gfx/Buffer/store(...)` wrappers over the public handle layout and builtin host
      allocation/readback/upload/storage substrate.
    - `/std/gpu/global_id_x()` → `i32` (kernel invocation x coordinate).
    - `/std/gpu/global_id_y()` → `i32` (kernel invocation y coordinate).
    - `/std/gpu/global_id_z()` → `i32` (kernel invocation z coordinate).
    - `/std/gpu/buffer_load(Buffer<T>, index)` / `/std/gpu/buffer_store(Buffer<T>, index, value)` for storage buffers.
    - `/std/gpu/buffer<T>(count)` / `/std/gpu/upload(array<T>)` / `/std/gpu/readback(Buffer<T>)` for host-side resource
      management.
    - `/std/gpu/dispatch(kernel, gx, gy, gz, args...)` submits a compute kernel and requires `effects(gpu_dispatch)`.
      For determinism in v1, dispatch is blocking and `/std/gpu/readback` returns only after completion.
  - **Operators (desugared forms):** `plus`, `minus`, `multiply`, `divide`, `negate`, `increment`, `decrement`.
  - **Comparisons/booleans:** `greater_than`, `less_than`, `greater_equal`, `less_equal`, `equal`, `not_equal`, `and`,
    `or`, `not`.
  - **Result helpers (draft):**
  - `Result<Error>` is a status-only wrapper for fallible operations; `Result<T, Error>` carries a value on success.
  - **ADT migration note:** `/std/result/*` now exposes importable `Result<E>` and value-carrying `Result<T, E>` sums
    at the same public path. `Result<E>` has a unit `ok` variant, an `error(E)` payload variant, default construction
    to `ok`, and an `ok<E>()` helper; error construction currently uses explicit sum construction such as
    `Result<E>{[error] err}`. `Result<T, E>` has `ok` and `error` payload variants plus explicit
    `ok<T, E>(value)` / `error<T, E>(err)` construction helpers. Legacy packed status-only `Result<Error>` values
    without the stdlib import remain a compiler/runtime bridge and produce a deterministic compatibility diagnostic
    if used as a `pick` target. `try(...)`
    semantic validation and semantic-product metadata accept both `Result<T, E>` and the qualified
    `/std/result/Result<T, E>` spelling for value-carrying results. IR-backed `try(...)` now consumes local imported
    stdlib value-result sums for `return<int> on_error<...>` status-code flows by branching on the `ok`/`error` tag,
    and Result-returning functions can propagate local imported stdlib value-result sum errors by copying the
    `error` payload into the declared return `Result` sum after running the active `on_error` handler. Direct calls
    that return imported stdlib value-result sums can also be consumed through postfix `?` on those same VM/native
    sum-backed paths. Typed imported value-carrying sum
    locals/returns may use `Result.ok(value)` as an `ok`-variant compatibility initializer on IR-backed VM/native
    paths, and typed imported value-carrying sum locals/returns may use the `Result.map(result, fn)` and
    `Result.and_then(result, fn)` compatibility helpers when the source is a local imported stdlib Result sum or a
    direct call returning one; `Result.map2(left, right, fn)` compatibility is available when both sources are local
    imported stdlib Result sums or direct calls returning them. Dereferenced local `Reference<Result<T, E>>` and
    `Pointer<Result<T, E>>` values can feed
    `try(...)`, `Result.error(...)`, and `Result.why(...)` when they point at imported stdlib Result sums.
    `Result.error(value)` already reads the imported value-carrying sum tag on those paths, so it returns `false` for
    `ok` and `true` for `error` values from `/std/result/*`.
    `Result.why(value)` also reads imported value-carrying sums, yielding the empty string for `ok` and calling the
    error payload type's `why` helper for `error`. IR-backed `try(...)` also consumes local, direct-call, and
    dereferenced local `Reference<Result<E>>` / `Pointer<Result<E>>` imported status-only `Result<E>` sums for
    status-code returns and Result-return error propagation. IR-backed `Result.error(...)` / `Result.why(...)` also
    inspect imported status-only `Result<E>` sums from locals, direct calls, and dereferenced local
    `Reference<Result<E>>` / `Pointer<Result<E>>` sources instead of falling back to the legacy packed-status bridge.
    The source C++ emitter still uses a compatibility Result bridge, but it now preserves nested
    `Result<T...>` types under `Reference` / `Pointer` and recognizes dereferenced local/indexed borrowed Result
    operands for `try(...)`, `Result.error(...)`, and `Result.why(...)`. Its source C++ Result storage-width decisions
    and construction/accessor expression emission are quarantined behind named emitter helpers. Value-carrying Result
    storage emits the tagged `ps_result_value` bridge type instead of raw `uint64_t` return/binding types or legacy
    `ps_legacy_result_*` helper names. That generated type has separate tag, error-payload, and `ok` success fields
    plus explicit ok/error construction helpers and value-qualified accessor helper names. The generated bridge uses
    named ok/error tag constants, and it does not retain raw
    packed-integer construction, conversion, generic `ps_result_*` value accessors, or `ps_result_pack(...)`
    compatibility. Status-only source C++ Result storage emits the tagged `ps_result_status` bridge type instead of
    raw `uint32_t` return/binding types, with the same named ok/error tag constants and low-level file helper status
    codes wrapped at the source Result boundary. Explicit source C++ `Result<T, E>{[ok] value}`,
    `Result<T, E>{[error] err}`, `Result<E>{}`, `Result<E>{ok}`, and `Result<E>{[error] err}` constructors route
    through those bridge helpers for supported scalar-compatible payloads. Source C++ `Result.why(...)` binds the
    bridge operand once, returns an empty string for `ok`, and only calls the error-domain `why` helper for `error`
    payloads. Explicit source C++ `ok` and `error` constructors pack single-field int-backed success/error structs
    through their code field before entering the value-carrying or status-only bridge. Direct source C++
    `Result.ok(value)` uses the same single-field success-struct packing path before entering the bridge.
    Result helper compatibility adapter inventory:
    - **Semantic validation and type inference:** `src/semantics/SemanticsValidatorExprResultFile.cpp`,
      `src/semantics/SemanticsValidatorResultHelpers.cpp`, `src/semantics/SemanticsValidatorExprSumConstructors.cpp`,
      `src/semantics/SemanticsValidatorStatementReturns.cpp`, `src/semantics/SemanticsValidatorBuildParameters.cpp`,
      `src/semantics/SemanticsValidatorInferGraph.cpp`, `src/semantics/SemanticsValidatorInferPreDispatchCalls.cpp`,
      `src/semantics/TemplateMonomorphBindingCallInference.h`, and
      `src/semantics/TemplateMonomorphExperimentalCollectionValueRewrites.h` own the remaining semantic acceptance,
      type inference, parameter inference, and collection-payload rewrite paths for `Result.ok`, `Result.map`,
      `Result.and_then`, and `Result.map2` compatibility. Retirement decision: keep them fenced until the same helper
      calls are either implemented as ordinary `/std/result/*` helpers or intentionally rejected with deterministic
      migration diagnostics.
    - **IR metadata and helper-call inference:** `src/ir_lowerer/IrLowererResultHelpers.cpp`,
      `src/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.cpp`,
      `src/ir_lowerer/IrLowererLowerInferenceDispatchSetup.cpp`,
      `src/ir_lowerer/IrLowererLowerEmitExprTryHelpers.cpp`,
      `src/ir_lowerer/IrLowererStatementBindingTypeMetadata.cpp`, and
      `src/ir_lowerer/IrLowererUninitializedStructInference.cpp` own helper-result metadata, base-kind inference,
      direct-call `try(...)` operand inference, binding metadata, and direct `Result.ok(...)` payload-struct inference.
      Retirement decision: delete the syntax-fallback pieces after semantic-product facts fully describe helper-call
      results and stdlib sum construction covers the payload cases directly.
    - **IR/backend lowering:** `src/ir_lowerer/IrLowererLowerSumHelpers.cpp`,
      `src/ir_lowerer/IrLowererLowerEmitExprResultHelpers.h`,
      `src/ir_lowerer/IrLowererPackedResultHelpers.cpp`, and
      `src/ir_lowerer/IrLowererStatementBindingStatementEmit.cpp` own the remaining VM/native sum lowering and packed
      bridge emission for `Result.ok`, `Result.map`, `Result.and_then`, and `Result.map2` compatibility. Retirement
      decision: keep the stdlib-sum lowering only until ordinary stdlib helper calls can lower without bespoke
      result-helper branches, and delete packed bridge emission after source and IR-backed paths no longer need packed
      Result values.
    - **Source C++ emitter:** `src/emitter/EmitterExprResultCalls.h` and `src/emitter/EmitterExprLambda.h` own the
      source C++ compatibility bridge for helper expression emission and lambda result inference. Retirement decision:
      keep the bridge quarantined behind those files until the source C++ emitter lowers imported Result sums directly
      or delegates to ordinary stdlib helper bodies.
  - `Result<T, Error>` is in transition: explicit imported value construction is stdlib-owned, while `?` propagation
    and the minimum success/error runtime contract stay language-defined until the sum-backed propagation contract is
    implemented. The semantic `try(...)` contract already recognizes the unqualified and qualified stdlib-owned
    value-result type spellings, and the IR-backed VM/native paths already branch on local stdlib Result sums for
    status-code returns, Result-return error propagation, direct-call postfix `?` operands, and imported status-only
    `try(...)` operands across local, direct-call, and dereferenced local pointer/reference sources, plus status-only
    `Result.error(...)` / `Result.why(...)` helpers on those same operand families. The source C++ emitter now mirrors
    the borrowed/pointer helper operand inference while still using a compatibility bridge; storage-width decisions and
    construction/accessor expression emission are quarantined behind named emitter helpers. Value-carrying Result
    storage is named through the tagged `ps_result_value` bridge type, uses tag-based error checks, and no longer
    accepts or converts back to a raw packed integer, generic value accessor helper, or `ps_result_pack(...)` helper.
    Source C++ Result bridge construction and checks now share named ok/error tag constants, and value-carrying
    storage uses an `ok` field for the success payload. Status-only source C++ Result storage is named through the
    tagged `ps_result_status` bridge type, uses tag-based error checks, and wraps raw low-level file helper status
    codes at the source Result boundary. Explicit source C++ Result sum constructors for supported scalar-compatible
    ok/error payloads now lower through the same bridge helpers. Source C++ `Result.why(...)` uses those tag checks
    before extracting the error payload, so `ok` bridge values yield the empty string instead of calling the
    error-domain `why` helper. Explicit source C++ `ok` and `error` constructors pack single-field int-backed
    success/error structs through their code field before entering the bridge. Direct source C++ `Result.ok(value)`
    does the same for local or explicitly constructed single-field success structs, and source C++
    `Result.map(...)` / `Result.map2(...)` bridge outputs use that same single-field success-struct packing path.
  - `Result.ok()` (or `Result.ok(value)` for value-carrying results) constructs a success value.
  - `Result.error()` returns `true` when the result is an error.
  - `Result.why()` returns an owned `string` describing the error (heap-allocated by default).
  - Nested `Result.map(...)`, `Result.and_then(...)`, and `Result.map2(...)` expressions now preserve enough
    `Result` metadata for downstream `Result.error(...)`, `Result.why(...)`, `try(...)`, and `[auto]` bindings on the
    semantics side. Lambda-backed execution now covers `Result.map(...)`, `Result.and_then(...)`, and
    `Result.map2(...)` on IR-backed native/VM paths for the current `i32`/`bool`/`f32`/`string` payload subset as
    well as the C++ emitter path, including direct `Result.ok(...)` source expressions instead of only local- or
    definition-backed `Result` inputs, and the IR-side result resolver now recognizes those direct combinator
    expressions themselves so VM/native `try(...)`, `Result.error(...)`, and `Result.why(...)` can consume them
    without an intermediate binding. IR-backed auto bindings now also preserve that same `Result` metadata for direct
    `Result.ok(...)`, `Result.map(...)`, `Result.and_then(...)`, and `Result.map2(...)` initializer expressions, so
    later `try(...)`, `Result.error(...)`, and `Result.why(...)` calls can consume the bound local without forcing an
    explicit `Result<...>` annotation.
  - Direct `Result.ok(...)` expressions now participate in that same metadata flow, including `Result.and_then(...)`
    lambdas that return `Result.ok(...)` and need to inherit the input `Result` error domain instead of depending on
    an unrelated outer context. Unsupported wider payload kinds now reject through the same explicit IR-backed
    contract across `Result.ok(...)`, `Result.map(...)`, `Result.and_then(...)`, and `Result.map2(...)`, and
    VM/native now also resolve simple value-carrying `Result.and_then(...)` lambda returns that end in an explicit
    block-bodied `return(...)` or in a final `if(...)` expression whose `then(){...}` / `else(){...}` branches each
    produce a `Result`, instead of forcing those lambdas down to a single direct expression. The same IR inference now
    recognizes direct string `try(Result.map(...))`, `try(Result.and_then(...))`, and `try(Result.map2(...))`
    expressions for downstream string consumers such as `count(...)` and `print_line(...)` without an intermediate
    binding, and the lowerer’s call-base inference helper now resolves those same direct combinators for `try(...)`
    while reusing the configured inference state for ordinary mapped/chained arithmetic bodies instead of depending on
    the later dispatch-only path. That same call-base path also now resolves definition-backed and receiver-method
    `Result` sources inside those combinators, so named helpers such as `greeting()` and method-sugar helpers such as
    `reader.read()` can feed direct `Result.map(...)`, `Result.and_then(...)`, and `Result.map2(...)` consumers on
    IR-backed VM/native paths without an intermediate local.
  - `/std/file/*` now also exposes a stdlib-owned `FileError` namespace surface: `FileError.why(err)`,
    `FileError.status(err)`, `FileError.result<T>(err)`, `FileError.eof()`, and `FileError.isEof(err)` resolve
    through `/std/file/FileError/*` even in direct nested `Result.error(...)` / `Result.why(...)` expressions, while
    receiver-style `err.status()` / `err.result<T>()` method sugar now routes through the same stdlib-owned FileError
    helpers as `err.why()` / `err.isEof()`. The old root `/FileError/*` wrappers and package-level
    `fileErrorStatus(err)` / `fileErrorResult<T>(err)` compatibility helpers are removed, while `fileReadEof()` and
    `fileErrorIsEof(err)` remain as the narrow convenience helpers over that same type-owned implementation.
  - Stdlib containers use `Result<ContainerError>` / `Result<T, ContainerError>` as the shared error contract;
    `ContainerError.status(err)` / `ContainerError.result<T>(err)` now own the type-level packing surface while
    `containerErrorStatus(err)` / `containerErrorResult<T>(err)` remain compatibility helpers,
    `/ContainerError/status([ContainerError] err)` and `/ContainerError/result<T>([ContainerError] err)` are the
    public root wrappers for those packers, `/ContainerError/why([ContainerError] err)` is the explicit public
    wrapper for container error strings, `ContainerError.missingKey()`, `ContainerError.indexOutOfBounds()`,
    `ContainerError.empty()`, and `ContainerError.capacityExceeded()` are the canonical constructor helpers,
    `/ContainerError/missingKey()`, `/ContainerError/indexOutOfBounds()`, `/ContainerError/empty()`, and
    `/ContainerError/capacityExceeded()` expose the same values on the public root surface, compatibility wrappers keep
    `/ContainerError/missing_key()`, `/ContainerError/index_out_of_bounds()`, and
    `/ContainerError/capacity_exceeded()` available for migration, and unknown codes fall back to
    `"container error"`. On current IR-backed backends, `Result.ok(value)`
    plus `Result.map(...)`, `Result.and_then(...)`, and `Result.map2(...)` support `i32`, `bool`, `f32`, `string`,
    ordinary user structs whose payload storage stays on the existing stack-backed struct path, the single-slot
    int-backed stdlib error structs (`FileError`, `ImageError`, `ContainerError`, `GfxError`), packed
    `File<Mode>` handles, and `Buffer<T>` handles on VM plus native codegen when downstream `try(...)` consumers
    stay explicitly typed. Direct `Result.ok(...)` plus downstream `try(...)` also preserve `array<T>` /
    `vector<T>` handles whose element kinds fit the current collection contract, and `map<K, V>` handles whose
    key/value kinds fit that same map contract. Native executable `Result<Buffer<T>, GfxError>` values preserve
    `try(...)`, `Result.error(...)`, and success/error `Result.why(...)` on that same explicitly typed path.
    Downstream `try(...)` preserves those direct handle/error-struct payloads, rebuilds single-slot struct payloads,
    and keeps multi-slot struct payloads on that same pointer-backed struct path on VM/native. Other wider payloads
    remain unsupported; add a concrete Result payload TODO before widening that IR-backed payload contract.
    Value-carrying
    container helpers such as `mapTryAt` can now return string values when the underlying container path supports
    them.
  - Canonical and experimental stdlib gfx use `Result<GfxError>` / `Result<T, GfxError>` as their shared error
    contract; `GfxError.why(err)`, `GfxError.status(err)`, the public `windowCreateFailed()` /
    `deviceCreateFailed()` / `swapchainCreateFailed()` / `meshCreateFailed()` / `pipelineCreateFailed()` /
    `materialCreateFailed()` / `frameAcquireFailed()` / `queueSubmitFailed()` / `framePresentFailed()`
    wrappers, and receiver-style `err.why()` / `err.status()` / `err.result<T>()`
    explicitly prefer the matching canonical or experimental `GfxError` helper surface. The old canonical
    root `/GfxError/*` compatibility wrappers plus the package-level `gfxErrorStatus(err)` / `gfxErrorResult<T>(err)`
    aliases are removed so mixed canonical+experimental imports no longer need a second public gfx error path or a
    second package helper layer.
  - The postfix `?` operator unwraps a `Result` or propagates the error (see Error Handling).
    - `Result.map(result, fn)` applies `fn` to the success value (if any) and returns a new `Result`.
    - `Result.and_then(result, fn)` (a.k.a. bind) applies `fn` to the success value and flattens the result.
    - `Result.map2(a, b, fn)` applies `fn` if both results are ok; otherwise returns the first error.

Signature sketches (not surface syntax; a stored `T -> U` callable is spelled `Function<U, T>`, see
value-lifecycle.md, Function values):
```
Result.map      : Result<T, Error> x (T -> U) -> Result<U, Error>
Result.and_then : Result<T, Error> x (T -> Result<U, Error>) -> Result<U, Error>
Result.map2     : Result<A, Error> x Result<B, Error> x (A, B -> C) -> Result<C, Error>
```

Example (surface):
```
[return<Result<i32, ParseError>>]
parse_and_double([string] text) {
  return(Result.map(parse_i32(text), []([i32] v) { return(multiply(v, 2i32)) }))
}

[return<Result<i32, FileError>>]
sum_two_files([string] a, [string] b) {
  return(Result.map2(read_i32(a), read_i32(b), []([i32] x, [i32] y) {
    return(plus(x, y))
  }))
}
```

### UI events (`/std/ui/events`)

`import /std/ui/events/*` gives the values a control's handler receives (docs/WebAppsPlan.md, principle 7). Web pages
(`/std/web`) and native windows pass the same structs, so a handler written as a
`Function<void, Reference<State>, PointerEvent>` can be attached to either. The module is pure data with small helpers
and no host calls, so it runs on the VM, the native backend and optexe.

- `PointerButton` is an enum: `none` (-1), `primary` (0), `secondary` (1), `middle` (2), the values of the retired
  `/std/ui` event records. `none` is a pointer that moved, or a click no button made, such as a key that activated a
  button.
- `Modifiers` holds the modifier keys as `bits`: 1 shift, 2 control, 4 alt (option), 8 meta (command), also named
  `Modifiers.shiftBit`, `controlBit`, `altBit` and `metaBit`. `shift()`, `control()`, `alt()` and `meta()` test one
  key and `any()` whether any is held. A set is written by adding bits:
  `Modifiers{[bits] Modifiers.shiftBit + Modifiers.metaBit}` (there are no bitwise operators).
- `Key` is a character, by its Unicode code point (`code` 0 or more), or a named key (`code` below 0): `Key.enter`,
  `tab`, `escape`, `backspace`, `delete`, `left`, `right`, `up`, `down`, `home`, `end`, `pageUp` and `pageDown`
  (-1 to -13). `characterKey(codePoint)` makes the key of a character; `isCharacter()` and `character()` (the code
  point, or -1) read it.
- `PointerEvent` (`onClick`, `onMouseDown`, `onMouseUp`, `onMouseOver`): `button`; `x` and `y` in the control's own
  coordinates from its top left corner, valid when `hasPosition`; `modifiers`; `clickCount` (1 for a single click, 2
  for a double click; for a native press or release, the count of the click it belongs to, 2 for the second press
  of a double click; 0 for a move, and for a press or release on the web). A web form post has no position.
- `KeyEvent` (`onKeyDown`, `onKeyUp`, while the control has focus): `key`, `modifiers` and `isRepeat`, true for a key
  down the system repeats while the key is held (`repeat` is a loop keyword, so the field is not called that).
- `ChangeEvent` (`onChange`): `checked`, the new state of a check box, and `index`, the newly selected item of a popup
  or list (-1 when none). Each control sets the field it has.

The enums and `Key` and `Modifiers` compare with `==` and `!=`: they are declared with `generate(Equal, NotEqual)`,
and `==` on such a struct calls its generated `Equal`, `!=` its generated `NotEqual` (docs/spec/transforms.md). A
`PointerButton` is an enum rather than a sum because its alternatives carry no payload, and an enum entry reads as a
value (`PointerButton.secondary`) in a comparison, where a sum would need a `pick`. A handler reads:

```prime
[effects(heap_alloc)]
on_click([Reference<Editor>] editor, [PointerEvent] event) {
  if(event.button == PointerButton.secondary) {
    editor.showMenu(event.x, event.y)
  } else {
    if(event.modifiers.shift()) {
      editor.extendSelection()
    }
  }
}
```

### Native UI (`/std/ui/native`)

`import /std/ui/native/*` brings in the native-widget surface of the version-0 C ABI
([Native UI plan](../NativeUiPlan.md)). It runs on the VM only: the embedding runner binds the `ps_ui_*` host
functions (`primec/ui/NativeUiBindings.h`) to a backend (headless in tests, AppKit on macOS).

- `start_app(name) -> App`: starts the app on the calling thread; `app.started` is false when it cannot start.
- `App`: `window(title, width, height) -> Window`, `textView() -> TextView`, `menu(title) -> Menu`,
  `waitEvent() -> AppEvent`, `openPanel(title)` and `savePanel(title, suggestedName)` (empty text means cancelled; test the result with
  `panelChosen() -> bool`, since `count` of a host-returned text is not yet reliable),
  `alert(message, detail, buttons) -> i32` (button titles separated by `\n`, returns the pressed index), `quit()`,
  the preferences `boolPref(key, fallback) -> bool` / `setBoolPref(key, value) -> bool` (booleans kept between runs), and integers and lists of texts `intPref(key, fallback)`, `setIntPref`, `listClear(key)`, `listAdd(key, item)`, `listCount(key)`, `listGet(key, index) -> String`, and the recent files `addRecent(path)`, `recentPath(index) -> String`, `clearRecent()`.
- `Window`: `setPreview(bool)`, `hasPreview()` and `refreshPreview()` (the markdown preview beside the window's text view), `setAutosaveName(name)` (remember the window's frame between runs), `setTitle`, `setContent(textView)`, `content() -> TextView`, `setEdited(bool)`, `show()`, `close()`,
  `addTab(window)` (joins that window to this one's tab group and selects it), `addBackgroundTab(window)` (joins it
  right after this window, neither shown nor selected: the platform builds it when it is first picked; for a restored
  session), and the document path kept with the window:
  `setPath(file)`, `path()`, `hasPath()`, `tabCount()`, `tabAt(index)` (tab-bar order) and `select()`. `App` adds `activeWindow()`, `windowCount()`, `windowAt(index)`.
- `TextView`: `text()`, `setText(text)`, `widget()` (for the pointer and key handlers), `setMonospace(bool)`, `setLineNumbers(bool)` (a bar of line numbers, one per paragraph, left of the text), `setWrapping(bool)` (wrap long lines, the default), `isModified()`, `clearModified()`, and
  `loadFile(path)` / `saveFile(path)` (the backend reads or writes the UTF-8 file itself; `false` means see
  `app.lastError()`; a successful load leaves the view unmodified, a save does not clear the modified flag), and
  `clearStyles()` / `addStyle(startByte, endByte, rgb, flags)` (colour and bold/italic over UTF-8 byte ranges), and `highlight(path)` (syntax highlighting chosen by the file
  name's extension).
- Controls for a settings window (`Widget`, `Label`, `CheckBox`, `Popup`, `Form`, `Tabs`, made by `App.label(text)`, `checkBox(title, checked)`, `popup()`, `form()`, `tabs()`): each has `widget()`; `CheckBox`: `setChecked(bool)`, `isChecked()`; `Popup`: `addItem(title)`, `select(index)`, `selected() -> i32`; `Form`: `heading(title)`, `row(label, widget)`, `control(widget)`; `Tabs`: `add(title, widget)`, `select(index)`; `Label`: `setText(text)`. `Window.setContentWidget(widget)` and `setSettingsStyle()`, `TextView.setFontSize(points)`, `App.setPreviewScrollMode(mode)` (0 independently, 1 linearly, 2 by block, 3 the page follows the text cursor), and the event `AppEvent.controlChanged` (the `Widget` the user changed). `ItemList` (`App.itemList()`; `widget()`, `addItem(text)`, `setItem(index, text)`, `count()`, `setSelected(index, bool)`, `isSelected(index)`; the user's change of the selection is a `controlChanged`, the program's own changes make no event), `Button` (`App.button(title)`; `widget()`, `setTitle(text)`; a click is a `controlChanged`), `AppEvent.openFile` (the system asks the app to open a file; `App.eventText()` is its path), and the file types: `App.documentTypeCount()`, `documentTypeName(i)`, `documentTypeExtensions(i)` (the types the app declares), `appPath()`, `defaultApp(extension)`, `setDefaultApp(extensions, application)` (comma separated extensions; "" is this app).
- `Menu`: `item(title, shortcut, commandId)`, `setChecked(commandId, bool)` (a check mark on those items), `followPreview(commandId)` (the check mark shows whether the active window has a preview), `submenu(title, menu)`, `clear()`, `addRecentFiles(firstCommandId, maxItems) -> i32` (one item per recent file; `addRecentFileCommands(range)` with handlers), `separator()`, the standard items `undo redo cut copy paste selectAll
  find quit about hide hideOthers minimize zoom bringAllToFront fullScreen help showTabBar mergeAllWindows
  moveTabToNewWindow nextTab previousTab`, and `addToBar()`,
  `addToBarAsApp()`, `addToBarAsWindow()`, `addToBarAsHelp()`. Shortcuts are portable (`"cmd+s"`).
- `AppEvent` is `quitRequested | command(i32) | windowCloseRequested(Window) | textChanged(Window)`; typing,
  selection, clipboard and undo stay inside the native widget.

Example (`examples/native_ui/hello_window.prime`):
```
import /std/ui/native/*

[return<int>]
main() {
  [App] app{start_app("Hello")}
  [Window] window{app.window("Untitled", 720i32, 480i32)}
  [TextView] editor{app.textView()}
  window.setContent(editor)
  editor.setText("Hello, native UI")
  window.show()
  [mut] running{true}
  while(running) {
    pick(app.waitEvent()) {
      command(id) {
      }
      windowCloseRequested(w) {
        [Window] target{w}
        target.close()
        running = false
      }
      textChanged(w) {
        [Window] target{w}
        target.setEdited(true)
      }
      quitRequested {
        running = false
      }
    }
  }
  return(0i32)
}
```

Copy a `pick` payload into a local before calling methods on it (`[Window] target{w}`): method calls directly on a
payload binding do not lower yet (TODO-5544).

#### Handlers (`AppHandlers<State>`)

Controls take handler closures instead of a loop over `waitEvent` (docs/WebAppsPlan.md, principle 7; the table of
events is in [Native UI plan](../NativeUiPlan.md) section 5). `AppHandlers<State>{[state] value}` holds the app's
state and the handlers, each registered once at setup, and `app.run(handlers)` waits on the events and calls their
handlers until the app quits. A handler is a `Function<void, Reference<State>, Event>`, with `Event` a
`/std/ui/events` struct (`import /std/ui/events/*`), the type a `/std/web` control takes too:

- `command(handler) -> Command` (a `PointerEvent` handler) and `Menu.commandItem(title, shortcut, command)`,
  `Toolbar.commandButton(title, symbol, command)`, `Menu.setCommandChecked(command, checked)`: a menu item and a
  toolbar button with the same `Command` call the same closure. The event is a click of the primary button with no
  position (`clickCount` 1): a command carries no pointer details. Ids start at `first_command_id()` (1000000), in
  the order of registration. `Menu.followPreviewCommand(command)`, `Window.selectToolbarCommand(command)` and
  `Window.runScriptCommand(name, text, command)` (the command arrives when the run ends) take a `Command` too.
- `commandRange(count, handler) -> CommandRange` (`first`, `count`; `at(offset) -> Command`): `count` consecutive
  commands with one `Function<void, Reference<State>, i32>` handler, which gets the offset of the chosen one. The
  controls that take consecutive ids take the range: `Toolbar.commandSegments(titles, range)` (one segment per
  command) and `Menu.addRecentFileCommands(range) -> i32` (at most one recent file per command; the offset is the
  index `App.recentPath` takes).
- `onClick(button, handler)` (`PointerEvent`, as above); `onCheckBoxChange(checkBox, handler)`,
  `onPopupChange(popup, handler)`, `onListChange(itemList, handler)` (`ChangeEvent`: `checked`, or `index`, the
  first selected row of a list or -1). For a control made after setup (a window built on first use),
  `onClickOf(find, handler)`, `onCheckBoxChangeOf`, `onPopupChangeOf` and `onListChangeOf(find, handler)` take a
  `find` closure (`Function<u64, Reference<State>>`) that gives the control's handle from the state when a control
  changes, 0 while there is none.
- `onMouseOver(widget, handler)`, `onMouseDown`, `onMouseUp` (`PointerEvent`) and `onKeyDown`, `onKeyUp`
  (`KeyEvent`, while the widget has focus) take any control or text view as a `Widget` (`button.widget()`,
  `textView.widget()`). The pointer events have a position from the widget's top left corner (a text view's visible
  part) and the modifiers; a press or release has its button and click count, the pointer entering has no button
  and `clickCount` 0. A text view keeps the keys it uses: its key handlers see only the others, such as Escape and
  Command combinations it has no binding for. Only widgets with such handlers are watched by the platform, and the
  last handler registered for an event of a widget is the one called. A whole click on a button calls its press,
  release and click handlers in that order.
- `onFirstFrame(handler)` (`Function<void, Reference<State>, Window>`) runs once, after the first window the app
  showed has been drawn, with that window: the place for launch work the first frame does not need, such as restoring
  the last session (PrimeScroll). A file the system asked the app to open at launch comes to `onOpenFile` before it.
  On a platform that cannot report the first frame (`PS_UI_FEATURE_FIRST_FRAME` of the C ABI) it runs before the
  first event. and `onTextChange(handler)` (`Function<void, Reference<State>, Window>`, for every
  window); without a close handler a close request closes the window. `onOpenFile(handler)` gets the path as a
  `String`. `onQuit(handler)` is a `Function<bool, Reference<State>>`: `run` returns when it gives `true`.
- `afterEvent(handler)` (`Function<bool, Reference<State>>`) runs after every event, whether a handler took it or
  not: the place to bring what the handlers changed on screen once. `run` returns when it gives `false`.
- `run` returns on a request to quit that no quit handler refuses, and after a close request when no window is open
  (with an `afterEvent` handler, when that gives `false` instead). Events without a handler are dropped. The last `onWindowClose`, `onTextChange`, `onOpenFile` or `onQuit` registered
  is the one called. `waitEvent` stays for programs with their own loop.

Example (`examples/native_ui/hello_button.prime`):
```
[return<int> effects(heap_alloc)]
main() {
  [App] app{start_app("Hello Button")}
  [Window] window{app.window("Hello", 320i32, 120i32)}
  [Button] hello{app.button("Say hello")}
  window.setContentWidget(hello.widget())
  [AppHandlers<Greeter> mut] handlers{AppHandlers<Greeter>{[state] Greeter{[button] hello}}}
  [mut] sayHello{[mut said{0i32}]([Reference<Greeter>] greeter, [PointerEvent] event) {
    said = said + 1i32
    greeter.say(greeting(said))
  }}
  handlers.onClick(hello, sayHello)
  window.show()
  app.run(handlers)
  return(0i32)
}
```

The closure is bound to a local before it is passed, because a lambda whose capture list starts with `mut` does not
parse as a call argument yet (TODO-5713); a lambda with `[]` or plain captures can be written in the call.

### Operating system (`/std/os`)

`import /std/os/*` gives a long-running program such as a server its configuration, a clock and a clean
stop. The functions are `[host]` calls answered by the POSIX shim `stdlib/std/os/os_posix.cpp`, linked into an
`--emit=optexe` program:

```bash
primec --emit=optexe server.prime -o server --link-arg stdlib/std/os/os_posix.cpp
```

- `envOr(name, fallback) -> String`: the environment variable `name`, or `fallback` when it is not set.
- `monotonicMs() -> i64`: milliseconds on `CLOCK_MONOTONIC`; it never goes backwards and is not wall-clock time, so
  only differences mean anything (idle timeouts).
- `stopRequested() -> bool`: true once the process has received SIGTERM or SIGINT. The shim installs both handlers
  when the program starts; they set a flag and do not restart interrupted system calls (no `SA_RESTART`), so a
  blocking socket call returns `EINTR` and the loop around it can check the flag and stop.
- `logLine(line) -> i32`: writes the `String` `line` and a line feed to stderr in one go. stderr is not buffered,
  so a supervisor sees the line at once. Returns the bytes written, or -1.
- `cpuCount() -> i32`: how many processors are online, at least 1 (`sysconf(_SC_NPROCESSORS_ONLN)`): how many
  threads can run at once.
- `blockStopSignals() -> i32`: blocks SIGTERM and SIGINT on the calling thread and returns 0, or -100 - the error.
  A worker task calls it first, so a signal does not interrupt the worker's own reads and writes (`serveWorkers`
  does this for its workers). A signal that arrives before a worker has called it can be delivered to that worker;
  the flag is still set, and a loop that must see it at once watches `stopSignalFd()`.
- `stopSignalFd() -> i32`: a descriptor that is readable from the moment the process has received SIGTERM or
  SIGINT, and stays readable; a negative code when it cannot be opened. The signal handler writes to a pipe, so it
  does not matter which thread the signal was delivered to, or whether it arrived before the first call. A loop
  that waits in `/std/net` `poll` watches it while `stopRequested()` is false: a signal that arrives between the
  check and the wait then ends the wait at once. Do not read from it or close it.

- `fileSize(path) -> i64` (`file_read`): the size of the regular file `path` (a `String`), -1 for a file that is not
  regular (a directory), or -100 - errno.
- `readFile(path, out, maxBytes) -> i32` (`file_read`, `heap_alloc`): replaces the `String` `out` with the bytes of
  the file (binary safe). Returns 0, -2 when the file is over `maxBytes`, or the negative code of `fileSize` or
  -100 - errno of a failed read.

The other functions take no effect. Like every host call they leave the deterministic subset of the language
(docs/spec/host-and-core-library.md), and a program that never calls them links without the shim. The VM and
`primevm` have no binding for them and report `unbound host function: ps_os_env_or ...`.

```prime
import /std/os/*
import /std/text/*

[return<int> effects(heap_alloc, io_out)]
main() {
  [String] port{envOr(stringFrom("PORT"), stringFrom("8080"))}
  [i64] start{monotonicMs()}
  print_line("serving")
  while(!stopRequested() && monotonicMs() - start < 1000i64) {
  }
  return(port.count())
}
```

### Network (`/std/net`)

`import /std/net/*` gives a server TCP sockets over IPv4 and a pipe to wake a thread that waits on them. The `std::net` functions call `[host]`
functions answered by the POSIX shim `stdlib/std/net/native/net_shim.cpp`, linked into an `--emit=optexe` program
(usually together with `/std/os`, whose stop flag ends the accept loop):

```bash
primec --emit=optexe server.prime -o server --link-arg stdlib/std/net/native/net_shim.cpp \
  --link-arg stdlib/std/os/os_posix.cpp
```

A socket is an `i32` descriptor. Byte buffers are a `vector<u8>` slice's `.data` followed by its `u64` length
(docs/spec/host-and-core-library.md, Byte buffers). Every call that can block takes `timeoutMs` (negative waits
forever), and every call returns a neutral negative code on failure instead of an errno, so the same checks work on
Linux and macOS:

| Code | Function | Meaning |
| --- | --- | --- |
| -1 | `timeoutCode()` | the call did not finish within `timeoutMs` |
| -2 | `closedCode()` | the peer closed or reset the connection (`ECONNRESET`, `EPIPE`, `ENOTCONN`, `ECONNABORTED`) |
| -3 | `againCode()` | the socket was not ready (`EAGAIN`) |
| -4 | `interruptedCode()` | a signal arrived (`EINTR`): with `/std/os` linked, check `stopRequested()` |
| -100 - errno | | anything else, for example -101 for `EPERM` when a sandbox forbids sockets |

- `listen(port, backlog) -> i32`: a listening socket on every interface (`SO_REUSEADDR`); port 0 picks a free port.
- `listenLoopback(port, backlog) -> i32`: the same on 127.0.0.1 only, for tests and local tools.
- `localPort(socket) -> i32`: the port a socket is bound to (the one picked for port 0).
- `accept(listener, timeoutMs) -> i32`: the next connection.
- `recv(socket, buffer, length, timeoutMs) -> i64`: reads up to `length` bytes; 0 means the peer closed.
- `send(socket, data, length, timeoutMs) -> i64`: writes all `length` bytes; a timeout or signal after some bytes
  returns the count written so far. A peer that has gone gives `closedCode()`, never `SIGPIPE`.
- `poll(interest, length, timeoutMs) -> i32`: waits until one of the marked sockets is readable: a listener has a
  connection waiting, or a connection has data, end of input or an error. `interest` is a byte buffer with one byte
  per descriptor (`length` bytes); a nonzero byte at index fd watches fd. On return each watched byte is 2 when that
  socket is ready and 1 when it is not. Returns how many sockets are ready, `timeoutCode()` when none became ready in
  time, or `interruptedCode()` when a signal arrived.
- `close(socket) -> i32`: 0 or a negative code.
- `connectLoopback(port, timeoutMs) -> i32`: a client connection to 127.0.0.1, for tests and health checks.
- `wakePipe() -> WakePipe`: a pipe with which one thread wakes another that waits in `poll`. `WakePipe` has two
  descriptors, `reader` and `writer`, both non-blocking; when opening fails, `reader` is the negative code. The
  waiting thread marks `reader` in its `poll` interest beside its sockets. Close both ends with `close`.
- `wake(writer) -> i32`: makes `reader` readable, from any thread. It never blocks, and wakes that have not been
  taken yet count as one. Returns 0 or a negative code.
- `takeWakes(reader)`: takes the pending wakes, so `reader` is readable again only after the next `wake`. Take them
  before looking at what the wake was about, so that a wake that arrives meanwhile is not lost.

The wrappers carry `effects(net_io)` (docs/spec/language-core.md, Effects); wasm and GLSL reject that effect. The
shim installs no signal handlers and keeps no state. The VM and `primevm` have no binding for the host functions and
report `unbound host function: net_listen ...`; native, wasm and GLSL reject host calls. POSIX only.

`examples/apps/http_hello/main.prime` is a complete server on `/std/http/server`: a `Router` behind
`withRequestLog`, served until SIGTERM, with the port and bind address taken from the environment.

```prime
import /std/collections/*
import /std/net/*

[return<int> effects(heap_alloc, net_io)]
main() {
  [i32] listener{listenLoopback(0i32, 4i32)}
  [i32] client{connectLoopback(localPort(listener), 1000i32)}
  [i32] server{accept(listener, 1000i32)}
  [vector<u8> mut] message{vector<u8>(104u8, 105u8)}
  [mut] outgoing{message.sliceMut(0i32, 2i32)}
  send(client, outgoing.data, 2u64, 1000i32)
  [vector<u8> mut] inbox{vector<u8>(0u8, 0u8, 0u8, 0u8)}
  [mut] window{inbox.sliceMut(0i32, 4i32)}
  [i64] count{recv(server, window.data, 4u64, 1000i32)}
  close(client)
  close(server)
  close(listener)
  return(convert<i32>(count))
}
```

### HTTP (`/std/http`)

`import /std/http/*` parses HTTP/1.1 requests and builds responses. It is pure code over bytes that have already
arrived, so it runs on the VM and every backend. The subset is deliberately small: bodies by `Content-Length` only,
one request per connection (`Connection: close`), and no chunked encoding, keep-alive or TLS. Text parameters are
`StringView`s, and a string literal passes to one as it is: `request.header("Host")`, `response.text("ok",
"text/plain")` (docs/spec/strings-and-views.md, Current surface of the text view).

- `parseRequest(text, maxBodyBytes) -> Request` parses one request from the start of a `StringView`. The request's
  `outcome()` is 0 when it is complete (`isComplete()`), 1 when more bytes are needed (`needsMore()`), or the status
  to answer:
  - 400 for a malformed request line, header line or `Content-Length`;
  - 413 for a body over `maxBodyBytes`;
  - 501 for any `Transfer-Encoding`;
  - 505 for a version other than `HTTP/1.0` or `HTTP/1.1`.

  A request line that is already wrong is rejected before the head is complete. `length()` is how many bytes the
  complete request took; bytes after it are left alone.
- `parseRequestBytes(bytes, maxBodyBytes)` does the same over a `ReadSlice<u8>`, such as a receive buffer. Bytes that
  end inside a UTF-8 sequence need more; bytes that cannot be UTF-8 answer 400.
- `Request` is a carrier. Its fields are `StringView`s into the parsed bytes, so it allocates nothing and is valid
  while those bytes are:
  - `method`, `target`, `path` (the target before `?`), `query` (after it, without the `?`), `version` and `body`;
  - `headers`, the header lines as sent.
  - `route`, the pattern of the route that `Router.call` matched (see `pathParam` below); empty otherwise.

  `header(name)` gives the value of the first header with that name, compared ASCII case-insensitively, without
  surrounding white space, or an empty view when the header is absent. The other header accessors are
  `hasHeader(name)`, `headerCount()`, `headerName(i)` and `headerValue(i)`. Header lookups scan the header block; there
  is no header table.

  Query and form parameters are read the same way, by scanning:
  - `queryParam(name) -> String` gives the decoded value of the first parameter of `query` whose decoded name is
    `name`; `formParam(name) -> String` does the same over the body when it is a form. Parameters are separated by
    `&`; a parameter is `name=value` or a bare `name`, and its value is everything after the first `=`. Decoding
    turns `+` into a space and `%XX` (two hex digits of either case) into that byte, in names and values alike.
  - The value is always an owned `String`, also when nothing needed decoding: one return type is simpler than a view
    or a copy depending on the bytes, and a parameter value is small. It is valid after the request bytes are reused.
  - The value is empty when the parameter is missing, has no value (`a=` or `a`), holds a malformed escape (a `%` not
    followed by two hex digits) or decodes to bytes that are not UTF-8. Reading a parameter never faults.
  - `hasQueryParam(name)` and `hasFormParam(name)` tell a missing parameter from an empty one: they are true for `a=`
    and `a`, and for a parameter whose value is malformed. With repeated parameters the first wins, as with headers.
  - A body is a form when `isFormBody()`: its `Content-Type` is `application/x-www-form-urlencoded`, compared ASCII
    case-insensitively after dropping parameters such as `; charset=utf-8` and surrounding white space. For any other
    body `formParam` is empty and `hasFormParam` false. Multipart bodies are not decoded.

  ```prime
  [String] name{request.queryParam("name")}         // "/hello?name=Ada%20L" gives "Ada L"
  [String] title{request.formParam("title")}        // a body "title=Buy+milk" gives "Buy milk"
  ```
- `Response` is built by a handler and serialized by the server:
  - `status` is 200 unless set, and `body` is a `String`;
  - `header(name, value)` adds a header line, dropping CR and LF bytes so that a value cannot end the head;
  - `text(body, contentType)` replaces the body and adds `Content-Type`;
  - `html(page)` replaces the body with `page`, a `String` such as the `out` of a `/std/html` `Writer`, and adds
    `Content-Type: text/html; charset=utf-8`. It takes the `String` rather than the `Writer` so that `/std/http` does
    not depend on `/std/html` and a page can come from anywhere; the bytes are copied into the body, whose buffer is
    kept between requests;
  - `redirect(location)` answers 303 See Other with a `Location` header, the reply to a form POST that the browser
    follows with a GET (post/redirect/get); `redirect(status, location)` takes another status, such as 301, 302, 307
    or 308. Both empty the body, so the response serializes with `Content-Length: 0`;
  - `clear()` makes it an empty 200 again and keeps its buffers.

  `serialize()` gives the bytes to send: the status line with its reason phrase (`reasonPhrase(code)`, empty for codes
  it does not know), the header lines, `Content-Length`, `Connection: close`, a blank line and the body. Statuses 1xx,
  204 and 304 get neither a `Content-Length` nor a body. `statusResponse(code)` is the bare answer to a rejected
  request.
- `Router` is a handler that dispatches to other handlers. Each route is a method, a pattern and a
  `Function<void, Request, Reference<Response>>`. A pattern is a prefix ending in `*`, which matches any path that
  starts with the text before the `*` (its other bytes, `:` included, are literal), or else a path whose segments
  (between `/`s) must match the request path's segments one for one: a `:name` segment matches any one non-empty
  segment, every other segment only itself. So `/todos/:id` matches `/todos/7` but not `/todos`, `/todos/` or
  `/todos/7/x`, and `/users/:user/posts/:post` matches `/users/ada/posts/3`. Routes are added with
  `get(pattern, handler)`, `post`, `put`, `delete` or `route(method, pattern, handler)`; the handler is a lambda, a
  struct with `call` or a named definition, converted to the `Function` where it is passed. `call(request,
  response)` runs the first route that matches, in the order the routes were added, so a literal `/todos/new` listed
  before `/todos/:id` wins for that path. When none matches, the status is 405 if some route matches the path under
  another method, and 404 otherwise. `Router` has a `call` method, so it can be passed wherever a handler is
  expected, such as `serve`, middleware or a generic parameter.

  `request.pathParam(name)` is the request path's segment at the place of the `:name` segment, a `StringView` into the
  path as sent (not percent-decoded), or an empty view when the matched pattern has no such segment. Before it calls
  the handler, `call` sets the request's `route` field to the matched pattern, and `pathParam` walks that pattern and
  the path again. The request is passed by value, so the handler gets its own copy with `route` set. Keeping the
  pattern rather than a table of captured segments keeps `Request` a small carrier of views with no limit on the
  number of parameters and nothing to copy per parameter; the pattern is a view into the program's string table, so
  it stays valid. `route` is empty outside a router, so `pathParam` is then empty; a router inside another router's
  handler sets it to its own pattern.

  ```prime
  [Router mut] router{Router{}}
  router.get("/health", []([Request] request, [Reference<Response>] response) {
    response.text("ok", "text/plain")
  })
  router.get("/todos/:id", []([Request] request, [Reference<Response>] response) {
    response.text(request.pathParam("id"), "text/plain")
  })
  router.call(request, location(response))
  ```
- `AppRouter<State>` is a `Router` that also holds the app's state and passes it to every handler, so routes can
  share and change it. Its `state` field is a `State`, set at construction (`AppRouter<TodoList>{[state]
  TodoList{}}`) and readable afterwards (`app.state`). Each route's handler is a
  `Function<void, Reference<State>, Request, Reference<Response>>`: the state comes first, as a reference to the
  router's own field. Routes are added and matched exactly as for `Router` (`get`, `post`, `put`, `delete`, `route`,
  the same patterns, 404 and 405, `pathParam`), and `AppRouter` has the same `call(request, response)`, so it is
  passed to `serve` or middleware as it is: `serve(withRequestLog(app), config)`.

  The state is a parameter rather than a capture because a handler stored in a `Function` cannot hold a `[ref]`
  capture (docs/spec/value-lifecycle.md, Lambdas): a closure that borrowed an outer `TodoList` could outlive it. A
  value capture would give each route its own copy, so a todo added by one route would be invisible to another. The
  router owns the one copy and lends it to the handler for the length of the call; the server calls one handler at
  a time (`serve` has one thread, `serveWorkers` holds the handler in a `Mutex`), so that needs no locking. Keeping the state in the router, rather than passing it to `serve` next to the router, keeps
  `serve` and middleware unchanged: they see one handler. `Router` stays as it is for handlers without state.

  ```prime
  [struct]
  Counter() {
    [i32 mut] clicks{0i32}
  }

  [AppRouter<Counter> mut] app{AppRouter<Counter>{[state] Counter{}}}
  app.post("/click", []([Reference<Counter>] counter, [Request] request, [Reference<Response>] response) {
    counter.clicks = counter.clicks + 1i32
    response.redirect("/")
  })
  ```

  A lambda or a named definition converts to the handler `Function` at `get`, `post` and the others, as for
  `Router`: the implicit conversion happens at a method of a generic struct instance, whose parameter types are known
  once the struct is instantiated. `examples/apps/todo_web/main.prime` is a whole app on it.
- `routeMatches(pattern, path)` tells whether a path matches a route pattern by the rules above, for page layers that
  keep their own route table, such as `WebApp<State>` of `/std/web`.
- A definition with plain struct parameters and a plain struct result (such as `parseRequest`) is one real call on the
  VM and under `--emit=optcpp`/`optexe` (docs/spec/vm-design.md, Real calls), so the number of call sites does not
  multiply the generated code.

### Web controls (`/std/web`)

`import /std/web/*` is the page layer over `/std/http` and `/std/html`: controls that take their handler closures
directly (docs/WebAppsPlan.md, principle 7). Code that uses it also imports `/std/html`, `/std/http` and
`/std/ui/events` for the types it names.

- `WebApp<State>` routes exactly as `AppRouter<State>` does: a `state` field set at construction
  (`WebApp<Greeter>{[state] Greeter{}}`), `get`, `post`, `put`, `delete` and `route` with handlers that take a
  `Reference<State>` first, the same patterns, 404 and 405, and `call(request, response)`, so it is passed to `serve`
  or middleware as it is. Moving an app from `AppRouter` to `WebApp` changes only the type.
- `app.button(id, title, handler)` registers a button and returns its `Control`. `handler` is a
  `Function<void, Reference<State>, PointerEvent>` (`/std/ui/events`): a lambda, a named definition or a closure
  bound to a local, which may hold `mut` captures of its own. The id names the generated route
  `POST /_on/<id>/click`; paths that start with `/_on/` belong to the controls and never reach the routes.
- A `Control` is the control's `id`, `title` and `target`, entries of the string table (`string`), and its `events`
  bits (see the handlers below), so it owns and borrows nothing and copies as three indices and an `i32`. A page
  route captures it by value (a `[hello]` capture list) or receives it as a parameter, and writes it with `hello.write(page)`
  (a `/std/html` `Writer`): `<form method="post" action="/_on/hello/click"><button type="submit">Say hello</button>
  </form>`, the title escaped. `clickRoute()` is the route as a `String`. `swapping(id)` returns the control with
  `target` set to a region's id (see Fragments below), and its form then carries `data-ps-target="id"`.
- A POST to a control's route calls its handler with the app's state and a `PointerEvent` of the `primary` button
  with `clickCount` 1, no modifiers and no position (`hasPosition` false): a form post carries none. It then answers
  303 to `backPath(request)`, the path and query of the request's `Referer` (the page the form was on) or `/` when
  there is none; only the path is kept, so the redirect stays on the server. Another method on a control's route is
  405 and a `/_on/` path no control has is 404.
- Each id is registered once. A second `button` with a taken id prints `duplicate control id: <id>` to stderr and
  stops the program (`panic`), at setup, before the server listens; the language has no error that `button` could
  return without every app writing `?` after each registration.

Controls are registered once, when the app is set up, never while a page renders: the closure lives in the app, so
its `mut` captures last from request to request, while a closure made during rendering would start again every
request. The page gets the control from registration, so a page can only write a control that has a handler. A
button that acts on one of many records, such as the Done button of each todo in `examples/apps/todo_web`, is still a
form posting to a route with the record's id in its path: a control is one button with one handler.

```prime
[return<WebApp<Greeter>> effects(heap_alloc, io_err)]
hello_app() {
  [WebApp<Greeter> mut] app{WebApp<Greeter>{[state] Greeter{}}}
  [mut] sayHello{[mut said{0i32}]([Reference<Greeter>] greeter, [PointerEvent] event) {
    said = said + 1i32
    greeter.say(greeting(said))
  }}
  [Control] hello{app.button("hello", "Say hello", sayHello)}
  app.get("/", [hello]([Reference<Greeter>] greeter, [Request] request, [Reference<Response>] response) {
    response.html(greeting_page(greeter, hello))
  })
  return(app)
}
```

`WebApp` is its own type in its own module rather than more methods on `AppRouter`, so that `/std/http` keeps
depending on neither `/std/html` nor `/std/ui/events`, and the page layer, where routes, HTML and events meet, has one
place to grow (fragments and hover and key handlers, below). `examples/apps/hello_button/main.prime` is a
whole app on it.

**Fragments.** A click can replace part of a page instead of loading the page again, with no JavaScript written by
the app (docs/WebAppsPlan.md, principle 4):

- `app.region(id, render)` registers a region. `id` is the id of the element that holds it, and `render` is a
  `Function<void, Reference<Writer>, Reference<State>>` that writes the element's contents from the state. The page
  writes the element and calls the same function inside it, so one function writes the region in the page and as a
  fragment, and the two cannot differ. A second `region` with a taken id prints `duplicate region id: <id>` and stops
  the program, as `button` does.
- A form names the region it replaces with `page.fragmentTarget(id)` (`/std/html`), which writes
  `data-ps-target="id"` on the form's start tag; a `Control` made with `swapping(id)` writes it on its own form.
- `writeFragmentScript(page)`, in the head, writes `<script src="/_web/app.js" defer></script>`
  (`fragmentScriptPath()`; the header's name is `fragmentHeader()`, and the script itself `fragmentScript()`). Paths that start
  with `/_web/` belong to the app: `GET /_web/app.js` answers the script as `text/javascript; charset=utf-8`, another
  method there is 405 and another path 404.
- The script handles the submit of a POST form that has `data-ps-target`: it stops the page load and posts the form's
  fields with `fetch`, as an urlencoded body, to the form's `action`, with the header `X-PrimeStruct-Fragment: <id>`
  and without following a redirect. An answer that carries the same header is a fragment: its body replaces the
  contents (`innerHTML`) of the element with that id. Any other answer (a redirect, an error, a failed connection)
  reloads the page, which then shows the state after the change. Requests go one at a time, so swaps land in the
  order the clicks were made; the forms inside a swapped region work again, because the script listens on the
  document.
- On the server, when a route or a control answers a request whose fragment header names a registered region with a
  303, the app replaces that answer with a 200: `Content-Type: text/html; charset=utf-8`,
  `X-PrimeStruct-Fragment: <id>` and the region's HTML from `render`. Handlers do not change: they make the change
  and redirect, as for a plain form, and the app turns the redirect into the fragment. Any other status, and a region
  id that no `region` registered, is sent as it is.
- Without the script (not loaded, blocked, or an old browser) the same forms post and redirect back, so every page
  works the same with a full load.

```prime
[effects(heap_alloc)]
write_todos([Reference<Writer>] page, [Reference<TodoList>] list) {
  for([i32 mut] index{0i32}; index < list.todos.count(); ++index) {
    write_todo(page, list.todos[index])            // each todo's forms call page.fragmentTarget("todos")
  }
}

app.region("todos", write_todos)                   // at setup; the page writes <ul id="todos"> around write_todos
```

The region's id travels in the request, rather than in a table from route to region in the app, so that any POST
route can answer a fragment, such as the Done button of each todo in `examples/apps/todo_web`, which is an ordinary
route with the todo's id in its path rather than a control. Turning the redirect into the fragment, rather than
letting handlers answer fragments themselves, keeps one handler for the plain form and the script. `render` writes
the contents rather than the element, so the page chooses the element (a `ul` for a list) and the function needs no
id. There is no client-side state or templating: the server writes all HTML, and the script only moves it.

The script is the fixed asset `stdlib/std/web/app.js`. The program carries the same bytes in a raw literal
(`stdlib/std/web/fragment_script.prime`), so a server binary reads no file to serve it. Its budget is **5120 bytes**
(5 KB), unminified and with its comments; it is 4914 bytes, of which the fragment part is about 1.5 KB and the
events below the rest. `tests/unit/backend_host/test_web.cpp` checks the budget and that the literal holds the
asset's bytes, and the todo_web loopback test that the server sends them.

**Hover, press, release and key handlers.** A control takes the other handlers of principle 7 too, registered at
setup like its click:

- `app.onMouseOver(control, handler)`, `onMouseDown` and `onMouseUp` take a
  `Function<void, Reference<State>, PointerEvent>`; `app.onKeyDown(control, handler)` and `onKeyUp` take a
  `Function<void, Reference<State>, KeyEvent>`. `control` is the `[Control mut]` that `button` returned: each
  registration adds the event to its `events` bits (`Control.mouseOverEvent` 1, `mouseDownEvent` 2, `mouseUpEvent` 4,
  `keyDownEvent` 8, `keyUpEvent` 16), so the page writes the control after its handlers are registered. A second
  handler for one event of a control prints `duplicate handler: <id> <event>`, and a control of another app
  `unknown control: <id>`; both stop the program.
- A control with events writes `data-ps-id="<id>"` and `data-ps-on="mouseover mousedown ..."` on its button, and
  `tabindex="0"` when it has a key handler, because some browsers leave a button out of the keyboard focus order.
- These events have no HTML-only form: only the fragment script sends them, each as a POST to
  `/_on/<id>/<event>` (`mouseover`, `mousedown`, `mouseup`, `keydown`, `keyup`) with an urlencoded body and, when the
  control's form has `data-ps-target`, the fragment header naming that region. The app decodes the body
  (`pointerEventOf(request)`, `keyEventOf(request)`), calls the handler and answers as for a click: 303 to the page,
  or the region's fragment when the request names one. A control without a region therefore reloads its page after
  each event, so a control that reacts to hover or keys should swap a region.

  | Event | Body | Decoded as |
  | --- | --- | --- |
  | `mouseover` | `button=-1&x=12&y=5&modifiers=0` | `PointerEvent`, `button` `none`, `clickCount` 0 |
  | `mousedown`, `mouseup` | `button=1&x=3&y=4&modifiers=9` | `button` -1 none, 0 primary, 1 secondary, 2 middle |
  | `keydown`, `keyup` | `key=97&repeat=1&modifiers=1` | `KeyEvent`: `Key{[code] key}`, `isRepeat` when `repeat` is 1 |

  `x` and `y` are the pointer's position in the control's box from its top left corner, rounded to whole pixels;
  `hasPosition` is true when `x` is present. `modifiers` holds the bits of `Modifiers` (1 shift, 2 control, 4 alt,
  8 meta). `key` is the code point of a key that types one character, or a named key's code (`Enter` -1 through
  `PageDown` -13); the script sends no event for other keys, such as a modifier pressed alone, or for a pointer
  button other than the three. A field that is missing takes the value of `PointerEvent{}` or `KeyEvent{}`.
- **Round trips.** Every event is a request and an answer, so the script keeps them few: the pointer must rest on a
  control for 100 ms before one `mouseover` is sent, and leaving it sooner sends nothing (moves within the control send
  nothing either). Requests go one at a time, as for clicks. Hover that only changes how a control looks, such as a
  highlight, belongs in CSS (`:hover`), which costs no request; a hover handler is for a change of the app's state.
- **Focus.** Key events go to the focused control only: the script reads `document.activeElement`, and a press on a
  control with key handlers focuses it, since some browsers do not focus a clicked button. When a swap replaces the
  focused control, the new control with the same `data-ps-id` gets the focus back, so typing continues.

```prime
[Control mut] pad{app.button("pad", "Pad", on_click).swapping("pad")}
app.onMouseOver(pad, show_hint)                    // ([Reference<Pad>] pad, [PointerEvent] event)
app.onKeyDown(pad, move_cursor)                    // ([Reference<Pad>] pad, [KeyEvent] event), while focused
```

### HTTP server (`/std/http/server`)

`import /std/http/server/*` runs the request loop over `/std/net`. It needs both shims at link time:

```bash
primec --emit=optexe app.prime -o app --link-arg stdlib/std/net/native/net_shim.cpp \
  --link-arg stdlib/std/os/os_posix.cpp
```

- `serve(handler, config) -> Result<ServeError>` serves until the process receives SIGTERM or SIGINT (`/std/os`
  `stopRequested()`), then returns `Result.ok()`.
- `serveUntil(handler, config, stop)` serves until `stop()`, any callable returning `bool`, is true.
- `serveWorkers(handler, config)` and `serveWorkersUntil(handler, config, stop)` are the same server on several
  threads ([Workers](#workers-serveworkers) below).
- **Handlers:** `handler` is any callable taking `(Request, Reference<Response>)`, such as a lambda, a struct with
  `call`, a `Router`, an `AppRouter` or a middleware closure. It is a generic parameter, so the call is direct and the
  handler keeps its state between requests.
- **The loop** of `serve` is single-threaded. It waits with `poll` on the listener and every open connection together and
  serves whichever is ready, so a client that connects and sends nothing (a browser's spare connection) or sends
  slowly holds only its own slot:
  1. Wait until a socket is ready or the first deadline passes, at most `acceptTimeoutMs`. That bounds how long the
     stop condition goes unchecked; a signal also ends the wait at once.
  2. Accept a waiting connection into a free slot. Up to `maxConnections` are open at once; while all are, new
     connections wait in the listen backlog until a slot frees (they are not answered 503).
  3. Read what has arrived on a ready connection until `parseRequestBytes` says the request is complete or
     rejected. The whole request must arrive within `readTimeoutMs` of the accept, and a client that is closed,
     silent or too slow is dropped without an answer.
  4. Call the handler with a cleared `Response`, write `serialize()` and close. Writing blocks the loop, for at most
     `writeTimeoutMs`; a response that fits in the socket's send buffer, as most do, is written at once.
- **Rejected requests** get a bare status (`statusResponse`) and never reach the handler: 400, 413, 501 and 505, plus
  413 for a request that does not fit in `maxRequestBytes`. The connection keeps its slot and its input is read and
  discarded until it pauses for 50 ms (at most 8 rounds), so the client sees the answer rather than a reset.
- **Memory:** each slot has its own `maxRequestBytes` buffer, allocated the first time the slot is used, so the
  buffers take at most `maxConnections * maxRequestBytes` (1 MiB with the defaults).
- **`ServerConfig`** fields and their defaults:
  - `port{8080}`; 0 picks a free port;
  - `backlog{64}`;
  - `maxRequestBytes{65536}`, the buffer that holds the head and body together;
  - `acceptTimeoutMs{500}`, `readTimeoutMs{5000}` and `writeTimeoutMs{5000}`;
  - `loopbackOnly{false}`, which listens on 127.0.0.1 only;
  - `announce{true}`, which prints `listening on <port>` to stderr once listening;
  - `maxConnections{16}`, how many connections are open at once (a value below 1 means 1);
  - `workers{0}`, how many worker tasks `serveWorkers` starts: 0 means one per processor (`/std/os` `cpuCount()`),
    and at most 64 are started. `serve` ignores it.
- **Middleware:** `withRequestLog(next)` wraps any handler and logs `METHOD target STATUS Nms` for each request
  through `logLine`. Middleware is a generic function that returns a closure capturing `next`, so wrapping is
  zero-cost and composes: `serve(withRequestLog(router), config)`. Do not erase a middleware chain into a `Function`
  (TODO-5677).
- **`ServeError`:** the only error is failing to listen. Its `code` is the `/std/net` code, and `why()` names the
  common causes (sockets not permitted, port in use).
- **Checking the result:** use `serve(...)?` in a `main` with `on_error<ServeError, handler>`; the handler prints
  `err.why()` and `main` exits with the error code.

```prime
import /std/http/*
import /std/http/server/*
import /std/text/view/*

[effects(io_err)]
log_serve_error([ServeError] err) {
  print_line_error(err.why())
}

[return<int> effects(heap_alloc, io_err, net_io) on_error<ServeError, log_serve_error>]
main() {
  [Router mut] router{Router{}}
  router.get("/", []([Request] request, [Reference<Response>] response) {
    response.text("hello\n", "text/plain")
  })
  serve(withRequestLog(router), ServerConfig{[port] 8080i32})?
  return(0i32)
}
```

#### Workers (`serveWorkers`)

`serveWorkers(handler, config) -> Result<ServeError>` serves like `serve`, with `config.workers` worker tasks beside
the loop; `serveWorkersUntil(handler, config, stop)` is its `serveUntil`. Both need
`effects(net_io, heap_alloc, io_err, task, lock, channel, atomic)`. A task is a thread on `optexe`; the VM has no
`/std/net`, so these run on `optexe` only.

- **The loop** only accepts and watches: it polls the listener, the open connections and a wake pipe (`/std/net`
  `wakePipe`). A readable connection leaves the watched set and goes to the workers over a `Channel`. The loop never
  waits for a worker, so a full set of busy workers does not stop it from accepting or from seeing the stop condition.
- **A worker** reads what has arrived on the connection. An incomplete request goes back to the loop to be watched
  again, so a client that stalls in the middle of a request holds its slot and never a worker. A complete request is
  answered by that worker: it calls the handler, serializes the response, writes it (at most `writeTimeoutMs`) and
  closes. Rejected requests get the same bare status as under `serve` and are drained by the loop.
- **The handler runs inside a mutex.** The handler moves into a `Mutex` (`/std/sync`), and each request calls it
  inside `locked`, one request at a time. A `Router`, `AppRouter<State>` or `WebApp<State>` therefore keeps plain
  state with no locking of its own, and its handlers are written exactly as for `serve`. Receiving, parsing,
  serializing and sending run in parallel; everything the handler does, building a page's HTML included, does not.
- **The handler must be sendable**, because the tasks share it: an app whose state holds a `Pointer` or a
  `StringView` is rejected when the server erases it into a `Function`, with the path to the field: ``closure
  cannot be a Function: capture `inner`, field `state` of WebApp<Borrowed>, field `bytes` of Borrowed is
  Pointer<u8>``.
- **Stopping:** when `stop()` is true the loop stops listening, closes the connections that wait for input and
  ends the workers; each worker finishes the request it is answering, and a connection still queued for a worker is
  closed unanswered. Then the call returns `Result.ok()`. The loop watches `/std/os` `stopSignalFd()` beside its
  sockets, so `serveWorkers` sees SIGTERM or SIGINT at once: whichever thread takes the signal, also while the
  workers are still starting, and also between the loop's check of `stop()` and its wait.
- **Errors:** failing to listen or to open the wake pipe is the `ServeError`, returned before any worker starts.
- **Memory:** the request buffers of all slots are one block of `maxConnections * maxRequestBytes` bytes,
  allocated when the server starts (`serve` allocates each slot's buffer on first use).

```prime
import /std/http/*
import /std/http/server/*
import /std/text/*
import /std/text/view/*

[struct]
Counter() {
  [i64 mut] hits{0i64}
}

[effects(io_err)]
log_serve_error([ServeError] err) {
  print_line_error(err.why())
}

[effects(heap_alloc)]
count_hit([Reference<Counter>] counter, [Request] request, [Reference<Response>] response) {
  counter.hits = counter.hits + 1i64
  response.body.appendI64(counter.hits)
}

[return<int> effects(heap_alloc, io_err, net_io, task, lock, channel, atomic) on_error<ServeError, log_serve_error>]
main() {
  [AppRouter<Counter> mut] app{AppRouter<Counter>{[state] Counter{}}}
  app.post("/hit", count_hit)
  serveWorkers(withRequestLog(app), ServerConfig{[port] 8080i32, [workers] 4i32})?
  return(0i32)
}
```

### Static files (`/std/http/files`)

`import /std/http/files/*` answers a request from a directory, over `/std/os` `readFile` (link the `/std/os` shim).

- `safe_relative_path(path) -> bool`: false for an empty path, an absolute path (leading `/`), a NUL or backslash
  byte and any `..` segment. Percent escapes are not decoded.
- `content_type_for(path) -> StringView`: by extension (html, css, js, json, png, jpg, svg, txt), else
  `application/octet-stream`.
- `serve_file(response, root, relative, maxBytes)` (`file_read`, `heap_alloc`): 200 with the file and its
  `Content-Type`; 403 for an unsafe path; 404 for a missing file or a directory; 413 for a file over `maxBytes`. The
  handler's enclosing definition needs `file_read` too, because `Function` values carry no effects.

```prime
router.get("/static/*", []([Request] request, [Reference<Response>] response) {
  serve_file(response, "public", request.path.slice(8i32, request.path.count()), 1048576i32)
})
```

`examples/apps/web_app/main.prime` serves `$STATIC_DIR` this way.

### Shared state (`/std/sync`)

`import /std/sync/*` provides `Atomic<i32>` and `Atomic<i64>`, `Mutex<T>` and `Channel<T>` (below); `/std/http/server`
`serveWorkers` is built from all three. The atomics
are integer cells that several tasks read and change without losing an update (docs/MultithreadingPrototype.md, Atomic). An `Atomic` is a handle: a copy shares the cell,
so a task that is given one works on the same integer as its spawner, and the cell is freed when its last handle is
destroyed. The type is `[sendable]`.

| Method | Result |
| --- | --- |
| `load()` | the value |
| `store(value)` | nothing |
| `add(delta)` | the value before the addition; the sum wraps |
| `exchange(value)` | the value before the store |
| `compareExchange(expected, desired)` | `true` when the cell held `expected` and now holds `desired` |

Every operation is sequentially consistent and needs the `atomic` effect (`atomic operation requires atomic
effect`); making an `Atomic` needs `heap_alloc`. `Atomic<T>` of any other type is `Atomic requires i32 or i64`.

```prime
import /std/sync/*

[effects(atomic), return<void>]
count_hits([Atomic<i64>] hits, [i32] times) {
  [i32 mut] i{0i32}
  while(i < times) {
    hits.add(1i64)
    i = i + 1i32
  }
}

[effects(atomic, heap_alloc, io_out, task), return<int>]
main() {
  hits{Atomic<i64>{0i64}}
  [Task<void>] first{[spawn] count_hits(hits, 1000i32)}
  [Task<void>] second{[spawn] count_hits(hits, 1000i32)}
  wait(first)
  wait(second)
  print_line(hits.load())
  return(0i32)
}
```

This prints `2000` on the VM, under every schedule seed, and on `optexe`, where the two tasks are threads. The
operations are IR opcodes (docs/spec/vm-design.md, Atomics); native, Wasm and GLSL do not have them yet and reject
the program with `atomic operations are not supported by the native backend`.

`Mutex<T>` owns a value of a sendable type `T` in a heap cell and lets one task at a time reach it
(docs/MultithreadingPrototype.md, Mutex). `Mutex<Tally>{Tally{}}` makes one holding a copy of that value. It is a
handle like `Atomic`: a copy shares the value, so a spawned call that takes one works on the same value as its
spawner, and the last handle destroyed destroys the value. The type is `[sendable]`.

Its one method is `locked(body)`: it locks, calls the closure `body` with a `Reference<T>` to the value, unlocks
on every way out of `body`, and returns what `body` returns (nothing, for a body that returns nothing). A task that
calls `locked` while another holds the lock waits. `body` runs on the calling task, so it may borrow the caller's
locals with `[ref name]`. `locked` needs the `lock` effect (`mutex operation requires lock effect`); making a
`Mutex` needs `heap_alloc`.

```prime
import /std/sync/*

[struct]
Tally() {
  [i64 mut] total{0i64}
  [i32 mut] entries{0i32}
}

[effects(lock), return<void>]
record([Mutex<Tally>] tally, [i64] amount) {
  tally.locked([amount]([Reference<Tally>] state) {
    state.total = state.total + amount
    state.entries = state.entries + 1i32
  })
}

[effects(task, lock, heap_alloc, io_out), return<int>]
main() {
  tally{Mutex<Tally>{Tally{}}}
  [Task<void>] left{[spawn] record(tally, 3i64)}
  [Task<void>] right{[spawn] record(tally, 4i64)}
  wait(left)
  wait(right)
  [i64] total{tally.locked([]([Reference<Tally>] state) { return(state.total) })}
  print_line(total)
  return(0i32)
}
```

This prints `7` on the VM, under every schedule seed, and on `optexe`.

- A body that assigns to a scalar value declares its parameter `[Reference<i64> mut] value`; fields of a struct
  are assigned through a plain `[Reference<Tally>] state`.
- A body that captures by reference is bound to a local first and passed by name:
  `[mut] look{[ref seen]([Reference<Tally>] state) { seen = state.total }}` then `tally.locked(look)`
  (a `[ref]` closure written in the call is not accepted yet, TODO-5851).
- `T` must be sendable: `Mutex<StringView>` is `Mutex element StringView is not sendable`, and a struct names the
  field, ``Mutex element Record is not sendable: field `next` is Reference<Record>``.
- The reference stays in the body. A body that takes the value as a borrowed parameter and returns it gets
  `borrowed parameter escapes via return`; returning a `Reference<T>` parameter returns a copy of the value.
- Locking a mutex again inside its own body ends the program with `mutex locked again by the task that holds it`
  (exit code 3). Two tasks that lock two mutexes in opposite orders can wait for each other for ever: the VM
  ends the program with `deadlock: every task is waiting` (exit code 3).
- The lock and unlock are IR opcodes (docs/spec/vm-design.md, Mutexes); native, Wasm and GLSL reject the program
  with `mutex operations are not supported by the native backend`.

`Channel<T>` is a bounded queue of a sendable type `T` that tasks use to hand values to each other
(docs/MultithreadingPrototype.md, Channel). `Channel<i64>{[capacity] 4i32}` makes one that holds up to four items
(a capacity below 1 is 1). It is a handle like `Mutex`: a copy shares the queue, so a spawned call that takes one
sends to and receives from the same queue as its spawner, and the last handle destroyed destroys the items still
queued. The type is `[sendable]`.

| Method | Result |
| --- | --- |
| `send(value)` | Moves `value` into the queue. Waits while the queue is full. |
| `receive()` | A `Maybe<T>`: the oldest item, waiting while the queue is empty and open; `none` once the channel is closed and empty. |
| `close()` | Ends the channel. Receivers get what is queued and then `none`. Closing twice is allowed. |

The three need the `channel` effect (`channel operation requires channel effect`); making a `Channel` needs
`heap_alloc`. Items arrive in the order they were sent, and each item is received once, whichever task receives it.

```prime
import /std/sync/*

[effects(channel)]
produce([Channel<i64>] jobs, [i64] count) {
  [i64 mut] i{0i64}
  while(i < count) {
    jobs.send(i)
    i = i + 1i64
  }
  jobs.close()
}

[effects(channel), return<i64>]
sum_jobs([Channel<i64>] jobs) {
  [i64 mut] sum{0i64}
  [bool mut] open{true}
  while(open) {
    pick(jobs.receive()) {
      none {
        open = false
      }
      some(job) {
        sum = sum + job
      }
    }
  }
  return(sum)
}

[effects(task, channel, heap_alloc, io_out), return<int>]
main() {
  jobs{Channel<i64>{[capacity] 4i32}}
  [Task<void>] producer{[spawn] produce(jobs, 100i64)}
  [Task<i64>] consumer{[spawn] sum_jobs(jobs)}
  wait(producer)
  print_line(wait(consumer))
  return(0i32)
}
```

This prints `4950` on the VM, under every schedule seed, and on `optexe`.

- A send on a closed channel ends the program with `send on a closed channel` (exit code 3); so does a send that
  was waiting on a full channel when it was closed.
- `T` must be sendable: `Channel<Pointer<u8>>` is `Channel element Pointer<u8> is not sendable`. A `String`,
  a struct of sendable fields and another handle (`Atomic`, `Mutex`, `Channel`) are sendable.
- A `String` made while a `StringArena` is open cannot be sent, since the arena frees it when the scope ends:
  `string escapes its arena via channel (arena: arena)`.
- A received item that owns memory (a `String`) is freed where it is received: `pick(lines.receive())` destroys
  the payload when the `pick` ends, unless an arm moved it out (`[String] owned{move(line)}`).
- A task that receives from an open channel nobody sends on waits for ever: the VM and `optexe` end the program
  with `deadlock: every task is waiting` (exit code 3) once every task waits.
- The waits are IR opcodes (docs/spec/vm-design.md, Conditions); native, Wasm and GLSL reject the program with
  `channel operations are not supported by the native backend`.

### JSON writer (`/std/json`)

`import /std/json/*` provides `Writer`, which builds JSON text in its public `out` `String`. It puts the commas and
colons where they belong, nests objects and arrays, and escapes `"`, `\` and control bytes in strings (other bytes,
such as UTF-8, pass through). It does not parse JSON and does not check that calls are balanced.

- `beginObject()` / `endObject()`, `beginArray()` / `endArray()`
- `key(name)` before each member value of an object
- `stringValue(text)`, `i64Value(n)`, `boolValue(b)`, `nullValue()`; `text` is a `StringView`

```prime
import /std/json/*
import /std/text/*
import /std/text/view/*

[return<int> effects(heap_alloc)]
main() {
  [Writer mut] w{Writer{}}
  w.beginObject()
  w.key("ok")
  w.boolValue(true)
  w.key("ids")
  w.beginArray()
  w.i64Value(1i64)
  w.i64Value(2i64)
  w.endArray()
  w.endObject()
  // w.out is {"ok":true,"ids":[1,2]}
  return(w.out.count())
}
```

### HTML writer (`/std/html`)

`import /std/html/*` provides `Writer`, which builds an HTML page in its public `out` `String`, in the style of the JSON
writer. Escaping is the default: text and attribute values are escaped as they are written (`&`, `<`, `>` and `"` as
character references, and `'` too in attribute values; other bytes, such as UTF-8, pass through), and element and
attribute names keep only ASCII letters, digits, `-`, `_`, `:` and `.`, so a name cannot end a tag. `raw(markup)` is
the only call that writes its text unescaped; it is for markup the program wrote itself, never for text from a
request. The writer does not check that calls are balanced or that an element is allowed where it is written.

**Attributes follow the start tag they belong to.** A call that writes a start tag and nothing after it
(`beginElement`, `voidElement`, `input`, `beginForm`, `beginSelect`, `stylesheet`) leaves the tag open, then
`attribute(name, value)` and `booleanAttribute(name)` add to it, and the next call of any other kind writes the `>`.
An attribute call with no open tag is ignored. This reads like the markup it writes, one call per attribute and no
closing call, and it costs one `bool` of state. An attribute-list value would need a vector of pairs per element. An
explicit `endAttributes()` would be one more call at every element, and forgetting it would put content inside the tag.

- Elements: `beginElement(name)` / `endElement(name)`, `voidElement(name)` for an element without an end tag (`br`,
  `img`, `meta`), and `element(name, text)` for `<name>text</name>`
- Attributes: `attribute(name, value)`, `booleanAttribute(name)`
- Content: `text(text)` (escaped), `raw(markup)` (unescaped)
- Page: `beginPage(title)` writes `<!doctype html><html><head><meta charset="utf-8"><title>…</title>`;
  `stylesheet(href)` writes `<link rel="stylesheet" href="…">` in the head; `beginBody()` ends the head and starts the
  body; `endPage()` ends the body and the page
- Forms: `beginForm(method, action)` / `endForm()`; `label(forId, text)`; `input(type, name, value)`, left open for
  attributes such as `id` or `required`; `button(text)`, a `<button type="submit">`; `beginSelect(name)`,
  `option(value, text, selected)` and `endSelect()`; `textarea(name, text)`, which writes an extra newline after the
  start tag when the text starts with one, because a browser drops the first
- Scripts and fragments: `script(src)` writes `<script src="…" defer></script>`, a script that runs once the page is
  parsed; `fragmentTarget(id)` writes `data-ps-target="id"` on an open form tag, the region that the fragment script
  of `/std/web` replaces with the form's answer
- Every name, text and value parameter is a `StringView`, so a literal passes as it is and a `String` passes as
  `s.view()`

A page is a function that returns the writer's `out`, and a handler passes it to `Response.html`. A button is a
`<button>` in a `<form method="post">` whose route redirects back:

```prime
import /std/html/*
import /std/http/*
import /std/text/*
import /std/text/view/*

[return<String> effects(heap_alloc)]
counter_page([i32] clicks) {
  [Writer mut] page{Writer{}}
  page.beginPage("Counter")
  page.beginBody()
  [String] count{stringFromI32(clicks)}
  page.element("p", count.view())
  page.beginForm("post", "/add")
  page.button("Add one")
  page.endForm()
  page.endPage()
  return(page.out)
}

[return<int> effects(heap_alloc)]
main() {
  [Response mut] response{Response{}}
  response.html(counter_page(3i32))
  // The body is <!doctype html>...<p>3</p><form method="post" action="/add"><button type="submit">Add one</button>...
  [Response mut] added{Response{}}
  added.redirect("/")
  // added serializes as HTTP/1.1 303 See Other, Location: /, Content-Length: 0
  return(response.status)
}
```

`/std/html` and `/std/json` both name their writer `Writer`. Importing both is an `import creates name conflict: Writer`
error today, even where the program names one of them by its full path, so one file cannot use both writers yet
(TODO-5717).
