# Integration Points and Tooling

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **implementation note / roadmap**.

## Integration Points
- **Build system:** extend CMake/tooling to run `primec`, track dependency graphs, and support incremental builds.
- **Testing:** unit/looped regression suites verify backend parity (C++, VM, GLSL).
- **Diagnostics:** metrics/logs land under `diagnostics/PrimeStruct/*`. Effect annotations drive error messaging.

### Run Profiles
`primec` and `primevm` take `--profile <program|build>`: a named set of defaults, so a runner does not pass a pile of
flags. `program` is today's behavior and the default; without the flag nothing changes. `build` is the sandbox of a
build script (docs/BuildSystemPlan.md, section 6):

| Setting | `program` | `build` |
| --- | --- | --- |
| Default effects | `io_out` | none, so a definition without `[effects]` cannot print |
| `--allow-effects` | none | `file_read,heap_alloc,build_run` |
| `--allow-import` | none | `/std/build,/std/path,/std/file,/std/collections,/std/dir,/std/glob,/std/text` |
| Optimization | `-O2` on the VM | `-O0` |
| `--vm-schedule-seed` | eager schedule | `0`, a fixed deterministic schedule |

Override rule: a flag given explicitly always wins over the profile's value of the same setting, whether it comes
before or after `--profile`; the order of arguments never changes the result. `PRIMESTRUCT_VM_SCHEDULE_SEED` also
wins over the profile. An unknown profile name is an argument error. The allowlist is prefix text only, so a prefix
such as `/std/build` does not need that module to exist. The runner picks the profile; a source file cannot choose or
relax its own.
```
[return<i32>]
square([i32] x) {
  return(x * x)
}

[return<i32>]
main() {
  return(square(2i32) + 1i32)
}
```
`primevm sum.prime --profile build` and `primevm sum.prime` both exit 5. With `import /std/net/*` added,
`--profile build` fails with `import /std/net/* is outside the allowed imports: /std/build,/std/path,/std/file,
/std/collections,/std/dir,/std/glob,/std/text`, and `--profile build --allow-import /std/net` lets it through. A definition that declares
`[effects(net_io)]` fails with `outside the allowed effects: file_read,heap_alloc,build_run`.

### Dependency List
`primec` and `primevm` take `--emit-deps <file>` (or `--emit-deps=<file>`): after a successful compile the file holds
the source files the compile read, one absolute, normalized path per line, sorted bytewise, without duplicates: the
primary input, every import (project files and stdlib imports), and the stdlib files that lazy imports included. Only
files that were read appear, so a program that imports nothing lists itself alone. It is the file-path list of the
expanded-source provenance ledger (docs/spec/source-pipeline.md, step 1). The flag changes no other output, including
`--dump-stage` text; a file that cannot be written is a diagnostic and exit code 2. A failed compile writes no list.
```
[return<i32>]
main() {
  return(4i32)
}
```
`primevm main.prime --emit-deps main.deps` exits 4 and `main.deps` holds the absolute path of `main.prime` and
nothing else. The cache of `primec build` (docs/BuildSystemPlan.md) hashes these files.

### Build Command
`primec build [--dry-run] [dir] [options]` finds `make.prime` in `dir` (default: the working directory) and runs it as a
build script (docs/BuildSystemPlan.md). It is the ordinary compile of that file with `--profile build --emit=vm`, so
the sandbox of the profile applies and a flag after `dir` such as `-I <dir>` or `--allow-effects` is passed on. The
entry is the stdlib function `/std/build_runner/run` (`stdlib/std/build_runner/build_runner.prime`): it constructs a `Build`, calls
the script's `build`, and writes the export of the returned graph to standard output. `primec` makes it reachable by
appending `import /std/build_runner/*` (a module of its own, because `import /std/build/*` takes every file of that folder, which `primec build` adds to the import allowlist) after the last line of the script, so line numbers in a diagnostic are lines of
`make.prime`, and the script's own `main`, if it has one, is not run.

```
primec build --dry-run tests/fixtures/build/project
```
prints the deterministic JSON of `tests/fixtures/build/build_graph.golden.json`, the same text the graph prints from a
plain program that builds those nodes. A directory without `make.prime` exits with 2
and `no make.prime in <dir>`; a type error, an effect outside the ceiling (`io_out` is one) or an import outside the
allowlist fails like any compile, with the position in `make.prime`. `primec` without `build` as its first argument
behaves as before.

