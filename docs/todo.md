# PrimeStruct TODO Log

## Purpose

This file is the live open-work queue for PrimeStruct.

- Keep only open work here: `[ ]` queued or `[~]` in progress.
- Move completed work to `docs/todo_finished.md` (below its marker), then run
  `python3 scripts/archive_todo_finished.py`: it files the block verbatim under
  `docs/todo_archive/` and regenerates the index (`grep TODO-NNNN
  docs/todo_finished.md` finds the archive file).
- Do not keep completed-task summaries, historical rollout notes, or closed
  coverage snapshots in this file.
- When this file has no task blocks, the tracked TODO queue is empty.

## Operating Rules

1. Use one task block per item with a stable `TODO-XXXX` ID.
2. Every active leaf must be implementable by someone arriving with no session
   context, including an AI agent.
3. Every active leaf must include `owner`, `created_at`, `scope`,
   `acceptance`, and `stop_rule`.
4. Prefer small, testable leaves over broad epics; split work before starting
   when acceptance cannot be verified in one bounded change.
5. Every active leaf must target at least one value outcome:
   - user-visible behavior change
   - measurable perf/memory improvement
   - deletion of a real compatibility subsystem
6. Avoid standalone micro-cleanups unless bundled into a value outcome.
7. If a leaf misses its value target after two attempts, archive it as
   low-value and replace it with a different hotspot.
8. Keep `Ready Now`, `Immediate Next 10`, `Priority Lanes`, `Execution Queue`,
   and task blocks synchronized when adding, splitting, completing, or deleting
   a task.
9. Keep `Ready Now` capped at eight active leaf tasks.
10. Keep active work leaf-shaped: queue sections must not contain umbrella,
    tracker, phase, research-shaped, or "continue with another slice" items.
11. For parallel work, each `Ready Now` item must name a `parallel_track` and a
    primary surface. Do not put two same-track successors in `Ready Now` unless
    their task blocks prove they touch different source/test surfaces.
12. Treat disabled tests as debt: each retained `doctest::skip(true)` cluster
    must map to an active TODO leaf with a re-enable-or-delete outcome, or be
    removed once proven stale.
13. Treat failing release-test cases as the top priority queue item: before
    starting new implementation work, update `docs/failing_tests.md`, fix the
    oldest reproducible failure first, and keep `docs/todo.md` aligned with the
    active test-fix work.
14. Every release test run must record any failing cases in
    `docs/failing_tests.md` before broader work continues.
15. When completing a task, mark it `[x]`, add `finished_at` plus a short
    evidence note, move the full block below the marker in `docs/todo_finished.md`,
    run `scripts/archive_todo_finished.py` (moves it into
    `docs/todo_archive/<YYYY-MM>.md` and regenerates the index), and remove
    it from this file.
16. This file is read fresh by an LLM agent each session, not browsed by a
    human - optimize for grep-ability over prose. Keep each task block to
    its current state (scope/acceptance/stop_rule), not a narrative history.
    Dated investigation/progress notes go in `docs/todo_log.md` instead,
    under that task's own `## TODO-XXXX` heading - when a task closes, fold
    whatever's still relevant into its `docs/todo_finished.md` resolution
    note and delete its `docs/todo_log.md` section.
17. Every active leaf must set `status` to exactly one of `ready` (its own
    repro/acceptance gap is confirmed and nothing external blocks starting),
    `blocked` (a specific, currently-`[ ]` `TODO-XXXX` must close first -
    name it in `blocked_on`; if that TODO is actually closed, the leaf is
    not blocked - fix the status instead of leaving it stale), or `deferred`
    (deprioritized, needs further scoping, or of confirmed-low value, with
    no single external blocker). Only `ready` leaves belong in `Ready Now`.

## Task Template

```md
- [ ] TODO-<id>: Short title
  - owner: ai|human
  - status: ready|blocked|deferred
  - blocked_on: TODO-XXXX (required when status: blocked; omit otherwise)
  - created_at: YYYY-MM-DD
  - phase: Group/Phase name (optional)
  - parallel_track: short-track-name (required when listed in Ready Now)
  - depends_on: TODO-XXXX, TODO-YYYY (optional)
  - scope: ...
  - implementation_notes: optional, but required when source/test entry points are not obvious
  - acceptance:
    - ...
    - ...
  - stop_rule: ...
  - notes: optional
```

Dated investigation history for this task goes in `docs/todo_log.md` under
a matching `## TODO-<id>` heading, not inline here.

## Open Tasks

### Queue Summary

Generated from each task block's own `status`/`parallel_track` fields -
re-derive after editing any block rather than hand-editing this table out
of sync with them.

| ID | Title | Status | Track |
| --- | --- | --- | --- |
| TODO-5738 | Sum results return through the caller's block in real calls | ready | ir-real-calls |
| TODO-5739 | Large struct copies lower to a slot loop | ready | ir-struct-copies |
| TODO-5752 | x86_64 native and the Linux JIT take the address ABI of real calls | deferred | native-real-calls |
| TODO-5741 | `string` parameters become real calls on the VM and optexe | ready | ir-real-calls |
| TODO-5824 | A null or stray local address faults on the VM | deferred | vm-addresses |
| TODO-5727 | Every struct copy site tells a place from a temporary by resolution | deferred | lifecycle-copy-sources |
| TODO-5707 | `Function` types accept `mut` parameters | deferred | closures-mut-params |
| TODO-5708 | Type aliases | deferred | type-aliases |
| TODO-5745 | A labeled value that is a call parses in every position | ready | parser-labeled-values |
| TODO-5746 | A parameterless `mut` method whose body is only bindings is a method | ready | diagnostics |
| TODO-5688 | Calls require the callee's whole active effect set | deferred | effects-transitive |
| TODO-5836 | A spawned definition without a `return` transform is a `Task<void>` | deferred | mt-vm-tasks |
| TODO-5837 | VM debug sessions step programs that spawn tasks | deferred | vm-debugger |
| TODO-5840 | A byte loop over one large region on `optexe` runs as fast as before the granule table | ready | optexe-bytes |
| TODO-5844 | Short-string `optexe` benchmarks take the cycles they took before the heap arenas | ready | optexe-bytes |
| TODO-5847 | A spawned call with a collection or a `copy` parameter runs as a task | ready | mt-vm-tasks |
| TODO-5848 | `return(wait(task))` returns a `String` task result | ready | mt-wait-types |
| TODO-5849 | A destructured multi-wait keeps each result's own type | ready | mt-wait-types |
| TODO-5850 | A call that returns a `Reference` is a closure argument | ready | closures-references |
| TODO-5851 | A `[ref]` closure written in a generic call infers the closure type | ready | closures-references |
| TODO-5852 | `wait(a, b)` takes `Task<void>` handles | ready | mt-wait-types |
| TODO-5853 | A borrowed `Reference` parameter stored through a `[ref]` capture is diagnosed | deferred | closures-references |
| TODO-5854 | A `Mutex` or `Atomic` handle does not keep the value it was made from | deferred | mt-primitives |
| TODO-5861 | `SharedString` keeps its owner count inside the text block | ready | string-concurrency |
| TODO-5862 | `serveWorkers` serves on the VM with 2 workers | deferred | mt-server |
| TODO-5863 | A struct named without its namespace is a `Channel` element | ready | mt-primitives |
| TODO-5864 | A lambda in a generic definition names the definition's type parameters | ready | closures-generics |
| TODO-5865 | A `Function` local given positionally to a brace constructor is copied | ready | closures-lifecycle |
| TODO-5868 | `serve(app)?` in an `if` block of `main` compiles | ready | result-question |
| TODO-5871 | `serveWorkers` answers more new connections a second than one thread can accept | ready | mt-server |
| TODO-5872 | A thread reads the bytes of a block another thread allocated as fast as its own | ready | mt-heap |
| TODO-5873 | A sum temporary passed as an argument is destroyed after the call | ready | pick-payloads |
| TODO-5874 | Assigning to a `mut` sum local destroys the payload it held | ready | pick-payloads |
| TODO-5875 | `count()` on a local declared `Vector<T>` lowers | ready | pick-payloads |
| TODO-5876 | `return(pick(...))` of a vector and assignment from a `pick` expression | ready | pick-payloads |
| TODO-5881 | A call takes a call to the same template instantiation as its argument on the VM | ready | pick-payloads |
| TODO-5878 | A `[move name]` lambda capture compiles | ready | pick-payloads |
| TODO-5870 | A VM program that uses tasks runs its string code on the fast kernel | ready | vm-tasks |
| TODO-5799 | arm64 native runs spawned tasks on pthreads, its heap behind a lock | ready | mt-native |
| TODO-5800 | arm64 native heaps are per thread, with remote frees | blocked | mt-native |
| TODO-5801 | arm64 native runs `Atomic`, `Mutex` and `Channel` | blocked | mt-native |
| TODO-5804 | GET handlers that only read the state run at the same time under `RwLock` | deferred | mt-server |
| TODO-5805 | The server keeps connections alive between requests | deferred | web-keepalive |
| TODO-5806 | Sessions: a cookie and per-session state | deferred | web-sessions |
| TODO-5821 | The first toolbar of a launch does not hold up the first frame | deferred | startup-toolbar |
| TODO-5603 | The real-launch baseline, measured by a person | ready | startup |
| TODO-5586 | Native String heap: pooled blocks, statistics and limit | deferred | native-backend-strings |
| TODO-5536 | Small, lazily loaded bytecode for apps | deferred | bytecode-startup |
| TODO-5535 | Launch-time harness and reference Objective-C++ editor | ready | native-ui-launch |
| TODO-5808 | `ps_ui_*` entry points fault when called off the main thread | ready | native-ui-threads |
| TODO-5533 | Windows and Linux native UI backends | deferred | native-ui-platforms |
| TODO-5712 | A callable field of a local struct is called | ready | closures-field-calls |
| TODO-5713 | A lambda whose capture list starts with `mut` parses as a call argument | ready | closures-parse |
| TODO-5714 | Native: a `Function` call nested in another call's arguments lowers | ready | native-backend |
| TODO-5715 | `Function<R, A...>{move(closure)}` infers the boxed type | ready | closures |
| TODO-5716 | A literal converts to a `StringView` at a call whose overloads differ there | deferred | text-literal-views |
| TODO-5717 | One file imports two modules that export the same name | ready | import-conflicts |
| TODO-5718 | All values of a repeated parameter and decoded path parameters | deferred | web-params |
| TODO-5719 | `location(this.field)` passes directly as a `Function` call argument | ready | closures-arguments |
| TODO-5721 | A string converts to a `StringView` at a field of a generic struct | ready | text-literal-views |
| TODO-5726 | An indexed element of a stdlib `Vector<T>` of structs binds without a fault | ready | collections-access |
| TODO-5728 | `slice` and `sliceMut` on a vector field of a struct resolve | ready | collections-access |
| TODO-5730 | Elements of a `vector<String>` struct field read without a fault | ready | collections-access |
| TODO-5731 | A `string` field of an element whose closure captures reads without a fault | ready | closures |
| TODO-5732 | A method call on a reference parameter resolves in a generic struct's method | ready | generic-methods |
| TODO-5733 | An `i32` field of a later element of a vector of handler structs reads without a fault | ready | closures |
| TODO-5735 | Same-arity overloads of struct methods resolve by argument type | ready | method-overloads |
| TODO-5736 | `pick` and sum parameters in a generic struct's method lower | ready | generic-methods |
| TODO-5737 | Implicit template arguments infer through a generic struct parameter | ready | template-inference |
| TODO-5682 | Native: a mutating vector-field method inside an arithmetic expression returns the right value | ready | native-backend |
| TODO-5676 | Lambdas can state their return type | deferred | closures-return-types |
| TODO-5677 | A `Function` whose closure holds a same-typed `Function` copies and destroys | deferred | closures-lifecycle |
| TODO-5510 | `return` returns from unit-variant pick arms correctly | ready | control-returns |
| TODO-5524 | Int-backed error structs round-trip through stdlib Result sums | ready | result-error-structs |
| TODO-5523 | A Result-returning main exits with its error code | deferred | result-main |
| TODO-5525 | `Result.ok(x)` passes as a stdlib Result argument | deferred | result-arguments |
| TODO-5526 | Vectors of stdlib Result values keep their elements | deferred | result-containers |
| TODO-5511 | Safe code cannot reach container storage or unsafe stdlib helpers | deferred | safety-stdlib |
| TODO-5512 | Pointers and aliases count as borrows of their root | deferred | safety-borrows |
| TODO-5513 | Methods through a dereferenced vector pointer read the right fields | deferred | collections-access |
| TODO-5515 | Native Result.ok(Buffer) reads as ok | deferred | native-result |
| TODO-5516 | Integer narrowing and float-to-int conversion agree across backends | deferred | numeric-conversions |
| TODO-5517 | Runtime faults exit the same way on every backend | deferred | runtime-faults |
| TODO-5744 | A runtime error in a real call ends the program on Wasm and GLSL | deferred | runtime-faults |
| TODO-5518 | Valid programs the frontend rejects compile | deferred | frontend-accept |
| TODO-5519 | Operator rewriting handles calls, nested `!` and `?` precedence | deferred | text-filter |
| TODO-5520 | Diagnostics point at user code with readable names | deferred | diagnostics |
| TODO-5521 | Math helpers return correct results | deferred | stdlib-math |
| TODO-5483 | Verify arm64 SextI32 on a macOS machine | deferred | ir-semantics |
| TODO-5543 | Repair or retire the disabled Apple/arm64 native backend test shards | deferred | native-arm64-tests |
| TODO-5544 | `Type.staticHelper()` resolves through `import` | deferred | pick-bindings |
| TODO-5545 | Record the macOS AppKit manual smoke checklist | deferred | native-ui-macos-manual |
| TODO-5841 | `[namespace] name { ... }` is a definition | ready | namespace-envelope |
| TODO-5843 | `namespace name { ... }` is sugar for the `[namespace]` definition | blocked | namespace-envelope |
| TODO-5842 | `summary("...")` is a checked transform on definitions | blocked | summary-transform |

### Ready Now

- TODO-5603 (startup): the real-launch baseline, measured by a person
- TODO-5535 (native-ui-launch): launch-time harness and reference Objective-C++ editor
- TODO-5510 (control-returns): `return` returns from unit-variant pick arms correctly
- TODO-5524 (result-error-structs): int-backed error structs round-trip through stdlib Result sums
- TODO-5841 (namespace-envelope): `[namespace] name { ... }` is a definition

### Immediate Next 10

1. TODO-5603
2. TODO-5535
3. TODO-5510
4. TODO-5524
5. TODO-5728
6. TODO-5841

### Priority Lanes

- Startup, first (docs/NativeUiPlan.md section 1, "Where a launch goes"): TODO-5603, TODO-5535, TODO-5821
- Native PrimeScroll, direct native backend (docs/NativeUiPlan.md): TODO-5586
- Native UI (docs/NativeUiPlan.md): TODO-5808, TODO-5536, TODO-5533
- Effects (docs/spec/language-core.md, Effects): TODO-5688
- AI workflow (docs/AiWorkflowPlan.md): TODO-5841, TODO-5843, TODO-5842
- Result and control flow (docs/spec/errors-and-file-io.md): TODO-5510, TODO-5524, TODO-5515, TODO-5523, TODO-5525, TODO-5526, TODO-5868, TODO-5873, TODO-5874, TODO-5875, TODO-5876, TODO-5878, TODO-5881
- Memory safety (docs/spec/type-system.md Memory safety): TODO-5511, TODO-5512, TODO-5513
- Backend parity: TODO-5516, TODO-5517, TODO-5744
- Frontend and diagnostics: TODO-5518, TODO-5519, TODO-5520, TODO-5521
- macOS support: TODO-5543, TODO-5545 (need macOS)
- Language gaps found by native UI: TODO-5544, TODO-5745, TODO-5746
- Closures (docs/spec/value-lifecycle.md, Lambdas): TODO-5676, TODO-5677, TODO-5712, TODO-5713, TODO-5714, TODO-5715, TODO-5719, TODO-5850, TODO-5851, TODO-5853, TODO-5864, TODO-5865; collection fields found by closures and the todo example: TODO-5682, TODO-5726, TODO-5727, TODO-5728, TODO-5730, TODO-5731, TODO-5733
- Web apps (docs/WebAppsPlan.md; built with `--emit=optexe`): TODO-5738, TODO-5718
- Language surface for handlers (docs/WebAppsPlan.md): TODO-5707, TODO-5708, TODO-5716, TODO-5717, TODO-5721, TODO-5732, TODO-5735, TODO-5736, TODO-5737
- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix; arm64 SextI32 TODO-5483 (needs macOS); VM speed ; passes ; optexe byte loops TODO-5840, TODO-5844; real calls and struct copies TODO-5739, TODO-5741, TODO-5752; VM address checks TODO-5824
- Multithreading (docs/MultithreadingPrototype.md, Threads For Real; docs/WebAppsPlan.md, Concurrency; after the startup lane): TODO-5861, TODO-5863, TODO-5799, TODO-5800, TODO-5801, TODO-5847, TODO-5848, TODO-5849, TODO-5852, TODO-5870, TODO-5871, TODO-5872; deferred TODO-5804, TODO-5805, TODO-5806, TODO-5836, TODO-5837, TODO-5854, TODO-5862

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

