# Pointers and References

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **normative**.

## Pointers & References
- **Explicit envelopes:** `Pointer<T>`, `Reference<T>` mirror C++ semantics; no implicit conversions.
- **Safe pointer optionality:** in safe code, `Pointer<T>` is a valid non-null storage identity, not an ordinary
  nullable address. There is no safe `null` pointer inhabitant; `0x0` remains just a numeric value. A safe API that
  might not produce a valid pointer must expose that possibility as `Maybe<Pointer<T>>` or
  `Result<Pointer<T>, ErrorT>` before callers can dereference, borrow, or construct views from it. `Reference<T>`
  inherits this invariant because it is initialized from a valid storage location or pointer. Raw or foreign nullable
  addresses remain unsafe adapter material, such as `RawPointer<T>` plus an FFI validation wrapper; converting one into
  `Pointer<T>` or `Reference<T>` requires boundary validation that returns `Maybe` or `Result` on failure.
- **Capability-parameterized views:** the normative view model is semantic
  `View<T, Capability>` over valid `Pointer<T>` storage plus extent,
  provenance, and capability facts. Canonical rule:
  `Reference<T, Capability>` is the non-null single-element view with
  `count == 1`; `Slice<T, Capability>` is the contiguous multi-element view
  with a runtime `count`; both share semantic `View<T, Capability>` borrow,
  provenance, extent, and capability facts over valid `Pointer<T>` storage.
  `Reference<T, Capability>` is not nullable and does not carry an absence
  state; optional production still uses `Maybe<Pointer<T>>`,
  `Maybe<Reference<T, Capability>>`, or an appropriate `Result` wrapper.
  `Slice<T, Capability>` carries a runtime element count and borrows
  `values[start, end)` or another contiguous proven range from its source
  storage. Both forms share provenance, lifetime/escape, capability authority,
  and alias/exclusivity rules; their capability parameter is compile-time
  authority metadata unless a future capability explicitly needs runtime state.
  Standard capability names such as read/write authority remain design
  vocabulary until the corresponding parser and semantic leaves land. Current
  implementation boundary: parser and lowering support the existing
  `Reference<T>`, array, and pointer surfaces only; do not rely on
  `Reference<T, Capability>` or `Slice<T, Capability>` source syntax before a
  later implementation leaf adds it.
- **Qualifiers:** `restrict<T>` is allowed on bindings and parameters only; it must match the binding type (including
  template args) and acts as an explicit type constraint. There is no `readonly` qualifier yet; use `mut` to opt into
  mutation.
- **Target whitelist:** `Pointer<T>` targets always support primitive and struct pointees. Additional specialized
  pointer targets are accepted only where the language/runtime has explicit handling (for example the supported
  `array<T>`, `vector<T>`, header-only `soa<T>`, `map<K, V>`, `File<Mode>`, `Result<T, Error>`, and `FileError`
  surfaces described elsewhere in this spec). `Reference<T>` targets always support primitive and struct pointees, and
  selected explicit wrapper cases such as `array<T>`, `vector<T>`, header-only `soa<T>`, `map<K, V>`,
  `File<Mode>`, `Result<T, Error>`, and `FileError` where the runtime has dedicated handling. Heap intrinsics still
  reject `alloc<array<T>>` even though location-backed `Pointer<array<T>>` bindings are accepted.
- **Backend limits:** VM/native lowering rejects `Pointer<string>` / `Reference<string>` (string pointers are not
  supported). Entry-argument arrays are not addressable (`location(args)` is invalid).
- **Surface syntax:** canonical syntax uses explicit calls (`location`, `dereference`, `plus`/`minus`); the `operators`
  text transform rewrites `&name`/`*name` sugar into those calls.
- **Reference binding:** in safe scopes, `Reference<T>` bindings are initialized from `location(...)`. In `[unsafe]`
  scopes they may also be initialized from pointer-like expressions (`Pointer<T>`/`Reference<T>` values and pointer
  arithmetic results) when the target type matches. References behave like `*Pointer<T>` in use. Use `mut` on the
  reference binding to allow `assign(ref, value)`.
