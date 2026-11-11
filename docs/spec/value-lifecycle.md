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
- **Move-only types:** a struct that defines `Destroy` and no `Copy` owns something that must be released exactly once
  (a socket, a descriptor), so it cannot be duplicated. Using a place of such a type as a value is a compile error:
  `move-only type /Handle cannot be copied: use move(a)`. The places are a binding initializer (`[Handle] b{a}`), an
  assignment (`b = a`) and a `copy` parameter argument, where `a` is a binding, parameter, field or element. Moving
  works: `move(a)`, a temporary (`make()`, `Handle{}`), a returned local, a `[T move]` parameter, and borrowing through
  a plain `[T]` or `[T mut]` parameter. A type that wants copies defines `Copy`. Generic code (template instances) and the stdlib are exempt, because a
  container copies elements of any type bitwise by design. `File<Mode>` keeps its builtin rule.

  ```prime
  [struct]
  Handle() {
    [i32 mut] fd{7i32}
    Destroy() {
    }
  }

  [return<int>]
  main() {
    [Handle] a{Handle{}}
    [Handle] b{move(a)}
    // [Handle] c{b} would not compile: move-only type /Handle cannot be copied
    return(b.fd)
  }
  ```
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
  parameter. Using the caller's binding afterwards is a `use-after-move` error until it is reassigned, on every path:
  a move in one `if` branch or one `pick` arm reaches the code after the join (each branch or arm starts from the
  state before it, and a `pick` payload binding's move ends with its arm, so a later arm or `pick` may bind the same
  name), and a loop body cannot move a binding from outside the
  loop unless it assigns it again before the next iteration. The diagnostic names the binding, where it moved and what
  to write instead: `use-after-move: row was moved at 9:13; assign row a new value before using it again, or keep a
  copy before the move: [vector<i32> mut] kept{row}`. The builtin `vector<T>` `push` (method `a.push(b)` or
  `push(a, b)`) is such an entry point: a named owner (an owning local or a `copy`/`move` parameter) is moved into the
  vector, while a borrowed parameter or a read-only view (a non-`mut` collection bound from a binding, field or
  element) owns nothing and is copied, so it stays usable. Why the move stays implicit rather than requiring
  `a.push(move(b))`: the checker rejects every later use of `b` either way, so requiring `move(...)` would add a word to
  the common, safe case (pushing a value that is not used again) without catching anything more; `move(b)` remains
  accepted for readers who want the hand-over spelled out. A temporary
  argument may be passed to any mode. A temporary (a constructor or a call returning a value, not a reference) has no
  other owner, so it is destroyed exactly once at the end of its full expression: the callee owns and destroys it
  whatever the parameter mode (a `copy` parameter takes it over without running `Copy`), a discarded temporary is
  destroyed at the end of its statement (a call result, a constructor statement such as `Named{...}`, a bindings-only
  definition called as a statement, which constructs its implicit struct, and a fresh `vector<T>(...)` or call
  returning one, whose elements are destroyed and buffer freed), and a temporary field-access receiver (`make().id`) is destroyed once the
  field is read. A fresh builtin vector that is only read (`make_values().count()`, `capacity()`,
  `make_values()[0i32]`, `at`, `at_unsafe`, and a scalar field of an element such as `make_rows()[1i32].id`) is
  destroyed, elements and buffer, once the scalar result is loaded. An element of a vector temporary read as a
  struct, a `String` or a vector is not supported yet (TODO-5888).
