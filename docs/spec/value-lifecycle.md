# Move/Copy/Destroy, Optional Values, and Lambdas

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **normative**.

## Move/Copy/Destroy
- **Lifecycle set:** structured types can define `Create`, `Move`, `Copy`, and `Destroy` helpers. `Create`/`Destroy`
  are optional hooks; `Move`/`Copy` must be nested inside the struct, return `void`, and accept exactly one parameter.
- **Copy signature:** the canonical copy constructor is `Copy([Reference<Self>] other) { ... }`. A shorthand
  `Copy(other) { ... }` desugars to the reference form.
- **Move-by-default:** assignments and returns consume values unless the type is `Copy` or the value is a
  `Reference<T>`. Argument passing follows the parameter's mode instead (see Parameter Passing below): by default a
  parameter borrows its argument.
- **`Copy` types (Rust-aligned):** values that can be duplicated by a simple bitwise copy with no custom destruction.
  - Built-in `Copy` types: `bool`, `i32`, `i64`, `u64`, `f32`, `f64`, `Pointer<T>`, `Reference<T>`.
  - Structs are `Copy` when they are `[pod]`, all fields are `Copy`, and they do **not** define `Destroy` or `Copy`.
  - Types with `handle`/`gpu_lane` fields are not `Copy` (they cannot be `[pod]`).
- **Copy helper use:** ownership-sensitive lowering may route aggregate duplication through `Copy` when one exists.
- **Move signature:** the canonical move constructor is `Move([Reference<Self>] other) { ... }`. A shorthand
  `Move(other) { ... }` desugars to the reference form.
- **Explicit move:** `move(value)` is the explicit consume helper. It requires a local binding or parameter name (no
  arbitrary expressions) and marks the source binding as moved-from while returning its value.
- **Use-after-move:** any use of a moved-from binding is a compile error until it is re-initialized (e.g.,
  `assign(value, ...)`). The analysis is conservative across control flow.
- **Pointer behavior:** pointers can be moved; moved-from pointers are treated as invalid without being auto-zeroed (no
  implicit `null` literal; `0x0` is just a numeric value).
- **References:** `move(...)` rejects `Reference<T>` bindings; references do not participate in move semantics.
- **Backend note:** `move(...)` is a semantic ownership marker. VM/native lower it as a passthrough; the C++ emitter
  emits `std::move`.

## Parameter Passing
- **Modes:** a parameter's transforms choose how its argument is passed. Every mode is observably the same for `Copy`
  scalars except where noted; backends may pass small values in registers.

  | Parameter | The callee gets | The caller |
  | --- | --- | --- |
  | `[T] x` | a read-only borrow of the argument (no copy) | keeps the value; it cannot change during the call |
  | `[T mut] x` | a mutable borrow; its writes are the caller's | must pass a mutable place (binding, field or element of one) or a temporary |
  | `[T copy] x` | its own read-only copy | keeps the value; `move(v)` hands it over without a copy |
  | `[T copy mut] x` | its own copy it may change | as for `copy` |
  | `[T move] x` | the value itself (passed by reference, no copy); the callee owns it | the binding ends: it is moved-from after the call |
  | `[T move mut] x` | as for `move`, and it may change it | as for `move` |

- **Borrows at a call:** borrowed parameters (`[T]`, `[T mut]`) follow the borrow rules of `type-system.md` for the
  duration of the call: a place passed to a `mut` parameter may not also be passed to any other parameter of the same
  call, and a borrowed parameter cannot escape the call (it cannot be returned, stored, or moved).
- **Ownership:** a `move` parameter's value is destroyed when the callee's scope ends unless the callee moves it on
  (for example into a container); the caller does not destroy it. `move(...)` at the call site is optional for a `move`
  parameter. Using the caller's binding afterwards is a `use-after-move` error until it is reassigned. A temporary
  argument may be passed to any mode.
