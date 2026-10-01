# PrimeStruct TODO Log

## Purpose

This file is the live open-work queue for PrimeStruct.

- Keep only open work here: `[ ]` queued or `[~]` in progress.
- Move completed work to `docs/todo_finished.md`.
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
    evidence note, move the full block to `docs/todo_finished.md`, and remove
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
| TODO-5338 | Installable `primec_embed` library, CMake package, and example host | ready | embedding-packaging |
| TODO-5339 | Host function binding: call C++ callbacks from script | ready | embedding-host-calls |
| TODO-5342 | Precompiled bytecode: save/load scripts and a runtime-only embed library | ready | embedding-bytecode |
| TODO-5343 | iOS-safe embed build: no process spawning, bundled stdlib, cross-compile check | blocked | embedding-ios |
| TODO-5340 | Typed entry arguments and return values across the embed boundary | blocked | embedding-values |
| TODO-5341 | Embedding lifetime: arena scope, reentrancy, compile-once run-many | blocked | embedding-lifetime |

### Ready Now

- TODO-5342 (track: embedding-bytecode, surface: `src/embed/`, `include/primec/ir/IrSerializer.h`, new `primec_embed_runtime_lib`): precompiled bytecode + runtime-only library (iOS path: compile on desktop, ship IR).
- TODO-5338 (track: embedding-packaging, surface: `CMakeLists.txt` install rules, `examples/embed/`): installable library + example host.
- TODO-5339 (track: embedding-host-calls, surface: host-call IR opcode, VM, `src/embed/`): host function binding.

### Immediate Next 10

1. TODO-5342 - iOS-critical: lets a host ship precompiled IR and link only the VM.
2. TODO-5338 - packaging plus a real example host proves the API from outside the repo.
3. TODO-5339 - host calls make scripts useful (the point of embedding).
4. TODO-5340 - typed values in/out.
5. TODO-5341 - lifetime and reuse hardening.
6. TODO-5343 - iOS build recipe; needs a macOS runner to fully verify.

### Priority Lanes

- Embedding (top priority, user-set; must support iOS): TODO-5342, TODO-5338 -> 5339 -> 5340 -> 5341

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10; 5338 and 5339 touch disjoint surfaces and may run in parallel; 5340 follows 5339, 5341 follows 5340.

### Task Blocks

- [ ] TODO-5338: Installable `primec_embed` library, CMake package, and example host
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Embedding
  - parallel_track: embedding-packaging
  - scope: add `primec_embed` as a single static library target bundling the
    facade plus its subsystem libs, `install(TARGETS/EXPORT)` rules with
    `PrimeStructConfig.cmake` so a host project can
    `find_package(PrimeStruct)` and link `PrimeStruct::embed`, install the
    stdlib `.prime` tree and have `ScriptEngine` locate it relative to the
    installed prefix. Add `examples/embed/` with a minimal host C++ program
    and its own CMakeLists consuming the installed package.
  - acceptance:
    - CTest case installs to a scratch prefix, configures and builds
      `examples/embed` against it, and runs it, printing the script result.
    - `./scripts/compile.sh --release` still passes; no change to its
      options.
  - stop_rule: do not alter `scripts/compile.sh`; if install-prefix stdlib
    discovery needs a runtime option, add it to `ScriptEngine`, not the CLI.

- [ ] TODO-5339: Host function binding - call C++ callbacks from script
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Embedding
  - parallel_track: embedding-host-calls
  - scope: scripts cannot currently call back into the host. Define the
    surface (a declaration form for host-provided functions gated by an
    effect such as `[effects(host)]`, documented first in
    `docs/PrimeStruct.md`), add an IR call opcode for host calls with
    validation for the VM target, and `ScriptEngine::bind(name, fn)` for
    primitive signatures (i32/i64/f32/f64/bool). Unbound host functions fail
    at compile/link time of the script, not at run time. Native/wasm
    backends reject host calls with a clear diagnostic.
  - acceptance:
    - script calls a bound `add(i32,i32)` and a bound void callback that
      mutates host state; both observed from C++.
    - unbound or signature-mismatched binding yields a diagnostic.
    - IR format change carries a version/migration note (IR stability rule).
    - positive parse+IR test and negative diagnostic test added.
  - stop_rule: strings, structs and collections across the boundary are out
    of scope (TODO-5340); stop at primitives.

- [ ] TODO-5340: Typed entry arguments and return values across the embed boundary
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5339
  - created_at: 2026-10-01
  - phase: Embedding
  - parallel_track: embedding-values
  - scope: let the host call a named script function (not just `main`) with
    typed primitive arguments and read a typed return (`Script::call<T>(
    "name", args...)`), and pass/return strings. VM strings are string-table
    indices with no dynamic construction, so host-provided strings need a
    host string table that the VM can reference; return strings must be
    copied out before the VM is torn down.
  - acceptance:
    - call `fn(i32, f64) -> f64` and `fn(string) -> i32` from C++.
    - arity/type mismatch returns an error, not UB.
  - stop_rule: struct/array marshalling is a separate leaf; stop at
    primitives plus strings.

