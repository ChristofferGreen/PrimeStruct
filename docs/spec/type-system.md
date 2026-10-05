# Type System v1

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **normative (draft)**.

## Type System v1 (draft)

### Goals
- Deterministic, explicit typing across backends.
- Minimal implicit behavior; no hidden conversions.
- Portable core type set with explicit backend extensions.
- Whole-program checks remain supported, but local reasoning is preferred.

### Type Grammar (canonical)
- **Atomic:** `bool`, `i32`, `i64`, `u64`, `f32`, `f64`, `string`, `void`, `Self`.
- **Composite:** `array<T>`, `vector<T>`, `map<K, V>`, `Pointer<T>`, `Reference<T>`,
  stdlib-owned `soa<T>`, stdlib-owned `tuple<Ts...>`, and draft math value
  types (`Mat2`, `Mat3`, `Mat4`, `Quat`).
- **User types:** struct definitions and named aliases, including
  stdlib-owned scene descriptor structs imported from `/std/scene/*`.
- **Template applications:** `Name<T1, T2, ...>`.

### Core Type Set (portable)
- `bool`, `i32`, `i64`, `u64`, `f32`, `f64`, `string`.
- `array<T>`, `vector<T>`, `map<K, V>` where parameters are core types.
- `soa<T>` for explicit structure-of-arrays storage of SoA-safe struct `T`.
  The public API is stdlib-owned on top of generic language/runtime SoA
  substrate rather than a compiler-owned collection contract. `soa_vector<T>`
  is a rejected compatibility spelling.
- `tuple<Ts...>` for heterogeneous product values implemented in stdlib
  `.prime` on top of generic type-pack storage and indexing substrate.
- Draft extension: `Mat2`, `Mat3`, `Mat4`, and `Quat` with explicit conversion-only interaction rules (not yet portable
  across backends).
- `Pointer<T>`, `Reference<T>` where `T` is primitive or a struct type.
- Capability-parameterized views are a core design direction, not yet a parsed
  surface: `Reference<T, Capability>` is the count-one view form and
  `Slice<T, Capability>` is the contiguous runtime-count view form. Both share
  semantic `View<T, Capability>` borrow, provenance, extent, and capability
  facts over valid `Pointer<T>` storage.
- User-defined structs with layout manifests.
- Types outside this set are backend-specific and must be rejected by backends that do not support them.

### Type Ownership Model (architectural direction)
The ownership matrix below is the canonical reference for which public type surfaces stay
language/runtime-owned, which remain hybrid, and which should move fully into stdlib
`.prime` implementations.

- `core`
  Public types/surfaces: fixed-width scalars, `string`, `array<T>`, `Pointer<T>`, `Reference<T>`,
  and the capability view model around `Reference<T, Capability>` / `Slice<T, Capability>`.
  Ownership rule: language/runtime owns both the public surface and the substrate because other
  features depend on them directly.
  Migration stance: treat these as stable substrate; delete workaround routing around them instead
  of trying to de-builtinize them.
- `hybrid`
  Public types/surfaces: `Result<T, Error>`, `File<Mode>`, `Buffer<T>`, `/std/gfx/*`.
  Ownership rule: keep only minimal builtin/runtime substrate for propagation, host I/O, and
  device interaction. Imported value-carrying `Result<T, Error>` construction now has a
  stdlib-owned sum surface under `/std/result/*`; `Result.ok(value)`,
  `Result.map(result, fn)`, `Result.and_then(result, fn)`, and
  `Result.map2(left, right, fn)` remain typed value-carrying sum
  compatibility paths on IR-backed VM/native. Those paths accept local
  imported Result sum sources and direct calls returning imported Result sums.
  Imported
  status-only `Result<Error>` construction, `pick`, `try(...)`, and
  `Result.error(...)` / `Result.why(...)` operands now live in stdlib on
  IR-backed VM/native for local, direct-call, and dereferenced local
  pointer/reference sources. `?` propagation still uses the hybrid bridge.
  Migration stance: move public constructors, helper APIs, and error-domain behavior into stdlib
  `.prime` wherever practical, then delete the old compatibility paths once the bridge is empty.
- `stdlib-owned`
  Public types/surfaces: `Maybe<T>`, `vector<T>`, `map<K, V>`, the public
  `Map<K, V>` wrapper struct, `soa<T>`,
  `tuple<Ts...>`, and the `/std/scene` descriptor surface (`Scene`, `Node`,
  `Transform`, `Camera`, `Material`, `Light`, and `Primitive`).
  Ownership rule: public API should live in stdlib `.prime` on top of minimal generic substrate.
  Migration stance: prefer slices that replace type-named compiler special cases with generic
  allocation/layout/drop substrate, then delete the old compatibility paths.

- `vector<T>` and `map<K, V>` therefore still appear in the portable type set today, but that
  should not be read as a permanent compiler-owned collection contract.