- **Copies:** `copy` duplicates through the type's `Copy` helper when it has one (collections copy their elements),
  otherwise bitwise for `Copy` types; a struct without a `Copy` helper copies its fields, running the helpers of the
  fields that have one. A user `Copy` helper runs after that field-wise copy, so `this` already owns copies of its
  fields (it never aliases `other`'s storage) and the helper only adjusts what it needs; assigning a field destroys
  that copy first. The same applies to a binding initialized from an existing place (a binding, field, element or
  dereference): `[Vector<i32> mut] b{a}` gives `b` its own elements, and `[Todo mut] t{todos[i]}` gives `t` its own
  `String` and `Function` fields, so changing or destroying either leaves the other intact. The builtin `vector<T>`
  spelling copies the same way: `[vector<i32> mut] b{a}`, a field built from a vector binding (`KeepV{[seen] start}`),
  a copy of a struct holding one, and a by-value capture (`[mut values]`) each get a buffer of their own. A builtin
  `vector<vector<T>>` stores each inner vector inline: `a.push(v)` takes over a fresh vector, `move(v)` or an owning
  binding and copies any other place (a view, field or element), `a[i]` is the inner vector (a read-only view when
  bound without `mut`, a copy of its own when bound with `mut`: `[vector<i32> mut] row{a[0i32]}`, likewise a field
  `[vector<i32> mut] items{bag.items}`), a mutating method on it changes it in place (`a[0i32].push(5i32)`,
  `this.rows[i].pop()`, `clear()`), and a copy of the outer vector gives every inner vector a buffer of its own;
  destroying it frees every inner buffer. A builtin vector mutator (`push`, `pop`, `clear`, `reserve`, `remove_*`)
  needs a mutable root: the vector, or the binding holding it, is `mut` or a `copy`/`move` parameter, else `push
  changes rows, which is not mutable: declare it [vector<vector<i32>> mut] rows` (a change through a `Reference` or
  `Pointer` follows their rules; a user definition named like the mutator is an ordinary call). A non-`mut` collection binding (`vector`, `map`, `soa_vector`) initialized from a place (a
  binding, field or element: `[vector<i32>] row{rows[0i32]}`) is a read-only view of it and does not copy; it borrows
  the place's root binding until its last use, so the root cannot be changed (passed to a `mut` or `move` parameter,
  assigned, or changed by a builtin vector `push`, `pop`, `clear`, `reserve` or `remove_*`, including pushing the view
  into it) while the view is still used: `borrowed binding: rows (root: rows, sink: row); rows cannot change while row
  still reads it: declare row `mut` to give it its own copy`. A temporary (a call result or constructor) or `move(x)`
  initializer is moved in without a copy. A `push` whose argument is an element of the same vector
  (`rows.push(rows[0i32])`) works: the element is read where the buffer moved to when the push grows it, and like any
  struct or vector pushed from a place (an element, a field, a borrowed parameter) it is copied, so the vector and the
  place each own their own String and vector buffers.
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
  copy. A builtin `[vector<T> copy]` parameter (also `copy mut`) gets a buffer of its own holding a copy of each element, except from a
  fresh vector (a constructor or a call returning one) or `move(v)`, which it takes over; the callee frees it when
  its body ends, and a fresh vector passed to a read-only `vector<T>` parameter is freed there too, also when the parameter is declared as the stdlib `Vector<T>` or as a template type bound to it (`some<Vector<i32>>(vector<i32>(1i32))`). The stdlib `Vector`, `RingBuffer` and `SoaColumn` `Copy` helpers allocate their own storage and copy each
  element. A struct's own `Destroy` runs first and its fields are destroyed after it, last declared first. A sum local
  (`Maybe<T>`, a user `[sum]`, the stdlib `Result`) initialized from a fresh value destroys its active payload the same
  way (`try(r)` on a named `Result` copies the ok payload out through its `Copy` helper, so `r` and the copy each destroy
  their own), and a payload
  taken from a place is copied in like a struct field. A sum that holds such a payload is an owning value like a
  struct with `Destroy`: a fresh one (a call result, a constructor, a `pick` expression) passed as an argument belongs
  to the parameter and is destroyed when the callee's body ends, a named one given to a `move` parameter is moved-from
  and destroyed by the callee, and a read-only sum parameter cannot be returned (`borrowed parameter escapes via
  return`). Assigning to a `mut` sum local takes the new value over from a temporary or `move(other)` and copies it
  from a place (the place keeps its payload), then destroys the payload the local held; `held = held` is valid. A
  `pick` whose subject is a fresh value (`pick(get())`, a method
  call included, or a constructor) owns it for as long as the `pick` runs and destroys its active payload when the
  `pick` ends or a `return` in an arm leaves the function. The payload binding of an arm stands for what the sum owns:
  moving it out (`move(line)`, a `move` parameter, `return(line)`) leaves nothing for the sum to
  destroy, so the payload is destroyed once, by its new owner. A fresh subject ends with the `pick`; a sum local or
  `copy`/`move` parameter whose payload was moved out is moved-from after the arm (`use-after-move` until it is
  assigned again). The payload of a sum the definition only borrows (a read-only parameter, `this`, a field, an
  element, a `Reference`) cannot be moved out or returned: `borrowed pick payload cannot be moved`, `borrowed pick
  payload escapes via return`. A builtin
  `vector<T>` payload is owned by the sum like a `vector<T>` field by its struct: one taken from a place gets a buffer of
  its own, a fresh one is taken over, and destroying the sum destroys the elements and frees the buffer. Its payload
  binding is a vector binding the sum keeps owning, so a `mut` binding initialized from it and `return(values)` copy it
  like any named vector. A sum stored as an element of a builtin `vector<T>` (`vector<Maybe<String>>`, a vector of
  a user `[sum]`) is owned by the vector like a struct element: `push` takes over a fresh sum or a named owner and
  copies any other place, a copy of the vector gives each element a payload of its own, and `pop`, `clear` and
  destroying the vector destroy the payload of each element's active variant. A `pick` expression whose arms yield a struct or a `vector<T>` makes a value of its own, which
  the binding or argument it initializes owns (an argument is destroyed after the call, a discarded result at once);
  `return(pick(...) { ... })` hands it to the caller and `name = pick(...) { ... }` replaces what a `mut` local held:
  an arm's fresh value is taken over, and so is a local that ends with the arm, which is a local declared in the arm
  or the payload binding of a fresh subject; any other place is copied with the type's `Copy` helpers and stays
  readable: a local or parameter of the enclosing scope, the payload binding of a sum local, parameter or field, a
  field, an element. An owning local (a struct with `Destroy`, `DestroyStack` or a field that has one, including containers) is
  destroyed when its scope ends on every exit path (fall-through, `return`, including from nested blocks of an inlined
  callee), last declared first, unless its value was moved out (`move(x)`, a `move` parameter) or named in the returned
  value; a per-local drop flag tracks this at run time. A builtin `vector<T>` local initialized by a constructor or a
  call returning a vector owns it the same way: its elements are destroyed and its buffer freed when its scope ends,
  a `mut` binding initialized from another vector binding owns its copy the same way, and a read-only one is a view
  and destroys nothing. A builtin `vector<T>` field of a struct (including a closure's by-value capture and the
  closure a `Function` box holds) owns its buffer too: destroying the struct destroys the field's elements and frees
  the buffer. A move hands the buffer over without a second owner, and a record that does not own its buffer (a view,
  a disarmed temporary) frees nothing. A struct field initialized
  from a place copies it, and
  `assign` of an owning value destroys the old value and copies the new one. Assigning to a `mut` builtin
  `vector<T>` local does the same: a fresh vector (a call, a constructor, a `pick` expression) is taken over, a
  place is copied with a buffer of its own and keeps its elements, and the vector the local held is destroyed
  (`kept = kept` is valid); a local whose `vector<T>` type is inferred (`[mut] names{make_names()}`) behaves like
  a declared one. Assigning to a `[vector<T> mut]` parameter that borrows the caller's vector destroys that vector
  and writes the new one in its place, so the caller sees the new vector and destroys it. A `[vector<T> move]`
  parameter and a fresh vector given to any vector parameter belong to the callee, which destroys every element.
  A `copy` or `move` parameter is
  destroyed by the callee when its body ends (the caller does not destroy a binding it passed to a `move` parameter),
  `init(slot, x)` hands `x` to the storage, and dropping a container slot (`pop`, `clear`, `Destroy` of the container)
  destroys the element, also on a builtin `vector<T>` of structs or vectors held in a local, a field
  (`grid.rows.pop()`) or an outer vector (`a[i].clear()`), and in the direct forms `pop(v)` and `clear(v)`. Error propagation (`?`) needs no extra cleanup: a handler does not flow into nested blocks, so
  `?` only runs at the top level of the body that declares `on_error`, and leaving through it destroys that body's
  owning locals like a `return`.

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
- **A lambda is a closure value.** The `lambda-closures` semantic pass (before template monomorphization) turns each
  lambda into a closure struct named after the definition it is written in (`/main__lambda0`): one field per capture
  and a `call` method holding the body. The lambda expression becomes a call of a generated factory
  (`/main__lambda0_make`) whose implicit template parameters are the capture types, so capture types are inferred from
  the enclosing bindings like any implicit template argument. A closure with a `[ref name]` capture is built in place
  instead (`/main__lambda0{[name] location(name)}`). Closure structs live in the enclosing definition's namespace,
  next to a struct and never inside it. To every later stage they are ordinary structs, so closures run on every
  backend that runs structs (VM, native, `optcpp`/`optexe`, `cpp`/`exe`).
