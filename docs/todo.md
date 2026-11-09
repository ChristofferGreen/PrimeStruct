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
| TODO-5773 | arm64 register allocation keeps floats in d-registers | ready | native-arm64-regalloc |
| TODO-5824 | A null or stray local address faults on the VM | deferred | vm-addresses |
| TODO-5727 | Every struct copy site tells a place from a temporary by resolution | deferred | lifecycle-copy-sources |
| TODO-5707 | `Function` types accept `mut` parameters | deferred | closures-mut-params |
| TODO-5708 | Type aliases | deferred | type-aliases |
| TODO-5745 | A labeled value that is a call parses in every position | ready | parser-labeled-values |
| TODO-5746 | A parameterless `mut` method whose body is only bindings is a method | ready | diagnostics |
| TODO-5688 | Calls require the callee's whole active effect set | deferred | effects-transitive |
| TODO-5807 | `[left right] wait(a, b)` destructures the results of several tasks | ready | mt-tasks |
| TODO-5790 | Spawn arguments are checked by type: only sendable values cross to a task | ready | mt-sendable |
| TODO-5791 | Every `Function` is sendable by construction: the conversion rejects non-sendable captures | blocked | mt-closures |
| TODO-5792 | The VM runs spawned tasks under a deterministic scheduler | ready | mt-vm-tasks |
| TODO-5793 | Byte-region accesses check liveness without a map search or a write | ready | mt-heap-optexe |
| TODO-5794 | `optexe` heaps are per-thread arenas with remote frees | blocked | mt-heap-optexe |
| TODO-5795 | `optexe` runs spawned tasks on threads | blocked | mt-optexe-threads |
| TODO-5796 | `Atomic<i32>` and `Atomic<i64>` with the `atomic` effect | blocked | mt-primitives |
| TODO-5797 | `Mutex<T>` with scoped `locked(body)` and the `lock` effect | blocked | mt-primitives |
| TODO-5798 | Bounded `Channel<T>` with the `channel` effect | blocked | mt-primitives |
| TODO-5656 | Thread-safe `SharedString`: an atomic owner count | blocked | string-concurrency |
| TODO-5802 | `serveWorkers`: one thread polls and accepts, N workers answer | blocked | mt-server |
| TODO-5803 | The web apps run on workers and their gain is measured | blocked | mt-server |
| TODO-5799 | arm64 native runs spawned tasks on pthreads, its heap behind a lock | blocked | mt-native |
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
| TODO-5681 | A struct field built from a named vector local owns its storage once | ready | collections-fields |
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
| TODO-5744 | A runtime error in a real call ends the program on exe, Wasm and GLSL | deferred | runtime-faults |
| TODO-5518 | Valid programs the frontend rejects compile | deferred | frontend-accept |
| TODO-5519 | Operator rewriting handles calls, nested `!` and `?` precedence | deferred | text-filter |
| TODO-5520 | Diagnostics point at user code with readable names | deferred | diagnostics |
| TODO-5521 | Math helpers return correct results | deferred | stdlib-math |
| TODO-5483 | Verify arm64 SextI32 on a macOS machine | deferred | ir-semantics |
| TODO-5543 | Repair or retire the disabled Apple/arm64 native backend test shards | deferred | native-arm64-tests |
| TODO-5544 | Method calls on `pick` payload bindings lower | deferred | pick-bindings |
| TODO-5545 | Record the macOS AppKit manual smoke checklist | deferred | native-ui-macos-manual |

### Ready Now

- TODO-5603 (startup): the real-launch baseline, measured by a person
- TODO-5535 (native-ui-launch): launch-time harness and reference Objective-C++ editor
- TODO-5510 (control-returns): `return` returns from unit-variant pick arms correctly
- TODO-5524 (result-error-structs): int-backed error structs round-trip through stdlib Result sums

### Immediate Next 10

1. TODO-5603
2. TODO-5535
3. TODO-5510
4. TODO-5524
5. TODO-5728
6. TODO-5773

### Priority Lanes