Without `--dry-run` the command runs the graph: it starts `primec build --dry-run` on the same arguments for the graph
(so a broken script reports the diagnostics of an ordinary compile), parses the export and runs every `compile` and
`executable` node as one `primec` process with the project directory as its working directory. A step's compiler
settings become flags: `--emit=<kind>`, `-O<level>`, `--entry`, one `--import-path` per path, `--transform-list`,
`--default-effects` (an empty list leaves the default effects of `primec`), one `--link-arg` per argument,
`--out-dir <out-dir> -o <output> <source>`. Paths in a step (source, import paths, link arguments) are relative to the
project directory.
`--out-dir <dir>` (default `<dir>/out`) is where outputs are written; `--jobs N` (default: the core count) bounds the
steps that run at once. Both are options of the command and are not passed on to the compile of the script. Each
finished step prints `built <id>` to standard output, in id order. A failed step prints
`primec build: step <name> (<id>) failed` and the compiler's diagnostics to standard error, steps that had not started
print `... skipped after a failure`, and the exit code is 1. A graph with a library or link node, or with a problem
(duplicate id, link to a missing target), runs nothing and exits 1 with `static libraries are not supported by this
compiler yet` or the problem. There is no command or function step yet.

Stages. A script that must read what a step wrote (the files a generator produced) calls `graph.run()` on the graph it
has built so far and then builds a second graph; `build` declares `effects(build_run, ...)`, which the profile grants
(a definition that calls `run` without declaring it is the ordinary `requires build_run effect` diagnostic, and a
subproject script is not granted it, see Subprojects). The command starts the dry run once per
stage with the internal option `--stage <n>` (default 0), which reaches the script as its first program argument:
the `run` that is call number `n` writes the export of the graph it was called on, with `"incomplete":true` after
`problems`, and ends the script, so nothing after it executes; an earlier `run` returns at once. The command runs
that graph and starts stage `n + 1`, until a dry run finishes without an incomplete export, which is the graph
`build` returns. So `primec build --dry-run` shows the first stage and the marker, because the later stages depend on
results that do not exist yet. Output: a build with more than one stage prints `stage <k>` before the steps of stage
`k`; a failed stage stops the build. The stages share one cache, so a step that a later graph repeats is `up to
date`; each stage reads the file system again, so a glob after a `run` sees the files the stage wrote. A script that
is run as a plain program (no stage argument) treats `run` as a no-op. A script with more than 256 stages is an
error. A step kind that writes PrimeStruct source does not exist yet (function steps, TODO-5919), so the first
stage of a script today writes other files (`--emit=ir` output, executables).

Subprojects. `b.subproject("dir")` (`/std/build`) includes the graph of `<dir>/make.prime`; the call only records the
folder, in `"subprojects":[...]` of the script's own export. `primec build` runs the project's script, then the script of
every folder named, and the scripts those name, each in a `primec build --dry-run --raw` VM run of its own (the internal
option `--raw` prints one script's export alone; the scripts of a level run at once), and merges the exports. The
plain `--dry-run` prints the merged graph, so a project that names no subproject prints exactly the export of its script.
A subproject runs with its folder as the project directory of its script, under the options of the project and an effect
ceiling without `build_run`: a subproject script that declares `build_run` or calls `Graph.run` fails like any script
outside the ceiling, with the position in `<dir>/make.prime`; only the root script has stages. Merging: a node's id and
the ends of a link get the prefix `<dir>/` (`app/exe:app`), the output name moves under the folder (`app/app`, built in
`<out-dir>/app/app`), and the paths of the node that are relative (source, import paths, watched directories, and the
link arguments that are not options and name an existing file of the subproject) become relative to the project root
(`app/src/main.prime`); an absolute path stays. The problems of a subproject get the prefix `<dir>: `. The merged nodes
are sorted by id as in a single export, and `"subprojects"` is dropped. `<dir>` is the folder relative to the project root,
normalized (`a/../shared` is `shared`), so a folder two scripts name is run and merged once (a diamond). A folder outside
the project (absolute, or reaching `..` past the root), one without `make.prime` and an inclusion cycle are diagnostics:
`primec build: make.prime: subproject "../x" is not a folder inside the project`, `... subproject "x" has no make.prime
(<path>)` and `primec build: subproject cycle: make.prime -> a/make.prime -> make.prime`. Each script keeps its own script
cache in its folder's `.ps/cache`, whose entry lists every file that script's compile read, so editing a subproject's
script or an import of it recompiles that script only; the step cache of the merged steps is the project's and keys on the
rebased arguments.

Glob inputs. A script that expands a pattern with `/std/glob` (docs/spec/stdlib-reference.md) passes the directories
the glob listed to `b.watch("compile:app", listed)`, after the target is added. The graph export then carries
`"inputDirs":[...]` on that node (sorted bytewise, without duplicates, written only when there are any), and a watch of a
target that is not in the graph is a problem of the export. The cache file of the step has one `dir <hex> <path>` line
per watched directory: the SHA-256 of that directory's listing, one level, names and kinds (file, directory, other)
sorted bytewise. A name that appears, disappears or is renamed in a watched directory, or a directory that vanishes or
turns into a file, makes the step stale; a file rewritten in place does not (a compile step hashes the files it read
itself). The runner does not know which entries matched, so a new file that does not match the pattern but sits in a
watched directory also runs the step. The sorted list of watched directories is part of the step key, so a script that
starts to watch another directory is a different step. A relative path is relative to the project directory.

Incremental builds. Each step has a cache file in `<dir>/.ps/cache/` (not committed; `.ps/cache/` is in `.gitignore`),
named after its id. A step is skipped, and prints `up to date <id>` instead of `built <id>`, when its output exists and
nothing it depends on changed. The file is text: the line `primestruct-build-cache 1`, `hash sha256` (the scheme; a
file of another scheme is ignored), `key <hex>`, `output <hex>` and one `dep <hex> <path>` line per input. The key is
the SHA-256 of the hash scheme, the SHA-256 of the running `primec` executable (the compiler version) and the compiler
arguments of the step (emit kind, optimization, entry, import paths, transforms, effects, link arguments, output
directory, output name, source). The inputs are the files `--emit-deps` listed for the step's last run (the primary
file, its imports and the stdlib files it included; `/std/math` is builtin and never listed) and every link argument
that names an existing file. A step is current when the key is equal, the output has the recorded content hash, and
every input still has its recorded content hash and every watched directory its recorded listing hash; file times play
no part. A step that fails, or whose inputs cannot be hashed, leaves no cache file. Deleting `.ps/` or a damaged cache file makes the steps run again and produce the same
bytes. Not covered: a file that appears earlier on an import path than the one a step read (the compiler lists only
files it read) and environment variables a host toolchain reads. There is no early cutoff on interface hashes (TODO-5929) and no shared cache.

