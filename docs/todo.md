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
| TODO-6258 | Two definition-validation workers publish extra `block` call facts for the `/std/maybe` constructors | deferred | semantic-workers-stale-facts |
| TODO-6187 | Node 24 crashes on a failed WASI `path_open` of an absolute path | deferred | wasm-bytes-host |
| TODO-5806 | Sessions: a session store and the handler API | deferred | web-sessions |
| TODO-5821 | The first toolbar of a launch does not hold up the first frame | deferred | startup-toolbar |
| TODO-5603 | The real-launch baseline, measured by a person | ready | startup |
| TODO-6117 | Native text used after its arena ended is read and written silently | ready | native-arena-stale-text |
| TODO-6118 | Native arena regions larger than the standard one are never unmapped | deferred | native-arena-large-regions |
| TODO-6119 | A native module with tasks keeps arena text in the pool | deferred | native-arena-threads |
| TODO-5536 | Small, lazily loaded bytecode for apps | deferred | bytecode-startup |
| TODO-6256 | A named key of a menu shortcut reaches AppKit as its name, not as a key | deferred | native-ui-shortcuts |
| TODO-6251 | A stdlib `Vector` record parameter keeps the collections helpers inline | deferred | bytecode-inline-blockers |
| TODO-6252 | A `File` parameter keeps the PNG reader helpers inline | deferred | bytecode-inline-blockers |
| TODO-6253 | A `string` parameter used beyond `count()` keeps the PNG file readers inline | deferred | bytecode-inline-blockers |
| TODO-6254 | `field_count` and `field_capacity` of the collections records are expanded at every call site | deferred | bytecode-inline-blockers |
| TODO-6255 | A stdlib error struct result keeps its constructors inline | deferred | bytecode-inline-blockers |
| TODO-5533 | Windows and Linux native UI backends | deferred | native-ui-platforms |
| TODO-5511 | Safe code cannot reach container storage or unsafe stdlib helpers | deferred | safety-stdlib |
| TODO-6198 | Safe code builds a `Slice` of any count over any pointer | ready | safety-slices |
| TODO-6059 | Native x86_64 float to integer conversion saturates | deferred | numeric-conversions |
| TODO-6051 | Native x86_64 (ELF) division by zero is the VM's runtime fault | deferred | runtime-faults |
| TODO-6142 | The recursion depth of Linux x86_64 native and `exe` is measured | deferred | runtime-faults |
| TODO-6153 | Twelve gate tests hit the 295 s hard cap together and pass alone in under 11 s | deferred | flaky-tests |
| TODO-6124 | Wasm lowers pointers, `Reference`, struct values, arrays and closures | blocked | wasm-opcode-gaps |
| TODO-6127 | Wasm refuses u8 and byte memory opcodes and host calls | deferred | wasm-bytes-host |
| TODO-6180 | Wasm formats and parses floats for `String` | deferred | wasm-bytes-host |
| TODO-6181 | A Wasm `__pstext_stat` of an unknown number traps instead of the VM's fault text | deferred | wasm-bytes-host |
| TODO-6230 | Wasm `PrintArgv` and `PrintArgvUnsafe` read the arguments into fixed buffers | deferred | wasm-bytes-host |
| TODO-6128 | Wasm refuses tasks, atomics, channels and the lock effect | deferred | wasm-concurrency |
| TODO-6133 | The binding initializer accepts an f64 value for an `f32` binding, assignment does not | ready | binding-float-compat |
| TODO-6081 | An unknown `break` is reported at the user's `break`, not inside stdlib/std/file/file.prime | deferred | diagnostics-break-span |
| TODO-6082 | `borrowed binding: v (root: v, sink: v)` names the sink | deferred | diagnostics-borrow-sink |
| TODO-6084 | Internal lowering errors (`missing semantic-product ...`) say so | deferred | diagnostics-internal-errors |
| TODO-6085 | The semantics validator uses the shared demangle helper | blocked | diagnostics-demangle-semantics |
| TODO-6067 | x86_64 native emits FMA3 `vfmadd231sd` and `vfmadd231ss` | deferred | stdlib-math-fma |
| TODO-5545 | Record the macOS AppKit manual smoke checklist | deferred | native-ui-macos-manual |
| TODO-6019 | PrimeScroll built by `make.prime` starts as fast as the script's, measured by a person | ready | build-dogfood |
| TODO-5932 | Libraries carry back-end code | deferred | lib-backend |
| TODO-6219 | `loadBytecode` of a script linked with a library says whether that library changed | deferred | lib-embed |
| TODO-5987 | The lowerer's include-only fragments become components | ready | lowerer-fragments |
| TODO-5973 | SoA rewrite passes share one context and one walker | ready | reuse-soa-rewrites |
| TODO-5976 | The semantic product publishes parsed types, and the lowerer stops re-parsing type text | deferred | reuse-type-model |
| TODO-5977 | Semantics diagnostics go through one builder | deferred | reuse-diagnostic-builder |
| TODO-6103 | Slots of `map<K, V>` and `soa<T>` storage | deferred | lifecycle-collection-slots |

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
- Native PrimeScroll, direct native backend (docs/NativeUiPlan.md): TODO-6117; deferred TODO-6118, TODO-6119
- Native UI (docs/NativeUiPlan.md): bytecode size (TODO-6233 benchmark, TODO-6240 VM target, TODO-6232 source units, in this order; decision TODO-5536 after them); ready outside Ready Now  (the inline report) (the backend contract test) (shortcut parsing); deferred TODO-5533 (Windows and Linux backends, platform scope to decide)
- Effects (docs/spec/language-core.md, Effects): option B (an unannotated callee declares the default set) landed with `effects(none)`, the annotated stdlib and the call-site rule; deferred TODO-6231 (inference, optional, not needed by the decision)
- Build system (docs/BuildSystemPlan.md; after the startup lane): TODO-6019
- Modules and libraries (docs/ModulesAndLibrariesPlan.md; after the startup lane): TODO-5933 (`ScriptEngine::addLibrary`) and TODO-6216 (the source-hash compiler build id) landed; TODO-6218 (a link merges only the functions the program reaches) landed; TODO-6217 (`Script::libraries()` and the records in the bytecode) landed; deferred TODO-6219 (the check of the records at load, the options wait for the owner); TODO-6032 (`string` and `Pointer<T>` results) landed; deferred TODO-5932 (back-end code in libraries, after TODO-6174 is measured), TODO-6173 (a borrowed sum parameter, refused as a library root with its rule named) landed, TODO-6213 (the named refusal of sum, `Result` and error struct results) landed, TODO-6222 (a public `[sum]` in the interface and a sum result, option A) landed, TODO-6214 (`vector<T>` results), TODO-6215 (collections-root rule for library roots) and TODO-6223 (packed `Result` and error struct results) landed
- Memory safety (docs/spec/type-system.md Memory safety): deferred TODO-5511 (tracker); ready outside Ready Now TODO-6198 (a `Slice` built over any pointer)
- Backend parity: TODO-6124 (Wasm pointers, structs, arrays; tracker of TODO-6138, TODO-6139, TODO-6140 and TODO-6141); deferred TODO-6182 (Wasm file open errors) (Wasm string heap, byte operations, literals, arguments and `LoadStringLength` landed), TODO-6230 (Wasm `PrintArgv` argument buffers), TODO-6180 (Wasm text formatting and parsing of floats), TODO-6059, TODO-6051, TODO-6142 (Linux recursion depths), TODO-6127 (Wasm u8, host calls), TODO-6128 (Wasm concurrency)
- Frontend and diagnostics (split from TODO-5520): deferred TODO-6081, TODO-6082, TODO-6084, TODO-6085, TODO-6258 (extra `block` call facts with more than one validation worker), TODO-6207 (the bare `push` on a stdlib `Vector` has no position)
- Fused `fma` (IR opcodes `FmaF32` and `FmaF64`, done on the VM, the C++ emitters, arm64 native and GLSL): deferred TODO-6067 (x86_64 native FMA3, needs an x86_64 machine) (Wasm exact emulation)
- macOS support: TODO-5545 (needs macOS)
- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix; VM speed ; passes ; real calls TODO-5752
- Multithreading (docs/MultithreadingPrototype.md, Threads For Real; docs/WebAppsPlan.md, Concurrency; after the startup lane): TODO-5872; sessions:  (`Request.cookie`) (`setCookie`) (`randomHex`); deferred TODO-5806 (the session store and handler API, options in the leaf)
- Code reuse (docs/CodeReusePlan.md; two at a time under rule 18): TODO-5973; deferred TODO-5976, TODO-5977
- Lifecycle bugs found with TODO-6098 (outside Ready Now): deferred TODO-6103 (`map` and `soa` slots)

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10: the leaves that fix leaks and lifecycle (TODO-5853) first. Ready outside Ready Now (the cap is eight): TODO-6019 (a person measures startup), TODO-6198 (a `Slice` built over any pointer), TODO-6234 (the inline report), TODO-6238 (the native UI contract test), TODO-6239 (shortcut parsing); the Wasm leaf found by the Wasm matrix config that is still ready: TODO-6141 (the heap; TODO-6124 tracks it). Decided 2026-11-20 by the owner; each block carries its `decision:` line. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

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