- Startup, first (docs/NativeUiPlan.md section 1, "Where a launch goes"): TODO-5603, TODO-5535, TODO-5821
- Native PrimeScroll, direct native backend (docs/NativeUiPlan.md): TODO-5586
- Native UI (docs/NativeUiPlan.md): TODO-5808, TODO-5536, TODO-5533
- Effects (docs/spec/language-core.md, Effects): TODO-5688
- Result and control flow (docs/spec/errors-and-file-io.md): TODO-5510, TODO-5524, TODO-5515, TODO-5523, TODO-5525, TODO-5526
- Memory safety (docs/spec/type-system.md Memory safety): TODO-5511, TODO-5512, TODO-5513
- Backend parity: TODO-5516, TODO-5517, TODO-5744
- Frontend and diagnostics: TODO-5518, TODO-5519, TODO-5520, TODO-5521
- macOS support: TODO-5543, TODO-5545 (need macOS)
- Language gaps found by native UI: TODO-5544, TODO-5745, TODO-5746
- Closures (docs/spec/value-lifecycle.md, Lambdas): TODO-5676, TODO-5677, TODO-5712, TODO-5713, TODO-5714, TODO-5715, TODO-5719; collection fields found by closures and the todo example: TODO-5681, TODO-5682, TODO-5726, TODO-5727, TODO-5728, TODO-5730, TODO-5731, TODO-5733
- Web apps (docs/WebAppsPlan.md; single-threaded, built with `--emit=optexe`): TODO-5738, TODO-5718
- Language surface for handlers (docs/WebAppsPlan.md): TODO-5707, TODO-5708, TODO-5716, TODO-5717, TODO-5721, TODO-5732, TODO-5735, TODO-5736, TODO-5737
- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix; arm64 SextI32 TODO-5483 (needs macOS); VM speed ; passes ; optexe; real calls and struct copies TODO-5739, TODO-5741, TODO-5752; VM address checks TODO-5824; native code quality (section 12, after the startup lane) TODO-5773
- Multithreading (docs/MultithreadingPrototype.md, Threads For Real; docs/WebAppsPlan.md, Concurrency; after the startup lane): TODO-5807, TODO-5790, TODO-5791, TODO-5792, TODO-5793, TODO-5794, TODO-5795, TODO-5796, TODO-5797, TODO-5798, TODO-5656, TODO-5802, TODO-5803, TODO-5799, TODO-5800, TODO-5801; deferred TODO-5804, TODO-5805, TODO-5806

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

Native code quality (docs/OptimizingBackendsPlan.md section 12), after the startup lane (TODO-5770, one-instruction constants, TODO-5777, IR dumps for the `--emit` target, TODO-5772, arm64 register allocation for integer code, TODO-5774, registers across calls, TODO-5775, address-taking functions, TODO-5776, `local-sroa`, and TODO-5771, the value stack of the templates that are left, are done): TODO-5773 (floats in d-registers, with float compares fused into their branches).

Multithreading (docs/MultithreadingPrototype.md, Threads For Real), after the startup lane (TODO-5772 is done); not above them in Ready Now or Immediate Next 10. In this order: TODO-5807 (destructure multi-wait results), TODO-5790 (sendable spawn arguments, checked by type), TODO-5791 (every `Function` sendable by construction), TODO-5792 (VM tasks under a deterministic scheduler; `TaskSpawn`/`TaskWait` IR), TODO-5793 (byte-region checks without a map search), TODO-5794 (`optexe` per-thread heap arenas), TODO-5795 (`optexe` threads for `[spawn]`), TODO-5796 (`Atomic`), TODO-5797 (`Mutex`), TODO-5798 (`Channel`), TODO-5656 (atomic `SharedString` count), TODO-5802 (`serveWorkers`), TODO-5803 (apps on workers, measured), then arm64 native: TODO-5799 (pthreads, locked heap), TODO-5800 (per-thread heaps), TODO-5801 (primitives). TODO-5807, TODO-5790, TODO-5792 and TODO-5793 are ready and independent of each other. Deferred: TODO-5804 (read-only handlers under `RwLock`), TODO-5805 (keep-alive), TODO-5806 (sessions).

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

- [ ] TODO-5773: arm64 register allocation keeps floats in d-registers
  - owner: ai
  - status: ready
  - created_at: 2026-11-09
  - phase: Optimizing backends, native code quality (docs/OptimizingBackendsPlan.md section 12)
  - parallel_track: native-arm64-regalloc
  - scope: arm64 float templates move both operands from general registers to d0/d1 and back for every operation (`fmov`), and float_series runs 8x the cycles of optexe. Colour float-only classes from v8-v31 (saving them around templates and calls), emit float arithmetic, compares, negation and conversions on them, and float constants without a general-register round trip, as the x86_64 allocator does with xmm2-xmm15. A float compare that feeds `JumpIfZero` becomes `fcmp` + `b.cond` (taken over from TODO-5771, whose template operand cache and compare-branch fusion would only have served float functions; docs/OptimizingBackendsPlan.md 12.13).
  - acceptance:
    - float_series `/main` register-allocated on arm64 and at most 2x optexe's cycles, back to back with the load average recorded (2026-11-09: 8.2-8.4x)
    - float compile-run and math conformance suites green on macOS arm64
  - stop_rule: float values in functions TODO-5772 already allocates.

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

