# PrimeStruct Multithreading Prototype

Status: prototype design note, not implemented yet and not part of the
canonical language spec.

This document captures the first proposed language-level multithreading model.
When the design is stable enough, fold the accepted parts into
`docs/PrimeStruct.md` and add implementation TODOs for any remaining compiler,
runtime, backend, or conformance work.

## Design Goals

PrimeStruct should be a systems programming language with C-level power while
keeping safe, readable defaults. Multithreading should follow the same rule:

- safe structured concurrency should be ergonomic by default
- user-requested concurrency is always explicit and effectful
- shared mutable state is rejected by default unless wrapped in safe primitives
  or guarded by explicit unsafe opt-outs
- low-level OS threads, raw shared memory, atomics, and scheduler controls
  should remain possible through visible effects and unsafe capabilities
- a function must not accidentally return while work it spawned is still live

## Non-Goals For The First Slice

The first slice should not try to design every concurrency feature at once.

Out of scope for the first implementation:

- multi-task `wait(left, right)` `tuple` returns
- tuple/product type implementation details beyond the separate
  `docs/TuplePrototype.md` design note
- detached tasks
- transferring tasks to supervisors or external runtimes
- channels, mutexes, atomics, thread-local storage, and fences
- raw OS thread APIs
- async/event-loop syntax
- GPU parallel kernel syntax

Those features can build on the same ownership and effect rules later.
`wait` is the prototype spelling for joining spawned work; reserve `await` for
a future async/event-loop model only if that model becomes distinct from
systems-level task or thread waiting.

## Core Principle

Any function that requests multithreaded or multitasked work must declare an
effect. This includes requesting a language task, runtime task group, OS thread,
thread group, worker pool, or scheduler-owned execution resource.

This remains true even if:

- every spawned task is waited before return
- the compiler can prove the task scope terminates
- the runtime uses cooperative scheduling rather than OS threads
- the result is deterministic for a given input

Termination, structured scope, and effect visibility are separate concerns.
Structured scope proves that spawned work cannot accidentally outlive the
function. The effect annotation tells callers that the function requests
concurrency resources or scheduling behavior.

Compiler-chosen internal parallelization of a pure deterministic operation is a
different case. If the source program did not request concurrency and the
operation has the same observable semantics, no source-level task effect is
required.

## First Surface Syntax

Use an execution transform for spawning:

```prime
[effects(task), int]
main() {
  left{[spawn] computeLeft()}

  leftResult{wait(left)}

  return(leftResult)
}
```

`[spawn] computeLeft()` is a transformed execution envelope. The execution
target remains `computeLeft()`, while `[spawn]` changes how the call is
scheduled and what value it produces.

For the first slice:

- `[spawn] f(...)` returns `Task<T>` when `f(...)` returns `T`
- `wait(task)` consumes `Task<T>` and returns `T`
- only single-task `wait` is required
- multi-task wait is reserved for a later tuple/product-value feature

The grouped effect name `task` is the initial prototype spelling. Later design
work may split it into finer effects such as `task_spawn`, `task_wait`, and
`task_cancel` if that proves useful.

## Required Effect

The spawning function must declare the task effect:

```prime
[effects(task), int]
main() {
  left{[spawn] computeLeft()}
  return(wait(left))
}
```

This should reject:

```prime
[int]
main() {
  left{[spawn] computeLeft()}
  return(wait(left))
}
```

Expected diagnostic shape:

```text
spawn requires effect `task`
```

If the effect is later split, the diagnostic should name the narrower required
effect.

## Structured Lifetime Rule

Every task handle created by `[spawn]` in a function must be consumed before the
function returns, unless a future explicit transfer or detach operation takes
ownership of it.

The same high-level rule should apply to future structured thread handles and
thread groups: a function that starts threads or task groups must wait, join,
transfer, or explicitly detach them before it can return. The first
implementation slice only needs to enforce this rule for `Task<T>` handles
created by `[spawn]`.

This should reject:

```prime
[effects(task), int]
main() {
  left{[spawn] computeLeft()}
  return(0)
}
```

Expected diagnostic shape:

```text
task `left` must be waited before returning from `/main`
```

The same check applies to implicit fallthrough at the end of a function.

## Wait Consumes The Handle

`wait(handle)` consumes the task handle.

This should reject:

```prime
[effects(task), int]
main() {
  left{[spawn] computeLeft()}
  first{wait(left)}
  second{wait(left)}
  return(first + second)
}
```

Expected diagnostic shape:

```text
task `left` has already been consumed by wait
```

After a successful `wait(left)`, `left` is no longer a live task handle.

## Handle Escape Rule

For the first slice, task handles cannot escape the function that spawned them.

Reject returning a task:

```prime
[effects(task), Task<int>]
main() {
  left{[spawn] computeLeft()}
  return(left)
}
```

Reject storing a task in a longer-lived object or passing it to an arbitrary
callee unless the callee is a future approved ownership-transfer primitive.

Expected diagnostic shape:

```text
task handles cannot escape their spawning function
```

This keeps the first model structured and avoids designing detached or
supervised tasks prematurely.

## Control Flow Tracking

The compiler must track task-handle state through control flow.