- **Capture forms** (comma or semicolon separated; duplicates and unknown names are diagnostics):

  | Form | Meaning |
  | --- | --- |
  | `[name]`, `[value name]` | copy `name` into a field when the lambda expression runs; read-only in the body |
  | `[mut name]` | copy, and the body may change the closure's own copy (never `name` itself) |
  | `[move name]` | move `name` into the field; `name` is moved-from afterwards |
  | `[ref name]` | borrow `name`: reads and (when `name` is `mut`) writes reach the binding itself |
  | `[name{expr}]`, `[mut name{expr}]` | init capture: a field initialized by `expr`, evaluated where the lambda is written |
  | `[=]`, `[&]` | capture by value / by reference every enclosing binding the body uses (not every local) |

  The payload binding of a `pick` arm is an enclosing binding for the lambdas written in that arm's body, and for no
  other arm.

  A body that reads an enclosing binding it does not capture is `unknown identifier: name (the lambda does not
  capture it; add [name])`; writing a non-`mut` capture is `assign target must be a mutable binding`. A `[ref name]`
  of a binding that already is a `Reference` or `Pointer` copies that reference.
- **Calling:** `f(x)` where `f` is a local, parameter or field whose type is a struct with a `call` method means
  `f.call(x)`, with labeled arguments allowed; indexing then calling (`handlers[i]` followed by `(x)`) calls the
  element. Lambdas and hand-written structs with `call` are therefore interchangeable: a generic
  `drive<Handler>([Handler mut] handler)` takes either and is monomorphized per type (a direct call, no allocation).
  `Holder{[handler] lambda}` infers `Holder<F>`'s template arguments from its field arguments.