Multithreading (docs/MultithreadingPrototype.md, Threads For Real), after the startup lane (TODO-5772 is done); not above them in Ready Now or Immediate Next 10. In this order (TODO-5792 and TODO-5835, VM tasks and their lowering, are done, and so are TODO-5793, byte-region checks without a map search, TODO-5794, `optexe` per-thread heap arenas, TODO-5795, `optexe` threads for `[spawn]`, `Atomic<i32>` and `Atomic<i64>` in `/std/sync`, TODO-5797, `Mutex<T>` with `locked(body)`, TODO-5846, `optexe` faults when every task waits, TODO-5798, bounded `Channel<T>`, TODO-5656, the atomic `SharedString` owner count, TODO-5802, `serveWorkers`, and TODO-5803, the apps on workers, measured): TODO-5861 (`SharedString` count inside the text block), then arm64 native: TODO-5799 (pthreads, locked heap), TODO-5800 (per-thread heaps), TODO-5801 (primitives). TODO-5861 and TODO-5799 are ready and independent of each other, and so are TODO-5848 (`return(wait(t))` of a `String` task result is rejected as a return type mismatch), TODO-5849 (a destructured multi-wait of tasks with different result types loses the later results' types) and TODO-5847 (spawned calls with collection or `copy` parameters or collection results run as tasks, which today run inline; TODO-5845, the same for `String`, struct and closure values, is done) (TODO-5807, destructured multi-wait, TODO-5790, sendable spawn arguments, and TODO-5791, every `Function` sendable by construction, are done). Deferred: TODO-5804 (read-only handlers under `RwLock`), TODO-5805 (keep-alive), TODO-5806 (sessions), TODO-5854 (a `Mutex` or `Atomic` handle keeps a copy of its first value). Found with `Mutex` and ready: TODO-5852 (`wait(a, b)` of two `Task<void>`), and in the closures lane TODO-5850 (a `Reference`-returning call as a closure argument) and TODO-5851 (a `[ref]` closure written in a generic call). Found with `Channel` and ready, in the Result and control flow lane: TODO-5855 (a method call on a `pick` payload of struct type), TODO-5856 (`pick` on a call result frees an owning payload), TODO-5857 (a `vector<T>` payload reads its count in a `pick` arm), TODO-5858 (`some<Vector<i32>>` of a fresh vector frees it), TODO-5859 (a `pick` expression yields a vector), TODO-5860 (moving a payload binding does not poison a later binding of the same name), TODO-5866 (a closure captures a `pick` payload binding), TODO-5877 (a field an arm of a `pick` expression yields is copied), TODO-5879 (a `pick` expression's result owns a value of its own) and TODO-5880 (a payload moved out of a sum local leaves the local moved-from, and a payload of a borrowed sum cannot be moved or returned) are done, and found with them and ready: TODO-5873 (a sum temporary passed as an argument is destroyed after the call), TODO-5874 (assigning to a `mut` sum local destroys the payload it held), TODO-5875 (`count()` on a local declared `Vector<T>` lowers), TODO-5876 (`return(pick(...))` and assignment from a `pick` expression), TODO-5878 (a `[move name]` lambda capture compiles) and TODO-5881 (a call takes a call to the same template instantiation as its argument on the VM). Found with `serveWorkers`: TODO-5862 (deferred: `serveWorkers` on the VM, which has no `/std/net`), and ready: TODO-5863 (a struct named without its namespace as a `Channel` element), TODO-5864 (a lambda in a generic definition names its type parameters), TODO-5865 (a `Function` local given positionally to a brace constructor is copied) and TODO-5866 (a closure captures a `pick` payload binding). Found measuring the apps on workers (TODO-5803, done): TODO-5867 (a string heap that alternates between block sizes keeps its chunks) and TODO-5869 (1 worker as fast as `serve`; the 32 KB form reached 2.41x with 4 workers, not 2.5x) are done; ready: TODO-5871 (every server stops near 14,600 new connections a second, which caps the form with 4 workers), TODO-5872 (a byte of another thread's block is checked on a slow path, so the worker parses from a copy) and, found with TODO-5867, TODO-5870 (a VM program that uses tasks runs all its code on the step kernel, 11 times the instructions for a string loop); and in the Result and control flow lane TODO-5868 (`serve(app)?` in an `if` block of `main` is rejected).

### Task Blocks

- [ ] TODO-5738: Sum results return through the caller's block in real calls
  - owner: ai
  - status: ready
  - created_at: 2026-11-08
  - phase: Web apps (docs/WebAppsPlan.md; found while finishing TODO-5734)
  - parallel_track: ir-real-calls
  - scope: A definition whose result is a sum (`Result`, `Maybe`, a user `[sum]`) is still inlined at every call site on the VM and optcpp/optexe, because the sum return path (`emitReturnSumStorage`) has no real-call form. `/std/http/server` `serve` and `serveUntil` return `Result<ServeError>`, so todo_web's `main` holds both bodies (about 260 KB of its 1.38 MB of C++). Return sums through the caller's result block like structs (docs/spec/vm-design.md, Real calls).
  - acceptance:
    - `serve`/`serveUntil` are real calls in todo_web; a matrix case returns `Result` and `Maybe` values (ok, error, a payload with a `Destroy`) from a real call on every backend
    - full release gate green
  - stop_rule: sum results of real calls only.

- [ ] TODO-5739: Large struct copies lower to a slot loop
  - owner: ai
  - status: ready
  - created_at: 2026-11-08
  - phase: Optimizing backends (found while finishing TODO-5734)
  - parallel_track: ir-struct-copies
  - scope: `emitStructCopyFromPtrs` writes one load/store sequence per slot, which optcpp turns into about eight lines of C++ per slot: `/std/http` `parseRequest` alone has 873 slot copies (a `Request` is about 23 slots). Emit a loop over the slots above a size threshold, on the targets where that is cheaper, and measure size and VM speed.
  - acceptance:
    - todo_web's optcpp output shrinks measurably; VM benchmarks no slower; same results on every backend
    - full release gate green
  - stop_rule: struct slot copies only.

- [ ] TODO-5752: x86_64 native and the Linux JIT take the address ABI of real calls
  - owner: ai
  - status: deferred
  - created_at: 2026-11-09
  - phase: Optimizing backends (split from TODO-5740)
  - parallel_track: native-real-calls
  - scope: Since TODO-5740 the arm64 native backend takes the address ABI of real calls (docs/spec/vm-design.md, Real calls): its locals live at machine addresses in frames that stay in place while callees run, so the lowering sets `realCallAllowStructs` for `IrValidationTarget::Native` on an arm64 macOS host only (src/ir_lowerer/IrLowererLowerSetupStage.cpp). The x86_64 native backend also addresses locals by machine address (rbp-relative), but its register allocator and local promotion were never run with cross-frame addresses, and the Linux JIT translates VM-numbered addresses against the current frame and refuses a function that takes a local address and calls. Enable the address ABI for x86_64 native (checking that address-taken locals stay in memory across calls) and number the JIT's local addresses across frames as the VM does. Deferred: needs a Linux x86_64 machine to run.
  - acceptance:
    - x86_64 native and the JIT run the real-call program-matrix cases (struct, `mut` and Reference parameters, struct results, runtime errors) with real calls; the JIT no longer refuses such modules
    - full release gate green on Linux x86_64
  - stop_rule: local addressing and real calls on x86_64 native and the JIT.

- [ ] TODO-5824: A null or stray local address faults on the VM
  - owner: ai
  - status: deferred
  - created_at: 2026-11-09
  - phase: Optimizing backends (found while checking TODO-5776)
  - parallel_track: vm-addresses
  - scope: A local address is `(base + index) * 16` with the entry frame's base 0 (docs/spec/vm-design.md, Frames & stack), so address 0 is the entry frame's slot 0 and `0 + 16` its slot 1: a null reference reads or writes a live slot on the VM (the gfx `ShaderLibrary.CubeBasic` bug returned 90 instead of faulting), while arm64 native faults on it. Start the numbering at base 1 (or reserve the entry frame's slot 0) in the step kernel, the fast kernel's locals arena and the optcpp frame registry, so an address below the first frame faults with `invalid indirect address in IR` (exit 3). Deferred: the gfx case is fixed at its source and no known program reaches a null local address now.
  - acceptance:
    - `LoadIndirect`/`StoreIndirect` of address 0 and 16 fault with exit 3 on the VM step and fast kernels and on optexe; a program-matrix case pins it, and every program-matrix and compile-run case keeps its result
    - full release gate green
  - stop_rule: local address numbering on the VM and optcpp only; no change to heap addresses or native.

- [ ] TODO-5741: `string` parameters become real calls on the VM and optexe
  - owner: ai
  - status: ready
  - created_at: 2026-11-08
  - phase: Optimizing backends (found while finishing TODO-5734)
  - parallel_track: ir-real-calls
  - scope: A definition with a `string` (module-table text) parameter stays inline, because the inline binding records where the text comes from (`stringSource`) and the body indexes it statically. `String.append([string] text)` is inlined about 40 times in todo_web. Pass the string index and read it with `LoadStringByteDynamic`/`LoadStringLength` in the callee on the VM and optcpp.
  - acceptance:
    - `String.append` with a `string` argument is a real call in todo_web, same output on every backend
    - full release gate green
  - stop_rule: `string` parameters of real calls only.

- [ ] TODO-5727: Every struct copy site tells a place from a temporary by resolution
  - owner: ai
  - status: deferred
  - created_at: 2026-11-07
  - phase: Lifecycle (found while fixing TODO-5720)
  - parallel_track: lifecycle-copy-sources
  - scope: TODO-5720 added `isTemporaryStructCopySourceExpr` (src/ir_lowerer/IrLowererFlowControlHelpers.cpp), which resolves a call before deciding whether a struct value is a place to copy or a temporary to move; bindings, assignments, returns, brace fields and sum payloads use it. Parameter and packed arguments, the collection receiver, sum mapping, `pick` and the method-call form of a sum payload still classify syntactically, so a user-defined `at` method whose result feeds one of them would be copied and its temporary leaked. Route those sites through the same resolver.
  - acceptance:
    - matrix cases with a user `at` method feeding each listed site destroy every value exactly once on VM, native and optexe
    - full release gate green
  - stop_rule: copy-source classification at the listed sites only.

- [ ] TODO-5744: A runtime error in a real call ends the program on Wasm and GLSL
  - owner: ai
  - status: deferred
  - created_at: 2026-11-08
  - phase: Backend parity (found while fixing TODO-5743)
  - parallel_track: runtime-faults
  - scope: TODO-5743 made a runtime error (a bounds check, `panic`) inside a real-call body end the program with `Trap 3` on the VM, optcpp/optexe, `--emit=native` and the Linux JIT. The simple C++ emitters (`cpp`, `exe`), Wasm and GLSL have no `Trap` (IrValidation rejects it there), so `emitRuntimeError` (src/ir_lowerer/IrLowererRuntimeErrorHelpers.cpp) still returns 3 from the callee and the caller carries on: the program of "a runtime error inside a real call stops the program" (tests/unit/program_matrix/test_program_matrix_parameter_modes.cpp) prints `20 30`, the error, `3`, `99` and exits 0 on `--emit=exe` (the slow `exe` matrix config, not in the default gate). Lower `Trap` in Wasm (a WASI `proc_exit`, or keep such bodies inline), and decide GLSL (no calls that can fault, or inline). The simple C++ emitters (`cpp`, `exe`) run `Trap` since TODO-5839.
  - acceptance:
    - a Wasm runtime check of the matrix program exits 3 where wasmtime is available
    - full release gate green
  - stop_rule: runtime errors in real calls on Wasm and GLSL only.

- [ ] TODO-5707: `Function` types accept `mut` parameters
  - owner: ai
  - status: deferred
  - created_at: 2026-11-07
  - phase: Language surface for handlers (docs/WebAppsPlan.md)
  - parallel_track: closures-mut-params
  - scope: Handlers stored in a `Function` take `Reference<Response>` because a `Function<R, A...>` type cannot say that a parameter is `mut`, while a generic handler can take `[Response mut]`. Specify a spelling for a mutable parameter in a `Function` type in docs/spec/value-lifecycle.md and lower it. Deferred: needs the spelling decided.
  - acceptance:
    - a `Router` route lambda takes `[Response mut] response` and runs on VM, native and optexe; spec text with a runnable example
    - full release gate green
  - stop_rule: the parameter mode in `Function` types only.

- [ ] TODO-5708: Type aliases
  - owner: ai
  - status: deferred
  - created_at: 2026-11-07
  - phase: Language surface for handlers (docs/WebAppsPlan.md)
  - parallel_track: type-aliases
  - scope: There is no way to name a type: `Function<void, Request, Reference<Response>>` is spelled out at every route and seven times in /std/http. Specify a public/private alias declaration (docs/spec/type-system.md), with generic aliases if cheap, and use it for the handler type. Deferred: needs the spelling decided.
  - acceptance:
    - an alias of a `Function` type and of a generic struct is used in parameters, fields and construction; a negative test for a cycle; spec text with a runnable example
    - full release gate green
  - stop_rule: alias declarations and their resolution only.

- [ ] TODO-5745: A labeled value that is a call parses in every position
  - owner: ai
  - status: ready
  - created_at: 2026-11-08
  - phase: Language gaps found by native UI (TODO-5710)
  - parallel_track: parser-labeled-values
  - scope: Three shapes fail to parse although their parts parse alone. A labeled brace argument whose value is a call followed by an operator: `Pair{[a] five() + 1i32}` fails with "invalid character: '+'" (`Pair{[a] 4i32 + x}` parses). An assignment whose right side is a brace construction with a call as a labeled value: `window = Win{[handle] h()}` fails with "unexpected token in expression" at `=` (`[Win] w{Win{[handle] h()}}` and `p = Pair{[a] 2i32}` parse). An assignment whose right side passes a lambda whose body starts with a binding: `a = apply([]([i32] x) { [i32] y{x} ... })` fails the same way (a call statement or a binding initializer with that lambda parses). handlers.prime and the editor bind a local first.
  - acceptance:
    - the three shapes parse and run on the VM, with a parser test each
    - full release gate green
  - stop_rule: the parser's labeled-value and assignment paths only.

- [ ] TODO-5746: A parameterless `mut` method whose body is only bindings is a method
  - owner: ai
  - status: ready
  - created_at: 2026-11-08
  - phase: Language gaps found by native UI (TODO-5710)
  - parallel_track: diagnostics
  - scope: A nested definition with no parameters and a body of bindings only is taken as a field-only struct, so `[mut] touch() { [i32] x{1i32} [i32] y{x} }` inside a struct fails with "mut transform is only supported on struct helpers: /Box/touch", which names neither the cause nor the fix. Either a definition with a method transform (`mut`, `return<T>`, `effects`) is never field-only, or the diagnostic says the body was read as a struct's fields (docs/spec/transforms.md).
  - acceptance:
    - the case of tests/unit compiles as a method, or fails with a diagnostic that names the field-only reading; spec text matches
    - full release gate green
  - stop_rule: the field-only rule and its diagnostic.

- [ ] TODO-5688: Calls require the callee's whole active effect set
  - owner: ai
  - status: deferred
  - created_at: 2026-11-06
  - phase: Effects
  - parallel_track: effects-transitive
  - scope: TODO-5687 checks only the effects a callee declares. A callee without `[effects]` runs with the default set (`io_out`), so the full rule would also require the caller to hold the callee's defaults. Measured with the TODO-5687 prototype (semantics defaults `io_out io_err`), the full rule failed 527 of 2840 semantics cases (narrow rule: 47) and 9 of the 42 examples that compile (narrow rule: 1), mostly inside stdlib lifecycle helpers such as `Vector.Copy`/`Destroy` and `SoaColumn.Destroy`, which declare `heap_alloc` only and call unannotated helpers. Decide whether unannotated helpers should infer their effects from their bodies (or default to none) before turning on the full transitive rule.
  - acceptance:
    - a rule for unannotated callees in docs/spec/language-core.md (Effects), stdlib, examples and tests annotated to match, and the full rule enforced in semantics
    - full release gate green
  - stop_rule: call-site effect checks only; no new effects.

- [ ] TODO-5712: A callable field of a local struct is called
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Closures follow-ups (found by TODO-5669)
  - parallel_track: closures-field-calls
  - scope: With `[Route mut] direct{Route{[code] 6i32, [handler] double}}` and `Route` holding `[Function<i32, i32> mut] handler`, `direct.handler(2i32)` (as a statement, a binding initializer or a return argument) fails with `unknown method: /Route/handler`. The same call through an indexed element (`routes[i].handler(x)`) or `this` works (`rewriteCallableFieldCall` in src/semantics/TemplateMonomorphCallableRewrites.cpp), so the receiver of a plain local struct does not resolve there.
  - acceptance:
    - `direct.handler(2i32)` on a local, a parameter and a `Reference` to the struct calls the field on VM, native and optexe (matrix case in test_program_matrix_closures.cpp)
    - full release gate green
  - stop_rule: receiver resolution for callable fields; no change to method lookup.

- [ ] TODO-5713: A lambda whose capture list starts with `mut` parses as a call argument
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Closures follow-ups (found by TODO-5669)
  - parallel_track: closures-parse
  - scope: `router.add(3i32, [mut calls{0i32}]([i32] x) { ... })` fails with `Parse error: reserved keyword cannot be used as identifier: mut`: in argument position `[` starts an argument label. `[base](...)` and `[seed{5i32}](...)` parse, and the same lambda parses inside braces (`Function<i32, i32>{[mut calls{0i32}](...) {...}}`) and as a binding initializer. Since TODO-5669 a lambda passes straight to a `Function` parameter, so this is the remaining reason to bind a stateful handler first (examples/apps/http_hello `/greet`). Disambiguate a label from a capture list when `[` is followed by a parameter list and a body.
  - acceptance:
    - a `mut`-capture lambda and a `[mut x, y]` lambda as call arguments parse and run on VM and optexe; labeled arguments still parse; a parser test for each
    - full release gate green
  - stop_rule: argument-position lambda parsing only.

- [ ] TODO-5714: Native: a `Function` call nested in another call's arguments lowers
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Closures follow-ups (found by TODO-5669)
  - parallel_track: native-backend
  - scope: `apply_twice([Function<i32, i32> mut] f, [i32] x) { return(f(f(x))) }` fails on `--emit=native` with `native backend does not support recursive calls: /__Function1__t.../call`, with the explicit `Function<i32, i32>{...}` too; the VM and optexe return the right value, and `[i32] once{f(x)} return(f(once))` works on native. The native lowering treats a call of `call` inside the arguments of the same `call` as recursion.
  - acceptance:
    - `f(f(x))` on a `Function` parameter returns the right value on native-O0 and native-O2 (matrix case)
    - full release gate green
  - stop_rule: the native recursion check for nested calls of one definition.

- [ ] TODO-5715: `Function<R, A...>{move(closure)}` infers the boxed type
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Closures follow-ups (found by TODO-5669)
  - parallel_track: closures
  - scope: `Function<i32, i32>{move(adder)}` with `adder` a closure local fails with `unable to infer implicit template arguments for /__fn_box`: implicit template inference does not see through `move(...)`. A copy (`Function<i32, i32>{adder}`) works. Once it infers, accept `move(name)` at the implicit conversion sites too (`isClosureArgument` in src/semantics/TemplateMonomorphCallableRewrites.cpp).
  - acceptance:
    - the explicit and the implicit form with `move(adder)` run on VM and optexe, and `adder` is moved-from afterwards (a negative test for a use after the move)
    - full release gate green
  - stop_rule: `move` arguments of `/__fn_box` and the implicit sites.

- [ ] TODO-5716: A literal converts to a `StringView` at a call whose overloads differ there
  - owner: ai
  - status: deferred
  - created_at: 2026-11-07
  - phase: Language surface for handlers (docs/WebAppsPlan.md; found by TODO-5700)
  - parallel_track: text-literal-views
  - scope: A `string` converts to a `StringView` where one is expected (docs/spec/strings-and-views.md), but at a call of same-arity overloads only when every overload takes a `StringView` in that position. With `pick([i32])` and `pick([StringView])`, `pick("y")` reports no viable overload, and even `pick(textOf("y"))` is ambiguous, because overload selection types a nested call argument as unknown (`requirementOverloadArgumentTypeText` in src/semantics/TemplateMonomorphCoreUtilities.cpp). Give a call with a known non-generic return type, such as `textOf`, its type there, then let the conversion pick the `StringView` overload when no overload takes the `string` itself (`TextViewConversion::convert`).
  - acceptance:
    - `pick("y")` and `pick(textOf("y"))` select the `StringView` overload on VM and optexe; `describe([string])` next to `describe([StringView])` still takes the literal as a `string`
    - full release gate green
  - stop_rule: overload argument facts for calls with a declared return type and the literal conversion at such calls; no change to the specificity rule.
  - notes: deferred: no stdlib family overloads `string` against `StringView` or another type today.

- [ ] TODO-5717: One file imports two modules that export the same name
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Language surface for handlers (docs/WebAppsPlan.md; found by TODO-5701)
  - parallel_track: import-conflicts
  - scope: `/std/html` and `/std/json` both export `Writer`. `import /std/html/*` next to `import /std/json/*` (or `import /std/json/Writer`) fails with `import creates name conflict: Writer` (src/semantics/SemanticsValidatorBuildImports.cpp), even when the program names one of them by its full path, and a full path such as `/std/json/Writer` into a module that is not imported is `unsupported binding type`. A web app that serves pages and JSON needs both writers in one file. Report the conflict where the bare name is used instead of at the import, so `/std/html/Writer` and `/std/json/Writer` work side by side; an unqualified use of the shared name stays an error that names both paths.
  - acceptance:
    - a program importing both modules builds an HTML page and a JSON value through their full paths on VM and optexe
    - an unqualified `Writer` in that program is a diagnostic naming `/std/html/Writer` and `/std/json/Writer`
    - docs/PrimeStruct_SyntaxSpec.md (Imports) and docs/spec/stdlib-reference.md (HTML writer) updated
    - full release gate green
  - stop_rule: use-site conflict detection for wildcard and single-name imports; no import aliases or renaming syntax.

- [ ] TODO-5718: All values of a repeated parameter and decoded path parameters
  - owner: ai
  - status: deferred
  - created_at: 2026-11-07
  - phase: Web apps (docs/WebAppsPlan.md; found by TODO-5702 and TODO-5703)
  - parallel_track: web-params
  - scope: `queryParam` and `formParam` give the first of repeated parameters (`?tag=a&tag=b`), so a form with checkboxes or a multi-select cannot read every value; and `pathParam` returns the segment as sent, so `/files/a%20b` gives `a%20b`. Add a way to read every value of a name (a count plus an index, as the header accessors do, or a vector of Strings) and a decoded form of a path parameter with the same decoding and failure rules as `queryParam`. Defer until an example needs either.
  - acceptance:
    - VM and optexe tests: three values of a repeated query and form parameter in order; a `%20` and a malformed path segment
    - docs/spec/stdlib-reference.md updated
    - full release gate green
  - stop_rule: repeated values and decoded path segments only; no typed parameters, no multipart.

- [ ] TODO-5719: `location(this.field)` passes directly as a `Function` call argument
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Closures (docs/spec/value-lifecycle.md, Function values; found by TODO-5704)
  - parallel_track: closures-arguments
  - scope: In a method, `this.handler(location(this.state), x)`, where `handler` is a `Function<void, Reference<S>, i32>` field, fails VM lowering with `struct parameter type mismatch: expected /S, got <unknown>`; binding `[Reference<S>] state{location(this.state)}` first and passing `state` works, and `handler(location(local), x)` on a local works. `AppRouter.call` in stdlib/std/http/http.prime uses the local. Make the lowerer infer the struct type of a `location(...)` of a field in a `Function` call's arguments.
  - acceptance:
    - the direct form runs on the VM and optexe, and `AppRouter.call` drops its local
    - full release gate green
  - stop_rule: `location` arguments of `Function` calls only.

- [ ] TODO-5721: A string converts to a `StringView` at a field of a generic struct
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Language surface for handlers (docs/spec/strings-and-views.md; found by TODO-5704)
  - parallel_track: text-literal-views
  - scope: A `string` value or literal converts to a `StringView` at a brace-constructor field of a non-generic struct (TODO-5700), but not of a generic one: `AppRoute<State>{[method] method}` with a `[string] method` fails with `argument type mismatch ... expected /std/text/view/StringView`, so `AppRouter.route` wraps both in `textOf`. Insert the conversion once the instance's field types are known.
  - acceptance:
    - a generic struct's StringView field takes a literal and a `string` value without `textOf` on the VM and optexe, and `AppRouter.route` drops its `textOf`
    - full release gate green
  - stop_rule: fields of generic struct instances only.

- [ ] TODO-5726: An indexed element of a stdlib `Vector<T>` of structs binds without a fault
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Collections (docs/spec/value-lifecycle.md, Copies; found by TODO-5720)
  - parallel_track: collections-access
  - scope: With `[Vector<Noisy> mut] v{vector<Noisy>()}` filled by `vectorPush<Noisy>`, where `Noisy` is a struct (one `i32` field, or an `i32` and a `bool`), `[Noisy] a{v[0i32]}`, `at(v, 0i32)` and `v.at(0i32)` stop with `VM error: unaligned indirect address in IR: 1` (or `: 2`); `vectorAt<Noisy>(v, 0i32)` binds and copies once, and the builtin `vector<Noisy>` spelling works. Present before TODO-5720; likely the builtin element access reads the stdlib Vector record with the builtin record layout.
  - acceptance:
    - the three spellings bind, copy once and destroy once on the VM and optexe (matrix case)
    - full release gate green
  - stop_rule: indexed reads of stdlib `Vector<T>` struct elements only.

- [ ] TODO-5728: `slice` and `sliceMut` on a vector field of a struct resolve
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Collections (docs/spec/strings-and-views.md, slices; found by TODO-5705)
  - parallel_track: collections-access
  - scope: With `[Holder mut] holder{Holder{}}` where `Holder` has a field `[vector<u8> mut] bytes`, `holder.bytes.slice(0i32, 2i32)` and `holder.bytes.sliceMut(0i32, 2i32)` fail with `unknown call target: slice` / `sliceMut` (PSC1005), while `push`, `count` and indexing on the same field work and the same calls on a `vector<u8>` local or parameter work. The /std/http/server connection table keeps its buffers in a separate `vector<u8>` parameter because of this.
  - acceptance:
    - `slice` and `sliceMut` on a vector field of a local struct and of a `mut` struct parameter read and write the field's storage on the VM and optexe, with a positive and a negative (out of range) case
    - full release gate green
  - stop_rule: method resolution of the slice calls on struct fields only.

- [ ] TODO-5730: Elements of a `vector<String>` struct field read without a fault
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Collections (docs/spec/value-lifecycle.md; found by TODO-5723)
  - parallel_track: collections-access
  - scope: A struct with a builtin `[vector<String> mut] lines` field can `push` and `count`, but binding an element (`[String] line{greeter.lines[0i32]}`) stops with `VM error: unaligned indirect address in IR: 4` on the VM and in optexe's build, and `greeter.lines[0i32].view()` fails to lower (`struct parameter type mismatch`); passing the element to a `[String]` parameter faults the same way, through a `Reference<Greeter>` too, and `page.element("p", greeter.lines[i].view())` is rejected (`expected StringView got array`). A local `vector<String>` works, as does a `vector<Todo>` field whose struct holds a String. The hello_button example of TODO-5722 keeps its lines in one `String` instead. Repro: `[Greeter mut] g{Greeter{}}`, `g.lines.push(stringFrom("x"))`, `[String] back{g.lines[0i32]}`.
  - acceptance:
    - a program pushes Strings to a `vector<String>` field, binds, views and passes each element, directly and through a Reference, on the VM and optexe, each String freed once
    - full release gate green
  - stop_rule: element access of `vector<String>` fields; other element types are TODO-5726.

- [ ] TODO-5731: A `string` field of an element whose closure captures reads without a fault
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Closures (docs/spec/value-lifecycle.md, Lambdas; found by TODO-5722)
  - parallel_track: closures
  - scope: In `/std/web` `WebApp<State>.callControl`, a `[string] id{this.clicks[index].id}` read of a `string` field of an element of `vector<ClickHandler<State>>`, whose `Function<void, Reference<State>, PointerEvent>` field holds a closure with a capture (`[mut calls{0i32}]` or `[calls{0i32}]`), stops the VM with `invalid indirect address in IR: 9223372036854775856` before the handler runs; with a non-capturing lambda or a named handler it works, and with a `StringView` id field it works. A smaller non-namespaced repro (a generic Holder with a `string` id and the same Function) did not fault, so the trigger also needs something of the web layer: the element struct in another namespace, the second vector field of `AppRoute<State>`s, or the `Request` path through `call`. Repro: make `ClickHandler.id` a `string` again in stdlib/std/web/web.prime and run tests/fixtures/http/web_controls.prime on the VM.
  - acceptance:
    - `ClickHandler.id` is a `string` and web_controls passes on the VM and optexe; a reduced program reading a `string` field beside a capturing `Function` field runs on the VM and optexe
    - full release gate green
  - stop_rule: the faulting field read; no `/std/web` API change.

- [ ] TODO-5732: A method call on a reference parameter resolves in a generic struct's method
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Language surface for handlers (docs/WebAppsPlan.md; found by TODO-5706)
  - parallel_track: generic-methods
  - scope: In a method of a generic struct, `response.clear()` on a `[Reference<Response>] response` parameter (`/std/http`) is rejected with `unknown call target: clear`, while `response.redirect("/")` in the same method and `response.clear()` in a free function both work. Repro: `Holder<T>` with `[mut effects(heap_alloc)] resetIt([Reference<Response>] response) { response.redirect("/") response.clear() }`, instantiated as `Holder<i32>` and called from `main`; the name `clear` likely takes a collection-helper path when the receiver's type is resolved during the template's instantiation. `WebApp.answerFragment` (stdlib/std/web/web.prime) resets the status and header lines by hand instead.
  - acceptance:
    - the repro runs on the VM and optexe and `answerFragment` calls `response.clear()`
    - full release gate green
  - stop_rule: method resolution on reference parameters in generic struct methods; no new methods.
  - notes: TODO-5709 hit the same name on a field: `this.closes.clear()` on a `vector<NativeWindowHandler<State>>` field in a method of `AppHandlers<State>` is `unknown call target: clear`, so the single-handler fields keep every registration and use the last.

- [ ] TODO-5733: An `i32` field of a later element of a vector of handler structs reads without a fault
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Closures (docs/spec/value-lifecycle.md, Lambdas; found by TODO-5724)
  - parallel_track: closures
  - scope: In `/std/web`, with `PointerHandler<State>` holding `[StringView] id`, `[i32] kind` and a `Function<void, Reference<State>, PointerEvent>` (in either field order), `[i32] kind{this.pointers[index].kind}` in a `WebApp<State>` method stops the VM with `invalid heap free address in IR` once `index` is 1 (the third `onMouseUp` after `onMouseOver` and `onMouseDown`), with named handlers as with closures; `this.pointers[index].id` reads fine at every index. A non-namespaced generic `App<State>` with the same element struct (id, kind, Function) did not fault, so the trigger also needs something of the web layer, as for TODO-5731. `WebApp` keeps the kinds in parallel `vector<i32>` fields (`pointerKinds`, `keyKinds`) instead. Repro: put `kind` back into `PointerHandler` in stdlib/std/web/web.prime, read it in `pointerIndex`, and run tests/fixtures/http/web_events.prime on the VM.
  - acceptance:
    - `PointerHandler` and `KeyHandler` carry their kind and web_events passes on the VM and optexe; a reduced program reading an `i32` field beside a `Function` field of the second element runs on the VM and optexe
    - full release gate green
  - stop_rule: the faulting element field read; no `/std/web` API change.

- [ ] TODO-5735: Same-arity overloads of struct methods resolve by argument type
  - owner: ai
  - status: ready
  - created_at: 2026-11-08
  - phase: Language surface for handlers (docs/WebAppsPlan.md; found by TODO-5709)
  - parallel_track: method-overloads
  - scope: docs/spec/language-core.md (Helper overloading) lets struct methods overload by parameter type, but a same-arity family declared in a struct body does not resolve. `evaluateRequirementOverloadViability` (src/semantics/TemplateMonomorphCoreUtilities.cpp) pairs the method's parameters, which have no implicit `this` yet, with the call's arguments, which start with the receiver, so `box.add("a", 3i32)` beside `add([string], [Tag])` is `no viable overload`; the struct-method path of `resolveMethodCallTemplateTarget` (src/semantics/TemplateMonomorphMethodTargets.cpp) passes no locals to `selectHelperOverloadPath`, so a struct-typed local is `unknown` and both candidates stay viable (`ambiguous`); a call argument is `unknown` too (as in TODO-5716), so `Menu.item([string], [string], [Command])` beside `item(..., [i32])` breaks the editor's `item(title, shortcut, command_save())`; and in a generic struct, `holder.on(tag, lambda)` with `on([Tag], [Function<...>])` beside `on([Other], [Function<...>])` is `unknown method: /Holder__t.../on`, and the lambda does not convert to the `Function` parameter. Skipping one argument when `expr.isMethodCall` and the call has one argument more than the method has parameters, and passing `&locals` at that `selectHelperOverloadPath`, resolved the non-generic local case in a trial. `/std/ui/native` uses separate names until then: `Menu.commandItem`, `Toolbar.commandButton`, `Menu.setCommandChecked`, `AppHandlers.onCheckBoxChange`, `onPopupChange`, `onListChange`.
  - acceptance:
    - same-arity struct method overloads resolve by a literal, a local, a parameter and a call with a declared return type, in a struct and in a generic struct, including a `Function` parameter given a lambda, on the VM and optexe; a no-viable and an ambiguous negative test
    - `Menu.item`, `Toolbar.button` and `AppHandlers.onChange` take a `Command` or a control as overloads, and the editor builds unchanged
    - full release gate green
  - stop_rule: overload selection of struct methods; no change to free-function overloads or to the specificity rule.

- [ ] TODO-5736: `pick` and sum parameters in a generic struct's method lower
  - owner: ai
  - status: ready
  - created_at: 2026-11-08
  - phase: Language surface for handlers (docs/WebAppsPlan.md; found by TODO-5709)
  - parallel_track: generic-methods
  - scope: In a method of a generic struct, a `pick` stops VM lowering with `block arguments require a definition target: /Holder__t.../pick`, and passing a value of a namespaced sum type to a parameter declared `[AppEvent] event` (in `/std/ui/native`'s `AppHandlers<State>`) stops it with `struct parameter type mismatch: expected AppEvent, got /std/ui/native/AppEvent`. Repro: `[sum] Event { none [i32] number }`, a `Holder<S>` with `[S mut] state` and `[return<i32>] value([Event] event) { [i32 mut] total{0i32} pick(event) { number(n) { total = n } none { total = 1i32 } } return(total) }`, called on a `Holder<i32>` from `main` (the same `pick` in a free function works). `AppHandlers.deliverNext` (stdlib/std/ui/native/handlers.prime) reads the C ABI event kinds itself instead of picking `app.waitEvent()`.
  - acceptance:
    - the repro and a namespaced sum parameter run on the VM and optexe; `deliverNext` picks `app.waitEvent()`
    - full release gate green
  - stop_rule: `pick` and sum-typed parameters in methods of generic structs.

- [ ] TODO-5737: Implicit template arguments infer through a generic struct parameter
  - owner: ai
  - status: ready
  - created_at: 2026-11-08
  - phase: Language surface for handlers (docs/WebAppsPlan.md; found by TODO-5709)
  - parallel_track: template-inference
  - scope: `size_of_holder<S>([Holder<S>] holder)` called as `size_of_holder(holder)` with a `Holder<i32>` local fails with `unable to infer implicit template arguments for /size_of_holder`, while a `[T]` parameter infers. `App.run` (stdlib/std/ui/native/native.prime) is therefore `run<Handlers>([Handlers mut] handlers)` and accepts any value with a `run()` method. Infer `S` from the argument's instance type.
  - acceptance:
    - a free function and a struct method with a `[Holder<S>]` parameter infer `S` on the VM and optexe; a negative test for an argument that is not a `Holder`; `App.run` takes `[AppHandlers<State> mut]`
    - full release gate green
  - stop_rule: inference through one generic struct parameter; no change to explicit template arguments.

- [ ] TODO-5682: Native: a mutating vector-field method inside an arithmetic expression returns the right value
  - owner: ai
  - status: ready
  - created_at: 2026-11-06
  - phase: Closures follow-ups (split from TODO-5668)
  - parallel_track: native-backend
  - scope: With `KeepV { [vector<i32> mut] seen  add([i32] x) { this.seen.push(x)  return(this.seen.count()) } }`, `return(direct * 10i32 + m.add(6i32))` returns 18 on `--emit=native` and 22 on the VM and optexe; binding `[i32] second{m.add(6i32)}` first gives 22 everywhere. Present before TODO-5668's count fix, so the native backend likely clobbers a pending operand while lowering the inlined push.
  - acceptance:
    - the expression form returns 22 on native-O0 and native-O2 (matrix case)
    - full release gate green
  - stop_rule: operand preservation around inlined vector mutations on native.

- [ ] TODO-5676: Lambdas can state their return type
  - owner: ai
  - status: deferred
  - created_at: 2026-11-06
  - phase: Closures follow-ups
  - parallel_track: closures-return-types
  - scope: A lambda has no return-type spelling, and inference fails for struct and `Result` returns. `Function<Result<i32, i32>, i32>{[]([i32] x) { return(Result<i32, i32>{[ok] x * 2i32}) }}` is rejected with `unable to infer return type on /main__lambda0/call`. In a generic wrapper whose lambda returns `inner(x) + seen` with `inner` a template-typed capture, the error is `return type inference cycle requires explicit annotation on /with_logging__lambda0__t.../call`, and there is no annotation to give. Specify a return annotation in docs/spec/value-lifecycle.md, and use the `R` of `Function<R, A...>` as the expected type when a lambda is converted. Deferred: handlers that return `void` and write into a `Reference<Response>` avoid it.
  - acceptance:
    - both repros run on VM and optexe; a negative test for a mismatched annotation; spec text with a runnable example
    - full release gate green
  - stop_rule: lambda return types only.

- [ ] TODO-5677: A `Function` whose closure holds a same-typed `Function` copies and destroys
  - owner: ai
  - status: deferred
  - created_at: 2026-11-06
  - phase: Closures follow-ups
  - parallel_track: closures-lifecycle
  - scope: `with_logging([Function<i32, i32> move] next)` returning `Function<i32, i32>{[mut inner{next}, mut seen{0i32}]([i32] x) { ... inner(x) ... }}` stops at lowering with `vm backend does not support recursive calls: /__Function1__t.../Copy` (and the native/optexe equivalent). The generated `Copy` and `Destroy` of that `Function` instance call themselves, and lifecycle helpers are lowered inline (`src/ir_lowerer/IrLowererInlineCallContextHelpers.cpp`), which rejects recursion; plain recursive functions work. Lower recursive lifecycle helpers as real calls. Deferred: generic middleware (a closure wrapping a template-typed handler) composes without erasure and runs today.
  - acceptance:
    - the repro (a two-level wrapped `Function` called, copied and destroyed) runs on VM, native and optexe with no leaked blocks
    - full release gate green
  - stop_rule: recursive generated lifecycle helpers.

- [ ] TODO-5510: `return` returns from unit-variant pick arms correctly
  - owner: ai
  - status: ready
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: control-returns
  - scope: `pick(m) { none { return(-1i32) } num(v) { return(v) } }` in statement position does not return from the unit-variant arm (rewritten to `none{-1}`, IR `PushI32 -1; Pop`); same for `Result<E>`'s `ok { return(...) }`. (The lambda half is done: lambdas are closure structs, TODO-5663.)
  - acceptance:
    - unit-variant pick arms return; parse/semantic + matrix tests
    - full release gate green
  - stop_rule: return lowering in pick arms.

- [ ] TODO-5511: Safe code cannot reach container storage or unsafe stdlib helpers
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: safety-stdlib
  - scope: Safe code frees or aliases container storage through stdlib internals: `[unsafe]` helpers are callable from safe code (`vectorFreeStorage`, `vectorSlotUnsafe`, `vectorBorrowSlot` returning an untracked `Reference`, `vectorTakeSlot`/`DropSlot`/`InitSlot`, ring-buffer and SoA slot helpers), non-public helpers are callable by absolute path, `Vector`'s `data`/`fieldCount`/`fieldCapacity` fields are public and writable (double free, out-of-bounds read), its brace constructor accepts arbitrary storage, and Map exposes `keys`/`payloads`/`inner`. Make those helpers `[unsafe_api]` (or private and enforced), make container fields private or read-only, and keep the stdlib compiling.
  - acceptance:
    - each repro (free via helper, borrowSlot then reserve, takeSlot then clear, `w.data = v.data`, writing fieldCount, brace-constructing a Vector around `v.data`) is rejected outside `[unsafe]`; stdlib and corpus still compile
    - full release gate green
  - stop_rule: visibility and unsafe marking; no container redesign.
  - notes: deferred: queued behind the Ready Now cap; large, split before starting.

- [ ] TODO-5512: Pointers and aliases count as borrows of their root
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: safety-borrows
  - scope: `[Pointer<vector<i32>>] p{location(v)}` stays usable after `move(v)` or passing `v` to a move parameter (only References are tracked); a read-only view `[vector<i32>] view{v}` stays live while `v` is grown or cleared through a `Reference` alias passed to a helper (the view check only matches root names).
  - acceptance:
    - both are rejected with borrow diagnostics; negative + positive semantics tests
    - full release gate green
  - stop_rule: borrow tracking for Pointer roots and aliases.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5513: Methods through a dereferenced vector pointer read the right fields
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: collections-access
  - scope: With `[Pointer<vector<i32>>] p{location(v)}`, `dereference(p).count()` returns 0 and `dereference(p).capacity()` returns the count on VM and native; `vectorCount<i32>(dereference(p))` on a `Pointer<Vector<i32>>` is rejected with `expected /std/collections/vector/Vector__t... got /vector`.
  - acceptance:
    - count/capacity/at through `dereference(p)` match the direct calls; matrix case
    - full release gate green
  - stop_rule: receiver offset handling only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5836: A spawned definition without a `return` transform is a `Task<void>`
  - owner: ai
  - status: deferred
  - created_at: 2026-11-10
  - phase: Multithreading (found while doing TODO-5835)
  - parallel_track: mt-vm-tasks
  - scope: `[Task<void>] t{[spawn] hello(5i32)}` where `hello` has `[effects(io_out)]` and no `return<...>` is rejected with `spawn requires call target with inferred return type` (src/semantics/SemanticsValidatorTaskHandles.cpp, `inferTaskSpawnCallBinding`), although a definition without a `return` transform is void everywhere else; `[effects(io_out), return<void>]` works.
  - acceptance:
    - a semantic test spawns a definition without a `return` transform into a `Task<void>` and waits it; a VM compile-run test prints its output
    - binding it as `Task<i32>` is still rejected
    - full release gate green
  - stop_rule: the spawn binding inference only.
  - notes: deferred: no example needs it; `return<void>` is the workaround.

- [ ] TODO-5837: VM debug sessions step programs that spawn tasks
  - owner: ai
  - status: deferred
  - created_at: 2026-11-10
  - phase: Multithreading (found while doing TODO-5792)
  - parallel_track: vm-debugger
  - scope: A debug session (`VmDebugSession`, `--debug-json`, `--debug-dap`, `--debug-trace`) steps one call stack with `stepVmKernel`, so it faults on `TaskSpawn`/`TaskWait` with `tasks run only in a plain VM run, not in a debug session`. Give the session the scheduler's task table (the eager default schedule is enough: a spawn pushes the task's call stack and the session steps it until it ends), with the call stack, snapshots and breakpoints of the running task.
  - acceptance:
    - a debug session of a program with two tasks stops at a breakpoint inside a task, shows that task's frames, and continues to the same output as a plain run
    - full release gate green
  - stop_rule: the debug session on the default schedule; seeded schedules in the debugger are later work.
  - notes: deferred: no debugger user needs tasks yet.

- [ ] TODO-5840: A byte loop over one large region on `optexe` runs as fast as before the granule table
  - owner: ai
  - status: ready
  - created_at: 2026-11-10
  - phase: Optimizing backends (found in TODO-5793)
  - parallel_track: optexe-bytes
  - scope: TODO-5793 replaced `VmHeapCore`'s last-region cache (three compares against members, but written on every miss) with a per-granule state table (one dependent load and compare, no write). On `optexe` `benchmarks/strings/append_chunks.prime` (43-byte appends into one String growing to 8.6 MB, byte by byte through `ps_store_u8`) takes 10% more CPU time (51.8 ms against 56.9 ms, median of 31 interleaved runs); every other string benchmark is within 4% and two_region_copy is 13% faster. With the check removed entirely both versions are within 2%, so the cost is the check in the hot loop: the generated program calls `VmHeapCore::byteAt` out of line, and forcing it inline (fast path inline, map search out of line) made append_chunks 15-20% slower, not faster. Find where the time goes (instruction count of the String.append loop in the generated code, the inlining of `textStore`) and win it back without a write on the read path.
  - implementation_notes: include/primec/runtime/VmHeapCoreBody.inc (`byteAt`, `byteAtEdge`), src/backend/IrToOptCppRuntime.h (`ps_load_u8`, `ps_store_u8`); measure with CPU time of interleaved runs (wall time on a shared machine varied by 50%).
  - acceptance:
    - `append_chunks` on `optexe` within 2% of the pre-TODO-5793 build; the other string benchmarks and two_region_copy no slower than now
    - no write on the byte read path; full release gate green
  - stop_rule: the optexe byte check; VM changes only if the shared heap code needs them.
  - notes: TODO-5794 (per-thread heap arenas) left this open: it has no per-thread region cache (a state-table read needs none), and append_chunks runs 0.7% more instructions than before it. Measured there again: the byte check inlined into the byte loops costs 8% more instructions on short_strings, so `ps_byte_at` (src/backend/IrToOptCppRuntime.h) keeps it out of line. The cycles of the same source move by 15% with `-falign-functions`/`-falign-loops`, so compare instruction counts and several layouts, not one CPU time.

- [ ] TODO-5844: Short-string `optexe` benchmarks take the cycles they took before the heap arenas
  - owner: ai
  - status: ready
  - created_at: 2026-11-10
  - phase: Optimizing backends (found in TODO-5794)
  - parallel_track: optexe-bytes
  - scope: After TODO-5794 (per-thread heap arenas) `benchmarks/strings/request_heap.prime`, `request_arena.prime` and `short_strings.prime` run 1.5% fewer to 0.3% more instructions on `optexe` but 4 to 7% more cycles (request_heap: +4.6% to +7.7% of the minimum over five code layouts, `-falign-functions`/`-falign-loops`; short_strings about +4% averaged over them), so the cost is in how the instructions run, not in how many there are. Ruled out by measurement: the `__pstext_*` host calls and `ps_heap_alloc`/`free`/`realloc` (timed with `mach_absolute_time`, equal before and after), the `Rt` members being references (a by-value variant is as slow), the remote-free check before an allocation, `slotAt` and `byteAt` themselves (the old bodies pasted into the new class change nothing), one read of the granule state instead of two, the arena base folded into the storage pointer. Not ruled out: the code around the out-of-line `ps_byte_at` and the inlined `ps_resolve` in the large generated functions (their fault paths, the order of blocks), `ZeroedAllocator` storage, and plain layout luck beyond what the alignment flags vary (the same source moves 15% with them). Find it with a sampling profiler on a quiet machine, and fix it without changing the argument list of `ps_resolve` or adding a local to every function (either stops clang inlining small functions: +19% instructions on append_chunks).
  - implementation_notes: src/backend/IrToOptCppRuntime.h (`ps_byte_at`, `ps_resolve`, `ps_resolve_outer`), include/primec/runtime/VmHeapCoreBody.inc. Compare `primec --emit=optcpp` output of commit e8a547504 (before the arenas) and of master, both built with `clang++ -std=c++23 -O2 -ffp-contract=off`; count instructions and cycles with `proc_pid_rusage` (`ri_instructions`, `ri_cycles`) of the program itself, minimum of interleaved runs, under several alignment flags. CPU-time medians on a shared machine moved by 10% between sessions.
  - acceptance:
    - request_heap, request_arena and short_strings on `optexe`: cycles within 3% of the e8a547504 build, averaged over five layouts; instructions no higher than now; the other string benchmarks no slower
    - full release gate green
  - stop_rule: the generated program's single-threaded cost; the arenas' design stays.

- [ ] TODO-5847: A spawned call with a collection or a `copy` parameter runs as a task
  - owner: ai
  - status: ready
  - created_at: 2026-11-10
  - phase: Multithreading (docs/MultithreadingPrototype.md, Runtime Model)
  - parallel_track: mt-vm-tasks
  - scope: A spawned call runs as a task on the VM and `optexe` when its parameters and result are scalars, `String`s, other structs outside the collections, or closures (`resolveTaskSpawnShape`, src/ir_lowerer/IrLowererRecursionAnalysis.h). It still runs inline, with an `--opt-report` note, when a parameter or the result is a `vector`, a `map` or another collection, and when a parameter is `[T copy]`, `[T copy mut]` or `[T move]` of a scalar or of a struct that is not a `Function`: the address ABI has no real call for those shapes, and `[String copy mut]` is what the checker tells a user to write for a task that changes its argument. Give those shapes a real call and lower their spawns to `TaskSpawn` through the task block the struct shapes use (`LowerSetupStageState::TaskSpawnSite`): a collection argument is copied (or moved) in the spawner, the task reads it through the block, an owned parameter is destroyed by the task, and a collection result is taken over by the waiter.
  - implementation_notes: `isAddressAbiParameter` (src/ir_lowerer/IrLowererRecursionAnalysis.cpp) rejects template types and `copy`/`move` of anything but a `Function`; a collection local carries element metadata (`LocalInfo`) the callee's parameter needs. A `string` parameter is TODO-5741: once a real call takes it, drop the `string` reason in `resolveTaskSpawnShape` and pass the module string index as a scalar. Tests: tests/unit/program_matrix/test_program_matrix_tasks.cpp, tests/unit/ir_pipeline/optimizer/test_ir_optexe_tasks.cpp (the notes for /total, /numbers and /bump there turn into tasks).
  - acceptance:
    - positive: a task that takes a `vector<i32>` and one that returns a `vector<String>` give the waiter the values on the VM (default schedule and seeds 1 to 8) and on `optexe`, with the heap empty afterwards (heap statistics); a task with a `[String copy mut]` parameter changes its own copy and the spawner's value is unchanged; `--opt-report` has no inline note for these spawns
    - negative: a spawned call that still runs inline keeps its note naming the reason
    - full release gate green
  - stop_rule: collection and `copy`/`move` parameters and collection results of spawned calls on the VM and `optexe`; `string` parameters are TODO-5741 and native is TODO-5799.

- [ ] TODO-5848: `return(wait(task))` returns a `String` task result
  - owner: ai
  - status: ready
  - created_at: 2026-11-10
  - phase: Multithreading (docs/MultithreadingPrototype.md, Runtime Model)
  - parallel_track: mt-wait-types
  - scope: In a definition with `return<String>`, `return(wait(t))` of a `[Task<String>] t` is rejected by the checker: `return type mismatch: expected /std/text/String`. Binding the result first (`[String] out{wait(t)}` then `return(out)`) is accepted, and so is `return(wait(t))` for a scalar result and for a user struct result. The return check does not match the `String` of the waited result with the declared `/std/text/String`; make it compare the task's resolved result type.
  - implementation_notes: the diagnostic comes from src/semantics/SemanticsValidatorStatementReturns.cpp (`return type mismatch: expected ` + the declared type). Repro: a `greet` that builds and returns a `String`, `[return<String> effects(task, heap_alloc)] relay([String] name) { [Task<String>] t{[spawn] greet(name)} return(wait(t)) }`, with `import /std/text/*`. Tests: tests/unit/semantics/calls_and_flow (the task and sendable suites), tests/unit/program_matrix/test_program_matrix_tasks.cpp.
  - acceptance:
    - positive: the repro compiles and prints the length of the relayed text on the VM (default schedule and seeds 1 to 8) and on `optexe`, with the heap empty afterwards (heap statistics); `return(wait(t))` of a user struct and of an `i32` result keep working
    - negative: `return(wait(t))` of a `Task<i32>` in a `return<String>` definition is still a return type mismatch
    - full release gate green
  - stop_rule: the return check of a waited `String` result; no change to what a spawned call may take or return.

- [ ] TODO-5849: A destructured multi-wait keeps each result's own type
  - owner: ai
  - status: ready
  - created_at: 2026-11-10
  - phase: Multithreading (docs/MultithreadingPrototype.md, Runtime Model)
  - parallel_track: mt-wait-types
  - scope: `[a b] wait(x, y)` with `[Task<String>] x` and `[Task<Reply>] y` (`Reply` a user struct with an `i32` field `code`) accepts `a.count()` but rejects `b.code` with `unknown field: code`: the second binding does not get the type of the second task's result when the results differ. Two tasks of the same struct type work. Give every destructured binding its own task's result type. Also, the destructured form needs `import /std/tuple/*` in the program, and without it the only message is `unknown template definition: /std/tuple/tuple`, with no source location: either make the form work without the import or report a diagnostic at the `wait` that names the import.
  - implementation_notes: the field diagnostic comes from src/semantics/SemanticsValidatorExprFieldResolution.cpp; the destructuring of a multi-wait is the tuple destructuring of `wait`'s `tuple<...>` result (docs/MultithreadingPrototype.md, the multi-wait section; docs/spec/stdlib-reference.md, tuples). Repro: `greet` returning a `String` and `make_reply` returning a `Reply{body, code}` with a `String` and an `i32` field, spawned as `x` and `y`, then `[a b] wait(x, y)`, `print_line(a.count())`, `print_line(b.code)`. Tests: tests/unit/semantics/calls_and_flow, tests/unit/program_matrix/test_program_matrix_tasks.cpp.
  - acceptance:
    - positive: the repro prints the text's length and the struct's `code` on the VM (default schedule and seeds 1 to 8) and on `optexe`; three tasks with an `i64`, a `String` and a struct result destructure into three bindings of those types; the heap is empty afterwards (heap statistics)
    - negative: a field the struct does not have is still `unknown field`; a destructured multi-wait in a program without the tuple import either compiles or is one diagnostic with a source location that names `/std/tuple`
    - full release gate green
  - stop_rule: the types of destructured multi-wait bindings and the missing-import message; no change to `get<I>` or bracket indexing of the tuple.

- [ ] TODO-5850: A call that returns a `Reference` is a closure argument
  - owner: ai
  - status: ready
  - created_at: 2026-11-10
  - phase: Closures (docs/spec/value-lifecycle.md, Lambdas)
  - parallel_track: closures-references
  - scope: Calling a closure with the result of a call that returns `Reference<T>` (`body(borrow_of(slot))`, `borrow_of` a `[return<Reference<T>>]` definition over `borrow(dereference(slot))`) hands the body a wrong address: the body reads and writes something other than the value. Binding the result first (`[Reference<T>] target{borrow_of(slot)}` then `body(target)`) works, which is what `Mutex.locked` in stdlib/std/sync/sync.prime does. Make the direct form pass the same reference.
  - implementation_notes: found while writing `Mutex.locked` (the direct form printed the old value after a body assigned through its parameter). Closure calls lower in src/ir_lowerer/IrLowererLowerInlineCalls.cpp; compare the argument materialisation of a `Reference` binding with that of a `Reference`-returning call. Tests: tests/unit/program_matrix/test_program_matrix_closures_references.cpp.
  - acceptance:
    - positive: a generic `apply<Body>([Body mut] body, [Pointer<uninitialized<i64>>] slot)` that calls `body(borrow_of(slot))` with a body that adds 1 leaves the slot one larger on the VM and `optexe`; `Mutex.locked` passes the call directly and its tests still pass
    - negative: a closure whose parameter is a value still gets a copy
    - full release gate green
  - stop_rule: `Reference`-returning calls as closure arguments.

- [ ] TODO-5851: A `[ref]` closure written in a generic call infers the closure type
  - owner: ai
  - status: ready
  - created_at: 2026-11-10
  - phase: Closures (docs/spec/value-lifecycle.md, Lambdas)
  - parallel_track: closures-references
  - scope: `cell.locked([ref seen]([Reference<i64>] value) { seen = value })` (any method or definition with an inferred closure template parameter, `locked<Body>([Body mut] body)`) is rejected with `template arguments required for /main__lambda0`, while the same closure bound first (`[mut] look{[ref seen](...) {...}}` then `cell.locked(look)`) and a closure with value captures written in the call both work. Infer the template argument for a by-reference closure written in the call.
  - implementation_notes: the by-reference closure struct is itself a template over its captures' lifetimes or types (hence the message); implicit template inference runs before the main validator (AGENTS.md, Semantics pipeline note). Tests: tests/unit/semantics/lambdas, tests/unit/program_matrix/test_program_matrix_closures_references.cpp; then drop the binding in the `[ref]` case of tests/unit/program_matrix/test_program_matrix_mutexes.cpp and the note in docs/spec/stdlib-reference.md (Shared state).
  - acceptance:
    - positive: the call above compiles and sets `seen` on the VM and `optexe`; the same for a free generic definition taking `[Body mut] body`
    - negative: a `[ref]` closure passed where the parameter is a `Function` is still the existing diagnostic
    - full release gate green
  - stop_rule: inference for inline by-reference closures.

- [ ] TODO-5852: `wait(a, b)` takes `Task<void>` handles
  - owner: ai
  - status: ready
  - created_at: 2026-11-10
  - phase: Multithreading (docs/MultithreadingPrototype.md, Runtime Model)
  - parallel_track: mt-wait-types
  - scope: `wait(left, right)` with two `Task<void>` handles fails with `unsupported binding type: void` from stdlib/std/tuple/tuple.prime, because the multi-wait builds a `tuple<void, void>`. The design's `Mutex` example uses this form. A multi-wait whose tasks are all `Task<void>` waits each in order and has no result; mixed with value tasks, either the void ones have no tuple element or the mix is one clear diagnostic at the `wait`.
  - implementation_notes: multi-wait typing is in src/semantics/SemanticsValidatorTaskHandles.cpp. Tests: tests/unit/program_matrix/test_program_matrix_tasks.cpp. Then use `wait(left, right)` in the `Mutex` examples (docs/MultithreadingPrototype.md, docs/spec/stdlib-reference.md).
  - acceptance:
    - positive: `wait(left, right)` of two `Task<void>` runs both on the VM (default schedule and seeds 1 to 8) and on `optexe`
    - negative: the mixed form is the chosen behaviour, tested; a non-task argument is still its diagnostic
    - full release gate green
  - stop_rule: void tasks in a multi-wait.

- [ ] TODO-5853: A borrowed `Reference` parameter stored through a `[ref]` capture is diagnosed
  - owner: ai
  - status: deferred
  - created_at: 2026-11-10
  - phase: Closures (docs/spec/value-lifecycle.md, Lambdas)
  - parallel_track: closures-references
  - scope: A closure `[ref keeper]([Reference<i64>] value) { keeper.kept = value }` with `keeper` a struct that has a `[Reference<i64>] kept` field is accepted; the assignment copies or rebinds in a way that does not alias the parameter today, but nothing says a borrowed parameter cannot be stored. Decide what assigning to a `Reference` field means (rebind or write through) and diagnose storing a borrowed parameter with the existing `borrowed parameter escapes via assignment`. Also: a closure that returns its `Reference<T>` parameter returns a copy of the value without a diagnostic, and `return(location(value))` is `unable to infer return type`.
  - implementation_notes: found with `Mutex.locked`, whose body gets a `Reference<T>` that must stay in the body (docs/MultithreadingPrototype.md, Mutex). The escape checks are in src/semantics (search `borrowed parameter escapes`). Deferred: needs the decision on `Reference` field assignment.
  - acceptance:
    - positive: reading through the parameter and assigning through it still work
    - negative: the closure above is `borrowed parameter escapes via assignment: value`
    - full release gate green
  - stop_rule: the escape diagnostic for `Reference` parameters of closures.

- [ ] TODO-5854: A `Mutex` or `Atomic` handle does not keep the value it was made from
  - owner: ai
  - status: deferred
  - created_at: 2026-11-10
  - phase: Multithreading (docs/MultithreadingPrototype.md, Mutex)
  - parallel_track: mt-primitives
  - scope: `Mutex<T>{value}` maps `value` to the struct's first field, `initial`, and the heap cell gets a copy of it; the field then stays in every handle. A `Mutex<vector<i64>>` handle therefore owns a vector of its own, a handle copy copies that vector, and each handle destroys one. Make the handle two pointers and a flag: the value moves into the cell when the mutex is made. `Atomic<T>` keeps its `initial` the same way (an integer, so only the size matters).
  - implementation_notes: stdlib/std/sync/sync.prime. Needs a way for a struct to take a construction argument that is not a stored field, or `Create` to move a field out and leave it empty; tests/unit/program_matrix/test_program_matrix_mutexes.cpp ("locked returns the body's value...") prints the copies and destroys of a `Noisy` element and must then show one copy at most and one destroy.
  - acceptance:
    - positive: the `Noisy` program prints no `copy` for a handle copy and one `destroy` in all; heap statistics stay balanced
    - negative: `Mutex<T>{}` without a value for a `T` that has no default is still a diagnostic
    - full release gate green
  - stop_rule: the handle layout of `Mutex` and `Atomic`.

- [ ] TODO-5861: `SharedString` keeps its owner count inside the text block
  - owner: ai
  - status: ready
  - created_at: 2026-11-10
  - phase: Multithreading (docs/MultithreadingPrototype.md, Heaps; docs/spec/strings-and-views.md, Beyond std::string)
  - parallel_track: string-concurrency
  - scope: The atomic owner count of a `SharedString` (stdlib/std/text/shared.prime) is one heap slot of its own beside the text block, because byte regions (`__pstext_alloc`) are not 8-byte aligned (large blocks, arena bumps) and the atomic opcodes (`__psatomic_*_i64`) take heap slot addresses only. So making a `SharedString` allocates and frees twice: an `optexe` loop of 1000000 `sharedFrom` plus destroy retires 4.67e9 instructions, against 3.01e9 with the old in-block plain count. Give the string heap an 8-aligned allocation (or align every block), let the atomic opcodes work on an aligned byte address on the VM and in the optimized C++ runtime, and put the count in the block again (before the text or after its padded 0 byte).
  - implementation_notes: `resolveIndirectAddress` of the VM and `ps_resolve` of src/backend/IrToOptCppEmitter.cpp reject byte addresses today; byte-region liveness (TODO-5793) already finds a region from an address. Measure with `/usr/bin/time -l` (instructions retired) on the two loops in the result note of the atomic count TODO in docs/todo_archive/2026-11.md.
  - acceptance:
    - positive: making a `SharedString` allocates one block (heap live blocks grow by 1 less than today); the create and destroy loop retires at most 3.2e9 instructions on `optexe` and the copy and destroy loop of 5000000 rounds no more than today's 4.5e9; the program-matrix `shared_strings` suite passes unchanged on the VM (all seeds) and `optexe`
    - negative: an atomic opcode on an unaligned byte address faults by name on the VM and on `optexe`
    - full release gate green
  - stop_rule: where the count lives; native follows with TODO-5800.

- [ ] TODO-5862: `serveWorkers` serves on the VM with 2 workers
  - owner: ai
  - status: deferred
  - created_at: 2026-11-11
  - phase: Multithreading and Web apps (docs/MultithreadingPrototype.md, The Server)
  - parallel_track: mt-server
  - scope: `/std/http/server` runs only through `--emit=optexe`: the VM has no binding for the `/std/net` and `/std/os` host functions (`unbound host function: net_listen ...`) and rejects host calls that take `Pointer<u8>` buffers, so the acceptance bullet of `serveWorkers` that asked for a VM run with 2 workers on the default schedule is open. Give the VM a deterministic way to run the server loop: either `primec --emit=vm` binds the two POSIX shims, or a scripted in-memory `/std/net` (a fixed list of connections and their bytes) answers the calls, so the poller, the two channels and the mutex run on the VM scheduler.
  - implementation_notes: deferred because the choice between real sockets on the VM and a scripted net needs scoping first; a scripted net keeps the test free of ports and time. The worker code is in stdlib/std/http/server.prime (`poll_slots`, `work_slots`); VM tasks, mutexes and channels already run it on seeds (tests/unit/program_matrix/test_program_matrix_channels.cpp).
  - acceptance:
    - positive: a VM test serves a fixed set of requests with 2 workers and gives the same responses and the same counter on the default schedule and on seeds 1 to 3
    - negative: a program that calls a `/std/net` function the VM does not answer still reports the unbound host function by name
    - full release gate green
  - stop_rule: the server on the VM; no other backend.

- [ ] TODO-5863: A struct named without its namespace is a `Channel` element
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Multithreading (found with `serveWorkers`)
  - parallel_track: mt-primitives
  - scope: Inside `namespace zed`, a definition with the parameter `[Channel<Job>] jobs` (`Job` a public struct of `zed`) that is called from `main` with a `Channel</zed/Job>` stops with `unsupported return type on /std/sync/channelItemsAlloc__t...`, reported at `Channel`'s `Destroy` in stdlib/std/sync/sync.prime. A second specialization of `Channel` is made for the unqualified name, and its element type is not resolved in the namespace it was written in. Resolve the template argument where it is written, so `Channel<Job>` in `zed` and `Channel</zed/Job>` are one type. `/std/http/server` avoids it by using its job structs only inside its own namespace.
  - implementation_notes: reproduce with a `[public struct] Job` holding two `i32` fields in `namespace zed`, `[public effects(channel, heap_alloc, io_out), return<void>] run([Channel<Job>] jobs)` that picks one `receive()`, and `main` doing `jobs{Channel</zed/Job>{[capacity] 2i32}}`, a `send` and `/zed/run(jobs)`. Check `Mutex<Job>` and `vector<Job>` the same way.
  - acceptance:
    - positive: the program prints the job's fields on the VM and `optexe`; the same with `Mutex<Job>`
    - negative: a name that is in neither the namespace nor the root is still an unknown type
    - full release gate green
  - stop_rule: template arguments that name a struct of the enclosing namespace.

- [ ] TODO-5864: A lambda in a generic definition names the definition's type parameters
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Closures (docs/spec/value-lifecycle.md, Lambdas; found with `serveWorkers`)
  - parallel_track: closures-generics
  - scope: In `show<T>([Mutex<T>] cell)`, the lambda `[seven]([Reference<T>] item) { return(item.value + seven) }` is `unsupported reference target type: T`, also when the call names the argument (`show<Box>(cell)`): the lambda's parameter types are not substituted when the definition is specialized. Substitute the enclosing definition's template arguments in a lambda's parameter and capture types. `/std/http/server` works around it by erasing its handler into a `Function` and keeping its workers non-generic.
  - implementation_notes: reproduce with `[struct] Box() { [i32 mut] value{3i32} }`, the `show<T>` above printing `cell.locked(body)`, and `main` making `cell{Mutex<Box>{Box{}}}`. The lambda is lifted to `/show__lambdaN` before monomorphization; see how a generic struct's method substitutes (src/semantics/TemplateMonomorph*.cpp).
  - acceptance:
    - positive: `show<Box>(cell)` prints 10 on the VM and `optexe`; a lambda with a `[T] item` parameter and one that captures a `T` value work the same way
    - negative: a lambda naming a type parameter that the definition does not have is still an unknown type
    - full release gate green
  - stop_rule: type parameters of the enclosing definition in lambdas; inferring `T` through `Mutex<T>` is TODO-5737.

- [ ] TODO-5865: A `Function` local given positionally to a brace constructor is copied
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Closures (docs/spec/value-lifecycle.md, Lambdas; found with `serveWorkers`)
  - parallel_track: closures-lifecycle
  - scope: With `[Function<i32, i32> mut] plain{[seven]([i32] x) { return(x + seven) }}` and a struct `Wrap` whose only field is `[Function<i32, i32> mut] run`, `[Wrap mut] wrap{Wrap{plain}}` followed by `print_line(plain(1i32))` prints 8 and then ends with `VM error: invalid heap free address in IR`: the field and the local share one closure block and both free it. The labeled form `Wrap{[run] plain}` copies the closure and works, and so does `Wrap{move(plain)}`. `Mutex<Function<i32, i32>>{plain}` fails the same way. Make the positional argument take the same copy path as the labeled one.
  - implementation_notes: compare the lowering of a labeled and a positional brace-constructor argument whose source is a named `Function` local (a place, not a temporary); TODO-5727 is the general rule for telling the two apart.
  - acceptance:
    - positive: the `Wrap{plain}` and `Mutex<Function<i32, i32>>{plain}` programs print 8 and exit 0 on the VM and `optexe`, with the heap balanced
    - negative: using `plain` after `Wrap{move(plain)}` is still the use-after-move diagnostic
    - full release gate green
  - stop_rule: positional brace-constructor arguments of `Function` type.


- [ ] TODO-5868: `serve(app)?` in an `if` block of `main` compiles
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Result and control flow (docs/spec/errors-and-file-io.md; found with benchmarks/http/bench_server.prime)
  - parallel_track: result-question
  - scope: With `[return<int> effects(heap_alloc, io_err, net_io) on_error<ServeError, log_serve_error>] main()`, the body `if(flag) { serve(app_router(), config)? return(0i32) } serve(app_router(), config)? return(0i32)` (`app_router()` returns an `AppRouter<Bench>`) is rejected with `missing on_error for ? usage`, reported at stdlib/std/http/http.prime in `/std/http/AppRouter__t.../call`, a definition that has no `?`. The same two `serve(...)?` statements one after the other compile, and so does the `if` when each branch calls a helper that carries its own `on_error`. Not yet narrowed: whether any `?` in a block does this or only one whose call takes a generic struct. Make the `?` inside the block use the enclosing definition's `on_error`, and report a missing one at the `?`.
  - implementation_notes: benchmarks/http/bench_server.prime works around it with `run_serve` and `run_workers`; inline them into `main` to reproduce.
  - acceptance:
    - positive: the program compiles and runs on the VM and `optexe`; bench_server.prime drops its two helpers
    - negative: a `?` in an `if` block of a definition without `on_error` is reported at that `?`, naming that definition
    - full release gate green
  - stop_rule: `?` inside statement blocks with `on_error` on the enclosing definition.

- [ ] TODO-5873: A sum temporary passed as an argument is destroyed after the call
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Result and control flow (docs/spec/errors-and-file-io.md)
  - parallel_track: pick-payloads
  - scope: With `[return<i32>] first_of([Maybe<String>] held)` that picks `held` and returns a byte of the payload, `print_line(first_of(get(8i32)))` (`get` returns a `Maybe<String>` holding a fresh `String`) leaves `stringHeapBytesInUse()` 32 higher on the VM and `optexe`; the same call with a `Maybe<vector<i32>>` leaves `heapLiveBlocks()` one higher. Passing a named sum local is balanced. A sum returned by a call and passed straight to a read-only parameter has no owner: destroy its active payload when the call returns, as for a fresh `vector<T>` passed to a read-only parameter (docs/spec/value-lifecycle.md).
  - implementation_notes: `emitActiveSumPayloadDestroyFromSumPtr` (src/ir_lowerer/IrLowererLowerSumHelpersPickAndDiagnostics.cpp) destroys the payload of a sum local and of a fresh `pick` subject; the inlined-call argument setup in src/ir_lowerer/IrLowererLowerInlineCallsBody.h frees a fresh builtin vector argument.
  - acceptance:
    - positive: both calls leave the heap statistics unchanged on the VM and `optexe`
    - negative: a named sum local passed to the same parameter is still usable after the call and destroyed once
    - full release gate green
  - stop_rule: sum temporaries passed to read-only parameters.

- [ ] TODO-5874: Assigning to a `mut` sum local destroys the payload it held
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Result and control flow (docs/spec/errors-and-file-io.md)
  - parallel_track: pick-payloads
  - scope: `[Maybe<String> mut] held{get(4i32)}` followed by `held = get(6i32)` and `held = none<String>()` leaves `stringHeapBytesInUse()` 64 higher at scope end on the VM and `optexe`: neither assignment destroys the payload the local held. The same with a `Maybe<vector<i32>>` leaves two heap blocks. `assign` of an owning struct value destroys the old value first (docs/spec/value-lifecycle.md); do the same for a sum local.
  - implementation_notes: Sum assignment is lowered in src/ir_lowerer/IrLowererLowerSumHelpersConstructEmit.cpp (`emitExistingSumValueIntoLocal` and the constructor paths write into the local's slots); `emitActiveSumPayloadDestroyFromSumPtr` destroys an active payload.
  - acceptance:
    - positive: the block leaves the heap statistics unchanged on the VM and `optexe`, for a `String`, a struct with `Destroy` and a `vector<T>` payload
    - negative: `held = held` keeps the payload readable
    - full release gate green
  - stop_rule: assignment to sum locals.

- [ ] TODO-5875: `count()` on a local declared `Vector<T>` lowers
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Result and control flow (docs/spec/errors-and-file-io.md)
  - parallel_track: pick-payloads
  - scope: With `import /std/collections/*`, `[Vector<i32>] local{vector<i32>(1i32, 2i32)}` followed by `print_line(local.count())` stops with `VM lowering error: vm backend only supports entry argument indexing`; `vectorCount<i32>(local)` prints 2, and the same binding declared `vector<i32>` reads its count with `local.count()`. The payload binding of a `Maybe<Vector<i32>>` in a `pick` arm fails the same way. Make the method spelling resolve like the helper call.
  - implementation_notes: The error is the fallback of the entry-argument access path (src/ir_lowerer), reached when the `count` method call has no collection or method target; compare the facts published for the `vector<i32>` spelling with `--dump-stage`.
  - acceptance:
    - positive: `local.count()` and `local[1i32]` print 2 on the VM and `optexe` for the `Vector<i32>` local and for a `Maybe<Vector<i32>>` payload binding
    - negative: `local.missing()` is still `unknown method`
    - full release gate green
  - stop_rule: method calls on bindings declared with the stdlib `Vector<T>` spelling.

- [ ] TODO-5876: `return(pick(...))` of a vector and assignment from a `pick` expression
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Result and control flow (docs/spec/errors-and-file-io.md)
  - parallel_track: pick-payloads
  - scope: In a definition with `return<vector<i32>>`, `return(pick(get_values(first)) { none { vector<i32>{} } some(values) { values } })` is rejected with `return type mismatch: expected vector`; binding the `pick` to a local and returning the local works, and so does the same `return` with `String` arms. And `kept = pick(get(3i32)) { none { label(0i32) } some(line) { line } }` for a `[String mut] kept` (the same for a `vector<i32>`) is rejected with `unable to infer implicit template arguments for /std/maybe/some`, though no `some` call is in the statement. Accept both; the returned or assigned value is the `pick`'s own (docs/spec/value-lifecycle.md), and the assignment destroys what the local held.
  - implementation_notes: The return check is in src/semantics/SemanticsValidatorStatementReturns.cpp (the return kind of a `pick` expression with collection arms); `pick` branch typing is in src/semantics/SemanticsValidatorExprPick.cpp. `tryEmitPickExpr` (src/ir_lowerer/IrLowererLowerSumHelpersPickAndDiagnostics.cpp) builds the result.
  - acceptance:
    - positive: both programs print the picked value on the VM and `optexe` with the heap statistics balanced, for a `String` and a `vector<i32>`
    - negative: `return(pick(...))` with `i32` arms from a `return<vector<i32>>` definition is still a return type mismatch
    - full release gate green
  - stop_rule: `pick` expressions as return and assignment values.

- [ ] TODO-5881: A call takes a call to the same template instantiation as its argument on the VM
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Result and control flow (docs/spec/errors-and-file-io.md)
  - parallel_track: pick-payloads
  - scope: `some<String>(first_or(get(3i32)))`, where `get` itself returns `some<String>(...)` and is inlined into the argument, is rejected with `vm backend does not support recursive calls: /std/maybe/some__t...`; so is `some<vector<i32>>(pick(get_values(id)) { ... })`. The call is not recursive: one instantiation of `some` appears inside the argument of another call to it. Binding the argument to a local first works.
  - implementation_notes: the inline-call stack of the lowerer (src/ir_lowerer/IrLowererLowerInlineCallsBody.h) holds the outer call while it lowers the arguments; the arguments are evaluated before the callee's body and are not inside it.
  - acceptance:
    - positive: both programs print their value on the VM and `optexe` with the heap statistics balanced
    - negative: a definition that calls itself is still rejected on the VM
    - full release gate green
  - stop_rule: calls to one definition inside an argument of a call to it.

- [ ] TODO-5878: A `[move name]` lambda capture compiles
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Result and control flow (docs/spec/errors-and-file-io.md)
  - parallel_track: pick-payloads
  - scope: `[i32 mut] n{4i32}` followed by `[mut] show{[move n]([i32] more) { return(n + more) }}` is rejected with `unable to infer implicit template arguments for /main__lambda0_make`; the same for a `String`, a `vector<i32>` and a `pick` payload binding. docs/spec/value-lifecycle.md lists `[move name]` as a capture form, and no test uses it. Make the capture move `name` into the closure, so that `name` is moved-from afterwards and the closure destroys the value.
  - implementation_notes: `desugarLambda` (src/semantics/SemanticsValidateLambdaClosures.cpp) passes `move(name)` to the closure's factory; the factory's template inference does not see through the `move` call.
  - acceptance:
    - positive: the program prints 5 on the VM and `optexe`; a moved `String` and a moved `vector<i32>` print their value with the heap statistics balanced
    - negative: a use of `name` after the lambda is `use-after-move`
    - full release gate green
  - stop_rule: `[move name]` captures.

- [ ] TODO-5871: `serveWorkers` answers more new connections a second than one thread can accept
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Multithreading and Web apps (docs/WebAppsPlan.md, Concurrency)
  - parallel_track: mt-server
  - scope: With benchmarks/http (16 clients, a new connection per request) every server tops out near 14,600 requests a second on the 11-processor arm64 machine, whatever the request costs: a POST of 8 bytes gets 15,450 from `serve` and 14,620, 14,718 and 14,656 from 1, 2 and 4 workers, while the server's CPU per request (62 us user, 68 us sys with 4 workers) would allow several times that. The 32 KB form with 4 workers (13,329, 2.41x `serve`) sits at nine tenths of that ceiling, which is why it misses 2.5x: 3 workers reach 12,585 and 4 only 13,167. What sets the ceiling was not measured (no sampling of the threads): the one thread that polls, accepts one connection per round and hands it over, the kernel's loopback connection setup, or the Python load generator. Measure which (per-thread CPU of the poller, or a load generator that keeps connections open against a server that allows it, TODO-5805), then raise it if it is the server's: accept in more than one thread, or hand over more than one connection per round (tried once here with no gain at this ceiling).
  - acceptance:
    - the measured cause of the ceiling is recorded in docs/WebAppsPlan.md (Measured)
    - if the cause is in `serveWorkers`: `python3 benchmarks/http/run_http_benchmarks.py` on an idle machine with at least 6 processors shows the 32 KB form at least 2.5x `serve` with 4 workers, and the table in docs/WebAppsPlan.md is replaced; if it is the load generator or the kernel, the target is restated against what the setup can show
    - no gate test measures time; full release gate green
  - stop_rule: the connection-rate ceiling of `serveWorkers`; the list page waits for read-only handlers (TODO-5804).

- [ ] TODO-5872: A thread reads the bytes of a block another thread allocated as fast as its own
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Multithreading (docs/spec/vm-design.md, heap arenas; found measuring `serveWorkers`)
  - parallel_track: mt-heap
  - scope: On `optexe` a byte load or store checks its address against the calling thread's arena in line (`byteAt` in include/primec/runtime/VmHeapCoreBody.inc); an address of another thread's arena misses that check and goes through `byteAtEdge`, the out-of-line `byteAtSlow`, `ownerOf` and the owner's `byteAtOwn` for every byte. Measured with benchmarks/http: a `serveWorkers` worker that parsed a 32 KB form in the slot block the polling thread allocated spent 327 us of user time per request against 146 us for `serve`; parsing from a copy in a block of its own brought it to 160 us, which is what `work_slot` in stdlib/std/http/server.prime now does. Make the check of a remote byte region cheap (for example one more in-line compare against the last remote arena that answered), so shared read-only data needs no copy, and then drop the copy in `work_slot` if it no longer pays.
  - acceptance:
    - an `optexe` program in which a task sums the bytes of a 1 MiB `String` the main task built retires at most 1.2x the instructions of the main task summing it itself (measured by hand, recorded in the result note)
    - benchmarks/strings instruction counts do not rise by more than 1%
    - no gate test measures time; full release gate green
  - stop_rule: byte-region checks of another arena's addresses on `optexe`; slot regions and the VM are not included.

- [ ] TODO-5870: A VM program that uses tasks runs its string code on the fast kernel
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Multithreading (docs/spec/vm-design.md, Tasks; found measuring the string heap, TODO-5867)
  - parallel_track: vm-tasks
  - scope: A module with a `TaskSpawn` or `TaskWait` anywhere runs every function on the VM's step kernel under the task scheduler (src/runtime/VmFastKernelPrepare.cpp keeps tasks, atomics, mutexes and conditions off the fast kernel; `moduleUsesTasks` in src/runtime/VmExecutionKernel.cpp). Measured on arm64 with `primevm` (instructions retired): a loop that builds and drops 200000 `String`s (a 3-byte text and four `append`s, 77 bytes) retires 13.4e9 in a program without tasks and 153.4e9 when the same loop runs in a spawned task, and the same 153.4e9 on the main task of a program that spawns one unrelated `seven()` first. A loop of integer arithmetic costs the same either way (55e6 for 200000 rounds), so the cost is in calls or in the host calls of `String`, not in the scheduler's loop. Find which instructions cost 700000 host instructions per string on the step kernel and either run the functions that contain no switch point on the fast kernel between switch points or make those instructions cheap.
  - implementation_notes: the loop is the one in the result note of the string heap task (docs/todo_archive/); the heap itself takes no region per string any more, so the difference is all kernel. `optexe` runs the same loop in 3.2e9 (main) and 3.4e9 (task).
  - acceptance:
    - the string loop above on a spawned VM task retires at most 2x the instructions of the same loop in a program without tasks (measured by hand, recorded in the result note)
    - the deterministic schedule is unchanged: the seeded task tests print what they print today
    - full release gate green
  - stop_rule: VM execution speed of programs that use tasks; no new language surface.

- [ ] TODO-5799: arm64 native runs spawned tasks on pthreads, its heap behind a lock
  - owner: ai
  - status: ready
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Runtime Model and Heaps)
  - parallel_track: mt-native
  - scope: Bind `pthread_create`, `pthread_join` and `pthread_attr_init`/`pthread_attr_setstacksize` from libSystem through the existing dyld pointer slots, and emit a thread trampoline (C ABI: saves callee-saved registers, sets x19/x20 from the saved argc/argv, sets up a frame with its own 1 MiB value stack on an 8 MiB thread stack, copies the arguments from the task block, calls the function, stores the result). `TaskSpawn` starts it and `TaskWait` joins. In a module with `TaskSpawn` the heap routines take one LSE spin lock; a module without it emits today's code byte for byte.
  - implementation_notes: the trampoline must establish whatever register state the arm64 register allocator assumes at a function entry (since TODO-5772: x27 frame, x28 value stack, x19/x20 argc/argv while some function reads them; allocated functions use x21-x25 and x3-x15 freely, NativeEmitterInternalsArm64RegAlloc.h); src/native_emitter/NativeEmitterMachO.cpp (imports), NativeEmitterInternalsArm64Heap.h, NativeEmitterInternalsArm64Host.h.
  - acceptance:
    - positive: two one-second `sum_range` tasks finish in at most 0.65 times the one-task time (back to back, load recorded); every VM task test gives the same output on native; 4 tasks allocating and freeing 100000 blocks each end with the heap balanced
    - negative: a heap fault in a task exits 3 with the VM's message; PrimeScroll's `__text` and launch library count are unchanged (the TODO-5603 guard)
    - full release gate green on macOS arm64
  - stop_rule: threads and a locked heap on arm64 native; per-thread heaps are TODO-5800.

- [ ] TODO-5800: arm64 native heaps are per thread, with remote frees
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5799
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Heaps)
  - parallel_track: mt-native
  - scope: Replace the lock of TODO-5799: each spawned thread takes an allocator state (today's layout plus a remote-free head) from a pool that is never unmapped and finds it through a pthread key (`pthread_getspecific`, null meaning the main thread's data-segment state); pooled chunks are mapped aligned to 256 KiB and start with their owner's state pointer; `HeapFree` of another thread's block pushes it onto the owner's remote list with `casal`, and the owner takes that list back with `swpal` when a size class is empty. The lock remains only for taking a state from the pool.
  - acceptance:
    - a 4-thread benchmark that allocates and frees small blocks (a quarter freed by another thread) runs at least 3x faster than with the TODO-5799 lock, and a 1-thread run is no slower than before TODO-5799
    - positive: blocks made on a thread that has ended are freed by another without a fault; negative: a double free across threads still exits 3 with `invalid heap free address`
    - full release gate green on macOS arm64
  - stop_rule: the native allocator.

- [ ] TODO-5801: arm64 native runs `Atomic`, `Mutex` and `Channel`
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5800
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Atomic, Mutex, Channel)
  - parallel_track: mt-native
  - scope: Emit the atomic opcodes with LSE (`ldar`/`stlr`, `ldaddal`, `swpal`, `casal`) and answer the lock and condition host functions with `pthread_mutex_*` and `pthread_cond_*` bound from libSystem, so the `/std/sync` types and `SharedString` run on arm64 native.
  - acceptance:
    - positive: the `Atomic` program-matrix tests (tests/unit/program_matrix/test_program_matrix_atomics.cpp) and the `Mutex`, `Channel` and `SharedString` tests (tests/unit/program_matrix/test_program_matrix_shared_strings.cpp) of TODO-5797 and TODO-5798 pass on native with the `optexe` output
    - negative: a nested `locked` on one mutex and a send after close exit 3 with the VM's messages
    - full release gate green on macOS arm64
  - stop_rule: the primitives on arm64 native; x86_64 native stays inline.

