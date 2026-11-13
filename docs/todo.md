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
| TODO-5738 | Sum results return through the caller's block in real calls | deferred | ir-real-calls |
| TODO-5752 | x86_64 native and the Linux JIT take the address ABI of real calls | deferred | native-real-calls |
| TODO-5824 | A null or stray local address faults on the VM | deferred | vm-addresses |
| TODO-5727 | Every struct copy site tells a place from a temporary by resolution | deferred | lifecycle-copy-sources |
| TODO-5688 | Calls require the callee's whole active effect set | deferred | effects-transitive |
| TODO-5836 | A spawned definition without a `return` transform is a `Task<void>` | deferred | mt-vm-tasks |
| TODO-5837 | VM debug sessions step programs that spawn tasks | deferred | vm-debugger |
| TODO-5844 | Short-string `optexe` benchmarks take the cycles they took before the heap arenas | ready | optexe-bytes |
| TODO-5938 | A byte loop over one large region on `optexe` takes the cycles it took before the granule table | ready | optexe-bytes |
| TODO-5847 | A spawned call with a collection parameter or result runs as a task | ready | mt-vm-tasks |
| TODO-5853 | A borrowed `Reference` parameter stored through a `[ref]` capture is diagnosed | deferred | closures-references |
| TODO-5854 | A `Mutex` or `Atomic` handle does not keep the value it was made from | deferred | mt-primitives |
| TODO-5862 | `serveWorkers` serves on the VM with 2 workers | deferred | mt-server |
| TODO-5868 | `serve(app)?` in an `if` block of `main` compiles | deferred | result-question |
| TODO-5940 | The HTTP load generator costs less CPU per connection than the servers it measures | ready | mt-server |
| TODO-5978 | An element of a `Vector<String>` is read by `at` and `[]` | ready | pick-payloads |
| TODO-5804 | GET handlers that only read the state run at the same time under `RwLock` | deferred | mt-server |
| TODO-5805 | The server keeps connections alive between requests | deferred | web-keepalive |
| TODO-5806 | Sessions: a cookie and per-session state | deferred | web-sessions |
| TODO-5821 | The first toolbar of a launch does not hold up the first frame | deferred | startup-toolbar |
| TODO-5603 | The real-launch baseline, measured by a person | ready | startup |
| TODO-5956 | Native string arenas and the remaining string heap statistics | deferred | native-backend-strings |
| TODO-5536 | Small, lazily loaded bytecode for apps | deferred | bytecode-startup |
| TODO-5535 | Launch-time harness and reference Objective-C++ editor | ready | native-ui-launch |
| TODO-5808 | `ps_ui_*` entry points fault when called off the main thread | ready | native-ui-threads |
| TODO-5533 | Windows and Linux native UI backends | deferred | native-ui-platforms |
| TODO-5716 | A literal converts to a `StringView` at a call whose overloads differ there | deferred | text-literal-views |
| TODO-5718 | All values of a repeated parameter and decoded path parameters | deferred | web-params |
| TODO-5728 | `slice` and `sliceMut` on a vector field of a struct resolve | ready | collections-access |
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
| TODO-5896 | `--vm-threads` runs each spawned task on its own OS thread | ready | vm-threads |
| TODO-5897 | Atomic, Mutex and Channel run on real primitives under `--vm-threads` | blocked | vm-threads |
| TODO-5898 | Host calls under `--vm-threads` serialize by default and block only their own task | blocked | vm-threads |
| TODO-5899 | A fault or a cancel stops every thread of a `--vm-threads` run without ending the process | blocked | vm-threads |
| TODO-5900 | The concurrent corpus runs repeatedly under `--vm-threads` and under ThreadSanitizer | blocked | vm-threads |
| TODO-5901 | A threaded VM run records its synchronization order and replays it on the deterministic scheduler | blocked | vm-threads |
| TODO-5902 | Each task of a `--vm-threads` run uses the fast kernel | deferred | vm-threads |
| TODO-5903 | A debug session runs programs that spawn tasks | ready | vm-debug-tasks |
| TODO-5904 | The VM runs tasks on threads by default | blocked | vm-threads |
| TODO-5905 | `/std/path` joins, splits and normalizes paths | ready | build-path |
| TODO-5906 | `/std/file` lists a directory and reports kind, size and modification time | ready | file-listing |
| TODO-5907 | `/std/file` expands glob patterns | blocked | build-glob |
| TODO-5908 | `--allow-effects` rejects declared effects outside a list in user code | ready | effect-ceiling |
| TODO-5909 | `--allow-import` rejects imports outside a list of prefixes | ready | import-allowlist |
| TODO-5910 | `--profile build` sets the defaults of a build script | blocked | build-profile |
| TODO-5911 | `/std/build` builds a graph value and exports it as JSON | ready | build-graph |
| TODO-5912 | `primec build --dry-run` prints the graph of `make.prime` | blocked | build-cli |
| TODO-5913 | `primec build` runs the graph | blocked | build-runner |
| TODO-5914 | `--emit-deps` writes the files a compile read | ready | build-deps |
| TODO-5915 | `primec build` skips steps whose inputs did not change | blocked | build-cache |
| TODO-5916 | `b.subproject` merges the graph of a subfolder's `make.prime` | blocked | build-subprojects |
| TODO-5917 | Build options: `b.option` and `primec build --option` | blocked | build-options |
| TODO-5918 | A script can run its graph in stages | blocked | build-stages |
| TODO-5919 | A build step can be a PrimeStruct function that runs on a VM thread | blocked | build-function-steps |
| TODO-5920 | PrimeScroll builds with `make.prime` | blocked | build-dogfood |
| TODO-5922 | `primevm --link` merges two serialized IR modules | ready | lib-ir-link |
| TODO-5923 | `primec --emit=interface` prints a module's interface and symbol table | ready | lib-interface |
| TODO-5924 | `--emit=lib` writes a library archive from every public definition of a module | blocked | lib-archive |
| TODO-5925 | `--library` compiles a program against a library and links it | blocked | lib-consume |
| TODO-5926 | Generic definitions and synthesized helpers cross the library boundary | blocked | lib-generics |
| TODO-5927 | A library owns its namespace roots | blocked | lib-ownership |
| TODO-5928 | `--emit-uses` writes the library symbols a compile resolved | blocked | lib-uses |
| TODO-5929 | The runner rebuilds a dependent only when a symbol it used changed | blocked | lib-cutoff |
| TODO-5930 | `primec build` builds libraries and links them | blocked | lib-build |
| TODO-5931 | The stdlib builds as a library | deferred | lib-stdlib |
| TODO-5932 | Libraries carry back-end code | deferred | lib-backend |
| TODO-5933 | The embed API loads a library at run time | deferred | lib-embed |
| TODO-5962 | Builtin math and control-flow call names are classified once, with one rule | blocked | reuse-builtin-names |
| TODO-5989 | Control-flow call names, simple-call-name tests and return kinds are classified once | ready | reuse-builtin-names |
| TODO-5965 | Reflection-generated helpers build AST nodes through one builder set | ready | reuse-ast-builders |
| TODO-5987 | The lowerer's include-only fragments become components | ready | lowerer-fragments |
| TODO-5984 | The http web apps lower in seconds, not minutes | ready | web-app-lowering-time |
| TODO-5970 | Publication fact snapshots run one templated merge per fact kind | ready | reuse-semantic-facts |
| TODO-5971 | Semantic product lookups are one template over index and fact members | ready | reuse-semantic-facts |
| TODO-5973 | SoA rewrite passes share one context and one walker | ready | reuse-soa-rewrites |
| TODO-5976 | The semantic product publishes parsed types, and the lowerer stops re-parsing type text | deferred | reuse-type-model |
| TODO-5977 | Semantics diagnostics go through one builder | deferred | reuse-diagnostic-builder |
| TODO-5979 | `--collect-diagnostics` reports no false intra-body call diagnostics | ready | collect-diagnostics-false-positives |

### Ready Now

- TODO-5603 (startup): the real-launch baseline, measured by a person
- TODO-5535 (native-ui-launch): launch-time harness and reference Objective-C++ editor
- TODO-5510 (control-returns): `return` returns from unit-variant pick arms correctly
- TODO-5524 (result-error-structs): int-backed error structs round-trip through stdlib Result sums
- TODO-5841 (namespace-envelope): `[namespace] name { ... }` is a definition
- TODO-5906 (file-listing): `/std/file` lists a directory and reports kind, size and modification time
- TODO-5911 (build-graph): `/std/build` builds a graph value and exports it as JSON
- TODO-5908 (effect-ceiling): `--allow-effects` rejects declared effects outside a list in user code

### Immediate Next 10

1. TODO-5603
2. TODO-5535
3. TODO-5510
4. TODO-5524
5. TODO-5728
6. TODO-5841
7. TODO-5906
8. TODO-5911
9. TODO-5908

### Priority Lanes

