# Safe Array Extents And Views

Status: design note, not yet implemented.

This note records the current PrimeStruct design direction after reviewing John
Nagle's "Safe arrays and pointers for C through compatible additions to the
language" discussion draft:
https://www.animats.com/papers/languages/safearraysforc43.pdf

The note is intentionally non-normative. It does not update the implemented
language contract and does not add a TODO entry by itself. If this direction is
prioritized, split it into explicit `docs/todo.md` leaves before implementation.

## Core Takeaway

The useful idea to carry into PrimeStruct is not C-compatible syntax. It is that
array extent should be a first-class semantic fact:

- a place or view can have a known element count
- callers can prove or check extent relationships at boundaries
- successful checks publish facts that the body and lowerer can consume
- indexing, slicing, copying, pointer conversion, and FFI can all use the same
  facts

In PrimeStruct terms, the paper's `lengthof` maps to `count(...)`, and unsafe
escape hatches such as `force_cast` map to explicit unsafe authority, qualified
unsafe intrinsics, or `at_unsafe`-style APIs.

PrimeStruct should not adopt the paper's strict/non-strict split as a language
mode. That split exists for C source compatibility. PrimeStruct should make safe
array and view behavior the default and put compatibility escapes at explicit
unsafe or FFI boundaries.

## Requirements And Contracts

We agreed on a phase distinction that should be considered for future syntax:

```prime
require<...>   // compile-time requirement, no runtime fallback
require(...)   // contract: prove statically if possible, otherwise check
```

This is a design direction, not current implemented syntax. Current docs use
`require(...)` for compile-time requirements. If this direction is adopted, the
requirement syntax section needs a deliberate migration.

`require<...>` is the forced compile-time form. It is evaluated during semantic
validation from compile-time facts, can participate in specialization and
overload viability, and emits no runtime code. If the predicate is not knowable
at compile time, it is a compile-time diagnostic or a non-viable constrained
candidate.

`require(...)` is a contract over values. The compiler should first try to
discharge the contract statically from semantic facts. If it cannot prove the
contract but the expression is pure and runtime-checkable, it emits a runtime
precondition check. If the expression is neither provable nor runtime-checkable,
it is rejected. On success, whether by proof or by emitted check, the contract
becomes an assumed fact in the body.

This makes `require(...)` suitable for array extents and slices because common
identity cases can compile to no check, while dynamic cases still stay safe.

Example sketch:

```prime
[return<void> require(count(dst) == count(src))]
copy<T>([Reference<array<T>> mut] dst, [Reference<array<T>>] src) {
  [int] n{count(dst)}
  for([int mut] i{0}; i < n; ++i) {
    dst[i] = src[i]
  }
}
```

The contract lets lowering emit at most one boundary check for
`count(dst) == count(src)`, and no check when the equality is already proven.
Inside the function, that fact can support bounds check elimination for `src[i]`
when `i` is already proven in range for `dst`.

Contract-form `require(...)` should stay restricted:

- pure only
- no allocation, mutation, IO, task spawning, or backend-only effects
- may read parameters, constants, `count(...)`, and other safe value facts
- deterministic failure behavior
- facts are published into the semantic product for downstream consumers

## Extent Facts

Extent facts should be semantic-product facts, not ad hoc lowerer
reconstruction. A minimal fact can describe:

```text
place: values
element_type: T
extent: count(values)
provenance: owner or source expression
```

For a contract-form `require(...)`, the semantic product should also be able to
expose the proven or checked relationship:

```text
count(dst) == count(src)
```

Consumers include:

- call-boundary array size checks
- index and loop bounds checks
- check hoisting and identity elimination
- slices and borrowed views
- pointer-to-reference conversion
- checked pointer arithmetic provenance
- safe copy and initialization helpers
- raw byte buffers and FFI adapters
- string literal and fixed-buffer APIs

## Pointer Optionality And Raw Addresses

Safe PrimeStruct should not use null or invalid pointer values as ordinary
control-flow results. If an operation might fail to produce a valid pointer, its
type should say so:

```prime
Maybe<Pointer<T>>
Result<Pointer<T>, AllocError>
```

In this model, `Pointer<T>` is a valid, non-null pointer value in safe code.
A caller must unwrap `Maybe<Pointer<T>>` or handle `Result<Pointer<T>, E>` before
using the pointer.

This changes the safe-array mapping from C. The paper needs pointer-to-reference
null checks because C pointers can be null. PrimeStruct can avoid null checks for
ordinary safe view construction from `Pointer<T>`. The remaining checks are about
provenance, lifetime, capability authority, and extent.

