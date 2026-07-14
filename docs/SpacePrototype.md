# PrimeStruct Space Prototype

Status: early prototype design note, not yet part of the canonical language
spec. Depends on substrate tracked in `docs/OverloadResolutionPrototype.md`
and, for its full blocking semantics, on the scheduler work `docs/
MultithreadingPrototype.md` already lists as remaining implementation work.
When the design here is stable enough, fold the accepted parts into
`docs/PrimeStruct.md` alongside implementation TODOs.

## Motivating Example

```prime
[Space<i32>] space{Space<i32>{}}
space.insert("/sys/data", 5)
value{space.take("/sys/data")}
```

`Space` is a hierarchical, path-keyed coordination structure in the spirit
of Linda tuple spaces: `insert` publishes a value under a path, `take`
consumes (removes and returns) the value at a path, blocking until one is
present. Paths are hierarchical strings such as `/sys/data`, matching the
project's existing slash-path convention for imports and namespaces (this
is a runtime string convention, not a new language-level path type — see
"Path Model And Hashing" below).

A `Space` can also delegate part of its hierarchy to user-supplied code: a
"mount point" is a subtree whose `insert`/`take`/`read` behavior is
implemented by an ordinary PrimeStruct type instead of `Space`'s own
default storage. That is the feature this document spends the most time
scoping, because it is also the main performance risk (see "Routing And
Mount Points").

## Design Goals

- hierarchical, slash-delimited path keys, matching the project's existing
  import/namespace path convention in spelling only (paths are ordinary
  runtime strings, not compile-time import paths)
- `insert`, `take` (blocking, destructive), and `read` (blocking,
  non-destructive) as the core Linda-style operations
- a path can be mounted with user-defined handler code that owns
  everything at and below that path; unmounted paths use `Space`'s own
  default storage
- literal path arguments should cost nothing at runtime beyond an integer
  comparison per segment — no per-call string hashing or parsing when the
  path is known at compile time
- shared mutable state stays explicit and effect-scoped, consistent with
  the rest of the language's concurrency story; `Space` does not exempt
  itself from "shared mutable state is rejected by default unless wrapped
  in safe primitives"

## Non-Goals For The First Slice

- wildcard or pattern-matching `take`/`read` across sibling paths (classic
  Linda tuple-shape matching). First slice is exact-path match only.
- heterogeneous value types within one `Space` instance. `Space<T>` is
  homogeneous per instance, matching `array<T>`/`vector<T>`/`map<K, V>`
  conventions rather than inventing a tagged/`Any`-like value.
- heterogeneous mount-point handler types within one `Space` instance (see
  "Routing And Mount Points" — this needs dynamic-dispatch substrate the
  language does not have yet, and is explicitly deferred)
- real concurrent blocking `take`/`read` in the first runtime slice. The
  current VM/native runtime "deliberately does not... schedule multiple
  tasks concurrently" (`docs/MultithreadingPrototype.md`); true blocking
  wake-on-insert needs that scheduler work to exist first. See "Concurrency
  Model."
- distributed or cross-process spaces
- transactional multi-path operations (Linda's `eval`, batch withdrawal,
  etc.)

## Public Surface

`Space<T>` uses `PascalCase`, not the lower-case `tuple<T>`/`vector<T>`
convention. Reasoning: `tuple`/`vector`/`map`/`array` are plain value
containers; `Space` is a coordination/resource primitive with effects and
blocking behavior, the same category as `Task<T>`, `Atomic<T>`, and
`Mutex<T>` in `docs/MultithreadingPrototype.md`'s "High-Level And
Low-Level Layers" — those are all `PascalCase`. `Space` follows that
family.

```prime
import /std/space/*

[Space<i32>] space{Space<i32>{}}
```

## Core Operations

The blocking forms from the motivating example are the target shape, but
they depend on scheduler work that does not exist yet (see "Concurrency
Model"). The first buildable slice ships non-blocking forms and treats
blocking `take`/`read` as the follow-up once that scheduler substrate
lands:

```prime
[effects(space)]
insert([string] path, [T] value) { ... }

[effects(space) Result<T, SpaceError>]
try_take([string] path) { ... }

[effects(space) Result<T, SpaceError>]
try_read([string] path) { ... }
```

`try_take`/`try_read` return immediately: `Result.err(SpaceError.not_found)`
if nothing is at that path, `Result.ok(value)` otherwise. This reuses the
existing `Result<T, Error>` + `?` + `on_error` convention rather than
introducing an `Option<T>` type the stdlib does not currently have —
consistent with the project's general preference for not adding new
generic substrate before it is required.

```prime
[struct]
SpaceError {
  [string] path
}
```

The exact `SpaceError` shape (single "not found" case vs. richer variants
for mount conflicts, etc.) is not finalized; keep it a plain struct for the
first slice rather than a `[sum]` with multiple payload cases until there
is a second real error condition to distinguish.

Follow-up, once scheduler substrate exists:

```prime
[effects(space, task) T]
take([string] path) { ... }   // blocks until a value is present, then removes it

[effects(space, task) T]
read([string] path) { ... }   // blocks until a value is present, does not remove it
```

## Path Model And Hashing

A path is an ordinary runtime `string` split on `/`. There is no new
language-level path type — this is deliberate, mirroring
`docs/TuplePrototype.md`'s bias toward not inventing compiler-native
machinery where ordinary stdlib types suffice.

Internally, `Space` needs a hashable, cheaply comparable key per path
segment for the routing trie described below. `PathKey` is the stdlib type
that carries this:

```prime
[struct]
PathKey {
  [array<u64>] segment_hashes
  [array<string>] segments
}

[pure] hash_path([string] path) -> PathKey { ... }
```

`hash_path` is an ordinary pure stdlib function, not a compiler intrinsic.
When called with a compile-time-constant `path` argument, the existing
"restricted compile-time callable" evaluator (`docs/PrimeStruct.md`'s
compile-time evaluation machinery, `CompileTimeCallable.cpp`) folds the
call at compile time, the same way any other pure function with constant
arguments would fold. When `path` is a runtime-computed string, it just
runs normally. This was chosen over a compile-time-only intrinsic
(`hash_path<"...">()`) specifically so a `Space` path built at runtime
(`space.insert(base + "/data", 5)`) uses the identical algorithm as a
literal path — one hashing behavior, not two.

`segments` is kept alongside `segment_hashes` deliberately: a hash is a
fast pre-filter for routing, not a replacement for equality. Two different
segments can collide on `u64` hash; the trie falls back to string
comparison only among children that share a hash at a given node, so
collision cost is paid locally, not across the whole path. Whole-program
perfect-hash generation over every literal `Space` path the compiler sees
(collision-free by construction, and would let `segments` be dropped) is a
plausible future optimization, not a first-slice requirement.

## Insert/Take Call Surface

The natural API has two entry points per operation — one taking a `string`
path (the common case, foldable to zero runtime hashing when literal) and
one taking an already-hashed `PathKey` (for callers holding a cached route,
e.g. inside a hot loop):

```prime
[T] insert([string] path, [T] value) {
  return(insert_by_key(hash_path(path), value))
}

[T] insert_by_key([PathKey] key, [T] value) {
  // real storage/routing logic
}
```

These are now a true overload set: type-based same-arity overload
resolution (Phase 1 of `docs/OverloadResolutionPrototype.md`) is
implemented, so `insert([string] ...)` and `insert([PathKey] ...)` coexist
at one path and call sites select by argument type — the exact
string-vs-`PathKey` pattern is pinned by the "same-arity helper overloads
resolve by parameter type" semantics test. The earlier two-name fallback
(`insert` calling `insert_by_key`) is no longer required, though note the
first-slice limit that a nested-call argument (e.g.
`insert(hash_path(p), v)`) has best-effort argument-type facts; when the
callee's type cannot be inferred, the call diagnoses as ambiguous rather
than selecting, so `Space`'s implementation should keep explicit typed
locals in its own internals where that matters.

## Routing And Mount Points

Unmounted paths use `Space`'s own default storage: conceptually a trie
keyed by `PathKey` segments, where each node either holds a bucket of
values (a plain path) or delegates to a mount.

A mount point hands an entire subtree to user code:

```prime
[trait]
SpaceHandler<T> {
  [T] insert([string] path_tail, [T] value)
  [Result<T, SpaceError>] try_take([string] path_tail)
  [Result<T, SpaceError>] try_read([string] path_tail)
}
```

The handler receives only the remaining path tail below the mount point,
not the full path — a handler mounted at `/sys` and asked for
`/sys/data/cache` sees `data/cache`, and has no visibility into paths
outside its own subtree.

**This needs dynamic dispatch, and the language does not have that
substrate today.** PrimeStruct traits are structural and satisfied by
monomorphization (`docs/PrimeStruct.md:4473-4475`: "a type satisfies a
trait if all required functions resolve... no inference of trait bounds")
— there is no runtime trait-object / type-erasure mechanism (confirmed:
no existing `dyn`-style construct, boxed trait value, or vtable-carrying
existential type anywhere in the current spec). A `Space` that wants
*different* mount points backed by *different* concrete `SpaceHandler`
implementations, chosen at runtime by which subtree a call routes into,
needs exactly that kind of type erasure.

Two ways to scope around this for a first slice, and the choice matters a
lot for what ships first:

1. **`Space<T, H>` — one statically-known handler type per `Space`
   instance.** Every mount point in a given `Space` uses the same
   concrete `H: SpaceHandler<T>`. This needs zero new dispatch substrate —
   ordinary monomorphization handles it, calls into `H`'s methods are
   direct, non-virtual calls. The restriction: you cannot mix, say, a
   `/cache` mount backed by an LRU-eviction handler and a `/log` mount
   backed by an append-only handler in the same `Space` instance; that
   requires two separate `Space` values.
2. **Heterogeneous mounts (any handler type, anywhere, in one instance)**
   — the full version of "user can insert new spaces that are really code
   that decides how they handle their own sub items" from the original
   motivating idea. This needs new trait-object/type-erasure language
   substrate that does not exist yet, and is out of scope until that
   substrate is designed separately (it is a generally useful capability,
   not `Space`-specific, the same way the tuple prototype's heterogeneous
   type packs were general substrate rather than tuple-specific).

Recommended sequencing: ship option 1 first. It already satisfies the
core idea — a subtree with custom insert/take logic — for the common case
of "this `Space` has one kind of smart subtree behavior," and does not
block on unbuilt compiler substrate. Option 2 is a real, separate,
larger follow-up.

Routing performance notes for whichever option:

- the trie should be read-mostly: looking up a path never needs to block
  on a lock; only mounting/unmounting a handler (rare, compared to
  `insert`/`take` volume) pays a write cost
- everything above a mount boundary stays on the plain default-storage
  path — no indirection, no allocation beyond what default storage itself
  needs; only crossing into a mount pays for the call into `H`

## Concurrency Model

Two separate questions, decided differently:

**Calls into a mounted handler are serialized by the runtime, not the
handler.** One in-flight call per mount point at a time. This matches
`docs/MultithreadingPrototype.md`'s existing stance ("shared mutable state
is rejected by default unless wrapped in safe primitives") — a
`SpaceHandler` implementation stays ordinary sequential code: no locks,
no atomics, no `unsafe`. If a specific mount point becomes a contention
point under heavy concurrent access, the fix is for its author to shard
it into several mounted sub-spaces (e.g. `/sys/data/0`..`/sys/data/N`),
not to push concurrent-reentrancy reasoning onto every handler author by
default.

**Blocking `take`/`read` needs the scheduler work that does not exist
yet.** `docs/MultithreadingPrototype.md` is explicit that the current
runtime slice "deliberately does not... schedule multiple tasks
concurrently," and lists "true parallel scheduling, task groups, detached
tasks, channels, and scheduler controls" as remaining work. A real
blocking `take` — park the calling task, wake it when a matching `insert`
happens, target the correct waiter rather than a global broadcast — is
exactly a scheduler-level capability. Per-path (or per-node) wait queues
are the right shape once that exists (wake the one waiter satisfied by
this insert, not every waiter in the `Space`), but there is nothing to
attach that design to today. This is why the first slice ships
`try_take`/`try_read` instead of blocking `take`/`read`: they are fully
implementable against today's single-task runtime, and the blocking forms
become a straightforward addition once scheduler-backed parking exists.

Effects: `Space` operations require declaring `effects(space)`, following
the same visible-effects rule `effects(task)` already establishes for
`[spawn]`/`wait`. Once blocking `take`/`read` exist, they additionally
require `effects(task)` (parking is a scheduler operation, same as
`wait`), matching this expected diagnostic shape from
`docs/MultithreadingPrototype.md`:

```text
space operation requires space effect
```

## Diagnostics

Expected diagnostic shapes:

```text
space operation requires space effect
mount conflict: /sys/data already mounted
no viable overload for `insert`: no candidate accepts (bool, i32)
```

The last one only applies once `Space` adopts a true `insert` overload set
per `docs/OverloadResolutionPrototype.md`; until then, `insert` and
`insert_by_key` are distinct names and ordinary "no matching definition"
diagnostics apply.

## Relationship To Multithreading Prototype

`Space` is a safe shared-state primitive in the same category
`docs/MultithreadingPrototype.md` already reserves space for under
"Future safe shared-state primitives," alongside `Atomic<T>` and
`Mutex<T>`/`RwLock<T>`. Its blocking operations are gated on that
document's "remaining implementation work" (true parallel scheduling), not
on anything specific to `Space` itself.

## Relationship To Overload Resolution Prototype

`Space.insert`/`try_take`/`try_read` presenting one name each for both
`string` and `PathKey` argument forms is the concrete motivating case for
`docs/OverloadResolutionPrototype.md`. That document's Phase 0
(consolidate the arity-only resolution that is currently re-derived by
string convention in ~28 files) and Phase 1 (type-based matching, with the
C++-style specificity rule for the generic-vs-concrete case) are both
prerequisites for collapsing `Space`'s two-name workaround into true
overloads.

## First Implementation Checklist

Buildable against today's compiler and runtime (no new substrate):

- [ ] `PathKey` struct and `hash_path` pure function
- [ ] `Space<T>` default storage: insert, `try_take`, `try_read` over
  exact-path keys
- [ ] `effects(space)` requirement and diagnostic
- [ ] `Space<T, H>` single-statically-typed-handler mount points
  (option 1 in "Routing And Mount Points"), monomorphized, no dynamic
  dispatch
- [ ] mount-point call serialization (one in-flight call per mount)
- [ ] positive/negative tests: exact-path insert/try_take/try_read,
  mount routing to `H`, mount-conflict diagnostic, missing-effect
  diagnostic

Blocked on other prototype substrate:

- [ ] blocking `take`/`read` — needs `docs/MultithreadingPrototype.md`'s
  true parallel scheduling work
- [ ] per-path wait queues for blocking operations — same dependency
- [ ] true `insert`/`take`/`read` overload sets (`string` vs `PathKey`
  under one name) — needs `docs/OverloadResolutionPrototype.md` Phase 0
  and Phase 1
- [ ] heterogeneous mount-point handler types in one `Space` instance
  (option 2 in "Routing And Mount Points") — needs new trait-object /
  type-erasure language substrate, not yet designed anywhere in this repo
- [ ] whole-program perfect-hash generation for literal `Space` paths
  (would let `PathKey` drop its `segments` fallback) — optimization,
  not required for correctness
