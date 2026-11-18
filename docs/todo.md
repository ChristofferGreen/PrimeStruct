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
| TODO-6028 | A gate at `nice -n 10` is not starved by other jobs until its tests time out | deferred | gate-timeouts |
| TODO-5738 | Sum results return through the caller's block in real calls | deferred | ir-real-calls |
| TODO-5752 | x86_64 native and the Linux JIT take the address ABI of real calls | deferred | native-real-calls |
| TODO-5824 | A null or stray local address faults on the VM | deferred | vm-addresses |
| TODO-5727 | Every struct copy site tells a place from a temporary by resolution | deferred | lifecycle-copy-sources |
| TODO-5688 | Calls require the callee's whole active effect set | deferred | effects-transitive |
| TODO-5836 | A spawned definition without a `return` transform is a `Task<void>` | deferred | mt-vm-tasks |
| TODO-5853 | A borrowed `Reference` parameter stored through a `[ref]` capture is diagnosed | deferred | closures-references |
| TODO-5854 | A `Mutex` or `Atomic` handle does not keep the value it was made from | deferred | mt-primitives |
| TODO-5862 | `serveWorkers` serves on the VM with 2 workers | deferred | mt-server |
| TODO-5868 | `serve(app)?` in an `if` block of `main` compiles | deferred | result-question |
| TODO-5804 | GET handlers that only read the state run at the same time under `RwLock` | deferred | mt-server |
| TODO-5805 | The server keeps connections alive between requests | deferred | web-keepalive |
| TODO-5806 | Sessions: a cookie and per-session state | deferred | web-sessions |
| TODO-5821 | The first toolbar of a launch does not hold up the first frame | deferred | startup-toolbar |
| TODO-5603 | The real-launch baseline, measured by a person | ready | startup |
| TODO-5956 | Native string arenas and the remaining string heap statistics | deferred | native-backend-strings |
| TODO-5536 | Small, lazily loaded bytecode for apps | deferred | bytecode-startup |
| TODO-5535 | Launch-time harness and reference Objective-C++ editor | ready | native-ui-launch |
| TODO-5533 | Windows and Linux native UI backends | deferred | native-ui-platforms |
| TODO-5716 | A literal converts to a `StringView` at a call whose overloads differ there | deferred | text-literal-views |
| TODO-5718 | All values of a repeated parameter and decoded path parameters | deferred | web-params |
| TODO-5676 | Lambdas can state their return type | deferred | closures-return-types |
| TODO-5677 | A `Function` whose closure holds a same-typed `Function` copies and destroys | deferred | closures-lifecycle |
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
| TODO-5902 | Each task of a `--vm-threads` run uses the fast kernel | deferred | vm-threads |
| TODO-5904 | The VM runs tasks on threads by default | ready | vm-threads |
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
- TODO-5535 (native-ui-launch): launch-time harness and reference Objective-C++ editor

### Immediate Next 10

1. TODO-5603
2. TODO-5535

### Priority Lanes