- **Copies:** `copy` duplicates through the type's `Copy` helper when it has one (collections copy their elements),
  otherwise bitwise for `Copy` types; a struct without a `Copy` helper copies its fields, running the helpers of the
  fields that have one. The same applies to a binding initialized from an existing place (a binding, field or
  dereference): `[Vector<i32> mut] b{a}` gives `b` its own elements, so changing or destroying either leaves the other
  intact. A non-`mut` collection binding (`vector`, `map`, `soa_vector`) initialized from a place is a read-only view of it
  and does not copy; it borrows the place's root binding until its last use, so the root cannot be changed (passed to a
  `mut` or `move` parameter, or assigned) while the view is still used: `borrowed binding: <root>`. A temporary (a
  call result or constructor) or `move(x)` initializer is moved in without a copy.
- **Implementation status:** the IR lowering passes non-`mut` struct and collection arguments by alias and treats
  `mut` parameters as borrows whose writes reach the caller. Semantics checks that a `mut` argument is a mutable place
  (a non-`mut` binding is `mut parameter requires a mutable place`; literals and other temporaries, fields and elements
  are accepted, and `Reference`/`Pointer`/capability-view parameters are exempt), that a binding passed to a `mut` or
  `move` parameter is not passed to another parameter of the same call (`borrow conflict`), and that a borrowed
  parameter of an owning type (a container, a type that defines `Destroy`, or one holding either) is not returned,
  assigned or moved (`borrowed parameter escapes via return` / `via assignment`, `borrowed parameter cannot be
  moved`). A named argument of an owning type passed to a `move` parameter is moved-from after the call (values of
  other types are copied). Stdlib container entry points that store their argument (push, insert, slot writes) take
  it as `move`. A `copy` parameter gets its own storage: scalars by value, structs slot by slot and then through the
  type's `Copy` helper when it defines one (or its fields' helpers); a `move(v)` argument is handed over without a
  copy. The stdlib `Vector`, `RingBuffer` and `SoaColumn` `Copy` helpers allocate their own storage and copy each
  element. An owning local (a struct with `Destroy`, `DestroyStack` or a field that has one, including containers) is
  destroyed when its scope ends on every exit path (fall-through, `return`, including from nested blocks of an inlined
  callee), last declared first, unless its value was moved out (`move(x)`, a `move` parameter) or named in the returned
  value; a per-local drop flag tracks this at run time. A struct field initialized from a place copies it, and
  `assign` of an owning value destroys the old value and copies the new one. A `copy` or `move` parameter is
  destroyed by the callee when its body ends (the caller does not destroy a binding it passed to a `move` parameter),
  `init(slot, x)` hands `x` to the storage, and dropping a container slot (`pop`, `clear`, `Destroy` of the container)
  destroys the element. Structs whose fields all have one scalar type, and locals left by error propagation out of a
  nested block, are not destroyed yet (TODO-5497); both only leak.

## Uninitialized Storage (draft)
- **Purpose:** model explicit, inline uninitialized storage without implicit construction (C-style tagged storage and
  optional values).
- **Envelope:** `uninitialized<T>` allocates space for `T` but does not construct a `T` value.
- **Allowed locations:** local bindings and struct fields only. `uninitialized<T>` is rejected for parameters, return
  types,
  collection elements, nested pointer/reference targets, or template arguments to user-defined types. Top-level
  `Pointer<uninitialized<T>>` and `Reference<uninitialized<T>>` wrappers are allowed as storage handles.
- **Initialization:** `init(storage, value)` constructs `T` in-place.
  - `storage` must be `uninitialized<T>` or `dereference(ptr)` where `ptr` is `Pointer<uninitialized<T>>` or
    `Reference<uninitialized<T>>`, and it must be definitely uninitialized at the call site.
  - `value` is consumed (move-by-default); use `clone(value)` to duplicate.
  - After `init`, the storage is definitely initialized.