- [ ] TODO-5744: A runtime error in a real call ends the program on exe, Wasm and GLSL
  - owner: ai
  - status: deferred
  - created_at: 2026-11-08
  - phase: Backend parity (found while fixing TODO-5743)
  - parallel_track: runtime-faults
  - scope: TODO-5743 made a runtime error (a bounds check, `panic`) inside a real-call body end the program with `Trap 3` on the VM, optcpp/optexe, `--emit=native` and the Linux JIT. The simple C++ emitters (`cpp`, `exe`), Wasm and GLSL have no `Trap` (IrValidation rejects it there), so `emitRuntimeError` (src/ir_lowerer/IrLowererRuntimeErrorHelpers.cpp) still returns 3 from the callee and the caller carries on: the program of "a runtime error inside a real call stops the program" (tests/unit/program_matrix/test_program_matrix_parameter_modes.cpp) prints `20 30`, the error, `3`, `99` and exits 0 on `--emit=exe` (the slow `exe` matrix config, not in the default gate). Lower `Trap` in the simple C++ emitter (`std::fflush(stdout); std::exit(code)` as optcpp does) and in Wasm (a WASI `proc_exit`, or keep such bodies inline), and decide GLSL (no calls that can fault, or inline).
  - acceptance:
    - the matrix case passes with `PRIMESTRUCT_MATRIX_CONFIGS=all` (exe included); a Wasm runtime check of the same program exits 3 where wasmtime is available
    - full release gate green
  - stop_rule: runtime errors in real calls on exe, Wasm and GLSL only.

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

- [ ] TODO-5681: A struct field built from a named vector local owns its storage once
  - owner: ai
  - status: ready
  - created_at: 2026-11-06
  - phase: Closures follow-ups (split from TODO-5668)
  - parallel_track: collections-fields
  - scope: `KeepV{[seen] start}` with `start` a local `vector<i32>` copies the 5-slot vector record into the field with `ownsData` still set in both, so the local and the struct both free the buffer (`VM error: invalid heap free address in IR: 9223372036854775808` on VM; same on optexe). Related: a plain local copy `[vector<i32> mut] b{a}` aliases `a` (after `b.push(2i32)`, `a.count()` is 2, not 1), while a struct copy (`[KeepV mut] copy{k}`) and the stdlib `Vector<T>` spelling copy elements. Brace construction from a named vector local must copy its elements (value semantics, docs/spec/value-lifecycle.md, Copies), and the `vector<T>` local copy must too.
  - implementation_notes: the vector record layout is `IrLowererVectorRecordLayoutHelpers.h` (`ownsData` at slot 4); the struct copy of TODO-5668's matrix case `struct_vector_field_copy_and_closure` already copies a vector field's elements.
  - acceptance:
    - `KeepV{[seen] start}` then pushes on both run on VM, native and optexe with independent counts and no double free; `b{a}` then `b.push` leaves `a.count()` unchanged; a test that the original stays usable
    - full release gate green
  - stop_rule: vector copies at brace construction and local copy bindings; no change to move semantics.

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

- [ ] TODO-5807: `[left right] wait(a, b)` destructures the results of several tasks
  - owner: ai
  - status: ready
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Multi-Wait)
  - parallel_track: mt-tasks
  - scope: The prototype documents `[leftResult rightResult] wait(left, right)` as implemented, but the task-handle check rejects it with `task handles cannot escape their spawning function: left` (checked 2026-11-09 with build-release/primec); only `both{wait(left, right)}` with `both[0i32]` works. Treat a destructuring binding of `wait(...)` like any other consuming `wait`, so the handles are waited and the names bind the results in order.
  - implementation_notes: src/semantics/SemanticsValidatorTaskHandles.cpp (the escape check), the tuple-destructuring binding path; the lazy stdlib import must still find `/std/tuple` for it.
  - acceptance:
    - positive: a semantic test and a VM compile-run test destructure `wait(left, right)` of two `Task<i32>` and of a `Task<i32>` with a `Task<i64>`, and print both results
    - negative: destructuring into three names from two tasks is a diagnostic; using `left` after the destructuring `wait` is `task handle already waited: left`
    - full release gate green
  - stop_rule: destructuring of `wait` results only.