- Startup, first (docs/NativeUiPlan.md section 1, "Where a launch goes"): TODO-5603, TODO-5535, TODO-5821
- Native PrimeScroll, direct native backend (docs/NativeUiPlan.md): TODO-5956
- Native UI (docs/NativeUiPlan.md): TODO-5808, TODO-5536, TODO-5533
- Effects (docs/spec/language-core.md, Effects): TODO-5688
- AI workflow (docs/AiWorkflowPlan.md): TODO-5841, TODO-5843, TODO-5842
- VM threads (docs/MultithreadingPrototype.md, VM Threads; after the startup lane): TODO-5896, TODO-5897, TODO-5898, TODO-5899, TODO-5900, TODO-5901, TODO-5902, TODO-5903, TODO-5904
- Build system (docs/BuildSystemPlan.md; after the startup lane): TODO-5905, TODO-5906, TODO-5907, TODO-5908, TODO-5909, TODO-5910, TODO-5911, TODO-5912, TODO-5913, TODO-5914, TODO-5915, TODO-5916, TODO-5917, TODO-5918, TODO-5919, TODO-5920
- Modules and libraries (docs/ModulesAndLibrariesPlan.md; after the startup lane): TODO-5922, TODO-5923, TODO-5924, TODO-5925, TODO-5926, TODO-5927, TODO-5928, TODO-5929, TODO-5930, TODO-5931, TODO-5932, TODO-5933
- Result and control flow (docs/spec/errors-and-file-io.md): TODO-5510, TODO-5524, TODO-5515, TODO-5523, TODO-5525, TODO-5526, TODO-5978
- Memory safety (docs/spec/type-system.md Memory safety): TODO-5511, TODO-5512, TODO-5513
- Backend parity: TODO-5516, TODO-5517, TODO-5744
- Frontend and diagnostics: TODO-5518, TODO-5519, TODO-5520, TODO-5521
- macOS support: TODO-5543, TODO-5545 (need macOS)
- Language gaps found by native UI: TODO-5544
- Closures (docs/spec/value-lifecycle.md, Lambdas): TODO-5676, TODO-5677, TODO-5853; collection fields found by closures and the todo example: TODO-5682, TODO-5727, TODO-5728
- Web apps (docs/WebAppsPlan.md; built with `--emit=optexe`): TODO-5718; deferred TODO-5738
- Language surface for handlers (docs/WebAppsPlan.md): TODO-5716
- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix; arm64 SextI32 TODO-5483 (needs macOS); VM speed ; passes ; optexe byte loops TODO-5844, TODO-5938; real calls TODO-5752; VM address checks TODO-5824
- Multithreading (docs/MultithreadingPrototype.md, Threads For Real; docs/WebAppsPlan.md, Concurrency; after the startup lane): TODO-5847, TODO-5872; deferred TODO-5804, TODO-5805, TODO-5806, TODO-5836, TODO-5837, TODO-5854, TODO-5862
- Code reuse (docs/CodeReusePlan.md; two at a time under rule 18): TODO-5962, TODO-5965, TODO-5970, TODO-5971, TODO-5973; deferred TODO-5976, TODO-5977

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