- **Mutable state:** `call` is a `mut` method when any value or init capture is `mut`, so such a closure must be held
  in a `mut` binding, field or parameter to call it. State lives in the closure value: a closure passed to a `mut`
  parameter is changed in place, and a copy has its own state. Writing through a `[ref name]` capture does not change
  the closure, so it does not make `call` `mut`.
- **`return`** inside a lambda returns from the lambda (its `call` method), never from the enclosing definition. This
  holds for lambdas passed to `Result.map`/`and_then`/`map2` too: the pass gives those helpers a one-statement
  trampoline lambda that calls the closure, so the body's early returns stay inside `call`.
- **Parameters:** lambda parameters must use binding syntax. Defaults are allowed but must be literal/pure expressions
  and may not use named arguments. A lambda parameter or body binding may not reuse a capture name.
- **Effects:** the closure's `call` method (and its factory) take the `effects(...)` and `capabilities(...)` of the
  definition the lambda is written in; a caller of the closure needs them like any method call.
- **Copy, Move, Destroy:** field-wise, from the capture types, like any struct.
- **Borrowing:** a closure with a `[ref name]` capture, or one that captures a view or carrier by value, is a carrier
  (`strings-and-views.md`): it borrows those roots until its last use (`borrowed binding: name (root: name, sink:
  closure)`), and it cannot be stored in a field, a container or an outer binding, returned, or converted to
  `Function` (`closure escapes via Function (root: name)` for a `[ref]` lambda written at the conversion, `closure
  cannot be a Function: capture `name` is Reference<T>` for a named one, below). A closure that captures a String made inside an open
  `StringArena` follows the String's arena rules. Its `call` always runs inline, in the frame that holds the borrowed
  locals. A sendable closure for tasks (`MultithreadingPrototype.md`) is one with no `ref` captures and no `mut`
  binding captured by reference.