- [ ] TODO-5804: GET handlers that only read the state run at the same time under `RwLock`
  - owner: ai
  - status: deferred
  - created_at: 2026-11-09
  - phase: Multithreading and Web apps (docs/MultithreadingPrototype.md, The Server)
  - parallel_track: mt-server
  - depends_on: TODO-5735
  - scope: Under `serveWorkers` every handler holds the app's mutex, so page rendering runs one request at a time. Add `RwLock<T>` (`read(body)` with a read-only `T`, `write(body)` with a `Reference<T>`), let a handler say it only reads (its state parameter `[State] state` against `[Reference<State>] state`, which needs same-arity overloads by argument type, TODO-5735; or a named registration, open question 5 of the design), and run read-only handlers under the read lock. Deferred: decide the registration form first.
  - acceptance:
    - the CPU-bound GET handler of benchmarks/http (`GET /work`; 0.98x measured 2026-11-11, docs/WebAppsPlan.md, Measured) at least 2.5x `serve` with 4 workers, and todo_web's list page with 200 items (0.95x measured) at least 1.5x; a read-only handler that writes its state is a compile error
  - stop_rule: read-only handlers and `RwLock`.

- [ ] TODO-5805: The server keeps connections alive between requests
  - owner: ai
  - status: deferred
  - created_at: 2026-11-09
  - phase: Web apps (docs/WebAppsPlan.md, Concurrency)
  - parallel_track: web-keepalive
  - scope: `serve` and `serveWorkers` close every connection after one response. Honor HTTP/1.1 persistent connections (`Connection: keep-alive` by default, `close` on request, an idle timeout and a request cap per connection), with an answered connection going back to the poller. Deferred until the worker server exists.
  - acceptance:
    - to be set when the leaf is un-deferred; at least: a client reusing one connection for 100 requests gets 100 answers, and an idle connection closes after its timeout
  - stop_rule: persistent connections.