The compiled `make.prime` has a cache entry of its own in the same directory (`make-script-<digest>.cache` and `.cache.ir`): the serialized VM IR of the script, keyed by the compiler version, the compile arguments, the text of the script (with the appended runner import) and the import paths, with the content hashes of every file the compile read as dependencies. `primec build` (and `--dry-run`) loads a current entry instead of compiling the script and then runs it, so the script's globs and file reads happen on every run; a script that fails to compile stores nothing. The same limits apply (a file that appears earlier on an import path is not seen).

The effect ceiling (`--allow-effects`) checks user code: definitions under `/std/` and definitions whose source is a
stdlib file are trusted, including stdlib types that live outside `/std/` such as `/File`. That is why the runner can
write to standard output under a ceiling of `file_read,heap_alloc,build_run` while `make.prime` cannot declare `file_write`.

### Diagnostics & Tooling Roadmap
- **Phase 0 (now):** standardize diagnostic records (`severity`, `code`, `message`, `notes`, source span), include
  canonical call paths, and ensure CLI output is deterministic. Establish a stable JSON diagnostic export
  (`--emit-diagnostics`) for tooling/tests.
- **Phase 1 (source maps):** attach token spans to AST, keep span provenance through text/semantic transforms, and emit
  IR-to-source maps for VM/native/GLSL. Require every diagnostic to carry at least one source span.
- **Phase 2 (incremental):** content-addressed caches for AST/IR, dependency graph tracking for imports, and
  invalidation rules per transform phase. Add `--watch` to reuse caches and stream diagnostics.
- **Phase 3 (IDE/LSP):** go-to-definition, completion, and signature help using the same symbol tables as the compiler.
  Provide diagnostics in LSP format plus an editor adapter.
- **Phase 4 (runtime):** VM/native stack traces mapped via source maps, crash reports emitted with IR/AST hashes, and
  opt-in runtime tracing for effect/capability usage.

### VM Debug Event Ordering
- `VmDebugSession` hook callbacks are emitted in one total order with a monotonically increasing `sequence` value that
  starts at `0` on each `start(...)`.
- Per instruction, hooks fire in this order: `beforeInstruction` -> zero or more in-instruction call events (`callPush`
  / `callPop`) -> `afterInstruction`.
- Faulted instructions emit `beforeInstruction` and then `fault`; they do not emit `afterInstruction`.
- For identical IR + entry arguments + control flow (`step`/`continue` decisions), the emitted hook event stream is
  deterministic and replayable.
- `primevm --debug-json` emits one JSON object per line (`version`, `event`, and event-specific fields). Hook records
  include `sequence` and `snapshot`; stop records include `reason` and a terminal snapshot.
- `primevm --debug-json-snapshots=stop|all` adds `snapshot_payload` on demand; `stop` limits payloads to `stop` events,
  while `all` includes payloads on session/hook/stop events.
- `primevm --debug-trace <path>` records deterministic VM events to a file (NDJSON) including hook `sequence` ordering,
  snapshots, and snapshot payloads; repeated runs over identical inputs must produce byte-identical trace logs.