- **Destruction:** `drop(storage)` destroys the in-place value and marks the storage uninitialized.
  - `storage` must be definitely initialized; otherwise a diagnostic is emitted.
- **Access:**
  - `take(storage)` moves the value out and leaves the storage uninitialized.
  - `borrow(storage)` now supports standalone `[Reference<T>]` bindings for direct local/field storage and
    pointer/reference-backed `borrow(dereference(slot))` storage surfaces, and helper
    `return<Reference<T>>` contracts now also accept direct borrowed carriers rooted in parameter-owned storage.
    Internal stdlib slot-borrow helpers now also validate through `[return<Reference<T>>]` with slot-pointer
    provenance preserved through local `slot` aliases, while public value-returning helpers such as
    `soaColumnRef<T>(...)`, `soaColumnRead<T>(...)`, `vectorAt<T>(...)`, and `vectorAtUnsafe<T>(...)` still
    intentionally dereference that borrowed carrier back to whole-element `T`. The storage must be initialized.
  - Any other use of an `uninitialized<T>` value is a type error.
- **Lifetime rules:** using a storage value after `drop`/`take` is a compile-time error until it is reinitialized.
- **`Destroy` handling:** structs owning `uninitialized<T>` fields must explicitly `drop` them when initialized.

## Constructor-Shaped Compatibility Inventory
- **Default rule:** value construction uses braces (`Type{...}` or a context-typed `{...}`). A call-shaped form that
  looks like construction is an ordinary helper call or a documented compatibility rewrite, not field-mapping
  construction.
- **File:** imported `File<Mode>(path)` is retained as compatibility helper syntax. It resolves through
  `/File/openRead(...)`, `/File/openWrite(...)`, or `/File/openAppend(...)` after `/std/file/*` is imported and remains
  subject to the same `file_read` / `file_write` effects. Lowerer inference coverage locks this as a fallible helper
  return, not as a `File<Mode>` value constructor.
- **Graphics:** imported `Window(...)`, `Device()`, and `Buffer<T>(count)` are retained compatibility entry points over
  `/std/gfx/*` and `/std/gfx/experimental/*`. `Window(...)` and `Device()` route through stdlib create helpers and keep
  their `Result`/`?` shape; `Buffer<T>(count)` routes through the gfx buffer helper rewrite. Gfx semantic tests also keep
  bare explicit bindings without `?` on the mismatch path, proving these are not plain value constructors.
- **Collections:** brace forms such as `array<T>{...}`, `vector<T>{...}`,
  and `soa<T>{...}` are preferred construction syntax for compiler-owned collection envelopes.
  Map construction is stdlib-owned and should use canonical `/std/collections/map/map<K, V>(...)`
  and `/std/collections/map/entry<K, V>(key, value)` helpers, or bare helper aliases from import surfaces that publish
  them. Legacy call-shaped `array<T>(...)`, `vector<T>(...)`,
  canonical `/std/collections/vector/vector<T>(...)`, and
  `/std/collections/map/map<K, V>(...)` spellings remain compatibility helper
  families registered through stdlib surface metadata; `soa_vector<T>(...)`
  is a rejected compatibility spelling. Semantic-product tests publish their
  constructor surface IDs as helper metadata rather than struct field
  construction.
- **Maybe/Result migration:** `Maybe<T>` is now a stdlib-owned sum with `none` and `some` variants. `Maybe<T>{}`
  defaults to `none`, `Maybe<T>{none}` is the explicit empty form, `some<T>(value)` is the named helper, and
  `Maybe<T>{value}` is accepted when the `some` payload is the only matching variant. Mutable `Maybe<T>` helper
  calls are intentionally retired for the sum-backed surface; use explicit construction plus `pick` instead. Result
  helper compatibility is separately inventoried and fenced rather than owned by file/gfx/collection compatibility.