- **Array pointers:** `Pointer<array<T>>` is allowed for location-backed locals/parameters and participates in the same
  read-only array receiver API (`count`/`at`/indexing) that value and reference arrays use.
- **Array references:** `Reference<array<T>>` is allowed; treat the reference like an array value for `count`/`at` and
  other array operations while still using `location(...)` to form the reference.
- **Core pointer calls:** `location(value)` yields a pointer to a local binding or parameter (entry argument arrays are
  not addressable); `location(ref)` returns the pointer stored by a `Reference<T>` binding; `dereference(ptr)` reads
  through a pointer/reference form; `assign(dereference(ptr), value)` writes through the pointer. Pointer writes require
  the pointer binding to be declared `mut`; attempting to assign through an immutable pointer or reference is rejected.
- **Heap intrinsics (semantic contract):** `/std/intrinsics/memory/alloc<T>(count)` returns `Pointer<T>`,
  `/std/intrinsics/memory/realloc(ptr, count)` returns the same `Pointer<T>` target type as `ptr`, and
  `/std/intrinsics/memory/free(ptr)` releases a pointer allocation. These names are recognized only in qualified
  `/std/intrinsics/memory/*` form. All three require `effects(heap_alloc)` (or the active default effects set), reject
  named/block arguments, and use element counts rather than raw byte sizes. `alloc` requires exactly one template
  argument plus one integer count argument; `realloc` requires one pointer argument plus one integer count argument;
  `free` requires one pointer argument and takes no template arguments. Current implementation status: semantic
  validation recognizes and validates all three calls; VM/native/IR-to-C++ lowering now lowers `alloc` onto the shared
  `HeapAlloc` runtime path (including struct element-count expansion by slot width), `free` now lowers onto `HeapFree`,
  `realloc` now lowers onto `HeapRealloc` on VM/native/IR-to-C++ backends, VM/IR-to-C++ runtimes reject dereferences
  into freed heap ranges deterministically, and `realloc` preserves slot payloads across successful growth/shrink
  reallocation while treating counts as element counts rather than raw bytes.
  Current implementation boundary: the built-in heap intrinsics still return bare `Pointer<T>` values in the
  compiler/runtime. The safe API direction for allocation that can fail is a stdlib wrapper returning
  `Maybe<Pointer<T>>` or `Result<Pointer<T>, AllocError>` rather than treating null as a valid `Pointer<T>`.
- **Checked pointer element access:** `/std/intrinsics/memory/at(ptr, index, count)` returns the same `Pointer<T>`
  target type as `ptr` after checked element-wise pointer arithmetic. It is recognized only in qualified
  `/std/intrinsics/memory/*` form, takes no template arguments, rejects named/block arguments, requires `ptr` to be a
  `Pointer<T>`, and currently requires `index` and `count` to use the same integer kind (`i32`, `i64`, or `u64`). Signed
  negative `index`/`count` values and `index >= count` fail deterministically with `pointer index out of bounds`.
  Lowering scales the element offset by the pointee slot width, so struct pointers step by full struct slot counts
  rather than raw scalar slots.
- **Unchecked pointer element access:** `/std/intrinsics/memory/at_unsafe(ptr, index)` returns the same `Pointer<T>`
  target type as `ptr` after unchecked element-wise pointer arithmetic. It is recognized only in qualified
  `/std/intrinsics/memory/*` form, takes no template arguments, rejects named/block arguments, requires `ptr` to be a
  `Pointer<T>`, and requires `index` to be an integer (`i32`, `i64`, or `u64`). Lowering scales the element offset by
  the pointee slot width just like checked `at(...)`, but performs no bounds check; this is the intended primitive for
  relocation/growth code paths in future stdlib-owned containers.
