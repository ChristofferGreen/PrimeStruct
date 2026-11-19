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
| TODO-6053 | Tests report progress and a watchdog replaces wall-clock timeouts | blocked | gate-timeouts |
| TODO-5752 | x86_64 native and the Linux JIT take the address ABI of real calls | deferred | native-real-calls |
| TODO-6074 | An args pack and `Result.map` destroy the `String`s of their temporaries | deferred | lifecycle-temporaries |
| TODO-6075 | A host call may yield on the deterministic VM scheduler | deferred | vm-host-yield |
| TODO-5688 | Calls require the callee's whole active effect set | deferred | effects-transitive |
| TODO-5853 | A borrowed `Reference` parameter stored through a `[ref]` capture is diagnosed | deferred | closures-references |
| TODO-5854 | A `Mutex` or `Atomic` handle does not keep the value it was made from | deferred | mt-primitives |
| TODO-5804 | GET handlers that only read the state run at the same time under `RwLock` | deferred | mt-server |
| TODO-5806 | Sessions: a cookie and per-session state | deferred | web-sessions |
| TODO-5821 | The first toolbar of a launch does not hold up the first frame | deferred | startup-toolbar |
| TODO-5603 | The real-launch baseline, measured by a person | ready | startup |
| TODO-5956 | Native string arenas and the remaining string heap statistics | deferred | native-backend-strings |
| TODO-5536 | Small, lazily loaded bytecode for apps | deferred | bytecode-startup |
| TODO-5533 | Windows and Linux native UI backends | deferred | native-ui-platforms |
| TODO-5718 | All values of a repeated parameter and decoded path parameters | deferred | web-params |
| TODO-6037 | A Result-returning main exits with its decoded code on wasm | deferred | result-main-wasm |
| TODO-5511 | Safe code cannot reach container storage or unsafe stdlib helpers | deferred | safety-stdlib |
| TODO-5512 | Pointers and aliases count as borrows of their root | deferred | safety-borrows |
| TODO-6059 | Native x86_64 float to integer conversion saturates | deferred | numeric-conversions |
| TODO-6061 | Integer narrowing and widening on Wasm | deferred | numeric-conversions |
| TODO-6051 | Native x86_64 (ELF) division by zero is the VM's runtime fault | deferred | runtime-faults |
| TODO-6052 | Recursion limits of each backend are documented | ready | runtime-faults |
| TODO-5744 | A runtime error in a real call ends the program on Wasm and GLSL | deferred | runtime-faults |
| TODO-6080 | A diagnostic column and snippet are those of the source the user wrote, not the text-filtered form | deferred | diagnostics-filtered-columns |
| TODO-6081 | An unknown `break` is reported at the user's `break`, not inside stdlib/std/file/file.prime | deferred | diagnostics-break-span |
| TODO-6082 | `borrowed binding: v (root: v, sink: v)` names the sink | deferred | diagnostics-borrow-sink |
| TODO-6083 | The arity and shape of an `on_error` handler are checked in semantics | deferred | diagnostics-on-error-handler |
| TODO-6084 | Internal lowering errors (`missing semantic-product ...`) say so | deferred | diagnostics-internal-errors |
| TODO-6085 | The semantics validator uses the shared demangle helper | blocked | diagnostics-demangle-semantics |
| TODO-6068 | Wasm computes `FmaF32` and `FmaF64` with one rounding | deferred | stdlib-math-fma |
| TODO-6067 | x86_64 native emits FMA3 `vfmadd231sd` and `vfmadd231ss` | deferred | stdlib-math-fma |
| TODO-5545 | Record the macOS AppKit manual smoke checklist | deferred | native-ui-macos-manual |
| TODO-5902 | Each task of a `--vm-threads` run uses the fast kernel | deferred | vm-threads |
| TODO-6019 | PrimeScroll built by `make.prime` starts as fast as the script's, measured by a person | ready | build-dogfood |
| TODO-5931 | The stdlib builds as a library | deferred | lib-stdlib |
| TODO-5932 | Libraries carry back-end code | deferred | lib-backend |
| TODO-5933 | The embed API loads a library at run time | deferred | lib-embed |
| TODO-6032 | A public function of any shape is a function of a library | deferred | lib-shapes |
| TODO-5987 | The lowerer's include-only fragments become components | ready | lowerer-fragments |
| TODO-5973 | SoA rewrite passes share one context and one walker | ready | reuse-soa-rewrites |
| TODO-5976 | The semantic product publishes parsed types, and the lowerer stops re-parsing type text | deferred | reuse-type-model |
| TODO-5977 | Semantics diagnostics go through one builder | deferred | reuse-diagnostic-builder |
| TODO-5979 | `--collect-diagnostics` reports no false intra-body call diagnostics | ready | collect-diagnostics-false-positives |

### Ready Now

- TODO-5603 (startup): the real-launch baseline, measured by a person
- TODO-6052 (runtime-faults): recursion limits of each backend are documented

### Immediate Next 10

1. TODO-5603
2. TODO-6052

### Priority Lanes