- [ ] TODO-5341: Embedding lifetime - arena scope, reentrancy, compile-once run-many
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5340
  - created_at: 2026-10-01
  - phase: Embedding
  - parallel_track: embedding-lifetime
  - scope: the CLI wraps compile+run in a process-wide `ScopedCompileArena`
    (see TODO-5233/5234/5235 notes in `docs/CompilerArenaAllocator.md`);
    an embedded engine must own its arena lifetime safely, support several
    engines and repeated `run`/`call` on one compiled `Script` without
    recompiling, and document thread-safety (one `Script` per thread, or
    guarded).
  - acceptance:
    - test compiles once and runs 1000 times with stable results and no
      memory growth (RSS or allocation-counter bound).
    - two engines alive at once on separate threads pass under TSAN smoke.
  - stop_rule: if the arena is inherently process-global, document the
    single-engine-per-process limit and stop rather than rewriting it.

- [ ] TODO-5342: Precompiled bytecode - save/load scripts and a runtime-only embed library
  - owner: ai
  - status: ready
  - created_at: 2026-10-01
  - phase: Embedding
  - parallel_track: embedding-bytecode
  - depends_on: TODO-5337 (closed)
  - scope: iOS forbids generating executable native code in-process (no JIT)
    and discourages heavy on-device compilation, but a bytecode interpreter
    is allowed - and the VM already interprets `IrModule`. Add
    `Script::saveBytecode()` / `ScriptEngine::loadBytecode(bytes)` using the
    existing `serializeIr` / `deserializeIr`, validating the loaded module
    for the VM target (`IrValidationTarget::Vm`) and rejecting a version
    mismatch with a diagnostic. Split the build so a host can link a
    `primec_embed_runtime_lib` (VM + IR deserializer + validation only, no
    parser/semantics/lowerer/stdlib) and compile scripts offline with
    `primec`/the full `primec_embed_lib`. Add a CLI path to emit the
    bytecode (check whether `primec --emit=ir` already produces the
    serialized form before adding a flag).
  - implementation_notes: check that `Vm` and `IrValidation` do not
    transitively require frontend sources; `primec_runtime_lib` currently
    links `primec_frontend_lib`, so the runtime-only target may need that
    link narrowed. Document the IR version/migration policy for shipped
    bytecode (AGENTS IR-stability rule).
  - acceptance:
    - test compiles a script, saves bytes, loads them through a binary that
      links only the runtime-only library, and gets the same exit code.
    - corrupt or wrong-version bytes return `ok=false` with a diagnostic,
      never UB.
    - size of the runtime-only library vs full embed library recorded in
      the result note.
  - stop_rule: if the VM cannot be separated from frontend sources without a
    wide refactor, stop and record the exact dependency edges as a new
    leaf instead of widening this one.

- [ ] TODO-5343: iOS-safe embed build - no process spawning, bundled stdlib, cross-compile check
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5342
  - created_at: 2026-10-01
  - phase: Embedding
  - parallel_track: embedding-ios
  - scope: make `primec_embed_runtime_lib` (and, as a stretch, the full
    `primec_embed_lib` for on-device compiling of small scripts) build for
    iOS: gate `fork`/`exec`/`posix_spawn` users (`src/support/ProcessRunner.cpp`,
    `ImportResolver` archive roots, `TempPaths`) behind a platform option so
    iOS builds exclude them; no reliance on `/tmp` or the working
    directory; provide the stdlib as a bundle path or embedded blob for the
    full-compile variant; ensure no executable-memory allocation. Add a CMake
    toolchain recipe (`-DCMAKE_SYSTEM_NAME=iOS`) and an XCFramework packaging
    script.
  - implementation_notes: this Linux CI cannot build or run iOS. Verify with
    a macOS runner (or have the user run the recipe) and record the exact
    toolchain version; until then acceptance covers the portable parts
    (a Linux build with the process-spawning sources excluded must pass the
    embed tests).
  - acceptance:
    - Linux build with `PRIMESTRUCT_EMBED_NO_PROCESS=ON` links and passes the
      runtime-only bytecode test.
    - documented, reproducible iOS cross-compile recipe; compile of
      `primec_embed_runtime_lib` for iOS arm64 verified on macOS.
  - stop_rule: if the full compiler pipeline cannot drop process spawning
    cleanly, ship runtime-only for iOS and record the gap.