- Final map/vector compiler-knowledge invariant: release validation runs the
  broad zero audit over production C++ so map/vector semantics and
  implementation stay in stdlib `.prime` plus `.psmeta` metadata files, while
  C++ treats them as ordinary included stdlib code on generic substrate rather
  than as type-specific compiler branches.
- `soa<T>` is the promoted stdlib-owned public collection spelling over the
  current SoA backing identity. `soa_vector<T>` and direct experimental SoA
  imports are rejected compatibility spellings, not ordinary public API.
- `tuple<Ts...>` is a stdlib-owned heterogeneous product type. It is not a
  compiler-owned tuple envelope, fixed-arity family, opcode, or backend runtime
  object.
- `/std/scene` is a stdlib-owned data model for renderer-facing descriptors.
  The compiler owns no scene graph, camera, material, light, or primitive
  special case; renderer/runtime slices consume authored scene records through
  ordinary imported stdlib definitions.

### Stdlib Surface-Style Boundary
This boundary is the scope reference for the stdlib surface-style cleanup lane in
`docs/todo.md`. It decides which `stdlib/std` paths should read like
user-facing library code and which ones remain intentionally canonical,
bridge-oriented, or substrate-heavy while ownership migrations are still in
flight.

- **Style-aligned surface code:** `stdlib/std/math/*`, `stdlib/std/maybe/*`,
  `stdlib/std/file/*`, `stdlib/std/image/*`, `stdlib/std/ui/*`,
  `stdlib/std/scene/*`,
  `stdlib/std/collections/vector.prime`,
  `stdlib/std/collections/map.prime`,
  `stdlib/std/collections/errors.prime`,
  `stdlib/std/collections/soa.prime`,
  `stdlib/std/tuple/tuple.prime`, and
  `stdlib/std/gfx/gfx.prime`.
- **Internal implementation, bridge, or substrate code:** `stdlib/std/bench_non_math/*`,
  `stdlib/std/collections/collections.prime`,
  `stdlib/std/collections/experimental_vector.prime`,
  `stdlib/std/collections/experimental_map.prime`,
  `stdlib/std/collections/experimental_soa_vector.prime`,
  `stdlib/std/collections/experimental_soa_vector_conversions.prime`,
  `stdlib/std/collections/internal_*`, and
  `stdlib/std/gfx/experimental.prime`.
- **Mixed-directory rule:** `stdlib/std/collections` and `stdlib/std/gfx`
  remain mixed on purpose, so contributors should follow the file-level mapping
  above rather than assuming the whole directory has one style target.

Style-aligned modules should follow `docs/CodeExamples.md`. The internal implementation or
bridge-oriented files may stay helper-heavy until the relevant migration or
cleanup TODO explicitly retargets them.

### Stdlib Module Inclusion Manifest
Stdlib import expansion may use small checked module metadata when a slash
import root should append a deterministic source file instead of scanning a
directory. The first manifest lives at `stdlib/std/modules.psmeta`. Each
`[module]` entry declares a `root` such as `/std/gfx` and a relative
`source_file` such as `std/gfx/gfx.prime`; `source_file` must stay under the
stdlib root and must name a `.prime` file. The compile pipeline consumes this
field before falling back to ordinary directory/module-file discovery, so
direct imports and wildcard imports can share the same manifest-owned module
root without adding new C++ path heuristics.

### Vector/Map Bridge Contract
This contract is the scope reference for future vector/map ownership-cutover
TODOs. Follow-on bridge tasks should share this boundary instead of
re-defining it piecemeal.

- **Bridge-owned public contract:** exact and wildcard `/std/collections`
  imports for `vector`/`map`, constructor and literal-rewrite surfaces for
  `vector<T>` and `map<K, V>`, collection helper families, compatibility
  spellings plus removed-helper diagnostics, semantic surface IDs, and lowerer
  dispatch metadata.
- **Stdlib-owned surface metadata:** canonical vector and map
  helper/import/constructor metadata is derived from `[public]` stdlib
  declarations by `StdlibSurfaceRegistry` (TODO-4635 deleted
  `stdlib/std/collections/surfaces.psmeta`). Production C++
  keeps the surface ids and registry APIs, but the canonical collection member
  lists, import aliases, lowering spellings, and vector mutator helper subset
  live with the stdlib collection source. Old vector compatibility spellings are
  intentionally absent from that metadata; rooted helper aliases are removed,
  and direct experimental vector source imports are rejected by import
  validation. Old rooted map helper aliases, `mapCount`-style wrapper
  spellings, and experimental map helper spellings are also absent from that
  metadata; direct experimental map imports and public `mapCount`-style
  wrapper bridges are rejected or retired, and lowerer/emitter production C++
  no longer adapts internal `mapCount`-style helper spellings as canonical map
  operations.
- **Migration-only seams:** experimental map implementation modules remain
  temporary compatibility seams. Rooted `/vector/*` and `/map/*` helper
  spellings no longer act as builtin compatibility aliases for canonical
  `/std/collections/*` helpers; explicit user definitions under those paths
  remain ordinary definitions.
