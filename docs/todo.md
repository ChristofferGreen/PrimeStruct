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
| TODO-6191 | A write to an element of a stdlib `Vector` changes a temporary copy silently | ready | vector-element-write-lost |
| TODO-6193 | `push` of a `string` into a `Vector<string>` or `vector<string>` is an internal lowering error | ready | vector-string-push |
| TODO-6195 | `print_line(bag.items.count())` of a `Vector` field is `print_line requires an integer/bool or string` | ready | print-field-count |
| TODO-6196 | A struct field's default initializer is not validated as a local's is | ready | field-initializer-validation |
| TODO-6199 | The tracked stdlib symbol manifests are not what the generator writes | deferred | stdlib-manifest-drift |
| TODO-6187 | Node 24 crashes on a failed WASI `path_open` of an absolute path | deferred | wasm-bytes-host |
| TODO-5806 | Sessions: a cookie and per-session state | deferred | web-sessions |
| TODO-5821 | The first toolbar of a launch does not hold up the first frame | deferred | startup-toolbar |
| TODO-5603 | The real-launch baseline, measured by a person | ready | startup |
| TODO-6117 | Native text used after its arena ended is read and written silently | ready | native-arena-stale-text |
| TODO-6118 | Native arena regions larger than the standard one are never unmapped | deferred | native-arena-large-regions |
| TODO-6119 | A native module with tasks keeps arena text in the pool | deferred | native-arena-threads |
| TODO-5536 | Small, lazily loaded bytecode for apps | deferred | bytecode-startup |
| TODO-5533 | Windows and Linux native UI backends | deferred | native-ui-platforms |
| TODO-5511 | Safe code cannot reach container storage or unsafe stdlib helpers | deferred | safety-stdlib |
| TODO-6197 | Whether a method of a `/std` struct needs `[public]`, and `[private]` outside `/std` | deferred | safety-stdlib |
| TODO-6198 | Safe code builds a `Slice` of any count over any pointer | ready | safety-slices |
| TODO-6171 | A view stays live across a lambda or a `Function` value that changes its container | ready | safety-borrows |
| TODO-6059 | Native x86_64 float to integer conversion saturates | deferred | numeric-conversions |
| TODO-6051 | Native x86_64 (ELF) division by zero is the VM's runtime fault | deferred | runtime-faults |
| TODO-6142 | The recursion depth of Linux x86_64 native and `exe` is measured | deferred | runtime-faults |
| TODO-6175 | The Wasm slot heap has a fixed capacity of 2,097,152 slots | deferred | wasm-opcode-gaps |
| TODO-6153 | Twelve gate tests hit the 295 s hard cap together and pass alone in under 11 s | deferred | flaky-tests |
| TODO-6124 | Wasm lowers pointers, `Reference`, struct values, arrays and closures | blocked | wasm-opcode-gaps |
| TODO-6127 | Wasm refuses u8 and byte memory opcodes and host calls | deferred | wasm-bytes-host |
| TODO-6177 | Wasm byte regions and the byte opcodes (`HeapAllocBytes`, `LoadU8`, `StoreU8`) | deferred | wasm-bytes-host |
| TODO-6178 | The Wasm string heap: the `__pstext_*` allocation functions and statistics 0 to 7 | deferred | wasm-bytes-host |
| TODO-6179 | The Wasm `__pstext_*` byte operations and entry arguments | deferred | wasm-bytes-host |
| TODO-6180 | Wasm formats and parses floats for `String` | deferred | wasm-bytes-host |
| TODO-6181 | A Wasm `__pstext_stat` of an unknown number traps instead of the VM's fault text | deferred | wasm-bytes-host |
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
| TODO-5933 | The embed API loads a library at run time | ready | lib-embed |
| TODO-6032 | A public function of any shape is a function of a library | ready | lib-shapes |
| TODO-6173 | A public function with a borrowed sum parameter passes the real-call shape check and is still refused as a library root | deferred | lib-shapes |
| TODO-5987 | The lowerer's include-only fragments become components | ready | lowerer-fragments |
| TODO-5973 | SoA rewrite passes share one context and one walker | ready | reuse-soa-rewrites |
| TODO-5976 | The semantic product publishes parsed types, and the lowerer stops re-parsing type text | deferred | reuse-type-model |
| TODO-5977 | Semantics diagnostics go through one builder | deferred | reuse-diagnostic-builder |
| TODO-5979 | `--collect-diagnostics` reports no false intra-body call diagnostics | ready | collect-diagnostics-false-positives |
| TODO-6103 | Slots of `map<K, V>` and `soa<T>` storage | deferred | lifecycle-collection-slots |

### Ready Now

- TODO-5603 (startup): the real-launch baseline, measured by a person
- TODO-5933 (lib-embed): `ScriptEngine::addLibrary` and a source-hash compiler build id
- TODO-6171 (safety-borrows): a lambda's capture list counts as a change of a viewed container

### Immediate Next 10

1. TODO-5603
6. TODO-5933
7. TODO-6171

### Priority Lanes

