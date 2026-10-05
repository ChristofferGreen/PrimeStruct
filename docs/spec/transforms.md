# Transforms and Surface Syntax

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **normative (draft)**.

### Transform phases (draft)
- **Two phases:** text transforms operate on raw tokens before AST construction; semantic transforms operate on the
  parsed AST node.
- **Explicit grouping:** use `text(...)` and `semantic(...)` inside the transform list to force phase placement.
  - Example:
    ```
    [text(operators, collections, implicit-utf8) semantic(return<i32>, effects(io_out))]
    main([i32] a, [i32] b) {
      return(a + b)
    }
    ```
- **Auto-deduce by name:** transforms listed without `text(...)` or `semantic(...)` are assigned to their declared phase
  automatically. If a name exists in both registries, the compiler emits an ambiguity error.
- **Ordering:** the compiler scans transforms left-to-right, appending each to its phase list while preserving relative
  order within the text and semantic phases.
- **Scope:** a text transform may rewrite any token inside the enclosing definition/execution envelope (transform list,
  templates, parameters, and body). Nested definitions/lambdas receive their own transform lists.
- **Self-expansion:** text transforms may append additional text transforms to the same node; appended transforms run
  after the current transform.
- **Applicability limits (v1):**
  - **Definitions/executions only:** `return<T>`, `effects(...)`, `capabilities(...)`, `text(...)`, `semantic(...)`,
    `single_type_to_return`.
  - **Executions only:** `spawn` is reserved for the first task surface and must prefix call syntax as
    `[spawn] f(...)`.
  - **Definitions only:** `compute`, `workgroup_size(x, y, z)`, `unsafe`, `ast`.
  - **Struct/tag only (definitions):** `struct`, `pod`, `handle`, `gpu_lane`, `align_bytes(n)`, `align_kbytes(n)`.
  - **Definitions/bindings:** access/visibility markers (`public`, `private`). `static` is valid on bindings and struct
    helpers (disables implicit `this` on helpers).
  - **Reserved/rejected in v1:** `stack`, `heap`, `buffer` placement transforms (diagnostic).
  - Any transform outside its allowed scope is a compile-time error with a diagnostic naming the enclosing path.
- **User-authored AST hooks (narrow executable v1):** declare metadata-only hooks with
  `[ast return<void>] hook_name() { ... }`, or executable definition rewrites with
  `[ast return<FunctionAst>] hook_name([FunctionAst] fn) { return(replace_body_with_return_i32(fn, 7i32)) }`.
  Imported hooks must also be `public`; private hooks remain visible only to local definitions in the same expanded
  source. A definition attaches the hook by spelling `[hook_name return<T>]` or `semantic(hook_name)` in its transform
  list. Resolution records the hook's full path on the transform metadata, rejects ambiguous imports, rejects private
  imported hooks, and rejects `text(hook_name)` because AST hooks are semantic-phase metadata. Executable v1 hooks are
  compile-time only: the semantic pipeline evaluates the single supported `FunctionAst` result helper through the
  syntax-owned `ct-eval ast-transform adapter`, rewrites the touched definition body before downstream
  validation/lowering, and removes hook definitions from the runtime program. The adapter maps only
  `replace_body_with_return_i32(fn, value)` to `/ct_eval/replace_body_with_return_i32` and emits deterministic
  diagnostics for unknown helper targets or contradictory helper inputs. The checked-in example module and consumer live
  under `examples/4.Transforms/`.

### Example function syntax
```
import /std/math/*
namespace demo {
  [return<void> effects(io_out)]
  hello_values() {
    [string] message{"Hello PrimeStruct"utf8}
    [i32] iterations{3i32}
    [f32 mut] exposure{1.25f32}
    [float3] tone_curve{float3(0.8f32, 0.9f32, 1.1f32)}

    print_line(message)
    assign(exposure, clamp(exposure, 0.0f32, 2.0f32))
    loop(iterations) {
      apply_curve(tone_curve, exposure)
    }
  }
}
```
Statements are separated by whitespace (including newlines). Commas and semicolons are ignored separators with no
meaning outside numeric literals (where commas may act as digit separators). PrimeStruct does not distinguish between
statements and envelopes—any envelope can stand alone as a statement, and unused values are discarded (for example,
`helper()` or `1i32` can appear as standalone statements).
When an expression is immediately followed by a binding transform list (`[...] name{...}`), the parser treats the
binding as a new statement rather than indexing sugar on the previous expression; use `at(value, index)` /
`value.at(index)`
or a semicolon if you intended to index.

