# Build System: Plan

Classification: **design direction**. Nothing here is implemented. The leaves are the Build system lane of
`docs/todo.md` (TODO-5905 to TODO-5921). It is a companion to `docs/AiWorkflowPlan.md` (the project box view reads the
same project folder) and to `docs/MultithreadingPrototype.md`, VM Threads (steps that are PrimeStruct functions run on
VM threads).

## 1. Goal

A project builds with its own language. A `make.prime` in the project root, and in any subfolder, is a PrimeStruct
program that runs on the VM and describes how to compile the project: which compiler settings, which inputs, which
outputs. `primec build` runs it and executes what it describes. There is no second language and no generator step.

Why this fits PrimeStruct:

- **One language.** The people and the AI that write the project write its build, and the box view and the inspector
  work on a build script like on any other program.
- **The VM is deterministic and sandboxable.** A build script gets only the effects the runner grants, so what a
  build file may do is checked by the language and shown in review.
- **One source, many targets.** `--emit` already selects vm, native, exe, optexe, optcpp, wasm, glsl and spirv
  outputs; a build describes several of them from one set of sources.

## 2. What exists today

Checked in the sources on 2026-11-11.

| Piece | Where | State |
| --- | --- | --- |
| Compiler settings | `include/primec/support/Options.h`, `src/pipeline/CliUsage.cpp` | Everything a build needs is a flag already: `-o`, `--out-dir`, `--emit`, `--entry`, `-I`, `-O0` to `-O3`, `--opt-pass`, `--link-arg`, `--default-effects`, the transform lists, `--vm-schedule-seed` |
| Compiling as a library | `include/primec/pipeline/CompilePipeline.h`, `primec_embed` | The pipeline can run in-process; the embed API takes host functions bound by the embedder and a cancel flag (`include/primec/embed/Script.h`) |
| Compilation unit | `docs/spec/source-pipeline.md` | Whole program: imports flatten into one stream, so a build of `.prime` sources has an entry point and an import closure, not a list of files to compile one by one. The compiler records an expanded-source ledger of the files it read |
| The current recipe | `scripts/build_native_app.sh` | Compiles a program with `--emit=optexe` and links a UI backend through `--link-arg` (or `--direct` with the native backend) |
| Stdlib for a build script | `stdlib/std/os/os.prime`, `stdlib/std/file/file.prime` | Environment variable, CPU count, file size, reading a file, opening files. **No directory listing, glob, file times or path functions** were found |
| Host functions | `[host]` definitions, for example `ps_os_*` in `stdlib/std/os/os.prime`, bound in the VM host table and in `*_posix.cpp` for `optexe` | The pattern for new host calls |
| Effect checks | `docs/spec/language-core.md`, Effects | A definition without `[effects]` gets the default set (`io_out`); `--default-effects` changes it. There is no ceiling that caps what a definition may declare |
| Embed library | `scripts/check_embed_no_process.sh` | `primec_embed` must stay free of process spawning, so the build runner lives in `primec` |

## 3. The model

**`make.prime` builds a graph value.** Calls on a `Build` value add nodes to a `Graph`: compile steps, files,
settings. Nothing runs while the graph is built. The graph is an ordinary value in the script, so functions can return
subgraphs and a subfolder's script can return its own.

There are two ways to finish:

- **Return the graph (the convention).** `build` returns the `Graph` and `primec build` decides what happens: run it,
  or print it with `--dry-run`.
- **Run it in the script (staged builds).** `graph.run()` executes the graph at that point, so the script can read
  what a stage produced, such as the files a generator wrote, and build a second graph from it. The call needs a build
  effect (open name, `build_run`) that the runner grants. In a dry run the output ends at the first `run` and says it
  is incomplete, because later stages depend on results that do not exist yet.

The alternative of an executing script (every call runs its command at once) was rejected: a script that runs
commands needs a process effect, a dry run cannot be exact, and nothing can be parallel without hand-written waits. An
implicit-forcing variant (lazy handles) was rejected because it hides where execution happens; the explicit `run` has
the one dangerous call in plain sight.

## 4. The graph

| Node | Meaning | First version |
| --- | --- | --- |
| Compile step | A compiler snapshot (emit kind, optimization, import paths, entry, transforms, default effects, link arguments), an entry file and its output | Yes |
| Command step | An external command with declared inputs and outputs | Later, with the runner |
| Function step | A PrimeStruct function that runs in the runner on a VM thread, with declared inputs and outputs | After VM threads |
| Static library, link edge | A library target and a dependency of an executable on it | Appears in the export; `run` rejects it until the compiler supports libraries (section 7) |

- **Export** is deterministic JSON: a version, nodes sorted by id, ids derived from target names. Hashes are the
  runner's job (section 8), not the script's.
- **Not always self-contained.** A function step exports as the function's path and the files of its import closure;
  only the runner can execute it. The export is for inspection and for tools such as the box view.
- The script runs once and quickly; it needs only `file_read` to list files. Executing steps is the runner's work.

## 5. The script (illustrative, not run through the compiler)

```
import /std/build/*

[return<Graph> effects(file_read)]
build([Build mut] b) {
  [auto mut] cc{b.compiler()}
  cc.emit(optexe)
  cc.optimize(2)
  cc.import_path("stdlib")
  cc.link_arg("native/host.cpp")

  b.executable("primescroll", cc, "src/main.prime")
  return(b.graph())
}
```

- A compiler object holds the settings; copying it and changing `emit` gives the same program as a second target.
- File arrays come from `b.files("src/**/*.prime")` once glob exists; for `.prime` sources the compiler discovers the
  import closure itself, so arrays matter most for native glue passed through link arguments.