- **Rooted same-path shadows (vector and map, symmetric):** an explicit user
  definition at a rooted same-path helper is an intentional override for the
  matching bare call on that receiver family, not a retiring alias. Bare
  `count(v)`, `count_ref(v)`, and `capacity(v)` on a `vector<T>` receiver call
  a user `/vector/count`, `/vector/count_ref`, or `/vector/capacity`
  definition when one exists. Bare `count(m)` and `count_ref(m)` on a
  `map<K, V>` receiver call a user `/map/count` or `/map/count_ref`
  definition in the same way (TODO-4809). Template monomorphization rewrites
  the call onto the rooted path before the canonical
  `/std/collections/*` import preference is considered, so the rooted shadow
  wins even when the canonical helper is also imported. Method sugar
  (`values.count()`) is not covered by this rule and keeps its
  canonical-helper routing. Because the call is resolved to a real
  definition, `--collect-diagnostics` reports its argument-count and
  argument-type mismatches against the rooted path (for example `argument
  count mismatch for /map/count`) alongside other diagnostics in the same
  definition or execution, instead of treating it as a builtin `count`.
  Example:
  ```
  [return<i32>]
  /map/count([map<i32, i32>] values) {
    return(96i32)
  }
  [return<i32>]
  main() {
    [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
    return(count(values))   // calls /map/count, exits with 96
  }
  ```
- **Cutover status:** The vector/map adapter cutover is complete for semantic,
  template-monomorph, and lowerer helper path-candidate decisions; direct
  experimental vector source imports are rejected, map surface metadata is now
  stdlib-owned, and the surface manifest no longer advertises map compatibility
  spellings.
  Direct experimental map source imports are also rejected, public
  `mapCount`-style wrapper bridges are retired, semantic validation no longer
  recognizes `mapCount`-style helper names as special map branches, and release
  validation gates reject reintroduced semantic or lowerer/emitter
  `mapCount`-style adapter traces; TODO-4464 owns the remaining broad
  production C++ map-surface audit.
- **Compatibility adapter inventory:** map insert helper compatibility no
  longer lives in the central surface manifest; the `CollectionsMapHelpers`
  registry metadata now classifies only canonical `/std/collections/map/*`
  helpers. Definition/execution intra-body diagnostics no longer carry special
  removed-map helper classification branches, and semantic pre-dispatch helper
  path candidates no longer mirror rooted and canonical map helpers. Native tail
  map-access helper probes no longer count rooted `/map/*` imports or
  definitions as canonical map helper availability, and semantic helper-path
  preference no longer cross-resolves rooted `/map/*` and canonical
  `/std/collections/map/*` definitions, and semantic method resolution no
  longer treats explicit rooted `/map/*` method targets as canonical
  `/std/collections/map/*` helper calls, and inline/native dispatch no longer
  treats rooted `/map/*` or experimental map helper raw paths as canonical map
  helper aliases. The internal `mapCount`-style `.prime` implementation names
  now lower as ordinary helper definitions rather than lowerer/emitter adapter
  spellings. Template
  monomorphization still asks the registry for preferred experimental
  vector/SoA helper spellings instead of carrying bespoke
  canonical-to-experimental helper maps. SoA public helper, constructor,
  import-alias, field-view, and conversion metadata is derived from
  `[public]` stdlib declarations by `StdlibSurfaceRegistry` (TODO-4635
  replaced `stdlib/std/collections/surfaces.psmeta`). The
  `soa<T>` public surface is declared in stdlib without old `soa_vector`, rooted
  `/soa_vector`, mixed-helper, experimental wrapper, or conversion-helper
  compatibility spellings. The registry keeps surface ids and generic APIs in C++,
  but it no longer owns SoA public collection member lists, import aliases,
  helper aliases, constructor spellings, or conversion spellings as handwritten
  tables.
  Vector/map and SoA constructor compatibility is metadata-backed by the
  constructor surface adapters outside the removed map compatibility spellings.
  Gfx Buffer helper compatibility is routed through
  `StdlibSurfaceRegistry::GfxBufferHelpers` for canonical `/std/gfx/Buffer/*`,
  legacy `/std/gfx/experimental/Buffer/*`, and rooted `/Buffer/*` helper
  spellings before semantic gfx-buffer rewrites and GPU wrapper diagnostics
  choose canonical builtin targets. Remaining explicit removed-helper
  diagnostics, import spellings, wildcard expansion, user-defined helper
  precedence, field-view field-name lowering, gfx constructor sugar, and lowerer
  raw-path dispatch checks are syntax/provenance-owned or lowering-owned.