### Slash paths & textual operator transforms
- Slash-prefixed identifiers (`/pkg/module/thing`) are valid anywhere the Envelope expects a name; `namespace foo { ...
  }` is shorthand for prepending `/foo` to enclosed names, and namespaces may be reopened freely.
- Text transforms run before the AST exists. Operator transforms scan the raw character stream and rewrite when they see
  a left operand and right operand, allowing optional whitespace around the operator token. Slash paths remain intact
  when `/` begins a path segment with no left operand (start of line or immediately after whitespace/delimiters). Binary
  operators respect standard precedence and associativity: `*`/`/` bind tighter than `+`/`-`, comparisons (`<`, `>`,
  `<=`, `>=`, `==`, `!=`) bind tighter than `&&`/`||`, and assignment (`=`) is lowest precedence and right-associative.
  Operators follow the same operand-based rule (`a > b` → `greater_than(a, b)`, `a < b` → `less_than(a, b)`, `a >= b` →
  `greater_equal(a, b)`, `a <= b` → `less_equal(a, b)`, `a == b` → `equal(a, b)`, `a != b` → `not_equal(a, b)`, `a && b`
  → `and(a, b)`, `a || b` → `or(a, b)`, `!a` → `not(a)`, `-a` → `negate(a)`, `a = b` → `assign(a, b)`, `++a` / `a++` →
  `increment(a)`, `--a` / `a--` → `decrement(a)`).
- Because imports expand first, slash paths survive every transform untouched until the AST builder consumes them, and
  IR lowering never needs to reason about infix syntax.

### Struct & envelope categories
- **Struct tag as transform:** any of `[struct]`, `[pod]`, `[handle]`, or `[gpu_lane]` marks the envelope as a
  struct-style definition. It records a layout manifest (field names, envelopes, offsets) and validates the body, but
  the underlying syntax remains a standard definition. Struct-tagged definitions are field-only: no parameters or return
  transforms, and no return statements. The body may include nested helper definitions; only stack-value bindings
  contribute to layout. Non-lifecycle helpers lower into the struct method namespace (`/<Struct>/name`) and can be
  called via method sugar or as plain namespace functions. Un-tagged definitions may still be instantiated as structs;
  they simply skip the extra validation/metadata until another transform demands it.
- **Placement policy:** where a value lives (stack/heap/buffer) is decided by allocation helpers plus capabilities, not
  by struct tags. Envelopes may express requirements (e.g., `pod`, `handle`, `gpu_lane`), but placement is a call-site
  decision gated by capabilities. The `stack`/`heap`/`buffer` transforms remain reserved and rejected in v1.
- **POD tag as validation:** `[pod]` on a struct-style definition asserts trivially-copyable semantics. Violations
  (hidden lifetimes, handles, async captures) raise diagnostics; without the tag the compiler treats the body
  permissively.
- **Member syntax:** every field is a stack-value binding (`[f32 mut] exposure{1.0f32}`, `[handle<PathNode>]
  target{get_default()}`). In inference/surface levels, fields may omit the envelope when the initializer resolves
  concretely (e.g., `center{Vec3{0.0f32, 0.0f32, 0.0f32}}`). Attributes (`[mut]`, `[align_bytes(16)]`,
  `[handle<PathNode>]`) decorate the execution, and transforms record the metadata for layout consumers.
- **Method calls & indexing:** `value.method(args...)` desugars to `/<envelope>/method(value, args...)` in the method
  namespace (no hidden object model), where `<envelope>` is the envelope name associated with `value`. For struct
  helpers with implicit `this`, the receiver becomes the hidden `this`
  parameter. Static helpers reject value-receiver method-call sugar
  (`value.helper()`), but accept type-qualified dot calls such as
  `Counter.defaultStep()` as a surface form for the same direct helper path.
  For collections, method syntax is the preferred surface style
  (`value.count()`, `value.at(i)`, `value.push(x)`), while helper calls remain
  canonical (`count(value)`, `at(value, i)`, `push(value, x)`). Vector helper
  surface syntax currently requires `import /std/collections/*` so the
  canonical stdlib wrappers are in scope. Indexing uses the safe helper by
  default: `value[index]` rewrites to `at(value, index)` with bounds checks and
  is equivalent to `value.at(index)`; `at_unsafe(value, index)` /
  `value.at_unsafe(index)` skips checks.