- Options: `b.option("native_ui", true)` is read from `primec build --option native_ui=false`, recorded in the export
  and in the cache key.

## 6. The run profile

A build script has different defaults from a normal program. They are set together by one named profile, `build`, and
not by a pile of flags. **The runner chooses the profile, never the file**: a `make.prime` cannot relax its own
sandbox, and a project that needs more is granted it in its configuration, where review sees it.

| Default | Program (today) | Build script |
| --- | --- | --- |
| Entry | `/main` | A stdlib wrapper that constructs `Build`, calls the script's `build`, and hands the graph to the runner |
| Default effects | `io_out` | None; printing goes through a logging call |
| Effect ceiling | None | A set the runner grants (`file_read`, `heap_alloc`); a larger declaration in user code is a diagnostic |
| Imports | Anything | An allowlist (`/std/build`, `/std/path`, `/std/file`, collections); not `/std/net`, `/std/ui` or process control |
| Optimization | `-O2` on the VM | `-O0`: the script runs once |
| Environment | `/std/os` | Only a call that records the variable as an input |
| Clock | `/std/os` `monotonicMs` | Not importable |
| Paths | Relative to the working directory | Relative to the folder of the `make.prime` |
| Task schedule | Eager or seeded | Deterministic |

The ceiling and the allowlist are checks on user code. The stdlib is trusted, and the host functions the runner binds
are what bounds it, as with the embed API.

## 7. Subprojects and libraries

- **Subprojects.** `b.subproject("examples")` runs that folder's `make.prime` under the same profile and merges its
  graph under its own namespace. Inclusion is explicit, as `add_subdirectory` is, so every target has a visible
  origin. Only the root script may call `run`. A file included twice is evaluated once; a cycle is a diagnostic.
- **Libraries (reserved).** The language is moving toward static libraries that link into an executable. The graph
  has the node kinds now, so the shape of a build does not change when the compiler catches up. What the compiler
  needs first is TODO-5921, a deferred leaf that must be split before it starts:
  - a **library is an IR module plus an interface plus its generic definitions.** Generics cannot be compiled ahead,
    because monomorphization needs the call sites; they ship as definitions and are instantiated in the consumer,
    with identical instantiations merged by content hash (`docs/LibrarySymbolManifestLazyImports.md`);
  - the **interface** is the outline of `docs/AiWorkflowPlan.md` section 3, and its hash lets a dependent skip a
    rebuild when only a body changed;
  - the **format** is proposed as serialized IR (PSIR): every backend consumes it, and the optimizer runs after
    linking, so inlining crosses library boundaries. Merging modules means remapping string-table indices, host
    imports and function names, which is the part not yet checked.

## 8. Caching

- The compiler writes the files it read (`--emit-deps`, TODO-5914); the ledger of source units already exists.
- A step's key is its compiler settings, the content hashes of its import closure and the compiler version. An output
  that exists with an equal key is up to date.
- The cache lives in `ProjectRoot/.ps/cache/` and is not committed; `.ps/metadata.json` of the AI workflow plan is.
- This is one mechanism with the cross-invocation symbol cache of `docs/LibrarySymbolManifestLazyImports.md` (its
  Phase 5), at a different granularity; it should not become a second cache.

## 9. VM threads

The runner executes independent steps in parallel itself, in C++. VM threads (`docs/MultithreadingPrototype.md`)
matter in two places: a step that is a PrimeStruct function runs on its own thread next to the others, and the
script's own tasks work as in any program. The runner stays in C++ because it has to exist before the compiler can
build anything; rewriting it in PrimeStruct is possible later.

## 10. Slices

Leaves in the Build system lane; the VM threads lane has its own list.

| Leaf | What |
| --- | --- |
| TODO-5905 | `/std/path`, textual path functions |
| TODO-5906 | `/std/file` lists a directory and reports kind, size and modification time (also needed by "Open PrimeStruct Project") |
| TODO-5907 | glob patterns over the listing |
| TODO-5908 | `--allow-effects`, the effect ceiling for user code |
| TODO-5909 | `--allow-import`, the import allowlist |
| TODO-5910 | `--profile build` |
| TODO-5911 | `/std/build` graph values and the JSON export |
| TODO-5912 | `primec build --dry-run` |
| TODO-5913 | `primec build` runs compile steps in parallel |
| TODO-5914 | `--emit-deps` |
| TODO-5915 | incremental builds from the cache |
| TODO-5916 | subprojects |
| TODO-5917 | options |
| TODO-5918 | staged builds with `run` |
| TODO-5919 | PrimeStruct-function steps |
| TODO-5920 | PrimeScroll builds with `make.prime` (replaces `scripts/build_native_app.sh` only after parity) |
| TODO-5921 | static libraries (deferred, to be split) |

## 11. Open decisions

- **In-process or separate compiler per step.** In-process is faster; a separate process isolates a compiler crash.
  TODO-5913 starts in-process through the pipeline and records what it finds.
- **The name of the run effect** (`build_run` here) and whether only the root script may hold it (proposed).
- **Whether the ceiling applies to library code.** Proposed: user code only.
- **How a subproject is loaded:** a separate VM run whose exported graph the runner merges, or one program that imports
  every script (needing one entry name per file). TODO-5916 decides and records it.
- **The library format** (section 7) and whether libraries merge before optimization.
- **File name:** `make.prime`, as proposed, or `build.prime`. `make` suggests GNU make's semantics, which this is not.
- **Fetching dependencies over the network** needs a lock file with hashes and the `net_io` effect; out of scope.
- **Tests and install steps** wait for `primec --test` of the AI workflow plan.