- [ ] TODO-5806: Sessions: a cookie and per-session state
  - owner: ai
  - status: deferred
  - created_at: 2026-11-09
  - phase: Web apps (docs/WebAppsPlan.md, Concurrency)
  - parallel_track: web-sessions
  - scope: Give `WebApp<State>` sessions: a random session id in a cookie (HttpOnly, SameSite), a session store in the app state with expiry, and handlers that receive their session's data. Deferred: needs the cookie parsing and a random source, and the design of how a handler names its session type.
  - acceptance:
    - to be set when the leaf is un-deferred
  - stop_rule: sessions in `WebApp`.

- [ ] TODO-5821: The first toolbar of a launch does not hold up the first frame
  - owner: human
  - status: deferred
  - created_at: 2026-11-09
  - phase: Native PrimeScroll startup (docs/NativeUiPlan.md section 1)
  - parallel_track: startup-toolbar
  - scope: A `.prime` or markdown file opened at launch spends 45 to 70 ms in `ps_ui_window_set_toolbar` before the first frame: the first `NSToolbar` of a process costs that much whatever it holds (measured 2026-11-09 in a scratch AppKit program: an empty toolbar, items without images, every `toolbarStyle`, and a titlebar accessory view of buttons instead were no cheaper). Choose between attaching the toolbar right after the first frame (the window then grows its title bar a frame later: a visible shift), drawing the controls inside the content view, or keeping it before the frame.
  - acceptance:
    - opening a `.prime` file at launch spends at most 15 ms of ps_ui calls on its toolbar before the first frame, or the plan records why it stays (the person's choice); full release gate green
  - stop_rule: the launch document's toolbar only.
  - notes: deferred for the user's choice: the cheap options change what the first frame shows.

- [ ] TODO-5603: The real-launch baseline, measured by a person
  - owner: human
  - status: ready
  - created_at: 2026-10-26
  - phase: Native PrimeScroll startup (docs/NativeUiPlan.md section 1)
  - parallel_track: startup
  - scope: The audit, the guard (CTest `PrimeStruct_native_ui_startup_guard`: images beyond a plain AppKit program) and `scripts/measure_startup.sh` are done (docs/todo_log.md under TODO-5603); `scripts/measure_app_startup.sh` is gone. Left, for a person at the Mac (an `--open` launch shows the window and takes focus, which agents and tests must not do), after reinstalling PrimeScroll and running `bash scripts/warm_macos_app.sh ~/Applications/PrimeScroll.app` on the installed copy: (1) the Launch Services baseline, `scripts/measure_startup.sh --open --runs 10 ~/Applications/PrimeScroll.app` and `scripts/measure_startup.sh --open --runs 10 --file <a small .txt> ~/Applications/PrimeScroll.app`; (2) the first launch after an install (TODO-5755), `scripts/measure_startup.sh --open --runs 1 ~/Applications/PrimeScroll.app` twice right after the install and warm-up (the warm-up line of the first is the one to compare; expected within 50 ms of the second); (3) a sampled real launch for TODO-5756, `scripts/measure_startup.sh --open --sample --runs 3 ~/Applications/PrimeScroll.app` with load under 4 (expected `app_init` at most 150 ms), and in the unified log (`log show --last 2m --predicate 'process == "PrimeScroll" OR process == "SafariPlatformSupport.Helper"'`) no AutoFill helper, Writing Tools or window restoration before the first drawn text.
  - acceptance:
    - the numbers of (1) to (3), with the load average, are in docs/NativeUiPlan.md section 1 and docs/todo_log.md; anything over its expectation gets a TODO
  - stop_rule: measurement only.
  - notes: needs a person at the Mac; the guard counts images, not milliseconds.

- [ ] TODO-5586: Native String heap: pooled blocks, statistics and limit
  - owner: ai
  - status: deferred
  - created_at: 2026-10-23
  - phase: Native PrimeScroll (docs/NativeUiPlan.md)
  - parallel_track: native-backend-strings
  - scope: The arm64 native String heap gives every text block its own mapping (a page or more each, a syscall per allocation and free). Pool small blocks in 256 KiB chunks per size class as the VM's `VmTextHeap` does, with state in the writable data segment of TODO-5582, and answer `__pstext_stat` and `__pstext_set_limit` ("string memory limit exceeded"); freed blocks are zeroed and a double free faults.
  - acceptance:
    - the string_heap program of tests/unit/backend_host/test_cpp_strings.cpp (reuse and figures) also runs through `--emit=native` with the VM's exit codes; a 100000-String loop uses no more than a few MB; full release gate green
  - stop_rule: the native String heap's allocator and its statistics.
  - notes: deferred to keep Ready Now at eight. PrimeScroll ships from the native build, so every String it makes takes a mapping of its own; the writable data segment this needs exists (`MachOData::stateBytes`), and since TODO-5750 slot blocks are pooled there (src/native_emitter/NativeEmitterInternalsArm64Heap.h: chunks shared by all size classes, free lists in the data segment), which text blocks could share. After TODO-5749 the text blocks are most of what native PrimeScroll on handlers still holds above its loop version (untagged dirty memory 0.41 MB in 11 regions against 0.28 MB in 5, of a 0.2 MB footprint gap).

- [ ] TODO-5536: Small, lazily loaded bytecode for apps
  - owner: ai
  - status: deferred
  - created_at: 2026-10-08
  - phase: Native UI (docs/NativeUiPlan.md)
  - parallel_track: bytecode-startup
  - scope: App startup is dominated by bytecode size: a program using the PNG decoder lowers to 12.7 MB of PSIR (lowering inlines almost every call) and takes ~80 ms to load on the runtime-only VM, against ~1 ms for a small program. Add a startup benchmark (bytecode size and load+run time of `embed_bytecode_runner` for a small, a medium and the PNG program) to the benchmark suite, then cut both: a size-oriented lowering mode for app bundles (real calls instead of inlining beyond a size threshold) and lazy decoding/validation of function bodies on first call.
  - acceptance:
    - the startup benchmark is in `scripts/benchmark.sh` with a baseline; the PNG program's bytecode shrinks at least 4x and loads at least 4x faster, with identical program output
    - full release gate green
  - stop_rule: bytecode size and load time only; split lazy loading out if the size change is large.
  - notes: deferred out of the startup lane on 2026-11-09: PrimeScroll ships the direct native build, which has no bytecode, so this is not on its launch path (the startup audit of TODO-5603 found the 0.79 MB executable's size makes no measurable difference warm). It matters for bytecode bundles (`primestruct_app_runtime` with `app.psir`).

- [ ] TODO-5535: Launch-time harness and reference Objective-C++ editor
  - owner: ai
  - status: ready
  - created_at: 2026-10-08
  - phase: Native UI (docs/NativeUiPlan.md)
  - parallel_track: native-ui-launch
  - scope: `scripts/measure_startup.sh` and its probe (TODO-5603) time PrimeStruct apps in snapshot mode and through Launch Services. Left: a reference editor written directly in Objective-C++ (`tools/launch_bench/reference_editor.mm`: the same window, tab, menu, toolbar and text view setup as PrimeScroll and the startup rules of section 1, with its own phase marks in the probe's format), so the budget "within 5 ms of a pure Objective-C++ editor" can be checked; and a cold-launch mode for the script (a launch after `purge` is not possible without root, so a fresh copy of the bundle at a new path, which also measures new-code assessment, and a person's run after a reboot). Compare PrimeScroll, the reference and TextEdit (`--open` against `/System/Applications/TextEdit.app` with window polling for apps without the probe).
  - acceptance:
    - the reference editor builds with clang from the script; a table of PrimeScroll, reference and TextEdit (snapshot where possible, `--open` for all) is recorded in docs/NativeUiPlan.md section 1; PrimeScroll's snapshot `text` median is within 30 ms of the reference (2026-11-09 with a one-text-view prototype: 217 against 192 ms) or each gap has a TODO
    - full release gate green
  - stop_rule: measurement tooling and the reference editor only.
  - notes: needs a Mac; the `--open` runs need a person at the machine (they show a window and take focus).

- [ ] TODO-5808: `ps_ui_*` entry points fault when called off the main thread
  - owner: ai
  - status: ready
  - created_at: 2026-11-09
  - phase: Native UI (docs/NativeUiPlan.md, The C ABI; docs/MultithreadingPrototype.md, Sendable Values)
  - parallel_track: native-ui-threads
  - scope: The C ABI says every function is callable only on the thread that called `ps_ui_init` (AppKit objects belong to the main thread), but nothing checks it: a UI handle is a `u64` and sendable, so once tasks run on threads (TODO-5795, TODO-5799) a spawned task can call `ps_ui_window_add_tab` on a worker thread and corrupt AppKit state. The multithreading design has no type marker for UI handles; the check is at run time. The AppKit backend records the calling thread in `ps_ui_init` and every other entry point faults (exit code 3, through the program's fault path) with `ui call off the main thread: ps_ui_window_add_tab` when called from another thread; the headless backend does the same, so the rule is testable without a screen. Cost: one thread-id compare per call.
  - implementation_notes: src/ui/appkit/NativeUiAppKit.mm (`ps_ui_init`; `pthread_main_np` or the recorded `pthread_self`), src/ui/headless/NativeUiHeadless*.cpp, the binding layer in src/ui/NativeUiBindings.cpp if the check is shared; tests in tests/unit/ui/test_native_ui_headless.cpp.
  - acceptance:
    - positive: a headless test initializes the UI and calls `ps_ui_window_create` and `ps_ui_window_add_tab` on the same thread and succeeds; the PrimeScroll scenarios are unchanged
    - negative: a headless test initializes the UI on one `std::thread` and calls `ps_ui_window_add_tab` from another, and observes the fault with that message through a fault hook the headless backend gets for this (a callback the test installs; without one it exits 3 like the program's fault path); `ps_ui_init` itself called again from a second thread is the same fault
    - full release gate green
  - stop_rule: the thread check in the two backends; no marker in the type system.

- [ ] TODO-5533: Windows and Linux native UI backends
  - owner: ai
  - status: deferred
  - created_at: 2026-10-08
  - phase: Native UI (docs/NativeUiPlan.md)
  - parallel_track: native-ui-platforms
  - scope: Implement the version-0 ABI with Win32 common controls and with GTK 4, following the coverage matrix in docs/NativeUiPlan.md section 9; split per platform before starting.
  - acceptance:
    - the editor scenarios pass on each platform by hand; the coverage matrix is updated to what each backend does
    - full release gate green
  - stop_rule: split into one leaf per platform before implementation.
  - notes: deferred: after the macOS editor works.

- [ ] TODO-5524: Int-backed error structs round-trip through stdlib Result sums
  - owner: ai
  - status: ready
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: result-error-structs
  - scope: With `/std/result/*` imported, a sum payload of `ContainerError`/`ImageError`/`GfxError` is stored as a scalar i32 (`valueKindFromTypeName` treats them as int-backed), but `[error] ContainerError{2i32}` stores the struct's address instead of its code; `Result.why(r)` passes the payload slot to `why()` as if it were struct storage (prints `container error` for a missing key); `pick(r) { error(e) { e.code } }` fails with "field access requires struct receiver"; the `on_error` handler receives the address as the code (`err.code` prints 560). Pick one payload storage (the code, or inline struct storage) and use it in construction, `why`, `pick` and `try`.
  - acceptance:
    - matrix cases on VM, native and C++: `error<i32, ContainerError>(ContainerError{2i32})` read through `pick`, `Result.why`, `try` with an `on_error` handler, and `m.tryAt(missing)` through `Result.why`
    - full release gate green
  - stop_rule: int-backed error struct payloads only.

- [ ] TODO-5523: A Result-returning main exits with its error code
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: result-main
  - scope: `[return<Result<i32, i32>>] main() { return(error<i32, i32>(3i32)) }` exits 0 on VM/C++ and 32 on native: the entry returns the raw sum pointer (or, without the import, the packed value whose low 32 bits are 0). Specify the exit code of a Result-returning entry (0 for ok, the error code or 1 otherwise) in docs/spec and decode the entry's return value on every backend.
  - acceptance:
    - spec states the rule; matrix cases for ok, int error and struct error agree on VM, native and C++
    - full release gate green
  - stop_rule: entry-point Result exit codes only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5525: `Result.ok(x)` passes as a stdlib Result argument
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: result-arguments
  - scope: With `/std/result/*` imported, `show(Result.ok(3i32))` for `show([Result<i32, i32>] r)` fails to lower ("struct parameter type mismatch: expected /std/result/Result__..., got <unknown>") because `Result.ok` in argument position still produces the packed value; `/std/result/ok<i32, i32>(3i32)` works. Construct the sum when a legacy `Result.ok` call feeds a stdlib Result parameter (or any sum-typed slot).
  - acceptance:
    - matrix case passing `Result.ok(...)` to a stdlib Result parameter on VM, native and C++
    - full release gate green
  - stop_rule: `Result.ok` arguments only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5526: Vectors of stdlib Result values keep their elements
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: result-containers
  - scope: `vector<Result<i32, i32>>` with the Result import pushes each sum's address instead of its storage: reading `rs[0i32]` back prints `2147483647` from `pick`, and `Result.error` reads garbage (probe `vr2`/`vr3`). Store sum elements inline (like struct elements) or reject the element type with a diagnostic.
  - acceptance:
    - matrix case pushing ok and error Results and reading them back with `pick` and `Result.error` on VM, native and C++, or a diagnostic test if rejected
    - full release gate green
  - stop_rule: Result elements in vectors only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5515: Native Result.ok(Buffer) reads as ok
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: native-result
  - scope: On native, `[Result<Buffer<i32>, GfxError>] s{Result.ok(b)}` then `Result.error(s)` prints 1 (VM/C++ print 0): the payload is packed as a stack address, and native addresses exceed 2^32.
  - acceptance:
    - matrix case agrees on VM, native and C++
    - full release gate green
  - stop_rule: covered by the convention chosen in TODO-5506 if possible.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5516: Integer narrowing and float-to-int conversion agree across backends
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: numeric-conversions
  - scope: After `[i32] x{convert<i32>(4294967297i64)}`, `x` prints 1 but `x == 1i32` is false and `convert<i64>(x)` is 4294967297 (VM/native keep high bits; spec says i32 values wrap mod 2^32). Out-of-range finite float-to-int conversions give a different answer on each backend, and optexe emits C++ undefined behavior (`static_cast<uint64_t>(-1.0)`), which crashed one program. Define and implement one rule (saturate, like the VM for i32, or a runtime fault).
  - acceptance:
    - spec states the rule; matrix cases for narrowing and out-of-range conversions agree on VM, native, C++ and optexe
    - full release gate green
  - stop_rule: conversions only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5517: Runtime faults exit the same way on every backend
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: runtime-faults
  - scope: Integer division by zero exits 3 with a message on the VM but raises SIGFPE on native and C++ (losing buffered output on C++); C++ reports runtime errors with exit 1 instead of 3; native at 100000 recursion levels segfaults instead of reporting overflow. vm-design.md lists division by zero as a runtime fault with exit 3.
  - acceptance:
    - division by zero and the C++ runtime-error exit code match the VM (exit 3, message on stderr)
    - full release gate green
  - stop_rule: fault reporting only; recursion limits stay backend-specific but documented.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5518: Valid programs the frontend rejects compile
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: frontend-accept
  - scope: Rejected though valid per the spec: an unused `import /std/result/*` or `/std/maybe/*` (`unknown import path`, no location); `return(move(v))` and `return(Holder{move(n)})` (`use-after-move`); an `if` yielding struct values (`if branches must return compatible types`); sum-typed struct fields (`missing struct field info`); `try(...)` as an argument to a user function (`missing on_error for ? usage`); `return([ok] value)` and `return(Result<i32,i32>{[error] 7i32})`; `assign(x, Val{none})` / `x = Val{[num] 3i32}` on a sum-typed local (`assign requires matching struct value`).
  - acceptance:
    - each form compiles and runs; semantics tests
    - full release gate green
  - stop_rule: acceptance of these forms; split per form if needed.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5519: Operator rewriting handles calls, nested `!` and `?` precedence
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: text-filter
  - scope: `-id(5i32)` becomes `negate(id)(5i32)`; `print_line(!(id(!true)))` leaves the inner `!` unrewritten (`invalid character '!'`); `x? + 1i32` and `x? * 3i32` are parse errors and `1i32 + x?` parses as `plus(1, x)?`; `try(f()).field` and `f()?.field` fail with `field access requires struct receiver`.
  - acceptance:
    - each form rewrites to the expected canonical call; text-filter tests
    - full release gate green
  - stop_rule: text filter only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5520: Diagnostics point at user code with readable names
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: diagnostics
  - scope: Many lowering errors carry no file/line (`missing on_error for ? usage`, `requires int-backed stdlib Result error payloads`, `missing semantic-product ...`, `argument count mismatch for /onErr`); C++ errors say `native backend`; messages show mangled names (`Result__arity2__ta77c...`, `Vector__t...`); source snippets show the text-filtered form instead of what the user wrote; an unknown `break` is reported inside stdlib/std/file/file.prime; `borrowed binding: v (root: v, sink: v)` names the root as the sink.
  - acceptance:
    - each listed diagnostic has a user-file location, the user's spelling and demangled names; diagnostics tests
    - full release gate green
  - stop_rule: diagnostic text and spans only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5521: Math helpers return correct results
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: stdlib-math
  - scope: Consistent on all backends but wrong: `atan2(1, -1)` gives 2.2749 (2.3562), `copysign(3, -0.0)` gives +3, `round(0.49999999999999994)` gives 1, `fma` is not fused.
  - acceptance:
    - each returns the IEEE/libm result on VM, native and C++; matrix cases
    - full release gate green
  - stop_rule: stdlib math only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5483: Verify arm64 SextI32 on a macOS machine
  - owner: ai
  - status: deferred
  - deferred_reason: needs an arm64 macOS machine; the Linux x86_64 session cannot run `Arm64Emitter` output.
  - created_at: 2026-10-03
  - phase: Optimizing backends
  - parallel_track: ir-semantics
  - scope: `Arm64Emitter::emitSextI32` (SXTW x0, w0, encoded 0x93407C00) was written from the encoding and never executed. The i32 builtins audit is done: increment, decrement, abs and pow emit SextI32, and integer lerp, saturate, clamp, min, max and sign already agree at the limits on every backend (matrix cases `i32_wrap_builtins` and `i32_limit_builtins`; lerp and clamp go through plus/minus/multiply, which wrap).
  - acceptance:
    - the `i32` matrix cases (`i32_wrap_basic`, `i32_wrap_loops`, `i32_wrap_builtins`, `i32_limit_builtins`) pass on arm64 macOS native; an encoding unit test pins the SXTW bytes (done: `primestruct.ir.native_codegen` checks SXTW and the float-compare branch conditions through `primec/testing/NativeEmitterEncodings.h`; the float compares now use MI/LS so NaN compares false, also unexecuted)
  - stop_rule: do not change i64/u64 behavior or the I32 arithmetic opcodes themselves; lowering also uses them for address arithmetic.

- [ ] TODO-5543: Repair or retire the disabled Apple/arm64 native backend test shards
  - owner: ai
  - status: deferred
  - deferred_reason: needs an arm64 macOS machine to run; the Linux x86_64 session compiles these suites out.
  - created_at: 2026-10-08
  - phase: macOS support
  - parallel_track: native-arm64-tests
  - scope: `primestruct.compile.run.native_backend.{core,math_numeric,collections}` are compiled only on Apple/arm64 and were never registered with CTest, so they drifted. They are now registered for Apple/arm64 in `CMakeLists.txt` (shards `*_apple_core_*`, `*_apple_math_numeric_*`, `*_apple_collections_*`), and the 30 shards listed in `PrimeStructAppleNativeKnownFailingShards` are `DISABLED`: stale expected diagnostics (`compile ... == 2` with a changed message), native arm64 exit-code mismatches in `math_numeric_types.h`, and collection shadow/shim cases. Run each disabled shard on a Mac, then fix the expectation, fix the native arm64 backend, or delete the case, and remove the shard from the list.
  - acceptance:
    - `PrimeStructAppleNativeKnownFailingShards` is empty and all `apple_*` shards pass on arm64 macOS
    - full release gate green on Linux and macOS
  - stop_rule: tests and arm64 native emitter output only; no language semantics changes.

- [ ] TODO-5544: `Type.staticHelper()` resolves through `import`
  - owner: ai
  - status: deferred
  - created_at: 2026-10-08
  - phase: Native UI (docs/NativeUiPlan.md)
  - parallel_track: pick-bindings
  - scope: a static helper of an imported `[public struct]` called as `Type.staticHelper()` does not resolve through `import`, which `App.start(...)` of the native UI found. (The other half found with it, a method call on a `pick` payload binding, is done.)
  - acceptance:
    - a static helper of an imported struct is called as `Type.helper()` in VM, native and C++; a negative test keeps the unknown-method diagnostic
    - full release gate green
  - stop_rule: imported static helpers only.

- [ ] TODO-5545: Record the macOS AppKit manual smoke checklist
  - owner: human
  - status: deferred
  - deferred_reason: needs a person at a Mac keyboard; keystrokes, menu shortcuts and modal panels cannot be driven by the automated smoke test.
  - created_at: 2026-10-08
  - phase: Native UI (docs/NativeUiPlan.md)
  - parallel_track: native-ui-macos-manual
  - scope: Run `scripts/bundle_macos_app.sh examples/native_ui/hello_window.prime Hello`, open `build-release/apps/Hello.app` and check by hand: typing marks the window edited, Cmd+S and the Edit menu items work (undo, copy, paste, select all, find), the open and save panels and the alert answer correctly, Cmd+Q and the close button reach the program as events. Record the result in docs/NativeUiPlan.md section 12.
  - acceptance:
    - each item is recorded as passed or filed as a follow-up TODO
  - stop_rule: recording the manual run only.

- [ ] TODO-5841: `[namespace] name { ... }` is a definition
  - owner: ai
  - status: ready
  - created_at: 2026-11-10
  - phase: AI workflow (docs/AiWorkflowPlan.md section 6.5)
  - parallel_track: namespace-envelope
  - scope: A definition body already works as a namespace: in `[return<void>] demo() { [return<i32>] one() { return(7i32) } }` the member is `/demo/one`, siblings call each other by bare name, `import /demo/*` works, and containers nest. But `/demo` is then a callable void function, it cannot be opened twice ("duplicate definition: /demo"), and `[namespace] demo() { ... }` fails with "expected transform identifier" because `namespace` is a keyword token. Add `namespace` as a definition transform: `[namespace] demo { ... }` (and `demo() { ... }`, a slash-path name, and a nested one) is a definition whose body holds only definitions. It takes no parameters, template list or `return`; a binding or statement in its body is a diagnostic; calling it or using it as a type is a diagnostic; its members get no implicit `this`. Definitions with `namespace` at one path are one namespace: the parser folds them into the first, in source order, with their transform lists joined. A `[namespace]` definition may share its path with a struct, enum or function (as `namespace P { ... }` next to `[struct] P` does today) only when its list is `namespace` alone. It lowers to nothing.
  - implementation_notes: `parseTransformList` must take the `KeywordNamespace` token as a transform name. Top-level definitions already accept a body without `()`; `tryParseNestedDefinition` (src/parser/ParserCoreDefinitionBodies.cpp) takes a nested `name { ... }` only for the `isNoReturnDefinitionTransform` names (struct-like and `sum`), so `namespace` joins that test. Members of a container are already flattened to slash paths with the container left as an empty definition (`--dump-stage ast`), so no AST type is added. Follow how field-less struct definitions are kept out of lowering. The keyword form (`Parser::parseNamespace`) is not touched here.
  - acceptance:
    - `[namespace] demo { ... }` with a function, a struct and a nested `[namespace]` runs on the VM; members resolve by path, by sibling bare name and through `import /demo/*`
    - two `[namespace] demo` blocks merge; `--dump-stage ast` shows one `[namespace] /demo` (golden test)
    - parameters, a template list, `return`, a statement in the body, a call of the namespace, and extra transforms on a path that is also a struct are diagnostics (one test each)
    - `--dump-stage ir-lowered` has no function for the namespace
    - docs/PrimeStruct_SyntaxSpec.md and docs/spec/transforms.md describe the transform with a runnable example
    - full release gate green
  - stop_rule: the `namespace` transform only; the keyword form keeps its own parser path until TODO-5843, and no other transform gets a meaning for a namespace's members.

- [ ] TODO-5843: `namespace name { ... }` is sugar for the `[namespace]` definition
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5841
  - created_at: 2026-11-10
  - phase: AI workflow (docs/AiWorkflowPlan.md section 6.5)
  - parallel_track: namespace-envelope
  - scope: `namespace name { ... }` has its own grammar production (`namespace_decl`) and parser function, which pushes a prefix and leaves no definition. It becomes surface sugar: the parser reads it as `[namespace] name { ... }` and goes through the definition path of TODO-5841, so the grammar has no top-level item besides `import`, definitions and executions. Executions inside a namespace block keep their prefix as today. The stdlib and the examples keep the keyword spelling.
  - implementation_notes: `Parser::parseNamespace` and `namespaceStack_` (src/parser/ParserCoreDefinitions.cpp, ParserCore.cpp). `namespace std` is opened in 50 stdlib files, so the fold of TODO-5841 carries the load. The parser skips `import` inside a namespace block today; keep that.
  - acceptance:
    - `namespace_decl` is gone from the grammar in docs/PrimeStruct_SyntaxSpec.md; section 3.2 gives the keyword form as sugar with its rewritten form
    - `--dump-stage ast` of a keyword namespace shows the `[namespace]` definition (golden test)
    - `--dump-stage ir-lowered` of examples/apps/text_editor/main.prime and of one stdlib-heavy test program is identical before and after (compared through `stripDumpTimings`)
    - full release gate green
  - stop_rule: the keyword's parser path and the grammar text only; no source file is rewritten to the envelope spelling.

- [ ] TODO-5842: `summary("...")` is a checked transform on definitions
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5841
  - created_at: 2026-11-10
  - phase: AI workflow (docs/AiWorkflowPlan.md section 6.2)
  - parallel_track: summary-transform
  - scope: `[summary("One.") return<i32>] one() { ... }` already passes semantics, but only because the transform is not checked: nothing documents it, validates its argument or publishes it. `summary` becomes a documented transform that carries a text for tools and has no effect on generated code. It takes exactly one string literal, appears at most once per list, and is valid on every definition: functions, structs, enums and `[namespace]` definitions (TODO-5841). Blocks of one namespace that carry two summaries are a diagnostic that names both positions. The semantic product publishes the summary of each definition.
  - acceptance:
    - docs/spec/transforms.md documents `summary` with a runnable example
    - wrong argument count, a non-literal argument, a repeated `summary`, and two summaries for one namespace are diagnostics (one test each)
    - `--dump-stage semantic-product` shows the summaries of a function, a struct and a namespace (golden test)
    - `--dump-stage ir-lowered` is identical with and without the summaries (compared through `stripDumpTimings`)
    - full release gate green
  - stop_rule: the `summary` transform only; no summary on fields or parameters, no outline output, no staleness tracking.