Multithreading (docs/MultithreadingPrototype.md, Threads For Real), after the startup lane (TODO-5772 is done); not above them in Ready Now or Immediate Next 10. In this order (TODO-5792 and TODO-5835, VM tasks and their lowering, are done, and so are TODO-5793, byte-region checks without a map search, TODO-5794, `optexe` per-thread heap arenas, TODO-5795, `optexe` threads for `[spawn]`, `Atomic<i32>` and `Atomic<i64>` in `/std/sync`, TODO-5797, `Mutex<T>` with `locked(body)`, TODO-5846, `optexe` faults when every task waits, TODO-5798, bounded `Channel<T>`, TODO-5656, the atomic `SharedString` owner count, TODO-5802, `serveWorkers`, and TODO-5803, the apps on workers, measured, and the `SharedString` count inside the text block, are done): arm64 native is done: TODO-5799 (pthreads; TODO-5942, TODO-5943), TODO-5800 (per-thread heaps) and TODO-5801 (atomics, `Mutex`, `Channel`; TODO-5950, TODO-5951). TODO-5799 is done, and TODO-5893 (an unused `/std/tuple/*` import is reported as an unknown import path) is done too, and TODO-5894 (tuple destructuring and the destructured multi-wait inside a nested block are expanded; TODO-5849, each destructured multi-wait binding has its own result type, is done) is done too, and TODO-5847 (spawned calls with collection parameters or results run as tasks, which today run inline; TODO-5845, the same for `String`, struct and closure values, and TODO-5895, `copy` and `move` scalars and structs, are done) (TODO-5807, destructured multi-wait, TODO-5790, sendable spawn arguments, and TODO-5791, every `Function` sendable by construction, and TODO-5848, `return(wait(t))` of a `String` task result, are done). Deferred: TODO-5804 (read-only handlers under `RwLock`), TODO-5805 (keep-alive), TODO-5806 (sessions), TODO-5854 (a `Mutex` or `Atomic` handle keeps a copy of its first value). Found with `Mutex` (TODO-5852, `wait(a, b)` of two `Task<void>`, is done) in the closures lane: TODO-5934 (a `Reference`-returning call read as a value yields its referent, found with TODO-5850), TODO-5850 (a `Reference`-returning call as a closure argument) and TODO-5851 (a `[ref]` closure written in a generic call) are done. Found with `Channel` and ready, in the Result and control flow lane: TODO-5855 (a method call on a `pick` payload of struct type), TODO-5856 (`pick` on a call result frees an owning payload), TODO-5857 (a `vector<T>` payload reads its count in a `pick` arm), TODO-5858 (`some<Vector<i32>>` of a fresh vector frees it), TODO-5859 (a `pick` expression yields a vector), TODO-5860 (moving a payload binding does not poison a later binding of the same name), TODO-5866 (a closure captures a `pick` payload binding), TODO-5877 (a field an arm of a `pick` expression yields is copied), TODO-5879 (a `pick` expression's result owns a value of its own), TODO-5880 (a payload moved out of a sum local leaves the local moved-from, and a payload of a borrowed sum cannot be moved or returned), TODO-5873 (a sum temporary passed as an argument is destroyed after the call), TODO-5874 (assigning to a `mut` sum local destroys the payload it held), TODO-5883 (assigning to a `mut` builtin vector local destroys what it held and copies a place), TODO-5876 (`return(pick(...))` of a vector and assignment from a `pick` expression), TODO-5875 (`count()` on a local declared `Vector<T>` lowers), TODO-5878 (a `[move name]` lambda capture compiles), TODO-5884 (assignment to a `mut` vector parameter and to an inferred vector-of-struct local keeps one owner), TODO-5882 (a vector of sums destroys the payloads of its elements), TODO-5885 (a vector temporary that is only read is destroyed), TODO-5889 (`a = move(b)` of a vector local takes the vector over), TODO-5890 (the arms of an assigned `pick` may open on the next line), TODO-5881 (a call takes a call to the same definition as its argument on the VM), TODO-5888 (an element of a vector temporary read as a struct or a `String`) and TODO-5887 (`remove_at` and `remove_swap` compile on a vector of sums) are done, and found with them: TODO-5886 (`push` on a local declared `Vector<T>` resolves) is done, and TODO-5978 (a `Vector<String>` leaks what `pop` and `clear` remove and faults after `remove_at`) is ready; TODO-5891 (a returned vector field or vector element of a local is a copy) and TODO-5892 (a block expression whose value is an owning local hands it over once) are done. Found with `serveWorkers` (TODO-5865, a `Function` local given positionally to a brace constructor, TODO-5864, a lambda in a generic definition naming its type parameters, and TODO-5935, a lambda parameter naming a struct of its namespace, are done): TODO-5862 (deferred: `serveWorkers` on the VM, which has no `/std/net`), and ready: TODO-5866 (a closure captures a `pick` payload binding). Found measuring the apps on workers (TODO-5803, done): TODO-5867 (a string heap that alternates between block sizes keeps its chunks) and TODO-5869 (1 worker as fast as `serve`; the 32 KB form reached 2.41x with 4 workers, not 2.5x) are done; ready: TODO-5940 (a load generator that costs less per connection than the servers it measures: TODO-5871 found every server held to about 14,600 new connections a second by the generator and the kernel's loopback setup, which caps the form with 4 workers at 2.41x); TODO-5871 (that measurement), TODO-5872 (a byte of another thread's block is checked on a slow path, so the worker parses from a copy) and TODO-5870 (a VM program that uses tasks runs its string code on the fast kernel) are done; and in the Result and control flow lane TODO-5868 (`serve(app)?` in an `if` block of `main` is rejected; deferred: it is the documented rule that a handler does not flow into nested blocks, so a decision comes first; TODO-5936, a diagnostic in `main` of a program that imports `/std/http/server` reported a stdlib position, found with it, is done).

VM threads (docs/MultithreadingPrototype.md, VM Threads), after the startup lane: the VM gets a threads mode next to its deterministic scheduler. In this order: TODO-5896 (`--vm-threads`, a thread and a heap arena per task) is ready; then TODO-5897 (Atomic, Mutex and Channel on real primitives), TODO-5898 (host-call locking), TODO-5899 (fault and cancel without ending the process), TODO-5900 (the concurrent corpus repeated and under ThreadSanitizer), TODO-5901 (record and replay of synchronization order), TODO-5904 (threads by default); deferred TODO-5902 (the fast kernel per task). TODO-5903 (debug sessions run programs with tasks) is ready and independent; it is the first prerequisite of docs/VmInspectorPlan.md.

Build system (docs/BuildSystemPlan.md), after the startup lane: ready and independent of each other are TODO-5905 (`/std/path`), TODO-5906 (directory listing and file metadata), TODO-5908 (`--allow-effects`), TODO-5909 (`--allow-import`), TODO-5911 (the graph value and its JSON export) and TODO-5914 (`--emit-deps`). Then TODO-5907 (glob), TODO-5910 (`--profile build`), TODO-5912 (`primec build --dry-run`), TODO-5913 (running the graph), TODO-5915 (incremental builds), TODO-5916 (subprojects), TODO-5917 (options), TODO-5918 (staged builds), TODO-5919 (function steps, after TODO-5896), TODO-5920 (PrimeScroll builds with `make.prime`); and the modules and libraries lane, which splits static libraries (docs/ModulesAndLibrariesPlan.md): TODO-5922 (`primevm --link` merges two IR modules) and TODO-5923 (`--emit=interface`) are ready and independent; then TODO-5924 (`--emit=lib`), TODO-5925 (`--library` and the link in the pipeline), TODO-5926 (generics and synthesized helpers), TODO-5927 (namespace ownership), TODO-5928 (`--emit-uses`), TODO-5929 (symbol-level rebuilds), TODO-5930 (the graph builds libraries); deferred TODO-5931 (the stdlib as a library, measure first), TODO-5932 (libraries that carry back-end code), TODO-5933 (the embed API loads a library).

Code reuse (docs/CodeReusePlan.md, section 17 for how to work it): these leaves are `ready` outside `Ready Now` because they are worked by their own pair of agents under rule 18. Start with TODO-5959 (delete the AST C++ and GLSL emitters) and TODO-5960 (one implementation of the execution and definition intra-body diagnostics), which touch different files. Then, as each lands: TODO-5961 (one type-text parser, after TODO-5959), TODO-5962 (one builtin-name classifier, after TODO-5961), TODO-5964 (semantics key-value predicates, after TODO-5960), TODO-5966 (binding validation as rules, not phases) then TODO-5967 and TODO-5968 (the same for template rewrites and call validation). Independent and ready at any time: TODO-5963 (lowerer collection spellings), TODO-5965 (reflection AST builders), TODO-5969 (table-driven float math lowering), TODO-5970 and TODO-5971 (semantic-product fact plumbing), TODO-5972 (lowerer helpers copied per unit), TODO-5973 (SoA rewrite walker), TODO-5974 (shared string, hash, JSON and C-literal helpers), TODO-5975 (opcode categories in the opcode table). Each block's `notes` names the leaves it must not run alongside. Deferred: TODO-5976 (parsed types in the semantic product) and TODO-5977 (one diagnostics builder).

### Task Blocks

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

- [ ] TODO-5938: A byte loop over one large region on `optexe` takes the cycles it took before the granule table
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Optimizing backends (residual of TODO-5840)
  - parallel_track: optexe-bytes
  - scope: TODO-5840 removed the per-byte loop from `String.append` of a literal (one bulk move), which took `append_chunks` from 234e6 to 93e6 cycles, but a loop that stores or loads every byte of one large region through `ps_byte_at` (`pushByte`, `textStore`, `byteAt` loops: `benchmarks/strings/append_bytes.prime`, 2.9e9 instructions, 480e6 cycles) still pays the granule-table check that TODO-5793 added (a dependent load and compare per byte, no write): the 10% cycles the predecessor measured on the byte loop of `append_chunks` before the bulk move (51.8 ms against 56.9 ms). Win it back for loops that remain per byte without a write on the read path, for example a loop that checks the region once per run of bytes (the bulk host functions already check once per call) or a `textStore` range form.
  - implementation_notes: include/primec/runtime/VmHeapCoreBody.inc (`byteAt`, `byteAtEdge`), src/backend/IrToOptCppRuntime.h (`ps_byte_at`, `ps_load_u8`, `ps_store_u8`). The check inlined into the byte loops costs 8% more instructions on short_strings and made `append_chunks` slower, so `ps_byte_at` stays out of line (TODO-5794 notes). Compare instruction counts and cycles of the program itself (`proc_pid_rusage`), minimum of interleaved runs, on a quiet machine; the one before this change is the build before e8a547504.
  - acceptance:
    - `append_bytes` on `optexe` within 2% of the cycles of the build before e8a547504, averaged over several code layouts; no other string benchmark slower than now
    - no write on the byte read path; full release gate green
  - stop_rule: the `optexe` byte check of per-byte loops; the bulk host functions and the VM are unchanged.

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
  - notes: TODO-5840 made `String.append` of a literal one bulk move, which cut these benchmarks (instructions and cycles, `proc_pid_rusage`, minimum of three on a machine at a load average of 27 to 64): `short_strings` 4.66e9 and 823e6 -> 3.90e9 and 748e6, `request_arena` 5.54e9 and 1115e6 -> 5.14e9 and 1039e6, `request_heap` 5.37e9 and 1073e6 -> 5.02e9 and 1049e6. That is a gain of 2 to 9% in cycles, the size of the 4 to 7% this task measured, but it does not show the cycles of the e8a547504 build: no build of that commit was made, so the acceptance (cycles within 3% of it, averaged over five layouts) is not checked and the task stays open; the first step is that baseline.

- [ ] TODO-5847: A spawned call with a collection parameter or result runs as a task
  - owner: ai
  - status: ready
  - created_at: 2026-11-10
  - phase: Multithreading (docs/MultithreadingPrototype.md, Runtime Model)
  - parallel_track: mt-vm-tasks
  - scope: A spawned call runs as a task on the VM and `optexe` when its parameters and result are scalars, `String`s, other structs outside the collections, closures, or `copy`/`copy mut`/`move` scalars and structs (`resolveTaskSpawnShape`, src/ir_lowerer/IrLowererRecursionAnalysis.h; TODO-5895 did the owned shapes). It still runs inline, with an `--opt-report` note, when a parameter or the result is a `vector`, a `map` or another collection, and when a parameter is a `copy` or `move` sum: the address ABI has no real call for those shapes. Give those shapes a real call and lower their spawns to `TaskSpawn` through the task block the struct shapes use (`LowerSetupStageState::TaskSpawnSite`): a collection argument is copied (or moved) in the spawner, the task reads it through the block, an owned parameter is destroyed by the task, and a collection result is taken over by the waiter.
  - implementation_notes: `isAddressAbiParameter` (src/ir_lowerer/IrLowererRecursionAnalysis.cpp) rejects template types and owned sums; a collection local carries element metadata (`LocalInfo`) the callee's parameter needs. An owned sum stays inline because a `[Item move]` real call freed the payload twice on the VM (the call site's inline binding of a sum temporary destroys it, and the body's drop flag destroys it again; TODO-5895 found it): decide one owner before taking sums. A `string` parameter is TODO-5741: a real call takes it since TODO-5741: drop the `string` reason in `resolveTaskSpawnShape` and pass the module string index as a scalar. Tests: tests/unit/program_matrix/test_program_matrix_tasks.cpp, tests/unit/ir_pipeline/optimizer/test_ir_optexe_tasks.cpp (the notes for /total and /numbers there turn into tasks).
  - acceptance:
    - positive: a task that takes a `vector<i32>` and one that returns a `vector<String>` give the waiter the values on the VM (default schedule and seeds 1 to 8) and on `optexe`, with the heap empty afterwards (heap statistics); `--opt-report` has no inline note for these spawns
    - negative: a spawned call that still runs inline keeps its note naming the reason
    - full release gate green
  - stop_rule: collection parameters and results, and owned sums, of spawned calls on the VM and `optexe`; `string` parameters stay inline for a task (it would need the module-table index passed as a scalar); native is TODO-5799.

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

- [ ] TODO-5978: An element of a `Vector<String>` is read by `at` and `[]`
  - owner: ai
  - status: ready
  - created_at: 2026-11-12
  - phase: Result and control flow (docs/spec/errors-and-file-io.md; found by TODO-5886)
  - parallel_track: pick-payloads
  - scope: Done 2026-11-12: `pop` and `clear` on a local declared `Vector<String>` destroy the strings they drop, so `push`, `pop`, `clear`, `remove_at` and `remove_swap` leave `stringHeapBytesInUse()` where it began on the VM (vector_owners case "a Vector of Strings destroys the strings ..."). Root cause: the canonical vector statement emitter (`tryEmitCanonicalVectorMutationStatement`) sized the dropped element from the receiver's element facts, which a specialization of the stdlib vector struct (`Vector__t...`) does not carry, so it assumed one scalar slot and destroyed nothing; it now reads the element type from the specialization's `instanceTemplateArgs`. Open: the method and index forms `names.at(i)` and `names[i]` of such a local read the element as the builtin vector access does, one slot of the element, and stop the VM with `VM error: unaligned indirect address in IR: N` (before any removal as well; `/std/collections/vector/at<String>(names, i)` and `vectorAtUnsafe` read the right string). `at` is not instantiated for the local, and the `isStructBoxedRecordTarget` rule that defers a record receiver to the stdlib `at` is not set when the target comes from semantic facts (setting it alone did not help, the helper has no instance to defer to). Make `at`, `at_unsafe` and `[]` on a local of the stdlib vector struct with a multi-slot element (a `String`, a struct) take the stdlib helper (monomorph rewrite as for `remove_at`), keeping the builtin access for scalar elements.
  - acceptance:
    - `names[i]` and `names.at(i)` of a `Vector<String>` read the right string after pushes, `pop`, `remove_at` and `remove_swap`, on the VM and `optexe`, with balanced heap statistics; a vector_owners program-matrix case (extend "a Vector of Strings destroys ...", which reads through the explicit helper)
    - `optexe` is checked for the removal case too
    - full release gate green
  - stop_rule: element reads of the stdlib `Vector<T>` with multi-slot elements only.

- [ ] TODO-5940: The HTTP load generator costs less CPU per connection than the servers it measures
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Multithreading and Web apps (docs/WebAppsPlan.md, Concurrency; found by TODO-5871)
  - parallel_track: mt-server
  - scope: scripts/http_load.py (16 Python client processes, a new connection per request) costs about 200 us of CPU per request, against 25 to 32 us for a C server, and every server held to it answers about 14,600 new connections a second on the 11-processor arm64 machine (docs/WebAppsPlan.md, Measured), so the 32 KB form with 4 workers (13,329, 2.41x `serve`) cannot show 2.5x. Provide a generator that costs a few microseconds per request: a small compiled client (C or a PrimeStruct program with `/std/net`, built by `run_http_benchmarks.py`) that keeps many connections in flight from one or two threads with non-blocking sockets, same request shapes and the same JSON result as scripts/http_load.py. Keep the Python generator for the gate test (`tests/scripts/test_http_load.py`).
  - acceptance:
    - the reference C server of TODO-5871 (not committed; a few lines) reaches at least 2x the rate with the new generator that it reaches with scripts/http_load.py on the same machine, or the result note says what limits it (the kernel's loopback setup, ephemeral ports)
    - `python3 benchmarks/http/run_http_benchmarks.py` on an idle machine with at least 6 processors reports the 32 KB form with 4 workers against `serve`; the table in docs/WebAppsPlan.md is replaced and the 2.5x target is met or the new limit is measured (per-thread CPU of the poller)
    - no gate test measures time; full release gate green
  - stop_rule: the load generator and its table; `serveWorkers` changes only if the new numbers show a limit of its own.

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

- [ ] TODO-5896: `--vm-threads` runs each spawned task on its own OS thread
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: VM threads (docs/MultithreadingPrototype.md, VM Threads)
  - parallel_track: vm-threads
  - scope: The VM runs every task on one OS thread under `VmTaskScheduler` (src/runtime/VmExecutionKernel.cpp). Add a threads mode behind `--vm-threads` (`primevm` and `primec --emit=vm`; the default does not change): `TaskSpawn` starts a pthread with an 8 MiB stack as `optexe` does (`OptCppTaskRuntime`, src/backend/IrToOptCppRuntime.h); the VM allocates through `VmHeapArenas` (include/primec/runtime/VmHeapArenas.h), one arena per thread and arena 0 for the main task; handles and `TaskWait` follow docs/spec/vm-design.md, Tasks and Tasks on threads (a table of slots with generations behind one mutex). Every host call takes one global lock, and a print is one write, so lines never mix. A fault or `Trap` on any task flushes and leaves with exit code 3 the way `optexe` does; the embed API does not offer the mode until TODO-5899. A module that holds Atomic, Mutex or Channel opcodes is rejected in this mode with `--vm-threads does not support atomics, mutexes or channels yet` until TODO-5897.
  - implementation_notes: the step function `stepImpl` already runs one call stack; a task needs its own frames, operand stack and local counts (see `VmTaskScheduler::Task`). The heap functions of src/runtime/VmHeapHelpers.cpp (`vm_detail::storeHeapByte` and the others) take the one `heap_`; how many places assume a single heap was not counted.
  - acceptance:
    - a program that spawns four tasks of scalar callees prints the same output as under the default schedule, in a test that runs it 20 times
    - in this mode a block a task allocates carries a non-zero arena number (address bits 41 to 46); a single-task program's addresses are identical to today's (golden test)
    - a `String` or vector a task returns is freed by the waiter (a block freed by a thread other than its owner, no fault)
    - a module with atomic, mutex or channel opcodes gets the diagnostic above; without the flag every existing task test is unchanged
    - the wall time of a CPU-bound four-task program in this mode and by default is recorded in docs/todo_log.md (measured, not asserted by a test)
    - docs/spec/vm-design.md, Tasks, describes the mode
    - full release gate green
  - stop_rule: spawn, wait, heaps and the one host lock only; no sync primitives, no host-call refinement, no fast kernel, no default change.

- [ ] TODO-5897: Atomic, Mutex and Channel run on real primitives under `--vm-threads`
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5896
  - created_at: 2026-11-11
  - phase: VM threads (docs/MultithreadingPrototype.md, VM Threads)
  - parallel_track: vm-threads
  - scope: The atomic opcodes of docs/spec/vm-design.md, Atomics, become sequentially consistent atomic operations on the heap slot. The mutex lock and notify opcodes of Mutexes block the thread; waiting threads are parked in a table keyed by the lock's address behind one mutex and condition variable, and `Channel<T>` of `/std/sync` (built on them) works unchanged. When every live thread waits the run faults with `deadlock: every task is waiting` (exit code 3), as `optexe` does since TODO-5846 (`optexe` faults when every task waits). The rejection of TODO-5896 is removed.
  - acceptance:
    - the `/std/sync` Atomic, Mutex and Channel tests pass under `--vm-threads`, each run 20 times
    - a program whose two tasks can finish only if they run at the same time (one spins on an atomic until the other sets it) finishes in this mode
    - two tasks waiting on each other fault with `deadlock: every task is waiting`
    - docs/spec/vm-design.md, Atomics, Mutexes and Channels, state the behavior under the flag
    - full release gate green
  - stop_rule: the three primitives and the deadlock fault; no host-call, fault or default changes.

- [ ] TODO-5898: Host calls under `--vm-threads` serialize by default and block only their own task
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5896
  - created_at: 2026-11-11
  - phase: VM threads (docs/MultithreadingPrototype.md, VM Threads)
  - parallel_track: vm-threads
  - scope: The global host lock of TODO-5896 is refined. A host function the embedder registers as reentrant runs without the lock; a call that blocks (`poll`, `recv`, a test host function that waits) then blocks only the task that made it. A host function registered as main-thread only (the native UI, docs/NativeUiPlan.md) faults with `<name> called off the main thread` when another thread calls it. Registration is a flag on the host function table (src/runtime/VmHost.cpp) and on `Script`'s host binding (include/primec/embed/Script.h).
  - acceptance:
    - with a test host function that blocks until another task signals through an atomic, the program finishes in this mode (it deadlocks under the lock) 
    - a non-reentrant host function is never entered by two threads at once (a test host function that fails on re-entry)
    - a main-thread-only host function called from a spawned task faults with the diagnostic
    - full release gate green
  - stop_rule: the registration flags and the lock; no new host functions.

- [ ] TODO-5899: A fault or a cancel stops every thread of a `--vm-threads` run without ending the process
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5896
  - created_at: 2026-11-11
  - phase: VM threads (docs/MultithreadingPrototype.md, VM Threads)
  - parallel_track: vm-threads
  - scope: Replace the `_Exit` path of TODO-5896 with a stop flag that every thread polls at calls and loop back-edges (a relaxed load, only in this mode). A fault or `Trap` on any task sets it, the other threads stop and are joined, and the run ends with exit code 3 after flushing, as on one thread. `Script::run` (include/primec/embed/Script.h) can use the mode, and `RunOptions::cancel` stops a threaded run with `VM error: execution stopped`. A module without task opcodes starts no thread.
  - acceptance:
    - a fault in one of three tasks ends the run with exit code 3 and every thread joined; the embed test harness keeps running afterwards in the same process
    - a task that loops forever stops when the cancel flag is set
    - a program without tasks behaves and starts exactly as before (startup unchanged, scripts/measure_startup.sh within noise)
    - full release gate green
  - stop_rule: stop and cancel only; no change to what a fault reports.

- [ ] TODO-5900: The concurrent corpus runs repeatedly under `--vm-threads` and under ThreadSanitizer
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5899
  - depends_on: TODO-5897, TODO-5898
  - created_at: 2026-11-11
  - phase: VM threads (docs/MultithreadingPrototype.md, VM Threads)
  - parallel_track: vm-threads
  - scope: Every concurrent conformance program (the ones that run several seeds today) runs under `--vm-threads` 20 times and must print what the eager schedule prints. A script `scripts/tsan_vm_check.sh` builds `primevm` with `-fsanitize=thread` in `build-tsan/` and runs the same corpus; a ThreadSanitizer report fails it. A program whose output depends on the interleaving is made schedule-independent or removed from the corpus; none is accepted as flaky (AGENTS.md, No flaky tests). The script is run by hand and one command at a time, like the other heavy commands.
  - acceptance:
    - a ctest entry runs the corpus under `--vm-threads` repeatedly and passes in the release gate
    - `scripts/tsan_vm_check.sh` passes with no report, and its last result is recorded in docs/todo_log.md
    - the corpus list is a file checked by a registration test, so a new concurrent test cannot be left out
    - full release gate green
  - stop_rule: the corpus runs and the TSAN script; no change to the default schedule.

- [ ] TODO-5901: A threaded VM run records its synchronization order and replays it on the deterministic scheduler
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5897
  - created_at: 2026-11-11
  - phase: VM threads (docs/MultithreadingPrototype.md, VM Threads)
  - parallel_track: vm-threads
  - scope: `--vm-record-sync <path>` writes, for a `--vm-threads` run, the order of the synchronization events across tasks (spawn, wait, lock, unlock, notify, every atomic operation, channel operations). `--vm-replay-sync <path>` runs the program on the deterministic scheduler following that order, so a run that was threaded can be repeated, stepped and inspected. Host input and output are not recorded; a program that reads from the outside is replayed only if the host calls return the same bytes.
  - acceptance:
    - a program with a mutex and a channel, run 20 times in threads mode with a record each, replays each record to the same stdout
    - a record from a different program or a changed program is refused with a diagnostic
    - the log format is versioned and documented in docs/spec/vm-design.md
    - full release gate green
  - stop_rule: record and replay of synchronization order; no per-instruction trace, no inspector work.

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

- [ ] TODO-5903: A debug session runs programs that spawn tasks
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: VM threads (docs/MultithreadingPrototype.md, VM Threads); docs/VmInspectorPlan.md
  - parallel_track: vm-debug-tasks
  - scope: A debug session faults on task opcodes today (docs/spec/vm-design.md, Tasks) and the DAP adapter reports one thread. `VmDebugSession` (src/runtime/VmDebugSession*.cpp) runs `TaskSpawn` and `TaskWait` under the deterministic scheduler (eager, or seeded), `VmDebugAdapter::threads` lists the main task and every live task, `stackTrace` takes a thread id, and a stop stops every task. This is the first prerequisite of the inspector (docs/VmInspectorPlan.md, section 11).
  - acceptance:
    - a DAP session on a program with `[spawn]` stops at a breakpoint inside the spawned callee, `threads` lists the main task and that task, and `stackTrace` of each answers
    - continuing runs to the same exit code and output as without the debugger
    - a program without tasks gives byte-identical DAP and `--debug-json` output
    - docs/spec/vm-design.md no longer says a debug session faults on tasks
    - full release gate green
  - stop_rule: debug sessions on the deterministic scheduler; no threads mode, no trace or replay of task programs.

- [ ] TODO-5904: The VM runs tasks on threads by default
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5900
  - created_at: 2026-11-11
  - phase: VM threads (docs/MultithreadingPrototype.md, VM Threads)
  - parallel_track: vm-threads
  - scope: `primevm` and `primec --emit=vm` run tasks on threads unless `--vm-deterministic` (or `--vm-schedule-seed`, or a debug session) selects the deterministic scheduler. The conformance tests that need a fixed order pass the flag. docs/spec/vm-design.md and the VM Threads section become normative text for the new default.
  - acceptance:
    - the full release gate is green with the new default
    - a program that does not spawn prints and starts exactly as before
    - docs/MultithreadingPrototype.md and docs/spec/vm-design.md state the default
  - stop_rule: the default and the flag only.

- [ ] TODO-5905: `/std/path` joins, splits and normalizes paths
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md)
  - parallel_track: build-path
  - scope: A pure stdlib module `stdlib/std/path/path.prime` with POSIX-separator functions over `String`: `join`, `parent`, `file_name`, `stem`, `extension`, `is_absolute`, `normalize` (textual: resolves `.` and `..`, collapses repeated separators) and `relative_to`. User-facing names and style follow docs/CodeExamples.md. No filesystem access, no host calls, no Windows separators.
  - acceptance:
    - compile-run tests on the VM and `--emit=optexe` for each function, including an empty path, a trailing separator, `..` past the root, and an absolute path joined onto a relative one
    - docs/spec/stdlib-reference.md has a `/std/path` section with a runnable example
    - full release gate green
  - stop_rule: textual path functions only.

- [ ] TODO-5906: `/std/file` lists a directory and reports kind, size and modification time
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md); also needed by "Open PrimeStruct Project" (docs/AiWorkflowPlan.md, section 4.1)
  - parallel_track: file-listing
  - scope: `/std/file` has no directory listing or file metadata. Add `[host]` functions on the pattern of `ps_os_*` (stdlib/std/os/os.prime, bound in the VM host table src/runtime/VmHost.cpp and in a `*_posix.cpp` linked for `--emit=optexe`) with the `file_read` effect: list a directory's entries sorted bytewise by name, each with kind (file, directory, other), size in bytes and modification time (seconds, `i64`). A missing or unreadable directory returns an error value in the style of stdlib/std/file/errors.prime. Which return shapes the host ABI allows for a list was not checked; one newline-separated `Text` that PrimeStruct splits is the fallback.
  - acceptance:
    - a fixture directory lists identically, sorted, on the VM and with `--emit=optexe`
    - a missing directory returns the error value on both
    - a program that does not call the new functions starts exactly as before (scripts/measure_startup.sh within noise)
    - docs/spec/stdlib-reference.md documents the functions with a runnable example
    - full release gate green
  - stop_rule: listing and metadata only; no watching, no creating, removing or copying, no recursion (glob is TODO-5907).