- Startup, first (docs/NativeUiPlan.md section 1, "Where a launch goes"): TODO-5603, TODO-5535, TODO-5821
- Native PrimeScroll, direct native backend (docs/NativeUiPlan.md): TODO-5956
- Native UI (docs/NativeUiPlan.md): TODO-5536, TODO-5533
- Effects (docs/spec/language-core.md, Effects): TODO-5688
- VM threads (docs/MultithreadingPrototype.md, VM Threads; after the startup lane): TODO-5902, TODO-5904
- Build system (docs/BuildSystemPlan.md; after the startup lane): TODO-6019
- Modules and libraries (docs/ModulesAndLibrariesPlan.md; after the startup lane): TODO-5931, TODO-5932, TODO-5933, TODO-6032
- Result and control flow (docs/spec/errors-and-file-io.md): TODO-5515, TODO-5523, TODO-5525, TODO-5526
- Memory safety (docs/spec/type-system.md Memory safety): TODO-5511, TODO-5512, TODO-5513
- Backend parity: TODO-5516, TODO-5517, TODO-5744
- Frontend and diagnostics: TODO-5518, TODO-5519, TODO-5520, TODO-5521
- macOS support: TODO-5543, TODO-5545 (need macOS)
- Language gaps found by native UI: TODO-5544
- Closures (docs/spec/value-lifecycle.md, Lambdas): TODO-5676, TODO-5677, TODO-5853; collection fields found by closures and the todo example: TODO-5727
- Web apps (docs/WebAppsPlan.md; built with `--emit=optexe`): TODO-5718; deferred TODO-5738
- Language surface for handlers (docs/WebAppsPlan.md): TODO-5716
- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix; arm64 SextI32 TODO-5483 (needs macOS); VM speed ; passes ; real calls TODO-5752; VM address checks TODO-5824
- Multithreading (docs/MultithreadingPrototype.md, Threads For Real; docs/WebAppsPlan.md, Concurrency; after the startup lane): TODO-5872; deferred TODO-5804, TODO-5805, TODO-5806, TODO-5836, TODO-5854, TODO-5862
- Code reuse (docs/CodeReusePlan.md; two at a time under rule 18): TODO-5973; deferred TODO-5976, TODO-5977

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

