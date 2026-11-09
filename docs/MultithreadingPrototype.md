# PrimeStruct Multithreading Prototype

Classification: **design direction**, not yet part of the canonical language
spec. Implemented: the `[spawn]`, `Task<T>`, `wait` and `effects(task)` surface
and its lifetime checks, with spawned calls run inline, one after another (no
threads). Approved but not implemented: [Threads For Real](#threads-for-real)
(parallel tasks, thread-safe heaps, sendable values, `Mutex`, `Atomic`,
`Channel`, a multi-core web server); its work is the Multithreading lane of
`docs/todo.md`.

This document captures the language-level multithreading model.
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

- detached tasks
- transferring tasks to supervisors or external runtimes
- channels, mutexes, atomics, thread-local storage, and fences (channels,
  mutexes and atomics are now designed in [Threads For Real](#threads-for-real))
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
[effects(task), return<int>]
main() {
  [Task<int>] left{[spawn] computeLeft()};

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
- `wait(left, right, ...)` consumes multiple task handles and returns an
  ordinary stdlib `tuple<...>` with one result per task in argument order

The grouped effect name `task` is the initial prototype spelling. Later design
work may split it into finer effects such as `task_spawn`, `task_wait`, and
`task_cancel` if that proves useful.

Current implementation status: TODO-4561 locked the parser surface and
documentation spelling, and TODO-4562 added semantic `Task<T>` facts, `task`
effect requirements, wait result inference, and first-slice lifetime
diagnostics. `spawn` is accepted as an execution transform on call envelopes
such as `[spawn] f(...)`, and `wait(task)` now consumes a semantic task handle.
TODO-4563 added the first VM/native execution behavior: a single spawned call
stores its result in the task handle binding, and `wait(handle)` returns that
stored result. TODO-4278 then layered multi-task `wait(left, right, ...)` on
top of that substrate by returning ordinary stdlib tuple values. This is
structured task syntax and result collection, not true parallel scheduling yet.

## First Runtime Slice

For the implemented single-task runtime slice, VM/native execution lowers:

```prime
[Task<int>] left{[spawn] computeLeft()};
leftResult{wait(left)}
```

as a stack-backed task handle whose payload is the result of `computeLeft()`.
`wait(left)` returns that payload. The semantic lifetime rules still require
the handle to be waited before return and reject double wait or escaping
handles, so future scheduler-backed tasks can preserve the same source
contract.

This runtime slice deliberately does not start an OS thread, detach work, or
schedule multiple tasks concurrently. It exists so the task handle lifetime
rules and stdlib tuple integration can settle before scheduler-backed tasks are
introduced.

## Required Effect

The spawning function must declare the task effect:

```prime
[effects(task), return<int>]
main() {
  [Task<int>] left{[spawn] computeLeft()};
  return(wait(left))
}
```

This should reject:

```prime
[return<int>]
main() {
  [Task<int>] left{[spawn] computeLeft()};
  return(wait(left))
}
```

Expected diagnostic shape:

```text
spawn requires task effect
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
[effects(task), return<int>]
main() {
  [Task<int>] left{[spawn] computeLeft()};
  return(0)
}
```

Expected diagnostic shape:

```text
task handle must be waited before return: left
```

The same check applies to implicit fallthrough at the end of a function.

## Wait Consumes The Handle

`wait(handle)` consumes the task handle.

This should reject:

```prime
[effects(task), return<int>]
main() {
  [Task<int>] left{[spawn] computeLeft()};
  first{wait(left)}
  second{wait(left)}
  return(first + second)
}
```

Expected diagnostic shape:

```text
task handle already waited: left
```

After a successful `wait(left)`, `left` is no longer a live task handle.

## Handle Escape Rule

For the first slice, task handles cannot escape the function that spawned them.

Reject returning a task:

```prime
[effects(task), return<Task<int>>]
main() {
  [Task<int>] left{[spawn] computeLeft()};
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
[effects(task), return<int>]
main([bool] cond) {
  [Task<int>] left{[spawn] computeLeft()};

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
[effects(task), return<Result<int, Error>>]
main() {
  [Task<int>] left{[spawn] computeLeft()};
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
[effects(task), return<int>]
main() {
  [mut] counter{0}

  [Task<int>] left{[spawn] increment(counter)};
  result{wait(left)}

  return(result)
}
```

Expected diagnostic shape:

```text
mutable binding `counter` cannot be captured by spawned task
```

Safe shared-state primitives make sharing explicit; their API is settled in
[Threads For Real](#threads-for-real):

```prime
[effects(task, atomic, heap_alloc), return<int>]
main() {
  counter{Atomic<i64>{0i64}}
  [Task<void>] left{[spawn] increment(counter)};
  wait(left)
  return(convert<i32>(counter.load()))
}
```

Lambdas are closure structs (`docs/spec/value-lifecycle.md`, Lambdas), so the same rule applies to them as values: a
closure is sendable only when every capture is a by-value, init or `move` capture of a sendable type. A `[ref name]`
or `[&]` capture is a borrow of the spawning frame and is never sendable; neither is a `Function<R, A...>`, since the
erased type no longer shows its capture types. `SendableFunction<R, A...>` is the marked form
([Sendable Functions](#sendable-functions)).

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

## Multi-Wait

Multi-wait returns ordinary stdlib `tuple` values. The tuple storage and helper
surface are tracked separately in `docs/TuplePrototype.md`.

Implemented shape:

```prime
import /std/tuple/*

[effects(task), return<int>]
main() {
  [Task<i32>] left{[spawn] computeLeft()};
  [Task<i32>] right{[spawn] computeRight()};

  both{wait(left, right)}

  return(both[0i32] + both[1i32])
}
```

Destructuring the result, `[leftResult rightResult] wait(left, right)`, is
rejected today with `task handles cannot escape their spawning function: left`
(checked 2026-11-09); TODO-5807 makes it work.

Semantically:

```prime
wait(Task<A>, Task<B>) -> tuple<A, B>
```

The tuple elements preserve task argument order. The result can be consumed
through `get<I>`, bracket indexing, or tuple destructuring.

## Threads For Real

Classification: **design direction**, approved 2026-11-09 for a web server that
uses several cores (`docs/WebAppsPlan.md`, Concurrency). None of it is
implemented yet; the leaves are the Multithreading lane of `docs/todo.md`, which
comes after the PrimeScroll startup lane and the first two native optimization
leaves (TODO-5770, TODO-5772).

The source contract above stays as it is: `[spawn] f(...)` gives a `Task<T>`,
`wait` consumes it, the spawning function declares `effects(task)`, and every
task is waited before its function returns. What changes is that tasks run in
parallel on `optexe` and arm64 native, and interleave in a reproducible order on
the VM. That needs thread-safe heaps, a type-level notion of which values may
cross to another thread, and three safe sharing primitives.

### Runtime Model

- **One OS thread per spawned task** on `optexe` (`std::thread`) and arm64 native (`pthread_create`), joined by
  `wait`. Not a fixed pool: a task may block in `Mutex.locked` or a `Channel`, and a pool whose threads are all
  blocked on work queued behind them deadlocks. A pool for tasks that never block can come later if the cost of
  starting a thread (tens of microseconds) shows up in a profile.
- **Spawn copies its arguments.** The arguments are evaluated on the spawning thread and copied into a task block
  on the heap (`move(x)` hands a value over without a copy); the callee's parameters borrow that block. A borrow
  of the spawning frame cannot cross: `optexe` and the VM number local addresses per call stack (`Rt::frames` in
  the generated runtime, the VM's numbering of live locals), so a local address means nothing on another stack.
  `Mutex`, `Atomic` and `Channel` are handles, so the copy shares them (below).
- **Results:** the task block holds the result slots; `wait` joins, copies the result out and frees the block.
  `Task<void>` works too (the checker accepts it today, but lowering rejects it with `void call not allowed in
  expression context`).
- **Ordering:** starting a task happens after the spawn; `wait` returns after the task ended; a `locked` body
  starts after the previous one on that mutex ended; a value is received after it was sent; atomic operations are
  sequentially consistent. There is no weaker memory order in safe code.
- **Faults:** a runtime fault on any task ends the program with exit code 3 after flushing, as on one thread.
  `optexe` must leave through `std::_Exit`, not `std::exit` (which `ps_fault` uses today and which runs static
  destructors while other threads still run).
- **Output:** a print writes its line in one write, so lines of different tasks never mix within a line
  (`optexe`'s `ps_emit` writes the newline with a second call today).
- **IR:** `TaskSpawn <function>` pops the callee's arguments into a new task block and pushes the task handle;
  `TaskWait` pops a handle and pushes the result; atomic opcodes are below. That is a PSIR version bump. Backends
  without threads (x86_64 native, the Linux JIT, Wasm, GLSL, the simple C++ emitters) keep running a spawned call
  inline, which is a legal schedule for programs without channels, and reject `Channel` ("channels need a backend
  that runs tasks"), because inline execution can deadlock on one. GPU targets keep rejecting `task`.

### VM Scheduler

The VM runs tasks on its one OS thread, so conformance tests stay reproducible (determinism, AGENTS.md):

- A task is a VM call stack (frames and operand stack); all tasks share the heap, which therefore needs no lock.
- **Switch points:** spawn, `wait`, lock and unlock, send, receive, close, and atomic operations. No switch
  happens anywhere else.
- **Default schedule, eager:** a spawn runs the new task until it ends or blocks, then the spawner continues; a
  blocked task yields to the lowest-numbered runnable task. Every program that runs today prints exactly what it
  prints now.
- **Seeded schedules:** `--vm-schedule-seed <n>` (or `PRIMESTRUCT_VM_SCHEDULE_SEED`) picks the next task at every
  switch point with a generator seeded by `n`; the same seed gives the same run. Conformance tests of concurrent
  programs run several seeds and expect the same output, which is how a test shows it is free of races.
- **Deadlock:** when every task is blocked the VM faults with `deadlock: every task is waiting` (exit code 3).
- A host call that blocks (`poll`, `recv`) blocks every task: on the VM a concurrent program runs correctly but
  not in parallel.

### Heaps

Today every allocator is one unsynchronized structure:

- arm64 native (`src/native_emitter/NativeEmitterInternalsArm64Heap.h`): the chunk cursor and end and one
  free-list head per size class sit in the writable data segment (`HeapStateBytes`).
- `optexe` (`include/primec/runtime/VmHeapCore*.{h,inc}`, `VmTextHeap*`): one `VmHeapCore` in `Rt`, whose slot
  and byte storage are `std::vector`s that move when they grow, with live and free runs in `std::map`/`std::set`;
  `byteAt` even writes its last-region cache on a read. One `VmTextHeap` holds the `StringArena` stack and a
  literal cache filled on first use.

The design is per-thread arenas, with a lock only on a shared slow path:

- **optexe:** one `VmHeapCore` arena per thread. The arena number goes in address bits 41 to 46, so arena 0
  (the main thread, and the VM) keeps today's addresses and the VM/`optexe` parity tests do not change. Once a
  second thread exists, storage never moves (a reservation, not a growing vector). A liveness check reads only a
  state table, with relaxed atomic loads: slots have one already, and byte regions get one instead of the map
  search (TODO-5793); no read writes anything, and the last-region cache is per thread. Each thread has its own
  `VmTextHeap`, so a `StringArena` opened on one thread never catches another thread's strings, and its own
  literal cache. At most 64 arenas; a 65th thread shares the last one behind its lock.
- **arm64 native:** each thread has its own allocator state (today's layout plus a remote-free head). The main
  thread keeps the data-segment state; a spawned thread takes one from a pool of states that is never unmapped,
  because a block can outlive its thread. Heap routines find the state through a pthread key
  (`pthread_getspecific`, null on the main thread). Pooled chunks are aligned to their 256 KiB size and start
  with their owner's state pointer, so `HeapFree` finds the owner by masking the address. A module without
  `TaskSpawn` emits today's routines byte for byte, so single-threaded programs (PrimeScroll) pay nothing in
  size or startup. The first native step puts one LSE spin lock around the routines; per-thread states replace
  it in a second step that has to show its gain.
- **Freed by another thread:** a block freed by a thread other than its owner goes on the owner's remote-free
  list (native: a compare-and-swap push with LSE `casal`; `optexe`: a queue behind the arena's lock), which the
  owner drains when it next allocates (native: when a size class's list is empty). A double free found while
  draining faults with the usual message. Large blocks (their own mapping) are unmapped by whoever frees them.
- **Native thread stacks:** a secondary pthread gets 512 KiB of stack on macOS by default, but an entry frame
  reserves a 1 MiB value stack, so the thread trampoline asks for 8 MiB (`pthread_attr_setstacksize`), like the
  main thread.
- **Strings:** `String` blocks come from these heaps, so `String` is sendable once a block can be freed on any
  thread. `SharedString` also needs an atomic owner count (TODO-5656); its count sits right after the text's 0
  byte today, at an unaligned offset, so it moves to an aligned place first.

### Sendable Values

A value is **sendable** when another thread may own it. The rule is structural:

| Sendable | Not sendable |
| --- | --- |
| `bool`, integers, floats, `string` (an index into the immutable string table), enums | `Reference<T>`, `Pointer<T>` |
| `String`, `vector<T>`, `map<K, V>`, `Maybe<T>`, `Result`, tuples of sendable types | views and carriers (`StringView`, slices, closures with a `[ref]` capture) |
| structs whose fields are all sendable | `Function<R, A...>` (its closure type is erased) |
| `SendableFunction<R, A...>`, `Mutex<T>`, `Atomic<T>`, `Channel<T>` | `Task<T>`; structs marked `[thread_bound]` (native UI handles: AppKit objects belong to the main thread) |
| | `SharedString` until TODO-5656 |

- `[sendable]` on a struct asserts that it is sendable although it holds a `Pointer` (the stdlib `String`,
  `vector`, `map`, `Mutex`, `Atomic`, `Channel`). Outside the stdlib it needs `unsafe` (open question 2).
- **Where it is checked:** every argument of a spawned call, by the type of the argument expression (today only
  argument names are checked: `mutable binding cannot be captured by spawned task`); the element type of
  `Mutex<T>` and `Channel<T>`; a closure converted to `SendableFunction`; the handler of `serveWorkers`.
- A `mut` binding stays rejected as a spawn argument, as above: it would be copied, which reads like sharing. The
  diagnostic points at `Mutex`, `Atomic` and `Channel`.
- A `String` made while a `StringArena` is open cannot be a spawn argument or be sent: `string escapes its arena
  via spawn` (or `via channel`), next to the existing return, outer-binding and container cases.

Expected diagnostic shapes:

```text
spawn argument is not sendable: Pointer<u8> (argument 1 of /fill)
Mutex element type is not sendable: StringView
closure cannot become a SendableFunction: capture `window` is a Window, which is thread_bound
```

### Sendable Functions

`SendableFunction<R, A...>` is a `Function<R, A...>` whose conversion accepts only sendable closures, and which is
sendable itself. It has `Function`'s layout and lowering: the closure pass spells it as its own struct family
(`/__SendableFunctionN`) and the `function-values` pass numbers the closures converted to each instance. A plain
`Function` stays not sendable. Inferring sendability for each `Function` instance from every closure converted to
it was considered and rejected: a closure added anywhere in the program could then break a spawn site far away,
with an error that does not point at the cause.

```prime
[return<i64>]
apply_twice([SendableFunction<i64, i64> mut] step, [i64] start) {
  return(step(step(start)))
}
```

`call` stays `mut`. Sendable means the value may move to another thread, not that two threads may call it at
once; a handler shared between threads is called inside `Mutex.locked`.

### Mutex

`Mutex<T>` owns a `T` in a heap cell, and the only access is `locked(body)`: it locks, calls `body` with a
`Reference<T>`, and unlocks on every way out of `body`. No guard value exists, so none can escape, be stored, or
be held across a `wait`.

```prime
import /std/sync/*

[struct]
Tally() {
  [i64 mut] total{0i64}
  [i32 mut] entries{0i32}
}

[effects(lock)]
record([Mutex<Tally>] tally, [i64] amount) {
  tally.locked([amount]([Reference<Tally>] state) {
    state.total = state.total + amount
    state.entries = state.entries + 1i32
  })
}

[effects(task, lock, heap_alloc, io_out), return<int>]
main() {
  tally{Mutex<Tally>{Tally{}}}
  [Task<void>] left{[spawn] record(tally, 3i64)};
  [Task<void>] right{[spawn] record(tally, 4i64)};
  wait(left, right)
  [i64] total{tally.locked([]([Reference<Tally>] state) { return(state.total) })}
  print_line(total)
  return(0)
}
```

- `locked` is generic over the closure type (`locked<Body>([Body mut] body)`), so `body` may borrow the caller's
  locals with `[ref name]`: it runs inline, on the calling thread. `locked` returns what `body` returns.
- The `Reference<T>` is a borrowed parameter of `body`, so the existing rules (`borrowed parameter escapes via
  return` / `via assignment`) keep it inside.
- **Copying a `Mutex` shares it:** the handle carries an atomic count, and the last copy destroys `T`. A spawn
  argument is a copy, which is how a task gets the mutex.
- `T` must be sendable. Locking a mutex again inside its own `locked` body faults with `mutex locked again by the
  task that holds it` (the VM and `optexe` record the holder). Two mutexes locked in opposite orders deadlock; the
  VM reports it, threads hang.
- Effect `lock`; making one needs `heap_alloc`. `RwLock<T>` follows the same shape later, with `read(body)` taking
  a read-only `T` (TODO-5804).

### Atomic

`Atomic<i32>` and `Atomic<i64>` are shared integer cells:

```prime
[effects(atomic)]
count_hits([Atomic<i64>] hits, [i32] times) {
  [i32 mut] i{0i32}
  while(i < times) {
    hits.add(1i64)
    i = i + 1i32
  }
}
```

- `load()`, `store(value)`, `add(delta)` and `exchange(value)` (both return the previous value) and
  `compareExchange(expected, desired)` (returns whether it stored). Sequentially consistent only.
- Copying shares the cell, as for `Mutex`. Effect `atomic`; making one needs `heap_alloc`.
- **IR:** `AtomicLoad`, `AtomicStore`, `AtomicAdd`, `AtomicExchange` and `AtomicCompareExchange` on a heap address,
  32 or 64 bits wide. arm64 native uses LSE (`ldar`/`stlr`, `ldaddal`, `swpal`, `casal`; every Apple arm64 CPU has
  it), `optexe` uses `std::atomic_ref`, and the VM does plain operations at a switch point.
- `docs/spec/type-system.md` calls the primitive types "Atomic"; that category is unrelated to this type.

### Channel

`Channel<T>` is a bounded FIFO between tasks, for any sendable `T`:

```prime
[effects(channel)]
produce([Channel<i64>] jobs, [i64] count) {
  [i64 mut] i{0i64}
  while(i < count) {
    jobs.send(i)
    i = i + 1i64
  }
  jobs.close()
}

[effects(channel), return<i64>]
sum_jobs([Channel<i64>] jobs) {
  [i64 mut] sum{0i64}
  [bool mut] open{true}
  while(open) {
    pick(jobs.receive()) {
      none {
        open = false
      }
      some(job) {
        sum = sum + job
      }
    }
  }
  return(sum)
}
```

- `Channel<T>{[capacity] 16i32}` makes one. `send(value)` moves the value in and blocks while the channel is
  full; `receive()` blocks while it is empty and open and gives `none` once it is closed and drained; `close()`
  ends it, and a send after that faults with `send on a closed channel`. Any number of tasks may send and receive.
- Copying shares the channel. Effect `channel`; making one needs `heap_alloc`.
- It is stdlib code over a lock and two condition waits, which are host functions: the VM's scheduler, `optexe`'s
  `std::mutex` and `std::condition_variable`, and `pthread_mutex_*`/`pthread_cond_*` bound from libSystem on native
  (the native backend already binds libSystem functions through dyld pointer slots).

### Effects

| Effect | Needed for |
| --- | --- |
| `task` | `[spawn]` and `wait` (unchanged) |
| `atomic` | operations on an `Atomic` |
| `lock` | `Mutex.locked` (and later `RwLock`) |
| `channel` | `send`, `receive` and `close` on a `Channel` |

Making a `Mutex`, `Atomic` or `Channel` also needs `heap_alloc`. `lock` and `channel` are the effects that can
block on another task; merging them is open question 3.

### The Server

`/std/http/server` gets `serveWorkers(handler, config)` and `serveWorkersUntil(handler, config, stop)`, and
`ServerConfig` gets `workers` (0 means one per core, from a new `/std/os` `cpuCount()`). The name mirrors
`serve`/`serveUntil` and says what differs, and the thread count joins the other limits in `ServerConfig`
instead of being a bare number argument.

```prime
[return<int> effects(heap_alloc, io_err, net_io, task) on_error<ServeError, log_serve_error>]
main() {
  [ServerConfig] config{ServerConfig{[port] 8080i32, [workers] 4i32}}
  serveWorkers(withRequestLog(todo_app()), config)?
  return(0i32)
}
```

- **Shape:** one thread polls the listener, the open connections and a wake pipe, and accepts. When a connection
  is readable it leaves the poll set and its slot goes to the workers over a `Channel`. A worker reads what has
  arrived and parses it. A complete request is answered: the worker calls the handler inside the handler's
  mutex, serializes the response, sends it, closes the connection and hands the slot back (a second channel and
  the wake pipe). An incomplete one goes back to the poller to be watched again. A slow client still holds only
  its own slot, never a worker (TODO-5705's guarantee).
- **The handler runs under a mutex:** the handler value (for a `WebApp<State>` its state, its routes and their
  closures' own `mut` captures) moves into a `Mutex`, and every request calls it inside `locked`. Existing
  `WebApp<State>`, `AppRouter<State>` and `Router` apps need no change beyond the call and `task` in `main`; their
  route tables store `SendableFunction` so the app is sendable.
- **What runs in parallel:** receiving, parsing, serializing the response and sending. What does not: everything
  the handler does, including writing the page's HTML, which happens inside handlers. A handler that does heavy
  work therefore gains little until handlers that only read the state share a read lock (TODO-5804); the server
  leaves measure both cases.
- Worker threads block `SIGINT` and `SIGTERM`, so the signal interrupts the poller's `poll` and `stopRequested()`
  is seen at once.
- **Rejected: several processes on one port** (`SO_REUSEPORT`, N copies of the binary). Each process would have its
  own state: a POST would change one copy of the todo list and the next GET could read another. It only fits
  stateless apps, and the stdlib's app model keeps state in the process (`docs/WebAppsPlan.md`, principle 2).
- **Later, filed but deferred:** read-only handlers under `RwLock` (TODO-5804), keep-alive (TODO-5805), sessions
  (TODO-5806).

### Open Questions

Each needs the user's decision; the recommendation is the plan's default.

1. **Sendable `Function` spelling.** A separate type `SendableFunction<R, A...>` (recommended), or a marker on
   `Function` itself. Also: do `Router`, `AppRouter` and `WebApp` store `SendableFunction` for every app? That
   makes a handler capturing a `Pointer` or a UI handle fail at setup even under single-threaded `serve`; no
   example does. Recommended: yes, one handler type for both servers, with no implicit conversion between the two
   `Function` types.
2. **`[sendable]` outside the stdlib.** Recommended: only with `unsafe`, like other unchecked promises.
3. **Effect names.** Recommended: four effects, `task`, `atomic`, `lock` and `channel`, as fine-grained as the I/O
   effects. The alternative is one `blocking` effect for `lock` and `channel`.
4. **Sharing a primitive.** Recommended: copying a `Mutex`, `Atomic` or `Channel` shares it (atomic handle
   count), so a spawn argument just works. The alternative is an explicit `share()` and non-copyable handles.
5. **Threads.** Recommended: one OS thread per task now, no pool, because tasks block on locks and channels.
6. **Handlers serialized in the first server.** Recommended: accept that page rendering runs one request at a time
   until TODO-5804, since it keeps every existing app correct with no change.
7. **How a handler says it only reads.** Recommended: by its state parameter, `[State] state` (a read-only borrow)
   against `[Reference<State>] state`, which needs overloads by `Function` type (TODO-5735); the alternative is a
   separately named registration such as `app.view(pattern, handler)`.
8. **VM default schedule.** Recommended: eager (today's order) by default, seeds on request; the alternative is a
   fixed seed by default, which changes the output order of existing programs that print from tasks.

## First Implementation Checklist

The first implementation slices now cover:

- parse `[spawn]` as an execution transform on call envelopes
- require `effects(task)` for functions that use `[spawn]` or `wait`
- introduce internal `Task<T>` type facts
- infer `wait(Task<T>) -> T`
- track task handle states through statements and returns
- reject double wait, escaped handles, and missing waits before return
- reject mutable/reference captures by default
- add positive and negative semantic tests
- add semantic-product coverage for task facts
- add first VM/native single-task runtime execution
- integrate multi-task `wait(left, right, ...)` with stdlib tuple values

Remaining implementation work:

- parallel tasks, thread-safe heaps, sendable values, `Mutex`, `Atomic`,
  `Channel` and the multi-core server: [Threads For Real](#threads-for-real),
  lane Multithreading in `docs/todo.md`
- task groups, detached tasks and scheduler controls: not designed yet

This document is a prototype note. The `[spawn] f(...)`, `wait(task)`, and
`effects(task)` spellings are now parser/source-lock commitments, and the
semantic and VM/native runtime rules above are implemented for single-task wait
and tuple-returning multi-wait.