- Startup, first (docs/NativeUiPlan.md section 1, "Where a launch goes"): TODO-5603, TODO-5821
- Native PrimeScroll, direct native backend (docs/NativeUiPlan.md): TODO-6117; deferred TODO-6118, TODO-6119
- Native UI (docs/NativeUiPlan.md): TODO-5536, TODO-5533
- Effects (docs/spec/language-core.md, Effects): TODO-5688
- Build system (docs/BuildSystemPlan.md; after the startup lane): TODO-6019
- Modules and libraries (docs/ModulesAndLibrariesPlan.md; after the startup lane): TODO-5933; ready outside Ready Now TODO-6032 (decided 2026-11-22); deferred TODO-5932 (back-end code in libraries, after TODO-6174 is measured), TODO-6173
- Memory safety (docs/spec/type-system.md Memory safety): deferred TODO-5511 (tracker); ready TODO-6171 (a lambda changes a viewed container); deferred TODO-6197 (methods of `/std` structs and `[private]` outside `/std`, decision needed); ready outside Ready Now TODO-6198 (a `Slice` built over any pointer); ready outside Ready Now TODO-6196 (a field's default initializer is not validated)
- Backend parity: TODO-6124 (Wasm pointers, structs, arrays; tracker of TODO-6138, TODO-6139, TODO-6140 and TODO-6141); deferred TODO-6175 (Wasm heap capacity), TODO-6182 (Wasm file open errors), TODO-6177 (Wasm byte regions), TODO-6178 (Wasm string heap), TODO-6179 (Wasm `__pstext_*` byte operations), TODO-6180 (Wasm text formatting and parsing of floats), TODO-6059, TODO-6051, TODO-6142 (Linux recursion depths), TODO-6127 (Wasm u8, host calls), TODO-6128 (Wasm concurrency)
- Frontend and diagnostics (split from TODO-5520): TODO-6121; deferred TODO-6081, TODO-6082, TODO-6084, TODO-6085
- Fused `fma` (IR opcodes `FmaF32` and `FmaF64`, done on the VM, the C++ emitters, arm64 native and GLSL): deferred TODO-6067 (x86_64 native FMA3, needs an x86_64 machine) (Wasm exact emulation)
- macOS support: TODO-5545 (needs macOS)
- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix; VM speed ; passes ; real calls TODO-5752
- Multithreading (docs/MultithreadingPrototype.md, Threads For Real; docs/WebAppsPlan.md, Concurrency; after the startup lane): TODO-5872; deferred TODO-5806
- Code reuse (docs/CodeReusePlan.md; two at a time under rule 18): TODO-5973; deferred TODO-5976, TODO-5977
- Stdlib `Vector` elements (found with the read-only element rule; outside Ready Now): TODO-6191, TODO-6193 (`push` of a `string` element), TODO-6195 (`print_line` of the count of a `Vector` field)
- Lifecycle bugs found with TODO-6098 (outside Ready Now): deferred TODO-6103 (`map` and `soa` slots)

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10: the leaves that fix leaks and lifecycle (TODO-5853) first. Ready outside Ready Now (the cap is eight): TODO-6121 (debugger columns of rewritten expressions), TODO-6019 (a person measures startup), TODO-6032 (library function shapes), TODO-6191 and TODO-6193 (elements of a stdlib `Vector`: element writes that are lost, `push` of a `string` element), TODO-6196 (a struct field's default initializer is not validated), TODO-6198 (a `Slice` built over any pointer), TODO-6195 (`print_line` of the count of a `Vector` field); the Wasm leaf found by the Wasm matrix config that is still ready: TODO-6141 (the heap; TODO-6124 tracks it). Decided 2026-11-20 by the owner; each block carries its `decision:` line. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

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
  - scope: Tracker. Done (2026-11-21): the `Vector` and ring buffer slot helpers are `[unsafe_api]` (TODO-6156) and the SoA slot helpers (TODO-6160). Field privacy is enforced (TODO-6161) and the `Vector`, `Map` and `SoaColumn` fields are private (TODO-6162). Namespace-level `/std` definitions without `[public]` are private to their namespace (TODO-6163). Open: methods of `/std` structs without a visibility transform (TODO-6197, decision needed) and slices built over any pointer (TODO-6198).
  - acceptance:
    - TODO-6161, TODO-6162 and TODO-6163 are done; each repro (borrowSlot then reserve, takeSlot then clear, `w.data = v.data`, writing fieldCount, brace-constructing a Vector around `v.data`) is rejected outside `[unsafe]`; stdlib and corpus still compile
    - full release gate green
  - stop_rule: tracker; the work is in the leaves.
  - notes: deferred: queued behind the Ready Now cap; the open decision is written into TODO-6197.



- [ ] TODO-6197: Whether a method of a `/std` struct needs `[public]`, and `[private]` outside `/std`
  - owner: ai
  - status: deferred
  - created_at: 2026-11-23
  - phase: Correctness audit 2026-10 (split from TODO-6163)
  - parallel_track: safety-stdlib
  - scope: TODO-6163 made a namespace-level `/std` definition without `[public]`, and a method marked `[private]`, private to its namespace (src/semantics/SemanticsPrivateDefinitionAccess.{h,cpp}). Two questions were left out because the decision of TODO-6163 does not answer them. (1) A method of a `/std` struct without a visibility transform. The decision's words (no `[public]` is private) make it private, but almost no method of the library carries `[public]`: a survey of the whole test suite and the examples found 304 distinct methods of `/std` structs called from outside their namespace (58,838 calls): the API of `String`, `StringView`, the slices, `Atomic`, `Channel`, the HTTP and UI types. Fields are public by default and private only when marked (docs/spec/type-system.md, Field visibility), so today a method follows its struct. The invariants this leaves open are methods such as `Vector.set_field_count`, `set_field_capacity`, `field_count` and `owns_storage`, which a program can call (`v.set_field_count(1000i32)` panics only when the value exceeds the capacity). (2) A definition marked `[private]` outside `/std` (`namespace util { [private] helper() { ... } }` called as `/util/helper()` from `main`) is still callable; the field rule applies to every struct, and the decision names `/std` only.
  - options_to_decide: (1a) members follow the field rule: a method is callable where its struct is unless it is marked `[private]`; the internal methods of the library (`Vector.set_field_count`, `set_field_capacity`, `field_count`, `field_capacity`, `owns_storage`, and the like in `MapValue`, `SoaColumn`, `String`) are marked `[private]` and their callers in other library namespaces (ring_buffer.prime, soa_storage) are rerouted or the methods stay open to them through a public unsafe helper. Small sweep, one rule for fields and methods; an author who forgets `[private]` leaves an internal method open. (1b) a method needs `[public]` like a namespace-level definition: about 300 library methods and every method of a struct a test defines under `/std` get `[public]`; closed by default, at the price of a mark on every API method and a spec change for fields to match. Recommendation: 1a. (2a) leave `[private]` outside `/std` as an import-only mark; (2b) enforce an explicit `[private]` everywhere with the same rule (the default of no transform stays callable outside `/std`), so that the word means one thing on a field, a method and a definition; the cost is the programs and tests that call a `[private]` definition across namespaces, not yet counted. Recommendation: 2b.
  - acceptance:
    - the decisions are recorded here; the chosen rule is in docs/spec/backend-type-support.md (Exports) and enforced, with a negative example for each; the library, examples and tests compile
    - `v.set_field_count(1000i32)` from a program is `is private`
    - full release gate green
  - stop_rule: call-site visibility of methods and of `[private]` definitions; no new visibility keywords.
  - repro: `import /std/collections/*  [effects(heap_alloc), return<int>] main() { [Vector<i32> mut] v{vector<i32>(1i32)}  v.set_field_count(0i32)  return(0i32) }` compiles.

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

- [ ] TODO-6171: A view stays live across a lambda or a `Function` value that changes its container
  - owner: ai
  - status: ready
  - created_at: 2026-11-21
  - phase: Correctness audit 2026-10 (found with TODO-6170)
  - parallel_track: safety-borrows
  - scope: the callee-effect analysis (src/semantics/ReferenceParameterEffects.cpp) reads definition bodies and the definitions a call resolves to. Two ways of changing a viewed container escape it. (1) A lambda: its body is hoisted into a definition of its own (`/reset_via_lambda__lambda0/call`) that holds the captures, so a plain `Reference` parameter captured by a lambda that replaces or pushes to what it reaches is not counted when the callee calls the lambda. (2) A callee reached only through an unresolved call: a call of a `Function` value (`f(bag)` where `f` is a `Function<void, Reference<Bag>>`) has no definition, so passing the parameter to it is not counted. Counting every pass to such a call as a change would reject read-only callbacks (a false positive on stdlib and UI code), so it needs either the capture list of the hoisted lambda (case 1, sound) or a decision for case 2.
  - decision: 2026-11-22 (owner): case 1 (a lambda): the hoisted lambda definition is counted through its capture list, so a lambda that replaces or pushes to a captured plain `Reference` parameter is a change. Case 2 (a `Function` value call) is option A: left open and documented as a limit of the view check (it cannot see through type erasure); no `Function<..., mut>` language change.
  - acceptance:
    - case 1: a lambda in the callee that replaces a container field of a captured plain `Reference` parameter, or calls `push` on it, counts as a change; a lambda that only reads does not; negative example with line and column; the stdlib, examples, UI and web code compile
    - case 2 decided and, if B or C, enforced with a negative example
    - full release gate green
  - stop_rule: lambdas and `Function` values in the callee-effect analysis; no new borrow kinds.
  - repro: (1) `import /std/collections/*  [struct] Bag() { [vector<i32> mut] items{vector<i32>()} } [effects(heap_alloc), return<void>] reset_via_lambda([Reference<Bag>] bag) { [auto] f{[bag]() { bag.items = vector<i32>(7i32) }} f() } [effects(heap_alloc), return<int>] main() { [Bag mut] bag{Bag{}} bag.items.push(1i32) [vector<i32>] view{bag.items} reset_via_lambda(location(bag)) return(view.count()) }` compiles. (2) the same with `reset_via_function([Reference<Bag>] bag, [Function<void, Reference<Bag>>] f) { f(bag) }` called with `Function<void, Reference<Bag>>{swap_items}` where `swap_items([Reference<Bag> mut] bag)` assigns `bag.items`, compiles.

- [ ] TODO-6173: A public function with a borrowed sum parameter passes the real-call shape check and is still refused as a library root
  - owner: ai
  - status: deferred
  - created_at: 2026-11-21
  - phase: Modules and libraries (found with TODO-6032)
  - parallel_track: lib-shapes
  - scope: `[sum] Pick { [i32] number [bool] flag } [public return<i32>] use_pick([Pick] value) { return(1i32) }` gives `public function /use_pick cannot be compiled into a library: its result, receiver or body shape is not one a real call takes`: `isAddressAbiParameter` accepts a borrowed sum parameter and `addressAbiShapeBlocker` (src/ir_lowerer/IrLowererRecursionAnalysis.cpp) finds no blocker, but the definition is not in `realCallReservationIndex` (src/ir_lowerer/IrLowererLowerSetupStage.cpp), so `realCallBlockerReason` has nothing to name. Find which rule keeps it out of `computeRealCallEligibleDefinitionPaths`, then either take the shape (and test it against the whole-program build) or make the blocker report it.
  - decision: 2026-11-22 (owner): follows the order of TODO-6032 (done with or right after its first slice).
  - acceptance:
    - the function is a function of the archive and gives the same result as the whole-program build when linked, or the refusal names the rule
    - negative or positive example with the program above
    - full release gate green
  - stop_rule: the sum parameter of a library root; no new ABI shape.
  - repro: the program in scope, `primec --emit=lib sum.prime -o sum.psa`.

- [~] TODO-6191: A write to an element of a stdlib `Vector` changes a temporary copy silently
  - owner: ai
  - status: ready
  - claimed_by: agent-ab69b87dc26ecd9b8 (worktree-agent-ab69b87dc26ecd9b8), 2026-11-23
  - created_at: 2026-11-23
  - phase: Collections (stdlib `Vector`; found with the read-only element rule)
  - parallel_track: vector-element-write-lost
  - scope: `/std/collections/vector/at` returns `T` by value, so `values[0i32].n = 7i32` and `values[0i32].bump()` on a `[Vector<Item> mut] values` compile and change a copy: the element keeps its old value. A builtin `vector<Item>` changes the element in place. Repro (`primec --emit=vm -I stdlib`): `import /std/collections/*  [struct] Item() { [i32 mut] n{0i32}  [mut] bump() { this.n = this.n + 1i32 } }  [effects(heap_alloc, io_out) return<int>] main() { [Vector<Item> mut] local{/std/collections/vector/vector<Item>()}  local.push(Item{})  local[0i32].n = 7i32  local[0i32].bump()  [i32] c{local[0i32].n}  print_line(c)  return(0i32) }` prints 0; with `vector<Item>` it prints 8. The read-only roots rule already rejects both writes when the root is read-only (docs/spec/value-lifecycle.md), so only the `mut` root is silent.
  - decision: (a), decided by the user on 2026-11-23: the element of a stdlib `Vector` is a place, as for the builtin `vector`; `values[i].n = x` and `values[i].bump()` write the element in place. Option (b), a diagnostic for a write to a temporary call result such as `make_bag().items[0i32].bump()`, was not chosen for `Vector` elements and is not part of this leaf.
  - acceptance:
    - the program above prints 8 on the VM and optexe, with tests
    - full release gate green
  - stop_rule: element writes of a stdlib `Vector`; no change to the builtin `vector`.
  - repro: the program in scope.

- [ ] TODO-6193: `push` of a `string` into a `Vector<string>` or `vector<string>` is an internal lowering error
  - owner: ai
  - status: ready
  - created_at: 2026-11-23
  - phase: Collections (stdlib `Vector`; found with the field element read)
  - parallel_track: vector-string-push
  - scope: `names.push("hello"utf8)` on a `[Vector<string> mut] names{/std/collections/vector/vector<string>()}` (and on a `[vector<string> mut] names{vector<string>()}`, and with a `[string]` local as the argument) passes semantics and fails in the lowerer: `VM lowering error: missing semantic-product method-call target: push [PSC2001]`. The direct form `push(names, "hello"utf8)` is `Semantic error: unknown call target: /std/collections/vector/push` without a position. `vectorPush<string>(names, "hello"utf8)` compiles and `print_line(names[0i32])` prints `hello`, so the element type is supported and only the `push` surface member is not resolved for it. Likely the `[T move] value` parameter of `/std/collections/vector/push` against a `string` argument.
  - acceptance:
    - the method and the direct form compile and the program prints `hello` on the VM and optexe, or both are one semantic diagnostic with file, line and column that says a `string` cannot be pushed and why, with a test
    - full release gate green
  - stop_rule: `push` with a `string` element; other element types are untouched.
  - repro: `import /std/collections/*  [effects(heap_alloc, io_out) return<int>] main() { [Vector<string> mut] names{/std/collections/vector/vector<string>()}  names.push("hello"utf8)  print_line(names[0i32])  return(0i32) }` with `primec --emit=vm -I stdlib`.

- [ ] TODO-6195: `print_line(bag.items.count())` of a `Vector` field is `print_line requires an integer/bool or string`
  - owner: ai
  - status: ready
  - created_at: 2026-11-23
  - phase: Frontend and diagnostics (found with the field element read)
  - parallel_track: print-field-count
  - scope: With `[struct] Bag() { [Vector<Item> mut] items{/std/collections/vector/vector<Item>()} }` and `[Bag mut] bag{Bag{}}`, `print_line(bag.items.count())` is `Semantic error: print_line requires an integer/bool or string literal/binding argument`, while `[i32] c{bag.items.count()}  print_line(c)` compiles and prints 0, and `print_line(local.count())` of a local `Vector<Item>` compiles. The print argument check does not type a `count()` whose receiver is a field holding the stdlib `Vector` record.
  - acceptance:
    - the call compiles and prints the count on the VM and optexe, for a field and a nested field, with a test
    - full release gate green
  - stop_rule: the type of a method call on a `Vector` field as a print argument.
  - repro: `import /std/collections/*  [struct] Item() { [i32 mut] n{0i32} }  [struct] Bag() { [Vector<Item> mut] items{/std/collections/vector/vector<Item>()} }  [effects(heap_alloc, io_out) return<int>] main() { [Bag mut] bag{Bag{}}  print_line(bag.items.count())  return(0i32) }` with `primec --emit=vm -I stdlib`.

- [ ] TODO-6196: A struct field's default initializer is not validated as a local's is
  - owner: ai
  - status: ready
  - created_at: 2026-11-23
  - phase: Frontend and diagnostics (brace construction; found with the empty brace `Vector`)
  - parallel_track: field-initializer-validation
  - scope: A brace constructor in a struct field's default initializer skips the checks the same expression gets as a local's initializer. (1) Missing entries: `[struct] P() { [i32] x [i32] y{2i32} }  [struct] Bag() { [P] p{P{}} }` compiles and `bag.p.x` reads 0, while `[P] p{P{}}` in `main` is `argument count mismatch for /P` (docs/spec/literals-and-composite-construction.md, Defaults & validation: a field without an initializer needs an entry or a `Create()` helper). (2) Private fields: `[struct] Bag() { [Vector<i32>] v{Vector<i32>{1i32}} }` compiles, `bag.v[0i32]` then reads storage that was never initialized (prints 0), and the program ends with `array index out of bounds` (exit 3) when the vector is destroyed (a count of 1 over a capacity of 0); the local form is `private field is not accessible: /std/collections/vector/Vector/fieldCount`. This is a way around the storage privacy of docs/spec/type-system.md (Memory safety): `Vector<i32>{5i32, 5i32}` as a field initializer builds a vector that claims five elements it does not have.
  - acceptance:
    - both programs are the diagnostics of the local form, at the field initializer, with file, line and column; a semantics test for each
    - the stdlib, examples and tests that relied on the gap are fixed in the same change
    - full release gate green
  - stop_rule: the validation of a brace constructor in a field's default initializer; no change to what a local accepts.
  - repro: `import /std/collections/*  [struct] Bag() { [Vector<i32>] v{Vector<i32>{1i32}} }  [effects(heap_alloc, io_out) return<int>] main() { [Bag] b{Bag{}}  print_line(b.v[0i32])  return(0i32) }` with `primec --emit=vm -I stdlib` compiles, prints 0 and ends with `array index out of bounds` (exit 3).

- [ ] TODO-6199: The tracked stdlib symbol manifests are not what the generator writes
  - owner: ai
  - status: deferred
  - created_at: 2026-11-23
  - phase: Modules and libraries (found with TODO-6163)
  - parallel_track: stdlib-manifest-drift
  - scope: `build-release/generate_stdlib_manifest stdlib/std/image/image.prime /std/image <out>` writes 130 symbols; the tracked stdlib/std/image/image.psmeta has 126: the generator now also writes the namespace definitions (`/std`, `/std/image`, ...). stdlib/std/gfx/experimental.psmeta was the same until it was regenerated for TODO-6163. Nothing fails, because a compile only compares the hash of the entries a manifest has, but no test tells a stale manifest from a fresh one, so the next edit of image.prime changes the file in ways unrelated to the edit.
  - acceptance:
    - a ctest entry regenerates each tracked `.psmeta` and fails when it differs from the tracked file; image.psmeta is regenerated
    - full release gate green
  - stop_rule: the freshness of the tracked manifests; no change to the manifest format.
  - repro: the command in scope, then `diff` against the tracked file.

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
  - done: `scripts/test_watchdog.py` now prints, with every kill, the last eight beats with times, the beat count, the load average, swap use, the free-memory level and every process of the killed group (state, elapsed, CPU time); tests/scripts/test_test_watchdog.py covers it and docs/TestWatchdog.md describes it. A recurrence names the cause in its own log (swap near full and processes in `U`: memory pressure; `S` with no CPU: a stalled child; `R` with CPU near elapsed: a runaway).
  - acceptance:
    - the next gate that hits a hard cap or a silence kill records the new message in `docs/failing_tests.md`, and the cause it names is fixed here (a shared cost paid once, or the test made cheaper); no timeout is raised and no test is retried
    - or twenty consecutive release gates pass with no kill, then this leaf is closed as not reproducible
  - stop_rule: the tests and shared costs the message names; no timeout is raised and no test is retried.
  - repro: run `./scripts/compile.sh --release` while a second one runs in another worktree, then read `build-release/Testing/Temporary/LastTestsFailed.log` and the kill message in `LastTest.log`.

- [ ] TODO-6177: Wasm byte regions and the byte opcodes (`HeapAllocBytes`, `LoadU8`, `StoreU8`)
  - owner: ai
  - status: deferred
  - created_at: 2026-11-22
  - phase: Backend parity (split from TODO-6150)
  - parallel_track: wasm-bytes-host
  - scope: `String` keeps its bytes in byte regions (`HeapAllocBytes`, byte addresses of `VmHeapCore`: include/primec/runtime/VmHeapCoreBody.inc `allocateBytesOwn`, `releaseBytesOwn`, `reallocateBytesOwn`, `byteLive_`) read and written by `LoadU8`, `StoreU8` and `ZextU8`; Wasm refuses all of them (`unsupported opcode for wasm target: HeapAllocBytes (byte regions, needed by `String` and byte buffers)`). 140 matrix programs are refused for a host call because they use `String`, and every one needs this first. Port the byte regions the way the slot heap was ported (hand-assembled helpers over regions of the linear memory, the VM's byte addresses, `HeapFree` and `HeapRealloc` of a byte address, the VM's fault texts), then lower `LoadU8`, `StoreU8` and `ZextU8` (the u8 conversion programs of TODO-6127 list `wasm-wasi` as well). Decide the capacity with TODO-6175 (the byte regions can share its growth).
  - acceptance:
    - a program that allocates a byte buffer, writes and reads bytes and frees it runs on Wasm like the VM (stdout, exit code, fault text); IR tests compare with the VM under node
    - `conv_integer_to_u8` and `conv_float_to_u8` list `wasm-wasi`
    - full release gate green
  - stop_rule: byte regions and the byte opcodes; the string heap functions are TODO-6178 and TODO-6179.
  - repro: `PRIMESTRUCT_MATRIX_WASM_COMPILE_ONLY=1 ./PrimeStruct_program_matrix_tests 2>&1 | grep -c "the string heap"` from the release build directory.
- [ ] TODO-6178: The Wasm string heap: the `__pstext_*` allocation functions and statistics 0 to 7
  - owner: ai
  - status: deferred
  - created_at: 2026-11-22
  - phase: Backend parity (split from TODO-6150)
  - parallel_track: wasm-bytes-host
  - scope: `String` allocates its blocks with `__pstext_alloc`, `__pstext_try_alloc`, `__pstext_free`, `__pstext_capacity`, `__pstext_length`, `__pstext_usable`, `__pstext_set_limit`, `__pstext_arena_begin` and `__pstext_arena_end`, which the VM answers with `VmTextHeap` (include/primec/runtime/VmTextHeapBody.inc: size classes, chunks, large blocks, arenas, the byte limit). A Wasm module refuses them (`host calls are only supported by the vm target, ...; the string heap (`String`) is not available on wasm`) and answers statistics 0 to 7 of `__pstext_stat` as 0 because it has no text blocks. Port `VmTextHeap` over the byte regions of TODO-6177 so that block addresses, capacities, reuse, the limit fault and the statistics (`stringHeapChunks()`, `stringHeapBytesInUse()`, `stringHeapBytesFree()`, `stringHeapLargeBlocks()`, `stringHeapLargeBytes()`, `stringHeapAllocations()`, the arena depth and arena bytes) equal the VM's, and replace the constant 0 of statistics 0 to 7 in `emitWasmStringHeapStat` (src/wasm_emitter/WasmEmitterMemory.cpp).
  - acceptance:
    - a program that builds and drops `String` values prints the same statistics as the VM under node; the string arena programs (`string_arenas_*`) list `wasm-wasi`
    - full release gate green
  - stop_rule: the allocation functions and statistics; the byte operations are TODO-6179 and float text is TODO-6180. Blocked by TODO-6177.
  - repro: `PRIMESTRUCT_MATRIX_WASM_COMPILE_ONLY=1 ./PrimeStruct_program_matrix_tests 2>&1 | grep -c "the string heap"` from the release build directory.
- [ ] TODO-6179: The Wasm `__pstext_*` byte operations and entry arguments
  - owner: ai
  - status: deferred
  - created_at: 2026-11-22
  - phase: Backend parity (split from TODO-6150)
  - parallel_track: wasm-bytes-host
  - scope: `String` copies, fills, compares and searches bytes with `__pstext_move` (overlap allowed), `__pstext_fill`, `__pstext_compare` (-1, 0 or 1) and `__pstext_find`, reads a string literal with `__pstext_literal` (the address of the literal's bytes: the module already keeps the string table in a data segment, so the address is known at emit time) and `argumentText(index)` reads `__pstext_argument` (WASI `args_get`, the argument copied into a fresh block). Each has one range check per call in the VM (`VmHeapCore::byteSpan`) and the same fault texts. Lower them as helper functions over the byte regions of TODO-6177.
  - acceptance:
    - the `String` programs of the matrix that only need these (concatenation, slicing, comparison, search, literals, arguments) run on Wasm like the VM and list `wasm-wasi`
    - full release gate green
  - stop_rule: the byte operations, literals and arguments; allocation is TODO-6178 and float text is TODO-6180. Blocked by TODO-6177 and TODO-6178.
  - repro: `PRIMESTRUCT_MATRIX_WASM_COMPILE_ONLY=1 ./PrimeStruct_program_matrix_tests 2>&1 | grep -c "the string heap"` from the release build directory.
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
  - stop_rule: float text only. Blocked by TODO-6177 and TODO-6178.
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
  - decision: 2026-11-22 (owner): option (b): build the offset map only when a debugger or an `--emit` mode that writes the source map asks for it; no cost on an ordinary compile. Startup instructions of a program that compiles are still measured and must not be worse.
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
  - measured: 2026-11-21 (release build, macOS arm64; numbers in docs/todo_log.md under TODO-5932). `--emit=optexe` of an empty `main` takes 0.91 s, of which the front end is 0.02 s (69M instructions) and `clang++` parsing the runtime preamble's standard headers alone is 0.64 s (`-fsyntax-only`); a program of 24k generated lines (examples/apps/http_hello) takes 4.18 s, 1.29 s of it the front end (14.6G instructions) and about 2.9 s the C++ compile and link. So the back end is 98 percent of a small program (a fixed cost every translation unit pays) and about 70 percent of an http-sized one. The emitter writes one translation unit with the runtime in an unnamed namespace, passing the runtime as `Rt &rt` (src/backend/IrToOptCppRuntime.h), so a library unit could share it; a string table, host import declarations and struct layouts are per unit, and the optimizer's inlining across the library boundary is lost. The native back end writes executables directly (src/native_emitter/NativeEmitterMachO.cpp, NativeEmitterElf.cpp) and has no object writer.
  - options_to_decide: (A) cut the fixed cost first, without libraries: a precompiled header of the runtime preamble (measured: an empty `main` 0.70 s to 0.39 s), which every optexe and `exe` compile gains and which needs no library ABI. (B) optexe only: a library carries a C++ unit emitted from its own IR (roots with external linkage, names from their paths), the consumer unit declares them, a cache of the object files keyed by archive hash, compiler build id and flags; costs at least one translation unit of compile per library (0.4 s with the header above) and the loss of cross-library inlining, so it pays only for a library whose body is large. (C) native: an object writer for Mach-O and ELF plus a system linker step, also giving C callers an `.a`; large, and only after B proves out.
  - decision: 2026-11-22 (owner): option A first: TODO-6174 (precompiled header or trimmed includes), which every optexe and `exe` compile gains. B (a C++ unit per library) only if a real library measures more than 1 s of back-end compile of a typical program; C not before B. This block stays deferred until that measurement exists.

- [ ] TODO-5933: The embed API loads a library at run time
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, section 14)
  - parallel_track: lib-embed
  - depends_on: TODO-5925
  - scope: The VM runs IR, so `Script` (include/primec/embed/Script.h) could load a `.psa` and link it into a script it compiles, which would let a PrimeScroll script use a precompiled library. The embed library must stay free of process spawning (scripts/check_embed_no_process.sh) and the link runs in-process. Needs a decision on how a script names the libraries it wants and on the startup cost of loading one; scope it when TODO-5925 has landed.
  - acceptance:
    - to be written when scoped: a script that uses a library function runs through the embed API with the library loaded at run time, and a script without libraries starts exactly as before
  - stop_rule: do not start before it is scoped.
  - notes: deferred: not committed; follows if the archive proves out.
  - measured: 2026-11-21 (numbers in docs/todo_log.md under TODO-5933). The embed engine already compiles through `runCompilePipelineResult` and `prepareIrModule` with an `Options` (src/embed/ScriptEngine.cpp, `compileEntry`), and `Options::libraryPaths` loads and links archives there (src/ir/IrPreparation.cpp, `linkProgramLibraries`), without a process or `dlopen` (scripts/check_embed_no_process.sh stays valid); so the first slice is small: `ScriptEngine::addLibrary(path)` that fills `options.libraryPaths`, with the script writing `import</root>`. A script without libraries pays nothing (`libraryPaths` empty returns at once, src/pipeline/LibraryLink.cpp); with one, the compiler build id is computed once per process (5M instructions, TODO-6093), plus the archive read and a link of about 2 ms. The blocker is the build id: `currentCompilerBuildId` is the running executable's linker UUID (src/pipeline/LibraryLink.cpp), and an embed host (PrimeScroll, a test binary) is a different executable from `primec`, so every archive `primec --emit=lib` wrote is refused with a build id mismatch. Existing coverage to reuse: the embed script tests (`PrimeStruct_embed_script_engine`) and the 23-program library differential corpus (tests/unit/compile_run, emit_lib and library link cases).
  - options_to_decide: build id: (1) the id names the compiler sources, not the executable: CMake hashes the inputs of the front end, IR and lowering into a generated header both `primec` and the embed library carry, so one source tree agrees with itself; cost is a build-time hash, and a stale id is impossible because it is derived from the content. (2) the host passes an id: `ScriptEngine::setLibraryCompilerId`, defaulting to its own UUID; fragile, the host must learn `primec`'s. (3) accept another binary's archive when the IR schema version and a hand-bumped ABI constant match; cheap, but a forgotten bump runs wrong code silently. How a script names libraries: (a) the host: `engine.addLibrary(path)`, the script writes `import</ui>`; the host decides which code a script reaches, as it does for the effects ceiling (`setAllowEffects`). (b) the script names a file path; rejected, a script could read any file. (c) `addLibraryPath(dir)` resolving `import</name>` to `dir/name.psa`; a convenience on top of (a).
  - decision: 2026-11-22 (owner): option (1): the compiler build id is a hash of the compiler sources (the inputs of the front end, IR and lowering) generated into a header that both `primec` and the embed library carry, so one source tree agrees with itself; naming (a): the host names libraries through `ScriptEngine::addLibrary(path)` and the script writes `import</root>`; a script never names a file path. Slice 1: `ScriptEngine::addLibrary` plus a test that builds a library with the host compiler (`buildLibraryArchiveContent`) and one built by `primec`, runs a script that calls it, and checks that a script without libraries is unchanged (instruction count of an empty script compile, no timing). Slice 2: `Script::bytecode` records the libraries so `loadBytecode` stays self-contained.

- [ ] TODO-6032: A public function of any shape is a function of a library
  - owner: ai
  - status: ready
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
  - measured: 2026-11-21 (the first refusal of `primec --emit=lib` on each of the 59 stdlib files; numbers in docs/todo_log.md under TODO-6032). 28 build, 13 fail before the shape check (the file is not self-contained: `unknown return type: Vec2` in math/matrix, a missing import, an `on_error` handler type, `unsupported binding type`), and 18 name a shape. Of those 18: 9 a result the address ABI does not return (4 a sum, `Result` or packed error value: collections/errors, dir, file/errors, glob; 2 a `string`: http/server, web/fragment_script; 2 a `Pointer<i64>`: sync, text/shared; 1 a `vector<i32>`: scene), 4 a `string` parameter used beyond `count()` (build_step, collections/equality, file, text/string), 4 a definition under the canonical collections root `/std/collections/` (buffer_checked, buffer_unchecked, ring_buffer, slice; the rule is by path, so a scalar-only helper such as `ringBufferWrap` is refused too), 1 a body that ends in a generated tail value (ui/events). An argument pack blocks none of them.
  - options_to_decide: (1) `string` and `Pointer<T>` results are one u64 slot: take them as slot results in the address ABI (4 of the 9 result cases; a `Reference<T>` result stays refused, it would dangle). (2) sum, `Result` and packed error results go through the struct-result path (a heap block the callee fills): needs TODO-6173 first, since a borrowed sum parameter passes the shape check and is still refused. (3) a `vector<T>` result with the owned-local rule a spawned call already uses (`vectorResultReturnsOwnedLocals`, the task shape of a spawned call). (4) a `string` parameter used beyond `count()`: the callee reads a merged-table string by a computed index (`LoadStringByteDynamic`, VM only), so lifting the rule breaks optexe and native; keep refused until those targets have it. (5) the collections-root rule: narrow it from the path to the definitions that take or return a collection record, so scalar helpers become roots; this changes which stdlib helpers are real calls in every program, so it needs the benchmarks of the lazy-import plan and an instruction-count check, or a flag limited to library roots. (6) an argument pack: a wrapper that takes a `vector<T>`, the call site packing; lowest value, no stdlib file needs it.
  - decision: 2026-11-22 (owner): order: (1) `string` and `Pointer<T>` results as slot results, then TODO-6173 plus (2) sum results, then (3) `vector<T>` results; (5) the `/std/collections` root rule is narrowed only for library roots; (4) and (6) later. Slice (1) is the first leaf to cut.

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

- [ ] TODO-6175: The Wasm slot heap has a fixed capacity of 2,097,152 slots
  - owner: ai
  - status: deferred
  - created_at: 2026-11-22
  - phase: Backend parity (found with TODO-6149)
  - parallel_track: wasm-opcode-gaps
  - scope: the Wasm heap (src/wasm_emitter/WasmEmitterHeap.cpp) keeps its values, states, allocation sizes and free runs in regions of the linear memory below the shadow stack, sized for `WasmHeapSlots` (2^21) slots, so an allocation of more than that (a `vector<i64>` of 2.1 million elements, or the sum of live blocks) ends with `VM heap allocation overflow` and exit 3 where the VM keeps growing (it faults only past 2^37 slots). The shadow stack owns the memory above its base and grows with `memory.grow`, so a heap that grows needs a second memory, a move of the heap regions when it outgrows them (the helpers address by index, so a move is a `memory.copy`), or a shared grower. Decide by measuring the memory a module has to declare up front (about 36 MiB virtual today).
  - measured: 2026-11-22 (node 24.2, macOS arm64): the regions of the heap are 36 MiB of the module's declared memory (values 16 MiB, states 4 MiB, counts 8 MiB, free runs 8 MiB: 8 + 2 + 4 bytes per slot plus 4 for the free-run table; 576 pages), so the capacity costs 18 bytes of declared memory per slot. A `WebAssembly.Memory` of 576, 4,608 (288 MiB) and 36,864 pages (2.25 GiB) was created in 0.4 to 3 ms and grew the process by 0.5 MiB or less when untouched, so V8 reserves the declared size lazily; a host that commits the declared memory at instantiation (a 32-bit or memory-limited embedder) would not. The shadow stack is the last region and grows in place with `memory.grow`; nothing else can follow it.
  - options: (A) Relocate: keep the four heap regions' base addresses in mutable globals (the helpers address by index, so a move is four `memory.copy` calls) and move a region to the end of the memory with `memory.grow` when its slot count outgrows it; the shadow stack's base becomes a mutable global too (its numbering of local addresses is virtual, so a move is one `memory.copy` and no address changes), because both grow and only one can be last. No limit but the 4 GiB of a 32-bit memory (about 2^27 slots at 18 bytes each); cost: every heap access adds a global read, and `slotAddress`, the frame prologue and the growth helper change; about 40 access sites in `WasmEmitterHeap.cpp`. (B) Multi-memory: one extra linear memory for each region (values, states, counts, runs), each grown by `memory.grow <index>` with no relocation and no change to the shadow stack; a memory index on every heap load, store and `memory.copy`. The module then needs the multi-memory feature (node 22 and later, Chrome 120, Firefox 125, Safari 18.4, wasmtime), which the browser profile did not need before. (C) Raise the fixed capacity (2^24 slots declares 288 MiB, 2^26 slots 1.1 GiB): no code change, still a limit, and a cost on hosts that commit declared memory. The decision is the product's: whether `--wasm-profile browser` may require multi-memory (B), or pays a larger rewrite for the MVP feature set (A). Recommendation: A, since a limit that the VM does not have and a feature gate on the browser profile are both new behavior differences, and A has neither.
  - acceptance:
    - a program that holds more than 2,097,152 live slots runs on Wasm like the VM, or the limit is raised with a measured reason; the overflow fault stays for what the host refuses
    - full release gate green
  - stop_rule: the heap capacity of Wasm only.
  - repro: an IR module `PushI64 2097153; HeapAlloc; Pop; ReturnVoid` runs on the VM and ends with `VM error: VM heap allocation overflow` under `--emit=wasm --wasm-profile wasi`.

