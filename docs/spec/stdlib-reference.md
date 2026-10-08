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

Signature sketches (not surface syntax):
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

### Native UI (`/std/ui/native`)

`import /std/ui/native/*` brings in the native-widget surface of the version-0 C ABI
([Native UI plan](../NativeUiPlan.md)). It runs on the VM only: the embedding runner binds the `ps_ui_*` host
functions (`primec/ui/NativeUiBindings.h`) to a backend (headless in tests, AppKit on macOS).

- `start_app(name) -> App`: starts the app on the calling thread; `app.started` is false when it cannot start.
- `App`: `window(title, width, height) -> Window`, `textView() -> TextView`, `menu(title) -> Menu`,
  `waitEvent() -> AppEvent`, `openPanel(title)` and `savePanel(title, suggestedName)` (empty text means cancelled; test the result with
  `panelChosen() -> bool`, since `count` of a host-returned text is not yet reliable),
  `alert(message, detail, buttons) -> i32` (button titles separated by `\n`, returns the pressed index), `quit()`.
- `Window`: `setTitle`, `setContent(textView)`, `setEdited(bool)`, `show()`, `close()`.
- `TextView`: `text()`, `setText(text)`, `setMonospace(bool)`, `isModified()`, `clearModified()`, and
  `loadFile(path)` / `saveFile(path)` (the backend reads or writes the UTF-8 file itself; `false` means see
  `app.lastError()`; a successful load leaves the view unmodified, a save does not clear the modified flag), and
  `clearStyles()` / `addStyle(startByte, endByte, rgb, flags)` (colour and bold/italic over UTF-8 byte ranges).
- `Menu`: `item(title, shortcut, commandId)`, `separator()`, the standard items `undo redo cut copy paste selectAll
  find quit about hide hideOthers minimize zoom bringAllToFront fullScreen help`, and `addToBar()`,
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
