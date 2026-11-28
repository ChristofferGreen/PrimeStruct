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
| TODO-6231 | Semantics records the effects each definition demands, and infers them over the call graph | deferred | effects-inference |
| TODO-6187 | Node 24 crashes on a failed WASI `path_open` of an absolute path | deferred | wasm-bytes-host |
| TODO-5806 | Sessions: a session store and the handler API | deferred | web-sessions |
| TODO-5821 | The first toolbar of a launch does not hold up the first frame | deferred | startup-toolbar |
| TODO-5603 | The real-launch baseline, measured by a person | ready | startup |
| TODO-5536 | Small, lazily loaded bytecode for apps | deferred | bytecode-startup |
| TODO-6256 | A named key of a menu shortcut reaches AppKit as its name, not as a key | deferred | native-ui-shortcuts |
| TODO-5533 | Windows and Linux native UI backends | deferred | native-ui-platforms |
| TODO-6059 | Native x86_64 float to integer conversion saturates | deferred | numeric-conversions |
| TODO-6051 | Native x86_64 (ELF) division by zero is the VM's runtime fault | deferred | runtime-faults |
| TODO-6142 | The recursion depth of Linux x86_64 native and `exe` is measured | deferred | runtime-faults |
| TODO-6153 | Twelve gate tests hit the 295 s hard cap together and pass alone in under 11 s | deferred | flaky-tests |
| TODO-6180 | Wasm formats and parses floats for `String` | deferred | wasm-bytes-host |
| TODO-6274 | A slot of a `map<K, V>` or a `soa<T>` lowers | deferred | lifecycle-collection-slots |
| TODO-6324 | A discarded call result that is a builtin `map<K, V>` leaks its two buffers | in_progress | inline-task-lowering |
| TODO-6325 | The task program matrix runs the `exe` config with `PRIMESTRUCT_MATRIX_CONFIGS=all` | in_progress | matrix-slow-configs |
| TODO-6326 | The gate never runs the `optexe` and `exe` program-matrix configs | ready | matrix-slow-configs |
| TODO-6327 | `primec` finds `stdlib` only from the working directory or the source's own directories | ready | stdlib-lookup |
| TODO-6081 | An unknown `break` is reported at the user's `break`, not inside stdlib/std/file/file.prime | deferred | diagnostics-break-span |
| TODO-6082 | `borrowed binding: v (root: v, sink: v)` names the sink | deferred | diagnostics-borrow-sink |
| TODO-6085 | The semantics validator uses the shared demangle helper | blocked | diagnostics-demangle-semantics |
| TODO-6067 | x86_64 native emits FMA3 `vfmadd231sd` and `vfmadd231ss` | deferred | stdlib-math-fma |
| TODO-5545 | Record the macOS AppKit manual smoke checklist | deferred | native-ui-macos-manual |
| TODO-6019 | PrimeScroll built by `make.prime` starts as fast as the script's, measured by a person | ready | build-dogfood |
| TODO-5932 | Libraries carry back-end code | deferred | lib-backend |
| TODO-6276 | `optexe` libraries carry a C++ unit | deferred | lib-backend |
| TODO-6277 | Native libraries are object files joined by a linker | deferred | lib-backend |
| TODO-6306 | Ending and reusing a native string arena costs two system calls | deferred | native-arena-stale-text |
| TODO-6318 | x86_64 native and the JIT emit `CallIndirect` | ready | lib-shapes-backends |
| TODO-6315 | The expansion of indirect calls is deleted | blocked | lib-shapes-backends |
| TODO-5987 | The lowerer's include-only fragments become components | ready | lowerer-fragments |
| TODO-5973 | SoA rewrite passes share one context and one walker | ready | reuse-soa-rewrites |
| TODO-5976 | The semantic product publishes parsed types, and the lowerer stops re-parsing type text | deferred | reuse-type-model |
| TODO-5977 | Semantics diagnostics go through one builder | deferred | reuse-diagnostic-builder |

### Ready Now

- TODO-5603 (startup): the real-launch baseline, measured by a person
- TODO-6233 (bytecode-bench): a startup benchmark for bytecode size and load time
- TODO-6240 (bytecode-vm-target): `primec --emit=ir` lowers for the VM target
- TODO-6232 (bytecode-size): the instruction source map names each source unit once

### Immediate Next 10

1. TODO-5603
2. TODO-6233
3. TODO-6240
4. TODO-6232

### Priority Lanes