Raw or foreign addresses are still useful at unsafe/FFI boundaries, but they
should not be the type returned by a safe function that might fail. A wrapper
around an untrusted foreign address should validate it at the boundary and
return `Maybe<Pointer<T>>` or `Result<Pointer<T>, E>`. If a raw-address type such
as `RawPointer<T>` remains in the language, it should be treated as unsafe
adapter material, not as the normal nullable-pointer surface.

## Unified Views

Slices are the most important application of extent facts. A slice should not be
treated as "just another array". It should be a borrowed view into contiguous
storage.

`Reference<T, Capability>` and `Slice<T, Capability>` should share one
underlying view model. A reference is the single-element case of a slice:

```text
Reference<T, Capability> == Slice<T, Capability> where count == 1
```

Conceptually, the core view is:

```text
View<T, Capability> {
  pointer: Pointer<T>
  count: element count
  provenance: semantic/debug owner identity
}
```

`Pointer<T>` names valid storage identity. `View<T, Capability>` names borrowed
authority over a contiguous range of that storage. The capability parameter is
compile-time authority metadata unless a specific capability requires runtime
state. It can encode read/write authority, escape permission, pinning, thread
sharing, FFI exposure, GPU visibility, or other project-defined permissions.

Possible public surfaces:

```prime
Reference<T, Capability> // view with count == 1
Slice<T, Capability>     // view with runtime count

slice(values, start, end)          // checked read view
slice_mut(values, start, end)      // checked write view
slice_unsafe(values, start, count) // unsafe/internal, no bounds proof
```

Core facts for `slice(values, start, end)`:

```text
0 <= start
start <= end
end <= count(values)
count(result) == end - start
result is a View<T, Capability>
result borrows values[start, end)
```

If the range is statically invalid, semantics can report a diagnostic. If the
range depends on runtime values, lowering emits a single construction-time
check. After construction, indexing into the slice uses `count(result)` rather
than rechecking the original owner expression.

## View Representation

For PrimeStruct public semantics, choose a real runtime value:

```text
View<T, Capability> {
  pointer
  count
}
```

For `Reference<T, Capability>`, `count` is statically one and can usually be
omitted from the runtime representation. For `Slice<T, Capability>`, `count` is
normally a runtime value. Provenance can remain semantic/debug metadata rather
than a user-visible field. The optimizer may scalar-replace a view into base
pointer plus count in tight loops. This differs from the paper's "no fat
pointers" goal because PrimeStruct does not need C ABI compatibility for native
language features.

FFI wrappers can still expose thin-pointer forms when required, but that should
be an adapter concern rather than the core view model.

## Cursors And Pointer Arithmetic

Safe PrimeStruct should prefer cursor traversal over arbitrary arithmetic on
`Pointer<T>`. A cursor is the public traversal abstraction for both C-style
contiguous pointer walks and C++-style container iterators.

Conceptually:

```text
Cursor<T, Capability> {
  provenance: source view or container generation
  position: traversal position
  category: forward | bidirectional | random_access | contiguous
}
```

For contiguous storage, cursor movement is safe pointer arithmetic with a
provenance root and a known traversal boundary. For non-contiguous containers,
cursor movement is iterator traversal without exposing an address.

Use `limit(values)` for the one-past-final traversal boundary:

```prime
[return<int>]
sum_vector([Reference<vector<int>, Read>] values) {
  [Cursor<int, Read> mut] it{start(values)}
  [Cursor<int, Read>] lim{limit(values)}
  [int mut] total{0}

  while(it != lim) {
    total = total + read(it)
    it = advance(it)
  }

  return(total)
}
```

`limit(values)` is not the last element. It is the exclusive upper traversal
boundary, matching the invariant:

```text
start(values) <= cursor < limit(values)
```

Reverse traversal uses the symmetric exclusive boundary before the first
element:

```text
reverse_start(values) // last position, or reverse_limit(values) when empty
reverse_limit(values) // one-before-first sentinel, never readable
```

Forward and reverse traversal should therefore use the same half-open shape:

```text
start(values) <= forward_cursor < limit(values)
reverse_limit(values) < reverse_cursor <= reverse_start(values)
```

Reserve `first(values)` and `last(values)` for element-oriented helpers. Since
empty collections have no first or last element, those helpers should return
`Maybe<Cursor<T, Capability>>`.

Initial cursor rules:

- `read(cursor)` requires the cursor to be within the readable range, not equal
  to `limit(...)`