- [ ] TODO-5907: `/std/file` expands glob patterns
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5906
  - depends_on: TODO-5905
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md)
  - parallel_track: build-glob
  - scope: `glob(base, pattern)` returns the paths under `base` that match, sorted bytewise: `*` (within a segment), `**` (any number of segments), `?`. A name that begins with a dot matches only a pattern segment that begins with a dot. Written in PrimeStruct over the listing of TODO-5906 and the path functions of TODO-5905. The directory listings it read are what a build treats as inputs, so the function also returns them (or a second function does) for TODO-5915.
  - acceptance:
    - tests over a fixture tree for `*.prime`, `**/*.prime`, `?`, a dotted file, and no match
    - results are identical on the VM and with `--emit=optexe`
    - docs/spec/stdlib-reference.md documents it
    - full release gate green
  - stop_rule: the three pattern forms; no bracket expressions, no negation.

- [ ] TODO-5908: `--allow-effects` rejects declared effects outside a list in user code
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md, section 6)
  - parallel_track: effect-ceiling
  - scope: A compiler option `--allow-effects <list>`: a definition or execution in user code (not the stdlib, which is trusted) that declares an effect outside the list, or whose default effects fall outside it, is a diagnostic naming the definition, the effect and the allowed list. It works with `--collect-diagnostics`. This is the ceiling a build script runs under, and also lets an embedder cap scripts. Where the effect subset check lives was not found; start from the validation that reports "effect/capability subset failures" (docs/spec/source-pipeline.md) and `Options::defaultEffects` (include/primec/support/Options.h).
  - acceptance:
    - with `--allow-effects heap_alloc,file_read`, user code declaring `net_io` is rejected with the diagnostic, and code within the list compiles and runs
    - a definition without `[effects]` is checked against the default set it receives
    - stdlib definitions are not checked against the list
    - without the flag every existing diagnostic and output is unchanged
    - docs/spec/language-core.md, Effects, documents the option
    - full release gate green
  - stop_rule: the option and its check on user code; no per-directory ceilings, no new effects.