- Startup, first (docs/NativeUiPlan.md section 1, "Where a launch goes"): TODO-5603, TODO-5821
- Native PrimeScroll, direct native backend (docs/NativeUiPlan.md): deferred TODO-6306 (the cost of protecting arena regions)
- Native UI (docs/NativeUiPlan.md): bytecode size (TODO-6233 benchmark, TODO-6240 VM target, TODO-6232 source units, in this order; decision TODO-5536 after them); ready outside Ready Now  (the inline report) (the backend contract test) (shortcut parsing); deferred TODO-5533 (Windows and Linux backends, platform scope to decide)
- Effects (docs/spec/language-core.md, Effects): option B (an unannotated callee declares the default set) landed with `effects(none)`, the annotated stdlib and the call-site rule; deferred TODO-6231 (inference, optional, not needed by the decision)
- Build system (docs/BuildSystemPlan.md; after the startup lane): TODO-6019
- Modules and libraries (docs/ModulesAndLibrariesPlan.md; after the startup lane): TODO-5933 (`ScriptEngine::addLibrary`) and TODO-6216 (the source-hash compiler build id) landed; TODO-6218 (a link merges only the functions the program reaches) landed; TODO-6217 (`Script::libraries()` and the records in the bytecode) landed; deferred TODO-6219 (the check of the records at load, the options wait for the owner); TODO-6032 (`string` and `Pointer<T>` results) landed; deferred TODO-5932 (back-end code in libraries:  measured the library part of `/std/http` at 0.55 to 0.6 s, under the owner's 1 s trigger, so B does not pay yet; TODO-6276 `optexe` unit and TODO-6277 native stay deferred), TODO-6173 (a borrowed sum parameter, refused as a library root with its rule named) landed, TODO-6213 (the named refusal of sum, `Result` and error struct results) landed, TODO-6222 (a public `[sum]` in the interface and a sum result, option A) landed, TODO-6214 (`vector<T>` results), TODO-6215 (collections-root rule for library roots) and TODO-6223 (packed `Result` and error struct results) landed
- Backend parity:  (Wasm control-flow pattern of two file programs) (Wasm `PrintArgv` fault text); deferred TODO-6182 (Wasm file open errors) (Wasm string heap, byte operations, literals, arguments and `LoadStringLength` landed), TODO-6180 (Wasm text formatting and parsing of floats), TODO-6059, TODO-6051, TODO-6142 (Linux recursion depths) (Wasm concurrency)
- Frontend and diagnostics (split from TODO-5520): deferred TODO-6081, TODO-6082, TODO-6085 (a different receiver text in worker query facts), TODO-6207 (the bare `push` on a stdlib `Vector` has no position)
- Fused `fma` (IR opcodes `FmaF32` and `FmaF64`, done on the VM, the C++ emitters, arm64 native and GLSL): deferred TODO-6067 (x86_64 native FMA3, needs an x86_64 machine) (Wasm exact emulation)
- macOS support: TODO-5545 (needs macOS)
- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix; VM speed ; passes ; real calls TODO-5752
- Multithreading (docs/MultithreadingPrototype.md, Threads For Real; docs/WebAppsPlan.md, Concurrency; after the startup lane): TODO-5872; sessions:  (`Request.cookie`) (`setCookie`) (`randomHex`); deferred TODO-5806 (the session store and handler API, options in the leaf)
- Code reuse (docs/CodeReusePlan.md; two at a time under rule 18): TODO-5973; deferred TODO-5976, TODO-5977
- Lifecycle slots (outside Ready Now): deferred TODO-6274 (`map` and `soa` slots)

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10: the leaves that fix leaks and lifecycle (TODO-5853) first. Ready outside Ready Now (the cap is eight): TODO-6019 (a person measures startup), TODO-6198 (a `Slice` built over any pointer), TODO-6234 (the inline report), TODO-6238 (the native UI contract test), TODO-6239 (shortcut parsing); the Wasm leaf found by the Wasm matrix config that is still ready: TODO-6141 (the heap; TODO-6124 tracks it) and TODO-6320 (the first of the three Wasm concurrency leaves of TODO-6128; TODO-6321 atomics and TODO-6322 locks and channels follow it). Decided 2026-11-20 by the owner; each block carries its `decision:` line. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

Multithreading (docs/MultithreadingPrototype.md, Threads For Real), after the startup lane (TODO-5772 is done); not above them in Ready Now or Immediate Next 10. In this order (TODO-5792 and TODO-5835, VM tasks and their lowering, are done, and so are TODO-5793, byte-region checks without a map search, TODO-5794, `optexe` per-thread heap arenas, TODO-5795, `optexe` threads for `[spawn]`, `Atomic<i32>` and `Atomic<i64>` in `/std/sync`, TODO-5797, `Mutex<T>` with `locked(body)`, TODO-5846, `optexe` faults when every task waits, TODO-5798, bounded `Channel<T>`, TODO-5656, the atomic `SharedString` owner count, TODO-5802, `serveWorkers`, and TODO-5803, the apps on workers, measured, and the `SharedString` count inside the text block, are done): arm64 native is done: TODO-5799 (pthreads; TODO-5942, TODO-5943), TODO-5800 (per-thread heaps) and TODO-5801 (atomics, `Mutex`, `Channel`; TODO-5950, TODO-5951). TODO-5799 is done, and TODO-5893 (an unused `/std/tuple/*` import is reported as an unknown import path) is done too, and TODO-5894 (tuple destructuring and the destructured multi-wait inside a nested block are expanded; TODO-5849, each destructured multi-wait binding has its own result type, is done) is done too, and TODO-5847 (spawned calls with builtin `map<K, V>` or owned sum parameters or results run as tasks, done with TODO-5994, `wait(t).count()` of a stdlib `Vector` result waits twice, is done; TODO-5992, builtin `vector<T>` parameters and results, TODO-5993, the stdlib `Vector` and `Map` structs, TODO-5845, the same for `String`, struct and closure values, and TODO-5895, `copy` and `move` scalars and structs, are done) (TODO-5807, destructured multi-wait, TODO-5790, sendable spawn arguments, and TODO-5791, every `Function` sendable by construction, and TODO-5848, `return(wait(t))` of a `String` task result, are done). TODO-5804 (read-only handlers with `app.view` under `RwLock`, with the compile error for a view that writes its state) is done; deferred: TODO-5806 (sessions); TODO-5805 (keep-alive) is done; TODO-5854 (a `Mutex` or `Atomic` handle keeps a copy of its first value; the unstored `ctor_only` field, on the construction-only field rule of TODO-5853) is done. Found with `Mutex` (TODO-5852, `wait(a, b)` of two `Task<void>`, is done) in the closures lane: TODO-5934 (a `Reference`-returning call read as a value yields its referent, found with TODO-5850), TODO-5850 (a `Reference`-returning call as a closure argument) and TODO-5851 (a `[ref]` closure written in a generic call) are done. Found with `Channel` and ready, in the Result and control flow lane: TODO-5855 (a method call on a `pick` payload of struct type), TODO-5856 (`pick` on a call result frees an owning payload), TODO-5857 (a `vector<T>` payload reads its count in a `pick` arm), TODO-5858 (`some<Vector<i32>>` of a fresh vector frees it), TODO-5859 (a `pick` expression yields a vector), TODO-5860 (moving a payload binding does not poison a later binding of the same name), TODO-5866 (a closure captures a `pick` payload binding), TODO-5877 (a field an arm of a `pick` expression yields is copied), TODO-5879 (a `pick` expression's result owns a value of its own), TODO-5880 (a payload moved out of a sum local leaves the local moved-from, and a payload of a borrowed sum cannot be moved or returned), TODO-5873 (a sum temporary passed as an argument is destroyed after the call), TODO-5874 (assigning to a `mut` sum local destroys the payload it held), TODO-5883 (assigning to a `mut` builtin vector local destroys what it held and copies a place), TODO-5876 (`return(pick(...))` of a vector and assignment from a `pick` expression), TODO-5875 (`count()` on a local declared `Vector<T>` lowers), TODO-5878 (a `[move name]` lambda capture compiles), TODO-5884 (assignment to a `mut` vector parameter and to an inferred vector-of-struct local keeps one owner), TODO-5882 (a vector of sums destroys the payloads of its elements), TODO-5885 (a vector temporary that is only read is destroyed), TODO-5889 (`a = move(b)` of a vector local takes the vector over), TODO-5890 (the arms of an assigned `pick` may open on the next line), TODO-5881 (a call takes a call to the same definition as its argument on the VM), TODO-5888 (an element of a vector temporary read as a struct or a `String`) and TODO-5887 (`remove_at` and `remove_swap` compile on a vector of sums) are done, and found with them: TODO-5886 (`push` on a local declared `Vector<T>` resolves) is done, and TODO-5978 (a `Vector<String>` leaks what `pop` and `clear` remove and faults after `remove_at`; its element reads by `at` and `[]`) is done, and found with it: TODO-5991 (a field read of a struct a call returns leaves the temporary's copy undestroyed) is done; TODO-5891 (a returned vector field or vector element of a local is a copy) and TODO-5892 (a block expression whose value is an owning local hands it over once) are done. Found with `serveWorkers` (TODO-5865, a `Function` local given positionally to a brace constructor, TODO-5864, a lambda in a generic definition naming its type parameters, and TODO-5935, a lambda parameter naming a struct of its namespace, are done): TODO-5862 (`serveWorkers` on the VM over a scripted net, done), and ready: TODO-5866 (a closure captures a `pick` payload binding). Found measuring the apps on workers (TODO-5803, done): TODO-5867 (a string heap that alternates between block sizes keeps its chunks) and TODO-5869 (1 worker as fast as `serve`; the 32 KB form reached 2.41x with 4 workers, not 2.5x) are done; TODO-5940 (a load generator that costs less per connection than the servers it measures, done: compiled, 1.4 us of user CPU per request, and the kernel's loopback setup is the ceiling; TODO-5871 found every server held to about 14,600 new connections a second by the generator and the kernel's loopback setup, which caps the form with 4 workers at 2.41x); TODO-5871 (that measurement), TODO-5872 (a byte of another thread's block is checked on a slow path, so the worker parses from a copy) and TODO-5870 (a VM program that uses tasks runs its string code on the fast kernel) are done; and in the Result and control flow lane TODO-5868 (a definition's `on_error` handler flows into nested blocks and a block-level `on_error` overrides it, so `serve(app)?` compiles in an `if` block of `main`) and TODO-5936 (a diagnostic in `main` of a program that imports `/std/http/server` reported a stdlib position, found with it) are done.

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
  - notes: 2026-11-23, looked at and left: the span of the type needs the position of the template argument (a `Transform` carries the position of its name only; `Transform::templateArgs` has none) and the return-kind diagnostic is built in two local lambdas of src/semantics/SemanticsValidatorBuildReturnKinds.cpp (`failReturnKindDiagnostic` through `failDefinitionDiagnostic`, and `addReturnKindDiagnostic` for `--collect-diagnostics`) that choose the span by hand, `failDefinitionDiagnostic` clearing the context a caller sets first. Giving them another span means editing the semantics diagnostic builder (src/semantics/SemanticsValidatorDiagnostics.cpp and those lambdas), which TODO-5977 (one diagnostics builder) owns. Do it after that leaf: record the position of the first template argument in the parser (`ParserListsTransforms.cpp`, `Transform` in include/primec/ast/Ast.h, a full rebuild), add the offset of the unknown name inside the argument text, and pass the span to the builder.

- [ ] TODO-6231: Semantics records the effects each definition demands, and infers them over the call graph
  - owner: ai
  - status: deferred
  - note: 2026-11-24: not required by the decision of TODO-5688 (option B); kept as an optional tool that finds the definitions whose body demands no effect, to be taken up if the sweeps prove error-prone
  - created_at: 2026-11-24
  - phase: Effects
  - parallel_track: effects-inference
  - scope: Analysis only; no diagnostic changes. The validator checks effects at about fifteen sites (`requires heap_alloc effect`, `File requires ...`, `print_line requires io_out effect`, `dispatch requires gpu_dispatch effect`, the pathspace builtins, atomics, locks, channels, tasks, and `validateCalleeEffects` for declared callee effects: `src/semantics/SemanticsValidatorPassesEffects.cpp`, `SemanticsValidatorStatement.cpp`, `SemanticsValidatorExprScalarPointerMemory.cpp`, `SemanticsValidatorExprResultFile.cpp`, `SemanticsValidatorExprGpuBuffer.cpp`, `SemanticsValidatorExprCollectionLiterals.cpp`, `SemanticsValidatorStatementBuiltins.cpp`). Route them through one `requireEffect(effect)` helper that also records the demand against the definition being validated, record each resolved call edge (caller to callee, at the `validateCalleeEffects` call in `SemanticsValidatorExprCallResolveRules.cpp`), and add a free-function component `src/semantics/EffectInference.{h,cpp}` that turns demands and edges into one inferred effect set per definition (fixpoint, recursion and mutual recursion handled, a definition with an explicit `[effects]` keeps its declared set as the answer and its own demands are only checked against it). Expose the result through a header under `include/primec/testing/` for tests.
  - acceptance:
    - unit tests of `EffectInference` alone (a chain, a diamond, direct and mutual recursion, an explicit declaration cutting the chain) and a semantics test that reads the inferred set of `/std/text/view/StringView/count` (empty), `/std/collections/vector/vectorCheckShape`, a `print_line` helper (`io_out`) and a helper that calls `/std/net/listen` (`net_io`)
    - the time of `primec --emit=ir examples/apps/todo_web/main.prime` measured before and after and written into docs/todo_log.md under TODO-5688 (the cost decides option A against option C)
    - no existing diagnostic or test output changes; full release gate green
  - stop_rule: recording and inference only; the call-site rule stays TODO-5688's.
  - repro: `grep -rn "requires .* effect" src/semantics/*.cpp` lists the sites; the measurement is in TODO-5688's scope.

- [ ] TODO-6187: Node 24 crashes on a failed WASI `path_open` of an absolute path
  - owner: ai
  - status: deferred
  - created_at: 2026-11-23
  - phase: Backend parity (found with TODO-6182)
  - parallel_track: wasm-bytes-host
  - scope: Under node 24.2 (`/opt/homebrew/bin/node`) a Wasm module that calls `path_open` on the preopened directory with an absolute path that does not exist (`File<Read>("/nonexistent_primestruct_dir/missing.txt")`) answers ENOENT (44) and then the node process dies at some later point, nondeterministically (exit 139 segmentation fault, 133 trace trap or 0, about 7 runs in 8). A relative missing path is stable (8 of 8 runs exit 0), and so is every program that only opens existing files. The crash is in node's WASI implementation, not in the emitted module (the trace of `fd_*` and `path_open` calls is identical to a run that exits 0, and no descriptor is closed twice). It keeps `file_io_errno_newline_png` (an absolute missing path and a missing png) off the `wasm-wasi` config, and a flaky matrix case is not allowed. Options: (a) change that program to relative missing paths for the Wasm config (loses the absolute-path coverage on the VM too unless a second case keeps it); (b) make the Wasm emitter answer an absolute path itself, as ENOENT without calling `path_open`, which matches the VM only when the path does not exist; (c) wait for a node fix and record the version. The owner decides.
  - acceptance:
    - a decision recorded here; then `file_io_errno_newline_png` (or its relative-path twin) lists `wasm-wasi` and passes 20 of 20 runs
    - full release gate green
  - stop_rule: the absolute-path failure only.
  - repro: `printf` the program `[return<int> effects(io_out, file_read) on_error<FileError, /on_file_err>] open_missing() { [File<Read>] file{File<Read>("/nonexistent_primestruct_dir/missing.txt")?} return(0i32) }` with `on_file_err([FileError] err) { print_line(err.status()) }` and a `main` that prints `open_missing()`, compile it with `--emit=wasm --wasm-profile wasi` and run the matrix runner (`WasmNodeRunner` in tests/unit/program_matrix/program_matrix.h) eight times: most runs exit 139 or 133 after printing `2` and `2`.

- [ ] TODO-5806: Sessions: a session store and the handler API
  - owner: ai
  - status: deferred
  - created_at: 2026-11-09
  - phase: Web apps (docs/WebAppsPlan.md, Concurrency)
  - parallel_track: web-sessions
  - depends_on: TODO-6235, TODO-6236, TODO-6237
  - scope: Sessions for `WebApp<State>` and `AppRouter<State>`: a random session id in a cookie (HttpOnly, SameSite=Lax), a store of session data with expiry (`/std/os` `monotonicMs`), and handlers that reach their session's data. The three parts that need no design are their own leaves: cookie parsing (TODO-6235), `Set-Cookie` (TODO-6236) and a random source (TODO-6237). What is left is the public API, and it is the owner's to choose: how a handler names its session type, and what a read-only `view` may do (`serveWorkers` runs views under the read lock, so a view cannot create or change a session).
  - options_to_decide:
    - A. A `Sessions<Data>` value held in the app's `State` (`state.sessions`): a handler writes `[Reference<Data>] session{state.sessions.open(request, response)}` (creates the session and sets the cookie when the request has none, refreshes the expiry) and `state.sessions.find(request)` for a read that creates nothing. No change to `WebApp`/`AppRouter`, no new type parameter, the session type is the field's, and a `view` can call `find` (read-only) but not `open`. Cost: the handler calls it explicitly. Recommended: it keeps principle 2 (state comes in as the handler's parameter) and adds one stdlib struct.
    - B. A second type parameter, `WebApp<State, Session>`, and a handler form `(State, Session, Request, Response)`; the framework opens the session before the handler runs. Cost: every handler alias and the `serveWorkers` plumbing get a variant, a view cannot know whether to create a session, every existing app is untouched but the alias set doubles.
    - C. An untyped per-session string map on the request (`request.session.get("user")`). Cost: smallest API, no type safety, values copied through `String`; a poor fit for the language's typed state.
  - also_decide: whether the cookie carries `Secure` by default (there is no TLS in `serve`, so a default `Secure` cookie never reaches the server over plain HTTP; recommendation: an option, off by default, documented), session lifetime defaults (recommendation: 30 minutes idle, a `SessionConfig` with `idleMs`), and whether session ids are 16 random bytes in hex (recommendation: 16 bytes, 32 hex digits, compared in constant time).
  - acceptance:
    - the decision recorded here and the leaf re-sliced (store with expiry and tests on the scripted clock; the handler API; the `todo_web` example gets a per-session counter; docs/spec/stdlib-reference.md and docs/WebAppsPlan.md updated)
    - full release gate green
  - stop_rule: sessions in `WebApp`/`AppRouter`; no persistence across restarts, no CSRF tokens.

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

- [ ] TODO-5536: Small, lazily loaded bytecode for apps
  - owner: ai
  - status: deferred
  - created_at: 2026-10-08
  - phase: Native UI (docs/NativeUiPlan.md)
  - parallel_track: bytecode-startup
  - depends_on: TODO-6232, TODO-6233, TODO-6234, TODO-6240
  - scope: The leaf's premise was measured again on 2026-11-24 (release build, `primec --emit=ir` of a program that reads a PNG, `/std/image/png/read`): the bytecode is 4,381,773 bytes, not 12.7 MB (real calls landed since), and 3,949,939 of them (90%) are the instruction source map: 33,078 entries that each repeat the source unit path (about 119 bytes an entry); the instructions are 430,014 bytes (32,596 instructions of 13 bytes, 32,582 of them in `/main`, 14 other functions). `primevm png.psir` runs 0.04 s against 0.02 s for a 372 byte program, so loading costs about 20 ms. Two causes, both measured: `primec --emit=ir` lowers for the `Serialized` target, which has no address-ABI real calls for structs and strings (`realCallAllowStructs` is false in `src/ir_lowerer/IrLowererLowerSetupStage.cpp`), although `Script::loadBytecode` validates the module for the VM target; a local experiment that lowered `--emit=ir` for the VM target (not committed) took the PNG program from 4,381,773 to 2,644,063 bytes, `examples/apps/text_editor` from 22,186,715 to 6,445,640, `examples/apps/web_app` from 14,610,447 to 4,749,980, `examples/apps/http_hello` from 12,613,431 to 4,012,423 and `examples/native_ui/hello_button` from 1,671,596 to 1,415,917, and `examples/apps/todo_web`, which `--emit=ir` rejects today (`IR backend does not support recursive calls: run_workers<...>`, a `RwLock` parameter), then saved (8,493,566 bytes). TODO-6240 makes that change; TODO-6232 (source units named once) takes the PNG file to about 0.9 MB after it; TODO-6233 (the benchmark) keeps it there; TODO-6234 (a report of what stays inline and why) names the blockers that keep `/main` at 32 k instructions. Lazy decoding of function bodies is not worth its format change while one function holds 99.9% of the instructions; the part of the original scope that is left is lifting the top inline blockers the report names, and a size threshold for real calls.
  - options_to_decide:
    - A. Close this leaf when TODO-6240, TODO-6232 and TODO-6233 have landed, and file one leaf per blocker TODO-6234 names (each a plain value outcome: a smaller module). Cost: none now. Recommended.
    - B. Also build lazy decoding of function bodies (a table of body offsets in the module, `Script::loadBytecode` validating a body on its first call). Cost: a schema change and a lazy validator that must keep every load-time fault (IR validation rejects hostile bytecode at load today, docs/Embedding.md); it pays only for programs with many functions of which few run, which the PNG program is not.
    - C. A size-oriented lowering mode for app bundles (real calls beyond an instruction threshold even for shapes that stay inline today). Cost: a lowering flag, a second code path to test, a runtime cost per call; decide after TODO-6234 shows which shapes dominate.
  - acceptance:
    - the choice recorded here and the leaf either closed (A) or re-sliced into leaves with a measured acceptance each (B, C)
  - stop_rule: bytecode size and load time only.
  - notes: deferred out of the startup lane on 2026-11-09: PrimeScroll ships the direct native build, which has no bytecode, so this is not on its launch path. It matters for bytecode bundles (`primestruct_app_runtime` with `app.psir`).

- [ ] TODO-6256: A named key of a menu shortcut reaches AppKit as its name, not as a key
  - owner: ai
  - status: deferred
  - created_at: 2026-11-24
  - phase: Native UI and host calls (found while landing TODO-6239 and TODO-6237)
  - parallel_track: native-ui-shortcuts
  - scope: An item added with `"cmd+return"`, `"cmd+left"` or `"f5"` is valid for `primec::ui::parseShortcut`, but `parseShortcut` in `src/ui/appkit/NativeUiAppKit.mm` gives `NSMenuItem` the key `"return"` / `"left"` / `"f5"` as its `keyEquivalent`, a string of several characters, where AppKit takes one character (`"\r"`, `NSLeftArrowFunctionKey`, `NSF5FunctionKey`). The behavior is unchanged from before the parser was shared, and it was found by reading the code: it is not verified on a Mac, because no window may be opened here. Map each named key of NativeUiShortcut.h to its character in the AppKit backend.
  - acceptance:
    - (the table landed 2026-11-25: `namedKeyEquivalent` in src/ui/appkit/NativeUiAppKit.mm maps each named key to its AppKit character, and the AppKit backend compiles; what is left is the hand check) a hand check on a Mac (a person) that a menu item with `"cmd+left"` and one with `"f5"` fires its command from the keys; the mapping is a table in the AppKit file with a comment naming each AppKit constant
    - the AppKit backend compiles and the headless contract and shortcut suites pass
  - stop_rule: the key equivalent of named keys in the AppKit backend; no change to the parser or to the headless backend.
  - repro: `primec/ui/NativeUiShortcut.h` accepts the key; read `parseShortcut` in `src/ui/appkit/NativeUiAppKit.mm` (a hand check on a Mac, not run here).
  - notes: deferred: small, not on the critical path of an open goal.

- [ ] TODO-5533: Windows and Linux native UI backends
  - owner: ai
  - status: deferred
  - created_at: 2026-10-08
  - phase: Native UI (docs/NativeUiPlan.md)
  - parallel_track: native-ui-platforms
  - depends_on: TODO-6238, TODO-6239
  - scope: Implement the version-0 ABI (130 functions, `include/primec/ui/NativeUi.h`) with Win32 common controls and with GTK 4, following the coverage matrix in docs/NativeUiPlan.md section 9. What is known on 2026-11-24: the AppKit backend is 5,369 lines (`src/ui/appkit/NativeUiAppKit.mm`) and the headless one 3,100 (`src/ui/headless/`); a native program reaches a backend by name (the direct native build on macOS loads `libps_ui_appkit.dylib` through `LC_LOAD_DYLIB`, `scripts/build_native_app.sh`); `optexe` programs reach shims by `--link-arg` (the `/std/os` precedent: `os_posix.cpp`); primec has almost no Windows host support (`_WIN32` appears in four files, none in the build of the compiler's backends); nothing on this Mac can compile or run GTK or Win32 code (no GTK headers, no windows may open), so a backend cannot be verified here. The two slices that can be prepared and verified on macOS are TODO-6238 (a backend-neutral contract test) and TODO-6239 (portable shortcut parsing). The rest is platform scope, and it is the owner's.
  - options_to_decide:
    - A. Linux first, GTK 4, shipped as an `optexe` program plus a C++ shim file linked with `--link-arg` (`stdlib/std/ui/native/ui_gtk4.cpp`, one `.cpp` file per ABI group), verified by the contract test of TODO-6238 under `xvfb-run` (or `GDK_BACKEND=broadway`) in `.github/workflows/release-gate.yml` on ubuntu. Windows later, after the compiler itself runs on Windows. Cost: a Linux CI image with `libgtk-4-dev`; the first backend is about the size of the headless one plus widgets (estimate 4,000 lines), done in slices per ABI group (window and text view, menus, controls, dialogs, toolbar, tabs, markdown preview). Recommended.
    - B. Both platforms in parallel, Windows through the VM runner (`primestruct_app_runtime` with bytecode, built by MSVC or clang-cl) and Win32 plus RichEdit. Cost: needs a Windows CI machine and the compiler's runtime library building on MSVC (not verified; `CMakeLists.txt` has MSVC branches at lines 138 and 1154 only); the VM runner is the only route until the native emitters have a PE target.
    - C. Linux through Qt instead of GTK 4 (one toolkit also covers Windows). Cost: a large dependency with an LGPL or commercial licence and a moc step; the plan (section 9) chose native widgets per platform so the app looks native.
  - acceptance:
    - the choice recorded here, then one leaf per platform and per ABI group, each verified by the contract test running on the new backend
    - full release gate green
  - stop_rule: split into one leaf per platform before implementation.
  - notes: deferred until the owner chooses; a backend leaf needs a machine that can build and run it.

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

- [ ] TODO-6153: Twelve gate tests hit the 295 s hard cap together and pass alone in under 11 s
  - owner: ai
  - status: deferred
  - created_at: 2026-11-21
  - phase: Tests (found by the release gate of TODO-6140)
  - parallel_track: flaky-tests
  - scope: One `./scripts/compile.sh --release` run (a second heavy gate was probably running on the machine; the run took 2037 s against 572 s for an earlier run of nearly the same tree) failed twelve tests, all among the first 150 to finish, with `test_watchdog: killed at the hard cap of 295 s`: `compile_run_vm_core_result_exit_codes_1_4` and `_5_8`, `compile_run_vm_outputs_ir_and_output_modes_argv_arrays_77_86_part2_49_49`, `program_matrix_tasks_10_10`, `_15_15`, `_30_30`, `_31_31`, `_32_32`, `program_matrix_text_views_1_1`, `program_matrix_element_assign_2_2`, `_4_4` and `program_matrix_element_receivers_3_3`. Rerun alone they pass in 6 to 11 s each.
  - measured (2026-11-21, ai-flaky, release build of this tree, `nice -n 10 ctest --parallel 22`, 11 cores, other agents' gates running): (1) the twelve tests share no lock, port, cache, temp directory or stdin: each writes under its own `testScratchPath` name, the compile-run and matrix helpers shell out to `./primec`/`primevm` (and `node` for Wasm cases) with no shared file, and nothing in src/, tests/ or scripts/ takes a `flock`/`lockf`/`O_EXCL` lock outside the VM, the build runner and the UI script runner. (2) All twelve started together on the machine at load 99 to 111 with swap 5.9 of 7 GB used and about 5000 free pages: they finished in 18 to 23 s each (23 s for the group), so a stalled 295 s needs a further factor of 13 that load alone did not produce in this run of the group (the full gate of this change is the second measurement). (3) The "primec: X is running" beat is only written while the process tree's CPU time advances, so the killed tests were consuming CPU, not blocked; the kill message could not say where. Not reproduced, so no cause is claimed: the evidence fits a machine-wide stall during the first minutes of that run (memory pressure and swap with a second gate and two `primec` experiments), not a property of the twelve tests.
  - note: 2026-11-23: a full gate an agent started without `caffeinate` had 40 timeouts at about 457 s, all at once; every later gate run under `caffeinate -i` was clean. This points at the machine sleeping or stalling, not at load or at the tests.
  - note: 2026-11-26: recurrence under `caffeinate -i`, in an agent's gate of the vector-operator change (only gate on the machine, no other agent heavy work): 24 `Timeout` results, 2097 s wall against 569 s for the green rerun of the same tree. 22 of them ran about 1020 s and ended within seconds of each other at ctest positions 1119 to 1140 of 3083 (`semantics_calls_flow_collections_291_300`, `compile_run_smoke_core_paths_wasm_and_debug_58_58`, `compile_run_vm_outputs_ir_and_output_modes_exe_backend_56_66_part2_25_25`, `..._argv_arrays_67_76_part2_35_35`, `_36_36`, `..._argv_arrays_77_86_part2_46_46`, `_47_47`, `compile_run_vm_collections_alias_and_basics_31_40`, `compile_run_native_backend_pointers_pointers_11_12`, `compile_run_text_filters_library_consume_5_6`, `..._library_corpus_5_5`, `_7_7`, `compile_run_native_backend_math_numeric_apple_math_numeric_31_40`, `program_matrix_element_receivers_16_16`, `program_matrix_lifecycle_23_23`, `_24_24`, `program_matrix_optimizer_13_13`, `program_matrix_results_11_11`, `program_matrix_user_shadows_1_1`, `vm_threads_corpus_tasks_10_r1`, `_25_r6`, `_31_r1`); `map_surface_strict_audit_self_test` (111 s) and `benchmark_report_self_test` (184 s) also timed out. All 24 passed in 6 s in a rerun. The agent saw its shell clock barely advance across long waits in that window, which again fits a machine-wide suspend (`caffeinate -i` prevents only idle sleep) rather than the tests. `LastTest.log`, with the watchdog's process and memory lines, was deleted with the build tree, so the cause is not proven; the compile.sh output (agent scratch `m/gate.log`) has no kill lines. Agents now keep `LastTest.log` of a failed gate.
  - note: 2026-11-27: recurrence in the gate of TODO-6319 (release build, `caffeinate -i`, 11 cores, load average 130 to 176 from other agents): 3 `Timeout` results, `PrimeStruct_primestruct_program_matrix_vector_owners_47_47` (434.6 s; its doctest run reported 52 passed), `PrimeStruct_test_registration` (337 s) and `PrimeStruct_test_inventory` (337 s; both are single-process Python scripts that take 8 s alone), no `test_watchdog` kill line (the kill is ctest's own timeout). Alone the three pass in 17 s together. Logs kept: LastTest.log and LastTestsFailed.log of that gate. The two script tests have no watchdog, so no beat or load line says why.
  - done: `scripts/test_watchdog.py` now prints, with every kill, the last eight beats with times, the beat count, the load average, swap use, the free-memory level and every process of the killed group (state, elapsed, CPU time); tests/scripts/test_test_watchdog.py covers it and docs/TestWatchdog.md describes it. A recurrence names the cause in its own log (swap near full and processes in `U`: memory pressure; `S` with no CPU: a stalled child; `R` with CPU near elapsed: a runaway).
  - acceptance:
    - the next gate that hits a hard cap or a silence kill records the new message in `docs/failing_tests.md`, and the cause it names is fixed here (a shared cost paid once, or the test made cheaper); no timeout is raised and no test is retried
    - or twenty consecutive release gates pass with no kill, then this leaf is closed as not reproducible
  - stop_rule: the tests and shared costs the message names; no timeout is raised and no test is retried.
  - repro: run `./scripts/compile.sh --release` while a second one runs in another worktree, then read `build-release/Testing/Temporary/LastTestsFailed.log` and the kill message in `LastTest.log`.

- [ ] TODO-6180: Wasm formats and parses floats for `String`
  - owner: ai
  - status: deferred
  - created_at: 2026-11-22
  - phase: Backend parity (split from TODO-6150)
  - parallel_track: wasm-bytes-host
  - scope: `String` formats an `f64` with `__pstext_format_f64(value, style, precision, destination, capacity)` and parses decimal text with `__pstext_f64_ok` and `__pstext_parse_f64` (include/primec/runtime/VmTextHeap.h); the VM calls the C++ library's shortest round-trip and fixed/scientific formatting and `strtod`. WebAssembly has no such function and the browser profile has no imports. Decide (options: a hand-written shortest round-trip printer and exact parser in wasm helpers, as the fused multiply-add was done in src/wasm_emitter/WasmEmitterFma.cpp; or a WASI-profile import the host provides, which the browser profile still lacks) before code: the choice changes what a Wasm program may rely on, and the shortest round-trip digits must be the same on every backend.
  - acceptance:
    - the option chosen is written here with a `decision:` line; then `String` formatting and parsing of floats run on Wasm like the VM (matrix programs that print floats through `String` list `wasm-wasi`)
    - full release gate green
  - stop_rule: float text only.
  - repro: `PRIMESTRUCT_MATRIX_WASM_COMPILE_ONLY=1 ./PrimeStruct_program_matrix_tests 2>&1 | grep -c "the string heap"` from the release build directory.
- [ ] TODO-6274: A slot of a `map<K, V>` or a `soa<T>` lowers
  - owner: ai
  - status: deferred
  - created_at: 2026-11-26
  - phase: Value lifecycle (docs/spec/value-lifecycle.md, uninitialized storage; follows the diagnostic slice of the earlier slot leaf)
  - parallel_track: lifecycle-collection-slots
  - scope: `alloc<uninitialized<map<i32, i32>>>` and `alloc<uninitialized<soa<Particle>>>` are lowering errors on every backend that name the type and the way out (docs/spec/value-lifecycle.md). Deferred until a program needs a `Mutex<map<..>>` or a `soa` in a slot: the lowering would give each its record slot count (as `vector<T>` has its five-slot record) and `init`, `borrow`, `take` and `drop` would move and free the whole value, on the VM, optexe and native.
  - acceptance:
    - `init(*slot, m)` of a `map<K, V>` and of a `soa<T>` into `Pointer<uninitialized<..>>` stores the whole value, `borrow` reads it, `drop` frees it, with the heap back at its start on the VM and optexe
    - full release gate green
  - stop_rule: `alloc`/`init`/`borrow`/`take`/`drop` of those slots only.
  - repro: `[Pointer<uninitialized<map<i32, i32>>> mut] table{/std/intrinsics/memory/alloc<uninitialized<map<i32, i32>>>(1i32)}` in a `[effects(heap_alloc), return<int>]` main with `primec --emit=vm`.

- [~] TODO-6324: A discarded call result that is a builtin `map<K, V>` leaks its two buffers
  - owner: ai
  - status: in_progress
  - claimed_by: worktree-agent-ac70ae69ba7bf0b14, 2026-11-28
  - created_at: 2026-11-27
  - phase: Correctness audit 2026-10 (found with TODO-6323)
  - parallel_track: inline-task-lowering
  - scope: a statement `make_table(9i32)` whose definition returns `map<i32, i32>` builds the map and nothing drops it: the heap ends with the map's two `Vector` buffers live. A binding of the same result is freed (TODO-6323). Drop the temporary at the end of the statement, the way the lowering drops a discarded struct or `vector` result.
  - acceptance:
    - a program that discards the result of a map-returning call reports `heapLiveBlocks()` back at its start on the VM, optexe and wasm-wasi, with a matrix case
    - full release gate green
  - stop_rule: the discarded result of a call that returns a builtin `map<K, V>` by value.
  - repro: `[return<map<i32, i32>> effects(heap_alloc)] make_table([i32] value) { [map<i32, i32>] out{map<i32, i32>(1i32, value, 3i32, 4i32)} return(out) }` and `[return<i32> effects(heap_alloc)] round() { make_table(9i32) return(0i32) }`; `main` prints `round()` then `heapLiveBlocks()`: `primevm` prints `0` then `2` (2026-11-27, release build).

- [~] TODO-6325: The task program matrix runs the `exe` config with `PRIMESTRUCT_MATRIX_CONFIGS=all`
  - owner: ai
  - status: in_progress
  - claimed_by: worktree-agent-ac70ae69ba7bf0b14, 2026-11-28
  - created_at: 2026-11-28
  - phase: Correctness audit 2026-10 (found checking a report that `--emit=exe` of `task_builtin_map_arguments` fails with an emission error about `std/text`)
  - parallel_track: matrix-slow-configs
  - scope: `runTaskProgram` in tests/unit/program_matrix/test_program_matrix_tasks.cpp lists every config but `exe` by name (`config.name != "exe"`), so no task program ever ran on `exe`, with or without `PRIMESTRUCT_MATRIX_CONFIGS=all`, though the comment above it says `exe` runs with `all`. The report could not be reproduced: at ecf5b354d `primec --emit=exe`, `--emit=cpp` and `--emit=optexe` of `task_builtin_map_arguments` compile and the binary prints the VM's output; with the exclusion removed all 34 task cases pass on every config including `exe` (1195 assertions), and all 588 program-matrix cases pass with `PRIMESTRUCT_MATRIX_CONFIGS=all` (0 programs fail on `exe`, 0 on `optexe-O2`). The message the report quotes is what `primec` prints when `stdlib` is not found (TODO-6327), which every backend prints alike. Drop the exclusion so the task cases run on `exe` with `all`.
  - acceptance:
    - `PRIMESTRUCT_MATRIX_CONFIGS=all PrimeStruct_program_matrix_tests --test-suite=primestruct.program_matrix.tasks` runs each task case on `exe` and passes
    - full release gate green
  - stop_rule: the one config list in `runTaskProgram`; no task program is skipped on `exe` by name.
  - repro: remove `config.name != "exe" &&` from `runTaskProgram`, build `PrimeStruct_program_matrix_tests` and run the command above in `build-release` (2026-11-28, release build, macOS arm64: 34 passed).

- [ ] TODO-6326: The gate never runs the `optexe` and `exe` program-matrix configs
  - owner: ai
  - status: ready
  - created_at: 2026-11-28
  - phase: Correctness audit 2026-10 (found with TODO-6325)
  - parallel_track: matrix-slow-configs
  - scope: `optexe-O2` and `exe` are `slow` configs of tests/unit/program_matrix/program_matrix.h: they run only with `PRIMESTRUCT_MATRIX_CONFIGS=all` or when a case lists them. No ctest entry, `scripts/compile.sh` or CMake file sets that variable, so a program that breaks on `optexe` or `exe` (the host C++ compiler paths) is never seen by the gate; the full run took about 25 minutes of one core on 2026-11-28, too long for the gate as it is. Add a gate entry that runs every case on those two configs in bounded shards (or a representative subset of each suite), each under the liveness watchdog, without raising any timeout.
  - acceptance:
    - a ctest entry runs program-matrix cases with `PRIMESTRUCT_MATRIX_CONFIGS=all` (the whole set across shards, or a documented subset per suite) and the added gate time is stated in the commit
    - a program that fails to compile or runs wrong on `exe` or `optexe-O2` fails that entry
    - full release gate green
  - stop_rule: the registration of the slow configs; no new matrix programs.
  - repro: `grep -rn PRIMESTRUCT_MATRIX_CONFIGS CMakeLists.txt cmake scripts` prints nothing (2026-11-28).

- [ ] TODO-6327: `primec` finds `stdlib` only from the working directory or the source's own directories
  - owner: ai
  - status: ready
  - created_at: 2026-11-28
  - phase: Correctness audit 2026-10 (found with TODO-6325)
  - parallel_track: stdlib-lookup
  - scope: `primec` run from a directory that is not inside a checkout, on a source outside it, fails every backend with `stdlib import requested but matching stdlib modules were not found: import /std/text/* [PSC1001]` although `stdlib/` sits next to the build directory (`build-release/../stdlib`). The message names no search path and does not suggest `--import-path`. Look for `stdlib` next to the `primec` binary's checkout as a last resort, or name the paths searched in the message.
  - acceptance:
    - the repro compiles, or its message lists the paths searched and the flag that adds one
    - a diagnostics test pins the choice
    - full release gate green
  - stop_rule: the `stdlib` root search and its message; no import-path redesign.
  - repro: copy a program with `import /std/text/*` to a scratch directory outside the repo and run `<checkout>/build-release/primec --emit=vm prog.prime --entry /main` from there (2026-11-28, release build).

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
    - TODO-6275 is done (2026-11-26) and its decision says B does not pay yet; so TODO-6276 (`optexe`) and TODO-6277 (native) stay deferred until a library larger than `/std/http` exists, then TODO-6276 is done, and TODO-6277 after it
    - a library built once is not regenerated when only its consumer changes, and the link of the final executable is a real link
  - stop_rule: tracker; the work is in TODO-6275, TODO-6276 and TODO-6277.
  - notes: split 2026-11-26 into a measurement, an `optexe` leaf and a native leaf, as the owner's 2026-11-22 decision orders (A done as TODO-6174; B only if a real library measures more than 1 s of back-end compile; C not before B).
  - measured: 2026-11-21 (release build, macOS arm64; numbers in docs/todo_log.md under TODO-5932). `--emit=optexe` of an empty `main` takes 0.91 s, of which the front end is 0.02 s (69M instructions) and `clang++` parsing the runtime preamble's standard headers alone is 0.64 s (`-fsyntax-only`); a program of 24k generated lines (examples/apps/http_hello) takes 4.18 s, 1.29 s of it the front end (14.6G instructions) and about 2.9 s the C++ compile and link. So the back end is 98 percent of a small program (a fixed cost every translation unit pays) and about 70 percent of an http-sized one. The emitter writes one translation unit with the runtime in an unnamed namespace, passing the runtime as `Rt &rt` (src/backend/IrToOptCppRuntime.h), so a library unit could share it; a string table, host import declarations and struct layouts are per unit, and the optimizer's inlining across the library boundary is lost. The native back end writes executables directly (src/native_emitter/NativeEmitterMachO.cpp, NativeEmitterElf.cpp) and has no object writer.
  - options_to_decide: (A) cut the fixed cost first, without libraries: a precompiled header of the runtime preamble (measured: an empty `main` 0.70 s to 0.39 s), which every optexe and `exe` compile gains and which needs no library ABI. (B) optexe only: a library carries a C++ unit emitted from its own IR (roots with external linkage, names from their paths), the consumer unit declares them, a cache of the object files keyed by archive hash, compiler build id and flags; costs at least one translation unit of compile per library (0.4 s with the header above) and the loss of cross-library inlining, so it pays only for a library whose body is large. (C) native: an object writer for Mach-O and ELF plus a system linker step, also giving C callers an `.a`; large, and only after B proves out.
  - decision: 2026-11-22 (owner): option A first: TODO-6174 (precompiled header or trimmed includes), which every optexe and `exe` compile gains. B (a C++ unit per library) only if a real library measures more than 1 s of back-end compile of a typical program; C not before B. This block stays deferred until that measurement exists.
  - decision: 2026-11-26 (TODO-6275, numbers in docs/todo_log.md under TODO-5932): B does not pay yet. Of the 1.63 s CPU C++ compile of examples/apps/http_hello (27k generated lines, with the precompiled header) the 41 `/std/http*` functions are 0.55 to 0.6 s (35 percent; their own unit with a copy of the runtime is 1.0 to 1.08 s, and the two units together 2.3 to 2.8 s against 1.63 s as one), under the owner's 1 s trigger. Deferred, not dropped: it is reopened when a library measures above 1 s of its own functions.

- [ ] TODO-6276: `optexe` libraries carry a C++ unit
  - owner: ai
  - status: deferred
  - created_at: 2026-11-26
  - phase: Modules and libraries (split from TODO-5932)
  - parallel_track: lib-backend
  - depends_on: TODO-5925
  - scope: Option B of TODO-5932, for `--emit=optexe` only: a library archive also carries a C++ translation unit emitted from its own IR (its roots with external linkage, names derived from their paths; the runtime shared through the precompiled header and `Rt &rt`, src/backend/IrToOptCppRuntime.h), and the consumer's unit declares them. The object files of libraries are cached by archive hash, compiler build id and flags, so a library built once is not recompiled when only its consumer changes. A string table, host import declarations and struct layouts are per unit (the 2026-11-21 measurement in TODO-5932); cross-library inlining is lost, which is why it pays only for a large library.
  - acceptance:
    - a program linked against a library builds to the same output as the whole-module `--emit=optexe`; the second build after a change in the consumer alone does not run the C++ compiler for the library unit (a test counts the compiler runs)
    - a change in the library, in the compiler or in the flags recompiles it
    - full release gate green
  - stop_rule: `optexe` only; native is TODO-6277.
  - notes: split again into the emitter, the cache and the link step when it starts, each with `scope`, `acceptance` and `stop_rule`, if it is larger than one gate-sized commit.
  - deferred: 2026-11-26, TODO-6275 measured the library part of the largest stdlib module at 0.55 to 0.6 s, under the owner's 1 s trigger (decision on TODO-5932). Set `ready` when a library measures above 1 s of its own functions.

- [ ] TODO-6277: Native libraries are object files joined by a linker
  - owner: ai
  - status: deferred
  - created_at: 2026-11-26
  - phase: Modules and libraries (split from TODO-5932)
  - parallel_track: lib-backend
  - scope: Option C of TODO-5932: the native back end writes executables directly (src/native_emitter/NativeEmitterMachO.cpp, NativeEmitterElf.cpp) and has no object writer. A library would carry machine code in an object file (Mach-O and ELF writers, relocations, symbols) and the executable come from a system linker step; this also gives C callers an `.a` file. Large, and only after TODO-6276 has proved the cache and the link step on `optexe`. Needs its own scoping (object writer first, then the linker step) before code.
  - acceptance:
    - to be written when TODO-6276 is done: split into an object writer leaf per format and the linker-step leaf
  - stop_rule: do not start before it is split into leaves.


- [ ] TODO-6305: An operator site the prediction cannot type validates the program twice
  - owner: ai
  - status: deferred
  - created_at: 2026-11-26
  - phase: Compiler performance (left open by the one-validation fix for operator programs)
  - parallel_track: semantic-memory
  - blocked_on: a program where the second round costs over 10% of its compile time, or a third way helpers are demanded
  - scope: under lazy import, src/pipeline/OperatorHelperPrediction.cpp predicts the `Vec`/`Mat`/`Quat` operator helpers from parameters, typed bindings, brace constructors and nested operators, and `runCompilePipelineRound` restarts import and parse (not validation) to take them. A site it cannot type (`make_vector() + make_vector()`, a method result) still falls back to the validator's `Program::operatorRewriteDemand` and a second validated round, and a typed operator program still imports and parses twice. Two mechanisms now name the same helpers. The full fix lets validation splice a demanded helper mid-run (after template monomorphization has run over the existing definitions), so one pass covers every site and the prediction can go.
  - acceptance:
    - every operator program, typed or not, runs import, parse and validation once (`pipelineRounds` 1 and `pipelineRestarts` 0 in the lazy-import tests), the prediction component is deleted, and the Vec2 `length()` program stays at most 6 definitions
    - full release gate green
  - stop_rule: how operator helpers reach the program; no change to the operators.
  - repro: the lazy-import test with an untyped site (2 rounds) in tests/unit/compile_run/test_compile_run_lazy_stdlib_imports.cpp.

- [ ] TODO-6306: Ending and reusing a native string arena costs two system calls
  - owner: ai
  - status: deferred
  - created_at: 2026-11-26
  - phase: Native PrimeScroll (found with the stale arena text leaf)
  - parallel_track: native-arena-stale-text
  - blocked_on: a program whose arena churn shows in a profile (the cost is about 1.9 microseconds per arena that reuses a region)
  - scope: the native arena protects the regions of an ended arena (`mprotect` no access) and gives them their access back when the next arena takes one, so each arena that reuses a region makes two system calls: 3,000,000 arenas of one small text took 0.19 to 0.25 s before (about 0.07 microseconds each) and 5.9 to 6.1 s after (about 2.0), measured in a quiet moment on macOS arm64 (docs/todo_log.md). A protect of one page costs the same as one of the whole 256 KiB region, so the cost is the call, not the pages.
  - implementation_notes: options for the owner: (a) accept it (an arena per frame costs microseconds); (b) keep the most recently ended region unprotected until the next arena end, which spends no call on a loop of one arena at a time and leaves the last stale text readable; (c) protect lazily, when more than N regions sit on the free list. Each option weakens the fault for the newest stale text.
  - acceptance:
    - the owner picks an option, or closes the leaf as (a)
  - stop_rule: the cost of the protect and unprotect calls of arenas; no change to which stale uses fault under option (a).
  - repro: native `--emit=native` of a program that opens and ends `StringArena` 3,000,000 times around one `stringFromI32` (see docs/todo_log.md, TODO-6306).

- [ ] TODO-6318: x86_64 native and the JIT emit `CallIndirect`
  - owner: ai
  - status: ready
  - created_at: 2026-11-27
  - phase: Modules and libraries (back ends of TODO-6283, split from TODO-6312)
  - parallel_track: lib-shapes-backends
  - scope: the x86_64 native emitters (src/native_emitter, ELF) and the Linux x86_64 JIT (src/native_emitter/NativeJit.cpp) emit the opcodes as the arm64 emitter does after TODO-6312 (a table of function entries, `call` through a register with the argument stack of a `Call`). x86_64 native and the JIT keep the expansion until then, so the expansion list holds them. This needs an x86_64 Linux machine to build and run.
  - acceptance:
    - the closure programs of the program matrix run on x86_64 native and the JIT without the expansion, on a machine that can run them
    - full release gate green
  - stop_rule: x86_64 native and JIT code generation of the opcodes.
  - repro: `--emit=native` on x86_64 of a program with `Function` values: its IR holds a compare chain instead of `CallIndirect`.

- [ ] TODO-6315: The expansion of indirect calls is deleted
  - owner: ai
  - status: blocked
  - blocked_on: TODO-6318
  - created_at: 2026-11-26
  - phase: Modules and libraries (end of TODO-6283)
  - parallel_track: lib-shapes-backends
  - depends_on: TODO-6312, TODO-6313, TODO-6314, TODO-6318
  - scope: `expandIndirectCalls` (src/ir/IrIndirectCallExpansion.cpp) and its call in `prepareIrModule` go once every target runs the opcodes. Done 2026-11-27: arm64 native, Wasm and the C++ targets run them, the expansion is run for x86_64 native only, and GLSL refuses a module that has them with a message that names the opcode (test in tests/unit/runtime/test_ir_indirect_calls.cpp). What is left: TODO-6318 (x86_64 native and the JIT) must close first, then delete the pass, its header, its tests and the x86_64 call.
  - acceptance:
    - no source file mentions the expansion
    - full release gate green
  - stop_rule: deleting the compatibility expansion.
  - repro: `grep expandIndirectCalls src` finds the pass today.

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