- **Internal implementation seams:** `/std/collections/internal_vector/*`
  owns the canonical vector backing adapter while preserving the current
  `/std/collections/experimental_vector/Vector` compatibility type identity.
  `/std/collections/experimental_vector/*` is rejected as a source import and
  remains only as a legacy forwarding shim behind the internal vector module.
  `/std/collections/internal_map/*` owns the canonical map backing adapter
  while preserving the current `/std/collections/experimental_map/Map`
  compatibility type identity. `/std/collections/experimental_map/*` is
  rejected as a source import and remains only as a legacy forwarding shim
  behind the internal map module.
- **Out of scope for this bridge lane:** `array<T>` core ownership, promoted
  `soa<T>` implementation details, and runtime storage/allocator
  redesign stay outside the vector/map bridge contract and require separate
  TODO lanes when they move.

### Stdlib De-Experimentalization Policy
This policy is the scope reference for the stdlib de-experimentalization lane
in `docs/todo.md`. Use it to classify every `experimental` stdlib namespace
before renaming, deleting, or promoting that surface.

- **Canonical public API:** a non-`experimental` namespace that docs, examples,
  and future compiler-path authority should treat as the long-term user-facing
  contract.
- **Temporary compatibility namespace:** an `experimental` namespace that
  remains importable only to preserve current behavior while the canonical
  namespace or maturity decision finishes. Every compatibility namespace must
  get an explicit exit or downgrade TODO before work starts to retire, promote,
  or reclassify it.
- **Internal substrate/helper namespace:** an `experimental` namespace that is
  implementation plumbing rather than public API. It may stay imported by
  wrappers, conformance harnesses, or bridge code, but it should not be
  presented as an ordinary user-facing contract.
- **Default rule:** no `experimental` namespace counts as canonical public API
  by default. If a surface still needs public incubation, the docs must say so
  explicitly, and future promotion or retirement work must be tied to a
  concrete TODO instead of relying on blanket `experimental` wording.

Current `stdlib/std` experimental and internal module classification:

| Namespace family | Current role | Current interpretation | Follow-up |
| --- | --- | --- | --- |
| `/std/collections/vector/*` | Internal substrate/helper namespace | Internal vector backing adapter used by canonical `/std/collections/vector/*`; it preserves the current compatibility `Vector<T>` type identity until the final vector surface audit. | TODO-4373 |
| `/std/collections/experimental_vector/*` | Rejected compatibility namespace | Direct source imports are rejected; the shim remains only as legacy forwarding storage identity behind `/std/collections/vector/*` until the final vector surface audit. | TODO-4373 |
| `/std/collections/experimental_map/*` | Rejected compatibility namespace | Direct source imports are rejected; the shim remains only as legacy forwarding storage identity behind `/std/collections/map/*` until the final map surface audit. | TODO-4464 |
| `/std/gfx/experimental/*` | Temporary compatibility namespace | Legacy compatibility shim over canonical `/std/gfx/*`; no longer part of the public gfx contract and retained only for targeted compatibility coverage while the residual seam remains importable. | none |
| `/std/collections/experimental_soa_vector/*` | Retired compatibility namespace | Retired and merged into `/std/collections/soa/*` (TODO-4633); direct source imports are rejected. Ordinary code must use `/std/collections/soa/*`. | none |
| `/std/collections/experimental_soa_vector_conversions/*` | Retired compatibility namespace | Retired and merged into `/std/collections/soa/*` (TODO-4633); direct source imports are rejected. Conversion helpers are now part of `/std/collections/soa/*`. | none |
| `/std/collections/buffer_checked/*` | Internal substrate/helper namespace | Explicitly internal checked buffer plumbing for container conformance and memory-wrapper flows, not a stable user-facing stdlib API. Renamed from `internal_buffer_checked` in TODO-4634. | none |
| `/std/collections/buffer_unchecked/*` | Internal substrate/helper namespace | Explicitly internal unchecked buffer plumbing for container conformance and memory-wrapper flows, not a stable user-facing stdlib API. Renamed from `internal_buffer_unchecked` in TODO-4634. | none |
| `/std/collections/soa_storage/*` | Internal substrate/helper namespace | Explicitly internal SoA storage/layout plumbing used by wrappers and lowering bridges, not a canonical surface contract. Renamed from `internal_soa_storage` in TODO-4633. | none |

The policy implication is immediate: vector/map/gfx/SoA work should prefer
canonical non-`experimental` namespaces in docs and compiler authority, while
substrate helpers should be treated as explicit implementation namespaces
rather than as candidate public APIs.

### SoA Public Collection Contract
This section is the scope reference for the promoted `soa<T>` public
surface. It remains separate from vector/map implementation details because SoA
uses a distinct structure-of-arrays substrate and field-view invalidation model.

- **Current status:** `soa<T>` is the promoted stdlib-owned public collection
  type spelling and normalizes onto the existing SoA backing identity.
  `soa_vector<T>` and direct experimental SoA imports are rejected
  compatibility spellings; ordinary public code should use the canonical SoA
  namespace below.