- **Internal pointer-helper shims:** `/std/collections/buffer_checked/*` and
  `/std/collections/buffer_unchecked/*` are explicit internal `.prime` helper namespaces used for
  container-conformance work. They wrap the qualified memory intrinsics into small alloc/grow/free plus
  checked/unchecked offset, read, and write helpers so future stdlib `vector`/`map` implementations can be proven
  through import-driven VM/native/C++ tests without presenting those helpers as candidate public collection APIs.
  VM/native conformance now also covers a non-vector `Pointer<uninitialized<T>>`
  fixture that allocates raw slots, initializes values with `init(...)`, moves a
  dynamic prefix between two buffers with `take(...)` plus `init(...)`, borrows
  moved storage with `borrow(dereference(slot))`, drops initialized survivors,
  frees both buffers, and checks that checked-offset failures still report
  `pointer index out of bounds` rather than vector-specific diagnostics.
  - When any `/std...` import is present, the stdlib now also provides canonical `.prime` wrappers at
    `/std/collections/vector/*` over the internal vector backing implementation.
    That imported path is now the sole public namespaced vector contract; the
    experimental vector namespace remains a backing seam rather than a peer
    public API. Canonical
    `/std/collections/vector/vector(...)`, `/std/collections/vector/count(...)`,
    `/std/collections/vector/capacity(...)`, `/std/collections/vector/at(...)`,
    `/std/collections/vector/at_unsafe(...)`, `/std/collections/vector/push(...)`, `/std/collections/vector/pop(...)`,
    `/std/collections/vector/reserve(...)`, `/std/collections/vector/clear(...)`,
    `/std/collections/vector/remove_at(...)`, and `/std/collections/vector/remove_swap(...)` calls now require either
    that imported stdlib path or an explicit source definition, and explicit canonical method forms such as
    `values./std/collections/vector/push(...)`, `values./std/collections/vector/pop()`,
    `values./std/collections/vector/reserve(...)`, `values./std/collections/vector/clear()`,
    `values./std/collections/vector/remove_at(...)`, and `values./std/collections/vector/remove_swap(...)` now reject
    with same-path `unknown method` diagnostics when no imported or declared canonical helper exists. The same
    explicit-helper rule now also covers bare builtin-vector `count()` method sugar: local `vector<T>` bindings such as
    `values.count()` reject with same-path `unknown method: /vector/count` unless an imported canonical helper or
    explicit `/vector/count` definition exists. Local non-vector explicit canonical slash-method `count()` and
    `capacity()` also now stay on that same-path rule: bindings such as `mapValues./std/collections/vector/count()`,
    `items./std/collections/vector/capacity()`, and `text./std/collections/vector/capacity()` reject with same-path
    `unknown method: /std/collections/vector/count` or `/std/collections/vector/capacity` unless a same-path helper
    exists. Wrapper-returned non-vector collection receivers now follow that same explicit-helper rule on both direct
    and slash-method `count`/`capacity` edges: `wrapMap()./std/collections/vector/capacity()`,
    `wrapMap()./vector/capacity()`, and `wrapMap()./vector/count()` reject with same-path `unknown method` diagnostics
    unless a same-path helper is declared, while direct canonical `/std/collections/vector/count(wrapMap())`,
    `/std/collections/vector/count(wrapText())`, and `/std/collections/vector/count(wrapArray())` calls also reject with
    same-path `unknown call target` diagnostics unless a same-path helper exists. Those canonical namespaced helper
    declarations now use experimental `Vector<T>` receivers directly where
    needed, but the old imported `/std/collections/vectorCount` /
    `vectorCapacity` / `vectorPush`-style wrappers and fixed-arity constructor
    aliases no longer form a public collection import surface. Canonical
    constructor/count/capacity/access wrappers plus statement-position mutators
    still follow ordinary definition argument rules such as named-argument
    support.
  - When any `/std...` import is present, the stdlib also provides canonical `.prime` wrappers at
    `/std/collections/map/*` over the current internal map helper substrate. That imported
    path is now the sole public namespaced map contract; the internal map module remains the backing seam while the
    experimental map namespace is a compatibility shim rather than a peer public API. Imported
    `/std/collections/map/map(...)`, `/std/collections/map/count(...)`, `/std/collections/map/contains(...)`,
    `/std/collections/map/tryAt(...)`, `/std/collections/map/at(...)`, and `/std/collections/map/at_unsafe(...)`
    wrappers follow ordinary definition argument rules such as named-argument support. Canonical
    `/std/collections/map/map(...)` legacy constructor-shaped builder calls, `/std/collections/map/count(...)` count calls,
    `/std/collections/map/contains(...)` contains calls, `/std/collections/map/tryAt(...)` calls, and canonical
    `/std/collections/map/at(...)` plus `/std/collections/map/at_unsafe(...)` access calls now require those imported
    wrappers or an explicit source definition instead of falling back to builtin `/map/map`, `/map/count`,
    `/map/contains`, `/map/tryAt`, or `/map/at*` handling. Explicit removed compatibility `/map/count(...)`,
    `/map/contains(...)`, `/map/tryAt(...)`, `/map/at(...)`, and `/map/at_unsafe(...)` spellings also no longer inherit
    canonical `/std/collections/map/*` behavior and now require explicit `/map/count`, `/map/contains`, `/map/tryAt`,
    `/map/at`, or `/map/at_unsafe` definitions. Internal map substrate imports
    of `/std/collections/internal_map/*` extend canonical namespaced helper
    calls in three distinct ways: value `Map<K, V>` receivers can use call-form
    `/std/collections/map/count|contains|tryAt|at|at_unsafe`, which now lowers
    through the ordinary canonical `.prime` wrapper definitions over the
    internal map/vector substrate. Canonical `insert` and `insert_ref`
    wrappers on canonical map bindings now also stay on the internal `.prime`
    insert substrate instead of the stale flat-map builtin rewrite.
    Direct canonical helper receivers built from canonical `/std/collections/map/map(...)`
    constructor expressions now rewrite those inner constructors onto
    `/std/collections/internal_map/mapNew|mapSingle|mapDouble|mapPair|...`; direct
    method-call receivers built from those same constructor expressions now do the same before method lowering; explicit
    `[Map<K, V>]` bindings, explicit `return<Map<K, V>>` definitions, direct arguments flowing into explicit `[Map<K,
    V>]` parameters plus inferred `[auto]` parameters backed by experimental-map default initializers, helper-wrapped
    argument expressions flowing into those explicit or inferred parameter targets, those same explicit or inferred
    parameter targets reached through method-call sugar, explicit or inferred experimental `Map<K, V>` struct fields
    initialized through struct-constructor arguments, direct `assign(values, ...)` writes into explicit `[Map<K, V>
    mut]` locals or parameters plus explicit or inferred experimental `Map<K, V>` struct fields, both explicit-template
    plus implicit-template legacy constructor-shaped calls flowing through `[auto]` locals and `return<auto>` definitions,
    `return<auto>` definitions whose inferred result is an experimental `Map<K, V>`, block-bodied value returns inside
    those inferred-return definitions, helper-wrapped return-path expressions inside those explicit or inferred
    experimental-map definitions, `auto` bindings nested inside those returned blocks, and direct method-call receivers
    produced by those inferred experimental-map definitions can initialize from either canonical
    `/std/collections/map/map(...)`, which rewrites onto or infers
    `/std/collections/internal_map/mapNew|mapSingle|mapDouble|mapPair|...` plus the compatibility `Map<K, V>`
    type during template monomorphization; and borrowed `Reference<Map<K, V>>` receivers use the overload-free canonical
    spellings `/std/collections/map/count_ref|contains_ref|tryAt_ref|at_ref|at_unsafe_ref`. Bare builtin map method
    sugar now follows the same import-driven path:
    `values.count()`, `values.contains(...)`, and `values.tryAt(...)` require imported canonical
    `/std/collections/map/count`, `/std/collections/map/contains`, and `/std/collections/map/tryAt` wrappers or explicit
    `/map/count`, `/map/contains`, and `/map/tryAt` definitions, while `values.at(...)` plus `values.at_unsafe(...)`
    require imported canonical `/std/collections/map/at*` wrappers or explicit `/map/at*` definitions, instead of
    falling back to compiler-owned builtin method semantics. Bare root `count(values)`, `contains(values, key)`,
    `at(values, key)`, and `at_unsafe(values, key)` calls on builtin `map<K, V>` targets now follow that same helper
    path: `count(values)` requires imported canonical `/std/collections/map/count` or explicit `/count` or `/map/count`
    definitions, while `contains(values, key)` and `at*(values, key)` require imported canonical
    `/std/collections/map/contains` or `/std/collections/map/at*` helpers, or explicit `/contains` or `/map/*`
    definitions, instead of falling back to compiler-owned builtin call semantics.