- [ ] TODO-5790: Spawn arguments are checked by type: only sendable values cross to a task
  - owner: ai
  - status: ready
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Sendable Values)
  - parallel_track: mt-sendable
  - scope: `validateTaskSpawnCall` checks only argument names (a `mut` binding, a `Reference`, a task handle, `location`/`borrow`/`dereference`); a `Pointer`, a `StringView`, a borrowing closure or a struct holding one passes, and a copied `mut` binding is rejected although spawn copies its arguments. Add the structural `sendable` predicate of the design (primitives, `string`, enums, `String`, `vector`, `map`, `Maybe`, `Result`, tuples and structs of sendable types, every `Function`; not `Reference`, `Pointer`, `StringView`, slices, a closure struct with a `[ref]` capture, `Task`, `SharedString` until TODO-5656), one `[sendable]` struct transform (asserts it for a struct with `Pointer` fields; outside `stdlib/` only on a definition that also carries `unsafe`; no opt-out marker), and check the type of every spawn argument expression. Mark the stdlib `String`, `vector`, `map` and their kin `[sendable]`. Drop the `mut`-binding rejection: a `mut` argument is copied, as in any call. No marker on the native UI handles (their thread check is TODO-5808). A `String` made in an open `StringArena` passed to a spawn is `string escapes its arena via spawn`.
  - implementation_notes: src/semantics/SemanticsValidatorTaskHandles.cpp (the validator passes run after template monomorphization, so argument types are concrete); the arena escape check lives with the existing `string escapes its arena via ...` diagnostics. Diagnostics name the path to the leaf: `spawn argument `buffer` of /fill is not sendable: field `data` is Pointer<u8>`, with the chain of fields through nested structs, and `(instantiated from /pump at main.prime:12)` when the type came through a template argument; each carries the note `sharing between tasks needs Mutex, Atomic or Channel`. Make the predicate a reusable unit (`.h/.cpp`) since TODO-5791, TODO-5797 and TODO-5798 call it.
  - acceptance:
    - positive: semantic tests spawn calls taking a `String`, a `vector<i32>`, a struct of both, an enum, a `Function<i32, i32>` and a `mut` binding (the spawner's copy is unchanged after `wait`), and all examples/ and stdlib programs still compile
    - negative: semantic tests reject a `Pointer<u8>` argument, a `StringView`, a closure with a `[ref]` capture, a struct with a `Reference` field, a struct nested two levels deep holding a `Pointer` (the message names both fields), a generic callee instantiated with a `Slice<i32>` (the message carries the instantiation chain), `[sendable]` on a user struct without `unsafe`, and an arena-made `String`, each with the diagnostic shape of the design
    - full release gate green
  - stop_rule: the sendable predicate, the `[sendable]` transform and the spawn-site check; the `Function` conversion, `Mutex` and `Channel` sites come with their own leaves.

- [ ] TODO-5791: Every `Function` is sendable by construction: the conversion rejects non-sendable captures
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5790
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Functions Are Sendable)
  - parallel_track: mt-closures
  - scope: A `Function<R, A...>` erases its closure's type, so sendability cannot be read off a `Function` value; instead every conversion to `Function`, explicit (`Function<i64>{closure}`) and implicit (a closure at an expected-type site), rejects a closure with a capture that is not sendable (TODO-5790's predicate): a `Pointer`, a `StringView`, a slice, a struct holding one. `[ref]` captures are already rejected (`closure escapes via Function`). `Function` itself is then sendable, with no second function type and no per-instance inference. Routers, `AppRouter`, `WebApp`, `AppHandlers` and `serveWorkers` keep plain `Function`.
  - implementation_notes: the conversion sites (src/semantics/TemplateMonomorphCallableRewrites.cpp, `convertArgument`; src/semantics/SemanticsValidateLambdaClosures.cpp, `rewriteFunctionTypes`) rewrite the closure into `/__fn_box(closure)` while the closure struct's field types are still the template parameters the factory call infers, so the check runs in the `function-values` pass (src/semantics/SemanticsValidateFunctionValues.cpp, `rewriteFunctionValues`), which runs after template monomorphization and already visits every `/__fn_box` with its concrete closure struct; report at the box's source position, which is the closure's. The loan-based check in src/semantics/SemanticsValidateSliceLoans.cpp (`checkFunctionBoxes`) stays.
  - acceptance:
    - positive: a lambda with value and `mut` captures of sendable types (a `String`, a `vector<i64>`, a struct of both) and a named definition convert to `Function<i64, i64>` at a parameter, a field and a binding, and run on the VM, native and `optexe`; a `Function` passes as a spawn argument; all examples/ and stdlib programs still compile
    - negative: a closure capturing a `Pointer<u8>`, a `StringView` or a struct with a `Pointer` field is `closure cannot be a Function: capture `view` is a StringView` (with the field path for the struct), at the closure's line, at an explicit `Function<...>{...}` and at an implicit site, and under single-threaded `serve` too
    - full release gate green
  - stop_rule: the capture check at `Function` conversion; no new function type.