- [ ] TODO-5909: `--allow-import` rejects imports outside a list of prefixes
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md, section 6)
  - parallel_track: import-allowlist
  - scope: A compiler option `--allow-import <prefix>[,<prefix>...]`: an `import` of a stdlib path outside the listed prefixes (for example `/std/net/*` when only `/std/build`, `/std/path`, `/std/file` and `/std/collections` are listed) is a diagnostic naming the import and the allowed prefixes. Imports of the project's own source files are not affected. The import resolver is src/frontend/ImportResolver.cpp; stdlib parts the compiler includes by itself (the expanded-source ledger calls them auto-included) are not checked, and whether an unlisted stdlib module can be reached that way was not checked.
  - acceptance:
    - an `import /std/net/*` is rejected under an allowlist without it and accepted with it
    - an import of a project file is accepted under any allowlist
    - without the flag every output is unchanged
    - docs/spec/source-pipeline.md documents the option
    - full release gate green
  - stop_rule: the option and its check on explicit imports; no transitive analysis of what a library imports.

- [ ] TODO-5910: `--profile build` sets the defaults of a build script
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5909
  - depends_on: TODO-5908
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md, section 6)
  - parallel_track: build-profile
  - scope: A named profile in `Options` (include/primec/support/Options.h): `--profile program` is today's behavior and the default; `--profile build` sets default effects to none, the effect ceiling and import allowlist of TODO-5908 and TODO-5909 (`file_read`, `heap_alloc`; `/std/build`, `/std/path`, `/std/file`, `/std/collections`), `-O0`, and the deterministic task schedule. Flags after the profile override its values, as `-O` flags do. The runner picks the profile; a file cannot choose its own.
  - acceptance:
    - under `--profile build` a definition without `[effects]` cannot print, an effect outside the ceiling is rejected, and `import /std/net/*` is rejected
    - a pure program gives the same result under both profiles
    - `--profile program`, and no flag, leave every existing output byte-identical
    - docs/spec/integration-and-tooling.md documents the profiles
    - full release gate green
  - stop_rule: the two profiles and their defaults; no profile for tests or scripts yet, and no entry wrapper (TODO-5912).

- [ ] TODO-5911: `/std/build` builds a graph value and exports it as JSON
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md, sections 3 to 5)
  - parallel_track: build-graph
  - scope: Stdlib `/std/build` in PrimeStruct: `Build`, `Compiler` (emit kind, optimization level, import paths, entry, transforms, default effects, link arguments), `Graph`, and nodes for compile steps, a static library target and a link edge. Library and link nodes export but are marked as not yet runnable. `graph.export()` returns deterministic JSON: a version field, nodes sorted by id, ids derived from target names; no hashes (the runner computes them). A plain program (no runner, no loader) builds a graph and prints the export. `b.files` takes an explicit list; pattern expansion follows TODO-5907.
  - acceptance:
    - a program builds a graph with two compiler configurations of one program, one executable, one library and a link edge, and prints a JSON export that matches a golden file
    - building the graph in a different call order gives the same export
    - the program needs only the `file_read` effect
    - docs/BuildSystemPlan.md section 5 is replaced with a program that was run
    - full release gate green
  - stop_rule: values and export only; no loader for `make.prime`, no running, no glob.

- [ ] TODO-5912: `primec build --dry-run` prints the graph of `make.prime`
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5910
  - depends_on: TODO-5911
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md)
  - parallel_track: build-cli
  - scope: `primec build [--dry-run] [dir]` finds `make.prime` in `dir` (default: the working directory), runs it on the VM under `--profile build` through a stdlib wrapper entry that constructs `Build`, calls the script's `build`, and hands the returned graph to the runner. With `--dry-run` the runner prints the export. Without it the command reports `running a graph is not supported yet` until TODO-5913. Diagnostics point at positions in `make.prime`. The subcommand is parsed in src/pipeline/CliDriver.cpp.
  - acceptance:
    - a fixture `make.prime` prints its graph; the output equals what the same graph prints from a plain program
    - a script with a type error, a missing `make.prime`, and a script that declares an effect outside the ceiling each fail with a diagnostic that names `make.prime`
    - `primec` without the `build` subcommand behaves exactly as before
    - docs/spec/integration-and-tooling.md documents `primec build`
    - full release gate green
  - stop_rule: finding, running and printing; no execution of steps.

- [ ] TODO-5913: `primec build` runs the graph
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5912
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md)
  - parallel_track: build-runner
  - scope: The runner in `primec` executes the compile steps of the graph: each through the in-process compile pipeline (include/primec/pipeline/CompilePipeline.h) with the step's settings, writing under `--out-dir`. Independent steps run in parallel, `--jobs N` (default: the core count) bounds them. A failed step stops its dependents and reports the step name and the compiler's diagnostics; the exit code is non-zero. Library and link nodes are rejected with `static libraries are not supported by this compiler yet`. The embed library stays free of process spawning (scripts/check_embed_no_process.sh): the runner lives in `primec`. Whether to compile in-process or start `primec` per step is decided here and recorded in docs/BuildSystemPlan.md section 11; in-process is the starting point.
  - acceptance:
    - a `make.prime` builds a program as `--emit=exe` and one as `--emit=optexe` with a link argument, and both outputs run
    - two independent steps both produce their outputs with `--jobs 2` (no timing assertion)
    - a failing step reports its name and diagnostics and stops its dependents
    - a graph with a library node fails with the diagnostic
    - full release gate green
  - stop_rule: compile steps only; no cache (TODO-5915), no command or function steps, no `run` in the script.