- [ ] TODO-5511: Safe code cannot reach container storage or unsafe stdlib helpers
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: safety-stdlib
  - scope: Tracker. Done (2026-11-21): the `Vector` and ring buffer slot helpers are `[unsafe_api]` (TODO-6156) and the SoA slot helpers (TODO-6160). Field privacy is enforced (TODO-6161) and the `Vector`, `Map` and `SoaColumn` fields are private (TODO-6162). Namespace-level `/std` definitions without `[public]` are private to their namespace (TODO-6163). Methods of `/std` structs follow the field rule and the internal ones are `[private]`; `[private]` is enforced for every definition and method (TODO-6197, decided 1a and 2b). Open: slices built over any pointer (TODO-6198).
  - acceptance:
    - TODO-6161, TODO-6162 and TODO-6163 are done; each repro (borrowSlot then reserve, takeSlot then clear, `w.data = v.data`, writing fieldCount, brace-constructing a Vector around `v.data`) is rejected outside `[unsafe]`; stdlib and corpus still compile
    - full release gate green
  - stop_rule: tracker; the work is in the leaves.
  - notes: deferred: queued behind the Ready Now cap; the remaining leaf is TODO-6198.



- [ ] TODO-6198: Safe code builds a `Slice` of any count over any pointer
  - owner: ai
  - status: ready
  - created_at: 2026-11-23
  - phase: Correctness audit 2026-10 (found with TODO-6163)
  - parallel_track: safety-slices
  - scope: `ReadSlice<T>` and `WriteSlice<T>` (stdlib/std/collections/slice.prime, written by scripts/generate_slice_stdlib.py) declare `[public Pointer<T> mut] data` and `[public i32 mut] fieldCount`, and `readSliceMake` / `writeSliceMake` are public, so a program outside `[unsafe]` builds a slice of 1000 elements over one local. On the VM the read is `VM error: invalid indirect address in IR: 8016` (exit 3); on native it reads memory past the local and prints 0 (exit 0). The fields of `Vector`, `MapValue` and `SoaColumn` are private for this reason (docs/spec/type-system.md, Memory safety). Make the two fields `[private]` and the two constructors `[public unsafe_api]`, and mark the library callers that build a slice from storage they own (`vectorView`, `vectorSlice`, `vectorSliceMut`, `StringView.bytes`, the HTTP server's `work_slot`, the slice methods) `[unsafe]`.
  - acceptance:
    - the program below is `private field is not accessible: /std/collections/slice/ReadSlice/data`, and `readSliceMake` from a definition that is not `[unsafe]` is `requires an unsafe definition`, each with a test with line and column; the library, examples and tests compile
    - full release gate green
  - stop_rule: the construction of the two slice structs; no change to what a slice can do once built.
  - repro: `import /std/collections/slice/*  [effects(io_out) return<int>] main() { [i32 mut] x{7i32}  [ReadSlice<i32>] s{ReadSlice<i32>{[data] location(x), [fieldCount] 1000i32}}  print_line(s.count())  print_line(s.at(500i32))  return(0i32) }` with `primec --emit=native -I stdlib` prints 1000 and 0.

- [ ] TODO-6258: Two definition-validation workers publish extra `block` call facts for the `/std/maybe` constructors
  - owner: ai
  - status: deferred
  - created_at: 2026-11-25
  - phase: Frontend and diagnostics (semantic validation workers; found with TODO-6203)
  - parallel_track: semantic-workers-stale-facts
  - scope: The semantic product of a program that imports `/std/text/*` and `/std/text/view/*` differs between 1 and 2 workers by four facts: `direct_call_targets` and `query_facts` for `call_name="block" resolved_path="/std/maybe/block" provenance_handle=0 source="0:0"` in the scopes `/std/maybe/none__t938e9516a78205be` and `/std/maybe/none__tead9077f04525e0f` exist only with workers. Compare `primec --emit=vm -I stdlib t.prime --dump-stage semantic-product` with and without `--benchmark-semantic-definition-validation-workers 2` (sort the lines, drop the `[n]` indexes).
  - acceptance:
    - the products of 1, 2 and 4 workers are line-for-line equal for that program, with a test that compares the whole product, not only the array extent facts (`tests/unit/semantics/test_semantics_definition_partitioner.cpp`)
    - full release gate green
  - stop_rule: the facts of the worker merge; no change to single-worker validation.
  - repro: the command in scope, with the program of TODO-6203.
  - notes: deferred: the option is a benchmark switch and the default is one worker.

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
    - a hand check on a Mac (a person) that a menu item with `"cmd+left"` and one with `"f5"` fires its command from the keys; the mapping is a table in the AppKit file with a comment naming each AppKit constant
    - the AppKit backend compiles and the headless contract and shortcut suites pass
  - stop_rule: the key equivalent of named keys in the AppKit backend; no change to the parser or to the headless backend.
  - repro: `primec/ui/NativeUiShortcut.h` accepts the key; read `parseShortcut` in `src/ui/appkit/NativeUiAppKit.mm` (a hand check on a Mac, not run here).
  - notes: deferred: small, not on the critical path of an open goal.

- [ ] TODO-6251: A stdlib `Vector` record parameter keeps the collections helpers inline
  - owner: ai
  - status: deferred
  - created_at: 2026-11-24
  - phase: Native UI (docs/NativeUiPlan.md; a blocker the inline report names for the PNG reader, TODO-5536)
  - parallel_track: bytecode-inline-blockers
  - depends_on: TODO-5536
  - scope: The `/std/collections/vector` helpers of the PNG reader (`count`, `vectorCheckShape`, `vectorElementRef`, `vectorCheckIndex`, `vectorSlotUnsafe`, `vectorBorrowSlot`) take the `Vector` record as a parameter, which the address ABI does not pass (`parameter-type`): six definitions, 63 call sites, 3,000 instructions added. Let a real call take a borrowed (read-only or `Reference`) `Vector` record by its address, as it takes a struct, and keep the helpers that must stay inline for another reason (`field_count`, `field_capacity`) as they are.
  - acceptance:
    - the report of the PNG program names those six as `real call`, and the `added` of `parameter-type` lines naming a `Vector` record falls from 3,000 to under 1,000
    - the module of the PNG program still validates and `primevm` prints the same width, height and pixel count
    - full release gate green
  - stop_rule: borrowed `Vector` record parameters only; no change to owned (`copy`, `move`) or `mut` collection parameters.
  - repro: `primec --emit=vm --dump-stage inline-report benchmarks/startup/png.prime` (release build, macOS arm64, 2026-11-24; `/main` is 4,097 instructions in a module of 59 functions).
  - notes: deferred: TODO-5536 decides whether the bytecode size is worth the lowering work; the counts are in docs/todo_log.md under TODO-5536.

- [ ] TODO-6252: A `File` parameter keeps the PNG reader helpers inline
  - owner: ai
  - status: deferred
  - created_at: 2026-11-24
  - phase: Native UI (docs/NativeUiPlan.md; a blocker the inline report names for the PNG reader, TODO-5536)
  - parallel_track: bytecode-inline-blockers
  - depends_on: TODO-5536
  - scope: The reader helpers `ppmReadByteStatus`, `pngReadU32Be`, `pngReadIhdr`, `pngAppendBytes`, `pngValidateSignature` and `pngReadChunkType` take a `File<Read>` and stay inline (`parameter-type`): six definitions, 38 call sites, 1,758 instructions added. Let a real call pass a `File` handle (one slot, a scalar value at run time) by `Reference` or by value.
  - acceptance:
    - the report of the PNG program names those six as `real call`, and the `added` of `parameter-type` lines naming `File` falls from 1,758 to under 500
    - the file handle is closed once on every exit (a test with an early return), and `primevm` prints the same output for the PNG program
    - full release gate green
  - stop_rule: the `File` parameter shape; no change to what a file operation does.
  - repro: `primec --emit=vm --dump-stage inline-report benchmarks/startup/png.prime` (release build, macOS arm64, 2026-11-24; `/main` is 4,097 instructions in a module of 59 functions).
  - notes: deferred: TODO-5536 decides whether the bytecode size is worth the lowering work; the counts are in docs/todo_log.md under TODO-5536.

- [ ] TODO-6253: A `string` parameter used beyond `count()` keeps the PNG file readers inline
  - owner: ai
  - status: deferred
  - created_at: 2026-11-24
  - phase: Native UI (docs/NativeUiPlan.md; a blocker the inline report names for the PNG reader, TODO-5536)
  - parallel_track: bytecode-inline-blockers
  - depends_on: TODO-5536
  - scope: `readImpl`, `read`, `openRead` and `open_read` of `/std/image` take a `string` path that the body passes on to a file open, not only to `count()` or `__pstext_literal`, so they stay inline (`string-parameter`): four definitions, 4 call sites, 1,390 instructions added (`readImpl` alone adds 1,336). Allow a `string` parameter whose uses are all arguments that take a module-table string index (a file open, a host call).
  - acceptance:
    - the report names `readImpl` as `real call`, and the `added` of `string-parameter` lines falls from 1,390 to under 300
    - a test that opens a file through a real call with a literal path on the VM and on optexe, and `primevm` prints the same output for the PNG program
    - full release gate green
  - stop_rule: the use rule of a `string` parameter; no new string operations.
  - repro: `primec --emit=vm --dump-stage inline-report benchmarks/startup/png.prime` (release build, macOS arm64, 2026-11-24; `/main` is 4,097 instructions in a module of 59 functions).
  - notes: deferred: TODO-5536 decides whether the bytecode size is worth the lowering work; the counts are in docs/todo_log.md under TODO-5536.

- [ ] TODO-6254: `field_count` and `field_capacity` of the collections records are expanded at every call site
  - owner: ai
  - status: deferred
  - created_at: 2026-11-24
  - phase: Native UI (docs/NativeUiPlan.md; a blocker the inline report names for the PNG reader, TODO-5536)
  - parallel_track: bytecode-inline-blockers
  - depends_on: TODO-5536
  - scope: The two accessors `/std/collections/vector/Vector/field_count` and `field_capacity` are `collections-helper` definitions, expanded inline by rule: 2 definitions, 52 call sites, 676 instructions added (13 per site, as many as a call). Lower a read of one of these fields of a collection record to the field load directly, or let them be real calls, whichever the report shows smaller.
  - acceptance:
    - the report of the PNG program shows the sum of `added` of the `collections-helper` lines falling from 676 to under 250 with the program output unchanged
    - a program-matrix case that reads `count()` and `capacity()` of a `Vector` on every backend the matrix runs
    - full release gate green
  - stop_rule: the two field accessors; no change to other `/std/collections/` helpers.
  - repro: `primec --emit=vm --dump-stage inline-report benchmarks/startup/png.prime` (release build, macOS arm64, 2026-11-24; `/main` is 4,097 instructions in a module of 59 functions).
  - notes: deferred: TODO-5536 decides whether the bytecode size is worth the lowering work; the counts are in docs/todo_log.md under TODO-5536.

- [ ] TODO-6255: A stdlib error struct result keeps its constructors inline
  - owner: ai
  - status: deferred
  - created_at: 2026-11-24
  - phase: Native UI (docs/NativeUiPlan.md; a blocker the inline report names for the PNG reader, TODO-5536)
  - parallel_track: bytecode-inline-blockers
  - depends_on: TODO-5536
  - scope: `/std/image/ImageError/invalid_operation`, `read_unsupported` and their `/ImageError` aliases, `why` and others (9 definitions, 11 call sites, 189 instructions added) return the stdlib error struct by value or a `string`, which a real call does not return (`unsupported-result`). Let a direct call return a stdlib error struct through the block of the caller, as `--emit=lib` does for a library function.
  - acceptance:
    - the report of the PNG program shows the `unsupported-result` lines as `real call` or gone, and their `added` falling from 189 to under 60
    - a test of a program that returns `ImageError` from a function and prints `why()` on the VM and on optexe
    - full release gate green
  - stop_rule: results that are a stdlib error struct; no change to the error structs themselves.
  - repro: `primec --emit=vm --dump-stage inline-report benchmarks/startup/png.prime` (release build, macOS arm64, 2026-11-24; `/main` is 4,097 instructions in a module of 59 functions).
  - notes: deferred: TODO-5536 decides whether the bytecode size is worth the lowering work; the counts are in docs/todo_log.md under TODO-5536.


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

- [ ] TODO-6153: Twelve gate tests hit the 295 s hard cap together and pass alone in under 11 s
  - owner: ai
  - status: deferred
  - created_at: 2026-11-21
  - phase: Tests (found by the release gate of TODO-6140)
  - parallel_track: flaky-tests
  - scope: One `./scripts/compile.sh --release` run (a second heavy gate was probably running on the machine; the run took 2037 s against 572 s for an earlier run of nearly the same tree) failed twelve tests, all among the first 150 to finish, with `test_watchdog: killed at the hard cap of 295 s`: `compile_run_vm_core_result_exit_codes_1_4` and `_5_8`, `compile_run_vm_outputs_ir_and_output_modes_argv_arrays_77_86_part2_49_49`, `program_matrix_tasks_10_10`, `_15_15`, `_30_30`, `_31_31`, `_32_32`, `program_matrix_text_views_1_1`, `program_matrix_element_assign_2_2`, `_4_4` and `program_matrix_element_receivers_3_3`. Rerun alone they pass in 6 to 11 s each.
  - measured (2026-11-21, ai-flaky, release build of this tree, `nice -n 10 ctest --parallel 22`, 11 cores, other agents' gates running): (1) the twelve tests share no lock, port, cache, temp directory or stdin: each writes under its own `testScratchPath` name, the compile-run and matrix helpers shell out to `./primec`/`primevm` (and `node` for Wasm cases) with no shared file, and nothing in src/, tests/ or scripts/ takes a `flock`/`lockf`/`O_EXCL` lock outside the VM, the build runner and the UI script runner. (2) All twelve started together on the machine at load 99 to 111 with swap 5.9 of 7 GB used and about 5000 free pages: they finished in 18 to 23 s each (23 s for the group), so a stalled 295 s needs a further factor of 13 that load alone did not produce in this run of the group (the full gate of this change is the second measurement). (3) The "primec: X is running" beat is only written while the process tree's CPU time advances, so the killed tests were consuming CPU, not blocked; the kill message could not say where. Not reproduced, so no cause is claimed: the evidence fits a machine-wide stall during the first minutes of that run (memory pressure and swap with a second gate and two `primec` experiments), not a property of the twelve tests.
  - note: 2026-11-23: a full gate an agent started without `caffeinate` had 40 timeouts at about 457 s, all at once; every later gate run under `caffeinate -i` was clean. This points at the machine sleeping or stalling, not at load or at the tests.
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
- [ ] TODO-6181: A Wasm `__pstext_stat` of an unknown number traps instead of the VM's fault text
  - owner: ai
  - status: deferred
  - created_at: 2026-11-22
  - phase: Backend parity (found with TODO-6150)
  - parallel_track: wasm-bytes-host
  - scope: the VM answers `__pstext_stat(9u64)` (any number above 8) with the runtime error `VM error: unknown string heap statistic: 9` and exit code 3; a Wasm module answers it with `unreachable` (`emitWasmStringHeapStat`, src/wasm_emitter/WasmEmitterMemory.cpp), so the WASI profile exits with a trap, not code 3 and the message. The stdlib only passes the constants 0 to 8, so no program reaches it today; a module built by hand or by a later stdlib change would. Print the VM's message and `proc_exit(3)` on the WASI profile (the heap's `fault` helper, src/wasm_emitter/WasmEmitterHeap.cpp, has the shape; it needs a data segment for the text and a decimal printer even when the module has no heap) and keep the trap on the browser profile.
  - acceptance:
    - an IR module that calls `__pstext_stat` with 9 ends with the VM's stderr text and exit code 3 under node on the WASI profile and with a trap on the browser profile
    - full release gate green
  - stop_rule: the unknown-statistic fault only.
  - repro: an IR module `PushI64 9; CallHost 0; ReturnI64` with host import `std/text/__pstext_stat(u64) -> u64` runs on the VM as `VM error: unknown string heap statistic: 9` and exits 3; under `--emit=wasm --wasm-profile wasi` it traps.
- [ ] TODO-6230: Wasm `PrintArgv` and `PrintArgvUnsafe` read the arguments into fixed buffers
  - owner: ai
  - status: deferred
  - created_at: 2026-11-24
  - phase: Backend parity (found with TODO-6227)
  - parallel_track: wasm-bytes-host
  - scope: `PrintArgv` reads the arguments with `args_get` into the static table of 1024 pointers and the buffer of 16 KiB (`argvPtrsBase`, `argvBufferBase`, src/wasm_emitter/WasmEmitterIo.cpp), which the VM does not have. A program with more arguments or more argument text now traps (a check before `args_get`; it used to write past the buffers and corrupt the memory that follows). `__pstext_argument` has no such limit: it takes the sizes from `args_sizes_get` and two blocks of the string heap's metadata area (TODO-6227), which a module without the string heap does not have. Give `PrintArgv` the same: take the blocks from the memory (`memory.grow` with the shadow stack no longer the last region, as `metaAlloc` does) or print the argument in pieces.
  - acceptance:
    - an IR module that calls `PrintArgv` with 2000 arguments and one with 100 arguments of 300 bytes print what the VM prints under node on the WASI profile
    - full release gate green
  - stop_rule: the static argument buffers of `PrintArgv` and `PrintArgvUnsafe`.
  - repro: an IR module `PushI32 1; PrintArgv 1; PushI32 0; ReturnI32` run under node with 2000 arguments traps (exit code 99 of the test runner) where the VM prints the second argument.

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
  - measured: 2026-11-21 (release build, macOS arm64; numbers in docs/todo_log.md under TODO-5932). `--emit=optexe` of an empty `main` takes 0.91 s, of which the front end is 0.02 s (69M instructions) and `clang++` parsing the runtime preamble's standard headers alone is 0.64 s (`-fsyntax-only`); a program of 24k generated lines (examples/apps/http_hello) takes 4.18 s, 1.29 s of it the front end (14.6G instructions) and about 2.9 s the C++ compile and link. So the back end is 98 percent of a small program (a fixed cost every translation unit pays) and about 70 percent of an http-sized one. The emitter writes one translation unit with the runtime in an unnamed namespace, passing the runtime as `Rt &rt` (src/backend/IrToOptCppRuntime.h), so a library unit could share it; a string table, host import declarations and struct layouts are per unit, and the optimizer's inlining across the library boundary is lost. The native back end writes executables directly (src/native_emitter/NativeEmitterMachO.cpp, NativeEmitterElf.cpp) and has no object writer.
  - options_to_decide: (A) cut the fixed cost first, without libraries: a precompiled header of the runtime preamble (measured: an empty `main` 0.70 s to 0.39 s), which every optexe and `exe` compile gains and which needs no library ABI. (B) optexe only: a library carries a C++ unit emitted from its own IR (roots with external linkage, names from their paths), the consumer unit declares them, a cache of the object files keyed by archive hash, compiler build id and flags; costs at least one translation unit of compile per library (0.4 s with the header above) and the loss of cross-library inlining, so it pays only for a library whose body is large. (C) native: an object writer for Mach-O and ELF plus a system linker step, also giving C callers an `.a`; large, and only after B proves out.
  - decision: 2026-11-22 (owner): option A first: TODO-6174 (precompiled header or trimmed includes), which every optexe and `exe` compile gains. B (a C++ unit per library) only if a real library measures more than 1 s of back-end compile of a typical program; C not before B. This block stays deferred until that measurement exists.

- [ ] TODO-6219: `loadBytecode` of a script linked with a library says whether that library changed
  - owner: ai
  - status: deferred
  - created_at: 2026-11-23
  - phase: Modules and libraries (split from TODO-6217: the check at load)
  - parallel_track: lib-embed
  - blocked_on: owner decision, options below
  - scope: `Script::bytecode` records the name and interface hash of each library a script was linked with (done), and the code of each library is merged into the module, so the bytecode is self-contained and cannot run stale library code. TODO-6217 asked that `loadBytecode` also "check" the records so that a script compiled against a library that changed is refused at load. `Script::loadBytecode(bytes, name)` is static and holds no library, so there is nothing to compare the records with unless the caller supplies it. The question is what the check is.
  - question: what does `loadBytecode` compare the recorded libraries with, and what is refused?
  - options:
    - A. Nothing: the records are information (`Script::libraries()`), the bytecode is self-contained, and a changed archive has no effect on a script compiled before it. Costs: nothing built; a host that wants to know whether its scripts are older than an archive compares `libraries()` itself.
    - B. `Script::loadBytecode(bytes, name, libraries)` with the archives (or their manifests) the host has: a recorded library that is missing or has another interface hash is `library ui changed since this script was compiled; rebuild` (the text of the compile-time check in `linkProgramLibraries`). Costs: a new overload in the runtime-only library, which must read an archive manifest (today only the full library does), and a host that passes none gets option A.
    - C. `ScriptEngine::loadBytecode(bytes)`, a member that checks the records against the libraries the engine was given with `addLibrary` and then calls the static function. Costs: a second entry point for loading; the runtime-only library stays as it is.
  - recommendation: A until a host asks for the check, then C: the engine already holds the archives and the compile-time check text, so the runtime-only library stays small. B only if a host without the full library needs it.
  - acceptance:
    - the option the owner picks is implemented with a positive and a negative test (the changed library is refused with a message that names it), or the leaf is closed as option A with the doc line that says the records are information
    - full release gate green
  - stop_rule: the check of the recorded libraries at load; no change to the record format of TODO-6217.
  - repro: compile a script against `tools.psa`, `saveBytecode`, rebuild `tools.psa` with a changed public function, `loadBytecode` the bytes: the script loads and runs the code it was compiled with.


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