- **Current user-facing surface:** `/std/collections/soa/*` is the canonical
  public helper spelling. The canonical wrapper owns ordinary constructor,
  count/get/ref, reserve/push, field-view, and AoS conversion helper names.
  Old `soa_vector<T>`, `/std/collections/soa_vector/*`,
  `/std/collections/soa_vector_conversions/*`, rooted `/soa_vector/*`,
  `SoaVector<T>`, `soaVector*`, and direct experimental SoA imports are
  rejected compatibility spellings. One source-locked wildcard canonical
  parity program runs
  across C++ emitter, VM, and native for construction/read/ref/mutator,
  field-view, and conversion behavior without direct experimental SoA imports
  in the test source. The canonical ECS example lives at
  `examples/3.Surface/soa_ecs.prime`.
- **Stdlib-owned surface metadata:** canonical `soa` helper/import/constructor,
  field-view, and conversion metadata is derived from `[public]` stdlib
  declarations by `StdlibSurfaceRegistry`. Generic compiler/runtime SoA substrate
  metadata remains separate.
- **Rejected compatibility seams:** `/std/collections/soa_vector*` and
  `/std/collections/experimental_soa_vector*` direct imports reject with a
  stable diagnostic pointing users to `/std/collections/soa/*`. `soa_vector<T>`
  type spelling also rejects in favor of `soa<T>`.
- **Internal substrate:** Implementation helpers are consolidated into
  `/std/collections/soa/*` (merged from `internal_soa_vector`,
  `internal_soa_vector_conversions`, `experimental_soa_vector`, and
  `experimental_soa_vector_conversions` in TODO-4633) and
  `/std/collections/soa_storage/*` (renamed from `internal_soa_storage` in
  TODO-4633). None of these are public APIs. The inline-parameter and direct
  lowerer wrapper-dispatch bridges no longer use rooted `/soa_vector/*`,
  `/to_aos`, or `/to_aos_ref` spellings as hidden raw fallbacks.
- **Promoted contract:** public behavior is owned by canonical stdlib surfaces
  and the remaining compiler/runtime pieces are generic substrate rather than
  SoA-specific compatibility paths:
  - Construction, read/ref, mutator, and field-view helpers are spelled through
    `/std/collections/soa/*` with `soa<T>` public type spelling; method sugar
    lowers to those same canonical helpers and preserves deterministic
    user-helper shadowing.
  - AoS/SoA conversions are spelled through `/std/collections/soa/*`;
    conversion receivers do not require direct imports of experimental
    conversion modules or legacy `/std/collections/soa_vector*` modules in
    ordinary code.
  - Borrowed `ref(...)` values, field views, structural mutation, explicit
    conversions, owner destroy, and scope exit share one documented
    invalidation model, and invalid escapes fail with deterministic diagnostics.
  - C++ emitter, VM, and native coverage exercise the same canonical
    construction/read/ref/mutator, field-view, and conversion programs where
    each backend supports the feature; unsupported paths reject explicitly
    instead of falling back to hidden raw-builtin behavior.
  - `/std/collections/soa_storage/*` remains implementation-only;
    direct old SoA compatibility imports are rejected.

### Generic SoA Substrate Boundary
This boundary is the scope reference for keeping compiler/runtime-owned SoA
behavior separate from the public `soa<T>` collection surface. It exists so
future `soa` work can delete public collection special cases without deleting
the generic layout and storage primitives that the stdlib wrapper still needs.

- **Public collection surface:** user code should spell the collection as
  `soa<T>` and import `/std/collections/soa/*`. Public construction,
  count/get/ref, push/reserve, field-view, conversion helper names, import
  aliases, and compatibility spelling rejection belong in stdlib wrapper
  modules or focused diagnostics, not in compiler-owned policy.
- **Accessor naming rule:** the soa accessor family composes two axes:
  the base name says what is returned (`get` = value, `ref` =
  `Reference<T>`), and the `_ref` suffix says the receiver is borrowed
  (`Reference<SoaVector<T>>`). `ref_ref` is therefore `ref` on a borrowed
  receiver, the same `<accessor>_ref` form as `count_ref`/`get_ref`; the
  suffix is the language-wide borrowed-receiver convention (vector and
  map use it too), so the name is intentionally not special-cased.
- **No-import helper rule:** the public helpers (`count`/`count_ref`,
  `get`/`get_ref`, `ref`/`ref_ref`, `to_aos`/`to_aos_ref`, `push`,
  `reserve`) exist only as `/std/collections/soa/*` wrappers. When a call
  to one of them has a `soa<T>` (or `SoaVector<T>`) receiver and the
  wrapper is not visible (neither `/std/collections/soa/*` nor
  `/std/collections/*` is imported), semantic validation rejects it with
  `soa helper requires import /std/collections/soa/*: <helper>`. The rule
  is the same for read helpers and mutators and for every spelling: bare
  (`count(values)`), method (`values.push(...)`), slash-method
  (`values./soa/count()`), rooted (`/soa/count(values)`), and canonical
  (`/std/collections/soa/count(values)`). A user same-path shadow
  (`/soa/<helper>` or root `/<helper>`) still wins and is not rejected.
  Constructing `soa<T>()` without the import stays allowed. No helper call
  reaches IR lowering without its wrapper definition.
  ```
  import /std/collections/soa/*

  [struct reflect]
  Particle() {
    [i32] x{1i32}
  }

  [effects(heap_alloc), return<int>]
  main() {
    [soa<Particle> mut] values{soa<Particle>()}
    values.push(Particle(3i32))
    return(values.count())
  }
  ```
  Dropping the import line from this example makes both `values.push(...)`
  and `values.count()` fail semantic validation with the diagnostic above.