- Startup, first (docs/NativeUiPlan.md section 1, "Where a launch goes"): TODO-5603, TODO-5821
- Test liveness (docs/TestWatchdog.md): TODO-6053
- Native PrimeScroll, direct native backend (docs/NativeUiPlan.md): TODO-5956
- Native UI (docs/NativeUiPlan.md): TODO-5536, TODO-5533
- Effects (docs/spec/language-core.md, Effects): TODO-5688
- VM threads (docs/MultithreadingPrototype.md, VM Threads; after the startup lane): TODO-5902, TODO-6075
- Build system (docs/BuildSystemPlan.md; after the startup lane): TODO-6019
- Modules and libraries (docs/ModulesAndLibrariesPlan.md; after the startup lane): TODO-5931, TODO-5932, TODO-5933, TODO-6032
- Result and control flow (docs/spec/errors-and-file-io.md): TODO-6037
- Memory safety (docs/spec/type-system.md Memory safety): TODO-5511, TODO-5512
- Backend parity: TODO-6059, TODO-6061, TODO-6051, TODO-6052, TODO-5744
- Frontend and diagnostics (split from TODO-5520): ; deferred TODO-6080, TODO-6081, TODO-6082, TODO-6083, TODO-6084, TODO-6085
- Fused `fma` (IR opcodes `FmaF32` and `FmaF64`, done on the VM, the C++ emitters, arm64 native and GLSL): deferred TODO-6067 (x86_64 native FMA3, needs an x86_64 machine), TODO-6068 (Wasm exact emulation)
- macOS support: TODO-5545 (needs macOS)
- Closures (docs/spec/value-lifecycle.md, Lambdas): TODO-5853
- Web apps (docs/WebAppsPlan.md; built with `--emit=optexe`): TODO-5718
- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix; VM speed ; passes ; real calls TODO-5752
- Multithreading (docs/MultithreadingPrototype.md, Threads For Real; docs/WebAppsPlan.md, Concurrency; after the startup lane): TODO-5872; deferred TODO-5804, TODO-5806, TODO-5854
- Code reuse (docs/CodeReusePlan.md; two at a time under rule 18): TODO-5973; deferred TODO-5976, TODO-5977

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

Multithreading (docs/MultithreadingPrototype.md, Threads For Real), after the startup lane (TODO-5772 is done); not above them in Ready Now or Immediate Next 10. In this order (TODO-5792 and TODO-5835, VM tasks and their lowering, are done, and so are TODO-5793, byte-region checks without a map search, TODO-5794, `optexe` per-thread heap arenas, TODO-5795, `optexe` threads for `[spawn]`, `Atomic<i32>` and `Atomic<i64>` in `/std/sync`, TODO-5797, `Mutex<T>` with `locked(body)`, TODO-5846, `optexe` faults when every task waits, TODO-5798, bounded `Channel<T>`, TODO-5656, the atomic `SharedString` owner count, TODO-5802, `serveWorkers`, and TODO-5803, the apps on workers, measured, and the `SharedString` count inside the text block, are done): arm64 native is done: TODO-5799 (pthreads; TODO-5942, TODO-5943), TODO-5800 (per-thread heaps) and TODO-5801 (atomics, `Mutex`, `Channel`; TODO-5950, TODO-5951). TODO-5799 is done, and TODO-5893 (an unused `/std/tuple/*` import is reported as an unknown import path) is done too, and TODO-5894 (tuple destructuring and the destructured multi-wait inside a nested block are expanded; TODO-5849, each destructured multi-wait binding has its own result type, is done) is done too, and TODO-5847 (spawned calls with builtin `map<K, V>` or owned sum parameters or results run as tasks, done with TODO-5994, `wait(t).count()` of a stdlib `Vector` result waits twice, is done; TODO-5992, builtin `vector<T>` parameters and results, TODO-5993, the stdlib `Vector` and `Map` structs, TODO-5845, the same for `String`, struct and closure values, and TODO-5895, `copy` and `move` scalars and structs, are done) (TODO-5807, destructured multi-wait, TODO-5790, sendable spawn arguments, and TODO-5791, every `Function` sendable by construction, and TODO-5848, `return(wait(t))` of a `String` task result, are done). Deferred: TODO-5804 (read-only handlers under `RwLock`), TODO-5806 (sessions); TODO-5805 (keep-alive) is done, TODO-5854 (a `Mutex` or `Atomic` handle keeps a copy of its first value). Found with `Mutex` (TODO-5852, `wait(a, b)` of two `Task<void>`, is done) in the closures lane: TODO-5934 (a `Reference`-returning call read as a value yields its referent, found with TODO-5850), TODO-5850 (a `Reference`-returning call as a closure argument) and TODO-5851 (a `[ref]` closure written in a generic call) are done. Found with `Channel` and ready, in the Result and control flow lane: TODO-5855 (a method call on a `pick` payload of struct type), TODO-5856 (`pick` on a call result frees an owning payload), TODO-5857 (a `vector<T>` payload reads its count in a `pick` arm), TODO-5858 (`some<Vector<i32>>` of a fresh vector frees it), TODO-5859 (a `pick` expression yields a vector), TODO-5860 (moving a payload binding does not poison a later binding of the same name), TODO-5866 (a closure captures a `pick` payload binding), TODO-5877 (a field an arm of a `pick` expression yields is copied), TODO-5879 (a `pick` expression's result owns a value of its own), TODO-5880 (a payload moved out of a sum local leaves the local moved-from, and a payload of a borrowed sum cannot be moved or returned), TODO-5873 (a sum temporary passed as an argument is destroyed after the call), TODO-5874 (assigning to a `mut` sum local destroys the payload it held), TODO-5883 (assigning to a `mut` builtin vector local destroys what it held and copies a place), TODO-5876 (`return(pick(...))` of a vector and assignment from a `pick` expression), TODO-5875 (`count()` on a local declared `Vector<T>` lowers), TODO-5878 (a `[move name]` lambda capture compiles), TODO-5884 (assignment to a `mut` vector parameter and to an inferred vector-of-struct local keeps one owner), TODO-5882 (a vector of sums destroys the payloads of its elements), TODO-5885 (a vector temporary that is only read is destroyed), TODO-5889 (`a = move(b)` of a vector local takes the vector over), TODO-5890 (the arms of an assigned `pick` may open on the next line), TODO-5881 (a call takes a call to the same definition as its argument on the VM), TODO-5888 (an element of a vector temporary read as a struct or a `String`) and TODO-5887 (`remove_at` and `remove_swap` compile on a vector of sums) are done, and found with them: TODO-5886 (`push` on a local declared `Vector<T>` resolves) is done, and TODO-5978 (a `Vector<String>` leaks what `pop` and `clear` remove and faults after `remove_at`; its element reads by `at` and `[]`) is done, and found with it: TODO-5991 (a field read of a struct a call returns leaves the temporary's copy undestroyed) is done; TODO-5891 (a returned vector field or vector element of a local is a copy) and TODO-5892 (a block expression whose value is an owning local hands it over once) are done. Found with `serveWorkers` (TODO-5865, a `Function` local given positionally to a brace constructor, TODO-5864, a lambda in a generic definition naming its type parameters, and TODO-5935, a lambda parameter naming a struct of its namespace, are done): TODO-5862 (`serveWorkers` on the VM over a scripted net, done), and ready: TODO-5866 (a closure captures a `pick` payload binding). Found measuring the apps on workers (TODO-5803, done): TODO-5867 (a string heap that alternates between block sizes keeps its chunks) and TODO-5869 (1 worker as fast as `serve`; the 32 KB form reached 2.41x with 4 workers, not 2.5x) are done; TODO-5940 (a load generator that costs less per connection than the servers it measures, done: compiled, 1.4 us of user CPU per request, and the kernel's loopback setup is the ceiling; TODO-5871 found every server held to about 14,600 new connections a second by the generator and the kernel's loopback setup, which caps the form with 4 workers at 2.41x); TODO-5871 (that measurement), TODO-5872 (a byte of another thread's block is checked on a slow path, so the worker parses from a copy) and TODO-5870 (a VM program that uses tasks runs its string code on the fast kernel) are done; and in the Result and control flow lane TODO-5868 (a definition's `on_error` handler flows into nested blocks and a block-level `on_error` overrides it, so `serve(app)?` compiles in an `if` block of `main`) and TODO-5936 (a diagnostic in `main` of a program that imports `/std/http/server` reported a stdlib position, found with it) are done.