- **Pointer/reference helper returns:** explicit helper return transforms such as `return<Pointer<T>>` and
  `return<Reference<T>>` lower through the address-like `i64` return path on VM/native/IR-backed backends (unless they
  wrap one of the collection envelopes handled separately), so imported `.prime` helpers can pass checked-access
  pointers between allocation, growth, and indexing helpers without backend-specific special cases.
- **Pointer arithmetic:** `plus(ptr, offset)` and `minus(ptr, offset)` treat `offset` as a byte offset. VM/native frames
  currently space locals in 16-byte slots, so adding or subtracting `16` advances one local slot. Offsets accept `i32`,
  `i64`, or `u64` in the front-end; non-integer offsets are rejected, and the native backend lowers all three widths.
  - Pointer + pointer is rejected; only pointer ± integer offsets are allowed.
  - Offsets are interpreted as unsigned byte counts at runtime; negative offsets require signed operands (e.g.,
    `-16i64`).
  - Example: `plus(location(second), -16i64)` steps back one 16-byte slot.
  - Example: `minus(location(second), 16u64)` also steps back one 16-byte slot.
  - You can use `location(ref)` inside pointer arithmetic when starting from a `Reference<T>` binding.
  - The C++ emitter performs raw pointer arithmetic on actual addresses; offsets are only well-defined within the same
    allocated object.
  - Minimal runnable example:
    ```
    [return<i32>]
    main() {
      [i32 mut] value{2i32}
      [Pointer<i32> mut] ptr{location(value)}
      assign(dereference(ptr), 4i32)
      return(value)
    }
    ```
  - Expected IR (shape only):
    ```
    PushI32 2
    StoreLocal value
    AddressOfLocal value
    StoreLocal ptr
    LoadLocal ptr
    PushI32 4
    StoreIndirect
    Pop
    LoadLocal value
    ReturnI32
    ```