- `write(cursor, value)` requires write authority and an in-range cursor
- `advance(cursor)` and `advance(cursor, n)` must prove or check that the result
  stays within the cursor's valid range
- cursor equality, ordering, and distance require compatible provenance
- random-access movement requires a random-access or contiguous cursor category
- exposing a raw address from a cursor requires explicit address/FFI capability
- structural mutation of the owner invalidates live cursors unless the container
  explicitly documents stronger stability

Raw `Pointer<T> + offset` should stay unsafe or internal unless paired with an
explicit extent and provenance proof. Existing checked memory intrinsics remain
useful implementation substrate for slices, cursors, vectors, raw buffers, and
FFI adapters, but should not be the primary user-facing pointer arithmetic
surface.

## Borrow Checking For Views

The main risk is borrow-checker expansion. The first implementation should be
conservative and lexical.

Separate extent facts from borrow facts:

```text
count(window) == end - start     // extent fact
window borrows values[start,end) // borrow/provenance fact
```

Do not make extent precision imply arbitrary alias precision. A first view loan
model can stay small:

```text
Loan {
  ownerPlace
  range: all | [start, end)
  mode: read | write
  structuralGeneration
  lifetimeRegion
}
```

Initial conflict rules:

- read loan plus read loan is allowed
- write loan conflicts with any overlapping live loan
- structural mutation of an owner conflicts with any live loan into that owner
- move or destruction of an owner conflicts with any live loan
- escaping a slice beyond the owner's lifetime is rejected

For fixed `array<T>`, storage identity is stable. For `vector<T>`, `soa<T>`,
maps, strings, and raw buffers, structural operations such as growth, clear,
remove, realloc, and destruction must invalidate views. The first rule should be
simple: reject structural mutation while any view into that owner is live.

## Disjoint Mutable Slices

Avoid general range theorem proving in the first borrow checker. When the
compiler cannot prove two mutable ranges are disjoint, reject the code or
require a blessed helper that performs the check and publishes the fact.

Example sketch:

```prime
[tuple<Slice<T mut>, Slice<T mut>>]
split_at_mut<T>([Reference<array<T>> mut] values, [int] mid) {
  // emits or proves 0 <= mid <= count(values)
  // publishes non-overlap:
  //   left borrows values[0, mid)
  //   right borrows values[mid, count(values))
}
```

This keeps the core checker predictable. Precision is introduced through
library helpers with explicit facts rather than through an implicit solver.

## Escape Rules

Slices are non-owning. Returning or storing a slice should be rejected until the
language has explicit escape/lifetime authority for views.

Invalid sketch:

```prime
[return<Slice<int, Read>>]
bad() {
  [array<int>] values{array<int>{1, 2, 3}}
  return(slice(values, 0, 2)) // invalid: view escapes owner
}
```

Valid sketch:

```prime
[return<int> require(start <= end, end <= count(values))]
sum_window([Reference<array<int>>] values, [int] start, [int] end) {
  [Slice<int, Read>] window{slice(values, start, end)}
  [int mut] total{0}

  for([int mut] i{0}; i < count(window); ++i) {
    total = total + window[i]
  }

  return(total)
}
```

Future escape rules should integrate with the memory-capability model instead
of adding a separate lifetime system just for slices.

## Raw Storage And Interop

The paper's `void_space` addresses C's lack of an array-of-void type. PrimeStruct
does not need that exact type. Prefer one of:

- `array<byte>` or `array<u8>` for initialized byte storage
- `RawBuffer` for owned byte buffers with explicit allocator behavior
- `Uninit<T>` or another typed uninitialized-storage surface for allocation and
  placement initialization

C-style APIs such as `read(fd, ptr, n)` should be wrapped by safe PrimeStruct
adapters that require or prove `n == count(buffer)`. Unsized C pointers remain
behind FFI and unsafe boundaries.

## Open Questions

- Exact failure behavior for runtime checks emitted by contract-form
  `require(...)`.
- Whether `require<...>` should replace current compile-time `require(...)` or
  whether both forms need a migration window.
- Whether dependent fixed extents need syntax such as `array<T, N>`, even though
  current docs intentionally avoid that spelling.
- Exact standard capability names and whether mutable views spell write
  authority as `Slice<T, Write>`, `Slice<T, ReadWrite>`, or another capability.
- How string literal extent should count terminators, UTF-8 bytes, and future
  string storage forms.
- Which semantic-product fact families should carry extent, view, and runtime
  contract facts.
- How much non-lexical lifetime precision should be added after the conservative
  first slice checker.
