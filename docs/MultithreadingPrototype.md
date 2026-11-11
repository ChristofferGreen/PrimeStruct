# PrimeStruct Multithreading Prototype

Classification: **design direction**, not yet part of the canonical language
spec. Implemented: the `[spawn]`, `Task<T>`, `wait` and `effects(task)` surface
and its lifetime checks, the sendable check of spawn arguments ([Sendable
Values](#sendable-values)), tasks of scalar callees under the VM's scheduler,
and the same tasks on one OS thread each on `optexe` (other spawned calls, and
every other backend, run inline, one after another), and `Atomic<i32>`,
`Atomic<i64>`, `Mutex<T>` and `Channel<T>` of `/std/sync` on the VM and
`optexe`. Approved but not implemented: the rest of
[Threads For Real](#threads-for-real) (tasks and atomics on arm64 native, a
multi-core web server); its work is the Multithreading lane of `docs/todo.md`.

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
- `wait(task)` consumes `Task<T>` and returns `T`; the result is a value of type `T` wherever a `T` is
  expected, so `return(wait(task))` returns a scalar, a `String` or a struct
  result directly
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

Since TODO-5835 the VM runs such a spawn as a task of its scheduler (Threads For
Real, VM Scheduler; docs/spec/vm-design.md, Tasks) when the callee takes and
returns scalars, `String`s, other structs outside the collections, or closures
(TODO-5845; a struct argument is copied in the spawner before the task starts,
and the waiter owns the result), and `optexe` runs it on an OS thread of its own
(Threads For Real, Runtime Model); other callees (collection or `copy`
parameters, TODO-5847; `string` parameters, TODO-5741), and every other backend,
keep the inline form, which starts no thread and runs the calls one after
another.

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

A spawned call is a call: its arguments are evaluated on the spawning thread and
copied (or moved with `move(x)`) into the task. The rule is therefore the rule
for any value that another thread may own, [Sendable Values](#sendable-values):

- a value may be copied or moved into a task if its type is sendable
- references, pointers and views may not cross into spawned work
- a `mut` binding is an ordinary argument: the task gets a copy, as any call
  would, and the spawner keeps its own
- sharing one value between tasks happens only through `Mutex`, `Atomic` and
  `Channel`, which are sendable handles

So this is accepted, and prints the spawner's `counter` plus the task's result,
each from its own copy:

```prime
[effects(task), return<int>]
main() {
  [mut] counter{0}
  counter = counter + 1
  [Task<int>] left{[spawn] increment(counter)};
  result{wait(left)}
  return(result + counter)
}
```

The check is by type (TODO-5790): the arguments are checked against the
parameter types of the spawned callee instance, so a `Reference`, a `Pointer`
or a view is rejected whatever expression produced it. A spawn diagnostic that
rejects a value says how to share instead: the message for a non-sendable
argument ends with `sharing between tasks needs Mutex, Atomic or Channel`.

Parameters borrow their arguments (`docs/spec/value-lifecycle.md`, Parameter
Passing), and a borrowed `[T mut]` parameter writes the caller's place, so a
spawned callee cannot take a binding or field through one: `spawn argument
`value` of /bump is a mutable borrow of the spawner's value: a task works on
copies, take it as `[i32 copy mut]``. A `[T copy mut]` parameter gets the task's
own copy, and the spawner's binding keeps its value; a spawned call with a
`copy`, `copy mut` or `move` parameter of a scalar or a struct runs as a task
on the VM and `optexe` (the spawner copies or hands over the value, the task
destroys its struct copy). A spawned method call
checks its receiver the same way: by its struct type (`spawn receiver of
/Holder/read is not sendable: field `entry` of Holder is Reference<i32>`), and a
`mut` method on a binding is `spawn receiver of /Counter/bump is a mutable
borrow of the spawner's value: a task works on copies`. (While tasks run inline, a
read-only `[T]` parameter observes the same value as a copy would; giving every
task its own copy of every argument is the task frame of TODO-5792.)

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
closure is sendable when every capture is a by-value, init or `move` capture of a sendable type. A `[ref name]` or
`[&]` capture is a borrow of the spawning frame and is never sendable. A `Function<R, A...>` is always sendable,
because its conversion accepts only sendable closures ([Functions Are Sendable](#functions-are-sendable)).

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

The results can also be bound by name in task order, which waits both handles
like any other consuming `wait`:

```prime
import /std/tuple/*

[effects(task, io_out), return<int>]
main() {
  [Task<i32>] left{[spawn] computeLeft()};
  [Task<i64>] right{[spawn] computeRight()};

  [leftResult rightResult] wait(left, right)

  print_line(leftResult)
  print_line(rightResult)
  return(0i32)
}
```

The statement is sugar for a hidden tuple binding of `wait(left, right)`
followed by tuple destructuring of it, so a pattern with the wrong number of
names is `tuple destructuring arity mismatch: pattern has 3 names but tuple has
2 elements`, and a later `wait(left)` is `task handle already waited: left`.
Each name is a binding of its own task's result type: with a `Task<String>` and
a `Task<Reply>`, the first name is a `String` and the second a `Reply`.

Tasks without results are waited together too: `wait(left, right)` of
`Task<void>` handles waits each in argument order, consumes every handle and has
no value, so it needs no tuple import. A multi-wait that mixes `Task<void>`
with tasks that have a result is rejected at the `wait` (`wait of several tasks
cannot mix Task<void> with tasks that have a result: wait them separately`): a
tuple has no `void` element, and leaving the void tasks out would shift the
positions of the others.

A multi-wait needs the tuple import. Without it the `wait` is the diagnostic
`wait of several tasks returns a tuple: import /std/tuple/*`.

Semantically:

```prime
wait(Task<A>, Task<B>) -> tuple<A, B>
```

The tuple elements preserve task argument order. The result can be consumed
through `get<I>`, bracket indexing, or tuple destructuring.

## Threads For Real

Classification: **design direction**, approved 2026-11-09 for a web server that
uses several cores (`docs/WebAppsPlan.md`, Concurrency). Implemented so far: the
sendable check of spawn arguments (TODO-5790) and the `TaskSpawn`/`TaskWait` IR
with the VM scheduler below (TODO-5792, docs/spec/vm-design.md, Tasks), and the
VM lowering of `[spawn]` to them for scalar callees, `Task<void>` included
(TODO-5835), and for callees with `String`, struct and closure parameters and
results (TODO-5845), the per-thread heap arenas of `optexe` (TODO-5794), and `optexe`
running those tasks on one OS thread each (TODO-5795, docs/spec/vm-design.md,
Tasks on threads), and `Atomic<i32>` and `Atomic<i64>` on the VM and `optexe`
([Atomic](#atomic); docs/spec/vm-design.md, Atomics). Tasks, atomics, mutexes and channels run on pthreads on arm64 native, with per-thread heaps (TODO-5942, TODO-5950, TODO-5951, TODO-5800). The leaves are the Multithreading lane of `docs/todo.md`, which
comes after the PrimeScroll startup lane and the first two native optimization
leaves (TODO-5770, TODO-5772).

The source contract above stays as it is: `[spawn] f(...)` gives a `Task<T>`,
`wait` consumes it, the spawning function declares `effects(task)`, and every
task is waited before its function returns. What changes is that tasks run in
parallel on `optexe` and arm64 native, and on the VM either interleave in a
reproducible order (the deterministic scheduler) or run in parallel too ([VM
Threads](#vm-threads)). That needs thread-safe heaps, a type-level notion of which values may
cross to another thread, and three safe sharing primitives.

### Runtime Model

- **One OS thread per spawned task** on `optexe` and arm64 native (`pthread_create`, on `optexe` too: a
  `std::thread` cannot be given a stack size, and a secondary thread's default of 512 KiB on macOS is too little
  for the call depth the main thread has), joined by `wait`. Not a fixed pool: a task may block in `Mutex.locked` or a `Channel`, and a pool whose threads are all
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
  `optexe` leaves through `std::_Exit`, not `std::exit` (which runs static destructors while other threads
  still run): `ps_fault` always, and a trap or the entry's return in a program that spawns.
- **Output:** a print writes its line in one write, so lines of different tasks never mix within a line
  (`optexe`'s `ps_emit` puts the text and its newline in one `fwrite`).
- **IR:** `TaskSpawn <function>` pops the callee's arguments into a new task block and pushes the task handle;
  `TaskWait` pops a handle and pushes the result; atomic opcodes are below. That is a PSIR version bump. Backends
  without threads (x86_64 native, the Linux JIT, Wasm, GLSL, the simple C++ emitters) keep running a spawned call
  inline, which is a legal schedule for programs without channels, and reject `Channel` (`channel operations are
  not supported by the native backend`), because inline execution can deadlock on one. GPU targets keep rejecting
  `task`.

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

### VM Threads

Classification: **design direction**, decided 2026-11-11. Nothing here is implemented; the leaves are the VM threads
lane of `docs/todo.md` (TODO-5896 to TODO-5904). The VM scheduler above describes what the VM does today and stays
true of the default until TODO-5904.

The VM gets a second way to run tasks: each on its own OS thread, in parallel, with the semantics `optexe` already
has. The deterministic scheduler stays; it is not replaced.

- **Two schedules.** The deterministic scheduler (eager, or seeded with `--vm-schedule-seed`) is the schedule of debug
  sessions, of race checking and of conformance tests. The threads schedule (`--vm-threads`) is the run schedule.
  It starts behind the flag; once the concurrent test corpus passes repeated runs and a ThreadSanitizer run
  (TODO-5900) it becomes the default (TODO-5904), because the flip changes the print order of every concurrent
  program.
- **One OS thread per task**, started with an 8 MiB stack as `optexe` does, with a task table of handles and
  generations behind one mutex. The task opcodes, handle format, ordering rules and fault behavior of docs/spec/
  vm-design.md (Tasks, Tasks on threads) apply unchanged.
- **Heaps:** the VM allocates through `VmHeapArenas` (include/primec/runtime/VmHeapArenas.h), one arena per thread,
  arena 0 for the main task, so a single-task program keeps today's addresses and pays nothing until a second arena is
  asked for. Blocks freed by another thread go through the owner's remote-free queue, as on `optexe`.
- **Sharing:** the sendable rules are unchanged. A `Reference` never crosses to another thread, so the only shared
  mutable state is `Mutex`, `Atomic` and `Channel`. Their opcodes become real atomic operations, and lock and notify
  block the thread, with waiting threads parked by address. When every thread is waiting the run faults with
  `deadlock: every task is waiting`, as on `optexe` since TODO-5846.
- **Host calls** take one global lock at first. A host function registered as reentrant runs without it, and a call
  that blocks (`poll`, `recv`) then blocks only its own task, which lifts the limit stated for the deterministic
  scheduler. A host function registered as main-thread only (the native UI) faults when another thread calls it.
- **Faults and cancel:** a fault or `Trap` on any task sets a stop flag that every thread polls at calls and loop
  back-edges; the run ends with exit code 3 after flushing, and the process survives, so the embed API
  (`Script::run`, `RunOptions::cancel`) can use the mode.
- **Speed:** a VM program that uses tasks runs each task on the flat loop between scheduler opcodes (TODO-5870), as
  fast as a program without tasks. The flat loop's state is already per call stack (`VmFastTask`), so TODO-5902 is the
  threads mode giving each task's `VmFastTask` an OS thread and the scheduler opcodes real primitives.
- **Replay:** the language has no shared mutable state outside `Mutex`, `Atomic` and `Channel`, so the recorded order
  of synchronization events is enough to reproduce a threaded run, apart from host input and output. A threads run
  records that order and the deterministic scheduler replays it (TODO-5901). This is also what lets the inspector
  (`docs/VmInspectorPlan.md`) show a threaded run.
- **Debug sessions** run programs with tasks on the deterministic scheduler, one DAP thread per task, so stopping every
  thread is trivial (TODO-5903). They fault on task opcodes today.

### Heaps

The arm64 native allocator is still one unsynchronized structure
(`src/native_emitter/NativeEmitterInternalsArm64Heap.h`): the chunk cursor and end and one free-list head per
size class sit in the writable data segment (`HeapStateBytes`). The VM has one `VmHeapCore` and one
`VmTextHeap` and no locks.

The design is per-thread arenas, with a lock only on a shared slow path:

- **optexe** (done, TODO-5794; `include/primec/runtime/VmHeapCore*.{h,inc}`, `VmTextHeap*`,
  `VmHeapArenas*`): one arena per thread, a `VmHeapCore` and a `VmTextHeap`, handed out by `VmHeapArenas`
  (`acquire`, `release`; an arena whose thread ended is reused, its live blocks stay). The arena number goes in
  address bits 41 to 46, so arena 0 (the main thread, and the VM) keeps today's addresses and the VM/`optexe`
  parity tests do not change. Until a second arena is asked for, arena 0 is exactly the single-threaded heap:
  growing vectors and no lock. The first `acquire` pins it: every arena reserves its address space once (64 GiB
  of bytes and 2^32 slots by default, mapped without commit), so storage never moves again, and an arena that
  fills its reservation faults with `VM heap allocation overflow`. A liveness check reads only a state table,
  with relaxed atomic loads (slots have one state each, byte regions one per 64 bytes since TODO-5793), so no
  read writes anything and there is no per-thread region cache of this arena's own; an address of another arena is
  answered by that arena's tables, found through a table of 64 arena pointers, and the last other arena a heap read
  is remembered (one pointer, since an arena's storage never moves), so a granule of another thread's block that
  its region fills is checked in line with one more compare (TODO-5872). Allocation, free and the few map lookups
  (a byte in a granule two regions share, a span over 128 bytes, a block's size) take the arena's lock, which
  only its owner and remote frees contend for. Each thread has its own `VmTextHeap`, so a `StringArena` opened
  on one thread never catches another thread's strings, and its own literal cache. At most 64 arenas; a 65th
  thread shares the last one behind its lock, and that arena's `StringArena` scopes are switched off (two
  threads would release each other's strings).
- **arm64 native:** each thread has its own allocator state (today's layout plus a remote-free head). The main
  thread keeps the data-segment state; a spawned thread takes one from a pool of states that is never unmapped,
  because a block can outlive its thread. Heap routines find the state through a pthread key
  (`pthread_getspecific`, null on the main thread). Pooled chunks are aligned to their 256 KiB size and start
  with their owner's state pointer, so `HeapFree` finds the owner by masking the address. A module without
  `TaskSpawn` emits today's routines byte for byte, so single-threaded programs (PrimeScroll) pay nothing in
  size or startup. The first native step (TODO-5942) put one LSE spin lock around the routines; per-thread states
  replaced it (TODO-5800, docs/spec/vm-design.md, Tasks on threads) and a 4-task allocate-and-free benchmark ran 70
  times faster.
- **Freed by another thread:** a block freed by a thread other than its owner goes on the owner's remote-free
  list (native: a compare-and-swap push with LSE `casal`; `optexe`: a queue behind the arena's lock), which the
  owner drains when it next allocates (native: when a size class's list is empty). A double free found while
  draining faults with the usual message (on `optexe` in the owner's thread, `invalid heap free address`; a
  string block goes back through the owner's `VmTextHeap`, so its size class and arena records stay right).
  A block reallocated by another thread is copied into the caller's arena and the old one freed remotely. Large blocks (their own mapping) are unmapped by whoever frees them.
- **Native thread stacks:** a secondary pthread gets 512 KiB of stack on macOS by default, but an entry frame
  reserves a 1 MiB value stack, so the thread trampoline asks for 8 MiB (`pthread_attr_setstacksize`), like the
  main thread.
- **Strings:** `String` blocks come from these heaps, so `String` is sendable once a block can be freed on any
  thread. `SharedString` keeps its owner count in the text block (the 8 bytes before the text) and counts with
  the atomic opcodes, so it is sendable too: the last owner frees the block on whichever thread it is.

### Sendable Values

There is one concept: a value is **sendable** when another thread may own it. It is structural and inferred; a
user never writes it on a type. Decided 2026-11-09 after a review of what Rust's `Send`/`Sync` cost its users
([What Rust Taught Us](#what-rust-taught-us)).

| Sendable | Not sendable |
| --- | --- |
| `bool`, integers, floats, `string` (an index into the immutable string table), enums | `Reference<T>`, `Pointer<T>` |
| `String`, `vector<T>`, `map<K, V>`, `Maybe<T>`, `Result`, tuples of sendable types | views and carriers (`StringView`, slices) |
| structs whose fields are all sendable | a closure, as a bare closure struct, with a `[ref]` capture |
| every `Function<R, A...>` ([Functions Are Sendable](#functions-are-sendable)) | `Task<T>` |
| `Mutex<T>`, `Atomic<T>`, `Channel<T>`, `SharedString` (an atomic owner count) | |

- **No `Sync`.** A `Reference` never crosses to another thread, so no value is ever read by two threads through
  references. Sharing happens only through `Mutex`, `Atomic` and `Channel`, which are sendable handles; copying one
  of them shares the cell. That is why one concept is enough.
- **Checked only at the boundary:** every argument of a spawned call, by the concrete type of the callee parameter
  that receives it (implemented, TODO-5790, `src/semantics/SemanticsSendable.{h,cpp}`); the element type of
  `Mutex<T>` and `Channel<T>`; and a closure converted to `Function` (implemented). There are no sendability bounds in generic
  signatures: templates are monomorphized, so the check runs on the concrete instance, and when the offending type
  came in through a template argument the diagnostic carries the instantiation chain.
- **`mut` arguments are allowed.** A spawn argument is copied; the task and the spawner each have their own value
  (a borrowed `[T mut]` parameter is rejected at a spawn, [Captures And Shared State](#captures-and-shared-state)).
  A diagnostic for a non-sendable argument carries the note `sharing between tasks needs Mutex,
  Atomic or Channel`.
- **`[sendable]`** on a struct asserts that it is sendable although it holds a `Pointer`: the stdlib uses it on
  `String`, `Vector` (and through it `map`), `RingBuffer` and `SoaColumn`, and on `Mutex`, `Atomic` and
  `Channel`, whose pointers are owned by the value (or counted). The check still looks through those
  pointers at the element type, so `vector<StringView>` is not sendable.
  Outside the stdlib it needs `unsafe` on the definition, like every other unchecked promise. There is no opt-out
  marker (`[not_sendable]` or similar) until a real case needs one: a struct of sendable fields with a reason not to
  cross threads has not come up, and the native UI handles, the one candidate, are checked at run time instead.
- **UI handles are not marked.** AppKit objects belong to the main thread, but a `Window` is a `u64` handle and so
  sendable. Instead of a type marker, every `ps_ui_*` entry point faults when called off the thread that called
  `ps_ui_init` (`docs/NativeUiPlan.md` already states the rule): `ui call off the main thread: ps_ui_window_add_tab`.
  That is a runtime check of the native UI lane, TODO-5808, with a positive and a negative test on the headless
  backend.
- A `String` made while a `StringArena` is open cannot be a spawn argument or be sent: `string escapes its arena
  via spawn` (or `via channel`), next to the existing return, outer-binding and container cases.

```prime
[unsafe sendable struct]
PixelRows() {
  [Pointer<u8>] bytes
  [i32] stride{4i32}
}

[return<i32>]
stride_of([PixelRows] rows) {
  return(rows.stride)
}

[effects(task), return<int>]
main() {
  [u8 mut] byte{1u8}
  [PixelRows] rows{PixelRows{[bytes] location(byte)}}
  [Task<i32>] task{[spawn] stride_of(rows)};
  [i32] stride{wait(task)}
  return(stride)
}
```

Without `unsafe` the struct is `sendable transform outside the stdlib requires unsafe on /PixelRows`; without
`sendable` the spawn is `spawn argument `rows` of /stride_of is not sendable: field `bytes` of PixelRows is
Pointer<u8>; sharing between tasks needs Mutex, Atomic or Channel`.

**Diagnostics name the path to the leaf.** Rust's `*mut u8 cannot be sent between threads safely` names the leaf
type and leaves the search for which field, in which struct, to the reader; PrimeStruct's messages name the
argument or element, then each field on the way to the offending type:

```text
spawn argument `buffer` of /fill is not sendable: field `data` of Buffer is Pointer<u8>
spawn argument `job` of /render is not sendable: field `rows` of Job, field `bytes` of PixelRows is Pointer<u8>
spawn argument `view` of /measure is not sendable: it is StringView
spawn argument `step` of /run<closure> is not sendable: capture `total` is Reference<i32> (instantiated from /main)
spawn argument `value` of /consume<ReadSlice<i32>> is not sendable: it is ReadSlice<i32> (instantiated from
  /relay<ReadSlice<i32>>, instantiated from /main)
closure cannot be a Function: capture `view` is StringView
Mutex element Record is not sendable: field `next` is Reference<Record>
Channel element Slice<i32> is not sendable
```

The spawn and `Function` lines are the messages of today's checks (each spawn line ends with `; sharing between
tasks needs Mutex, Atomic or Channel`); the `Mutex` and `Channel` lines are the messages of `Mutex<T>` and
`Channel<T>`. Every field on the path names its struct; a closure's fields are its captures; an element of a
builtin container is `element of vector<StringView>`. The instantiation chain names the definition that first
requested each template instance; it carries no line yet, because the message text cannot map a position of the
expanded source back to its file.

### Functions Are Sendable

Every `Function<R, A...>` is sendable **by construction**: the conversion of a closure to `Function`, explicit
(`Function<i64>{closure}`) or implicit (a closure written where a `Function` is expected), rejects a closure that
captures a `Pointer`, a view (`StringView`, a slice) or any other non-sendable value. A `[ref name]` or `[&]` capture
already cannot be stored in a `Function` (`closure escapes via Function`, `docs/spec/value-lifecycle.md`, Lambdas).
So the error appears where the closure is written, not at a distant spawn site; and routers, `AppRouter`, `WebApp`,
`AppHandlers` and `serveWorkers` keep storing plain `Function`.

```prime
[return<i64>]
apply_twice([Function<i64, i64> mut] step, [i64] start) {
  return(step(step(start)))
}
```

```prime
[effects(heap_alloc), return<Function<i64>>]
length_of([StringView] view) {
  return(Function<i64>{[view]() { return(view.length()) }})
}
```

The second (with `import /std/text/*` and `import /std/text/view/*`) is rejected with `closure cannot be a Function:
capture `view` is StringView` at the closure; capturing a `String` instead is accepted. A route handler that
captures a `Pointer` is rejected the same way where it is written, also for a single-threaded `serve`. A named
closure with a `[ref name]` capture converted to a `Function` reports `capture `name` is Reference<T>`; a `[ref]`
lambda written at the conversion keeps `closure escapes via Function (root: name)`.

- **Rejected: inferring sendability per `Function` instance** from the closures converted to it. A closure added
  anywhere in the program could then break a spawn site far away, with an error that does not point at the cause.
  This is the opposite of Rust's async problem, where a future stops being `Send` because of a value held across an
  `.await` deep inside it and the error surfaces at `tokio::spawn`. Sendable by construction costs one restriction
  (a `Function` cannot hold a pointer or a view) that no example or stdlib handler needs.
- **Where the check goes** (implemented). The lambda-closures pass spells each lambda as a closure struct whose
  field types are template parameters that the factory call infers (`SemanticsValidateLambdaClosures.cpp`); the
  conversion itself (`TemplateMonomorphCallableRewrites.cpp` for implicit sites, the closure pass for
  `Function<R, A...>{...}`) rewrites the closure into `/__fn_box(closure)` while those types are still parameters.
  After template monomorphization every `/__fn_box` instance takes its concrete closure struct, and the validator
  checks that parameter type with the spawn-argument predicate when it validates the call
  (`validateFunctionBoxSendable`, `SemanticsValidatorTaskHandles.cpp`), so struct fields resolve as at a spawn and
  the diagnostic sits at the box's source position, which is the closure's (or the conversion's, for a named
  closure). The loan-based `closure escapes via Function` check stays where it is (`SemanticsValidateSliceLoans.cpp`).
  The box takes the closure as a `copy` parameter: the heap block owns its own copy of every capture, so a `String`
  capture is not freed twice.
- `call` stays `mut` when a capture is `mut`. Sendable means the value may move to another thread, not that two
  threads may call it at once; a handler shared between threads is called inside `Mutex.locked`.

### What Rust Taught Us

Rust is the one widely used language with checked thread safety; its design is right in the large and costly in the
small, and PrimeStruct keeps the former.

| Rust | What it cost | PrimeStruct |
| --- | --- | --- |
| `Send` is an auto trait, inferred structurally | nothing: it worked | kept, as "sendable" |
| `unsafe impl Send for T` | a visible unchecked promise | `[sendable]` under `unsafe` |
| `spawn` requires `'static`; `thread::scope` came in 1.63 (2022) | every shared value went into `Arc` for years | tasks are structured from the start: a spawn argument is a copy and `wait` is required, so no `'static` and no `Arc` |
| `Send` and `Sync` as a pair; `Mutex<T>: Sync iff T: Send` | a second concept and its interaction rules | no cross-thread `Reference`, so no `Sync` |
| `Rc` against `Arc`, `RefCell` against `Mutex` | a choice per value, and a conversion when the choice changes | one `SharedString`, one `Mutex` that shares on copy |
| a future "cannot be sent between threads safely" | the error is at `spawn`, the cause is a value held across an `.await` far away | `Function` is sendable by construction: the error is at the closure |
| `T: Send` bounds through every generic signature | bounds written and propagated by hand | checked on monomorphized instances; the diagnostic carries the instantiation chain |
| `MutexGuard` held across `.await` or leaked | deadlocks and `Send` errors from a guard's lifetime | `locked(body)` has no guard value |

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

[effects(lock), return<void>]
record([Mutex<Tally>] tally, [i64] amount) {
  tally.locked([amount]([Reference<Tally>] state) {
    state.total = state.total + amount
    state.entries = state.entries + 1i32
  })
}

[effects(task, lock, heap_alloc, io_out), return<int>]
main() {
  tally{Mutex<Tally>{Tally{}}}
  [Task<void>] left{[spawn] record(tally, 3i64)}
  [Task<void>] right{[spawn] record(tally, 4i64)}
  wait(left, right)
  [i64] total{tally.locked([]([Reference<Tally>] state) { return(state.total) })}
  print_line(total)
  return(0i32)
}
```

This prints `7`. Implemented on the VM and `optexe` (`stdlib/std/sync/sync.prime`); the library reference is
docs/spec/stdlib-reference.md, Shared state.

- `locked` is generic over the closure type (`locked<Body>([Body mut] body)`), so `body` may borrow the caller's
  locals with `[ref name]`: it runs inline, on the calling thread. `locked` returns what `body` returns. A `[ref]`
  closure may be written in the call or bound to a local and passed by name.
- The `Reference<T>` is a borrowed parameter of `body`, so the existing rules (`borrowed parameter escapes via
  return` / `via assignment`) keep it inside. As implemented: a body whose parameter is the value type borrowed
  (`[vector<i64>] values`) and returns it gets `borrowed parameter escapes via return: values`; a body that
  returns its `Reference<T>` parameter returns a copy of the value, and a `Reference<T>` binding of the result is
  refused (`Reference bindings require location(...)`). Storing the reference into a struct field a `[ref]`
  capture reaches is not diagnosed yet (TODO-5853).
- **Copying a `Mutex` shares it:** the handle carries an atomic count, and the last copy destroys `T`. A spawn
  argument is a copy, which is how a task gets the mutex.
- `T` must be sendable. Locking a mutex again inside its own `locked` body faults with `mutex locked again by the
  task that holds it` (the VM and `optexe` record the holder). Two mutexes locked in opposite orders deadlock; the
  VM and `optexe` report it (`deadlock: every task is waiting`, exit code 3). On `optexe` the program counts its
  running and its waiting threads, and the last running one that has to wait faults (docs/spec/vm-design.md,
  Tasks on threads).
- The lock is two IR opcodes, `MutexLock` and `MutexUnlock` (docs/spec/vm-design.md, Mutexes), on a heap slot
  of the mutex's control cell; the unlock is the `Destroy` of a local of `locked`. On `optexe` an uncontended
  lock or unlock is one atomic operation on that slot and a waiter sleeps on a condition variable, where the
  first sketch said `std::mutex`: the slot has to hold the holder for the locked-again fault either way.
- Each handle also keeps the value it was made from (the struct's `initial` field), so a `Mutex<T>` handle is as
  large as a `T` plus two pointers and a `T` is destroyed with each handle (TODO-5854).
- Effect `lock`; making one needs `heap_alloc`. `RwLock<T>` follows the same shape later, with `read(body)` taking
  a read-only `T` (TODO-5804).

### Atomic

`Atomic<i32>` and `Atomic<i64>` are shared integer cells:

```prime
import /std/sync/*

[effects(atomic), return<void>]
count_hits([Atomic<i64>] hits, [i32] times) {
  [i32 mut] i{0i32}
  while(i < times) {
    hits.add(1i64)
    i = i + 1i32
  }
}

[effects(atomic, heap_alloc, io_out, task), return<int>]
main() {
  hits{Atomic<i64>{0i64}}
  [Task<void>] first{[spawn] count_hits(hits, 1000i32)}
  [Task<void>] second{[spawn] count_hits(hits, 1000i32)}
  wait(first, second)
  print_line(hits.load())
  return(0i32)
}
```

This prints `2000`. Implemented on the VM and `optexe` (`stdlib/std/sync/sync.prime`); the library reference is
docs/spec/stdlib-reference.md, Shared state.

- `load()`, `store(value)`, `add(delta)` and `exchange(value)` (both return the previous value) and
  `compareExchange(expected, desired)` (returns whether it stored). Sequentially consistent only. `add` wraps.
- Copying shares the cell, as for `Mutex`: an `Atomic` is a handle of a heap cell that holds the integer and the
  number of handles, itself counted atomically, and the last handle destroyed frees the cell. The type is
  `[sendable]`, so it is a spawn argument, and a spawned call that takes one runs as a task. Effect `atomic`;
  making one needs `heap_alloc`.
- A method called in a definition without the effect is `atomic operation requires atomic effect: <method> in
  <definition>`. Any other element type is `Atomic requires i32 or i64: Atomic<f32>`.
- **IR:** `AtomicLoad`, `AtomicStore`, `AtomicAdd`, `AtomicExchange` and `AtomicCompareExchange` on a heap address,
  32 or 64 bits wide (PSIR v35; docs/spec/vm-design.md, Atomics). `optexe` uses `std::atomic_ref`, and the VM does
  plain operations, each a switch point under a schedule seed. arm64 native will use LSE (`ldar`/`stlr`,
  `ldaddal`, `swpal`, `casal`; every Apple arm64 CPU has it); until then native, Wasm and GLSL reject a module
  that has these opcodes: `atomic operations are not supported by the native backend`.
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
- Implemented on the VM and `optexe` (`stdlib/std/sync/sync.prime`; the library reference is
  docs/spec/stdlib-reference.md, Shared state). A `Channel` is a handle of a control cell (the lock, the number of
  handles, two conditions, the ring's head and count, the closed flag and the capacity) and a ring of `capacity`
  uninitialized `T` slots: `send` moves the value into a slot and `receive` moves it out, so an item is never
  copied on its way, and the last handle destroyed destroys the items still queued. The handle keeps no value of
  its own. A sender that waits on a full channel when it is closed faults like a send after the close.
- It is stdlib code over the lock of `Mutex` and two condition waits. A wait is the `ConditionWait` opcode, which
  gives up the lock and parks the task on a heap slot, and `ConditionNotify` wakes every task parked on a slot
  (PSIR v37; docs/spec/vm-design.md, Conditions): the VM's scheduler keeps the parked tasks, `optexe` counts the
  notifies in the slot and sleeps on a `std::condition_variable` until the count changes, and native will bind `pthread_cond_*` from libSystem (the native backend
  already binds libSystem functions through dyld pointer slots). A woken task takes the lock again and tests its
  condition again, so a wake-up without cause is harmless. A task parked on a condition counts as waiting for the
  deadlock fault (`deadlock: every task is waiting`) on both backends.
- A method called in a definition without the effect is `channel operation requires channel effect: <method> in
  <definition>`. An element that is not sendable is `Channel element Pointer<u8> is not sendable`, and sending a
  `String` made in an open `StringArena` is `string escapes its arena via channel (arena: <name>)`.

### Effects

| Effect | Needed for |
| --- | --- |
| `task` | `[spawn]` and `wait` (unchanged) |
| `atomic` | operations on an `Atomic` |
| `lock` | `Mutex.locked` (and later `RwLock`) |
| `channel` | `send`, `receive` and `close` on a `Channel` |

Making a `Mutex`, `Atomic` or `Channel` also needs `heap_alloc`. `lock` and `channel` are the effects that can
block on another task; merging them is open question 1.

### The Server

Implemented on `optexe` (docs/spec/stdlib-reference.md, Workers): `/std/http/server` has
`serveWorkers(handler, config)` and `serveWorkersUntil(handler, config, stop)`, and
`ServerConfig` has `workers` (0 means one per core, from `/std/os` `cpuCount()`). The name mirrors
`serve`/`serveUntil` and says what differs, and the thread count joins the other limits in `ServerConfig`
instead of being a bare number argument.

```prime
[return<int> effects(heap_alloc, io_err, net_io, task, lock, channel, atomic) on_error<ServeError, log_serve_error>]
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
  `WebApp<State>`, `AppRouter<State>` and `Router` apps need no change beyond the call and the effects `task`,
  `lock`, `channel` and `atomic` in `main`; their
  route tables store plain `Function` handlers, which are sendable by construction, and keep their literal names in
  the sendable `LiteralText`, so the app is sendable when its `State` is. The workers are not generic: the server
  erases the handler into a `Function` and locks a `Mutex` of that, so an app that is not sendable is rejected by
  the `Function` rule (``closure cannot be a Function: capture `inner`, field `state` of ...``). A handler that captures a `Pointer` is already rejected where it is written, under `serve` too.
- **What runs in parallel:** receiving, parsing, serializing the response and sending. What does not: everything
  the handler does, including writing the page's HTML, which happens inside handlers. A handler that does heavy
  work therefore gains little until handlers that only read the state share a read lock (TODO-5804); the server
  leaves measure both cases.
- The poller watches `/std/os` `stopSignalFd()`, a pipe the signal handler writes to, so `SIGINT` or `SIGTERM` ends
  its `poll` at once on whichever thread the signal lands, at any moment from startup on. Worker threads also block
  both signals (`blockStopSignals`), so their reads and writes are not interrupted. On stop the poller stops
  listening, closes the connections that wait for input and closes the job channel; each worker finishes the
  request it is answering, and then the call returns.
- **The slots' bytes** are one block that the poller and the workers share through a `[sendable]` struct holding
  its pointer; a slot's bytes belong to whoever holds the slot. Both channels hold one entry per slot, so neither
  side ever waits in `send`.
- **The VM** cannot run it: `/std/net` passes `Pointer<u8>` buffers to host functions, which the VM rejects
  (TODO-5862).
- **Rejected: several processes on one port** (`SO_REUSEPORT`, N copies of the binary). Each process would have its
  own state: a POST would change one copy of the todo list and the next GET could read another. It only fits
  stateless apps, and the stdlib's app model keeps state in the process (`docs/WebAppsPlan.md`, principle 2).
- **Later, filed but deferred:** read-only handlers under `RwLock` (TODO-5804), keep-alive (TODO-5805), sessions
  (TODO-5806).

### Open Questions

Each needs the user's decision; the recommendation is the plan's default. Decided 2026-11-09 and recorded in
[Sendable Values](#sendable-values): one inferred "sendable" concept and no `Sync`; every `Function` is sendable by
construction, so there is no `SendableFunction` and the routers keep plain `Function`; `[sendable]` outside the
stdlib needs `unsafe`; `mut` spawn arguments are copied, not rejected; UI handles carry no marker and the `ps_ui_*`
entry points check the thread at run time.

1. **Effect names.** Recommended: four effects, `task`, `atomic`, `lock` and `channel`, as fine-grained as the I/O
   effects. The alternative is one `blocking` effect for `lock` and `channel`.
2. **Sharing a primitive.** Recommended: copying a `Mutex`, `Atomic` or `Channel` shares it (atomic handle
   count), so a spawn argument just works. The alternative is an explicit `share()` and non-copyable handles.
3. **Threads.** Recommended: one OS thread per task now, no pool, because tasks block on locks and channels.
4. **Handlers serialized in the first server.** Recommended: accept that page rendering runs one request at a time
   until TODO-5804, since it keeps every existing app correct with no change.
5. **How a handler says it only reads.** Recommended: by its state parameter, `[State] state` (a read-only borrow)
   against `[Reference<State>] state`, which needs overloads by `Function` type (TODO-5735); the alternative is a
   separately named registration such as `app.view(pattern, handler)`.
6. **VM default schedule.** Recommended: eager (today's order) by default, seeds on request; the alternative is a
   fixed seed by default, which changes the output order of existing programs that print from tasks.
7. **A borrowed `[T mut]` parameter at a spawn.** Parameters borrow (`docs/spec/value-lifecycle.md`), so "a spawn
   argument is a copy" needs either a task frame that copies every argument, after which the callee's writes land
   in the task's copy, or a rule against the borrow. Until decided (found 2026-11-10 with TODO-5790), the check
   rejects a place passed to a borrowed `mut` parameter and points at `[T copy mut]`, which is the least committal:
   lifting the rule later changes no accepted program.

## First Implementation Checklist

The first implementation slices now cover:

- parse `[spawn]` as an execution transform on call envelopes
- require `effects(task)` for functions that use `[spawn]` or `wait`
- introduce internal `Task<T>` type facts
- infer `wait(Task<T>) -> T`
- track task handle states through statements and returns
- reject double wait, escaped handles, and missing waits before return
- reject task-handle arguments by name, and every argument whose parameter
  type is not sendable by type ([Sendable Values](#sendable-values));
  `mut` bindings are ordinary arguments
- reject a closure converted to `Function` whose captures are not sendable
  ([Functions Are Sendable](#functions-are-sendable))
- add positive and negative semantic tests
- add semantic-product coverage for task facts
- add first VM/native single-task runtime execution
- integrate multi-task `wait(left, right, ...)` with stdlib tuple values

Remaining implementation work:

- parallel tasks, thread-safe heaps, `Mutex`, `Atomic`,
  `Channel` and the multi-core server: [Threads For Real](#threads-for-real),
  lane Multithreading in `docs/todo.md`
- task groups, detached tasks and scheduler controls: not designed yet

This document is a prototype note. The `[spawn] f(...)`, `wait(task)`, and
`effects(task)` spellings are now parser/source-lock commitments, and the
semantic and VM/native runtime rules above are implemented for single-task wait
and tuple-returning multi-wait.