- [ ] TODO-5792: The VM runs spawned tasks under a deterministic scheduler
  - owner: ai
  - status: ready
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Runtime Model and VM Scheduler)
  - parallel_track: mt-vm-tasks
  - scope: `[spawn] f(...)` is lowered inline today (IrLowererLowerStatementsBindings.h) and `Task<void>` fails in lowering (`void call not allowed in expression context`). Add the IR opcodes `TaskSpawn <function>` (arguments copied into a heap task block, pushes the handle) and `TaskWait` (pushes the result, frees the block), with a PSIR version bump and a note in docs/spec/vm-design.md. The VM runs each task on its own call stack in one OS thread: switch points at spawn and `wait`, the eager default schedule (a new task runs until it ends or blocks, so every existing program prints what it prints now), `--vm-schedule-seed <n>` (and `PRIMESTRUCT_VM_SCHEDULE_SEED`) for seeded schedules, and `deadlock: every task is waiting` (exit 3). Every other backend lowers `TaskSpawn`/`TaskWait` as an inline call and a load, as today. Spawned callees become real calls.
  - implementation_notes: the spawned callee's arguments are copies (or moves) owned by the task block; its parameters borrow them. Keep the semantic-product task facts as the lowering's input. A callee whose parameters or result real calls cannot take yet (sum results, TODO-5738; `string` parameters, TODO-5741) keeps running inline with an `--opt-report` reason, which is a legal schedule.
  - acceptance:
    - positive: VM compile-run tests of two `Task<i64>` sums, a `Task<void>`, a spawn inside a loop waited per iteration, and the existing task tests give the same output on the default schedule and on seeds 1 to 8; native and `optexe` run the same programs inline with the same output
    - negative: waiting a handle twice and letting one escape are still rejected; an IR validation test rejects `TaskWait` on a value that is not a task handle; `--vm-schedule-seed` without a number is a CLI error; a module of the previous PSIR version still loads (the deadlock fault is covered by TODO-5798's channel test, since task handles cannot cross to let two tasks wait on each other)
    - full release gate green
  - stop_rule: the IR opcodes, the VM scheduler and inline lowering elsewhere; threads on `optexe` are TODO-5795, on native TODO-5799.

- [ ] TODO-5793: Byte-region accesses check liveness without a map search or a write
  - owner: ai
  - status: ready
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Heaps)
  - parallel_track: mt-heap-optexe
  - scope: `VmHeapCore::byteAt` (include/primec/runtime/VmHeapCoreBody.inc) finds a byte region with a `std::map` search unless the address is in the last region found, and writes that cache on every lookup, so even a read mutates the heap and a loop over two regions searches the map on every byte. Give byte regions a state table indexed by the address, like the slots' (`states`): the region's generation for each granule, so a check is one array load and compare and writes nothing. Keep the addresses, the fault messages and the VM/`optexe` parity.
  - acceptance:
    - a VM benchmark that copies between two byte regions byte by byte (add it under benchmarks/) runs at least 2x faster; `benchmarks/aggregate.prime` and the strings benchmarks are no slower on the VM and `optexe`
    - positive: tests/unit/runtime/test_vm_byte_regions.cpp gains a case that interleaves accesses to three regions and checks every byte; negative: an access to a freed region, to a region reused with a new generation and one byte past a region's end still fault with today's messages
    - the PrimeScroll `optexe` peak footprint (close:1 median, as in TODO-5751) grows by at most 5%; full release gate green
  - stop_rule: the byte-region liveness check; arenas and threads are TODO-5794.

