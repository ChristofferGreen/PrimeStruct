# Capability-Checked Memory Safety

Status: design direction, not an implemented language contract.

This note sketches a memory-safety model for PrimeStruct that aims for
Rust-class guarantees without copying Rust's ownership model as the primary
user-facing concept. The central idea is that memory safety should be one part
of the same capability and effect system used for other forms of authority.

PrimeStruct should stay simple to reason about at the core, while allowing
deeper control to be opted into when a program needs it. A user should be able
to write ordinary value code without thinking about regions, raw addresses, or
advanced effect signatures. The deeper model should become visible only when a
public API, allocator boundary, unsafe interop boundary, concurrency boundary,
or nontrivial lifetime relationship needs to state its authority precisely.

All PrimeStruct-shaped source examples in this note use the readable surface
style from `docs/CodeExamples.md`: return/effect metadata in `[...]`, brace
local bindings, `return(...)`, method-style calls, `Reference<T>`, and
`location(value)`. Capability requirements are shown as proposed metadata
transforms such as `needs<read(value)>` or `needs<write(this)>`. Those
`needs<...>` transforms are design notation, not a currently implemented
language feature.

## Design Goal

The goal is safe code with these properties:

- no use after move or destroy
- no dangling references
- no mutation through shared read aliases
- no data races in safe concurrent code
- no hidden writes through APIs that only inherited read authority
- explicit unsafe boundaries for raw address exposure and unchecked aliasing

The core concept is:

```text
An execution may only perform an operation when it holds the required
capability for the target place, and the current temporal state of that place
allows the operation.
```

This separates two questions that are often blended together:

```text
authority:   is this code allowed to do the operation at all?
temporality: is the operation safe right now, given moves and active loans?
```

Rust starts with ownership and borrowing. PrimeStruct can start with inherited
capabilities. Ownership, references, and borrowing then become ordinary ways of
granting, narrowing, lending, or consuming capabilities over places.

## Places

A place is any storage location or abstract region that can be named by the
checker:

- a local binding
- a field of a struct
- an array, vector, or map element when the element can be tracked precisely
- an allocation or allocation region
- a global or imported resource
- a device, file, handle, or foreign memory object

The checker does not need every place to have the same precision. Simple locals
can be tracked directly. Aggregate fields can be split when that is useful.
Collection elements can start coarse and become more precise only when the
language grows stable indexing and iterator authority rules.

The important rule is that capabilities are always capabilities over some
place, even when the surface syntax omits that detail.

```text
read(buffer)
write(buffer)
own(image)
escape(region)
addr(rawMemory)
```

## Core Capabilities

The initial capability set should stay small. More specific capabilities can be
introduced later as aliases, refinements, or library-level effects.

```text
read(P)
  Observe the value stored at place P.

write(P)
  Mutate the value stored at place P.

own(P)
  Move, destroy, reallocate, or otherwise consume the identity of P.

escape(P)
  Store or return a reference/capability so it may outlive the current scope.

addr(P)
  Expose or depend on the raw address/identity of P.
```

The capabilities form a small authority lattice:

```text
own(P)   implies write(P) and read(P)
write(P) implies read(P)
read(P)  is shareable
```

`escape(P)` and `addr(P)` are deliberately separate from `read`, `write`, and
`own`. A function may be allowed to mutate a buffer without being allowed to
store a reference to it. A function may be allowed to read a value without being
allowed to reveal its address or create a raw pointer to it.

This distinction is the key difference between a capability-first design and a
model where reference syntax alone carries most of the meaning.

## Temporal State

Capabilities answer what the code is allowed to do. Temporal state answers
whether the requested action is safe at the current program point.

Each tracked place can be modeled by a small state machine:

```text
available
moved
shared_read(n)
exclusive_write
destroyed
```

The basic transition rules are intentionally close to familiar ownership
systems:

- Reading requires `read(P)` and is allowed when P is available or already
  shared for reading.
- Creating a shared read loan requires `read(P)` and increments the shared
  read count for the loan's scope.
- Writing requires `write(P)` and is rejected while shared read loans or an
  exclusive write loan are active.
- Creating an exclusive write loan requires `write(P)` and excludes all other
  reads and writes for the loan's scope.
- Moving requires `own(P)` and changes P to `moved`.
- Destroying requires `own(P)` and changes P to `destroyed`.
- Using a moved or destroyed place is rejected unless it is reinitialized by a
  rule that explicitly permits reinitialization.

The temporal checker can therefore be small. It does not need to be the public
identity of the language. It is the checker that enforces exclusivity and
single-consumption beneath the broader authority model.

## Reference Values

References are capability-carrying views of places. They are not the source of
authority by themselves; they are values that carry limited inherited authority.

A read reference carries read authority:

```text
Reference<T> carries read(P)
```

A write reference carries write authority:

```text
[Reference<T> mut] carries write(P)
```

An owning value carries ownership authority:

```text
T           carries own(P)
```