- **Type spelling:** a closure type has no source name. Pass closures through template parameters (`[F]`, `[F mut]`,
  `Holder<F>` fields), or erase the type with `Function<R, A...>` (below).

### Function values
- **`Function<R, A0, ..., An>`** is a value that calls any closure (or struct with `call`) taking `A0..An` and
  returning `R`; `Function<R>` takes no arguments. It holds the closure in one heap block, so one `vector`, struct
  field or parameter type holds different lambdas. Converting copies the closure into that block (needs
  `heap_alloc`); brace construction spells it out: `Function<i32, string>{handler}` or
  `Function<i32>{[mut n{0i32}]() { ... }}`.
- **Implicit conversion:** a closure written where a `Function<R, A...>` is expected converts as if wrapped in
  `Function<R, A...>{...}`. The expected-type sites are a parameter (`apply(lambda, 1i32)`, `router.add(1i32,
  handler)`), a brace-constructor field (`Route{[handler] hits}`), a binding with a declared `Function` type, an
  assignment to a `Function` local and the value returned from a definition whose `return<...>` is a `Function`. The
  closure there is a lambda, a local or parameter whose type is a closure or a struct with `call`, or a brace
  constructor of such a struct (`apply(Tripler{}, 2i32)`). A `Function` value passes as it is.
- **Named definitions:** `Function<i32, i32>{/double}` (or `{double}`) holds the free function `/double`: the closure
  pass generates a closure struct without fields, `/double__fnref`, whose `call` forwards its arguments. A bare
  definition name converts the same way at an implicit site (`router.add(1i32, double)`). A definition that is
  generic, a struct method, or carries transforms other than `return`, `effects`, `capabilities`, `public` and
  `private` stays a name and does not convert.