- `primevm --debug-replay <trace>` restores a deterministic checkpoint from a captured trace and emits a single
  `replay_checkpoint` record (`target_sequence`, resolved `checkpoint_sequence`, event/reason, snapshot,
  snapshot_payload).
- `primevm --debug-replay-sequence <n>` selects the latest checkpoint with `sequence <= n` for time-travel; without an
  explicit sequence, replay uses the terminal checkpoint and mirrors traced exit codes for terminal `stop`/`Exit`
  checkpoints.

### VM IR Breakpoints
- `VmDebugSession` supports IR-level breakpoints keyed by `(functionIndex, instructionPointer)` through `addBreakpoint`,
  `removeBreakpoint`, and `clearBreakpoints`.
- `resolveSourceBreakpoints(...)` maps source locations (`line`, optional `column`, optional source-unit/file identity)
  to executable IR breakpoint locations via canonical source-map entries; line-only requests reject ambiguous
  multi-column matches with explicit diagnostics, and source-unit filtering disambiguates identical positions in
  primary/imported files.
- `VmDebugSession::addSourceBreakpoint(line, column, resolvedCount, ..., sourceUnit)` resolves and installs all matching
  IR breakpoints in one call; `resolvedCount` reports how many executable locations were mapped.
- Breakpoints are checked in `continueExecution` before instruction execution and stop with reason `Breakpoint`.
- Continuing from a breakpoint resumes past that same location once (prevents immediate repeat-stops at the same
  `(functionIndex, instructionPointer)`), then normal breakpoint checks resume.

### VM Fault Stack Traces
- `VmDebugSession` fault diagnostics append a deterministic mapped stack trace when source-map metadata is available.
- Stack frames are listed from faulting frame to caller frames and include function path, instruction pointer,
  instruction debug ID, and mapped source span (`source_unit:line:column` when available, otherwise `line:column`) with
  provenance.
- Caller-frame mapping reports call-site instructions (the `Call`/`CallVoid` slot) rather than the post-call instruction
  pointer.

### VM Debug Adapter (MVP)
- `VmDebugAdapter` (`primec/VmDebugAdapter.h`) translates VM debug-session behavior into debugger-facing primitives:
  - execution control: `launch`, `continueExecution`, `step`, `pause`
  - debugger queries: `threads`, `stackTrace`, `scopes`, `variables`
  - breakpoints: `setInstructionBreakpoints`, `setSourceBreakpoints`
- Stack-frame responses use source-map metadata when available (`sourceUnit`, `line`, `column`, provenance) and keep the
  same call-site mapping behavior used by VM fault stack traces.
- Adapter calls append deterministic transcript lines through `transcript()` to support protocol transcript tests and
  replay checks.
- Snapshot payloads now carry per-frame locals (`frame_locals`) so `scopes/variables` return concrete local values for
  non-top frames as well as the active frame.
- `primevm --debug-dap` adds stdio JSON-RPC (`Content-Length`) framing and a request router for `initialize`, `launch`,
  `setBreakpoints`, `setInstructionBreakpoints`, `threads`, `stackTrace`, `scopes`, `variables`, `continue`, `next`,
  `pause`, and `disconnect`.

### Semantics Parallelism (Investigation)
We plan to parallelize semantic validation across root functions using a
deterministic diagnostics pipeline. See
`docs/Semantics_Multithread_Design.md` for the canonical phase boundaries,
ownership model, and rollout plan.

### IDE/LSP Integration Plan
Goals:
- Deliver a first-class IDE/LSP experience powered by the compiler front end.
- Keep diagnostics and symbol resolution deterministic across CLI and LSP.
- Preserve transform provenance so hovers and completions map to source.

Scope (MVP):
- `primec-lsp` process that reuses the parser, transform pipeline, and semantic resolver.
- Diagnostics wired through the JSON export (`--emit-diagnostics`) for identical output.
- Core language intelligence: hover, go-to-definition, signature help, and completion.
- Workspace symbol search driven by import dependency graphs.

Architecture:
- Maintain a long-lived compiler service with per-file token/AST caches.
- Track a dependency graph keyed by canonical paths; invalidate in topological order.
- Store per-definition symbol tables and a reverse reference index for navigation.
- Use source maps (Phase 1) to map transformed diagnostics back to surface text.

Milestones:
- M1: LSP diagnostics and document symbols (requires Phase 0).
- M2: go-to-definition/hover/signature help with stable symbol tables.
- M3: completion and workspace symbols with incremental cache invalidation (Phase 2).
- M4: rename/code actions backed by reference indexing and stable IDs.

Out of scope (initial):
- Full editor integration packaging (for example VS Code extension manifests and launch profile templates).
- GPU-specific editor features beyond diagnostics and symbol navigation.