- **Struct body semantics (example):**
  ```
  [struct]
  BrushSettings {
    // Field bindings contribute to layout.
    [f32] size{12.0f32}
    [f32] hardness{0.75f32}

    // Helpers are ordinary definitions in /BrushSettings/* and do not affect layout.
    // Non-static helpers have an implicit `this` (Reference<Self>).
    [public return<f32>]
    clampSize([f32] value) {
      return(clamp(value, 1.0f32, 64.0f32))
    }

    // Method-call sugar supplies the implicit `this` (settings.normalize()).
    [public return<f32>]
    normalize() {
      return(this.clampSize(this.size))
    }

    // Static helper: no implicit `this`; call via BrushSettings.defaultSize().
    [public static return<f32>]
    defaultSize() {
      return(12.0f32)
    }

    // `mut` makes `this` writable inside the helper.
    [public mut return<void>]
    setSize([f32] value) {
      assign(this.size, this.clampSize(value))
    }
  }
  ```
- **Baseline layout rule:** members default to source-order packing. Backend-imposed padding is allowed only when the
  metadata (`layout.fields[].padding_kind`) records the reason; `[no_padding]` and `[platform_independent_padding]` fail
  the build if the backend cannot honor them bit-for-bit.
- **Alignment transforms:** `[align_bytes(n)]` (or `[align_kbytes(n)]`) may appear on the struct or field; violations
  again produce diagnostics instead of silent adjustments.
- **Stack value executions:** every local binding and struct field uses the binding form `name{initializer}` with
  optional envelope annotation (`[Type qualifiers…] name{initializer}`), so stack frames remain declarative (e.g., `[f32
  mut] exposure{1.0f32}`). Concrete level keeps the envelope explicit. In inference/surface levels, omitted envelopes
  are allowed only when inference resolves a single concrete envelope; struct fields must resolve before layout manifest
  emission. Default initializers are mandatory for fields; local bindings may omit the initializer only when the
  envelope is a struct type with a zero-argument constructor **and** the compiler can prove the construction has no
  outside effects. In the planned brace-only construction model, the binding desugars to `Type{}` (e.g.,
  `[BrushSettings] s` -> `[BrushSettings] s{BrushSettings{}}`).
  - **No outside effects (definition):** zero-arg construction is outside-effect-free only when all of the following
    hold:
    - The constructor path has an empty effects/capabilities mask (no `effects(...)`/`capabilities(...)`, and no callees
      with non-empty effects).
    - Writes are limited to the newly constructed value (`this`) and local temporaries; any writes through
      pointers/references or to non-local bindings are disallowed.
    - Field initializers are effect-free under the same rules, and all called helpers are effect-free transitively.
    - If the compiler cannot prove these constraints, omitted-initializer bindings are rejected.
  - Multi-step initializer example:
    ```
    [return<f32>]
    main() {
    [f32] x{
      [f32 mut] tmp{1.0f32}
      tmp = tmp + 2.0f32
      tmp
    }
      return(x)
    }
    ```
- **Lifecycle helpers (Create/Destroy):** Within a struct-tagged or field-only struct definition, nested definitions
  named `Create` and `Destroy` gain constructor/destructor semantics. Placement-specific variants add suffixes
  (`CreateStack`, `DestroyHeap`, etc.). Without these helpers the field initializer list defines the default
  constructor/destructor semantics. Struct helpers receive an implicit `this` unless marked `[static]`; add `mut` to the
  helper’s transform list when it writes to `this` (otherwise `this` stays immutable). Lifecycle helpers must return
  `void` and accept no parameters. We capitalise system-provided helper names so they stand out, but authors are free to
  use uppercase identifiers elsewhere—only the documented helper names receive special treatment.
  - **Helper visibility:** nested non-lifecycle helpers are normal definitions in the struct method namespace. Use
    `[public]` on the helper definition to export it; private helpers remain callable within the same compilation unit.
    Non-static helpers receive an implicit `this` (`Reference<Self>`); `[static]`
    disables the implicit `this` and value-receiver method-call sugar, but still
    allows type-qualified dot calls on the struct name.
  ```
  import /std/math/*
  namespace demo {
    [struct pod]
    color_grade() {
      [f32 mut] exposure{1.0f32}

      [mut]
      Create() {
        assign(this.exposure, clamp(this.exposure, 0.0f32, 2.0f32))
      }

      [effects(io_out)]
      Destroy() {
        print_line("color grade destroyed"utf8)
      }
    }
  }
  ```