Multithreading (docs/MultithreadingPrototype.md, Threads For Real), after the startup lane (TODO-5772 is done); not above them in Ready Now or Immediate Next 10. In this order (TODO-5792 and TODO-5835, VM tasks and their lowering, are done, and so are TODO-5793, byte-region checks without a map search, TODO-5794, `optexe` per-thread heap arenas, TODO-5795, `optexe` threads for `[spawn]`, `Atomic<i32>` and `Atomic<i64>` in `/std/sync`, TODO-5797, `Mutex<T>` with `locked(body)`, TODO-5846, `optexe` faults when every task waits, TODO-5798, bounded `Channel<T>`, TODO-5656, the atomic `SharedString` owner count, TODO-5802, `serveWorkers`, and TODO-5803, the apps on workers, measured, and the `SharedString` count inside the text block, are done): arm64 native is done: TODO-5799 (pthreads; TODO-5942, TODO-5943), TODO-5800 (per-thread heaps) and TODO-5801 (atomics, `Mutex`, `Channel`; TODO-5950, TODO-5951). TODO-5799 is done, and TODO-5893 (an unused `/std/tuple/*` import is reported as an unknown import path) is done too, and TODO-5894 (tuple destructuring and the destructured multi-wait inside a nested block are expanded; TODO-5849, each destructured multi-wait binding has its own result type, is done) is done too, and TODO-5847 (spawned calls with builtin `map<K, V>` or owned sum parameters or results run as tasks, done with TODO-5994, `wait(t).count()` of a stdlib `Vector` result waits twice, is done; TODO-5992, builtin `vector<T>` parameters and results, TODO-5993, the stdlib `Vector` and `Map` structs, TODO-5845, the same for `String`, struct and closure values, and TODO-5895, `copy` and `move` scalars and structs, are done) (TODO-5807, destructured multi-wait, TODO-5790, sendable spawn arguments, and TODO-5791, every `Function` sendable by construction, and TODO-5848, `return(wait(t))` of a `String` task result, are done). Deferred: TODO-5804 (read-only handlers under `RwLock`), TODO-5805 (keep-alive), TODO-5806 (sessions), TODO-5854 (a `Mutex` or `Atomic` handle keeps a copy of its first value). Found with `Mutex` (TODO-5852, `wait(a, b)` of two `Task<void>`, is done) in the closures lane: TODO-5934 (a `Reference`-returning call read as a value yields its referent, found with TODO-5850), TODO-5850 (a `Reference`-returning call as a closure argument) and TODO-5851 (a `[ref]` closure written in a generic call) are done. Found with `Channel` and ready, in the Result and control flow lane: TODO-5855 (a method call on a `pick` payload of struct type), TODO-5856 (`pick` on a call result frees an owning payload), TODO-5857 (a `vector<T>` payload reads its count in a `pick` arm), TODO-5858 (`some<Vector<i32>>` of a fresh vector frees it), TODO-5859 (a `pick` expression yields a vector), TODO-5860 (moving a payload binding does not poison a later binding of the same name), TODO-5866 (a closure captures a `pick` payload binding), TODO-5877 (a field an arm of a `pick` expression yields is copied), TODO-5879 (a `pick` expression's result owns a value of its own), TODO-5880 (a payload moved out of a sum local leaves the local moved-from, and a payload of a borrowed sum cannot be moved or returned), TODO-5873 (a sum temporary passed as an argument is destroyed after the call), TODO-5874 (assigning to a `mut` sum local destroys the payload it held), TODO-5883 (assigning to a `mut` builtin vector local destroys what it held and copies a place), TODO-5876 (`return(pick(...))` of a vector and assignment from a `pick` expression), TODO-5875 (`count()` on a local declared `Vector<T>` lowers), TODO-5878 (a `[move name]` lambda capture compiles), TODO-5884 (assignment to a `mut` vector parameter and to an inferred vector-of-struct local keeps one owner), TODO-5882 (a vector of sums destroys the payloads of its elements), TODO-5885 (a vector temporary that is only read is destroyed), TODO-5889 (`a = move(b)` of a vector local takes the vector over), TODO-5890 (the arms of an assigned `pick` may open on the next line), TODO-5881 (a call takes a call to the same definition as its argument on the VM), TODO-5888 (an element of a vector temporary read as a struct or a `String`) and TODO-5887 (`remove_at` and `remove_swap` compile on a vector of sums) are done, and found with them: TODO-5886 (`push` on a local declared `Vector<T>` resolves) is done, and TODO-5978 (a `Vector<String>` leaks what `pop` and `clear` remove and faults after `remove_at`; its element reads by `at` and `[]`) is done, and found with it: TODO-5991 (a field read of a struct a call returns leaves the temporary's copy undestroyed) is done; TODO-5891 (a returned vector field or vector element of a local is a copy) and TODO-5892 (a block expression whose value is an owning local hands it over once) are done. Found with `serveWorkers` (TODO-5865, a `Function` local given positionally to a brace constructor, TODO-5864, a lambda in a generic definition naming its type parameters, and TODO-5935, a lambda parameter naming a struct of its namespace, are done): TODO-5862 (deferred: `serveWorkers` on the VM, which has no `/std/net`), and ready: TODO-5866 (a closure captures a `pick` payload binding). Found measuring the apps on workers (TODO-5803, done): TODO-5867 (a string heap that alternates between block sizes keeps its chunks) and TODO-5869 (1 worker as fast as `serve`; the 32 KB form reached 2.41x with 4 workers, not 2.5x) are done; TODO-5940 (a load generator that costs less per connection than the servers it measures, done: compiled, 1.4 us of user CPU per request, and the kernel's loopback setup is the ceiling; TODO-5871 found every server held to about 14,600 new connections a second by the generator and the kernel's loopback setup, which caps the form with 4 workers at 2.41x); TODO-5871 (that measurement), TODO-5872 (a byte of another thread's block is checked on a slow path, so the worker parses from a copy) and TODO-5870 (a VM program that uses tasks runs its string code on the fast kernel) are done; and in the Result and control flow lane TODO-5868 (`serve(app)?` in an `if` block of `main` is rejected; deferred: it is the documented rule that a handler does not flow into nested blocks, so a decision comes first; TODO-5936, a diagnostic in `main` of a program that imports `/std/http/server` reported a stdlib position, found with it, is done).

VM threads (docs/MultithreadingPrototype.md, VM Threads), after the startup lane: the VM gets a threads mode next to its deterministic scheduler. TODO-5896 (`--vm-threads`, a thread and a heap arena per task) and TODO-5897 (Atomic, Mutex and Channel on real primitives, with the deadlock fault) and TODO-5898 (reentrant and main-thread-only host functions) and TODO-5899 (`RunOptions::threads`, fault and cancel through the embed API) and TODO-5900 (the concurrent corpus repeated under `--vm-threads` and under ThreadSanitizer) and TODO-5901 (`--vm-record-sync` and `--vm-replay-sync`, the synchronization order of a threaded run replayed on the deterministic scheduler) landed; ready: TODO-5904 (threads by default); deferred TODO-5902 (the fast kernel per task). TODO-5903 (debug sessions run programs with tasks, the first prerequisite of docs/VmInspectorPlan.md) landed.

Build system (docs/BuildSystemPlan.md), after the startup lane: TODO-5906 (directory listing and file metadata, `/std/dir`) and TODO-5915 (incremental builds) landed. TODO-5907 (glob, `/std/glob`) and TODO-6009 (a changed glob listing re-runs a step) and TODO-5918 (staged builds, `Graph.run` and the `build_run` effect) landed; TODO-5916 (subprojects, `b.subproject`) and TODO-5917 (options, `b.option`) landed; TODO-5920 (PrimeScroll builds with `make.prime`) landed; TODO-5919 (function steps, `b.step`) landed; ready: TODO-6019 (person measures startup); and the modules and libraries lane, which splits static libraries (docs/ModulesAndLibrariesPlan.md): TODO-5922 (`primevm --link` merges two IR modules) landed; TODO-6031 (lowering marks string literal pushes, so a library's strings always relocate) landed; TODO-5923 (`--emit=interface`) landed; TODO-5924 (`--emit=lib`) landed; TODO-5925 (`--library` and the link in the pipeline) landed; TODO-5926 (generics and lifecycle helpers cross the library boundary) landed; TODO-5927 (namespace ownership) landed; TODO-5928 (`--emit-uses`) landed; TODO-5929 (symbol-level rebuilds) landed; TODO-5930 (the graph builds libraries and links them) landed; deferred TODO-5931 (the stdlib as a library, measure first), TODO-5932 (libraries that carry back-end code), TODO-5933 (the embed API loads a library), TODO-6032 (public functions of any shape in a library).


Code reuse (docs/CodeReusePlan.md, section 17 for how to work it): these leaves are `ready` outside `Ready Now` because they are worked by their own pair of agents under rule 18. Start with TODO-5959 (delete the AST C++ and GLSL emitters) and TODO-5960 (one implementation of the execution and definition intra-body diagnostics), which touch different files. Then, as each lands: TODO-5961 (one type-text parser, after TODO-5959), TODO-5962 (one builtin-name classifier, after TODO-5961), TODO-5964 (semantics key-value predicates, after TODO-5960), TODO-5966 (binding validation as rules, not phases) then TODO-5967 and TODO-5968 (the same for template rewrites and call validation). Independent and ready at any time: TODO-5963 (lowerer collection spellings), TODO-5965 (reflection AST builders), TODO-5969 (table-driven float math lowering), TODO-5970 and TODO-5971 (semantic-product fact plumbing), TODO-5972 (lowerer helpers copied per unit), TODO-5973 (SoA rewrite walker), TODO-5974 (shared string, hash, JSON and C-literal helpers), TODO-5975 (opcode categories in the opcode table). Each block's `notes` names the leaves it must not run alongside. Deferred: TODO-5976 (parsed types in the semantic product) and TODO-5977 (one diagnostics builder).

### Task Blocks

- [ ] TODO-6028: A gate at `nice -n 10` is not starved by other jobs until its tests time out
  - owner: ai
  - status: deferred
  - created_at: 2026-11-17
  - phase: Test infrastructure (found by TODO-6026; needs a decision on `scripts/compile.sh`)
  - parallel_track: gate-timeouts
  - scope: `scripts/compile.sh` runs `ctest` at `nice -n 10` (TODO-5757), so on a machine with other jobs at normal priority the gate's tests get a small share of the CPUs. TODO-6026 measured the same five entries alone and with 44 and 60 busy-loop processes at normal priority (11 cores): at `nice -n 10` they took 5 to 15 times their quiet time, at normal priority 2 to 5 times, so a 300 s timeout is about 20 to 60 times a quiet entry but at a load of 100 and nice 10 only 10 to 20. Timeouts of 300 s that were reached by entries of 2 to 40 s need something beyond load (memory pressure with several 2 GB compilers at once, or a stalled Gatekeeper or disk), which no loaded repro in TODO-6026 produced. Decide with the owner whether the gate should run at normal priority when the load is high, then, if so, implement it in `scripts/compile.sh` (the stability rule there forbids changing it without the owner asking) and measure a loaded gate before and after.
  - acceptance:
    - the owner has said whether `compile.sh` may change; if so, a loaded full gate (load above 40) is recorded with and without the change and no entry times out in the change's run
    - if the owner declines, this leaf is deleted with the numbers kept in docs/todo_log.md
  - stop_rule: the priority of the gate only; no timeout changes.

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

- [ ] TODO-5738: Sum results return through the caller's block in real calls
  - owner: ai
  - status: deferred
  - created_at: 2026-11-08
  - phase: Web apps (docs/WebAppsPlan.md; found while finishing TODO-5734)
  - parallel_track: ir-real-calls
  - scope: A definition whose result is a sum (`Result`, `Maybe`, a user `[sum]`) is still inlined at every call site on the VM and optcpp/optexe, because the sum return path (`emitReturnSumStorage`) has no real-call form. `/std/http/server` `serve` and `serveUntil` return `Result<ServeError>`, so todo_web's `main` holds both bodies (about 260 KB of its 1.38 MB of C++). Return sums through the caller's result block like structs (docs/spec/vm-design.md, Real calls).
  - acceptance:
    - `serve`/`serveUntil` are real calls in todo_web; a matrix case returns `Result` and `Maybe` values (ok, error, a payload with a `Destroy`) from a real call on every backend
    - full release gate green
  - stop_rule: sum results of real calls only.
  - findings: Measured 2026-11-12 (TODO-5741 and TODO-5739 landed first). Two facts change the case for this task. (1) `serve` and `serveUntil` are called once, from todo_web's `main`, so inlining them duplicates nothing: a prototype that made them real calls (`Result<ServeError>` is a packed scalar there, because todo_web does not import `/std/result`; the shape check rejects it only because `hasScalarOrVoidReturn` reads the text `Result<ServeError>` as non-scalar, and the callee body needs `entryHasResultInfo` set from its `ReturnInfo`) grew `--emit=optcpp` from 1,563,197 to 1,588,608 bytes, and clang++ -O2 took 3.3 s before and 3.5 s after, so the 260 KB sits in `main` either way. The task has no size outcome until a definition with a `Result` or `Maybe` result has two or more call sites. (2) With `/std/result` imported a `Result<...>` is sum storage, which a real call must return through a block of the caller's frame: `emitReturnSumStorage` (src/ir_lowerer/IrLowererLowerStatementsBindingsContinued.h) copies the sum into the block (`realCallStructReturnLocal`) and returns its address, the call site (src/ir_lowerer/IrLowererLowerInlineCalls.cpp) allocates the block from `SumHelpersContext::loweredSumSlotCount`, and the sum definition has to come from the semantic product's return fact when the declared text does not resolve; the aggregate return path (`return(other_call())`) already copies into the block. Pick this up when a program with a multi-call-site `Result` or `Maybe` function makes it pay; start from the packed case (no `/std/result`), then sums, then `Maybe` and user `[sum]`.

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

- [ ] TODO-5868: `serve(app)?` in an `if` block of `main` compiles
  - owner: ai
  - status: deferred
  - created_at: 2026-11-11
  - phase: Result and control flow (docs/spec/errors-and-file-io.md; found with benchmarks/http/bench_server.prime)
  - parallel_track: result-question
  - scope: With `[return<int> effects(heap_alloc, io_err, net_io) on_error<ServeError, log_serve_error>] main()`, the body `if(flag) { serve(app_router(), config)? return(0i32) } serve(app_router(), config)? return(0i32)` (`app_router()` returns an `AppRouter<Bench>`) is rejected with `missing on_error for ? usage`, reported at stdlib/std/http/http.prime in `/std/http/AppRouter__t.../call`, a definition that has no `?`. The same two `serve(...)?` statements one after the other compile, and so does the `if` when each branch calls a helper that carries its own `on_error`. Not yet narrowed: whether any `?` in a block does this or only one whose call takes a generic struct. Make the `?` inside the block use the enclosing definition's `on_error`, and report a missing one at the `?`.
  - findings: the rejection is the documented rule, not a defect of generic structs. docs/spec/errors-and-file-io.md (Local handlers) says handlers do not flow into nested blocks and that any nested block using `?` must declare its own `on_error`; `SemanticsValidatorStatementControlFlow.cpp` clears the handler (`OnErrorScope onErrorScope(*this, std::nullopt)`) in every `if` branch, loop body and block-argument body, and `SemanticsValidatorStatement.cpp` installs a block's own `on_error`. A plain `[return<int> effects(io_out) on_error<i32, /on_err>] main()` whose `if` branch holds `run(3i32)?` is rejected the same way with a program that imports nothing from `/std/http`; so the "`?` in an `if` block of `main`" in the scope is that rule, and `/std/http` only makes the diagnostic point at the wrong place (TODO-5936). Making the definition's handler flow into `if` branches and loops would reverse a normative rule (the handler runs and the definition returns, which in a loop body or a block with its own cleanup is a design question), so it is not done here.
  - decision_needed: either keep the rule and make a block-level handler usable where `main` needs it (the statement form `[on_error<ServeError, log_serve_error>] block() { ... }` is spelled in the spec and tested at the top level of a definition, requires the block to return `Result` or `int` (`on_error requires Result or int return type on /main/block`), and written inside an `if` branch it fails today with `omitted initializer requires vector or soa type: on_error`, which is a second finding), or amend the spec so a definition's handler also covers the `if` branches (not loops) of its own body. Either is a language decision; the leaf stays deferred until it is made.
  - implementation_notes: benchmarks/http/bench_server.prime works around it with `run_serve` and `run_workers`; inline them into `main` to reproduce.
  - acceptance:
    - positive: the program compiles and runs on the VM and `optexe`; bench_server.prime drops its two helpers
    - negative: a `?` in an `if` block of a definition without `on_error` is reported at that `?`, naming that definition
    - full release gate green
  - stop_rule: `?` inside statement blocks with `on_error` on the enclosing definition.

- [ ] TODO-5804: GET handlers that only read the state run at the same time under `RwLock`
  - owner: ai
  - status: deferred
  - created_at: 2026-11-09
  - phase: Multithreading and Web apps (docs/MultithreadingPrototype.md, The Server)
  - parallel_track: mt-server
  - scope: Under `serveWorkers` every handler holds the app's mutex, so page rendering runs one request at a time. Add `RwLock<T>` (`read(body)` with a read-only `T`, `write(body)` with a `Reference<T>`), let a handler say it only reads (its state parameter `[State] state` against `[Reference<State>] state`, which needs same-arity overloads by argument type (struct methods resolve them since TODO-5735); or a named registration, open question 5 of the design), and run read-only handlers under the read lock. Deferred: decide the registration form first.
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

- [ ] TODO-5904: The VM runs tasks on threads by default
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: VM threads (docs/MultithreadingPrototype.md, VM Threads)
  - parallel_track: vm-threads
  - scope: `primevm` and `primec --emit=vm` run tasks on threads unless `--vm-deterministic` (or `--vm-schedule-seed`, or a debug session) selects the deterministic scheduler. The conformance tests that need a fixed order pass the flag. docs/spec/vm-design.md and the VM Threads section become normative text for the new default.
  - acceptance:
    - the full release gate is green with the new default
    - a program that does not spawn prints and starts exactly as before
    - docs/MultithreadingPrototype.md and docs/spec/vm-design.md state the default
  - stop_rule: the default and the flag only.

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