- [ ] TODO-5794: `optexe` heaps are per-thread arenas with remote frees
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5793
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Heaps)
  - parallel_track: mt-heap-optexe
  - scope: Make the generated program's heap usable from several threads: one `VmHeapCore` arena per thread with the arena number in address bits 41 to 46 (arena 0 keeps today's addresses), storage that does not move once a second arena exists, liveness reads with relaxed atomic loads of the state tables and a per-thread region cache, a remote-free queue per arena (behind the arena's lock, drained by the owner at its next allocation), and at most 64 arenas (a 65th thread shares the last behind its lock). Each thread gets its own `VmTextHeap` (its `StringArena` stack and literal cache), whose blocks freed elsewhere go back the same way.
  - implementation_notes: include/primec/runtime/VmHeapCore*.{h,inc}, VmTextHeap*.{h,inc}, src/backend/IrToOptCppRuntime.h (`Rt` splits into per-thread state and the shared arena table). The VM keeps one arena and no locks.
  - acceptance:
    - positive: a runtime unit test runs 4 `std::thread`s that each allocate, read and free 100000 slot and byte blocks and free a quarter of another thread's blocks, and checks live counts and contents at the end; a single-threaded `optexe` program gives the same addresses as the VM (the existing parity tests)
    - negative: a block freed twice from two threads faults with `invalid heap free address` when the owner drains; an address of a freed block read from another thread faults
    - single-threaded `optexe` benchmarks within 3% of before; the unit test passes under ThreadSanitizer in one manual run, recorded in docs/todo_log.md; full release gate green
  - stop_rule: the `optexe` heap and string heap; starting threads is TODO-5795.

- [ ] TODO-5795: `optexe` runs spawned tasks on threads
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5794, TODO-5792
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Runtime Model)
  - parallel_track: mt-optexe-threads
  - scope: Lower `TaskSpawn` in IrToOptCppEmitter to a `std::thread` per task over a per-thread `Rt` (call depth, local-address frames, heap arena) and `TaskWait` to a join that copies the result out. `ps_fault` flushes and leaves with `std::_Exit(3)`; a print writes its line and newline in one call.
  - acceptance:
    - positive: two `sum_range` tasks of about one second each finish in at most 0.65 times the time of the same work on one task (wall clock, back to back, load recorded); every VM task test gives the same output on `optexe`; a task that allocates and returns a `String` hands it to the waiter, which frees it
    - negative: a fault in a task exits 3 with the VM's message while another task is still running; 4 tasks printing 1000 lines each never produce a mixed line
    - full release gate green
  - stop_rule: threads for `[spawn]` on `optexe`; the sharing primitives are TODO-5796 to TODO-5798.

- [ ] TODO-5796: `Atomic<i32>` and `Atomic<i64>` with the `atomic` effect
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5795, TODO-5790
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Atomic)
  - parallel_track: mt-primitives
  - scope: Add `/std/sync` with `Atomic<i32>` and `Atomic<i64>` (`load`, `store`, `add` and `exchange` returning the previous value, `compareExchange`), sequentially consistent, copies sharing one heap cell with an atomic handle count, `[sendable]`, and the effect `atomic`. IR opcodes `AtomicLoad`, `AtomicStore`, `AtomicAdd`, `AtomicExchange`, `AtomicCompareExchange` (32/64-bit, heap address) on the VM (a switch point under a seed) and `optexe` (`std::atomic_ref`); arm64 native comes with TODO-5801, other backends reject them.
  - acceptance:
    - positive: 4 tasks adding 1 to a shared `Atomic<i64>` 100000 times each end at 400000 on the VM (default schedule and seeds 1 to 8) and on `optexe`; a compare-exchange spin counter gives the same total; dropping every copy frees the cell (heap statistics)
    - negative: an `Atomic` method in a definition without `atomic` is `atomic operation requires atomic effect`; `Atomic<f32>` and `Atomic<String>` are diagnostics; native and Wasm reject a module using atomics with a message naming the backend
    - full release gate green
  - stop_rule: the two atomic types on the VM and `optexe`.

- [ ] TODO-5797: `Mutex<T>` with scoped `locked(body)` and the `lock` effect
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5796
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Mutex)
  - parallel_track: mt-primitives
  - scope: `Mutex<T>` in `/std/sync`: owns a sendable `T` in a heap cell, `locked<Body>(body)` locks, calls `body` with a `Reference<T>`, unlocks on every way out and returns what `body` returns; copies share it (atomic handle count). The lock is a runtime host function: on the VM a scheduler-aware lock (switch points at lock and unlock, blocked tasks count for deadlock detection), on `optexe` `std::mutex` with the holder recorded. Locking it again inside its own body faults with `mutex locked again by the task that holds it`.
  - acceptance:
    - positive: the design's `Tally` example prints 7 on the VM (all seeds) and `optexe`; 8 tasks each appending 1000 entries to a `vector<i64>` inside one mutex leave 8000 entries; `locked` returning an `i64` and a body with a `[ref]` capture both work
    - negative: a non-sendable `T` is `Mutex element StringView is not sendable` and, for a struct, `Mutex element Record is not sendable: field `next` is Reference<Record>`; returning or storing the body's `Reference<T>` is the existing borrowed-parameter diagnostic; `locked` without the `lock` effect is a diagnostic; a nested `locked` on the same mutex faults with exit 3 on both backends; two tasks locking two mutexes in opposite order report `deadlock: every task is waiting` on a VM seed that interleaves them
    - full release gate green
  - stop_rule: `Mutex` on the VM and `optexe`; `RwLock` is TODO-5804.

