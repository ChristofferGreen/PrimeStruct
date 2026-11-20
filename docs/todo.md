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
| TODO-5956 | Native string arenas and the remaining string heap statistics | ready | native-backend-strings |
| TODO-5536 | Small, lazily loaded bytecode for apps | deferred | bytecode-startup |
| TODO-5533 | Windows and Linux native UI backends | deferred | native-ui-platforms |
| TODO-5718 | All values of a repeated parameter and decoded path parameters | ready | web-params |
| TODO-5511 | Safe code cannot reach container storage or unsafe stdlib helpers | deferred | safety-stdlib |
| TODO-5512 | Pointers and aliases count as borrows of their root | deferred | safety-borrows |
| TODO-6059 | Native x86_64 float to integer conversion saturates | deferred | numeric-conversions |
| TODO-6051 | Native x86_64 (ELF) division by zero is the VM's runtime fault | deferred | runtime-faults |
| TODO-6052 | Recursion limits of each backend are documented | ready | runtime-faults |
| TODO-6090 | The program matrix runs the programs Wasm supports on a Wasm config | ready | wasm-matrix |
| TODO-6105 | `panic` and every array runtime error compile for Wasm | ready | wasm-arrays-panic |
| TODO-6106 | `repeat` with a negative `i32` count says so at run time | ready | repeat-negative-count |
| TODO-6080 | A diagnostic column and snippet are those of the source the user wrote, not the text-filtered form | ready | diagnostics-filtered-columns |
| TODO-6081 | An unknown `break` is reported at the user's `break`, not inside stdlib/std/file/file.prime | deferred | diagnostics-break-span |
| TODO-6082 | `borrowed binding: v (root: v, sink: v)` names the sink | deferred | diagnostics-borrow-sink |
| TODO-6083 | The arity and shape of an `on_error` handler are checked in semantics | ready | diagnostics-on-error-handler |
| TODO-6084 | Internal lowering errors (`missing semantic-product ...`) say so | deferred | diagnostics-internal-errors |
| TODO-6085 | The semantics validator uses the shared demangle helper | blocked | diagnostics-demangle-semantics |
| TODO-6067 | x86_64 native emits FMA3 `vfmadd231sd` and `vfmadd231ss` | deferred | stdlib-math-fma |
| TODO-5545 | Record the macOS AppKit manual smoke checklist | deferred | native-ui-macos-manual |
| TODO-6019 | PrimeScroll built by `make.prime` starts as fast as the script's, measured by a person | ready | build-dogfood |
| TODO-5932 | Libraries carry back-end code | deferred | lib-backend |
| TODO-5933 | The embed API loads a library at run time | deferred | lib-embed |
| TODO-6032 | A public function of any shape is a function of a library | deferred | lib-shapes |
| TODO-6093 | The compiler build id of a library costs less than hashing the 22 MB `primec` | ready | lib-build-id |
| TODO-6097 | Several stdlib libraries link, and a module a linked library owns is not spliced from the stdlib source | ready | lib-stdlib-roots |
| TODO-5987 | The lowerer's include-only fragments become components | ready | lowerer-fragments |
| TODO-5973 | SoA rewrite passes share one context and one walker | ready | reuse-soa-rewrites |
| TODO-5976 | The semantic product publishes parsed types, and the lowerer stops re-parsing type text | deferred | reuse-type-model |
| TODO-5977 | Semantics diagnostics go through one builder | deferred | reuse-diagnostic-builder |
| TODO-5979 | `--collect-diagnostics` reports no false intra-body call diagnostics | ready | collect-diagnostics-false-positives |
| TODO-6104 | An element of a vector of vectors read through a `Reference<vector<vector<i64>>>` frees the element's buffer | ready | lifecycle-reference-element |
| TODO-6103 | Slots of `map<K, V>` and `soa<T>` storage | deferred | lifecycle-collection-slots |
| TODO-6105 | `*field = value` writes through a `mut` `Reference` field | ready | reference-field-write |

### Ready Now

- TODO-6097 (lib-stdlib-roots): several stdlib libraries link, and an owned module is not spliced from the stdlib source
- TODO-5804 (mt-server): read-only handlers registered with `app.view(...)` run under the read lock
- TODO-5603 (startup): the real-launch baseline, measured by a person
- TODO-6052 (runtime-faults): recursion limits of each backend are documented

### Immediate Next 10

1. TODO-6097
2. TODO-5804
4. TODO-5603
5. TODO-6052
6. TODO-5956
7. TODO-6080

### Priority Lanes

