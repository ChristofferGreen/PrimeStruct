# PrimeStruct Tuple Prototype

Status: prototype design note, not implemented yet and not part of the
canonical language spec.

This document captures the proposed `tuple` design needed by future
multi-value APIs such as multithreaded `wait(left, right)`. When the design is
stable enough, fold accepted parts into `docs/PrimeStruct.md`,
`docs/CodeExamples.md`, and the stdlib docs alongside implementation TODOs.

## Design Goals

`tuple` should be a stdlib-owned heterogeneous product type with C++-style
semantics:

- `tuple<T0, T1, ...>` stores one value of each element type
- `tuple<>` is the empty tuple and `tuple<T>` is a one-element tuple
- tuple arity is genuinely variable and not implemented as hand-written
  `tuple1`, `tuple2`, `tuple3`, ... families
- construction uses ordinary PrimeStruct brace construction
- element access is compile-time indexed
- the type owns its elements and follows normal field lifetime rules
- copying, moving, destruction, equality, and ordering are available only when
  every element type supports the corresponding operation
- no tuple-specific IR opcode, runtime object, backend special case, or
  compiler-owned tuple envelope is required
- the implementation lives in `.prime` files

The compiler may still need generic language substrate that is not
tuple-specific, such as heterogeneous type-parameter packs or compile-time
integer template arguments. That substrate should be reusable by other stdlib
types and metaprogramming features. `tuple` itself should remain ordinary
PrimeStruct source.

The existing `args<T>` variadic parameter model is homogeneous and is not
enough to express `tuple<T0, T1, ...>` by itself.

Therefore `tuple` should not be implemented until the language can express
heterogeneous type packs in `.prime`. A fixed-arity tuple family is not the
accepted implementation model.

## Required Language Substrate

PrimeStruct does not currently appear to have every generic feature needed for
this tuple implementation. The missing pieces should be implemented as generic
language substrate, not as tuple-specific compiler behavior.

Required before shipping `tuple<Ts...>`:

- heterogeneous type-parameter packs on definitions, structs, helpers, and
  return envelopes
- pack expansion in `.prime` struct fields, constructor initialization,
  lifecycle helpers, and helper bodies
- monomorphization support for an arbitrary number of type arguments bound to a
  single pack parameter
- compile-time non-type template arguments, at least integer indexes for
  `get<0>(value)` and `tuple_element<1, tuple<...>>()`
- compile-time indexed pack lookup so `get<I>` can resolve the `I`th storage
  slot and its type
- diagnostics for invalid pack use, arity mismatches, non-constant indexes, and
  out-of-range indexes

Existing `args<T>` variadics are still useful for homogeneous value lists such
as `vector<T>{...}`, but they are not the tuple substrate because every element
has the same type.

Optional but useful follow-up substrate:

- heterogeneous value-pack inference for helpers such as `make_tuple(values...)`
- tuple destructuring sugar once tuple construction and `get<I>` are stable
- type-returning compile-time helpers for a public `tuple_element`-style API

## Non-Goals For The First Slice

The first implementation should not try to solve every product-type feature.

Out of scope initially:

- anonymous compiler-native tuple literals
- implicit tuple creation from comma expressions
- implicit tuple-to-struct conversion
- named tuple fields
- reflection over arbitrary tuple values beyond the explicit tuple API
- `tie`, forwarding tuples, or borrowed view tuples
- heterogeneous value-pack inference for `make_tuple(...)`, unless the generic
  pack substrate already provides it
- detached lifetime or aliasing behavior that bypasses normal field rules
- ABI compatibility with C++ `std::tuple`

The first useful slice only needs owned value tuples, construction, indexed
access, and arbitrary arity through the generic pack substrate. Planned
multi-wait results are the first concrete consumer, not a reason to special-case
arity two.

## Public Surface

Public spelling should be lower-case `tuple<Ts...>`, matching
`array<T>`, `vector<T>`, `map<K, V>`, and `soa_vector<T>` rather than
PascalCase user-defined struct names. Internal implementation helpers may use
PascalCase names if needed, but the documented user-facing type is `tuple`.

`tuple` should live in a stdlib module, for example:

```prime
import /std/tuple/*
```

Preferred user-facing shape:

```prime
pair{tuple<i32, string>{7, "left"}}

number{get<0>(pair)}
label{get<1>(pair)}
```

Method sugar should be valid once template arguments on method calls are
supported for this helper shape:

```prime
number{pair.get<0>()}
label{pair.get<1>()}
```

For the multithreading design, multi-wait can then return a tuple:

```prime
[effects(task), int]
main() {
  left{[spawn] compute_left()}
  right{[spawn] compute_right()}

  result{wait(left, right)}

  return(get<0>(result) + get<1>(result))
}
```

Later destructuring can be layered on top:

```prime
[left_result right_result] wait(left, right)
```

That binding form should be sugar for extracting tuple elements in order. In
this context, destructuring follows the bracket-list name binding rule in
`docs/PrimeStruct.md`: known bracket entries keep their existing meaning, while
fresh identifiers introduce names because the grammar expects a binding pattern.

The tuple type does not require destructuring for the first implementation.

## Construction

`tuple` construction should follow the project-wide brace-construction rule:

```prime
value{tuple<i32, bool, string>{1, true, "ok"}}
```

The explicit type argument list gives the tuple arity and element types. The
constructor accepts exactly one value per element type.

Context-typed construction can be a later convenience:

```prime
[tuple<i32, bool>] value{1, true}
```

`tuple(...)` or `make_tuple(...)` helper forms are optional convenience APIs,
not the canonical construction model. If added, they should still be `.prime`
helpers that return `tuple<...>`.

## Representation

There should be no core tuple envelope. The public `tuple` type should be a
stdlib type implemented in `.prime`.

The representation uses heterogeneous type-parameter packs:

```prime
namespace std {
  namespace tuple {
  [public struct]
  tuple<Ts...>() {
    // Expanded by generic type-pack substrate, not by tuple-specific compiler
    // logic.
    [public Ts] item...
  }
  }
}
```

The exact pack-field syntax is intentionally not finalized here. The key rule
is that the language feature must be generic: a type pack can expand fields,
helpers, lifecycle code, and compile-time indexed access for any stdlib type
that needs it.

`tuple` should expose `get<I>` as the stable API. If current brace construction
requires field visibility, tuple storage fields may be public in the `.prime`
implementation, but direct field access should be treated as representation
detail rather than documented user style.

The accepted implementation model is a single generic variadic tuple
definition. A hand-written fixed-arity family is explicitly not the target:

```prime
// Not the target:
tuple1<T0>
tuple2<T0, T1>
tuple3<T0, T1, T2>
```

Do not ship tuple support by generating repetitive arity definitions. If the
compiler cannot yet parse, type-check, and monomorphize a true `tuple<Ts...>`
definition from `.prime` source, tuple implementation should wait behind that
generic language work.

## Element Access

The primary accessor is compile-time indexed `get<I>`:

```prime
first{get<0>(pair)}
second{get<1>(pair)}
```

Tuple bracket indexing should be accepted as sugar for the same compile-time
operation:

```prime
first{pair[0]}
second{pair[1]}
```

Unlike array/vector indexing, tuple indexes are not runtime values. `pair[i]`
must reject unless `i` is a compile-time constant accepted by the tuple-indexing
rules.

Rules:

- `I` must be a compile-time integer template argument
- indexes are zero-based to match C++ tuple and existing indexed collection
  conventions
- out-of-range indexes are compile-time diagnostics
- `get<I>(tuple<...>)` returns the `I`th element type
- `get<I>(Reference<tuple<...>>)` returns `Reference<TI>` when borrowed field
  access is available for the representation
- mutable borrowed access requires a mutable tuple binding or mutable reference

`get<I>` should be implemented against the same generic pack substrate as the
tuple storage itself. Fixed helper names such as `get0(value)` and `get1(value)`
should not be used as tuple implementation scaffolding because they recreate
the fixed-arity family problem under a different name.

## Type Metadata

`tuple` should expose compile-time metadata equivalent to the useful parts of
C++ `tuple_size` and `tuple_element`.

Target shape:

```prime
tuple_size<tuple<i32, bool>>()        // 2
tuple_element<1, tuple<i32, bool>>() // bool
```

The exact syntax for type-returning compile-time helpers is not finalized. The
semantic requirements are:

- arity is known at compile time
- each element type is known at compile time
- invalid element indexes reject during semantic validation
- downstream generic code can constrain or specialize on tuple arity and
  element type without runtime reflection

If type-returning helpers are not ready, first implementation can keep metadata
internal to the tuple helper overloads and add the public metadata API later.

## Ownership And Lifetime

`tuple` is an owned value type. Each element is a normal field.

Construction:

- initializes elements left-to-right
- rejects missing or extra elements
- accepts an element only when the supplied value can initialize that field type

Move:

- moving a tuple moves every element in order
- a tuple is movable only when every element is movable

Copy:

- copying a tuple copies every element in order
- a tuple is copyable only when every element is copyable

Destroy:

- destroying a tuple destroys every initialized element
- destruction order should match the project-wide struct destruction rule; if no
  rule is canonical yet, specify one before implementing non-trivial elements
- pack-expanded destruction should align with the reflection field metadata
  model: source-order fields as reported by `meta.field_count<T>()`,
  `meta.field_name<T>(i)`, `meta.field_type<T>(i)`, and
  `meta.field_visibility<T>(i)` should be the same order used by generated
  lifecycle helpers and tuple element destruction

No extra effect is required to construct, access, copy, move, or destroy a
tuple unless an element operation itself requires an effect. `tuple` construction
does not allocate by default.

## Mutation And Borrowing

`tuple` fields should follow normal PrimeStruct mutability rules.

```prime
[mut] pair{tuple<i32, i32>{1, 2}}
first_ref{location(pair).get<0>()}
```

The precise borrowed accessor syntax can wait for implementation, but the rules
should be:

- reading from an immutable tuple is allowed
- writing an element requires a mutable tuple binding or mutable reference
- returning a reference to an element must obey the same lifetime and aliasing
  rules as returning any other field reference
- unsafe pointer projection from tuple storage is allowed only in `[unsafe]`
  code and should not receive tuple-specific exemptions

## Operations

The minimal first API:

- `tuple<...>{...}` construction
- `get<I>(tuple)` indexed read
- borrowed `get<I>` when reference projection is available
- tuple arity/type metadata if the generic substrate supports it

Likely follow-up APIs:

- `make_tuple(values...)` once heterogeneous value-pack inference exists
- `tuple_size<T>()` or equivalent compile-time arity helper
- `tuple_element<I, T>()` or equivalent type helper
- `equal(left, right)` when all elements are comparable
- lexicographic `less_than(left, right)` when all elements are orderable
- `swap(left, right)` when all elements are swappable

Do not add tuple-specific effects. Effects come from element operations only.

## Relationship To Multithreading

The multithreading prototype wants this future shape:

```prime
wait(Task<A>, Task<B>) -> tuple<A, B>
```

That should be a normal stdlib tuple return. The task system should not invent a
separate product type.

For the first task implementation, only single-task `wait(Task<T>) -> T` is
needed. When tuple support exists, multi-wait can return `tuple<...>` and
destructuring can be added as sugar later.

## Diagnostics

Expected diagnostic shapes:

```text
tuple expects 2 values but received 3
tuple element 2 is out of range for tuple<i32, bool>
get index must be a compile-time integer
tuple element 1 cannot initialize bool from string
tuple copy requires element 0 to be copyable
```

Diagnostics should point at the tuple construction, `get<I>` index, or element
expression that caused the failure.

## First Implementation Checklist

When this prototype graduates into active implementation work:

- complete the generic type-pack substrate tracked by TODO-4268 through
  TODO-4271, including the split field/helper expansion work in TODO-4275 and
  TODO-4276
- add `stdlib/std/tuple/tuple.prime` and a public import surface in TODO-4272
- implement `tuple<Ts...>` as one generic `.prime` definition, not a generated
  or hand-written fixed-arity family
- add construction and indexed access tests, including negative diagnostics for
  arity mismatch, bad element type, non-constant index, and out-of-range index
- add copy/move/destruction tests with non-trivial element types once lifecycle
  behavior is settled
- add tuple bracket indexing, heterogeneous value-pack inference, tuple
  destructuring, and multi-wait integration only as later follow-ups, tracked
  by TODO-4274, TODO-4273, TODO-4277, and TODO-4278
- update `docs/PrimeStruct.md` and `docs/CodeExamples.md` only after the
  supported surface is implemented