The reference spelling follows the current PrimeStruct examples. The exact
capability inference attached to those forms is proposed. The important
semantic distinction is:

```text
read reference:  shared, cannot mutate
write reference: exclusive while active, can mutate
owning value:    may move/destroy and can lend read or write authority
```

Writing through a pointer or reference is then ordinary capability checking:

```text
pointer points to place P
current execution holds write(P)
no active conflicting loan prevents the write
```

There is no special hidden rule for "pointer writes". It is just a write effect
on the pointee place.

## Effects As Inherited Authority

PrimeStruct already wants effects to describe what an execution may do. Memory
capabilities should use the same channel.

Conceptually:

```prime
[struct]
Counter {
  [int mut] value{0}

  [return<void> needs<write(this)>]
  increment() {
    this.value = this.value + 1
  }
}

[return<void> needs<write(counter)>]
increment_counter([Reference<Counter> mut] counter) {
  counter.increment()
}
```

The caller must hold `write(counter)`. The callee inherits exactly the authority
declared by the call boundary. It cannot retain, widen, or smuggle out more
authority unless its signature says so.

This lets memory checking compose with other authority:

```prime
[return<Image> effects(file_read) needs<read(path), write(allocator)>]
load_image([Path] path, [Reference<Allocator> mut] allocator) {
  // body elided
}

[return<void> effects(gpu_queue) needs<write(texture), read(image)>]
upload([Reference<Texture> mut] texture, [Reference<Image>] image) {
  // body elided
}
```

Memory authority, IO authority, GPU authority, allocator authority, and unsafe
interop authority can all be checked through the same conceptual model.

## Public API Shape

The default user experience should be mostly inferred.

Simple value code should not need explicit capability annotations:

```prime
[int]
main() {
  value{1}
  next{add_one(value)}

  return(next)
}
```

Public APIs should show authority only where it matters:

```prime
[struct]
Counter {
  [int mut] value{0}

  [int needs<read(this)>]
  current() {
    return(this.value)
  }

  [return<void> needs<write(this)>]
  increment() {
    this.value = this.value + 1
  }
}

[int needs<read(counter)>]
read_counter([Reference<Counter>] counter) {
  return(counter.current())
}

[return<void> needs<write(counter)>]
bump_counter([Reference<Counter> mut] counter) {
  counter.increment()
}
```

The exact syntax should be decided later. The design constraint is that the
surface form should make inherited authority visible without forcing every
local operation to be annotated.

## Capability Weakening

Stronger capabilities can be weakened into weaker capabilities:

```text
own(P)   -> write(P)
write(P) -> read(P)
```

This supports common call patterns:

```text
caller owns buffer
caller lends write(buffer) to fill(buffer)
caller later lends read(buffer) to sum(buffer)
caller finally destroys buffer
```

Weakening is one-way. A callee that receives `read(P)` cannot manufacture
`write(P)` or `own(P)`.

## Duplication Rules

Capability duplication follows the safety properties of the authority:

```text
read(P)   may be copied freely
write(P)  may be lent temporarily but not duplicated as two active writers
own(P)    is affine or linear and may not be copied
escape(P) may be copied only if the referenced lifetime/region permits it
addr(P)   is unsafe-sensitive and should not be implicit
```

This gives the language a compact explanation for aliasing:

```text
Many readers are fine.
One writer is fine.
Readers and a writer at the same time are not fine unless an explicit safe
interior-mutability abstraction owns the rule.
```

## Escape Control

Escape is the rule that prevents dangling references.

A function may read or write a place without being allowed to let a reference to
that place escape. Returning, storing, spawning, or otherwise extending a
reference beyond the current scope requires explicit escape authority for the
target region.

Conceptually:

```prime
[return<Reference<int>> needs<read(values), escape(values)>]
first([Reference<vector<int>>] values) {
  return(values.at_ref(0))
}
```

Without `escape(values)`, `first` may inspect the vector but may not return a
reference into it.

This keeps lifetime-like reasoning local:

```text
read/write authority says what can be done now.
escape authority says what can outlive now.
```

Regions can be introduced as the advanced form of escape authority:

```prime
[return<Reference<T>> needs<write(arena), escape(arena)>]
allocate<T>([Reference<Region> mut] arena) {
  // body elided
}
```

The beginner model does not need explicit regions. Region names become useful
when APIs return views, store references, or manage arenas/device memory.

## Raw Address Authority

Raw address exposure should be a separate capability. A function that can read
or write a value should not automatically be able to reveal its address, depend
on stable identity, or pass raw pointers to foreign code.

Conceptually:

```prime
[return<RawPointer<int>> unsafe needs<addr(buffer), read(buffer)>]
as_raw([Reference<Buffer>] buffer) {
  // body elided
}
```

`addr(P)` should be rare in safe code. It marks an API that depends on physical
or foreign identity rather than only on PrimeStruct value semantics.

This gives unsafe interop a clear boundary:

- safe code can read and write through checked capabilities
- raw address code requires explicit `addr`
- unchecked aliasing or lifetime promises require `unsafe`

## Concurrency

Data-race freedom should fall out of the same model.

Sending a read-only shared value to multiple workers requires only shareable
read authority:

```prime
[return<void> effects(thread_spawn) needs<read(data)>]
spawn_readers([Reference<Data>] data) {
  // body elided
}
```

Sending write authority to a worker transfers or lends exclusive authority:

```prime
[return<void> effects(thread_spawn) needs<write(buffer)>]
spawn_writer([Reference<Buffer> mut] buffer) {
  // body elided
}
```

The caller cannot keep using `write(buffer)` while the worker owns or holds the
exclusive loan. A join, scope exit, or synchronization primitive can return the
authority.

Interior mutability and shared concurrency abstractions should be library types
that own their synchronization rule:

```text
Mutex<T>
Atomic<T>
Channel<T>
```

Those types may expose safe APIs that convert shared read authority over the
container into scoped write authority over protected contents, but only because
the type's implementation owns the proof.

## Relationship To Rust

The intended capability set is similar in power to Rust's safe subset:

- moved values cannot be reused
- mutable access is exclusive
- shared access is read-only
- references cannot outlive their backing storage
- unsafe/raw pointer behavior is explicit
- safe concurrent mutation requires a synchronization abstraction

The difference is presentation and composition.

Rust makes ownership and borrowing the central surface model. PrimeStruct can
make inherited authority the central surface model:

```text
Rust:
  who owns this value, and what borrow is active?

PrimeStruct:
  what authority did this execution inherit, and is the requested use
  temporally valid now?
```

The distinction matters because PrimeStruct wants effects and capabilities to
cover more than memory. File access, GPU queues, allocators, raw addresses,
foreign handles, and memory mutation can all use one conceptual mechanism.

## Simplicity Layers

The model should be exposed in layers:

```text
Level 0: ordinary value code
  Locals, values, read refs, write refs, and moves are inferred.

Level 1: public memory authority
  API signatures expose read/write/own where callers need to understand the
  contract.

Level 2: escape and region authority
  APIs that return views, store references, allocate from arenas, or cross async
  and thread boundaries state their region relationships.

Level 3: raw address and unsafe authority
  FFI, raw pointers, custom allocators, and unchecked aliasing promises require
  explicit authority and unsafe boundaries.
```

Each level should be usable without forcing users to manually reason about all
lower levels in ordinary code.

## Checker Sketch

A minimal checker can be built from three maps:

```text
place -> temporal state
value -> carried capabilities
execution -> required/inherited capabilities
```

For each operation:

1. Resolve the target place.
2. Determine the required capability.
3. Check that the current execution or value carries that capability.
4. Check the place's temporal state.
5. Apply the state transition for the operation.
6. Reject any escape that lacks escape authority or a valid region relation.

For calls:

1. Resolve the callee's required capabilities.
2. Prove each requirement from the caller's current capabilities.
3. Create scoped loans or transfers for the call.
4. Restore lent capabilities after the call if the callee did not consume or
   escape them.

For returns and stores:

1. Check that any returned/stored reference has escape authority.
2. Check that the target region outlives the destination.
3. Reject hidden transfer of stronger authority than the signature permits.

This is enough to model a first useful version without requiring a full
borrow-checker clone in the public language design.

## Diagnostics

Diagnostics should explain missing authority or conflicting temporal state in
plain terms.

Examples:

```text
cannot write to buffer
  fill(buffer) requires write(buffer)
  current execution only has read(buffer)
```

```text
cannot mutate value while read view is active
  read view was created here
  write attempted here
  read view ends here
```

```text
cannot return reference into local value
  returned reference needs escape(local)
  local value is destroyed at function exit
```

The diagnostic vocabulary should teach the model:

- required capability
- available capability
- active loan
- escaped reference
- consumed value

## Open Design Questions

Several details should remain open until implementation pressure makes them
concrete:

- final surface syntax for capability requirements
- whether `read`, `write`, and `own` are spelled directly or hidden behind
  reference/value types in most signatures
- how precisely aggregate fields and collection elements are tracked
- whether ownership is affine by default or requires explicit destruction for
  some resource types
- how region names are introduced in source syntax
- how async and generator-like executions return or suspend capabilities
- how much capability information appears in IR versus semantic-product facts
- how unsafe library types state and test their internal authority proofs

The north star is not to maximize annotation power. The north star is to keep
ordinary code obvious while making advanced authority boundaries explicit,
checkable, and composable.

## Summary

PrimeStruct should aim for capability-checked memory safety:

```text
Ownership grants authority.
References lend limited authority.
Effects describe inherited authority.
A small temporal checker prevents unsafe simultaneous use.
Escape authority controls lifetimes.
Raw address authority marks unsafe identity exposure.
```

This gives the language a path toward Rust-class safety while preserving a
different design center: simple core building blocks, inherited capabilities,
and opt-in depth where the program actually needs it.