Minimum state per task handle:

- `live`: spawned and not yet consumed
- `waited`: consumed by `wait`

At every explicit `return` and function end, no task owned by that function may
be `live`.

For branches, the first implementation can be conservative:

- waited in all paths means waited after the branch
- live in any path means live after the branch
- waited in one path and live in another remains live unless the compiler emits
  a more precise path-specific diagnostic

Example:

```prime
[effects(task), int]
main([bool] cond) {
  left{[spawn] computeLeft()}

  if (cond) {
    value{wait(left)}
  }

  return(0)
}
```

Expected diagnostic shape:

```text
task `left` may still be running when `/main` returns
```

Loops can start with conservative rules. A task spawned inside a loop body must
be waited on every path through that iteration unless the compiler can prove the
handle is consumed before the next iteration or loop exit.

## Error Handling And Early Return

Structured task scope must remain valid under early return and `Result`
propagation.

If a future expression can return early while a task is live, the compiler must
either reject it or require a defined cleanup policy before the function exits.

For the first slice, prefer rejection when the cleanup behavior is not explicit:

```prime
[effects(task), Result<int, Error>]
main() {
  left{[spawn] computeLeft()}
  value{fallible()?}
  result{wait(left)}
  return(Result.ok(result + value))
}
```

Expected diagnostic shape:

```text
task `left` may still be running on early return from `/main`
```

Later designs can add scoped task groups with explicit cancellation and join
policy so early error propagation remains ergonomic without leaks.

## Captures And Shared State

The first safe rule should be strict:

- immutable values may be copied or moved into a task if their type is sendable
- mutable bindings may not be captured by spawned work by default
- references may not cross into spawned work by default
- raw pointers and unchecked shared state require future unsafe APIs

This should reject:

```prime
[effects(task), int]
main() {
  [mut] counter{0}

  left{[spawn] increment(counter)}
  result{wait(left)}

  return(result)
}
```

Expected diagnostic shape:

```text
mutable binding `counter` cannot be captured by spawned task
```

Future safe shared-state primitives can make sharing explicit:

```prime
[effects(task, atomic), int]
main() {
  counter{Atomic<int>{0}}
  left{[spawn] increment(counter)}
  wait(left)
  return(counter.load())
}
```

The exact atomic, lock, and channel APIs are out of scope for this prototype.

## High-Level And Low-Level Layers

The prototype intentionally starts with safe structured tasks. The long-term
model should still support lower-level systems programming.

Potential future layers:

- `Task<T>`: safe structured task handle
- `TaskGroup`: structured owner for multiple child tasks
- `Thread<T>`: lower-level OS or native thread handle
- `ThreadGroup`: lower-level owner for OS or native thread sets
- `Atomic<T>`: explicit atomic shared state
- `Mutex<T>` and `RwLock<T>`: safe shared mutation
- `ThreadLocal<T>`: explicit per-thread state
- unsafe raw thread APIs for FFI, raw pointers, custom schedulers, and manual
  synchronization

Low-level APIs should require visible effects and, where appropriate, `unsafe`.

Example future shape:

```prime
[unsafe effects(thread, raw_memory), int]
main([Pointer<byte>] ptr) {
  handle{thread_start(worker, ptr)}
  return(thread_wait(handle))
}
```

## Backend Expectations

Initial backend behavior may vary, but the language semantics should stay the
same.

- VM backend may implement tasks with a cooperative or deterministic scheduler.
- Native/C++ backends may lower tasks to real threads, futures, or a runtime
  task pool.
- GPU-oriented backends should reject general task effects until a specific
  kernel-level model exists.

The semantic product should eventually publish enough task facts for lowering:

- spawn site ids and callee paths
- task handle binding ids and result types
- wait site ids and consumed handles
- live-task diagnostics and source provenance
- effect summaries needed by backend selection

## Future Multi-Wait

Multi-wait is desirable but should wait for `tuple` values. The tuple design is
tracked separately in `docs/TuplePrototype.md`.

Future shape:

```prime
[effects(task), int]
main() {
  left{[spawn] computeLeft()}
  right{[spawn] computeRight()}

  [leftResult rightResult] wait(left, right)

  return(leftResult + rightResult)
}
```

Semantically:

```prime
wait(Task<A>, Task<B>) -> tuple<A, B>
```

The destructuring binding receives tuple elements in argument order. Until tuple
or product return semantics are settled, implement only:

```prime
value{wait(task)}
```

## First Implementation Checklist

When this prototype graduates into active implementation work, the first slice
should include:

- parse `[spawn]` as an execution transform on call envelopes
- require `effects(task)` for functions that use `[spawn]` or `wait`
- introduce internal `Task<T>` type facts
- infer `wait(Task<T>) -> T`
- track task handle states through statements and returns
- reject double wait, escaped handles, and missing waits before return
- reject mutable/reference captures by default
- add positive and negative semantic tests
- add at least one compile-pipeline or semantic-product dump test once task
  facts are published
- document any backend that intentionally rejects `effects(task)` at first

This document is a prototype note only. Do not treat the syntax or diagnostics
above as canonical until they are folded into `docs/PrimeStruct.md` and backed
by TODO entries plus implementation tests.