- **IR layout manifest:** `[struct]` extends the IR descriptor with `layout.total_size_bytes`, `layout.alignment_bytes`,
  and ordered `layout.fields`. Each field record stores `{ name, envelope, offset_bytes, size_bytes, padding_kind,
  category }`. Placement transforms consume this manifest verbatim, ensuring C++, VM, and GPU backends share one source
  of truth.
- **Categories:** `[pod]`, `[handle]`, `[gpu_lane]` tags classify members for resource rules. Handles remain opaque
  tokens with subsystem-managed lifetimes; GPU lanes require staging transforms before CPU inspection.
- **Category mapping:**
  - `pod`: stored inline; treated as trivially-copyable for layout.
  - `handle`: stored as an opaque reference token; lifetime is managed by the owning subsystem.
  - `gpu_lane`: stored as a GPU-only handle; CPU access requires explicit staging transforms.
  - Un-tagged fields default to `default`.
  - Conflicts are rejected (`pod` with `handle` or `gpu_lane`, `handle` with `gpu_lane`, and `pod` definitions
    containing `handle`/`gpu_lane` fields).

### Transforms (draft)
- **Purpose:** transforms are metafunctions that rewrite tokens (text transforms) or stamp semantic flags on the AST
  (semantic transforms). Later passes (backend filters) consume the semantic flags; transforms do not emit code
  directly.
- **Evaluation mode:** when the compiler sees `[transform ...]`, it routes through the metafunction's declared
  signature—pure token rewrites operate on the raw stream, while semantic transforms receive the AST node and in-place
  metadata writers.
- **Registry note:** only registered text transforms can appear in `text(...)` groups or `--text-transforms`; only
  registered semantic transforms can appear in `semantic(...)` groups or `--semantic-transforms`. Other transform names
  are treated as semantic directives and validated by the semantics pass (they cannot be forced into `text(...)`).

**Text transforms (token-level, registered)**
- **`append_operators`:** injects `operators` into the leading transform list when missing, enabling text-transform
  self-expansion without repeating `operators` everywhere.
- **`operators`:** desugars infix/prefix operators, comparisons, boolean ops, assignment, and increment/decrement
  (`++`/`--`) into canonical calls (`plus`, `less_than`, `assign`, `increment`, etc.).
  - Example: `a = b` rewrites to `assign(a, b)`.
- **`collections`:** preserves `array<T>{...}` / `vector<T>{...}` / `soa<T>{...}` as brace construction and normalizes
  bracket aliases to braces. Map construction is ordinary stdlib helper resolution, so use
  `/std/collections/map/map<K,V>(...)` with `/std/collections/map/entry<K,V>(key, value)` helpers when the entry form is
  clearer; bare helper aliases are import-surface dependent.
  Legacy compatibility lowering may still route call-shaped collection helpers through stdlib adapters.
- **`implicit-utf8`:** appends `utf8` to bare string literals.
- **`implicit-i32`:** appends `i32` to bare integer literals (enabled by default).
  - Text transform arguments are limited to identifiers and literals (no nested envelopes or calls).

**Semantic transforms (AST-level, registered)**
- **`single_type_to_return`:** semantic transform that rewrites a single bare envelope in a transform list into
  `return<envelope>` (e.g., `[i32] main()` → `[return<i32>] main()`); enabled by default, but can be disabled or
  overridden via `--no-semantic-transforms`, `--semantic-transforms`, or `--transform-list`.

**Semantic directives (AST-level, validated)**
- **`copy`:** on a parameter, the callee receives its own copy of the argument instead of a borrow (through the type's
  `Copy` helper when it has one); on a binding, force a copy instead of a move. Often paired with `mut`. See Parameter
  Passing in `value-lifecycle.md`.