- [ ] TODO-5914: `--emit-deps` writes the files a compile read
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md, section 8)
  - parallel_track: build-deps
  - scope: `primec` and `primevm` with `--emit-deps <file>` write the sorted list of source files the compile read (the primary input, every import, the stdlib files actually included), one resolved path per line, without changing any output. The expanded-source provenance ledger (docs/spec/source-pipeline.md, step 1) already records these units; the list is its file paths. Only stdlib files that were included appear, so lazy imports keep a small list.
  - acceptance:
    - a program with two imports and a stdlib import lists exactly those files, sorted, identically on two runs
    - a program that imports nothing lists only itself
    - the program's output and `--dump-stage` output are byte-identical with and without the flag
    - docs/spec/integration-and-tooling.md documents the flag
    - full release gate green
  - stop_rule: the flag and its list; no cache, no make-style dependency file syntax.

- [ ] TODO-5915: `primec build` skips steps whose inputs did not change
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5913
  - depends_on: TODO-5914
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md, section 8)
  - parallel_track: build-cache
  - scope: A compile step's key is its compiler settings, the content hashes of the files `--emit-deps` listed for its last run, and the compiler version. The keys and the lists are kept in `ProjectRoot/.ps/cache/`, which is not committed. A step whose output exists and whose key and file hashes are unchanged is reported as up to date and not run. The hash scheme is named in the cache file. The directory listings a glob read count as inputs of the graph, so adding a file that a pattern matches re-runs the step.
  - acceptance:
    - a second run of an unchanged build runs no step
    - touching an imported file, changing a compiler setting, and deleting the output each re-run exactly the affected step
    - touching an unrelated file re-runs nothing
    - a build with the cache deleted gives byte-identical outputs
    - full release gate green
  - stop_rule: compile steps with file-hash keys; no interface-hash early cutoff (that comes with libraries, TODO-5929), no shared or remote cache.

- [ ] TODO-5916: `b.subproject` merges the graph of a subfolder's `make.prime`
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5912
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md, section 7)
  - parallel_track: build-subprojects
  - scope: `b.subproject("dir")` runs `dir/make.prime` under the same profile with paths relative to `dir`, and merges the graph it returns into the parent under the namespace `dir` (node ids prefixed). A file reached twice is evaluated once; a cycle is a diagnostic naming the files; a missing `make.prime` is a diagnostic. How a subproject loads is decided here and recorded in docs/BuildSystemPlan.md section 11: a separate VM run whose export the runner merges (preferred), or one program that imports every script, which needs one entry name per file.
  - acceptance:
    - a fixture with two subfolders prints a merged graph with namespaced node ids in the dry run
    - a diamond (two subprojects including a third) evaluates the third once
    - a cycle and a missing `make.prime` each give a diagnostic
    - relative paths in a subproject's script resolve against its own folder
    - full release gate green
  - stop_rule: explicit inclusion and merging; no automatic discovery, no running from a subproject.

- [ ] TODO-5917: Build options: `b.option` and `primec build --option`
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5912
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md, section 5)
  - parallel_track: build-options
  - scope: `b.option("name", default)` declares a typed option (bool, i32 or String, with a description) and returns its value from `primec build --option name=value`. The options and their values appear in the export and in the cache key (TODO-5915 reads them when it lands). `--list-options` prints the declared options with types, defaults and descriptions. An `--option` the script never declares is a diagnostic.
  - acceptance:
    - a script that branches on a bool option gives different dry-run exports for the two values
    - an unknown `--option`, and a value of the wrong type, are diagnostics
    - `--list-options` output matches a golden file
    - full release gate green
  - stop_rule: three option types; no environment-variable options, no option files.

- [ ] TODO-5918: A script can run its graph in stages
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5913
  - depends_on: TODO-5908
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md, section 3)
  - parallel_track: build-stages
  - scope: `graph.run()` executes the graph at that point and returns when it has finished, so a script can list files a first stage wrote and build a second graph from them. The call needs the effect `build_run` (name open, recorded in docs/BuildSystemPlan.md), which the build profile grants only to the root script. In `--dry-run` the output ends at the first `run` and says the plan is incomplete; the script is not continued.
  - acceptance:
    - a fixture whose first stage has a generator step that writes `.prime` files, whose script lists them and compiles them in a second stage, builds with `primec build`
    - `--dry-run` of that fixture prints the first stage and marks the plan incomplete
    - `run` called from a subproject script, and without the effect, are diagnostics
    - full release gate green
  - stop_rule: `run` and its effect; the stages share one cache (TODO-5915 when landed), no new step kinds.

- [ ] TODO-5919: A build step can be a PrimeStruct function that runs on a VM thread
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5913
  - depends_on: TODO-5896
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md, section 4)
  - parallel_track: build-function-steps
  - scope: `b.step(name, inputs, outputs, function)` adds a step the runner executes in-process by running the function on a VM thread (TODO-5896), in parallel with other steps. The function's own effect declaration is its sandbox, and its file writes are confined to the build output directory by a path check in the host function. The export names the function and the files of its import closure and marks the graph as not self-contained. Large for one change: split it once it starts if the path confinement needs its own leaf.
  - acceptance:
    - a generator function step writes a `.prime` file that a compile step then builds
    - two function steps both produce their outputs (no timing assertion)
    - a step that writes outside the output directory faults with a diagnostic naming the path
    - the dry-run export marks the graph as not self-contained
    - full release gate green
  - stop_rule: function steps with declared inputs and outputs; no command steps, no network.

- [ ] TODO-5920: PrimeScroll builds with `make.prime`
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5913
  - created_at: 2026-11-11
  - phase: Build system (docs/BuildSystemPlan.md, section 10)
  - parallel_track: build-dogfood
  - scope: Add a `make.prime` in examples/apps/text_editor that builds PrimeScroll the way scripts/build_native_app.sh does today (`--emit=optexe` with the UI backend's link arguments, or the `--direct` native form). Needs macOS for the AppKit backend. The script stays until the two produce the same program. The installed app and its install recipe are not touched.
  - acceptance:
    - `primec build` in that folder produces an executable that runs as the script's output does
    - startup of the two executables, measured with scripts/measure_startup.sh, is within noise
    - the differences, if any, are listed in docs/todo_log.md
  - stop_rule: building only; the script is not deleted and the install recipe is not changed.

- [ ] TODO-5922: `primevm --link` merges two serialized IR modules
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, sections 9 and 16)
  - parallel_track: lib-ir-link
  - scope: `IrFunction` (include/primec/ir/Ir.h) can be an import: no instructions, a flag and the name of the library that defines it; this is a schema bump with a migration note (IR stability rule, AGENTS.md). A new table classifies the operand of every opcode (function index: `Call`, `CallVoid`, `TaskSpawn`; host-import index: `CallHost`; string index: the string-printing and string-byte opcodes and string immediates; none; other), next to include/primec/ir/IrOpcodeTable.h and checked by a script in the style of scripts/check_ir_opcode_table.py, so a new opcode cannot be added without saying how it links. A merge function combines a consumer module with library modules by name: functions (an import resolves to the definition; a name defined twice is an error), struct layouts (equal layouts merge, unequal are an error), host imports (by name and signature), string tables (concatenated, every string operand remapped), instruction source maps and local debug slots (debug ids shifted). The result goes through `IrValidation`. `primevm <a> --link <b>` runs the merged module. Which opcodes carry which operand was not classified yet; read src/ir/IrValidation.cpp and the VM kernels for the ground truth.
  - acceptance:
    - two hand-built serialized modules, one calling an import the other defines, link and run on the VM with the expected result
    - one test per error: a name defined twice, unequal layouts of one name, an unresolved import (`undefined symbol <name> (library <lib>)`), a host import of one name with two signatures
    - string operands of every string opcode point at the right text after the merge (a test that prints strings from both modules)
    - the check script fails when an opcode has no operand class
    - the schema version bump has a loader test for the previous version
    - full release gate green
  - stop_rule: the merge and the table only; no frontend, no archive, no optimizer or backend change beyond reading the merged module.
  - notes: large; if the schema bump and the table need separate changes when it starts, split them with this id on the table.

- [ ] TODO-5923: `primec --emit=interface` prints a module's interface and symbol table
  - owner: ai
  - status: ready
  - created_at: 2026-11-11
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, section 5)
  - parallel_track: lib-interface
  - scope: For a module root (a `[namespace]` or a file whose public definitions are the library, with no `main`), print the interface text in canonical PrimeStruct syntax and the symbol table as JSON. The text holds public structs and enums in full (fields with types and visibility, static fields), public non-generic functions as declarations with resolved parameter and return types, `[effects]` and `[summary]` and no body, and the `[namespace]` envelopes. The symbol table has one entry per symbol: path, kind, hash of the signature, hash of the content, hash of the closure of referenced symbols; hashes are SHA-256 (the stdlib manifest's FNV-1a 64 bit in include/primec/frontend/StdlibSymbolManifest.h only detects drift); the interface hash is the hash of the sorted table. A declaration printer is written for this; `--dump-stage ast` is a debug rendering and is not reused. Generic definitions are not included in this leaf (TODO-5926). Whether the repository has a SHA-256 to use was not checked. The pipeline may demand `/main`; emitting an interface needs no entry point.
  - acceptance:
    - golden tests of the text and the table for a module with a struct, an enum, a function, a nested namespace and a private function (absent from the output)
    - the printed text parses and validates as a program of declarations (parse-back test); if it does not, the printer is wrong
    - the same module printed twice gives byte-identical output; changing a function body changes its content hash and not its signature hash or the interface hash
    - inferred return types appear resolved (a function written without `return<...>` prints with its inferred one)
    - docs/spec/integration-and-tooling.md documents the emit kind
    - full release gate green
  - stop_rule: non-generic interface and the table; no archive, no library loading, no generics.

- [ ] TODO-5924: `--emit=lib` writes a library archive from every public definition of a module
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5923
  - depends_on: TODO-5922
  - created_at: 2026-11-11
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, sections 6 and 7)
  - parallel_track: lib-archive
  - scope: Lowering takes every public non-generic definition under the module's roots as a root (today it takes one `entryPath`; whether it lowers only what the entry reaches was not checked), and what those roots reach is lowered with them. The IR is stored unoptimized, with an entry index of -1, struct layouts, host imports, source map and debug slots. `primec --emit=lib <root.prime> -o <name>.psa` writes the archive of docs/ModulesAndLibrariesPlan.md section 6: `manifest` (name, owned namespace roots, compiler build id, IR schema version, hash scheme, interface hash, linked libraries), `interface`, `symbols`, `code`, and optionally `sources` (`--with-sources`). The output is deterministic: sorted sections, no timestamps, no absolute paths. A reader loads and verifies an archive (hashes, versions) and reports a mismatch with a message that says to rebuild.
  - acceptance:
    - building a library twice, and on two checkouts of the repository, gives byte-identical archives
    - the reader rejects an archive with a changed section, another compiler build id, or another schema version, each with its own message
    - a public function no other definition calls is present in the archive's code
    - docs/spec/integration-and-tooling.md documents `--emit=lib` and the archive layout
    - full release gate green
  - stop_rule: writing and reading the archive; nothing consumes it yet.