VM threads (docs/MultithreadingPrototype.md, VM Threads), after the startup lane: the VM gets a threads mode next to its deterministic scheduler. TODO-5896 (`--vm-threads`, a thread and a heap arena per task) and TODO-5897 (Atomic, Mutex and Channel on real primitives, with the deadlock fault) and TODO-5898 (reentrant and main-thread-only host functions) and TODO-5899 (`RunOptions::threads`, fault and cancel through the embed API) and TODO-5900 (the concurrent corpus repeated under `--vm-threads` and under ThreadSanitizer) and TODO-5901 (`--vm-record-sync` and `--vm-replay-sync`, the synchronization order of a threaded run replayed on the deterministic scheduler) landed; TODO-5904 (threads by default) is closed: the default stays the deterministic scheduler; deferred TODO-5902 (the fast kernel per task). TODO-5903 (debug sessions run programs with tasks, the first prerequisite of docs/VmInspectorPlan.md) landed.

Build system (docs/BuildSystemPlan.md), after the startup lane: TODO-5906 (directory listing and file metadata, `/std/dir`) and TODO-5915 (incremental builds) landed. TODO-5907 (glob, `/std/glob`) and TODO-6009 (a changed glob listing re-runs a step) and TODO-5918 (staged builds, `Graph.run` and the `build_run` effect) landed; TODO-5916 (subprojects, `b.subproject`) and TODO-5917 (options, `b.option`) landed; TODO-5920 (PrimeScroll builds with `make.prime`) landed; TODO-5919 (function steps, `b.step`) landed; ready: TODO-6019 (person measures startup); and the modules and libraries lane, which splits static libraries (docs/ModulesAndLibrariesPlan.md): TODO-5922 (`primevm --link` merges two IR modules) landed; TODO-6031 (lowering marks string literal pushes, so a library's strings always relocate) landed; TODO-5923 (`--emit=interface`) landed; TODO-5924 (`--emit=lib`) landed; TODO-5925 (`--library` and the link in the pipeline) landed; TODO-5926 (generics and lifecycle helpers cross the library boundary) landed; TODO-5927 (namespace ownership) landed; TODO-5928 (`--emit-uses`) landed; TODO-5929 (symbol-level rebuilds) landed; TODO-5930 (the graph builds libraries and links them) landed; deferred TODO-5931 (the stdlib as a library, measure first), TODO-5932 (libraries that carry back-end code), TODO-5933 (the embed API loads a library), TODO-6032 (public functions of any shape in a library).


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

- [ ] TODO-6053: Tests report progress and a watchdog replaces wall-clock timeouts
  - owner: ai
  - status: blocked
  - blocked_on: owner decision: no pre-watchdog loaded gate exists to compare with (docs/todo_log.md, TODO-6053); waive that line or ask for a gate of a pre-watchdog commit at load above 40
  - created_at: 2026-11-19
  - phase: Test infrastructure (replaces TODO-6028; owner decision 2026-11-19)
  - parallel_track: gate-timeouts
  - depends_on: TODO-6069, TODO-6070, TODO-6071, TODO-6072, TODO-6073
  - scope: (split into TODO-6069 the wrapper and design, TODO-6070 the doctest beat, TODO-6071 the shell-out beats, TODO-6072 and TODO-6073 the registrations; what is left here is the loaded-gate record) a wall-clock timeout cannot tell a hung test from a slow one, so a loaded gate times out tests that are making progress (TODO-6026 measured 5 to 15 times the quiet time at `nice -n 10`). Replace it with liveness: every test that can run long reports that it is awake, and a watchdog kills a test only when no progress arrived for a stated time. Design: (1) a beat is emitted by the work loop, never by a timer thread, so a deadlocked main thread stops beating (per doctest case through a custom reporter, per compiled file or batch in compile-run, build and library cases); (2) the limit is "no beat for N seconds" with N covering one unit of work under heavy load, not the whole test; (3) a test that shells out to `primec` counts output on the child's pipe or a beat written by `primec` itself as progress; (4) a generous hard cap stays as a backstop against a test that beats forever; (5) `ctest` has no heartbeat, so a small wrapper around each test command watches for beats and kills the process tree. `scripts/compile.sh` is not changed (the stability rule); the wrapper goes in the CTest registration. `nice -n 10` stays as it is.
  - acceptance:
    - a short design in `docs/` states the beat format, the wrapper, the child-process rule and the hard cap, and names which test families beat
    - a test that stops beating is killed with a message naming the last beat; a test that beats slowly under a load above 40 is not
    - a loaded full gate (load above 40) is recorded before and after with no entry killed in the run that has the watchdog
    - ctest `PrimeStruct_test_registration` still passes; no timing assertion is added to any test
    - full release gate green
  - stop_rule: the test-liveness mechanism only; no timeout of an individual test is raised, and `scripts/compile.sh` is not touched.

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

- [ ] TODO-6074: An args pack and `Result.map` destroy the `String`s of their temporaries
  - owner: ai
  - status: deferred
  - created_at: 2026-11-19
  - phase: Lifecycle (found while finishing TODO-5727)
  - parallel_track: lifecycle-temporaries
  - scope: `total(Box{[text] label(3i32)}, Box{[text] label(4i32)})` for `total([args<Box>] xs)` leaves both `String`s on the string heap after the call, and `Result.map(Result.ok(1i32), [maker]([i32] v) { return(Box{[text] label(v)}) })` leaves the mapped `Box`'s `String` (measured with `heapLiveBlocks`/`stringHeapBytesInUse` on the VM: 64 and 32 bytes). Whoever owns the pack elements or the mapped sum payload must destroy them; the matrix cases would copy the shape of tests/unit/program_matrix/test_program_matrix_user_at_sources.cpp.
  - acceptance:
    - matrix cases for an args pack of `Box` temporaries and a `Result.map` yielding a `Box` end with the heap statistics of the start on VM and optexe
    - full release gate green
  - stop_rule: ownership of args-pack elements and of a mapped sum payload only.
  - notes: needs a decision, not chosen yet (measured 2026-11-19 on the release VM and optexe, the two halves have different causes).
    - Args pack: `emitStructPackedValueToSlots` (src/ir_lowerer/IrLowererInlinePackedArgs.cpp) copies each element into the pack slots, disarms a temporary argument (the pack takes it over) and does a shallow slot copy of a named one (no `Copy` helper, no disarm), and the callee never destroys the pack. So the pack owns what a temporary gave it and borrows what a place gave it, with no per-element record of which. Option A: the pack owns every element (a named argument runs its `Copy` helper like a vector push) and the callee destroys all elements when its body ends; this changes a named argument's cost and its `Copy` side effects. Option B: keep the mixed ownership and give each element a drop flag, set only for a taken-over temporary; no `Copy` change, more state per call. Option C: the caller destroys the temporaries after the call (they are borrowed by the pack for the call's duration), like a read-only parameter; simplest, but `[spread]` forwarding of a pack then has to say who owns the elements.
    - `Result.map`: the leak does not happen for a typed local (`[Result<Box, i32>] r{...}` destroys the payload once, measured); it happens when the result is held by an `[auto]` local or read by `try` from one: the semantic product types `[auto] m{Result.map(...)}` and `[auto] r{Result.ok(Box{...})}` as `i64` (a packed legacy Result), the payload lives in untracked struct slots next to it, `try(m)` copies it out through `Copy` and nothing destroys the original (32 bytes: one `String`). Option A: infer the stdlib `Result<T, E>` sum for an `[auto]` local of `Result.ok`/`Result.map`/`Result.and_then`, so the existing sum drop applies (the binding type text changes from `i64`). Option B: keep the packed legacy local and register a drop entry for its struct payload when the init is a fresh legacy producer. Option C: refuse `[auto]` for a Result whose payload owns something (`declare the Result type`), a diagnostic instead of a fix.