- Startup, first (docs/NativeUiPlan.md section 1, "Where a launch goes"): TODO-5603, TODO-5821
- Native PrimeScroll, direct native backend (docs/NativeUiPlan.md): TODO-5956
- Native UI (docs/NativeUiPlan.md): TODO-5536, TODO-5533
- Effects (docs/spec/language-core.md, Effects): TODO-5688
- Build system (docs/BuildSystemPlan.md; after the startup lane): TODO-6019
- Modules and libraries (docs/ModulesAndLibrariesPlan.md; after the startup lane): TODO-6097, TODO-6093; deferred TODO-5932, TODO-5933, TODO-6032
- Memory safety (docs/spec/type-system.md Memory safety): TODO-5511, TODO-5512
- Backend parity: TODO-6052, TODO-6090 (Wasm matrix config), TODO-6105 (`panic` on Wasm); deferred TODO-6059, TODO-6051
- Frontend and diagnostics (split from TODO-5520): TODO-6080; deferred TODO-6081, TODO-6082, TODO-6083, TODO-6084, TODO-6085
- Fused `fma` (IR opcodes `FmaF32` and `FmaF64`, done on the VM, the C++ emitters, arm64 native and GLSL): deferred TODO-6067 (x86_64 native FMA3, needs an x86_64 machine) (Wasm exact emulation)
- macOS support: TODO-5545 (needs macOS)
- Web apps (docs/WebAppsPlan.md; built with `--emit=optexe`): TODO-5718
- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix; VM speed ; passes ; real calls TODO-5752
- Multithreading (docs/MultithreadingPrototype.md, Threads For Real; docs/WebAppsPlan.md, Concurrency; after the startup lane): TODO-5872, TODO-5804; deferred TODO-5806
- Code reuse (docs/CodeReusePlan.md; two at a time under rule 18): TODO-5973; deferred TODO-5976, TODO-5977
- Lifecycle bugs found with TODO-6098 (outside Ready Now): ready TODO-6104 (an element read through a `Reference<vector<vector<i64>>>`); deferred TODO-6103 (`map` and `soa` slots)

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10: the leaves that fix leaks and lifecycle (TODO-5853) first. Ready outside Ready Now (the cap is eight): TODO-5956 (native string arenas and statistics), TODO-6080 (columns and snippets of the user's source), TODO-6093, TODO-5718 (repeated and decoded web parameters), TODO-6019 (a person measures startup); TODO-6090 (the Wasm matrix config) and TODO-6105 (`panic` on Wasm) are ready. Decided 2026-11-20 by the owner; each block carries its `decision:` line. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

Multithreading (docs/MultithreadingPrototype.md, Threads For Real), after the startup lane (TODO-5772 is done); not above them in Ready Now or Immediate Next 10. In this order (TODO-5792 and TODO-5835, VM tasks and their lowering, are done, and so are TODO-5793, byte-region checks without a map search, TODO-5794, `optexe` per-thread heap arenas, TODO-5795, `optexe` threads for `[spawn]`, `Atomic<i32>` and `Atomic<i64>` in `/std/sync`, TODO-5797, `Mutex<T>` with `locked(body)`, TODO-5846, `optexe` faults when every task waits, TODO-5798, bounded `Channel<T>`, TODO-5656, the atomic `SharedString` owner count, TODO-5802, `serveWorkers`, and TODO-5803, the apps on workers, measured, and the `SharedString` count inside the text block, are done): arm64 native is done: TODO-5799 (pthreads; TODO-5942, TODO-5943), TODO-5800 (per-thread heaps) and TODO-5801 (atomics, `Mutex`, `Channel`; TODO-5950, TODO-5951). TODO-5799 is done, and TODO-5893 (an unused `/std/tuple/*` import is reported as an unknown import path) is done too, and TODO-5894 (tuple destructuring and the destructured multi-wait inside a nested block are expanded; TODO-5849, each destructured multi-wait binding has its own result type, is done) is done too, and TODO-5847 (spawned calls with builtin `map<K, V>` or owned sum parameters or results run as tasks, done with TODO-5994, `wait(t).count()` of a stdlib `Vector` result waits twice, is done; TODO-5992, builtin `vector<T>` parameters and results, TODO-5993, the stdlib `Vector` and `Map` structs, TODO-5845, the same for `String`, struct and closure values, and TODO-5895, `copy` and `move` scalars and structs, are done) (TODO-5807, destructured multi-wait, TODO-5790, sendable spawn arguments, and TODO-5791, every `Function` sendable by construction, and TODO-5848, `return(wait(t))` of a `String` task result, are done). Ready: TODO-5804 (read-only handlers with `app.view` under `RwLock`); deferred: TODO-5806 (sessions); TODO-5805 (keep-alive) is done; TODO-5854 (a `Mutex` or `Atomic` handle keeps a copy of its first value; the unstored `ctor_only` field, on the construction-only field rule of TODO-5853) is done. Found with `Mutex` (TODO-5852, `wait(a, b)` of two `Task<void>`, is done) in the closures lane: TODO-5934 (a `Reference`-returning call read as a value yields its referent, found with TODO-5850), TODO-5850 (a `Reference`-returning call as a closure argument) and TODO-5851 (a `[ref]` closure written in a generic call) are done. Found with `Channel` and ready, in the Result and control flow lane: TODO-5855 (a method call on a `pick` payload of struct type), TODO-5856 (`pick` on a call result frees an owning payload), TODO-5857 (a `vector<T>` payload reads its count in a `pick` arm), TODO-5858 (`some<Vector<i32>>` of a fresh vector frees it), TODO-5859 (a `pick` expression yields a vector), TODO-5860 (moving a payload binding does not poison a later binding of the same name), TODO-5866 (a closure captures a `pick` payload binding), TODO-5877 (a field an arm of a `pick` expression yields is copied), TODO-5879 (a `pick` expression's result owns a value of its own), TODO-5880 (a payload moved out of a sum local leaves the local moved-from, and a payload of a borrowed sum cannot be moved or returned), TODO-5873 (a sum temporary passed as an argument is destroyed after the call), TODO-5874 (assigning to a `mut` sum local destroys the payload it held), TODO-5883 (assigning to a `mut` builtin vector local destroys what it held and copies a place), TODO-5876 (`return(pick(...))` of a vector and assignment from a `pick` expression), TODO-5875 (`count()` on a local declared `Vector<T>` lowers), TODO-5878 (a `[move name]` lambda capture compiles), TODO-5884 (assignment to a `mut` vector parameter and to an inferred vector-of-struct local keeps one owner), TODO-5882 (a vector of sums destroys the payloads of its elements), TODO-5885 (a vector temporary that is only read is destroyed), TODO-5889 (`a = move(b)` of a vector local takes the vector over), TODO-5890 (the arms of an assigned `pick` may open on the next line), TODO-5881 (a call takes a call to the same definition as its argument on the VM), TODO-5888 (an element of a vector temporary read as a struct or a `String`) and TODO-5887 (`remove_at` and `remove_swap` compile on a vector of sums) are done, and found with them: TODO-5886 (`push` on a local declared `Vector<T>` resolves) is done, and TODO-5978 (a `Vector<String>` leaks what `pop` and `clear` remove and faults after `remove_at`; its element reads by `at` and `[]`) is done, and found with it: TODO-5991 (a field read of a struct a call returns leaves the temporary's copy undestroyed) is done; TODO-5891 (a returned vector field or vector element of a local is a copy) and TODO-5892 (a block expression whose value is an owning local hands it over once) are done. Found with `serveWorkers` (TODO-5865, a `Function` local given positionally to a brace constructor, TODO-5864, a lambda in a generic definition naming its type parameters, and TODO-5935, a lambda parameter naming a struct of its namespace, are done): TODO-5862 (`serveWorkers` on the VM over a scripted net, done), and ready: TODO-5866 (a closure captures a `pick` payload binding). Found measuring the apps on workers (TODO-5803, done): TODO-5867 (a string heap that alternates between block sizes keeps its chunks) and TODO-5869 (1 worker as fast as `serve`; the 32 KB form reached 2.41x with 4 workers, not 2.5x) are done; TODO-5940 (a load generator that costs less per connection than the servers it measures, done: compiled, 1.4 us of user CPU per request, and the kernel's loopback setup is the ceiling; TODO-5871 found every server held to about 14,600 new connections a second by the generator and the kernel's loopback setup, which caps the form with 4 workers at 2.41x); TODO-5871 (that measurement), TODO-5872 (a byte of another thread's block is checked on a slow path, so the worker parses from a copy) and TODO-5870 (a VM program that uses tasks runs its string code on the fast kernel) are done; and in the Result and control flow lane TODO-5868 (a definition's `on_error` handler flows into nested blocks and a block-level `on_error` overrides it, so `serve(app)?` compiles in an `if` block of `main`) and TODO-5936 (a diagnostic in `main` of a program that imports `/std/http/server` reported a stdlib position, found with it) are done.

VM threads (docs/MultithreadingPrototype.md, VM Threads), after the startup lane: the VM gets a threads mode next to its deterministic scheduler. TODO-5896 (`--vm-threads`, a thread and a heap arena per task) and TODO-5897 (Atomic, Mutex and Channel on real primitives, with the deadlock fault) and TODO-5898 (reentrant and main-thread-only host functions) and TODO-5899 (`RunOptions::threads`, fault and cancel through the embed API) and TODO-5900 (the concurrent corpus repeated under `--vm-threads` and under ThreadSanitizer) and TODO-5901 (`--vm-record-sync` and `--vm-replay-sync`, the synchronization order of a threaded run replayed on the deterministic scheduler) landed; TODO-5904 (threads by default) is closed: the default stays the deterministic scheduler; TODO-6075 (a host call yields on it) is closed: it does not, a poller with workers needs `--vm-threads` with `Reentrant` host functions (docs/spec/vm-design.md, Tasks); TODO-5902 (each task on the fast kernel) landed. TODO-5903 (debug sessions run programs with tasks, the first prerequisite of docs/VmInspectorPlan.md) landed.

Build system (docs/BuildSystemPlan.md), after the startup lane: TODO-5906 (directory listing and file metadata, `/std/dir`) and TODO-5915 (incremental builds) landed. TODO-5907 (glob, `/std/glob`) and TODO-6009 (a changed glob listing re-runs a step) and TODO-5918 (staged builds, `Graph.run` and the `build_run` effect) landed; TODO-5916 (subprojects, `b.subproject`) and TODO-5917 (options, `b.option`) landed; TODO-5920 (PrimeScroll builds with `make.prime`) landed; TODO-5919 (function steps, `b.step`) landed; ready: TODO-6019 (person measures startup); and the modules and libraries lane, which splits static libraries (docs/ModulesAndLibrariesPlan.md): TODO-5922 (`primevm --link` merges two IR modules) landed; TODO-6031 (lowering marks string literal pushes, so a library's strings always relocate) landed; TODO-5923 (`--emit=interface`) landed; TODO-5924 (`--emit=lib`) landed; TODO-5925 (`--library` and the link in the pipeline) landed; TODO-5926 (generics and lifecycle helpers cross the library boundary) landed; TODO-5927 (namespace ownership) landed; TODO-5928 (`--emit-uses`) landed; TODO-5929 (symbol-level rebuilds) landed; TODO-5930 (the graph builds libraries and links them) landed; archived as low-value TODO-5931 (the stdlib as a library: measured slower, see docs/todo_finished.md), ready TODO-6097 (roots below `/std`, one per module family, so several stdlib libraries link and an owned module is not spliced from the stdlib source) and TODO-6093 (the compiler build id is the linker UUID, with the full hash as the fallback; the stdlib as a library was measured again after TODO-6092 and stays archived, numbers in docs/todo_log.md; do not run the two at once, both touch src/pipeline/LibraryLink.cpp); deferred TODO-5932 (libraries that carry back-end code), TODO-5933 (the embed API loads a library), TODO-6032 (public functions of any shape in a library).


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

- [ ] TODO-6105: `panic` and every array runtime error compile for Wasm
  - owner: ai
  - status: ready
  - created_at: 2026-11-20
  - phase: Backend parity (found while doing TODO-5744)
  - parallel_track: wasm-arrays-panic
  - scope: `/std/panic/panic` is `let empty = array(); /at(empty, code)`, so any program that calls it is refused by Wasm with `Wasm IR validation error: invalid IR instruction 15 in /pick: unsupported opcode for wasm target` (the array opcodes `AddressOfLocal`, `LoadIndirect`). A runtime error that `Trap` ends works on Wasm now (TODO-5744), but the documented way to raise one (`panic`) cannot be written for Wasm. Give `panic` a Wasm lowering that does not need arrays: for example lower the call to a `Trap` with the code, printing the VM's `array index out of bounds` text first on WASI, or make the stdlib helper a builtin the lowerer turns into the runtime-error sequence.
  - implementation_notes: repro: `import /std/panic/*`, `[return<i32>] pick([i32] value) { if(value > 0i32) { return(value * 2i32) } panic(7i32) }`, `main` printing `pick(3i32)` then `pick(0i32)`; `primec --emit=vm` prints `6`, the message, exits 3; `primec --emit=wasm --wasm-profile wasi` fails as above.
  - acceptance:
    - the repro above compiles for Wasm (WASI and browser) and under node exits 3 (browser: `RuntimeError`) with the VM's stdout, a node case beside the TODO-5744 cases
    - full release gate green
  - stop_rule: `panic` on Wasm only; arrays stay unsupported on Wasm.

- [ ] TODO-6106: `repeat` with a negative `i32` count says so at run time
  - owner: ai
  - status: ready
  - created_at: 2026-11-20
  - phase: Runtime errors (found while doing TODO-5744)
  - parallel_track: repeat-negative-count
  - scope: The lowerer has a runtime error `loop count must be non-negative` (`emitLoopCountNegative`, src/ir_lowerer/IrLowererFlowLoopHelpers.cpp `emitCountedLoopPrologue`), and the semantics reject a negative literal count, but a negative count held in a variable runs zero times with no error on the VM. Either the runtime check is not reached for a `repeat(count)` over a local, or the documented rule is that a negative count means zero iterations; decide against docs/spec and make code, docs and tests agree.
  - implementation_notes: repro: `[effects(io_out), return<int>] main() { [i32] count{0i32 - 1i32}  [i32 mut] total{0i32}  repeat(count) { total = total + 1i32 }  print_line(total)  return(0i32) }` prints `0` and exits 0 on the VM (release build, 2026-11-20); the spec text for `repeat` counts is the place to check.
  - acceptance:
    - a negative non-literal `repeat` count either ends the program with `loop count must be non-negative` and exit code 3 on every backend, or the spec states that it runs zero times and the unused runtime-error emitter is removed
    - matrix case; full release gate green
  - stop_rule: the negative count of `repeat` only.

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

- [ ] TODO-5718: All values of a repeated parameter and decoded path parameters
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Web apps (docs/WebAppsPlan.md; found by TODO-5702 and TODO-5703)
  - parallel_track: web-params
  - scope: `queryParam` and `formParam` give the first of repeated parameters (`?tag=a&tag=b`), so a form with checkboxes or a multi-select cannot read every value; and `pathParam` returns the segment as sent, so `/files/a%20b` gives `a%20b`.
  - decision: 2026-11-20. (1) All values of a name: count plus index, like the header accessors: `queryParamCount(name) -> i32` and `queryParamAt(name, index) -> String`, and the same for `formParam` (`formParamCount`, `formParamAt`); each call rescans the text, nothing is allocated beyond the returned String, an out-of-range index gives an empty String. A `vector<String>` form is not taken. (2) Path segments: `pathParamDecoded(name) -> String` decodes only `%XX` and leaves `+` alone (a `+` in a path is a literal plus, RFC 3986; option b); a `%` not followed by two hex digits, or bytes that are not UTF-8 after decoding, gives an empty String; `pathParam` still tells a missing segment, so no `hasPathParam` is needed. `queryParam` and `formParam` keep their decoding (`+` is a space).
  - acceptance:
    - VM and optexe tests: three values of a repeated query and form parameter in order through the count and index accessors, an out-of-range index, a `%20` and a kept `+` in `pathParamDecoded`, a malformed `%` and a non-UTF-8 segment (empty String)
    - docs/spec/stdlib-reference.md and docs/WebAppsPlan.md updated
    - full release gate green
  - stop_rule: repeated values and decoded path segments only; no typed parameters, no multipart.

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

- [ ] TODO-5956: Native string arenas and the remaining string heap statistics
  - owner: ai
  - status: ready
  - created_at: 2026-11-12
  - phase: Native PrimeScroll (docs/NativeUiPlan.md; follows TODO-5586)
  - parallel_track: native-backend-strings
  - scope: The arm64 native backend answers `__pstext_stat` for chunks, bytes in use, large blocks and bytes, and blocks handed out, and `__pstext_set_limit` over the bytes of live text blocks, but `stringHeapBytesFree()` and `heapLiveBlocks()` (statistics 2 and 8) stop the program with "string heap statistic is not available on the native backend", and `StringArena` (`__pstext_arena_begin` and `_end`) is rejected at validation ("string arenas are not available on the native backend yet"). The limit counts live text-block bytes where the VM counts the bytes of its chunk regions, so a program that sits close to a limit can differ. Count the slot heap's live blocks and the free blocks' bytes in the pool, and give arenas a bump region in the data segment's state.
  - decision: 2026-11-20, option A: native-defined statistics, not the VM's figures. Statistic 2 (`stringHeapBytesFree()`) is the bytes of freed text blocks on the pool's free lists (a counter in `emitTextFree` and in the reuse path of the pool alloc). Statistic 8 (`heapLiveBlocks()`) is the live slot-magic blocks, a counter in the heap routines gated by `textStats_` (one global atomic counter, so a threaded program counts all threads where the VM's `liveSlotAllocationCount` is per thread heap). Arenas are bump regions mapped per arena and linked from the data segment's state (statistics 6 and 7 answered), `__pstext_arena_end` unmaps nothing that a later use could trip over silently: keep the regions mapped and fill the header of freed arena text with the free magic, so text used after its arena ended hits the existing double-free style message (exit 3) rather than SIGSEGV; the limit counts text-block bytes. No startup cost for a program that does not use the figures. Option B (a second allocator mirroring `VmTextHeap`) and option C (keep faulting) are not taken.
  - implementation_notes: the native heap is one shared slot pool (256 KiB chunks, size classes shared by all blocks) while the VM's text heap is per-size-class chunks (4 KiB first chunk, doubling to 256 KiB, free chunks returned beyond a 1 MiB cap), so `chunks` and absolute `bytesFree` differ even today. The tests print relations and deltas (a figure after minus before, balanced to zero, `live` goes up by one per live block), never absolute `chunks` or `bytesFree`. docs/spec/vm-design.md (or the native backend spec) lists the differences between the VM's and the native figures.
  - acceptance:
    - `stringMemory()`, `heapLiveBlocks()` and a `StringArena` program (the programs of `tests/unit/runtime/test_vm_heap_arenas.cpp`, rewritten to print relations and deltas) run through `--emit=native` with the VM's output
    - text used after its arena ended reports the free-magic double-free message and exit 3 on native
    - the spec lists the differences in the figures
    - full release gate green
  - stop_rule: the native string heap's arenas and the two statistics only.

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

- [ ] TODO-6052: Recursion limits of each backend are documented
  - owner: ai
  - status: ready
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10 (split from TODO-5517)
  - parallel_track: runtime-faults
  - scope: vm-design.md (Errors, "Recursion limits") now states the measured limits of the VM, `optexe`, native and `exe` on macOS arm64 (4096 frames and exit 3 on the VM and `optexe`; SIGSEGV past 8000 to 20000 levels on native and 20000 to 50000 on `exe`). Still open: measure Wasm and GLSL (a Wasm runtime is needed), the Linux x86_64 native stack depth, and add the exact native and `exe` limits for the default stack of a Linux machine.
  - acceptance:
    - vm-design.md states the Wasm outcome past its limit and the Linux native and `exe` depths, each measured
    - full release gate green
  - stop_rule: documentation and measurement; no stack-depth checks are added to native.

- [ ] TODO-6090: The program matrix runs the programs Wasm supports on a Wasm config
  - owner: ai
  - status: ready
  - created_at: 2026-11-20
  - phase: Correctness audit 2026-10
  - parallel_track: wasm-matrix
  - depends_on: TODO-5744 (done)
  - scope: `PRIMESTRUCT_MATRIX_CONFIGS` (tests/unit/program_matrix/program_matrix.h) has no Wasm config. A survey (every raw-string program of tests/unit/program_matrix/test_program_matrix_*.cpp compiled with `--emit=wasm --wasm-profile wasi` and run under node, 365 programs, 2026-11-20) found 47 that already print and exit as the VM does (27 of control, 6 optimizer, 5 shared_programs, 4 user_shadows, 1 each of conversions, frontend_accept, parameter_modes, runtime_faults, struct_methods), 2 that only differ because the runner reads the result of a `void` main, and 316 that do not compile for Wasm (about 200 `unsupported opcode for wasm target`, 23 `unsupported effect mask bits`, 19 atomics, 18 channels, the rest frontend errors of programs that need a flag or are fragments).
  - decision: 2026-11-20, option (a): the Wasm config is slow and opt-in (`PRIMESTRUCT_MATRIX_CONFIGS=all`) and runs only the cases that list it in `onlyConfigs` (about 47 annotations, so a backend gap stays visible as the absence of the annotation). Options (b) (about 316 `skipConfigs`) and (c) (`wasmUnsupported`) are not taken. The config adds `Backend::Wasm` to `ExecutionConfig` and `runConfigUnnormalized`: `primec --emit=wasm --wasm-profile wasi ... -o x.wasm`, then `node` with a runner like `WasmNodeRunner` in tests/unit/compile_run/smoke/test_compile_run_smoke_wasm_node.cpp; a `void` main is exit code 0; a Wasm trap is a fault outcome (exit 3 under WASI once TODO-5744 lands, which this leaf depends on); a machine without node skips the config with a message (`hasNode`).
  - acceptance:
    - the Wasm config runs the cases that list it in `onlyConfigs` (the 47 programs of the survey) and agrees with the VM on stdout and exit code, a trap as the fault outcome
    - a machine without node skips the config with a message
    - full release gate green
  - stop_rule: the config and its annotations only; opcode gaps found by it (heap, strings, atomics, channels) are separate leaves.
  - notes: a matrix helper such as `check()` that returns an `if` expression could not run on Wasm until TODO-6089 (a value-yielding `if`) was fixed.

- [ ] TODO-6080: A diagnostic column and snippet are those of the source the user wrote, not the text-filtered form
  - owner: ai
  - status: ready
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10 (split from TODO-5520)
  - parallel_track: diagnostics-filtered-columns
  - scope: A semantic error in `[i32] b{a + 2i32 * nope}` prints line 4 column 34 and the snippet `plus(a, multiply(2i32, nope))`; `nope` is at column 22 of the file. The text filters rewrite operators into calls and keep no offset map.
  - decision: 2026-11-20, option (a): the text filter records each rewrite as an edit (original range, filtered range), an offset map from the filtered text back to the user's source, and spans, columns and snippets map back through the edits, so a diagnostic shows the user's text and true columns. The token-aligning heuristic (b) and printing both lines (c) are not taken.
  - implementation_notes: the filters are in src/text_filter (search the operator and call rewrites); the edit list is carried by the parsed program or its source record so `primec` diagnostics, `--collect-diagnostics` and the semantic product spans use it; imports and stdlib text keep their own maps.
  - acceptance:
    - a diagnostics test asserts file, line, column and the snippet of an error inside an operator expression and a rewritten call, and of an error after a rewritten span on the same line
    - diagnostics of programs without rewrites are unchanged
    - full release gate green
  - stop_rule: spans and snippets only.

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

- [ ] TODO-6083: The arity and shape of an `on_error` handler are checked in semantics
  - owner: ai
  - status: ready
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10 (split from TODO-5520)
  - parallel_track: diagnostics-on-error-handler
  - scope: `on_error<Oops, /onErr>` with `onErr([Oops] err, [i32] extra)` passes semantics and fails in lowering (`argument count mismatch for /onErr`). The check belongs in semantics with a message that names the handler and the expected shape. Span decided by the owner on 2026-11-20: option (a), the `on_error` transform that names the handler (primary span), with the handler definition as a related span. Rejected: the first `try` that uses the handler.
  - acceptance:
    - a semantics test for a wrong arity and a wrong first parameter type, with file, line and column
  - stop_rule: semantic diagnostics of `on_error` handlers only.

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

- [ ] TODO-6093: The compiler build id of a library costs less than hashing the 22 MB `primec`
  - owner: ai
  - status: ready
  - created_at: 2026-11-20
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, section 6)
  - parallel_track: lib-build-id
  - scope: `currentCompilerBuildId` (src/pipeline/LibraryLink.cpp) hashes the whole `primec` executable once per compile that has `--library`; since TODO-6092 that is 14 ms on arm64 (SHA-2 instructions) and still most of the 129M instructions a linked library costs over an empty `main` (61.5M against about 190M); on x86_64 and Linux the portable routine is about 5 times slower. The id makes an archive refuse a compiler that did not write it, and that meaning must survive. Options considered: a sidecar next to the executable (A), a stamp embedded at link time (B), a hash tree (D) and no check inside a build graph (E).
  - decision: 2026-11-20, option C: the id is the linker's own UUID (`LC_UUID` on macOS, the GNU `.note.gnu.build-id` with `--build-id`, `lld --build-id=sha1`), read from the first kilobytes of the executable. Cost near zero, no sidecar and no build-graph step, and the linker derives it from the content, so it changes with the code. Fall back to the full SHA-256 of the executable when the executable carries no UUID or build-id note (`-no_uuid`, a deterministic-build flag, a PE build); the two forms are distinguishable in the id text so an archive written under one is refused by a compiler that computes the other. Make sure the release and debug builds link with a UUID (`--build-id` on Linux). Option A may be added later if a toolchain without a UUID matters.
  - acceptance:
    - the build id of `primec` is its linker UUID or build-id note when present and the full hash otherwise (both paths tested; the fallback by a test hook or a stripped copy)
    - a library built by one compiler is refused by another, as the corpus tests it today
    - instructions retired of a linked library compile against an empty `main`, recorded in docs/todo_log.md
  - stop_rule: the definition of the compiler build id; no change to what an archive carries.
  - notes: the measurements are in docs/todo_log.md, TODO-6093. TODO-6097 (roots below `/std`) also touches src/pipeline/LibraryLink.cpp; do not run the two at once.

- [ ] TODO-6097: Several stdlib libraries link, and a module a linked library owns is not spliced from the stdlib source
  - owner: ai
  - status: ready
  - created_at: 2026-11-20
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, section 12; from the TODO-6093 measurements in docs/todo_log.md)
  - parallel_track: lib-stdlib-roots
  - scope: Every stdlib library has the root `/std` (the root of a library is the first path segment of its symbols), and `loadLibraries` refuses two that own one root: `--library color.psa --library vector.psa` gives `library color and library vector both own /std`. And `import /std/math/*` splices the library interface (the root is owned) and then also the stdlib source (the stdlib pass reads `sourceStdImports` of the expanded text without asking who owns the root), so the definitions exist twice: `cannot link library color: duplicate definition of /std/math/linearToSrgbChannel`; `import /std/math/linearToSrgbChannel` gives the same. `IrLinkOptions::sharedRoots` (`/std`, src/pipeline/LibraryLink.cpp) only removes a duplicate at the link, after the front end has already paid for the source; `checkProgramDefinesNoOwnedPath` (src/pipeline/LibraryOwnership.cpp) looks at the program's own definitions and imports, not at stdlib splices.
  - decision: 2026-11-20 (stdlib ownership): roots below `/std`, one per module family (for example `/std/math`, `/std/path`), so several stdlib libraries can be linked, and the stdlib pass skips every module a linked library owns. One `std.psa` for all of `/std` is not taken (most stdlib modules do not build with `--emit=lib`: math/matrix, math/quaternion, image, text/string, collections/map and others; the list is in docs/todo_log.md, TODO-6093).
  - implementation_notes: the root of a library (its owned namespace) becomes the module family path instead of the first path segment, derived from the interface's public symbols or declared by the library's build step (pick the smaller; document it in docs/ModulesAndLibrariesPlan.md sections 4 and 6); `loadLibraries` refuses two libraries only when one root is a prefix of the other or equal; the stdlib pass (the reader of `sourceStdImports`) is given the set of owned roots and drops the stdlib modules below them, so `import /std/math/*` with `--library` for `/std/math` splices the interface once and no stdlib source; `sharedRoots` is deleted or narrowed to what remains. Corpus tests live under tests/unit/library (search `--library`).
  - acceptance:
    - `--library color.psa --library vector.psa` (stdlib math/color and math/vector built with `--emit=lib`) links and the program gives the same results as the whole-program build
    - `import /std/math/*` plus `--library` for the same root no longer gives `duplicate definition`; the stdlib source of an owned module is not read (a test counts it)
    - two libraries with overlapping roots are still refused with a message that names both
    - the TODO-5931 benchmark (docs/todo_log.md, TODO-5931 and TODO-6093 tables) rerun with the real stdlib imports and recorded in docs/todo_log.md, with a note on whether the 20 percent bar of a typical program is met
    - full release gate green
  - stop_rule: library roots and stdlib splice skipping only; no new library shapes (TODO-6032), no `String` as a library.
  - notes: shares src/pipeline/LibraryLink.cpp with TODO-6093; do not run the two at once.

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

- [ ] TODO-6104: An element of a vector of vectors read through a `Reference<vector<vector<i64>>>` frees the element's buffer
  - owner: ai
  - status: ready
  - created_at: 2026-11-20
  - phase: Value lifecycle (docs/spec/value-lifecycle.md, Copies and places)
  - parallel_track: lifecycle-reference-element
  - scope: Found with TODO-6099. `peek([Reference<vector<vector<i64>>>] v) { return(v[0i32].count()) }` called on a local `vector<vector<i64>>` with one pushed vector frees the element's buffer (the live block count drops by one) and the owner's destroy at scope end then faults with `VM error: invalid heap free address in IR`. The same read through a read-only `[vector<vector<i64>>] v` parameter or a local is fine. The IR copies the five-slot element record into a temporary, clears the source's owner flag (`[record + 4 slots]`) and destroys the temporary: the element is treated as a temporary the copy takes over. `isBuiltinVectorElementHelperCall` (src/ir_lowerer/IrLowererLowerReturnEmitStage.cpp) answers false for a receiver local that is a `Reference` with a struct path and no `referenceToVector` flag, which is what a `Reference<vector<T>>` parameter of a builtin vector of vectors has, so the `at` it resolves to is read as a call returning a value. Hits `Mutex<vector<vector<i64>>>` (a `locked` lambda reading `rows[0i32][0i32]` through its `Reference` parameter).
  - implementation_notes: reproduce with the `peek` helper above and `heapLiveBlocks()` (VM, optexe); compare the IR of `peek` taking `[vector<vector<i64>>]`.
  - acceptance:
    - positive: reading `v[i]`, `v[i].count()` and `v[i][j]` through a `Reference<vector<vector<i64>>>` parameter leaves the owner's buffers alone (heap statistics unchanged, values right) on the VM and optexe; values on native
    - matrix case on every config; full release gate green
  - stop_rule: element reads of builtin vectors of vectors through a `Reference`; no change to the stdlib `Vector<T>`.

- [ ] TODO-6105: `*field = value` writes through a `mut` `Reference` field
  - owner: ai
  - status: ready
  - created_at: 2026-11-20
  - phase: Value lifecycle (docs/spec/value-lifecycle.md, Copies and places)
  - parallel_track: reference-field-write
  - scope: Decided by the owner on 2026-11-20: it should work. A `Reference<T>` field is construction-only (TODO-5853), so `this.kept = v` is a diagnostic. Writing the referent today needs `[Reference<i64> mut] target{location(this.kept)}` and `target = value`. Make `*kept = value` (and `*this.kept = value`) in a struct member write through the reference to the referent on every backend, as assigning to a reference member does in C++; it never rebinds the field.
  - acceptance:
    - positive: a `[Reference<i64> mut]` field written with `*kept = value` changes the referent (values on the VM, native and optexe); a read through the same spelling still works
    - negative: `*kept = value` on a field that is not `mut` is a diagnostic with file, line and column
    - docs/spec updated; matrix case on every config; full release gate green
  - stop_rule: write-through of `Reference` fields only; no change to `Pointer` assignment.

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