- [ ] TODO-5798: Bounded `Channel<T>` with the `channel` effect
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5797
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Channel)
  - parallel_track: mt-primitives
  - scope: `Channel<T>{[capacity] n}` in `/std/sync`: a bounded FIFO of a sendable `T` in a heap cell, `send` (moves in, blocks while full), `receive` (a `Maybe<T>`, blocks while empty and open, `none` once closed and drained), `close` (a later send faults with `send on a closed channel`), copies sharing it. Built from the lock of TODO-5797 and two condition waits (VM scheduler, `std::condition_variable` on `optexe`). Backends that run tasks inline reject channels.
  - acceptance:
    - positive: the design's producer and `sum_jobs` example prints 4950 for 100 jobs through a channel of capacity 4 on the VM (all seeds) and `optexe`; 3 producers and 3 consumers move 30000 `String`s and every one arrives exactly once; a `String` sent and freed by the receiver leaves the heap statistics balanced
    - negative: send after close faults with exit 3; a task receiving from a channel nobody sends on reports the VM deadlock fault; `Channel<Pointer<u8>>` and sending an arena-made `String` are diagnostics; native, Wasm and the JIT reject a module with channels
    - full release gate green
  - stop_rule: `Channel` on the VM and `optexe`.

- [ ] TODO-5656: Thread-safe `SharedString`: an atomic owner count
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5796
  - created_at: 2026-11-03
  - phase: Multithreading (docs/MultithreadingPrototype.md, Heaps; docs/spec/strings-and-views.md, Beyond std::string)
  - parallel_track: string-concurrency
  - scope: `SharedString` (stdlib/std/text/shared.prime, TODO-5638) keeps its owner count as a plain `i32` right after the text's 0 byte, at an unaligned offset, so it may be shared on one thread only. Move the count to an aligned place in the block, count with the atomic operations of TODO-5796 (`add` on copy, `add(-1)` on destroy, the last owner frees), and mark the type `[sendable]`. Freeing on a thread other than the allocating one is the heaps' remote free (TODO-5794 on `optexe`, TODO-5800 on native). Update the strings-and-views status table.
  - acceptance:
    - positive: 4 tasks each copy and destroy one `SharedString` 100000 times and `owners()` is 1 at the end on the VM (all seeds) and `optexe`; a `SharedString` passes as a spawn argument and through a `Channel`; the single-threaded `SharedString` tests are unchanged
    - negative: a `SharedString` made inside a `StringArena` still cannot be sent (`string escapes its arena via spawn`)
    - full release gate green
  - stop_rule: the owner count and sendability; native follows with TODO-5800.

- [ ] TODO-5802: `serveWorkers`: one thread polls and accepts, N workers answer
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5798, TODO-5791
  - created_at: 2026-11-09
  - phase: Multithreading and Web apps (docs/MultithreadingPrototype.md, The Server; docs/WebAppsPlan.md, Concurrency)
  - parallel_track: mt-server
  - scope: In `/std/http/server` add `serveWorkers(handler, config)` and `serveWorkersUntil(handler, config, stop)` (effects `net_io`, `heap_alloc`, `io_err`, `task`) and `ServerConfig.workers` (0 = one per core); `/std/os` gets `cpuCount()` and a way for a worker thread to block `SIGINT`/`SIGTERM`; `/std/net` gets a wake pipe. The poller watches the listener, the open connections and the wake pipe, and hands a readable connection's slot to the workers over a `Channel`; a worker receives, parses, calls the handler inside a `Mutex` that holds the whole handler, serializes, sends and closes, and returns the slot (or returns an incomplete request to be watched again). `Router`, `AppRouter<State>` and `WebApp<State>` keep their plain `Function` handlers, sendable by construction (TODO-5791), so an app is sendable when its `State` is; the `Mutex<Handler>` element check (TODO-5797) is what rejects an app that is not. `serve` and `serveUntil` stay as they are.
  - acceptance:
    - positive: an `optexe` test with 4 workers answers 16 concurrent clients sending 50 requests each, and a counter app's count equals the number of POSTs; a client that sends half a request and stalls does not delay the others with 1 worker; a VM test serves 2 workers deterministically on the default schedule; examples/apps/todo_web and hello_button compile with `serveWorkers` without changing their handlers
    - negative: a `WebApp<State>` whose `State` has a `Pointer<u8>` field passed to `serveWorkers` is `Mutex element WebApp<State> is not sendable: field `state` of WebApp<State>, field `bytes` is Pointer<u8>` (a handler capturing a `Pointer` never gets this far: TODO-5791 rejects it where it is written); SIGTERM stops the server within `acceptTimeoutMs` while all workers are busy
    - full release gate green
  - stop_rule: the library server; switching the examples and measuring is TODO-5803.