- [ ] TODO-6075: A host call may yield on the deterministic VM scheduler
  - owner: ai
  - status: deferred
  - created_at: 2026-11-19
  - phase: VM threads (docs/spec/vm-design.md, Tasks; found by the worker of TODO-5862)
  - parallel_track: vm-host-yield
  - scope: A host `poll` cannot yield on the deterministic scheduler: a host call never switches tasks, and without a seed only a spawn, a wait, a block and a task's end do, so a poller (main) spins forever on the eager schedule and no worker runs. tests/unit/runtime/test_vm_serve_workers.cpp works around it by blocking its stop closure on a `Channel` the handler fills. Real sockets on the VM hit the same limit unless `--vm-threads` runs with a `Reentrant` host (`VmHostThreading`, include/primec/runtime/VmHost.h). Decide how a host call (or the program around it) gives the scheduler a switch point, then let a poller and its workers make progress on the deterministic scheduler.
  - decision_needed: the scheduler and host-call yield semantics are a design choice (docs/spec/vm-design.md fixes today that the VM switches only at a spawn, a wait and a task's end, and that the default eager schedule prints what inline calls print), so this block stays deferred until the owner picks one. Options:
    - option A, a yielding host binding: a new `VmHostThreading::Yielding` (or a `yields` flag on `VmHostBinding`) makes the call a switch point, so after it returns the scheduler hands over to the lowest-numbered runnable task (a seed picks any). Trade-off: smallest change and opt-in per host function, so existing embeddings keep today's order; but the eager schedule then depends on which functions a host marks, and a poller that calls it in a tight loop still yields every time.
    - option B, a "would block" result of the host call: the call returns a status (for example the host sets `blocked` in the result slot or returns a distinct code) and the VM switches to another runnable task and retries the call when it comes back, a wait-like block that counts for `deadlock: every task is waiting` only while nothing else is runnable. Trade-off: the right semantics for real sockets (a poll with nothing ready), but a new host-call protocol on `VmHostInvoke`/`VmHostStringInvoke`, retry rules for calls with side effects, and a deadlock rule for a host that never becomes ready.
    - option C, a program-level yield: a `yield` builtin or `/std/sync` function (a new task-switch opcode that is a no-op where one call stack runs, a pthread yield under `--vm-threads`, and a switch point on `optexe` and arm64 native), which a poller calls between polls. Trade-off: no host change and it also serves pure compute tasks, but every backend and the effect system get a new operation, and `serveWorkers` and every poller must call it.
    - option D, no yield: keep the deterministic scheduler without host-call switch points and document that a poller and workers on the VM need `--vm-threads` with `Reentrant` host functions; scripted tests keep the blocking-closure form. Trade-off: no change and no new semantics, but real sockets cannot serve workers on the deterministic schedule and the debugger, seeds and the eager order cannot be used for them.
  - implementation_notes: the scheduler is `VmTaskScheduler` (src/runtime/VmTaskScheduler.cpp), host calls are dispatched through `VmHostFunctions`/`VmHostBinding` (include/primec/runtime/VmHost.h) and the threaded mode is src/runtime/VmThreadedTasks.cpp. The test that works around the limit is in tests/unit/runtime/test_vm_serve_workers.cpp.
  - acceptance:
    - to be set when the leaf is un-deferred; at least: a poller that calls a host `poll` in a loop lets a spawned worker run on the deterministic scheduler (eager and seeded), with the outcome and output the same for a given seed, and a poll that can never be satisfied is `deadlock: every task is waiting`
  - stop_rule: the scheduler and host-call yield semantics only.

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
  - notes: deferred, needs a decision (checked 2026-11-19, nothing implemented). Two points the http conventions do not settle:
    1. Shape for all values of a name. (a) Count plus index like the header accessors: `queryParamCount(name) -> i32` and `queryParamAt(name, index) -> String` (same for `formParam`); each call rescans the text, no allocation beyond the returned String, out-of-range gives an empty String. (b) One call returning a `vector<String>`: `queryParams(name)`, `formParams(name)`; one scan and one heap-owned result, but the stdlib http surface returns no vectors today and a `vector<String>` needs the collection imports in every web app. The block text offers both; the header accessors (`headerCount`/`headerName`/`headerValue`) favour (a), whose names would still need a call (`queryParamCount`/`queryParamAt` versus `queryParamValueCount`/`queryParamValue`).
    2. Decoding a path segment. The block says "the same decoding as `queryParam`", which turns `+` into a space. In a path a `+` is a literal plus (RFC 3986; only form encoding makes it a space), so `/files/a+b` would decode to `a b`. Options: (a) share `urlencoded_byte_at` literally, `+` is a space; (b) decode only `%XX`, `+` stays; the malformed rule is the same either way: a `%` not followed by two hex digits, or non-UTF-8 bytes, gives an empty String (the name is `pathParamDecoded(name) -> String`, with `hasPathParam` not needed since `pathParam` still tells a missing segment).

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

- [ ] TODO-5853: A borrowed `Reference` parameter stored through a `[ref]` capture is diagnosed
  - owner: ai
  - status: deferred
  - created_at: 2026-11-10
  - phase: Closures (docs/spec/value-lifecycle.md, Lambdas)
  - parallel_track: closures-references
  - scope: A closure `[ref keeper]([Reference<i64>] value) { keeper.kept = value }` with `keeper` a struct that has a `[Reference<i64>] kept` field is accepted; the assignment copies or rebinds in a way that does not alias the parameter today, but nothing says a borrowed parameter cannot be stored. Decide what assigning to a `Reference` field means (rebind or write through) and diagnose storing a borrowed parameter with the existing `borrowed parameter escapes via assignment`. Also: a closure that returns its `Reference<T>` parameter returns a copy of the value without a diagnostic, and `return(location(value))` is `unable to infer return type`.
  - design_decision_needed: what `keeper.kept = value` means for a `[Reference<T>]` field is not stated by the spec (docs/spec/value-lifecycle.md and docs/spec/language-core.md only cover `Reference` parameters and locals), so this block stays open until the owner picks one. The escape diagnostic itself is trivial once the meaning is fixed.
    - option A, assignment writes through: `keeper.kept = value` stores the value into the referent of `kept`, like assigning to a `Reference` parameter. Nothing is stored, so a borrowed `Reference` parameter can never escape through a field and no new diagnostic is needed for the assignment; the gap left is that `Reference` fields have no way to be rebound after construction. Trade-off: consistent with `[Reference<i64> mut] value` parameters, but then this TODO reduces to the two return cases (a `Reference<T>` parameter returned as a copy, `return(location(value))` not inferring), and the acceptance line "negative: `borrowed parameter escapes via assignment: value`" is dropped.
    - option B, assignment rebinds: `keeper.kept = value` makes the field alias the parameter's referent. That is a real escape, so it gets `borrowed parameter escapes via assignment: value` (the existing message) for a borrowed `Reference` parameter, and a `Reference` field needs a rule for what it may be rebound to (a place that outlives the struct) plus a way to write through it (`keeper.kept = ...` could no longer mean both). Trade-off: needs a distinct write-through spelling and lifetime checks for the rebound target; most expressive and matches the acceptance as written.
    - option C, `Reference` fields are construction-only: a field of type `Reference<T>` is bound once by brace construction and an assignment to it is `assign target must be a mutable binding` (or a new `Reference field cannot be reassigned`); a write through it uses a method or `*kept = value`. Trade-off: no rebinding semantics to define and no escape through assignment at all, but a closure `[ref keeper]` can still never store a parameter, so the acceptance becomes that diagnostic instead of the escape one.
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
  - design_decision_needed: choosing between the options below is a language-design decision (new construction semantics or new surface syntax), so this block stays open until the owner picks one. Nothing inside the current language fixes it: `Mutex<T>{value}` maps `value` to the first field, every field is a stored `T` that the implicit field destroy and a user `Copy` must treat, and `uninitialized<T>` fields (docs/spec/value-lifecycle.md) cannot take a `T` brace argument.
    - option A, a construction-only parameter: a field marked as constructor-only (for example `[ctor_only T move] initial`) takes the brace argument, is visible to field defaults (`value{mutexValueAlloc<T>(initial)}`) and is not stored. Trade-off: new field modifier and brace-mapping rules, a new layout case (fields that exist only during construction), and every backend's struct layout and the struct-brace spec change; it also serves other handle types (`Channel`, `SharedString`), so it is the most general.
    - option B, `move` of a field in a field default leaves that field empty: `value{mutexValueAlloc<T>(move(initial))}` then marks `initial` moved-from and the handle's destroy and a user `Copy` skip it. Trade-off: semantics of moving out of `this` fields during construction and a per-field moved-from flag (or a `Maybe<T>`-like cost), so the handle still has `sizeof(T)` storage; smaller than A but the handle stays as large as a `T` plus two pointers, and the empty-field state for a type with no default needs a rule.
    - option C, factory instead of brace: `Mutex<T>` takes only `cell`, `value` and `live` as fields and is made by `makeMutex<T>(value)` (and `Atomic` by `makeAtomic`). Trade-off: no language change, but `Mutex<T>{value}` stops compiling, every use (stdlib, tests, docs, apps) changes, and the spec's rule that value construction uses braces then has the stdlib as an exception.
  - implementation_notes: stdlib/std/sync/sync.prime. Needs a way for a struct to take a construction argument that is not a stored field, or `Create` to move a field out and leave it empty (see design_decision_needed); tests/unit/program_matrix/test_program_matrix_mutexes.cpp ("locked returns the body's value...") prints the copies and destroys of a `Noisy` element and must then show one copy at most and one destroy.
  - acceptance:
    - positive: the `Noisy` program prints no `copy` for a handle copy and one `destroy` in all; heap statistics stay balanced
    - negative: `Mutex<T>{}` without a value for a `T` that has no default is still a diagnostic
    - full release gate green
  - stop_rule: the handle layout of `Mutex` and `Atomic`.

- [ ] TODO-5804: GET handlers that only read the state run at the same time under `RwLock`
  - owner: ai
  - status: deferred
  - created_at: 2026-11-09
  - phase: Multithreading and Web apps (docs/MultithreadingPrototype.md, The Server)
  - parallel_track: mt-server
  - scope: Under `serveWorkers` every handler holds the app's mutex, so page rendering runs one request at a time. Add `RwLock<T>` (`read(body)` with a read-only `T`, `write(body)` with a `Reference<T>`), let a handler say it only reads (its state parameter `[State] state` against `[Reference<State>] state`, which needs same-arity overloads by argument type (struct methods resolve them since TODO-5735); or a named registration, open question 5 of the design), and run read-only handlers under the read lock. Deferred: decide the registration form first.
  - design_decision_needed: how a handler says it only reads is open question 5 of docs/MultithreadingPrototype.md (The Server) and a language-design choice (surface syntax and overload rules), so this block stays deferred until the owner picks one; nothing is implemented. Measured context: `serveWorkers` (stdlib/std/http/server.prime) erases the whole app into one `RequestHandler` held in one `Mutex<RequestHandler>` and takes the lock around each call (`work_slot`), so the lock mode must be known per request before the handler runs, and a route lookup must happen before the lock is taken, or the lock mode must come from the handler's type. `RwLock<T>` exists in stdlib/std/sync (`read(body)` over a read-only `T`, `write(body)` over `Reference<T>`; docs/MultithreadingPrototype.md, RwLock). Options for the registration form:
    - option A, by the state parameter (the recommendation of question 5): a handler whose state parameter is `[State] state` is read-only, one with `[Reference<State>] state` may write, picked by overloads of `get`/`route` on the `Function` type. Trade-off: no new syntax and the read-only rule is the existing borrow check (writing a `[State]` is already a diagnostic, which gives the acceptance's compile error for free), but a lambda's type is not known at overload resolution (only same-arity struct-method overloads by argument type resolve, TODO-5735), so it needs the lambda parameter types to drive resolution or an explicit `Function<...>` at the call; and the stored `RequestHandler` must carry the mode.
    - option B, a separately named registration: `app.view(pattern, handler)` registers a read-only handler (state passed as `[State]`) and `get`/`post`/`route` keep taking `Reference<State>`. Trade-off: trivially resolved and explicit, no overload change, but a second registration name for every method, a handler's mode is the caller's promise unless the parameter type enforces it (it would, as in A), and existing apps keep their serialized GETs until they switch to `view`.
    - option C, the method decides: every GET (and HEAD) handler is read-only and runs under the read lock, every other method writes. Trade-off: no per-handler syntax at all and every `get` benefits, but it is a breaking change for any GET handler that writes state (counters, caches, the todo_web list page if it does), and HTTP method says nothing about a handler's actual effects.
    - option D, a marker on the handler: a transform or effect on the lambda (for example `[reads]` next to the handler or an effect `state_read` that semantics checks against writes through the state parameter). Trade-off: a new checked annotation independent of overloads and method names, but a new attribute plus its checker, and the mode lives apart from the parameter type that already says it.
  - acceptance:
    - the CPU-bound GET handler of benchmarks/http (`GET /work`; 0.98x measured 2026-11-11, docs/WebAppsPlan.md, Measured) at least 2.5x `serve` with 4 workers, and todo_web's list page with 200 items (0.95x measured) at least 1.5x; a read-only handler that writes its state is a compile error
  - stop_rule: read-only handlers and `RwLock`.

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
  - status: deferred
  - created_at: 2026-11-12
  - phase: Native PrimeScroll (docs/NativeUiPlan.md; follows TODO-5586)
  - parallel_track: native-backend-strings
  - scope: The arm64 native backend answers `__pstext_stat` for chunks, bytes in use, large blocks and bytes, and blocks handed out, and `__pstext_set_limit` over the bytes of live text blocks, but `stringHeapBytesFree()` and `heapLiveBlocks()` (statistics 2 and 8) stop the program with "string heap statistic is not available on the native backend", and `StringArena` (`__pstext_arena_begin` and `_end`) is rejected at validation ("string arenas are not available on the native backend yet"). The limit counts live text-block bytes where the VM counts the bytes of its chunk regions, so a program that sits close to a limit can differ. Count the slot heap's live blocks and the free blocks' bytes in the pool, and give arenas a bump region in the data segment's state.
  - acceptance:
    - `stringMemory()`, `heapLiveBlocks()` and a `StringArena` program (`tests/unit/runtime/test_vm_heap_arenas.cpp` programs) run through `--emit=native` with the VM's output
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

- [ ] TODO-6037: A Result-returning main exits with its decoded code on wasm
  - owner: ai
  - status: deferred
  - created_at: 2026-11-18
  - phase: Correctness audit 2026-10
  - parallel_track: result-main-wasm
  - scope: The entry exit code rule (docs/spec/errors-and-file-io.md, Result-returning entry) is decoded on the VM, native, C++ and optimized C++ backends. The wasm emitter rejects the jump to the appended decode block (`unsupported control-flow pattern`), so the wasm and GLSL targets keep the raw return value. Decode the value with structured control flow (branch-free arithmetic or an in-place `if` the wasm emitter accepts) and drop the target exclusion in `IrLowererLower.cpp`.
  - acceptance:
    - the ok, error and packed cases of test_compile_run_vm_result_exit_codes.cpp also pass under wasmtime (or the wasm emitter's own run check)
    - full release gate green
  - stop_rule: entry exit codes on wasm only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-6061: Integer narrowing and widening on Wasm
  - owner: ai
  - status: deferred
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10 (follow-up of TODO-5516)
  - parallel_track: numeric-conversions
  - scope: The Wasm emitter gives every local the same value type, so `convert<i32>(x)` of an `i64` (no instruction) and `convert<i64>(x)` of an `i32` leave a value of the wrong type on the Wasm stack, and the module is invalid. Emit `i32.wrap_i64` and `i64.extend_i32_s` for the integer conversions and type the locals by the value kind, so narrowing wraps modulo 2^32 on Wasm too.
  - acceptance:
    - an emitter test shows `i32.wrap_i64` for an i64 to i32 conversion and `i64.extend_i32_s` for the reverse
    - the output validates and, with a Wasm runtime, prints what the VM prints for the conversions matrix rows
    - full release gate green
  - stop_rule: integer conversions on Wasm only.
  - notes: deferred: needs a Wasm runtime to check the output (none is available here).

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

- [ ] TODO-6080: A diagnostic column and snippet are those of the source the user wrote, not the text-filtered form
  - owner: ai
  - status: deferred
  - deferred_reason: owner picks an option
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10 (split from TODO-5520)
  - parallel_track: diagnostics-filtered-columns
  - scope: A semantic error in `[i32] b{a + 2i32 * nope}` prints line 4 column 34 and the snippet `plus(a, multiply(2i32, nope))`; `nope` is at column 22 of the file. The text filters rewrite operators into calls and keep no offset map. Options: (a) the filter records each rewrite as an edit (original range, filtered range) and spans map back through the edits; (b) the snippet shows the original line and the caret is placed by aligning identifier and literal tokens of the two lines (cheap, wrong when a rewrite reorders operands); (c) print both lines. Not chosen: it decides what the user sees.
  - acceptance:
    - an owner picks an option; then a diagnostics test asserts file, line, column and the snippet of an operator expression and a rewritten call
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
  - status: deferred
  - deferred_reason: owner picks the span
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10 (split from TODO-5520)
  - parallel_track: diagnostics-on-error-handler
  - scope: `on_error<Oops, /onErr>` with `onErr([Oops] err, [i32] extra)` passes semantics and fails in lowering (`argument count mismatch for /onErr`). The check belongs in semantics with a message that names the handler and the expected shape. Options for the span: the `on_error` transform, or the handler definition, or the first `try` that uses it (with the other two as related spans). Not chosen: it is a language-level judgment.
  - acceptance:
    - an owner picks the span; then a semantics test for a wrong arity and a wrong first parameter type, with file, line and column
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

- [ ] TODO-6068: Wasm computes `FmaF32` and `FmaF64` with one rounding
  - owner: ai
  - status: deferred
  - created_at: 2026-11-19
  - phase: Correctness audit 2026-10
  - parallel_track: stdlib-math-fma
  - depends_on: none
  - scope: The owner asked for an exact emulation on Wasm; today Wasm computes `(a * b) + c`, which rounds twice; an emulation gives the fused bits and is slower. f32: the product of two f32 is exact in an f64, so add the addend in an f64 with a round-to-odd step (TwoSum error, force the last mantissa bit odd when the error is not zero) and demote; correct because 53 >= 2 * 24 + 2. f64: no wider type, so an exact soft-float fma on i64 bit patterns (an exact 128-bit product from 32-bit limbs, an aligned add with a sticky bit, one round to nearest even, subnormals, infinities, NaN, and a product that overflows while the addend is infinite) as helper functions of the module. Note that the Wasm emitter keeps every IR local an i32 today, so a program cannot hold a float in a local on Wasm and fma only appears in a pure expression; check that before sizing this.
  - acceptance:
    - the helpers agree with `std::fma` bit for bit on a grid of operands, run under node or wasmtime from a test that skips when neither exists
    - a wasm test runs `fma` of the matrix inputs and gets the fused values (skipped when no runtime exists)
    - full release gate green
  - stop_rule: Wasm emitter only.

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

- [ ] TODO-5902: Each task of a `--vm-threads` run uses the fast kernel
  - owner: ai
  - status: deferred
  - created_at: 2026-11-11
  - phase: VM threads (docs/MultithreadingPrototype.md, VM Threads)
  - parallel_track: vm-threads
  - depends_on: TODO-5896
  - scope: The VM scheduler already runs each task's call stack on the flat loop (TODO-5870, `VmFastTask` in src/runtime/VmFastKernelProgram.h: operand stack, locals arena, frames, one per task). In threads mode each task's `VmFastTask` should run on an OS thread of its own, and the scheduler opcodes (spawn, wait, atomics, mutex, condition), which the flat loop hands to `VmTaskScheduler` today, should become real primitives. Needs scoping against `runVmFastTask` (src/runtime/VmFastKernel.cpp) before it starts; split it then.
  - acceptance:
    - a program that spawns the string-loop task of TODO-5870 on `--vm-threads` retires per task the instruction count of the same loop in a program without tasks
    - the numbers are recorded in docs/todo_log.md
    - full release gate green
  - stop_rule: threads mode only; the default schedule is the single-threaded scheduler of TODO-5870.
  - notes: deferred: scope against the fast kernel's locals arena first.

- [ ] TODO-5931: The stdlib builds as a library
  - owner: ai
  - status: deferred
  - created_at: 2026-11-11
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, section 12)
  - parallel_track: lib-stdlib
  - depends_on: TODO-5925
  - scope: Lazy import expansion already prunes the stdlib to what a program reaches, so most of the front-end cost is taken. A prebuilt `std.psa`, produced by the project's own build, would also skip parsing and validating the non-generic stdlib per compile. Measure first with the benchmark of docs/LibrarySymbolManifestLazyImports.md (an import-heavy program with a small main) and the startup lane; build it only if the measured gain is large.
  - acceptance:
    - the measured compile time of the benchmark program with and without `std.psa`, recorded in docs/todo_log.md
    - if built: the corpus of TODO-5925 passes with the stdlib linked as a library, and startup of PrimeScroll is not slower
  - stop_rule: do not build it unless the measurement shows a gain; archive this leaf as low-value otherwise.
  - notes: deferred: measure first.

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

