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
18. Claim a leaf before working on it, because several workers share this
    queue. Fetch origin/master, pick a `ready` leaf with no `claimed_by`
    line, change its `- [ ]` to `- [~]`, add
    `claimed_by: <worker-name> (<branch or worktree>), <YYYY-MM-DD>` under
    `status`, and commit and push that change alone ("Claim TODO-NNNN")
    before writing code. If the push is rejected, fetch and rebase; if the
    leaf now names another worker, drop your claim and pick another leaf.
    Never start a leaf claimed by another worker. To abandon a leaf, remove
    the line, set `- [ ]` again and push. Finishing moves the block out as
    in rule 15.

## Task Template

```md
- [ ] TODO-<id>: Short title
  - owner: ai|human
  - status: ready|blocked|deferred
  - blocked_on: TODO-XXXX (required when status: blocked; omit otherwise)
  - claimed_by: worker-name (branch or worktree), YYYY-MM-DD (only while `[~]`; see rule 18)
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
| TODO-5752 | x86_64 native and the Linux JIT take the address ABI of real calls | deferred | native-real-calls |
| TODO-5688 | Calls require the callee's whole active effect set | deferred | effects-transitive |
| TODO-5804 | GET handlers that only read the state run at the same time under `RwLock` | ready | mt-server |
| TODO-5806 | Sessions: a cookie and per-session state | deferred | web-sessions |
| TODO-5821 | The first toolbar of a launch does not hold up the first frame | deferred | startup-toolbar |
| TODO-5603 | The real-launch baseline, measured by a person | ready | startup |
| TODO-6117 | Native text used after its arena ended is read and written silently | ready | native-arena-stale-text |
| TODO-6118 | Native arena regions larger than the standard one are never unmapped | deferred | native-arena-large-regions |
| TODO-6119 | A native module with tasks keeps arena text in the pool | deferred | native-arena-threads |
| TODO-5536 | Small, lazily loaded bytecode for apps | deferred | bytecode-startup |
| TODO-5533 | Windows and Linux native UI backends | deferred | native-ui-platforms |
| TODO-5511 | Safe code cannot reach container storage or unsafe stdlib helpers | deferred | safety-stdlib |
| TODO-6162 | `Vector`, `Map` and `SoaColumn` expose their storage fields and a brace constructor that takes any storage | ready | safety-stdlib |
| TODO-6163 | Non-public stdlib helpers are callable by absolute path | deferred | safety-stdlib |
| TODO-6165 | A container is changed through a `Reference` parameter that is not `mut` | deferred | safety-borrows |
| TODO-6166 | A plain `Reference<vector<T>>` argument can change the container a live view reads | blocked | safety-borrows |
| TODO-6059 | Native x86_64 float to integer conversion saturates | deferred | numeric-conversions |
| TODO-6051 | Native x86_64 (ELF) division by zero is the VM's runtime fault | deferred | runtime-faults |
| TODO-6142 | The recursion depth of Linux x86_64 native and `exe` is measured | deferred | runtime-faults |
| TODO-6143 | Deep recursion on Wasm is a host stack trap, not the VM's `VM call stack overflow` | deferred | runtime-faults |
| TODO-6141 | Wasm lowers the heap opcodes and string byte reads (`vector`, `String`) | ready | wasm-opcode-gaps |
| TODO-6149 | Wasm slot heap: `HeapAlloc`, `HeapFree`, `HeapRealloc` and heap addresses | deferred | wasm-opcode-gaps |
| TODO-6150 | `heapLiveBlocks()` and the string heap statistics on Wasm | blocked | wasm-opcode-gaps |
| TODO-6153 | Twelve gate tests hit the 295 s hard cap together and pass alone in under 11 s | deferred | flaky-tests |
| TODO-6124 | Wasm lowers pointers, `Reference`, struct values, arrays and closures | blocked | wasm-opcode-gaps |
| TODO-6127 | Wasm refuses u8 and byte memory opcodes and host calls | deferred | wasm-bytes-host |
| TODO-6128 | Wasm refuses tasks, atomics, channels and the lock effect | deferred | wasm-concurrency |
| TODO-6133 | The binding initializer accepts an f64 value for an `f32` binding, assignment does not | ready | binding-float-compat |
| TODO-6121 | The VM debugger reports the column of the text-filtered source for a rewritten expression | ready | debugger-filtered-columns |
| TODO-6081 | An unknown `break` is reported at the user's `break`, not inside stdlib/std/file/file.prime | deferred | diagnostics-break-span |
| TODO-6082 | `borrowed binding: v (root: v, sink: v)` names the sink | deferred | diagnostics-borrow-sink |
| TODO-6084 | Internal lowering errors (`missing semantic-product ...`) say so | deferred | diagnostics-internal-errors |
| TODO-6085 | The semantics validator uses the shared demangle helper | blocked | diagnostics-demangle-semantics |
| TODO-6067 | x86_64 native emits FMA3 `vfmadd231sd` and `vfmadd231ss` | deferred | stdlib-math-fma |
| TODO-5545 | Record the macOS AppKit manual smoke checklist | deferred | native-ui-macos-manual |
| TODO-6019 | PrimeScroll built by `make.prime` starts as fast as the script's, measured by a person | ready | build-dogfood |
| TODO-5932 | Libraries carry back-end code | deferred | lib-backend |
| TODO-5933 | The embed API loads a library at run time | deferred | lib-embed |
| TODO-6032 | A public function of any shape is a function of a library | deferred | lib-shapes |
| TODO-5987 | The lowerer's include-only fragments become components | ready | lowerer-fragments |
| TODO-5973 | SoA rewrite passes share one context and one walker | ready | reuse-soa-rewrites |
| TODO-5976 | The semantic product publishes parsed types, and the lowerer stops re-parsing type text | deferred | reuse-type-model |
| TODO-5977 | Semantics diagnostics go through one builder | deferred | reuse-diagnostic-builder |
| TODO-5979 | `--collect-diagnostics` reports no false intra-body call diagnostics | ready | collect-diagnostics-false-positives |
| TODO-6113 | Calling a `Function` field of a read-only value says `unknown method` | deferred | function-call-diagnostic |
| TODO-6116 | Calling a `Function` through a read-only place is accepted although `call` is `mut` | deferred | function-call-mutability |
| TODO-6103 | Slots of `map<K, V>` and `soa<T>` storage | deferred | lifecycle-collection-slots |

### Ready Now

- TODO-5804 (mt-server): read-only handlers registered with `app.view(...)` run under the read lock
- TODO-5603 (startup): the real-launch baseline, measured by a person

### Immediate Next 10

1. TODO-5804
2. TODO-5603

### Priority Lanes

- Startup, first (docs/NativeUiPlan.md section 1, "Where a launch goes"): TODO-5603, TODO-5821
- Native PrimeScroll, direct native backend (docs/NativeUiPlan.md): TODO-6117; deferred TODO-6118, TODO-6119
- Native UI (docs/NativeUiPlan.md): TODO-5536, TODO-5533
- Effects (docs/spec/language-core.md, Effects): TODO-5688
- Build system (docs/BuildSystemPlan.md; after the startup lane): TODO-6019
- Modules and libraries (docs/ModulesAndLibrariesPlan.md; after the startup lane): none ready; deferred TODO-5932, TODO-5933, TODO-6032
- Memory safety (docs/spec/type-system.md Memory safety): deferred TODO-5511 (tracker), TODO-6162 (container fields private), TODO-6163 (private definitions, decision needed), TODO-6165 (writes through a non-`mut` Reference), TODO-6166 (view live across a plain `Reference` argument)
- Backend parity: TODO-6124 (Wasm pointers, structs, arrays; tracker of TODO-6138, TODO-6139, TODO-6140 and TODO-6141); deferred TODO-6059, TODO-6051, TODO-6142 (Linux recursion depths), TODO-6143 (Wasm recursion trap), TODO-6127 (Wasm u8, host calls), TODO-6128 (Wasm concurrency)
- Frontend and diagnostics (split from TODO-5520): TODO-6121; deferred TODO-6081, TODO-6082, TODO-6084, TODO-6085
- Fused `fma` (IR opcodes `FmaF32` and `FmaF64`, done on the VM, the C++ emitters, arm64 native and GLSL): deferred TODO-6067 (x86_64 native FMA3, needs an x86_64 machine) (Wasm exact emulation)
- macOS support: TODO-5545 (needs macOS)
- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix; VM speed ; passes ; real calls TODO-5752
- Multithreading (docs/MultithreadingPrototype.md, Threads For Real; docs/WebAppsPlan.md, Concurrency; after the startup lane): TODO-5872, TODO-5804; deferred TODO-5806
- Code reuse (docs/CodeReusePlan.md; two at a time under rule 18): TODO-5973; deferred TODO-5976, TODO-5977
- Lifecycle bugs found with TODO-6098 (outside Ready Now): deferred TODO-6103 (`map` and `soa` slots)

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10: the leaves that fix leaks and lifecycle (TODO-5853) first. Ready outside Ready Now (the cap is eight): TODO-6121 (debugger columns of rewritten expressions), TODO-6019 (a person measures startup); the Wasm leaf found by the Wasm matrix config that is still ready: TODO-6141 (the heap; TODO-6124 tracks it). Decided 2026-11-20 by the owner; each block carries its `decision:` line. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

Multithreading (docs/MultithreadingPrototype.md, Threads For Real), after the startup lane (TODO-5772 is done); not above them in Ready Now or Immediate Next 10. In this order (TODO-5792 and TODO-5835, VM tasks and their lowering, are done, and so are TODO-5793, byte-region checks without a map search, TODO-5794, `optexe` per-thread heap arenas, TODO-5795, `optexe` threads for `[spawn]`, `Atomic<i32>` and `Atomic<i64>` in `/std/sync`, TODO-5797, `Mutex<T>` with `locked(body)`, TODO-5846, `optexe` faults when every task waits, TODO-5798, bounded `Channel<T>`, TODO-5656, the atomic `SharedString` owner count, TODO-5802, `serveWorkers`, and TODO-5803, the apps on workers, measured, and the `SharedString` count inside the text block, are done): arm64 native is done: TODO-5799 (pthreads; TODO-5942, TODO-5943), TODO-5800 (per-thread heaps) and TODO-5801 (atomics, `Mutex`, `Channel`; TODO-5950, TODO-5951). TODO-5799 is done, and TODO-5893 (an unused `/std/tuple/*` import is reported as an unknown import path) is done too, and TODO-5894 (tuple destructuring and the destructured multi-wait inside a nested block are expanded; TODO-5849, each destructured multi-wait binding has its own result type, is done) is done too, and TODO-5847 (spawned calls with builtin `map<K, V>` or owned sum parameters or results run as tasks, done with TODO-5994, `wait(t).count()` of a stdlib `Vector` result waits twice, is done; TODO-5992, builtin `vector<T>` parameters and results, TODO-5993, the stdlib `Vector` and `Map` structs, TODO-5845, the same for `String`, struct and closure values, and TODO-5895, `copy` and `move` scalars and structs, are done) (TODO-5807, destructured multi-wait, TODO-5790, sendable spawn arguments, and TODO-5791, every `Function` sendable by construction, and TODO-5848, `return(wait(t))` of a `String` task result, are done). Ready: TODO-5804 (read-only handlers with `app.view` under `RwLock`); deferred: TODO-5806 (sessions); TODO-5805 (keep-alive) is done; TODO-5854 (a `Mutex` or `Atomic` handle keeps a copy of its first value; the unstored `ctor_only` field, on the construction-only field rule of TODO-5853) is done. Found with `Mutex` (TODO-5852, `wait(a, b)` of two `Task<void>`, is done) in the closures lane: TODO-5934 (a `Reference`-returning call read as a value yields its referent, found with TODO-5850), TODO-5850 (a `Reference`-returning call as a closure argument) and TODO-5851 (a `[ref]` closure written in a generic call) are done. Found with `Channel` and ready, in the Result and control flow lane: TODO-5855 (a method call on a `pick` payload of struct type), TODO-5856 (`pick` on a call result frees an owning payload), TODO-5857 (a `vector<T>` payload reads its count in a `pick` arm), TODO-5858 (`some<Vector<i32>>` of a fresh vector frees it), TODO-5859 (a `pick` expression yields a vector), TODO-5860 (moving a payload binding does not poison a later binding of the same name), TODO-5866 (a closure captures a `pick` payload binding), TODO-5877 (a field an arm of a `pick` expression yields is copied), TODO-5879 (a `pick` expression's result owns a value of its own), TODO-5880 (a payload moved out of a sum local leaves the local moved-from, and a payload of a borrowed sum cannot be moved or returned), TODO-5873 (a sum temporary passed as an argument is destroyed after the call), TODO-5874 (assigning to a `mut` sum local destroys the payload it held), TODO-5883 (assigning to a `mut` builtin vector local destroys what it held and copies a place), TODO-5876 (`return(pick(...))` of a vector and assignment from a `pick` expression), TODO-5875 (`count()` on a local declared `Vector<T>` lowers), TODO-5878 (a `[move name]` lambda capture compiles), TODO-5884 (assignment to a `mut` vector parameter and to an inferred vector-of-struct local keeps one owner), TODO-5882 (a vector of sums destroys the payloads of its elements), TODO-5885 (a vector temporary that is only read is destroyed), TODO-5889 (`a = move(b)` of a vector local takes the vector over), TODO-5890 (the arms of an assigned `pick` may open on the next line), TODO-5881 (a call takes a call to the same definition as its argument on the VM), TODO-5888 (an element of a vector temporary read as a struct or a `String`) and TODO-5887 (`remove_at` and `remove_swap` compile on a vector of sums) are done, and found with them: TODO-5886 (`push` on a local declared `Vector<T>` resolves) is done, and TODO-5978 (a `Vector<String>` leaks what `pop` and `clear` remove and faults after `remove_at`; its element reads by `at` and `[]`) is done, and found with it: TODO-5991 (a field read of a struct a call returns leaves the temporary's copy undestroyed) is done; TODO-5891 (a returned vector field or vector element of a local is a copy) and TODO-5892 (a block expression whose value is an owning local hands it over once) are done. Found with `serveWorkers` (TODO-5865, a `Function` local given positionally to a brace constructor, TODO-5864, a lambda in a generic definition naming its type parameters, and TODO-5935, a lambda parameter naming a struct of its namespace, are done): TODO-5862 (`serveWorkers` on the VM over a scripted net, done), and ready: TODO-5866 (a closure captures a `pick` payload binding). Found measuring the apps on workers (TODO-5803, done): TODO-5867 (a string heap that alternates between block sizes keeps its chunks) and TODO-5869 (1 worker as fast as `serve`; the 32 KB form reached 2.41x with 4 workers, not 2.5x) are done; TODO-5940 (a load generator that costs less per connection than the servers it measures, done: compiled, 1.4 us of user CPU per request, and the kernel's loopback setup is the ceiling; TODO-5871 found every server held to about 14,600 new connections a second by the generator and the kernel's loopback setup, which caps the form with 4 workers at 2.41x); TODO-5871 (that measurement), TODO-5872 (a byte of another thread's block is checked on a slow path, so the worker parses from a copy) and TODO-5870 (a VM program that uses tasks runs its string code on the fast kernel) are done; and in the Result and control flow lane TODO-5868 (a definition's `on_error` handler flows into nested blocks and a block-level `on_error` overrides it, so `serve(app)?` compiles in an `if` block of `main`) and TODO-5936 (a diagnostic in `main` of a program that imports `/std/http/server` reported a stdlib position, found with it) are done.

VM threads (docs/MultithreadingPrototype.md, VM Threads), after the startup lane: the VM gets a threads mode next to its deterministic scheduler. TODO-5896 (`--vm-threads`, a thread and a heap arena per task) and TODO-5897 (Atomic, Mutex and Channel on real primitives, with the deadlock fault) and TODO-5898 (reentrant and main-thread-only host functions) and TODO-5899 (`RunOptions::threads`, fault and cancel through the embed API) and TODO-5900 (the concurrent corpus repeated under `--vm-threads` and under ThreadSanitizer) and TODO-5901 (`--vm-record-sync` and `--vm-replay-sync`, the synchronization order of a threaded run replayed on the deterministic scheduler) landed; TODO-5904 (threads by default) is closed: the default stays the deterministic scheduler; TODO-6075 (a host call yields on it) is closed: it does not, a poller with workers needs `--vm-threads` with `Reentrant` host functions (docs/spec/vm-design.md, Tasks); TODO-5902 (each task on the fast kernel) landed. TODO-5903 (debug sessions run programs with tasks, the first prerequisite of docs/VmInspectorPlan.md) landed.

Build system (docs/BuildSystemPlan.md), after the startup lane: TODO-5906 (directory listing and file metadata, `/std/dir`) and TODO-5915 (incremental builds) landed. TODO-5907 (glob, `/std/glob`) and TODO-6009 (a changed glob listing re-runs a step) and TODO-5918 (staged builds, `Graph.run` and the `build_run` effect) landed; TODO-5916 (subprojects, `b.subproject`) and TODO-5917 (options, `b.option`) landed; TODO-5920 (PrimeScroll builds with `make.prime`) landed; TODO-5919 (function steps, `b.step`) landed; ready: TODO-6019 (person measures startup); and the modules and libraries lane, which splits static libraries (docs/ModulesAndLibrariesPlan.md): TODO-5922 (`primevm --link` merges two IR modules) landed; TODO-6031 (lowering marks string literal pushes, so a library's strings always relocate) landed; TODO-5923 (`--emit=interface`) landed; TODO-5924 (`--emit=lib`) landed; TODO-5925 (`--library` and the link in the pipeline) landed; TODO-5926 (generics and lifecycle helpers cross the library boundary) landed; TODO-5927 (namespace ownership) landed; TODO-5928 (`--emit-uses`) landed; TODO-5929 (symbol-level rebuilds) landed; TODO-5930 (the graph builds libraries and links them) landed; archived as low-value TODO-5931 (the stdlib as a library: measured slower, see docs/todo_finished.md), TODO-6097 (roots below `/std`, so several stdlib libraries link and an owned module is not spliced from the stdlib source; measured 17.8 percent on three modules, numbers in docs/todo_log.md) landed, TODO-6093 (the compiler build id is the linker UUID, with the full hash as the fallback; 5M instructions of a linked library over an empty `main`, numbers in docs/todo_log.md) landed, TODO-6123 (the build command and the script cache key read the compiler build id instead of hashing `primec`: 145.5M to 60.7M instructions of an up-to-date `primec build`, numbers in docs/todo_log.md) landed (the stdlib as a library was measured again after TODO-6092 and stays archived); deferred TODO-5932 (libraries that carry back-end code), TODO-5933 (the embed API loads a library), TODO-6032 (public functions of any shape in a library).


Code reuse (docs/CodeReusePlan.md, section 17 for how to work it): these leaves are `ready` outside `Ready Now` because they are worked by their own pair of agents under rule 18. Start with TODO-5959 (delete the AST C++ and GLSL emitters) and TODO-5960 (one implementation of the execution and definition intra-body diagnostics), which touch different files. Then, as each lands: TODO-5961 (one type-text parser, after TODO-5959), TODO-5962 (one builtin-name classifier, after TODO-5961), TODO-5964 (semantics key-value predicates, after TODO-5960), TODO-5966 (binding validation as rules, not phases) then TODO-5967 and TODO-5968 (the same for template rewrites and call validation). Independent and ready at any time: TODO-5963 (lowerer collection spellings), TODO-5965 (reflection AST builders), TODO-5969 (table-driven float math lowering), TODO-5970 and TODO-5971 (semantic-product fact plumbing), TODO-5972 (lowerer helpers copied per unit), TODO-5973 (SoA rewrite walker), TODO-5974 (shared string, hash, JSON and C-literal helpers), TODO-5975 (opcode categories in the opcode table). Each block's `notes` names the leaves it must not run alongside. Deferred: TODO-5976 (parsed types in the semantic product) and TODO-5977 (one diagnostics builder).

### Task Blocks

- [ ] TODO-6019: PrimeScroll built by `make.prime` starts as fast as the script's, measured by a person
  - owner: ai
  - status: ready
  - created_at: 2026-11-17
  - phase: Build system (acceptance of TODO-5920 that needs a window; see docs/todo_log.md)
  - parallel_track: build-dogfood
  - scope: A person builds both executables (`./scripts/build_native_app.sh examples/apps/text_editor/main.prime /path/script_primescroll` and `primec build examples/apps/text_editor`, which writes `examples/apps/text_editor/out/primescroll`) from one commit and runs `scripts/measure_startup.sh` on each, which shows a window; the agent that wrote make.prime may not open windows. The generated C++ of the two is byte-identical and the dynamic library lists equal (docs/todo_log.md), so the expected result is "within noise".
  - acceptance:
    - the medians of both executables are recorded in docs/todo_log.md and are within noise
    - the executable from `primec build` launches and opens a document
  - stop_rule: measuring and recording; no code change unless the medians differ, then file the cause.

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

- [ ] TODO-6131: The span of an unknown return type is the definition, not the type
  - owner: ai
  - status: ready
  - created_at: 2026-11-20
  - phase: Frontend and diagnostics (found with TODO-6111)
  - parallel_track: unknown-type-diagnostic
  - scope: `unknown return type: NoSuchType on /make` is reported at the definition name (line 2, column 1 in `[return<NoSuchType>]` followed by `make() {`) because a `Transform` carries no source position of its own; TODO-6111 asked for the span of the type. The diagnostic should point at `NoSuchType` inside `[return<...>]`, as the unknown binding type of a parameter points at the parameter.
  - implementation_notes: record the line and column of each transform and template argument in the parser (include/primec/Ast.h `Transform`), then use them in the return-kind diagnostic through the existing source-location plumbing. Mind that the AST header is included everywhere (full rebuild).
  - acceptance:
    - the primary span of an unknown return type is the type text, pinned with file, line and column in a stable-payload diagnostics test
    - full release gate green
  - stop_rule: transform positions for this one diagnostic; no change to other messages.

- [ ] TODO-6133: The binding initializer accepts an f64 value for an `f32` binding, assignment does not
  - owner: ai
  - status: ready
  - created_at: 2026-11-20
  - phase: Correctness audit 2026-11 (found with TODO-6129)
  - parallel_track: binding-float-compat
  - scope: `[f32] z{2.5f64}` compiles (`isFloatBindingCompatible` in SemanticsValidatorBindingInitializerRules.cpp lets `f32` and `f64` initialize each other), while `[f32 mut] w{1.0f32}` followed by `w = 2.5f64` is `assign value type mismatch: expected f32, got f64` (TODO-6129, docs/spec/type-system.md: assignment takes the exact type). The same helper accepts `i32`, `bool` and the other scalars for the software `integer`, `decimal` and `complex` bindings. The spec has no implicit float narrowing, and clarity says a binding and an assignment agree. Not decided whether `f32` from `f64` stays an allowed initializer (C++ narrows in `float z = 2.5;` but not in `float z{2.5}`): the owner decides.
  - implementation_notes: options: (a) reject `f64` into `f32` in the binding like the assignment, and fix the stdlib and tests that rely on it; (b) allow the widening `f32` into `f64` only, in both places; (c) keep both float widths interchangeable in both places and document it as the one implicit conversion (then the assignment check changes). Count the programs that rely on it before choosing; do not pick without that count.
  - acceptance:
    - the chosen rule stated in docs/spec/type-system.md for bindings and assignment alike, with positive and negative semantics tests for both
    - full release gate green
  - stop_rule: float widths of binding initializers and assignment; no change to `integer`/`decimal`/`complex`.

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

- [ ] TODO-5511: Safe code cannot reach container storage or unsafe stdlib helpers
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: safety-stdlib
  - scope: Tracker. Done (2026-11-21): the `Vector` and ring buffer slot helpers are `[unsafe_api]` (TODO-6156) and the SoA slot helpers (TODO-6160). Field privacy is enforced (TODO-6161). Open: making the `Vector`, `Map` and `SoaColumn` fields private with a brace constructor that takes no storage (TODO-6162), and non-public helpers callable by absolute path (TODO-6163, decision needed).
  - acceptance:
    - TODO-6161, TODO-6162 and TODO-6163 are done; each repro (borrowSlot then reserve, takeSlot then clear, `w.data = v.data`, writing fieldCount, brace-constructing a Vector around `v.data`) is rejected outside `[unsafe]`; stdlib and corpus still compile
    - full release gate green
  - stop_rule: tracker; the work is in the leaves.
  - notes: deferred: queued behind the Ready Now cap; the two decisions are written into TODO-6161 and TODO-6163.


- [~] TODO-6162: `Vector`, `Map` and `SoaColumn` expose their storage fields and a brace constructor that takes any storage
  - owner: ai
  - status: ready
  - claimed_by: ai-private (worktree-agent-aff951f79450d966f), 2026-11-21
  - created_at: 2026-11-21
  - phase: Correctness audit 2026-10 (found with TODO-5511)
  - parallel_track: safety-stdlib
  - scope: With `[/std/collections/vector/Vector<i32> mut] v{...}`, safe code can write `w.data = v.data` (two vectors that free the same storage: `invalid heap free address`), `v.fieldCount = 1000i32; v.at(500i32)` (`invalid indirect address`) and `Vector<i32>{5i32, 5i32, v.data}` (a vector around another's storage), because `data`, `fieldCount`, `fieldCapacity` are `[public]` in stdlib/std/collections/vector.prime; `MapValue` has public `keys` and `payloads`, `SoaColumn` public `count`, `capacity` and `data`. Make them `[private]` (after TODO-6161 enforces it) and give the types a brace constructor that takes only what a user may choose (a capacity), keeping every stdlib file compiling. A user-facing `vector<i32>` binding does not expose the fields (`field access requires struct receiver`), only the explicit `Vector<T>` type does.
  - acceptance:
    - each of the three repros above is `private field is not accessible` (or a constructor `argument count mismatch`) outside `/std/collections`; the stdlib and corpus compile
    - full release gate green
  - stop_rule: visibility of the container fields and their constructors; no container redesign.
  - repro: see scope; `w.data = v.data` runs and ends with `VM error: invalid heap free address in IR`.

- [ ] TODO-6163: Non-public stdlib helpers are callable by absolute path
  - owner: ai
  - status: deferred
  - created_at: 2026-11-21
  - phase: Correctness audit 2026-10 (found with TODO-5511)
  - parallel_track: safety-stdlib
  - scope: docs/spec/backend-type-support.md (Exports) says a definition without `[public]` is private to its module but still callable within the same compilation unit; visibility only affects imports. `/std/collections/vector/vectorCheckShape<i32>(v)` and every other helper without `[public]` therefore compile in user code written by absolute path. The dangerous ones are `[unsafe_api]` now (TODO-6156), so this is about the rest: internal invariants (`vectorReserveInternal`, `vectorAlloc`, the checks) callable out of order.
  - decision_needed: does `[private]` (and the default of no `[public]`) stop an absolute-path call from another module? (A) Yes: a call to a non-public definition from a definition outside its namespace is `<path> is private`, with the stdlib and examples fixed up. Matches C++ and the C++-legacy direction, and makes `[public]` mean what it says. Costs a sweep of the stdlib for helpers that other modules call without `[public]`. (B) No, keep the spec as it is and mark every helper that can break an invariant `[unsafe_api]`. No new rule, but the marker then means two things (memory unsafe, and internal). Recommendation: A for definitions in `/std`, enforced together with the field rule of TODO-6161 so that `private` has one meaning; the spec sentence in Exports changes in the same commit.
  - acceptance:
    - the chosen rule is in docs/spec/backend-type-support.md (Exports) and enforced, with a negative example; stdlib and examples compile
    - full release gate green
  - stop_rule: call-site visibility of definitions; no new visibility keywords.
  - repro: `[effects(heap_alloc), return<int>] main() { [/std/collections/vector/Vector<i32> mut] v{/std/collections/vector/vector<i32>(1i32)} /std/collections/vector/vectorCheckShape<i32>(v) return(0i32) }` compiles.


- [ ] TODO-6165: A container is changed through a `Reference` parameter that is not `mut`
  - owner: ai
  - status: deferred
  - created_at: 2026-11-21
  - phase: Correctness audit 2026-10 (found with TODO-5512)
  - parallel_track: safety-borrows
  - scope: `assign(target, 5i32)` through a plain `[Reference<i32>] target` is `assign target must be a mutable binding: target`, but `dereference(target).clear()` and `target.push(7i32)` through a plain `[Reference<vector<i32>>] target` compile and change the caller's vector. The stdlib marks the writing references `mut` (`insert_ref`, `overwriteSlot`), so the rule "a `Reference` without `mut` is read-only" is already how the code is written; structural changes are the hole. The borrow checks of TODO-6159 can only trust `mut` because of this.
  - acceptance:
    - a structural change (`clear`, `push`, `pop`, `reserve`, `remove_*`, `insert`) of a container through a `Reference` binding or parameter that is not `mut` is `mutating call requires a mutable place` (or the diagnostic the same call on a non-`mut` local gives); the stdlib and corpus compile
    - full release gate green
  - stop_rule: structural mutation through non-`mut` references; no new reference kinds.
  - repro: `[effects(heap_alloc), return<void>] wipe([Reference<vector<i32>>] target) { dereference(target).clear() }` with `wipe(location(v))` compiles and clears `v`.
  - notes: decision needed (checked 2026-11-21, not implemented): the spec does not settle this. docs/spec/pointers-and-references.md says `mut` on a reference binding allows `assign(ref, value)` and that assigning through an immutable pointer or reference is rejected, which is only about whole-value assignment. docs/spec/stdlib-reference.md (Mutex bodies) documents the opposite for structs: "fields of a struct are assigned through a plain `[Reference<Tally>] state`", and the validator accepts `t.total = 5i32` and a `[mut]` helper call `t.bump()` through a plain `Reference<Tally>` today (only `assign(t, 5i32)` on a plain `Reference<i32>` is rejected). `Vector`, `Map` and `SoaColumn` are stdlib structs, so rejecting `clear`/`push` through a plain `Reference<vector<T>>` while `state.items.push(x)` through a plain `Reference<State>` stays legal would be an arbitrary line, and the UI handlers (`[Reference<State>] state`) rely on the second. Options:
    - A. A plain `Reference<T>` is read-only for everything: whole-value assign, field writes and `mut` helper calls (structural changes included). Consistent and simple to state, but it breaks the documented Mutex/handler idiom (`[Reference<Tally>] state` with `state.total = ...`) and every stdlib and example body written that way; each would need `mut` first.
    - B. Keep the spec as is (a plain `Reference<struct>` may change its fields and call `mut` helpers; only whole-value `assign` needs `mut`) and state it explicitly in pointers-and-references.md; the borrow check of TODO-6159 then trusts `mut` only for whole-value writes and must also treat a plain `Reference<container>` argument as a possible structural change (a view live across `wipe(location(v))` is rejected). Smaller change, no migration.
    - C. Only the builtin containers (`vector`, `map`, `soa_vector` and their stdlib structs) need `mut` for structural changes through a `Reference`; other structs keep B. Narrow and matches this leaf's acceptance, but gives containers a rule structs do not have.
    Recommendation: B (the declared idiom wins, and TODO-6159's existing check already needs to cover a plain `Reference` argument under B); if the owner wants A or C, say so and the leaf is a validator change in SemanticsValidatorMoveOnly.cpp plus the stdlib audit of plain `Reference` parameters.

- [ ] TODO-6166: A plain `Reference<vector<T>>` argument can change the container a live view reads
  - owner: ai
  - status: blocked
  - created_at: 2026-11-21
  - phase: Correctness audit 2026-10 (found with TODO-6165)
  - parallel_track: safety-borrows
  - scope: TODO-6159 rejects a `[Reference<T> mut]` or `mut` argument that can change a container a live read-only view reads, on the premise that a plain `[Reference<T>]` parameter is read-only. It is not (TODO-6165): `wipe([Reference<vector<i32>>] target) { dereference(target).clear() }` clears the caller's vector, so `[vector<i32>] view{v}` then `wipe(location(v))` then `view.count()` compiles and reads cleared storage. A plain `Reference` argument cannot simply count as a change, because most plain `Reference` parameters only read (`vectorCountRef`).
  - acceptance:
    - decided with TODO-6165 (option A makes plain `Reference` read-only, so nothing to add here; options B and C need the callee's effect: a call passing a view's root to a plain `Reference<container>` parameter whose body changes it structurally, directly or through callees, is `borrowed binding: v (root: v, sink: view)`)
    - the stdlib and corpus compile (no false positives for read-only `Reference` parameters); negative example with line and column
    - full release gate green
  - stop_rule: the view check at a call with a plain `Reference` argument; no new borrow kinds.
  - repro: `[effects(heap_alloc), return<void>] wipe([Reference<vector<i32>>] target) { dereference(target).clear() } [effects(heap_alloc), return<int>] main() { [vector<i32> mut] v{vector<i32>(1i32, 2i32)} [vector<i32>] view{v} wipe(location(v)) return(view.count()) }` compiles.
  - notes: blocked on the TODO-6165 decision.

- [ ] TODO-5804: GET handlers that only read the state run at the same time under `RwLock`
  - owner: ai
  - status: ready
  - created_at: 2026-11-09
  - phase: Multithreading and Web apps (docs/MultithreadingPrototype.md, The Server)
  - parallel_track: mt-server
  - scope: Under `serveWorkers` every handler holds the app's mutex, so page rendering runs one request at a time (`serveWorkers` in stdlib/std/http/server.prime erases the whole app into one `RequestHandler` held in one `Mutex<RequestHandler>` and takes the lock around each call, `work_slot`). Run read-only handlers under the read lock of an `RwLock<T>` (stdlib/std/sync: `read(body)` over a read-only `T`, `write(body)` over `Reference<T>`; done).
  - decision: 2026-11-20 (open question 5 of docs/MultithreadingPrototype.md, The Server), option B: a separately named registration. `app.view(pattern, handler)` registers a read-only handler that takes its state as `[State] state` (not `Reference<State>`), and `get`/`post`/`route` keep taking `Reference<State>`. Writing the state through a `view` handler is already a diagnostic (writing a `[State]` parameter), which gives the compile error. The stored `RequestHandler` carries the mode (read or write), known before the handler runs, and `serveWorkers` takes the read lock for a `view` handler and the write lock for the others; `serve` (one thread) runs both the same. Existing apps keep their serialized `get` until they switch to `view`. Options A (overloads by state parameter), C (every GET reads) and D (a marker on the lambda) are not taken.
  - implementation_notes: the registration is in stdlib/std/http/server.prime and its app type; switch `todo_web`'s list page and benchmarks/http `GET /work` to `view`; update docs/MultithreadingPrototype.md (The Server, open question 5), docs/WebAppsPlan.md (Concurrency) and docs/spec/stdlib-reference.md.
  - acceptance:
    - the CPU-bound GET handler of benchmarks/http (`GET /work`, registered with `view`; 0.98x measured 2026-11-11, docs/WebAppsPlan.md, Measured) at least 2.5x `serve` with 4 workers, and todo_web's list page with 200 items (0.95x measured) at least 1.5x
    - a `view` handler that writes its state is a compile error (test); a `view` handler and a `get` handler of one app both answer, and a `get` after a `post` sees the change
    - full release gate green
  - stop_rule: the `view` registration and read-lock dispatch only; no sessions, no new method names.
  - open_questions: found 2026-11-20 on the first attempt (nothing implemented; the block's design needs two decisions). Facts measured with primec: (1) a `Function` value's `call` is `mut` (docs/spec/value-lifecycle.md, Function values), so a `view` handler stored as a `Function` in the read-only `T` of `RwLock<T>.read` cannot be called there (`b.f(x)` on a `[T]` parameter is `unknown method: /Box/f`, TODO-6113); copying it first (`[Function<...> mut] h{b.f}` inside `read`, a closure clone with its own state) works and runs on the VM, so concurrent readers each get a private clone of the handler, at one small heap block per request. (2) `serveWorkers<Handler>` receives the whole app (`todo_web` passes `withRequestLog(todo_app())`, a lambda that wraps it), erased today into one `Function` in a `Mutex<RequestHandler>`; the server must learn the mode of a request before it takes a lock, and the language has no way to ask an arbitrary `Handler` for an optional method: overloads `mode<S>([App<S>])` next to `mode<H>([H])` always pick the generic one (TODO-6112), so a free-function protocol that `AppRouter`, `WebApp` and middleware specialise cannot be written today. Erasing the app into a `Function` is not possible either: a read lock over a `Function` that holds the app would have to clone the app's state per request.
  - options_to_decide: (a) fix TODO-6112 (a concrete generic struct parameter beats an unbound `H`), then `readsOnly(handler, request)` and `callView(handler, ...)` are overloads for `AppRouter<State>` and `WebApp<State>` with a generic default of "writes"; `withRequestLog` becomes a struct `RequestLog<Next>` with the same overloads that forward to `next`; the workers become generic over `Handler` (a generic `[spawn]` call works) and hold `RwLock<Handler>`; `AppRouter` gains a `views` route table whose handlers are `Function<void, State, Request, Reference<Response>>` cloned per request. (b) Make `readsOnly(request)` and `callView(...)` required methods of every handler `serveWorkers` takes: `Router`, `AppRouter`, `WebApp` and `RequestLog` get them, and a bare lambda passed to `serveWorkers` is wrapped by `asHandler(lambda)` (breaking for callers that pass a lambda or a struct with only `call`). (c) A separate entry `serveWorkersApp` for apps with views, leaving `serveWorkers` as it is (the block's "serveWorkers takes the read lock for a view handler" then does not hold by name). Option (a) keeps the block's text; (b) is the smallest compiler-free change.

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
  - scope: The audit, the guard (CTest `PrimeStruct_native_ui_startup_guard`: images beyond a plain AppKit program) and `scripts/measure_startup.sh` are done (docs/todo_log.md under TODO-5603); `scripts/measure_app_startup.sh` is gone. Steps (1) and (2) were measured on 2026-11-19 by an agent the user allowed to show windows, on fresh `ditto` copies of the master bundle (signed and warmed as the install recipe does; the installed app was not touched) and are in docs/NativeUiPlan.md section 1 ("The real-launch baseline of TODO-5603"): (1) `--open --runs 10` first text 305 ms without a file and 330 ms with a small `.txt`; (2) the first launch after an install and warm-up started `main` 4 to 23 ms later than the second, inside the expected 50 ms. Left, for a person at the Mac with a quiet machine (the load stayed at 6 to 20 from other jobs for four hours, so the agent could not get under 4): (3) a sampled real launch for TODO-5756, `scripts/measure_startup.sh --open --sample --runs 3 ~/Applications/PrimeScroll.app` with load under 4 (expected `app_init` at most 150 ms; at load 9.4 on a copy it was 164 ms with `ps_ui_init` at 101 ms, so no verdict), and in the unified log (`log show --last 2m --predicate 'process == "PrimeScroll" OR process == "SafariPlatformSupport.Helper"'`) no AutoFill helper, Writing Tools or window restoration before the first drawn text (the agent's `log show --last 20m` for those processes printed no entries at all, so it checked nothing). Steps (1) and (2) on the installed copy after a reinstall, if the person wants the installed bundle itself measured.
  - acceptance:
    - the numbers of (1) to (3), with the load average, are in docs/NativeUiPlan.md section 1 and docs/todo_log.md; anything over its expectation gets a TODO
  - stop_rule: measurement only.
  - notes: step (3) needs a quiet machine and a person (or an agent allowed to show windows) at the Mac; the guard counts images, not milliseconds.

- [ ] TODO-6117: Native text used after its arena ended is read and written silently
  - owner: ai
  - status: ready
  - created_at: 2026-11-20
  - phase: Native PrimeScroll (docs/NativeUiPlan.md; found with TODO-5956)
  - parallel_track: native-arena-stale-text
  - scope: Ending a native `StringArena` writes the free magic into the header of every block it made, so a later `__pstext_free` of that text reports "invalid heap free address: the block is already free" (exit 3), but every other use of it is unchecked: `capacity`, `usable` and `length` answer 0, and a read or write of its bytes touches the arena's region, which stays mapped and which the next arena reuses (a write then corrupts that arena's blocks without any message). The VM faults on any use ("invalid byte address in IR"). Repro: `[String mut] escaped{String{}}`, open a `StringArena`, `escaped = stringFromI32(7i32)`, close the arena, call `escaped.clear()` and print: native prints and exits 3 only at the destroy of `escaped`; the VM stops at `clear()`.
  - implementation_notes: options for the owner (docs/todo.md TODO-5956 decided to keep the regions mapped): (a) leave as is and document it (done in docs/spec/strings-and-views.md); (b) protect the regions of an ended arena (`mprotect` no access) until the next arena reuses them, so a stale use is a SIGSEGV instead of silent; (c) check the header magic in the byte memory loads and stores of the native backend, which costs every load and store. Not decided; do not pick one without the owner.
  - acceptance:
    - the decided option, with a native matrix case that uses text after its arena ended and the expected fault
    - full release gate green
  - stop_rule: stale arena text on the arm64 native backend only.

- [ ] TODO-6118: Native arena regions larger than the standard one are never unmapped
  - owner: ai
  - status: deferred
  - created_at: 2026-11-20
  - phase: Native PrimeScroll (docs/NativeUiPlan.md; found with TODO-5956)
  - parallel_track: native-arena-large-regions
  - scope: A native arena maps a region of 256 KiB; a block that does not fit one gets a region of its own size, and TODO-5956 keeps every region mapped when its arena ends (a standard region goes on a free list the next arena reuses, a larger one is just left). A loop that opens an arena and makes one large text leaks that mapping each round: 3000 rounds of `stringFilled(300000i32, 66u8)` in a `StringArena` reach a peak resident size of 935 MB on native (`/usr/bin/time -l`), where the VM's peak stays flat.
  - implementation_notes: options for the owner: (a) `munmap` the larger regions at the end (a stale use of such text is then a SIGSEGV, exit 139, not silent), or (b) keep a free list of larger regions by size and reuse the first that fits, or (c) map blocks above a limit by the pool's own large path inside the arena and unmap them at the end. (a) is the smallest; the decision of TODO-5956 ("unmaps nothing that a later use could trip over silently") does not say which of these it allows.
  - acceptance:
    - the 3000-round program above runs in bounded memory on native (a test that compares the peak resident size of the first 100 and the last 100 rounds, or a counter, not a timing), and its output is the VM's
    - full release gate green
  - stop_rule: arena regions larger than the standard one only.

- [ ] TODO-6119: A native module with tasks keeps arena text in the pool
  - owner: ai
  - status: deferred
  - created_at: 2026-11-20
  - phase: Native PrimeScroll (docs/NativeUiPlan.md; found with TODO-5956)
  - parallel_track: native-arena-threads
  - scope: The native arena state is the program's (one innermost arena and one list of free regions in the data segment), so a module with `TaskSpawn` cannot let a thread bump from it: its `beginStringArena` and end only change the depth that `stringArenaDepth()` reads, and its text blocks come from the pool and are freed one by one. The VM gives each task thread a text heap of its own with its own arenas (it only disables them for a heap that threads share). Repro: spawn a task, open a `StringArena` on the main task, make 500 strings: `stringArenaBytes()` stays 0 on native and is not 0 on the VM.
  - implementation_notes: the fix is a per-thread arena stack in the thread's allocator state (NativeEmitterInternalsArm64ThreadHeap.h), found through the same lookup the pool uses.
  - acceptance:
    - the arena matrix cases run on a program that also spawns a task, with the same output on the VM, optexe and native
    - full release gate green
  - stop_rule: arenas of threaded native modules only.

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

- [ ] TODO-6059: Native x86_64 float to integer conversion saturates
  - owner: ai
  - status: deferred
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10 (split from TODO-5516)
  - parallel_track: numeric-conversions
  - depends_on: TODO-6056
  - scope: `CVTTSS2SI`/`CVTTSD2SI` return the integer indefinite value (0x8000000000000000) for NaN and out of range values, so x86_64 native differs from the rule (template, register-allocated and JIT emitters, and the u64 path that splits at 2^63). After the conversion, compare for the indefinite value and fix it up (NaN to 0, positive to the maximum, negative to the minimum), and narrow to the i32 range for the I32 forms.
  - acceptance:
    - the TODO-6056 matrix case passes on `native-O0` and `native-O2` on a Linux x86_64 machine
    - full release gate green on Linux x86_64
  - stop_rule: x86_64 float to integer only.
  - notes: deferred: needs a Linux x86_64 machine to run; an untested encoding is not landed (as TODO-6051).

- [ ] TODO-6051: Native x86_64 (ELF) division by zero is the VM's runtime fault
  - owner: ai
  - status: deferred
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10 (split from TODO-5517)
  - parallel_track: runtime-faults
  - scope: `X64Emitter::emitDiv`/`emitDivU` and the register-allocated `DivI32`/`DivI64`/`DivU64` emit `idiv`/`div` with no zero check, so a zero divisor raises SIGFPE (the Linux x86_64 JIT already reports the VM fault). Also `INT64_MIN / -1` raises SIGFPE from `idiv` instead of wrapping as the VM does. Same fix as arm64 native (zero check, then the fault message and exit 3) (the fault message and exit 3 through the existing x64 exit stubs).
  - acceptance:
    - the `runtime_faults` matrix suite passes on `native-O0` and `native-O2` on a Linux x86_64 machine
    - full release gate green on Linux x86_64
  - stop_rule: x86_64 division only.
  - notes: deferred: needs a Linux x86_64 machine to run; the worker of the arm64 fix only had arm64 macOS, and an untested encoding is not landed.

- [ ] TODO-6142: The recursion depth of Linux x86_64 native and `exe` is measured
  - owner: ai
  - status: deferred
  - created_at: 2026-11-20
  - phase: Correctness audit 2026-10 (split from TODO-5517, TODO-6052)
  - parallel_track: runtime-faults
  - scope: vm-design.md (Errors, "Recursion limits") states the macOS arm64 depths of `--emit=native` (SIGSEGV past 8000 to 20000 levels) and `--emit=exe` (20000 to 50000) and says the Linux x86_64 ones are not measured. Measure the same recursive `i32` function (`plus(depth(n - 1i32), 1i32)`) on Linux x86_64 with the default `ulimit -s` and write the two depths into the paragraph.
  - acceptance:
    - the paragraph states the measured Linux x86_64 depths of `native` and `exe`, and the exit status past the limit
    - full release gate green
  - stop_rule: measurement and documentation; no stack-depth check is added.
  - repro: a function `[return<i32>] depth([i32] n)` returning `plus(depth(n - 1i32), 1i32)` for `n > 0`, built with `primec --emit=native` and `--emit=exe`, run at depths 8000, 20000, 50000.
  - notes: deferred: needs a Linux x86_64 machine (the 2026-11-20 worker had arm64 macOS only; as TODO-6051).

- [ ] TODO-6143: Deep recursion on Wasm is a host stack trap, not the VM's `VM call stack overflow`
  - owner: ai
  - status: deferred
  - created_at: 2026-11-20
  - phase: Correctness audit 2026-10 (found with TODO-6052)
  - parallel_track: runtime-faults
  - scope: the VM, `optexe` and the JIT fault past 4096 frames with `VM error: VM call stack overflow` and exit code 3; Wasm lowers a call to a Wasm `call`, so the limit is the host's: Node 24 returned depth 15000 and raised `RangeError: Maximum call stack size exceeded` at 20000 (a trap with no PrimeStruct message). A program that needs more than 4096 levels is portable to neither the VM nor `optexe` but runs on Wasm until the host stack is gone, and what happens then depends on the runtime.
  - options: (a) document only (done by TODO-6052); (b) a frame counter global in every Wasm function that traps at 4096 (cost: a global read and write per call, and a code-size cost); (c) leave it. The owner decides; (b) needs a measurement of the cost on the Wasm matrix.
  - acceptance:
    - per the decision: a depth counter with a test under node that sees the trap at the VM's limit, or the documented difference
    - full release gate green
  - stop_rule: the Wasm call depth only.
  - repro: `primec --emit=wasm --wasm-profile wasi rec.prime -o rec.wasm --entry /main` for the `depth` function above at `depth(20000i32)`, run under Node 24 (`new WebAssembly.Instance(...).exports.main()`).

- [ ] TODO-6124: Wasm lowers pointers, `Reference`, struct values, arrays and closures
  - owner: ai
  - status: blocked
  - blocked_on: TODO-6138
  - created_at: 2026-11-20
  - phase: Correctness audit 2026-10 (found with TODO-6090, TODO-6105)
  - parallel_track: wasm-opcode-gaps
  - scope: The Wasm config of the program matrix (TODO-6090) found that 369 of the 511 matrix programs are refused with `Wasm IR validation error: invalid IR instruction N in /main: unsupported opcode for wasm target` while the VM, native and the C++ emitters run them. Probed one by one on 2026-11-20, each refused on its own: a struct local with two fields read back (`Point{3i32, 4i32}`, `p.x + p.y`), `[Pointer<i32>] p{location(v)}` read with `*p`, a `[Reference<i64> mut]` parameter written with `*r = value` (the whole of TODO-6105's write-through on a `Reference` field is refused for the same reason), `array<i32>(1i32, 2i32)` indexed, a capturing lambda called. TODO-6107 found the same for the array opcodes (`AddressOfLocal`, `LoadIndirect`). Split on 2026-11-20 into TODO-6136 (the error names the opcode and the feature; finished), TODO-6137 (count the refused matrix programs per opcode), TODO-6138 (the local-address and indirect load/store opcodes on the linear memory) and TODO-6139 (list the programs that then agree in `extraConfigs`). This block is the tracker: it closes when TODO-6139 does.
  - acceptance:
    - TODO-6136, TODO-6137, TODO-6138, TODO-6139, TODO-6140 and TODO-6141 are finished
    - full release gate green
  - stop_rule: local-address and indirect memory opcodes only; the heap (`vector`, `String`), atomics and channels, tasks, u8 and host calls are separate leaves.
  - repro: `printf '[return<int>]\nmain() {\n  [i32 mut] v{4i32}\n  [Pointer<i32>] p{location(v)}\n  return(*p)\n}\n' > p.prime; ./primec --emit=wasm --wasm-profile wasi p.prime -o p.wasm --entry /main` now fails with `unsupported opcode for wasm target: AddressOfLocal (...)`.

- [~] TODO-6141: Wasm lowers the heap opcodes and string byte reads (`vector`, `String`)
  - owner: ai
  - status: ready
  - claimed_by: ai-wasm (worktree-agent-ad6161ae949a7116b), 2026-11-21
  - created_at: 2026-11-20
  - phase: Correctness audit 2026-10 (found with TODO-6137)
  - parallel_track: wasm-opcode-gaps
  - scope: After the local-address opcodes, the refusals counted by TODO-6137 are `HeapAlloc` (42 matrix programs as the first refusal: every `vector` program) and `LoadStringByte` (2 programs), then the opcodes behind them (`HeapFree`, `HeapRealloc` and the string heap). TODO-6138's stop rule leaves the heap to a leaf of its own and none existed. Give Wasm `HeapAlloc`, `HeapFree`, `HeapRealloc` on the module's linear memory (a bump allocator with a free list over the pages the shadow stack of TODO-6138 does not use, `memory.grow` when it runs out; out of memory is the VM's fault) and `LoadStringByte` on the data segment of the string table. Settle in the block, before code, how the VM's allocator limits and fault texts map (docs/spec/vm-design.md, Memory) and write the options here if they are not settled.
  - split: 2026-11-21 into TODO-6148 (`LoadStringByte`, 2 programs; done first because nothing about it is open), TODO-6149 (`HeapAlloc`, `HeapFree`, `HeapRealloc` and the heap addresses: 136 + 14 + 5 programs; the address and fault-text question is written there as options, not decided) and TODO-6150 (`heapLiveBlocks()` and the other `__pstext_stat` readings). This block is the tracker and closes when the three do.
  - acceptance:
    - TODO-6148, TODO-6149 and TODO-6150 are finished: a `vector<i32>` that is pushed to, indexed, popped and read by `count` runs on Wasm like the VM, and a program that scans a string by byte index; tests of each
    - full release gate green
  - stop_rule: the heap opcodes and string byte reads only; atomics, channels, tasks, u8 and host calls stay separate leaves.
  - repro: `PRIMESTRUCT_MATRIX_WASM_COMPILE_ONLY=1 ./PrimeStruct_program_matrix_tests 2>&1 | grep -c "HeapAlloc"` from the release build directory.


- [ ] TODO-6149: Wasm slot heap: `HeapAlloc`, `HeapFree`, `HeapRealloc` and heap addresses
  - owner: ai
  - status: deferred
  - created_at: 2026-11-21
  - phase: Correctness audit 2026-10 (split from TODO-6141)
  - parallel_track: wasm-opcode-gaps
  - scope: 136 matrix programs (every `vector`) are first refused for `HeapAlloc`, 14 more for `HeapRealloc` and 5 for `HeapFree`. The VM's heap (docs/spec/vm-design.md, Heap; `include/primec/runtime/VmHeapCore.h`, `VmHeapCoreBody.inc`) hands out addresses `bit 63 | generation << 48 | slot * 16`, serves a request from the smallest free run that fits (lowest base among equal sizes), else extends the free run at the end of the heap, else takes new slots; merges adjacent free runs; zeroes freed slots; gives reused slots the next generation (15 bits, wraps after 32,768 reuses); grows a `HeapRealloc` in place when the next slots are free; and faults with `VM error: invalid indirect address in IR: <address>`, `invalid heap free address in IR: <address>`, `invalid heap realloc address in IR: <address>` and `VM heap allocation overflow` (exit 3). Wasm would keep the slot storage and a per-slot state (generation, live) in its linear memory after the shadow stack (TODO-6147 settles how the two grow), serve `LoadIndirect`/`StoreIndirect` of a heap address through the state check, and write the same messages. The open question is how exact the port must be. Options: (A) a bit-exact port of the single-arena `VmHeapCore` algorithm (same policy, so the same addresses and the same fault texts, `stderr` byte for byte; the free runs need an ordered structure, so the helper functions are hand-assembled wasm of a few hundred instructions, or a sorted array scanned linearly); (B) the same address encoding and the same observable rules (stale and double-freed addresses fault, `heapLiveBlocks()` counts live allocations) with a simpler allocator, so an address printed by a program, or inside a fault text, can differ from the VM's after a free and a reuse; (C) Wasm's own addresses and its own fault texts without numbers, as the arm64 native backend has (`invalid heap free address: the block is already free`), documented as a per-backend difference. The user's direction (identical behavior on every backend) favors (A); (C) has the arm64 backend as precedent. Decide before code.
  - acceptance:
    - the option chosen is written here with a `decision:` line; then a `vector<i32>` that is pushed to, indexed, popped and read by `count`, a double free, a stale address and a `HeapRealloc` of a freed block run on Wasm under node like the VM (stdout, exit code and, for (A), stderr), the matrix programs that then agree list `wasm-wasi`, and an allocation that cannot be served ends with `VM heap allocation overflow` and exit 3
    - full release gate green
  - stop_rule: the slot heap only; byte regions (`HeapAllocBytes`, `LoadU8`) stay with TODO-6127 and the statistic `heapLiveBlocks()` with TODO-6150.
  - repro: `PRIMESTRUCT_MATRIX_WASM_COMPILE_ONLY=1 ./PrimeStruct_program_matrix_tests 2>&1 | grep -c "HeapAlloc"` from the release build directory.

- [ ] TODO-6150: `heapLiveBlocks()` and the string heap statistics on Wasm
  - owner: ai
  - status: blocked
  - blocked_on: TODO-6149
  - created_at: 2026-11-21
  - phase: Correctness audit 2026-10 (split from TODO-6141)
  - parallel_track: wasm-opcode-gaps
  - scope: `heapLiveBlocks()` (stdlib/std/text/string.prime) is `__pstext_stat(8u64)`, a host call the VM answers itself, so a matrix program that reads it (the leak checks of the vector, sum-owner and task programs) is refused with `host calls are only supported by the vm target, ...` (of the 111 programs refused for a host call, 87 read `heapLiveBlocks()` or a string heap statistic; the other 24 use `String` itself). Once TODO-6149 has an allocator, answer the statistic `8` (live slot allocations) in the Wasm module from the allocator's count, and refuse the other `__pstext_*` functions with a message that names the string heap (TODO-6127).
  - acceptance:
    - a program that prints `heapLiveBlocks() - blocks` around a copy-and-destroy loop prints what the VM prints; the matrix programs that then agree list `wasm-wasi`
    - full release gate green
  - stop_rule: the statistic 8 only; the string heap and the other host calls are TODO-6127.
  - repro: `PRIMESTRUCT_MATRIX_WASM_COMPILE_ONLY=1 ./PrimeStruct_program_matrix_tests 2>&1 | grep -c "host calls are only"` from the release build directory.
- [ ] TODO-6153: Twelve gate tests hit the 295 s hard cap together and pass alone in under 11 s
  - owner: ai
  - status: deferred
  - created_at: 2026-11-21
  - phase: Tests (found by the release gate of TODO-6140)
  - parallel_track: flaky-tests
  - scope: One `./scripts/compile.sh --release` run (a second heavy gate was probably running on the machine; the run took 2037 s against 572 s for an earlier run of nearly the same tree) failed twelve tests, all among the first 150 to finish, with `test_watchdog: killed at the hard cap of 295 s`: `compile_run_vm_core_result_exit_codes_1_4` and `_5_8`, `compile_run_vm_outputs_ir_and_output_modes_argv_arrays_77_86_part2_49_49`, `program_matrix_tasks_10_10`, `_15_15`, `_30_30`, `_31_31`, `_32_32`, `program_matrix_text_views_1_1`, `program_matrix_element_assign_2_2`, `_4_4` and `program_matrix_element_receivers_3_3`. Rerun alone they pass in 6 to 11 s each.
  - measured (2026-11-21, ai-flaky, release build of this tree, `nice -n 10 ctest --parallel 22`, 11 cores, other agents' gates running): (1) the twelve tests share no lock, port, cache, temp directory or stdin: each writes under its own `testScratchPath` name, the compile-run and matrix helpers shell out to `./primec`/`primevm` (and `node` for Wasm cases) with no shared file, and nothing in src/, tests/ or scripts/ takes a `flock`/`lockf`/`O_EXCL` lock outside the VM, the build runner and the UI script runner. (2) All twelve started together on the machine at load 99 to 111 with swap 5.9 of 7 GB used and about 5000 free pages: they finished in 18 to 23 s each (23 s for the group), so a stalled 295 s needs a further factor of 13 that load alone did not produce in this run of the group (the full gate of this change is the second measurement). (3) The "primec: X is running" beat is only written while the process tree's CPU time advances, so the killed tests were consuming CPU, not blocked; the kill message could not say where. Not reproduced, so no cause is claimed: the evidence fits a machine-wide stall during the first minutes of that run (memory pressure and swap with a second gate and two `primec` experiments), not a property of the twelve tests.
  - done: `scripts/test_watchdog.py` now prints, with every kill, the last eight beats with times, the beat count, the load average, swap use, the free-memory level and every process of the killed group (state, elapsed, CPU time); tests/scripts/test_test_watchdog.py covers it and docs/TestWatchdog.md describes it. A recurrence names the cause in its own log (swap near full and processes in `U`: memory pressure; `S` with no CPU: a stalled child; `R` with CPU near elapsed: a runaway).
  - acceptance:
    - the next gate that hits a hard cap or a silence kill records the new message in `docs/failing_tests.md`, and the cause it names is fixed here (a shared cost paid once, or the test made cheaper); no timeout is raised and no test is retried
    - or twenty consecutive release gates pass with no kill, then this leaf is closed as not reproducible
  - stop_rule: the tests and shared costs the message names; no timeout is raised and no test is retried.
  - repro: run `./scripts/compile.sh --release` while a second one runs in another worktree, then read `build-release/Testing/Temporary/LastTestsFailed.log` and the kill message in `LastTest.log`.

- [ ] TODO-6127: Wasm refuses u8 and byte memory opcodes and host calls
  - owner: ai
  - status: deferred
  - created_at: 2026-11-20
  - phase: Correctness audit 2026-10 (found with TODO-6090)
  - parallel_track: wasm-bytes-host
  - scope: Matrix programs refused by Wasm for a documented reason, listed so the gap stays visible: `conv_integer_to_u8` and `conv_float_to_u8` (tests/unit/program_matrix/test_program_matrix_conversions.cpp: `u8 and byte memory opcodes are only supported by the vm, the C++ emitters and the arm64 native backend`), `string_arenas_limit` (`host calls are only supported by the vm target, the C++ targets and serialized bytecode`) and `result_buffer_payload_reads_ok` (`graphics stdlib runtime substrate unavailable for wasm-wasi target: /std/gfx/*`). The u8 rows are plain integer conversions (`convert<u8>(300i32)` is 44), so Wasm could do them with an `and` mask; the others need a host decision. Deferred until a Wasm program needs them.
  - acceptance:
    - `convert<u8>` and u8 values compile for Wasm (a mask or `i32.load8_u`/`store8`), the two conversion programs list `wasm-wasi` in `extraConfigs`
    - or: each refusal stays a clear diagnostic that names Wasm and the way out, documented in docs/spec/integration-and-tooling.md
  - stop_rule: u8 and byte opcodes, and the documentation of the two host refusals.

- [ ] TODO-6128: Wasm refuses tasks, atomics, channels and the lock effect
  - owner: ai
  - status: deferred
  - created_at: 2026-11-20
  - phase: Correctness audit 2026-10 (found with TODO-6090)
  - parallel_track: wasm-concurrency
  - scope: Matrix programs refused by Wasm because it has no concurrency: 32 concurrent programs (the `task_*` ones among them) with `invalid IR function /main: unsupported effect mask bits for wasm target` (the effect bits of `spawn` and the related concurrency effects), 18 atomics programs (`atomic operations are not supported by the wasm backend`) and 16 channel programs (`channel operations are not supported by the wasm backend`), counted on 2026-11-20 with the Wasm config of TODO-6090. They are supported on the VM (also under `--vm-threads`), `optexe` and, partly, native. A single-threaded Wasm lowering (tasks run inline at the `spawn`, as the x86_64 native backend runs them, `Atomic` as plain memory operations, `Channel` as a queue) would run the programs whose answer does not depend on the interleaving; the shared-memory Wasm threads proposal is the other route. The owner decides which.
  - acceptance:
    - a decision recorded here between the inline lowering, Wasm threads and "refused with a diagnostic that names Wasm" (the diagnostic is today a validation error that names an effect mask)
    - the chosen lowering or diagnostic, with matrix coverage
  - stop_rule: the decision and its first slice; no change to the VM or native task models.

- [ ] TODO-6121: The VM debugger reports the column of the text-filtered source for a rewritten expression
  - owner: ai
  - status: ready
  - created_at: 2026-11-20
  - phase: Correctness audit 2026-10 (found with TODO-6080)
  - parallel_track: debugger-filtered-columns
  - scope: TODO-6080 maps the spans of diagnostics back to the text the user wrote, but the IR source map keeps the parser's positions: `IrLowererLowerReturnEmitStage.cpp` (instruction source ranges) and `IrLowererLowerSetupStage.cpp` (function syntax provenance) call `mapExpandedSourceLocation`, which finds the source unit and the line but takes the column of the text-filtered source. `VmDebugAdapter.cpp` and `VmDebugDap.cpp` report that column for stack frames and match breakpoints against it, so a breakpoint written at the user's column of `nope` in `[i32] b{a + 2i32 * x}` (column 22) does not match the instruction, whose recorded column is the filtered `plus(a, multiply(2i32, x))` one. Not run: found by reading the call sites while closing TODO-6080; the repro is a DAP `setBreakpoints` with a column and a `stackTrace` over a statement that contains an operator.
  - implementation_notes: `SourceLocationMapper::mapFilteredSourceLocation` (TODO-6080) moves a position to the user's text, but building the offset map costs a second, tracking run of the text filters (about +30 percent instructions on a program that imports collections, measured as the first version of TODO-6080 did), so the lowerer must not call it per instruction. Options to decide: (a) record the filter's offsets in the `ExpandedSource` while the first run is made, at the price of one origin per byte on every compile (a startup cost); (b) build the map only when a debugger or `--emit` mode that writes the source map asks for it; (c) leave the debugger on the filtered column and document it. The owner decides; do not pick one without a measurement of startup instructions.
  - acceptance:
    - per the decision: a test that reads the column of a stack frame and of a breakpoint over a rewritten expression, or the documented limitation
    - startup instructions of a program that compiles are measured and not worse
    - full release gate green
  - stop_rule: the columns of the IR source map only.

- [ ] TODO-6081: An unknown `break` is reported at the user's `break`, not inside stdlib/std/file/file.prime
  - owner: ai
  - status: deferred
  - deferred_reason: needs a repro first
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10 (split from TODO-5520)
  - parallel_track: diagnostics-break-span
  - scope: Seen in the audit; no repro yet. Find a program that reports it (a `break` in a body that reaches a stdlib file helper), write it as a diagnostics test, then fix the span the way the http one was fixed (docs/todo_finished.md, TODO-5936).
  - acceptance:
    - the repro reports the user file, line and column; a diagnostics test asserts them
  - stop_rule: span only.
  - notes: no repro found (tried 2026-11-19 on a release build). `break` is not a PrimeStruct statement, so it is always an unknown call target (`break()`) or identifier (`break`); each of these reports the user's file, line and column and `definition: /main` (or the helper): a `break()` after `File<Write>` open, `writeLine` and `readByte` calls with `?` in `main`; inside a `while` and a `loop` after generic file helper instantiations (`writeLine<Mode, T>`, `readByte<Mode, T>`); in a helper called from a `main` that uses the file helpers; behind `if`. The http bug (TODO-5936) needed a dropped return-type inference failure in a generic stdlib router, and the file helpers have no such failing inference. Next: get the audit's original program from its author, or try `break` in a body whose return type is inferred from a file-helper call.

- [ ] TODO-6082: `borrowed binding: v (root: v, sink: v)` names the sink
  - owner: ai
  - status: deferred
  - depends_on: TODO-5977
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10 (split from TODO-5520)
  - parallel_track: diagnostics-borrow-sink
  - scope: The message names the root of the borrow as the sink. The semantic validator code that builds it belongs to TODO-5977 (one diagnostics builder): do it after that leaf or inside it.
  - acceptance:
    - a diagnostics test where the root and the sink differ names each correctly
  - stop_rule: message of that diagnostic only.

- [ ] TODO-6084: Internal lowering errors (`missing semantic-product ...`) say so
  - owner: ai
  - status: deferred
  - deferred_reason: owner picks the wording
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10 (split from TODO-5520)
  - parallel_track: diagnostics-internal-errors
  - scope: About 40 `missing semantic-product ...` messages report a compiler invariant, not a mistake of the user, and read as one. Options: prefix every one `internal compiler error:` through one helper and ask the user to report it; or leave the text and rely on TODO-6077 for the location. Not chosen: wording.
  - acceptance:
    - an owner picks an option; then one test per prefix path
  - stop_rule: text of internal-invariant messages only.

- [ ] TODO-6085: The semantics validator uses the shared demangle helper
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5977
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10 (split from TODO-5520)
  - parallel_track: diagnostics-demangle-semantics
  - scope: `SemanticsValidator::diagnosticTypeText` and `diagnosticDefinitionText` rebuild the template spelling on their own; make them call the helper of TODO-6079.
  - acceptance:
    - both members delegate to `primec/support/DiagnosticNames.h`; semantics diagnostics tests unchanged
    - full release gate green
  - stop_rule: semantics diagnostics builder only.

- [ ] TODO-6067: x86_64 native emits FMA3 `vfmadd231sd` and `vfmadd231ss`
  - owner: ai
  - status: deferred
  - deferred_reason: needs an x86_64 machine to run the encoding.
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10
  - parallel_track: stdlib-math-fma
  - depends_on: none
  - scope: Replace the multiply-and-add expansion (`expandFusedMultiplyAdd`, primec/ir/IrFmaExpansion.h) with the fused FMA3 encodings (VEX), guarded by a CPUID check or a documented baseline. Not landed without a run on an x86_64 machine.
  - acceptance:
    - `fma` matrix case passes on `native-O0` and `native-O2` on x86_64 Linux
    - full release gate green on Linux
  - stop_rule: x86_64 emitter only.

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

- [ ] TODO-5932: Libraries carry back-end code
  - owner: ai
  - status: deferred
  - created_at: 2026-11-11
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, section 13)
  - parallel_track: lib-backend
  - depends_on: TODO-5925
  - scope: IR-level libraries save the front end only; `--emit=optexe` and `--emit=native` still generate code for the whole merged module at every link. Separate back-end compilation needs libraries that carry per-library C++ translation units or machine code that a linker joins, which would also give C callers an `.a` file. The native backend writes executables directly and has no object writer. Needs scoping against both back ends before it starts; split it then.
  - acceptance:
    - to be written when scoped: a library built once is not regenerated when only its consumer changes, and the link of the final executable is a real link
  - stop_rule: do not start before it is split into leaves for `optexe` and for native.
  - notes: deferred: needs scoping and an object writer.

- [ ] TODO-5933: The embed API loads a library at run time
  - owner: ai
  - status: deferred
  - created_at: 2026-11-11
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, section 14)
  - parallel_track: lib-embed
  - depends_on: TODO-5925
  - scope: The VM runs IR, so `Script` (include/primec/embed/Script.h) could load a `.psa` and link it into a script it compiles, which would let a PrimeScroll script use a precompiled library. The embed library must stay free of process spawning (scripts/check_embed_no_process.sh) and the link runs in-process. Needs a decision on how a script names the libraries it wants and on the startup cost of loading one; scope it when TODO-5925 has landed.
  - acceptance:
    - to be written when scoped: a script that uses a library function runs through the embed API with the library loaded at run time, and a script without libraries starts exactly as before
  - stop_rule: do not start before it is scoped.
  - notes: deferred: not committed; follows if the archive proves out.

- [ ] TODO-6032: A public function of any shape is a function of a library
  - owner: ai
  - status: deferred
  - created_at: 2026-11-18
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, section 7)
  - parallel_track: lib-shapes
  - depends_on: TODO-5925
  - scope: `primec --emit=lib` makes each public non-generic function a real call with the address ABI, and a function whose shape that ABI does not take (an argument pack, a builtin `vector<T>` result, a templated `Reference` parameter, a `string` parameter used by more than `count()`, a sum parameter; see `realCallBlockerReason` in src/ir_lowerer/IrLowererRecursionAnalysis.cpp) is a diagnostic: `public function /packed cannot be compiled into a library: parameter `values` is an argument pack`. A library of the stdlib's own collections cannot be built until those shapes are real calls too. Lowering has paths for some of them (a spawned call's shape takes a `vector` result and parameters, IrLowererTaskSpawn.cpp); decide which shapes the library ABI takes, and lower the rest through a wrapper.
  - acceptance:
    - a public function with an argument pack, one with a `vector<i32>` result and one with a sum parameter are functions of the archive and give the same results as the whole-program build when linked
    - the diagnostic stays for a shape that is still not taken, and names it
    - full release gate green
  - stop_rule: function shapes in the library ABI; no change to generics or to the link.
  - notes: deferred: not committed; follows when a real library needs a shape that is refused today.

- [~] TODO-5973: SoA rewrite passes share one context and one walker
  - owner: ai
  - status: in_progress
  - claimed_by: reuse-a (worktree-agent-a7863bef322c6bc3f), 2026-11-13
  - created_at: 2026-11-12
  - phase: Code reuse (docs/CodeReusePlan.md, section 11)
  - parallel_track: reuse-soa-rewrites
  - scope: src/semantics/SemanticsValidateExperimentalSoaFieldViewRewrites{,CarrierRewrites,HelperRewrites}.cpp and SemanticsValidateBuiltinSoaRewrites{,AccessCountRewrites,MutatorRewrites}.cpp each have a recursive expression walker and a statement walker threaded with the same 8-9 arguments (`bindings`, `allBindings`, `soaCollectionReturnDefinitions`, `specializedSoaVectorElementTypes`, `structFieldNames`, `structPaths`, `visibleSoaFieldHelpers`, the namespace) and repeat the receiver-resolution preamble (`tryReceiverBinding`). The FieldView and HelperRewrites walkers differ in about 120 of 200 lines after renaming. SemanticsValidateBuiltinSoaRewritesAccessCountRewrites.cpp is the most duplicated file in src (67% exact, 75% identifier-normalized). Add a `SoaRewriteContext`, one expression/statement visitor with a per-pass callback, one shared receiver resolver, and a table (name, arity, replacement path) for the access/count rewrites. Before starting, check whether the experimental SoA field-view surface is scheduled for removal; if it is, delete instead of refactoring and say so in the commit.
  - acceptance:
    - one walker and one context serve all six files; the access/count rewrites are table rows
    - SoA semantics, compile-run and program-matrix tests keep their results; AST dumps after semantics of the SoA examples are unchanged
    - full release gate green
  - stop_rule: the six SoA rewrite files and their shared unit only.
  - notes: Safe alongside the lowerer and backend leaves.

- [ ] TODO-5976: The semantic product publishes parsed types, and the lowerer stops re-parsing type text
  - owner: ai
  - status: deferred
  - created_at: 2026-11-12
  - phase: Code reuse (docs/CodeReusePlan.md, section 3)
  - parallel_track: reuse-type-model
  - depends_on: TODO-5961
  - scope: The semantic product carries about 45 `*TypeText*` strings (include/primec/frontend/SemanticProduct.h) and the lowerer re-splits them (163 `splitTemplateTypeName` calls; IrLowererLowerInference* about 5,400 lines and IrLowererSetupType* about 5,700 lines). docs/spec/semantics-lowering-boundary.md says lowering should consume the product without re-derivation; the remaining fallbacks are marked at src/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.cpp:446 and IrLowererLowerStatementsCallsStage.cpp:314. include/primec/support/CanonicalReceiverType.h is the only structured type today. Publish a parsed, interned type reference (base, arguments, element family, resolved receiver) next to the text fields and move lowerer call sites onto it. Deferred: needs scoping into leaves (which fields first, how the reference is interned, memory cost) after TODO-5961 lands.
  - acceptance:
    - to be written when scoped: the lowerer's `splitTemplateTypeName` call count falls to a stated target, and lowering time on the benchmark suite does not rise
  - stop_rule: do not start before it is split into leaves.
  - notes: deferred: needs scoping.

- [ ] TODO-5977: Semantics diagnostics go through one builder
  - owner: ai
  - status: deferred
  - created_at: 2026-11-12
  - phase: Code reuse (docs/CodeReusePlan.md, section 15)
  - parallel_track: reuse-diagnostic-builder
  - scope: `failExprDiagnostic` is a member (src/semantics/SemanticsValidatorPrivateCore.h:399, used from 63 files), but 81 local `auto fail*Diagnostic = [&]` lambdas also exist (`failExprRootDiagnostic` x9, `failStatementDiagnostic` x4, two each of `failPassesEffects*`, `failNamedArgument*`, `failInferDefinition*`, `failContainerHelper*`), each choosing its own span, label and related-span logic. First list them with what each does differently (a table in docs/todo_log.md under this id); then decide whether a builder is worth it. Deferred: not checked whether the lambdas differ in behavior.
  - acceptance:
    - to be written after the survey: the lambdas that only set a message and a span use one builder, and diagnostic text of the semantics tests is unchanged
  - stop_rule: survey first; do not refactor before the table exists.
  - notes: deferred: needs the survey. Do not run alongside TODO-5960.

- [~] TODO-5979: `--collect-diagnostics` reports no false intra-body call diagnostics
  - owner: ai
  - status: ready
  - claimed_by: reuse-b (worktree-agent-a051c39accd554d52), 2026-11-13
  - created_at: 2026-11-12
  - phase: Semantics diagnostics (found while doing TODO-5960)
  - parallel_track: collect-diagnostics-false-positives
  - scope: The intra-body call collector (`collectIntraBodyCallDiagnostics`, src/semantics/SemanticsValidatorPassesDiagnostics.cpp) reports errors that the real validator does not. Checked with `primec` on minimal programs. (a) A valid program with `[Maybe] m{[some] 3i32}` (or `return(Maybe{})`) in a definition compiles and runs under plain `primec`/`primevm` (exit code is the program result) but `primec --emit-diagnostics --collect-diagnostics` exits 2 with `unknown named argument: some` or `argument count mismatch`, so tooling that collects diagnostics sees errors in valid code. (b) With `import /std/collections/*` and one real error elsewhere in the program, the collected list also holds `unknown call target: init`, `drop`, `borrow` inside stdlib definitions (`vectorInitSlot__t...`, `vectorBorrowSlot__t...`, `vectorMovePrefixToBuffer__t...`); plain `primec` stops at the first real error and does not show them, and a valid program with the import collects nothing, so (b) is noise only in collect mode. (c) `/soa/get` and `/soa/ref` give different results per owner: in a definition body the call is rewritten to `/std/collections/soa_vector/*` before collection, in an execution argument it is not, so the execution reports `unknown call target: /soa/get`.
  - acceptance:
    - a minimal valid program using sum construction (`[Maybe] m{[some] 3i32}`, `Maybe{}`) collects no diagnostics, with a semantics test
    - a program that imports /std/collections and has one real error collects only that error, with a test
    - an execution argument `/soa/get(...)` and the same call in a definition body collect the same message, with a test
    - full release gate green
  - stop_rule: the three false positives only; no general diagnostics refactor (TODO-5977).
  - notes: Touches the collector that TODO-5960 merged; do not run alongside TODO-5977.

- [ ] TODO-6113: Calling a `Function` field of a read-only value says `unknown method`
  - owner: ai
  - status: deferred
  - created_at: 2026-11-20
  - phase: Frontend and diagnostics (docs/spec/value-lifecycle.md, Function values; found with TODO-5804)
  - parallel_track: function-call-diagnostic
  - scope: In `lock.read([]([Box] b) { return(b.f(b.base)) })` with `Box{[Function<i32, i32> mut] f ...}` the call `b.f(x)` reports `unknown method: /Box/f [PSC1005]`. The block assumed the reason is that `Function.call` is `mut` and `b` is read-only.
  - open_questions: found 2026-11-20 on the first attempt (nothing implemented; the premise of the block is wrong and the right fix is a language decision). Measured with primec on the VM: (1) the same call in a free function, `read_it([Box] b) { return(b.f(b.base)) }`, compiles and runs, and so do `b0.f(1i32)` on a read-only local `[Box] b0` and `g(1i32)` on a read-only `[Function<i32, i32>] g`; a stateful closure (`[mut n{0i32}]`) called through them changes its own state (prints 2, 3, 2). So the spec line "`call` is `mut`, hold the `Function` in a `mut` binding" (docs/spec/value-lifecycle.md, Function values) is not enforced anywhere (TODO-6116). (2) The `unknown method` message is not a mutability check: the `b.f(x)` to `b.f.call(x)` rewrite in the template monomorphizer (`TemplateMonomorphCallableRewrites.cpp`, the field-callable rewrite) finds the owner's type through the locals and parameters it knows, and a parameter of a lambda is not among them, so the call stays a method call `/Box/f` that does not exist (TODO-6115). Only calls inside lambdas fail; nothing about read-only values does.
  - options_to_decide: (a) enforce the spec: calling a `Function` through a read-only place (a non-`mut` local, parameter or field, a `[T]` lambda parameter) is `calling a Function needs a mutable place: hold a clone in a mut local, [Function<...> mut] h{b.f}`, for every call site; this is what makes concurrent readers of an `RwLock<T>` safe (a closure's state cannot be written through a shared read-only `T`), but it rejects programs that compile today (the gate shows which). Needs TODO-6115 first so lambdas and functions agree. (b) relax the spec: a `Function` is callable through a read-only place; then a state-carrying closure held in a read-only `T` races under `RwLock.read`, so TODO-5804 would need its per-request clone anyway. (c) leave the spec as written and fix only TODO-6115 (lambdas behave like functions), with TODO-6116 tracking the missing check. Option (a) is the one the spec and the language direction (well-defined behavior) point to; it is a breaking change, so it is the user's call.
  - acceptance:
    - the decision above is made and recorded here; then a diagnostic or a spec change with a semantics test and the full release gate green
  - stop_rule: this one decision and the diagnostic or spec line it needs; TODO-6115 and TODO-6116 carry the mechanical parts.

- [ ] TODO-6116: Calling a `Function` through a read-only place is accepted although `call` is `mut`
  - owner: ai
  - status: deferred
  - created_at: 2026-11-20
  - phase: Function values (docs/spec/value-lifecycle.md; found with TODO-6113)
  - parallel_track: function-call-mutability
  - scope: docs/spec/value-lifecycle.md says a `Function`'s `call` is `mut` and the `Function` is held in a `mut` binding, field or parameter, but `[Function<i32, i32>] g{[mut n{0i32}]([i32] x) { n = plus(n, 1i32)  return(plus(x, n)) }}  print_line(g(1i32))` and the same call through a field of a read-only `[Box] b0` compile and change the closure's state. Repro on the VM: a `[mut n{0i32}]` closure in a read-only `[Box] b0` prints 2 then 3 for `b0.f(1i32)` twice. Blocked on the decision in TODO-6113 (enforce the spec, or change the spec).
  - acceptance:
    - per the decision of TODO-6113: either the call through a read-only place is a diagnostic that names the `mut` local clone as the fix, with semantics tests and every existing program that relied on it updated, or the spec line is changed and the unenforced rule removed
    - full release gate green
  - stop_rule: the mutability check of a `Function` call only.

- [ ] TODO-6103: Slots of `map<K, V>` and `soa<T>` storage
  - owner: ai
  - status: deferred
  - created_at: 2026-11-20
  - phase: Value lifecycle (docs/spec/value-lifecycle.md, uninitialized storage)
  - parallel_track: lifecycle-collection-slots
  - scope: Found with TODO-6098, checked with the right spellings (`soa</Particle>`, `map<i32, i32>`, `String`). A `String` slot works (`init(*slot, s)`, `borrow`, `drop`: the count reads back and the heap returns to its start on the VM and optexe). `alloc<uninitialized<map<i32, i32>>>` is a clear lowering error on every backend: `does not support alloc target type: map<i32, i32>` (VM, optexe, native). `init(*slot, ps)` of a `soa</Particle>` into `Pointer<uninitialized<soa</Particle>>>` is the semantic error `init value type mismatch`. So `Mutex<map<..>>` and a `soa` in a slot do not exist yet; this leaf is deferred until a program needs one.
  - acceptance:
    - a slot of a `map<K, V>` and of a `soa<T>` lowers, `init` stores the whole value, `drop` frees it
    - or: each stays a clear diagnostic that names the type and the way out, documented in docs/spec/value-lifecycle.md
  - stop_rule: `alloc`/`init`/`drop` of those slots only.

- [ ] TODO-5987: The lowerer's include-only fragments become components
  - owner: ai
  - status: ready
  - created_at: 2026-11-13
  - phase: Code reuse (docs/CodeReusePlan.md, section 6; proposed by TODO-5968)
  - parallel_track: lowerer-fragments
  - scope: Five src/ir_lowerer headers are not headers: they are bodies of one function, `#include`d in the middle of it to stay under the 1,200-line cap (src/ir_lowerer/IrLowererLowerReturnEmitStage.cpp:467, :473 and :482 include the first three; IrLowererLowerEmitExprContinued.h:497 includes the tail dispatch). Proposed split, one leaf each (they share one function, so they cannot run in parallel): (1) `IrLowererLowerStatementsBindingsContinued.h`, 1,195 lines: the binding-statement lowering from the callee struct surface (`declaredStructSurface`/`calleeStructSurface`) through brace constructors to the initializer kinds; extract (a) struct-surface resolution of a binding's initializer callee and (b) one lowering function per initializer kind (brace constructor, call, literal, reference) taking an explicit binding context. (2) `IrLowererLowerStatementsExprContinued.h`, 885 lines: expression-statement lowering; extract the key-value helper paths (`statementsExprHelpers.resolveSameFamilyKeyValueHelperMemberName`, `keyValueHelperMetadata`, the count helper) as a key-value statement component and the gfx `Buffer.allocate` handling as its own function. (3) `IrLowererLowerEmitExprContinued.h` (497 lines) with `IrLowererLowerEmitExprTailDispatch.h` (1,000 lines) and `...TailDispatchHelpers.{h,cpp}` (71 lines): the tail of `emitExpr`; turn the tail dispatch into an ordered list of rules named by call family (pick/match, native calls, collection helpers, constructors), each taking a plain-data emit context, as TODO-5966, TODO-5967 and TODO-5968 did for semantics. Each leaf is done when its fragment is gone, `check_include_layers.py` and the lowerer tests pass, and IR dumps of examples/ are identical.
  - acceptance:
    - the three leaves above exist as task blocks with their own scope, acceptance and stop_rule
  - stop_rule: this leaf only files the split; no lowerer code changes.
  - notes: Stay out of src/ir_lowerer while TODO-5972 and TODO-5963 are open.