- **Rooted helper rule:** with the wrapper visible, a rooted direct call
  `/soa/<helper>(values, ...)` on a `soa<T>` receiver is the non-method
  twin of the slash-method form `values./soa/<helper>(...)` and routes the
  same way for every public helper (`count`, `get`, `ref`, `to_aos`,
  `push`, `reserve` and their `_ref` variants): to a visible user
  `/soa/<helper>` shadow when one exists, otherwise to the canonical
  `/std/collections/soa/<helper>` wrapper. A rooted call on a non-soa
  receiver (for example `vector<i32>`) still rejects with
  `unknown call target: /soa/<helper>`, and no diagnostic names the
  retired `/std/collections/soa_vector/*` family.
  ```
  import /std/collections/*
  import /std/collections/soa/*

  [struct reflect]
  Particle() {
    [i32] x{3i32}
  }

  [effects(heap_alloc), return<int>]
  main() {
    [soa<Particle> mut] values{soa<Particle>()}
    /soa/reserve(values, 4i32)
    /soa/push(values, Particle{})
    [int] counted{/soa/count(values)}
    [Particle] picked{/soa/get(values, 0i32)}
    [Particle] borrowed{/soa/ref(values, 0i32)}
    [int] unpacked{/soa/to_aos(values).count()}
    return(plus(plus(counted, unpacked), plus(picked.x, borrowed.x)))
  }
  ```
  This exits with 8 on vm and native.
- **Allowed compiler/runtime substrate:** field-layout/codegen/introspection,
  generated `SoaSchema*` metadata, `SoaColumn<T>` column storage,
  `SoaFieldView<T>` non-owning field views, checked-buffer allocation/growth,
  byte-addressable field-slot addressing, borrow-root provenance, and
  invalidation tracking may remain compiler/runtime-owned when they are generic
  over reflected storage rather than public collection helper names.
- **Internal `.prime` substrate fixtures:** `/std/collections/soa_storage/*`
  owns `SoaColumn<T>`, `SoaColumnsN<...>`, `SoaFieldView<T>`,
  `soaColumnFieldSlotUnsafe<Struct, Field>(...)`, and field-view read/ref/write
  helpers. Reflection runtime coverage imports that storage module to exercise
  generated `SoaSchema*` and `SoaSchemaStorage` helpers without using public
  `soa_vector` collection helpers.
- **Rejected compatibility names:** `soa_vector<T>`,
  `/std/collections/soa_vector/*`, rooted `/soa_vector/*`, `SoaVector<T>`,
  `soaVector*`, and direct experimental SoA imports are no longer public
  collection spellings. Focused rejection tests keep their diagnostics stable.
- **Current helper-lowering gap:** canonical `soa<T>` construction, helper
  calls, field-view borrow roots, and live structural-mutation invalidation
  are covered now. Remaining SoA public-surface work should move helper
  behavior through ordinary `.prime` bodies before compatibility cleanup.

### Backend Profiles
- A definition is well-typed only with respect to a backend profile.
- Profiles include: `vm_native`, `glsl`, `cpp`.
- Each profile declares supported types, effects, and constructs.
- A definition may declare `[profile(vm_native, cpp)]`. If omitted, the compiler target profile is used.

### Typing Context
- Type checking runs after import expansion and namespace resolution.
- The typing context maps local bindings, parameters, signatures, struct layouts, and import aliases to resolved types.

### Expression Rules
- **Literals:** fixed type determined by suffix (`1i32`, `2u64`, `1.0f32`, `"hi"utf8`, `true`).
- **Bindings:** declared type in the transform list, or `auto` if omitted.
- **Calls:** well-typed if the callee resolves to a definition or builtin, and every argument matches the parameter
  type.
- **Constructors:** value construction uses brace forms (`Type{...}` and context-typed `{...}`), never call syntax.
  Struct constructors map positional/labeled entries to fields; all fields must be provided or defaulted.
- **`convert<T>` and `T{...}`:** explicit conversions; `T` must be a supported conversion target.
- **`assign(target, value)`:** allowed only when `target` is mutable and `value` has the exact same type.
- **Matrix/quaternion operators (draft):** matrix and quaternion arithmetic is explicit and shape-checked; no implicit
  widening or family conversion is allowed.