- **Signatures:** the closure's `call` must take the `Function`'s argument count, and its parameter types must accept
  the `Function`'s argument types: `Function<i32, i32> cannot hold /main__lambda0 in /main: its parameter text is
  string, the Function passes i32` or `... cannot hold /add in /main: it takes 2 arguments, the Function passes 1`.
  An explicit `return<T>` on `call` (a named definition, a hand-written struct) is checked against `R` the same way.
- **Calling** `f(x)` dispatches to the stored closure; `call` is `mut` (the closure may change its state), so hold
  the `Function` in a `mut` binding, field or parameter.
- **Copy** clones the stored closure into a new block (the copy has its own state); **Move** hands the block over;
  **Destroy** destroys the closure and frees the block.
- **Lowering:** the closure pass spells `Function<R, A...>` as the struct `/__FunctionN<R, A...>` (a tag and the heap
  address) and a conversion as `/__FunctionN<R, A...>{[tag] 0i32, [env] /__fn_box(closure)}`. After
  monomorphization the `function-values` pass numbers the closure types converted to each `Function` instance and
  writes its `call` (an `if` chain on the tag), `Copy` and `Destroy`. No IR change: dispatch is ordinary struct and
  pointer code, so `Function` runs on the VM, native and `optexe` alike.
- **Sendable by construction:** a `Function` may cross to another task (`MultithreadingPrototype.md`, Functions Are
  Sendable), so every conversion, explicit or implicit, accepts only a closure whose captures are sendable (the
  spawn-argument rule of `language-core.md`): a capture that is a `Pointer`, a `Reference`, a view (`StringView`, a
  slice), a `Task` or a struct holding one is rejected where the closure is converted, with the path to it:
  `closure cannot be a Function: capture `view` is StringView`, `closure cannot be a Function: capture `rows`, field
  `bytes` of PixelRows is Pointer<u8>`. This holds whether or not the program uses tasks. State a handler works on
  comes in through its parameters (`Reference<State>`), not through a captured pointer. The heap block holds its own
  copy of the closure, so a `String` or `vector` capture belongs to the `Function` (example below).
- **Limits:** a borrowing closure cannot be converted, explicitly or at an implicit site (`closure escapes via Function
  (root: name)`, above). An implicit conversion needs the expected type without inference: it does not happen for a
  parameter of a generic definition, a field of a generic struct, an element pushed onto a `vector<Function<...>>`, or
  a closure produced by an arbitrary call (bind it to a local or construct the `Function` explicitly). A parameter of a
  method of a generic struct does convert, since its type is known once the struct is instantiated
  (`AppRouter<Counter>.get`, docs/spec/stdlib-reference.md). A lambda whose
  capture list starts with `mut` cannot yet be written directly as a call argument (the parser reads `[mut ...]` as
  an argument label); bind it to a local first.

Example (runs on the VM, native and `optexe`):

```prime
import /std/collections/*

[struct]
Route {
  [i32] code
  [Function<i32, i32> mut] handler
}

[return<i32> effects(heap_alloc)]
route_request([vector<Route> mut] routes, [i32] code, [i32] input) {
  [i32 mut] i{0i32}
  while(i < routes.count()) {
    if(routes[i].code == code) {
      return(routes[i].handler(input))
    }
    i = i + 1i32
  }
  return(404i32)
}

[return<int> effects(heap_alloc)]
main() {
  [i32] base{100i32}
  [mut] hits{[mut calls{0i32}]([i32] input) {
    calls = calls + 1i32
    return(calls)
  }}
  [vector<Route> mut] routes{vector<Route>()}
  routes.push(Route{[code] 1i32, [handler] Function<i32, i32>{hits}})
  routes.push(Route{[code] 2i32, [handler] Function<i32, i32>{[base]([i32] input) { return(input + base) }}})
  route_request(routes, 1i32, 0i32)
  [i32] second{route_request(routes, 1i32, 0i32)}     // 2: the route's copy of `hits` keeps counting
  return(second + route_request(routes, 2i32, 5i32))  // 2 + 105
}
```

The same conversions without the wrapper, at a parameter, a field and a binding, with a named definition (runs on
the VM, native and `optexe`):

```prime
[struct]
Hook {
  [Function<i32, i32> mut] run
}

[return<i32>]
apply([Function<i32, i32> mut] f, [i32] x) {
  return(f(x))
}

[return<i32>]
double([i32] x) {
  return(x * 2i32)
}

[return<int> effects(heap_alloc)]
main() {
  [i32] base{3i32}
  [i32] added{apply([base]([i32] x) { return(x + base) }, 1i32)}  // 4
  [Hook mut] hook{Hook{[run] double}}
  [Function<i32, i32> mut] named{Function<i32, i32>{/double}}
  return(added + apply(double, 5i32) + apply(hook.run, 1i32) + named(2i32))  // 4 + 10 + 2 + 4
}
```

A `Function` owns sendable captures, a `String` and a struct holding a `vector` (runs on the VM, native and
`optexe`; capturing a `StringView` or a `Pointer` instead is rejected at the closure):

```prime
import /std/text/*
import /std/collections/*

[struct]
Tally {
  [String] label{String{}}
  [vector<i64>] values{vector<i64>()}
}

[return<i64>]
apply([Function<i64, i64> mut] step, [i64] value) {
  return(step(value))
}

[return<int> effects(heap_alloc)]
main() {
  [String] name{stringFrom("abcd")}
  [Tally mut] tally{Tally{}}
  tally.values.push(7i64)
  byName{[name]([i64] x) { return(x + convert<i64>(name.count())) }}
  [Function<i64, i64> mut] byTally{[tally]([i64] x) { return(x + convert<i64>(tally.values.count())) }}
  [Function<i64, i64> mut] twin{byTally}
  return(convert<i32>(apply(byName, 1i64) + byTally(10i64) + twin(0i64)))  // 5 + 11 + 1
}
```

Desugared closure shape (visible with `--dump-stage ast-semantic`):

```prime
[struct] /main__lambda0<C0>() { [C0, mut] calls }
[mut] /main__lambda0/call([i32] input) { assign(this.calls, plus(this.calls, 1)) return this.calls }
[return</main__lambda0<C0>>] /main__lambda0_make<C0>([C0] calls) { return /main__lambda0<C0>([calls] calls) }
// the lambda expression: /main__lambda0_make(0i32)
```