- [ ] TODO-5803: The web apps run on workers and their gain is measured
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5802
  - created_at: 2026-11-09
  - phase: Multithreading and Web apps (docs/WebAppsPlan.md, Concurrency)
  - parallel_track: mt-server
  - scope: Add a load generator (`scripts/http_load.py`, Python standard library only: N concurrent connections for a fixed time, requests per second and p50/p99) and server benchmarks under benchmarks/http/; switch examples/apps/todo_web and hello_button to `serveWorkers` (their comments say the server is single-threaded today) and record the numbers in docs/WebAppsPlan.md.
  - acceptance:
    - with 16 concurrent clients on a machine with at least 4 cores, each measured against `serve` back to back with the load average recorded: todo_web's list page with 200 items at least 1.5x the requests per second with 4 workers; a POST of a 32 KB form whose handler does little at least 2.5x; a handler doing about 1 ms of CPU work under the lock recorded as the baseline for TODO-5804 (about 1x expected); 1 worker at least 0.9x `serve`
    - negative: the load generator exits non-zero when any response is not 2xx or 3xx, and a run against a server that refuses connections fails with a message
    - full release gate green
  - stop_rule: the examples, the load generator and the numbers.

- [ ] TODO-5799: arm64 native runs spawned tasks on pthreads, its heap behind a lock
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5792
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
  - blocked_on: TODO-5800, TODO-5798
  - created_at: 2026-11-09
  - phase: Multithreading (docs/MultithreadingPrototype.md, Atomic, Mutex, Channel)
  - parallel_track: mt-native
  - scope: Emit the atomic opcodes with LSE (`ldar`/`stlr`, `ldaddal`, `swpal`, `casal`) and answer the lock and condition host functions with `pthread_mutex_*` and `pthread_cond_*` bound from libSystem, so the `/std/sync` types and `SharedString` run on arm64 native.
  - acceptance:
    - positive: the `Atomic`, `Mutex`, `Channel` and `SharedString` compile-run tests of TODO-5796, TODO-5797, TODO-5798 and TODO-5656 pass on native with the `optexe` output
    - negative: a nested `locked` on one mutex and a send after close exit 3 with the VM's messages
    - full release gate green on macOS arm64
  - stop_rule: the primitives on arm64 native; x86_64 native stays inline.

- [ ] TODO-5804: GET handlers that only read the state run at the same time under `RwLock`
  - owner: ai
  - status: deferred
  - created_at: 2026-11-09
  - phase: Multithreading and Web apps (docs/MultithreadingPrototype.md, The Server)
  - parallel_track: mt-server
  - depends_on: TODO-5803, TODO-5735
  - scope: Under TODO-5802 every handler holds the app's mutex, so page rendering runs one request at a time. Add `RwLock<T>` (`read(body)` with a read-only `T`, `write(body)` with a `Reference<T>`), let a handler say it only reads (its state parameter `[State] state` against `[Reference<State>] state`, which needs same-arity overloads by argument type, TODO-5735; or a named registration, open question 5 of the design), and run read-only handlers under the read lock. Deferred: decide the registration form first, and measure TODO-5803's baseline.
  - acceptance:
    - the TODO-5803 CPU-bound GET handler at least 2.5x `serve` with 4 workers; a read-only handler that writes its state is a compile error
  - stop_rule: read-only handlers and `RwLock`.

- [ ] TODO-5805: The server keeps connections alive between requests
  - owner: ai
  - status: deferred
  - created_at: 2026-11-09
  - phase: Web apps (docs/WebAppsPlan.md, Concurrency)
  - parallel_track: web-keepalive
  - depends_on: TODO-5802
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
  - depends_on: TODO-5802
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

- [ ] TODO-5544: Method calls on `pick` payload bindings lower
  - owner: ai
  - status: deferred
  - created_at: 2026-10-08
  - phase: Native UI (docs/NativeUiPlan.md)
  - parallel_track: pick-bindings
  - scope: `pick(app.waitEvent()) { windowCloseRequested(w) { w.close() } }` with `[public struct] Window` payloads passes semantics but VM lowering fails with `missing semantic-product method-call target: /main -> close`; copying the payload first (`[Window] target{w}`, then `target.close()`) works. Publish the method-call target for pick payload bindings (and make `Type.staticHelper()` resolve through `import`, which `App.start(...)` does not).
  - acceptance:
    - a pick payload struct binding accepts method calls in VM, native and C++; a negative test keeps the unknown-method diagnostic
    - docs/spec/stdlib-reference.md "Native UI" drops the copy-first note; full release gate green
  - stop_rule: method-call target publication for pick bindings and imported static helpers only.

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