- **Control flow:** `if`, `while`, and `for` conditions must be `bool`. `loop(count)` requires an integer type.
- **Pointers/references:** targets are restricted as described in pointer rules; no implicit conversions.

### Definition Rules
- Parameters use binding syntax and must resolve to concrete types before lowering (per instantiation).
- Return types are explicit (`return<T>`) or inferred from `return(value)` sites when `return<auto>` is used.
- All `return(value)` statements in a definition must agree on the same type.
- `return()` is only valid for `void`.

### Inference Rules
- `auto` is allowed on bindings, parameters, and return transforms.
- Signature `auto` introduces implicit template parameters; template arguments are inferred per call site from
  parameter and return constraints.
- Binding `auto` is inferred from the initializer within the definition.
- Unresolved or conflicting `auto` is a diagnostic.

### Traits and Constraints
- Traits are explicit requirements over named functions (not operators).
- Trait constraints are explicit in signatures; no inference of trait bounds.
- A type satisfies a trait if all required functions resolve in the current program and backend profile.
- Minimal built-in traits:
  - `Additive<T>` requires `plus(T, T) -> T`.
  - `Multiplicative<T>` requires `multiply(T, T) -> T`.
  - `Comparable<T>` requires `equal(T, T) -> bool` and `less_than(T, T) -> bool`.
  - `Indexable<T, Elem>` requires `count(T) -> i32` and `at(T, i32) -> Elem`.
Example (trait constraints are transforms on definitions):
```
[return<i32> Additive<i32> Comparable<i32>]
sum([i32] left, [i32] right) {
  return(plus(left, right))
}
```

Existing transform-style trait constraints such as `[Additive<i32>]` remain
source-compatible compatibility vocabulary. New procedural generic code should
prefer the requirement predicate vocabulary, for example
`[require<meta.has_trait<typeof<value>>(Additive)>]`, so trait checks compose
with type equality, construction, lifecycle, field/member, and compile-time
value predicates in one `require<...>` transform.

### Parametric Types
- Type parameters are explicit in signatures or implicit via `auto` in signatures.
- Type arguments must be fully concrete before lowering.
- Template argument inference is limited to local call sites.

### Algebraic Data Types (v1.1 target)
- `enum` introduces tagged union types.
- Enum syntax follows the standard envelope rules; the `enum` transform rewrites the declaration to an ordinary struct
  plus static bindings so the base language never needs a dedicated enum form.
- Enums use an integer backing type. The default is `i32`; `enum<i64>` and `enum<u64>` select other widths.
- Entries may specify explicit integer literal values (via `Name = 5i32` or `assign(Name, 5i32)`), and omitted values
  auto-increment from `0` or the previous entry. Values must fit in the backing type; unsigned enums reject negatives.
- Pattern matching must be exhaustive and type-checked.
- `match` expressions have a single result type.

Enum example (surface):
```
[enum]
Colors {
  Blue = 5
  Red
  Green
}
```

Enum example (desugared shape):
```
[struct]
Colors() {
  [i32] value{0i32}

  [public static Colors] Blue{Colors{[value] 5i32}}
  [public static Colors] Red{Colors{[value] 6i32}}
  [public static Colors] Green{Colors{[value] 7i32}}
}
```

Enum entry access uses static field syntax (`Colors.Blue`) and rewrites to brace construction (`Colors{[value] 5i32}`).

### Ownership and Mutability
- `mut` marks writeable bindings.
- `move(x)` consumes a binding and forbids use until reinitialized.
- References cannot be moved.
- **Borrow rules (safe scope):**
  - A `Reference<T>` is a borrow of a storage location.
  - Many immutable borrows are allowed, or one mutable borrow; they may not overlap.
  - Borrows can end at last use before lexical scope exit (non-lexical lifetimes), including alias tracking through
    `location(ref)`.
  - Last-use analysis is conservative across complex control flow: if later use cannot be proven absent, the borrow
    stays active.
  - Borrowing a field borrows the whole struct value (no field-splitting in v1).
  - Borrowed bindings cannot be reassigned or moved until all borrows end.
  - `return<Reference<T>>` accepts either a direct `Reference<T>` parameter (`return(paramRef)`) or a direct
    parameter-rooted borrowed carrier such as `return(borrow(dereference(slot)))` when the borrowed storage is
    rooted in parameter-owned `uninitialized<T>` storage; local and non-parameter-rooted reference escapes are still
    rejected.
- **Unsafe scopes:** `[unsafe]` on a definition allows aliasing within that body, and also allows pointer-to-reference
  initialization from pointer-like expressions when types match. References created there must not escape the unsafe
  scope. Unsafe scopes are aliasing barriers for optimization.
- **Unsafe calls:** `[unsafe]` definitions may be called from safe code; the call does not taint the caller as long as
  unsafe-created references do not escape. `[unsafe_api]` definitions (unsafe to call, like the raw stdlib buffer
  wrappers) may only be called from `[unsafe]` or `[unsafe_api]` definitions.