- **`move`:** on a parameter, the callee takes ownership of the argument, which is passed by reference and ends its
  lifetime in the caller (the caller's binding is moved-from after the call). Combines with `mut`.
- **`mut`:** mark the local binding as writable; without it the binding behaves like a `const` reference. On a
  parameter without `copy`/`move`, `mut` makes the parameter a mutable borrow: its writes reach the caller's argument. On
  definitions, `mut` is valid on struct helpers (including lifecycle helpers) to make the implicit `this` mutable; using
  `mut` on a `[static]` helper is a diagnostic. Executions do not accept `mut`.
- **`restrict<T>`:** constrain the accepted envelope to `T`. For bindings/parameters this is equivalent to writing the
  envelope directly (e.g., `[i32] x{...}`), and canonicalization rewrites `[i32]` into `[restrict<i32>]` at the low
  level.
- **`unsafe_api`:** marks a definition whose misuse can break memory safety (for example a wrapper that frees
  memory): its body is an unsafe scope as with `unsafe`, and only `[unsafe]` or `[unsafe_api]` definitions may call it
  (`calling /path requires an unsafe definition`).
- **`unsafe`:** marks a definition body as an unsafe scope. Aliasing rules are relaxed within the body, and
  `Reference<T>` bindings may use pointer-like initializers (`Pointer<T>`/`Reference<T>` values, including pointer
  arithmetic) when the pointee type matches. References created there must not escape the unsafe scope.
- **`return<T>`:** optional contract that pins the inferred return envelope. `return<auto>` requests inference;
  unresolved or conflicting returns are diagnostics.
- **`effects(...)`:** declare side-effect capabilities; absence implies purity. Backends reject unsupported
  capabilities.
- **Transform scope:** `effects(...)` and `capabilities(...)` are only valid on definitions/executions, not bindings.
- **`align_bytes(n)`, `align_kbytes(n)`:** encode alignment requirements for struct members and buffers. `align_kbytes`
  applies `n * 1024` bytes before emitting the metadata.
- **`no_padding`, `platform_independent_padding`:** layout constraints for struct definitions; they reject backend-added
  padding or non-deterministic padding respectively.
- **`capabilities(...)`:** reuse the transform plumbing to describe opt-in privileges without encoding backend-specific
  scheduling hints.
- **`compute`:** marks a definition as a GPU kernel; kernel bodies are validated against the GPU-safe subset.
- **`workgroup_size(x, y, z)`:** fixes the kernel's local workgroup size (must appear with `compute`).
- **`struct`, `pod`, `handle`, `gpu_lane`:** declarative tags that emit metadata/validation only. They never change
  syntax; instead they fail compilation when the body violates the advertised contract (e.g., `[pod]` forbids
  handles/async fields).
- **`public`, `private`:** visibility tags. On definitions, they control export visibility for imports (default:
  private). On bindings, they control field visibility (default: public). Mutually exclusive.
- **`static`:** on fields, hoists storage to namespace scope while keeping the field in the layout manifest. On struct
  helpers, disables the implicit `this` parameter and method-call sugar.
- **`reflect`:** marks a struct definition as reflection-enabled. It accepts no template arguments or call arguments and
  is rejected on non-struct definitions/executions. Use it when a type needs
  reflection metadata queries without requesting generated helpers.
- **`generate(...)`:** declares reflection helper generation intents for struct
  definitions. It accepts one or more generator names, implies reflection
  enablement for that type, and validates the generator allowlist (`Equal`,
  `NotEqual`, `Default`, `IsDefault`, `Clone`, `DebugPrint`, `Compare`,
  `Hash64`, `Clear`, `CopyFrom`, `Validate`, `Serialize`, `Deserialize`,
  `SoaSchema`) with deterministic diagnostics. Writing both `reflect` and
  `generate(...)` remains valid but is redundant when generation alone is
  sufficient.
  - **Naming contract:** generated helpers always use `/Type/Helper` paths (`/Type/Equal`, `/Type/Default`, etc.), where
    `Type` is the canonical struct path.
  - **Visibility contract:** generated helpers are emitted with explicit `[public]`, so `import /Type/*` exposes them by
    helper name.
  - **Collision/override contract:** generated helpers never silently override existing definitions; if any
    `/Type/Helper` path already exists, semantics reports `generated reflection helper already exists: /Type/Helper`.
  - **Current v1 generated helpers:** `Equal`, `NotEqual`, `Default`, `IsDefault`, `Clone`, and `DebugPrint`.
  - **`Equal`/`NotEqual`:** emits `/Type/Equal([Type] left, [Type] right) -> bool` and `/Type/NotEqual([Type] left,
    [Type] right) -> bool` with field-wise comparisons over non-static fields (`equal(...)` folded by `and`,
    `not_equal(...)` folded by `or`), using identity results when no instance fields exist (`Equal -> true`, `NotEqual
    -> false`). When `Equal` is generated for a reflected struct, surface `left == right` routes through the generated
    `/Type/Equal(left, right)` helper instead of requiring the explicit helper spelling.
  - **`Default`:** emits `/Type/Default() -> Type` returning `Type{}`; static fields are ignored by constructor entry
    mapping.
  - **`IsDefault`:** emits `/Type/IsDefault([Type] value) -> bool` by comparing non-static fields against a synthesized
    default instance (`Type{}`); structs with no instance fields return `true`.
  - **`Clone`:** emits `/Type/Clone([Type] value) -> Type` by rebuilding `Type{...}` from non-static fields
    (static-only structs clone as `Type{}`).
  - **`DebugPrint`:** emits `/Type/DebugPrint([Type] value) -> void` with deterministic line-based output (`/Type {`,
    one line per non-static field name, closing `}`, or `/Type {}` when no instance fields exist).
  - **v1.1 progress (`Compare`, `Hash64`, `Clear`, `CopyFrom`):** `Compare` emits `/Type/Compare([Type] left, [Type]
    right) -> i32` with lexicographic field ordering over non-static fields (`-1` when `left < right`, `1` when `left >
    right`, `0` when equal); `Hash64` emits `/Type/Hash64([Type] value) -> u64` and folds non-static fields in
    deterministic source order using `convert<u64>(field)` with stable FNV-style multiply/add constants; `Clear` emits
    `/Type/Clear([Type mut] value) -> void` and resets non-static fields by assigning from a synthesized default
    instance (`Type{}`); `CopyFrom` emits `/Type/CopyFrom([Type mut] value, [Type] other) -> void` and copies non-static
    fields from `other` into `value` in deterministic source order. Structs with no instance fields (or only static
    fields) keep identity/no-op behavior (`Compare -> 0`, `Hash64 -> 1469598103934665603u64`, `Clear -> no-op`,
    `CopyFrom -> no-op`), and `Compare`/`Hash64` now emit deterministic helper-scoped diagnostics when a field envelope
    is unsupported.
  - **v2 progress (`Validate`, `Serialize`, `Deserialize`):** `Validate` emits `/Type/Validate([Type] value) ->
    Result<FileError>` plus deterministic field-check scaffolding hooks `/Type/ValidateField_<field>([Type] value) ->
    bool` for each non-static field in source order. The generated hooks currently return `true` by default, and
    `/Type/Validate` short-circuits in source order: the first failing hook returns a deterministic non-zero error code
    (`1`-based field index), otherwise `Result.ok()`. `Serialize` emits `/Type/Serialize([Type] value) -> array<u64>`
    with a stable leading format tag (`1u64`) followed by `convert<u64>(field)` payload slots in deterministic
    non-static field order. `Deserialize` emits `/Type/Deserialize([Type mut] value, [array<u64>] payload) ->
    Result<FileError>`, enforces deterministic payload shape/version guards (`count(payload) == field_count + 1`,
    `at(payload, 0i32) == 1u64`), returns deterministic non-zero error codes (`1` = size mismatch, `2` = version
    mismatch), decodes payload slots in source order via `convert<fieldType>(at(payload, index))`, and returns
    `Result.ok()` on success. Structs with no instance fields (or only static fields) keep identity behavior (`Validate`
    returns `Result.ok()`, `Serialize` returns `array<u64>{1u64}`, `Deserialize` only applies the size/version guards
    then returns `Result.ok()`). `ToString` generation remains deferred; requesting `generate(ToString)` emits a
    deterministic diagnostic directing users to `DebugPrint`.
  - **v2.1 progress (`SoaSchema`):** `SoaSchema` emits `/Type/SoaSchemaFieldCount() -> i32`,
    `/Type/SoaSchemaFieldName([i32] index) -> string`, `/Type/SoaSchemaFieldType([i32] index) -> string`, and
    `/Type/SoaSchemaFieldVisibility([i32] index) -> string` over the non-static fields of `Type` in deterministic
    source order. `SoaSchema` also emits `/Type/SoaSchemaChunkCount() -> i32`,
    `/Type/SoaSchemaChunkFieldStart([i32] index) -> i32`, and `/Type/SoaSchemaChunkFieldCount([i32] index) -> i32`
    so wide reflected schemas can be grouped into deterministic sixteen-column storage chunks. `SoaSchema` now also
    emits `/Type/SoaSchemaStorage` and `/Type/SoaSchemaStorageNew()` on top of that chunk layer, where each chunk
    field is backed by `SoaColumn<T>` or `SoaColumnsN<...>` (`N <= 16`) over the reflected source-order fields.
    `SoaSchema` now also emits `/Type/SoaSchemaStorageCount([/Type/SoaSchemaStorage] value) -> i32`,
    `/Type/SoaSchemaStorageCapacity([/Type/SoaSchemaStorage] value) -> i32`,
    `/Type/SoaSchemaStorageReserve([/Type/SoaSchemaStorage mut] value, [i32] capacity) -> void`, and
    `/Type/SoaSchemaStorageClear([/Type/SoaSchemaStorage mut] value) -> void`, plus
    `/Type/SoaSchemaStorage/Destroy()`, routing chunk-wise count/capacity/reserve/clear and lifecycle cleanup
    through the existing fixed-width `SoaColumn<T>` / `SoaColumnsN<...>` substrate.
    These helpers bridge the current constant-index-only `meta.field_*<T>(i)` rule for reflected SoA work: callers can loop from `0` to
    `/Type/SoaSchemaFieldCount()` and query names/types/visibility through runtime
    `i32` indices without direct `meta.field_name<T>(...)` / `meta.field_type<T>(...)` /
    `meta.field_visibility<T>(...)` calls. Out-of-range string helper indices currently return the empty string
    sentinel, while out-of-range chunk helper indices return `0i32`. Wide reflected schemas can now synthesize
    deterministic chunked allocation, grow/realloc, explicit clear, and implicit drop/free cleanup on top of the
    fixed-width sixteen-column storage substrate.
- **Baseline reflection API scope (v1):** reflection APIs are compile-time-only metadata queries. Runtime reflection
  objects/tables are out of scope and rejected (`/meta/object`, `/meta/table`).
- **Reserved compile-time metadata query names:** `meta.type_name<T>`, `meta.type_kind<T>`, `meta.is_struct<T>`,
  `meta.field_count<T>`, `meta.field_name<T>(i)`, `meta.field_type<T>(i)`, `meta.field_visibility<T>(i)`,
  `meta.has_transform<T>(name)`, and `meta.has_trait<T>(traitName)` / `meta.has_trait<T, Elem>(Indexable)`. Query
  execution semantics now evaluate at compile time for the listed primitives; add a concrete reflection TODO before
  changing that metadata-query contract.
- **Current primitive status:** `meta.type_name<T>`, `meta.type_kind<T>`, `meta.is_struct<T>`, `meta.field_count<T>`,
  `meta.field_name<T>(i)`, `meta.field_type<T>(i)`, `meta.field_visibility<T>(i)`, `meta.has_transform<T>(name)`, and
  `meta.has_trait<...>(...)` evaluate at compile time in semantics. Reflection diagnostics for non-reflect
  field-metadata targets, invalid indices, and unsupported metadata query names are deterministic.
- **Field metadata target rule:** `meta.field_count<T>`, `meta.field_name<T>(i)`, `meta.field_type<T>(i)`, and
  `meta.field_visibility<T>(i)` require `T` to be a `reflect`-enabled struct.
- **Field metadata index rule:** `meta.field_name<T>(i)`, `meta.field_type<T>(i)`, and
  `meta.field_visibility<T>(i)` currently require constant integer indices, so `.prime`
  code cannot yet iterate arbitrary reflected field lists dynamically.
- **IR elimination rule:** reflection metadata queries must be eliminated before IR emission; the IR lowerer rejects
  non-eliminated reserved `/meta/*` reflection query paths.
- **`stack`, `heap`, `buffer`:** placement transforms reserved for future backends; currently rejected in validation.
- **`shared_scope`:** loop-only transform that makes a loop body share one scope across all iterations. Valid on
  `loop`/`while`/`for` only. Bindings declared in the loop body are initialized once before the loop body runs and
  persist for the duration of the loop without escaping the surrounding scope.
The lists above reflect the built-in transforms recognized by the compiler today; future additions will extend them
here.