- [ ] TODO-5925: `--library` compiles a program against a library and links it
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5924
  - created_at: 2026-11-11
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, sections 8 and 9)
  - parallel_track: lib-consume
  - scope: `--library <name>.psa` (repeatable) registers a library: the import resolver (src/frontend/ImportResolver.cpp) finds its modules in the archive's interface and treats the text as source; a declaration marked `[imported(<library>)]` has no body and the marker is a diagnostic in any file that is not an interface. Lowering turns a call of an imported function into a `Call` to an import function (TODO-5922); the pipeline links before `prepareIrModule` and checks, before linking, that each interface hash the program was compiled against equals the library's (`library ui changed since this program was compiled; rebuild`) and that the library's own libraries are supplied (`library ui needs library base`). Every backend then sees one merged module. A program built from a library and one built as a whole must behave the same, and that is tested here against a corpus (the differential standard of docs/LibrarySymbolManifestLazyImports.md): the first version covers programs with structs, functions, effects, strings and a task.
  - acceptance:
    - a small program with a library of a struct, helper methods and functions runs on the VM and with `--emit=optexe`, with the same output as the whole-program build of the same sources
    - a corpus of at least 20 programs gives identical output and exit code built whole and built as library plus consumer, in a ctest entry
    - a stale library, a missing transitive library and `[imported]` in a program source each give their diagnostic
    - a program that uses no `--library` compiles exactly as before (outputs byte-identical)
    - docs/spec/source-pipeline.md and integration-and-tooling.md document the flag
    - full release gate green
  - stop_rule: non-generic code only; generics, namespace ownership and uses output are their own leaves.

- [ ] TODO-5926: Generic definitions and synthesized helpers cross the library boundary
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5925
  - created_at: 2026-11-11
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, sections 5 and 10)
  - parallel_track: lib-generics
  - scope: A public generic definition is in the interface in full, with its body, and is instantiated in the consumer. A private definition a public generic reaches is in the interface too, marked private and in the same namespace, so the visibility check of the generic's body still passes. An instantiation that both the library and the consumer produce (`Vector<i32>`) is merged by the linker when the two are identical after remapping and an error when they differ. A struct's compiler-synthesized helpers (field-wise copy, destroy) are lowered in the library that owns the struct and declared in the interface; a struct defined in the consumer synthesizes its own. Not checked: whether instantiated names are deterministic and content-derived, and whether the synthesized helpers are functions or are expanded at each use; if names are not deterministic, that is fixed first and this leaf says so.
  - acceptance:
    - a library exports a generic `Box<T>` and a function using `Box<i32>` internally; a consumer instantiates `Box<i32>` and `Box<String>`; the program runs as its whole-program build does, with one copy of `Box<i32>` in the linked module
    - a public generic that calls a private helper compiles in the consumer
    - two instantiations of one name with different bodies are a link error
    - a struct from a library, copied and destroyed in the consumer, behaves as in the whole-program build (no double free, no leak), checked on the VM and `--emit=optexe`
    - the corpus of TODO-5925 passes with generics added
    - full release gate green
  - stop_rule: generics, their private reach and the synthesized helpers; no inference changes.

- [ ] TODO-5927: A library owns its namespace roots
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5925
  - created_at: 2026-11-11
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, section 4)
  - parallel_track: lib-ownership
  - scope: The manifest lists the roots a library owns (section 6 of the plan). Linking two libraries whose roots overlap is an error naming both; a program that defines something under a root of a linked library is an error (`/ui/draw is owned by library ui`) and not a duplicate-definition error found late. `[namespace]` envelopes (TODO-5841) are how a module declares its root.
  - acceptance:
    - overlapping roots, and a program defining a function under a library's root, each give a diagnostic that names the root and the library
    - a program that defines names next to a library's root (`/ui2`, `/app`) compiles
    - without `--library` nothing changes
    - full release gate green
  - stop_rule: the two checks; no nested ownership, no private roots.

- [ ] TODO-5928: `--emit-uses` writes the library symbols a compile resolved
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5925
  - created_at: 2026-11-11
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, sections 8 and 11)
  - parallel_track: lib-uses
  - scope: With `--emit-uses <file>` a compile writes, sorted, every library symbol the program resolved (path, library, signature hash and content hash from the library's symbol table), including the generic definitions it instantiated and the private declarations they reached. The output does not change any other output.
  - acceptance:
    - a program that calls two of a library's twenty functions lists exactly those and what they reach, identically on two runs
    - a program that imports a library and uses nothing lists nothing
    - outputs and dumps are byte-identical with and without the flag
    - full release gate green
  - stop_rule: the list only; no build-graph use (TODO-5929).

- [ ] TODO-5929: The runner rebuilds a dependent only when a symbol it used changed
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5928
  - depends_on: TODO-5915
  - created_at: 2026-11-11
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, section 11)
  - parallel_track: lib-cutoff
  - scope: A compile step that links libraries keys on the symbol hashes of its last `--emit-uses` list instead of on the whole interface hash. A library body change that leaves the signature and content hashes of the symbols the step used unchanged re-links but does not recompile the step; a change to a symbol the step used recompiles it; a change to a symbol it did not use does neither. Uses the cache of TODO-5915.
  - acceptance:
    - changing the body of a library function the program did not call does not re-run the program's compile step
    - changing the signature of a function the program called re-runs it
    - changing a body the program called re-runs the link (the output differs) and the result runs correctly
    - full release gate green
  - stop_rule: the rebuild decision for libraries; no remote cache.

- [ ] TODO-5930: `primec build` builds libraries and links them
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5925
  - depends_on: TODO-5913
  - created_at: 2026-11-11
  - phase: Modules and libraries (docs/ModulesAndLibrariesPlan.md, section 11); Build system (docs/BuildSystemPlan.md)
  - parallel_track: lib-build
  - scope: The library and link nodes of the graph (TODO-5911) run: `b.library(name, cc, root)` produces a `.psa` through `--emit=lib`, `exe.link(lib)` becomes `--library` on the compile step, and a library that links with another records it in its manifest so the runner passes the whole closure. Libraries build in parallel with each other; a diamond (two libraries linking a third) builds the third once. The diagnostic of TODO-5913 for library nodes (`static libraries are not supported by this compiler yet`) is removed.
  - acceptance:
    - a `make.prime` with a library, a second library that links it, and an executable that links both builds and the executable runs
    - a diamond builds the shared library once
    - a library whose build fails stops its dependents and reports the library's name
    - docs/BuildSystemPlan.md section 4 drops the "not runnable" note
    - full release gate green
  - stop_rule: building and linking through the graph; the symbol-level cutoff is TODO-5929.

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