- **Memory safety (partly implemented; open work TODO-5489, TODO-5490, TODO-5492):** safe code cannot read or write memory that has been freed. The
  compiler enforces it with these rules; code that breaks the guarantee from inside an `[unsafe]` definition (or a safe
  API built on one incorrectly) is the author's responsibility.
  - Freeing and reinterpreting memory is unsafe: `/std/intrinsics/memory/free`, `realloc`, `at_unsafe` and
    `reinterpret` are only accepted in `[unsafe]`/`[unsafe_api]` definitions, and wrappers around them are
    `[unsafe_api]`. `alloc` stays safe (without `free` it can only leak).
  - Containers own their storage. A borrow of an element or of storage inside a container (a reference returned by a
    container helper, a view, an iterator) borrows the container: the container cannot be structurally changed
    (grown, shrunk, cleared, moved or destroyed) while the borrow is live.
  - Parameters borrow their arguments by default (see Parameter Passing in `value-lifecycle.md`); a borrowed
    parameter cannot escape the call.
  - A pointer or reference to a local (`location(x)`) cannot escape the local's scope: returning it, storing it in a
    longer-lived place, or passing it to a `move`/`copy` parameter that keeps it is rejected.
  - Copying a container copies its elements; two containers never share storage in safe code.
  - Use-after-move is tracked per control-flow path: a binding moved on any path reaching a use is a compile error
    (implemented for `if` branches, where a branch ending in `return` does not reach the code after it, and for loops,
    where moving a binding from outside the loop without reassigning it in the body is an error).

### Layout and Struct Semantics
- Structs record layout manifests in IR.
- Recursive-by-value struct layouts are rejected.
- `[pod]` asserts trivially-copyable semantics and is validated.

### Diagnostics
- Diagnostics include expected type, actual type, source span, and backend profile.
- Diagnostic messages must be stable for snapshot testing.

### Conformance Tests (required)
- **Positive typing:** literals, calls, struct construction, pointers/references.
- **Negative typing:** return mismatch, invalid assign, invalid convert, invalid pointer target.
- **Inference:** binding inference, implicit-template inference, unresolved `auto`.
- **Traits:** satisfied, missing requirement, incorrect signature.

### Type Semantics (draft)
- **Nested generics:** template arguments may themselves be generic envelopes (`map<i32, array<i32>>`), and the parser
  preserves the nested envelope string for later lowering.
- **Heterogeneous type-pack storage:** a struct with a final type-pack
  parameter can expand that pack in stored-field position with `[Ts...] values`.
  Each specialization replaces the expansion with deterministic internal
  fields named `__pack_values_0`, `__pack_values_1`, etc., in pack-argument
  order. Zero-length packs produce no stored fields. These generated fields
  participate in brace construction, layout, and semantic-product field
  metadata as ordinary fields. Helper parameters and helper-local bindings may
  use the same `[Ts...] name` expansion, while return/type envelopes and call
  template arguments may spell `Ts...` where specialization expands it into
  zero or more concrete template arguments. Generated reflection helpers such
  as `Clone` and `CopyFrom` consume the same expanded source-order field list,
  so helper field order matches reflection metadata order. Generic `Ts[I]`
  pack indexing and `pack_at<I, fieldStem>(receiver)` access are available to
  `.prime` helpers after monomorphization.
- **Field visibility:** stack-value declarations accept `[public]` or `[private]` transforms (default: public); they are
  mutually exclusive. The compiler records `visibility` metadata per field so tooling and backends enforce access rules
  consistently. Field visibility is in-language field-access metadata; it does not make a field a package-importable API
  symbol.
- **Static members:** add `[static]` to hoist storage to namespace scope while reusing the field’s visibility transform.
  Static fields still participate in the struct manifest so documentation and reflection stay aligned, but only one
  storage slot exists per struct definition.
- **Example:**
  ```
  import /std/math/*
  namespace demo {
    [struct]
    brush_settings() {
      [f32] size{12.0f32}
      [private f32] jitter{0.1f32}
      [private static handle<Texture>] palette{load_default_palette()}

      [public]
      Create() {
        assign(this.size, clamp(this.size, 1.0f32, 64.0f32))
      }
    }
  }
  ```
- **Constructor semantics:** struct constructors use field initializers as defaults; `Create`/`Destroy` remain optional
  hooks. Constant member behavior follows the normal `mut` rules (immutable unless declared `mut`).
  - **Zero-arg constructor exists when:** either (a) every field has an initializer, or (b) a `Create()` helper exists
    and initializes every field (including `uninitialized<T>` fields via `init`).
  - **Execution order:** field initializers run first, then `Create()` runs (if present) and may override field values.
  - **Effect interaction:** a zero-arg constructor is outside-effect-free only when both `Create()` (if present) and all
    field initializers are effect-free under the “no outside effects” rules.