- **Follow-up policy:** do not add new constructor-shaped compatibility surfaces. Existing retained forms must either
  route through named helpers with focused coverage or get a dedicated migration TODO with scope, acceptance, and
  stop_rule before their status changes.

## Optional Values (Maybe)
- **Purpose:** represent either "no value" or a value of `T` without heap allocation.
- **Naming:** `Maybe<T>` is the canonical optional type in PrimeStruct; there is no separate `Option<T>`.
- **Concrete representation:** `Maybe<T>` is a stdlib-owned generic sum type with a unit `none` variant followed by a
  payload-carrying `some` variant. The active variant is represented by the generic sum layout contract.
- **Ergonomic constructor surface:** `Maybe<T>{}` yields `none`, `Maybe<T>{none}` is the explicit empty form, and
  `some<T>(value)` constructs the present variant. Direct `Maybe<T>{value}` construction is also accepted when the
  payload uniquely matches the `some` variant; use `some<T>(value)` when call-site clarity matters.
- **Helper surface (stdlib):** `isEmpty()` / `isSome()` and compatibility wrappers `is_empty()` / `is_some()` are
  implemented as type-path helpers over `pick`. The old mutable struct helpers `set(value)`, `clear()`, and `take()`
  are retired for the sum-backed representation; callers should rebind with `some<T>(value)`, explicit `[some]`
  construction, `Maybe<T>{}`, or `none<T>()`, and use `pick(...)` to inspect or move payloads explicitly. A future
  mutable active-variant helper surface would need a new language contract and TODO before it is added.
- **Example shape:**
  ```
  [sum]
  Maybe<T> {
    none
    [T] some
  }

  [return<Maybe<T>>]
  some<T>([T] value) {
    [Maybe<T>] result{[some] value}
    return(result)
  }

  [return<Maybe<T>>]
  none<T>() {
    [Maybe<T>] result{}
    return(result)
  }

  [return<bool>]
  /Maybe/isSome<T>([Maybe<T>] self) {
    return(pick(self) {
      none {
        return(false)
      }
      some(value) {
        return(true)
      }
    })
  }
  ```

Draft tests (shape only):
```
// Positive: create empty, create filled, inspect with pick.
[return<i32>]
maybe_basic() {
  [Maybe<i32>] a{none<i32>()}
  [Maybe<i32>] b{some<i32>(1i32)}
  return(pick(b) {
    none {
      return(0i32)
    }
    some(value) {
      return(value)
    }
  })
}

// Negative: retired mutable helpers.
[return<i32>]
bad_set() {
  [Maybe<i32> mut] value{none<i32>()}
  value.set(1i32) // error: sum-backed Maybe<T> has no mutable helper set
  value.clear() // error: sum-backed Maybe<T> has no mutable helper clear
  [i32] out{value.take()} // error: sum-backed Maybe<T> has no mutable helper take
  return(0i32)
}
```

## Lambdas & Higher-Order Functions
- **Syntax mirrors definitions:** lambdas omit the identifier (`[captures] <T>(params){ body }`). The capture list is
  required but may be empty (`[]`). Template arguments are optional.
- **Capture semantics:** supported forms are `[]`, `[=]`, `[&]`, and explicit entries (`[name]`, `[value name]`, `[ref
  name]`). Entries are comma/semicolon separated. Explicit names must resolve to parameters or locals in the enclosing
  scope; duplicates are diagnostics. `=` or `&` capture-all tokens cannot be repeated. `ref` marks a by-reference
  capture; otherwise captures are by value.
- **Parameters:** lambda parameters must use binding syntax. Defaults are allowed but must be literal/pure expressions
  and may not use named arguments.
- **Backend support (current):** lambdas are supported only by the C++ emitter, which lowers them to native C++ lambdas
  with the same capture list. IR/VM/GLSL backends reject lambdas; use named definitions when targeting those backends.
- **Inlining transforms:** standard transforms may inline pure lambdas in C++ emission paths; non-pure lambdas remain as
  closures.