- [~] TODO-5962: Builtin math and control-flow call names are classified once, with one rule
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5989
  - claimed_by: reuse-b (worktree-agent-a051c39accd554d52), 2026-11-13
  - split_into: TODO-5988 (the math-name rule, done with this split), TODO-5989 (control-flow call names, `isSimpleCallName`, `trimLeadingSlash`, convert and return kinds; closes this leaf)
  - created_at: 2026-11-12
  - phase: Code reuse (docs/CodeReusePlan.md, section 4)
  - parallel_track: reuse-builtin-names
  - depends_on: TODO-5959, TODO-5961
  - scope: `parseMathName` exists in src/semantics/SemanticsBuiltinPathHelpersShared.h:44, src/ir_lowerer/IrLowererBuiltinNameHelpers.cpp:206 and src/ir_lowerer/IrLowererSetupMathHelpers.cpp:16 (different signature, `hasMathImport` flag); `getBuiltinMinMaxName`, `getBuiltinAbsSignName`, `getBuiltinSaturateName`, `getBuiltinConvertName`, `getBuiltinClampName` exist in semantics (SemanticsBuiltinPathHelpers.cpp:329-350, SemanticsBuiltinPathHelpersRewrites.cpp:176), the lowerer (IrLowererBuiltinNameHelpers.cpp:261-342, table-driven via `classifyMathName`) and the IR printer (src/ir/IrPrinterHelpers.cpp:220-292); `isSimpleCallName` in src/semantics/SemanticsCallPathHelpers.cpp:123, src/semantics/SemanticsValidatorStatementBindingsPhasesBReferencePhasesA.cpp:459, src/ir_lowerer/IrLowererHelpers.cpp:296 and :589, IrPrinterHelpers.cpp:165; `isReturnCall` in SemanticsCallPathHelpers.cpp:160, IrLowererHelpers.cpp:413, IrPrinterHelpers.cpp:151 and src/ast/AstPrinter.cpp:17; `isIfCall`, `isLoopCall`, `isWhileCall`, `isForCall`, `isRepeatCall` three times each; `trimLeadingSlash` in SemanticsCallPathHelpers.cpp:15, three times in src/semantics/SemanticsValidatorInferCollectionCompatibilityInternal.h (:366, :422, :435) and src/ir_lowerer/IrLowererCallHelpers.cpp:46. The rules differ: semantics reads `expr.name`, skips SoA-alias normalization and rejects any `/` path other than `std/math/...`; the lowerer normalizes the SoA alias and turns `/foo/bar/min` into `min`; the IR printer rejects any name containing `/`, so `/std/math/min` is not recognized there. Put one classifier in primec_support_lib (or extend include/primec/ast/AstCallPathHelpers.h, which already has `normalizedCallName`) with one normalizer and a small policy parameter (bare names allowed or not, import-gated or not); only "which name string to read from the Expr" stays per layer.
  - implementation_notes: The lowerer's last-segment rule is the outlier; check what depends on it before dropping it. A probe on 2026-11-12 (`/mylib/min` returning `a + b`, `/mylib/abs` returning `a + 100`, `main` returning `/mylib/min(3, 4) + /mylib/abs(1)`) exits 108 on vm and native today; keep it as a program-matrix or compile-run case.
  - acceptance:
    - one definition of each listed classifier; the lowerer, semantics, IR printer and AST printer copies are deleted
    - a table test runs the same spellings (bare, `/min`, `/std/math/min`, `/foo/bar/min`, SoA aliases) through the classifier with each policy and pins the results
    - the `/mylib/min` probe returns 108 on vm, native and optexe
    - full release gate green
  - stop_rule: name classification only; collection spelling predicates are TODO-5963 and TODO-5964.
  - notes: Do not run alongside TODO-5961, TODO-5963 or TODO-5966 (shared files).

- [ ] TODO-5965: Reflection-generated helpers build AST nodes through one builder set
  - owner: ai
  - status: ready
  - created_at: 2026-11-12
  - phase: Code reuse (docs/CodeReusePlan.md, section 8)
  - parallel_track: reuse-ast-builders
  - scope: src/semantics/SemanticsValidateReflectionGeneratedHelpers{,CloneDebug,Compare,Serialization,State,Validate}.cpp each re-define anonymous-namespace AST builders: `appendPublicVisibility` (5 copies), `makeFieldAccessExpr` (5, 2-3 distinct bodies), `makeTypeBinding` (5, 2-3 distinct bodies), and `makeI32LiteralExpr`, `makeU64LiteralExpr`, `makeBinaryCallExpr`, `makeNameExpr`, `makeStringLiteralExpr`, `makeReturnStatementExpr` (2-4 each). `makeI32LiteralExpr` (4 copies) and `makeNameExpr` (about 3) also exist elsewhere in semantics. Add one builder unit (include/primec/ast/AstBuilders.h with a .cpp, or a semantics-local unit if a builder needs validator state), reconcile the forked variants, and use it in all reflection files and the other copies.
  - acceptance:
    - each builder has one definition; the reflection files contain no local builder copies
    - `--dump-stage` output (semantic product and AST after semantics) of the reflection examples and tests is unchanged, compared through `stripDumpTimings`
    - full release gate green
  - stop_rule: AST builders only.
  - notes: Safe alongside TODO-5959, TODO-5969, TODO-5970.

- [ ] TODO-5970: Publication fact snapshots run one templated merge per fact kind
  - owner: ai
  - status: ready
  - created_at: 2026-11-12
  - phase: Code reuse (docs/CodeReusePlan.md, section 10)
  - parallel_track: reuse-semantic-facts
  - scope: `SemanticsValidator::rebindMergedWorkerPublicationFactSemanticNodeIds` (src/semantics/SemanticsValidatorSnapshotsPublicationFacts.cpp:140 to about 510) repeats, once per fact kind (direct call, method call, bridge, callable summary, binding, return, query, try, on-error and more, about 12), the sequence: key lambda, `pruneReplacedEntriesBySnapshotKey`, `appendMissingEntriesBySnapshotKey`, `stable_sort` with a hand-written comparator, `rebindSemanticNodeIdsBySnapshotKey`. src/semantics/SemanticsValidatorSnapshotsFacts.cpp has about 12 `*FactSnapshotForSemanticProduct` functions that build, key and sort a vector each, and src/semantics/SemanticsValidatorSnapshotLocals.cpp is the query/call/try variant. Add a per-fact-kind traits struct (key function, ordering tuple, node-id fields) and one template for each sequence.
  - acceptance:
    - the merge sequence and the snapshot builder exist once each as templates; per-kind code is a traits definition
    - semantic-product dumps of examples/ and the multi-worker semantics tests are byte-identical (compared through `stripDumpTimings`)
    - semantics publication time on the benchmark suite is not slower (numbers in the commit)
    - full release gate green
  - stop_rule: these three files and the traits unit only; frontend lookups are TODO-5971.
  - notes: Safe alongside TODO-5959, TODO-5969, TODO-5965.

- [ ] TODO-5971: Semantic product lookups are one template over index and fact members
  - owner: ai
  - status: ready
  - created_at: 2026-11-12
  - phase: Code reuse (docs/CodeReusePlan.md, section 10)
  - parallel_track: reuse-semantic-facts
  - scope: src/frontend/SemanticProductLookups.cpp (730 lines, 82% identifier-normalized self-duplication) has about 20 `semanticProgramLookupPublished*By*` functions of one shape (guard on id 0 or `InvalidSymbolId`, find in an index map, `lookupPublishedSemanticEntryByIndex`), about 14 `semanticProgram*Fact*` accessors and about 12 `*View` functions. Replace them with a template over (index-map member, fact-vector member), keeping the public names in include/primec as thin wrappers where callers need them.
  - acceptance:
    - SemanticProductLookups.cpp shrinks to about a quarter of its size (counts in the commit)
    - every caller compiles unchanged or with a mechanical rename; the lowerer and semantics tests pass unchanged
    - full release gate green
  - stop_rule: this file and its header only.
  - notes: Do not run alongside TODO-5970 if both edit include/primec/frontend/SemanticProduct.h.

- [ ] TODO-5973: SoA rewrite passes share one context and one walker
  - owner: ai
  - status: ready
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

- [ ] TODO-5979: `--collect-diagnostics` reports no false intra-body call diagnostics
  - owner: ai
  - status: ready
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

- [ ] TODO-5984: The http web apps lower in seconds, not minutes
  - owner: ai
  - status: ready
  - created_at: 2026-11-12
  - phase: Performance
  - parallel_track: web-app-lowering-time
  - scope: `primec --dump-stage ir-lowered` on examples/apps/hello_button/main.prime takes about 15 s of CPU (5 s for web_app, about the same for text_editor and todo_web) in a release build, measured 2026-11-12 with `/usr/bin/time -p`; under the gate's load a single example passes 60 s of wall time (rc=124). The four examples then end in an expected error for the native target (`native backend does not support recursive calls: /std/http/server/run_workers__t...` or `host calls are only supported by the vm target`), so the time is spent before the result is known. Profile the run (semantic validation of the /std/http and /std/ui imports versus monomorphization versus lowering), find the dominant cost and remove it.
  - acceptance:
    - the profile and the dominant cost are recorded in docs/todo_log.md under this id
    - `--dump-stage ir-lowered` on the four examples takes at most a third of the recorded CPU time, with output unchanged
    - full release gate green
  - stop_rule: the cost the profile names; no unrelated compiler speedups.
  - notes: Found while checking the IR comparison of TODO-5963; the error text is the same before and after that change.

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

- [~] TODO-5989: Control-flow call names, simple-call-name tests and return kinds are classified once
  - owner: ai
  - status: ready
  - claimed_by: reuse-b (worktree-agent-a051c39accd554d52), 2026-11-13
  - created_at: 2026-11-13
  - phase: Code reuse (docs/CodeReusePlan.md, section 4; closes TODO-5962)
  - parallel_track: reuse-builtin-names
  - depends_on: TODO-5988
  - scope: What is left of TODO-5962 after the math names: `isSimpleCallName` (src/semantics/SemanticsCallPathHelpers.cpp:123, src/ir_lowerer/IrLowererHelpers.cpp, src/ir/IrPrinterHelpers.cpp), `isReturnCall` (SemanticsCallPathHelpers.cpp, IrLowererHelpers.cpp, IrPrinterHelpers.cpp, src/ast/AstPrinter.cpp:17), `isIfCall`, `isLoopCall`, `isWhileCall`, `isForCall`, `isRepeatCall` (semantics, lowerer, printer), `trimLeadingSlash` (SemanticsCallPathHelpers.cpp, SemanticsValidatorInferCollectionCompatibilityInternal.h three times, src/ir_lowerer/IrLowererCallHelpers.cpp), `getBuiltinConvertName` (semantics, lowerer, printer), and `returnKindForTypeName` (src/semantics/SemanticsBindingTypeHelpers.cpp:673, src/ir/IrPrinterHelpers.cpp:413) with the lowerer's `valueKindFromTypeName` (src/ir_lowerer/IrLowererSetupTypeHelpers.cpp:28). The printer and AST printer copies reject any name containing `/`; the semantics and lowerer ones read through a cached key-value member resolver. Put the single-segment test in support with a policy bit for the member-resolver spelling, and one type-name to return-kind table.
  - acceptance:
    - one definition of each listed predicate and of the return-kind table; the per-layer copies are deleted
    - a table test pins the spellings through each policy
    - ast-semantic, semantic-product, ir and ir-lowered dumps of examples/ are unchanged
    - full release gate green
  - stop_rule: name and type-name classification only; collection spellings are TODO-5963 and TODO-5964.