- **Reference example:** `Reference<T>` behaves like a dereferenced pointer in value positions while still storing the
  address.
  - Minimal runnable example:
    ```
    [return<i32>]
    main() {
      [i32 mut] value{2i32}
      [Reference<i32> mut] ref{location(value)}
      assign(ref, 4i32)
      return(ref)
    }
    ```
  - You can recover a raw pointer from a reference via `location(ref)` when needed for pointer arithmetic or passing
    into APIs.
  - Expected IR (shape only):
    ```
    PushI32 2
    StoreLocal value
    AddressOfLocal value
    StoreLocal ref
    LoadLocal ref
    PushI32 4
    StoreIndirect
    Pop
    LoadLocal ref
    LoadIndirect
    ReturnI32
    ```
  - References can be used directly in arithmetic (e.g., `plus(ref, 2i32)`), and `location(ref)` yields the underlying
    pointer.
- **Ownership:** references are non-owning, frame-bound views. Pointer ownership tags (`raw`, `unique`, `shared`) are
  reserved for future allocator integrations.
- **Raw memory:** `memory::load/store` primitives expose byte-level access; opt-in to highlight unsafe operations.
- **Layout control:** attributes like `[packed]` guarantee interop-friendly layouts for C++/GLSL.
- **Open design:** pointer qualifier syntax, aliasing rules (restrict/readonly), and GPU backend constraints remain TBD.
