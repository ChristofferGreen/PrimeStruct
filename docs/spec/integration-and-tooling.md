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
| `--vm-threads` | off | off (the seed applies) |
| `--vm-record-sync`, `--vm-replay-sync` | none | none |

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

### Runtime errors on Wasm and GLSL
A runtime error (a guard rail such as `pow` with a negative exponent) ends the program with exit code 3 on every
backend (docs/spec/runtime-model.md). On Wasm the IR `Trap` lowers by profile:

| Profile | `Trap 3` lowers to | What the host sees |
| --- | --- | --- |
| `--wasm-profile wasi` (default) | import `wasi_snapshot_preview1.proc_exit` called with 3, then `unreachable` | exit code 3; output written before it is already out; the message is on stderr (`fd_write` to 2) as on the VM |
| `--wasm-profile browser` | `unreachable` alone (no import) | a `WebAssembly.RuntimeError`; there is no stderr message and no exit code |

The profiles differ by design: the browser profile imports nothing from WASI, so it has neither the message nor
`proc_exit`. A runtime error in the entry returns 3 from `main` on both profiles. GLSL has no trap and no host to
report to, so a program whose called (not inlined) function contains a runtime error is a compile error
(`a runtime error inside the called function /name cannot end the program on this target`).

```
[return<i32>]
power_of_two([i32] exponent) {
  [i32] shifted{exponent + 0i32}
  [i32] base{2i32 * 1i32}
  [i32] result{pow(base, shifted)}
  [i32] doubled{result + result}
  return(doubled - result)
}
```
`power_of_two(0i32 - 1i32)` called from `main` exits 3 under WASI (`node` with a `proc_exit` import that unwinds) and
traps in the browser profile.

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

### IR Link
`primevm <program> --link <module.psir>` (repeatable) merges serialized IR modules into the program before it runs, and a
`.psir` program is loaded and run without compiling. A function that a library defines is an import in the program
(`IrFunction::isImport`, PSIR v38); the merge resolves it by name and every operand that names a function, a host import
or a string is rewritten. Errors: `duplicate definition of <name>`, `struct layout of <name> differs between the
modules`, `host import <name> has two signatures`, `undefined symbol <name> (library <lib>)`; each is a VM error with
exit code 3. The rules and the limit on strings are in docs/spec/vm-design.md (IR link). There is no archive and no
compiler front end for libraries yet (docs/ModulesAndLibrariesPlan.md).

### Module Interface
`primec --emit=interface <module.prime>` prints the interface of a module in canonical PrimeStruct syntax to standard
output, and `primec --emit=symbols <module.prime>` prints its symbol table as JSON. A module is a file (with the files
it imports) whose public definitions are the library; it needs no entry point. The compile runs through semantic
validation, with a stub `return<int> /main() { return(0i32) }` standing in for a missing entry (the stub, and the
entry path whatever it is, never appears in the output), so a module that does not validate is a diagnostic and exit
code 2. Both kinds print only what the module's own sources define: stdlib definitions that an import pulled in are not
part of it. Both are the first step of the library plan (docs/ModulesAndLibrariesPlan.md, section 5); they write no
archive (`--emit=lib`, below, writes one) and read a library only when `--library` names it.

The interface holds the public (`public`, and not `private`) definitions, sorted by path inside nested
`[namespace]` envelopes (a namespace's `summary` is printed on its envelope):

- a struct in full: its transforms that say layout (`pod`, `handle`, `no_padding`, `align_bytes(..)`, ...), every field
  with its visibility, type and initializer, and its static fields;
- an enum in its surface form (`Red = 0i32`, `enum<i64>` for another backing type). Validation rewrites an enum to a
  struct with static entries and keeps no mark of it, so an enum is recognized by its shape (a `value` field and static
  entries of the struct's own type built from integer literals); a struct of that shape prints as an enum, which
  validates to the same struct;
- a function as a declaration: `[public, effects(io_out), return<i32>, imported(ui), summary("...")] add([i32] a,
  [i32] b{2i32}) {}`. The return type is the one validation resolved, so a function written without `return<...>`
  prints its inferred type. The effects are the ones the definition declares, sorted: a caller needs the effects a callee declares, and a function that declares none needs nothing of its caller, whatever its body does under the default effects. `mut`
  and `static` are kept, because they decide how a method is called. The body is empty, and `imported(<library>)`
  marks it a declaration (see Compiling against a library); the library name is the root file's name without its
  extension, with every character that is not a letter, a digit or `_` written as `_`. A method nested in a struct is
  printed inside the struct, so its implicit `this` stays implicit; a lifecycle helper written beside the struct is
  a declaration with its full path;
- a generic function or struct in full, with its template arguments and its body (a generic struct with its fields and
  its methods, bodies included), as the source wrote it: validation replaces a generic by its instances, so the
  interface is printed from the program as parsed. The consumer instantiates it, so one copy of `Box<i32>` per
  program is made by the consumer and `Box<string>` and every other instance is made as in one program. The private
  definitions a generic reaches (by name, through any depth) are printed too and stay private: a private function or
  non-generic struct as a declaration (`[imported(ui)]`, the library lowers it and the consumer calls it), a private
  generic in full;
- a lifecycle helper (`Create`, `Destroy`, `Copy`, `Move`) of a struct in full, body included, in the struct. The
  lowerer expands these at each use instead of calling them, so the library has no function for one (it is not a root)
  and the consumer expands the printed body where a value is created, copied or destroyed. A private function such a
  body calls is reached like the one a generic calls. A struct with no helper of its own is copied and destroyed
  field by field, which the lowerer also expands at the use site, so the consumer synthesizes it from the struct's
  layout and the helpers of its fields;
- `summary` on structs, enums, functions and namespaces.

Transform lists are separated by commas, because an absolute type path after a space (`[static /ui/Color]`) would be
rewritten as a division by the operator filter. A private definition that no generic or lifecycle helper reaches, an
entry, a sum type and the stdlib are not printed.

```
[namespace summary("Drawing.")]
ui {
  [public enum] Color { Red  Green = 5 }
  [public effects(io_out) return<i32> summary("Adds two numbers.")] add([i32] a, [i32] b{2i32}) { return(a + b) }
  [return<i32>] hidden() { return(1i32) }
}
```
prints
```
[namespace summary("Drawing.")] ui {
  [public, enum] Color {
    Red = 0i32
    Green = 5i32
  }

  [public, effects(io_out), return<i32>, imported(ui), summary("Adds two numbers.")] add([i32] a, [i32] b{2i32}) {}
}
```
(the example's file is `ui.prime`). Struct and enum text reads back: printing it again gives the same text. A function
declaration is valid only as the interface of a library (below), where it validates like any definition; the signature
hash does not cover the library name.

The symbol table has one line per public symbol: `{"path":"/ui/add","kind":"function","signature":"..","content":"..",
"closure":".."}` with `kind` `struct`, `enum` or `function`, sorted by path. Every hash is SHA-256 of the text it covers
(the stdlib manifest's FNV-1a only detects drift):

| Hash | Covers |
| --- | --- |
| `signature` | The declaration without its `summary`: a struct or enum in full, a function without a body, with the full path |
| `content` | The whole definition after validation, body and `summary` included |
| `closure` | One hash over the `content` hashes of the symbol and of every definition it refers to, directly or through others (private and stdlib definitions included). The references are found by name, so the closure may include more than it needs, never less |

The table begins with `"interfaceHash"`: the SHA-256 of the sorted `path kind signature` lines. A change of a body or
of a `summary` leaves it, and every `signature`, alone; a change of a signature, or an added or removed public symbol,
moves it. Output is byte-identical for equal input.

### Library Archive
`primec --emit=lib <root.prime> -o <name>.psa` writes the library archive of a module (docs/ModulesAndLibrariesPlan.md,
sections 6 and 7): its interface, its symbol table and the unoptimized IR of its public functions, in one file. The
module is read as for `--emit=interface`: a file with the files it imports, no entry point needed (a stub stands in for
a missing entry, and an entry the module defines is lowered as that stub and dropped). `--with-sources` adds the
module's `.prime` files. `--library` consumes it (Compiling against a library, below).

**Code.** Lowering takes every public, non-generic function (a lifecycle helper excepted: it is expanded where it is used, so the interface carries it) and every private function a generic or a lifecycle helper reaches of the module's own sources as a root (`host` and
`compute` functions have no IR body and are left out), whether or not another definition calls it, and lowers what the
roots call with them: a callee with two call sites or more, or a recursive one, is a function of the archive (a stdlib
one such as `/std/collections/slice/Read` too, which the link shares with the program's own copy), and a callee with one call site is inlined into its caller as in any
program. The IR is lowered for the VM target, which gives every function the address ABI (a struct, a `mut` parameter or
a string is one slot that holds an address or an index; a consumer must call it with the same convention, so a
consumer target without that ABI is refused) and is not optimized or inlined: several passes are target-specific, so the link runs them on
the merged module. The module has `entryIndex` -1, its struct layouts, host imports, string table, source map and debug
slots; a stub entry that lowering needs stands in for `main` and is removed. A root the real-call ABI cannot take (an
argument pack, a `vector<T>` result, ...) is a diagnostic that names the function and the reason: `public function
/packed cannot be compiled into a library: parameter `values` is an argument pack` (exit code 2, no file). The source map
names each file relative to the root file's directory, so two checkouts of one source give one archive. Without `-o`,
the archive is `<root file name>.psa` in the output directory.

**Layout.** A fixed mark `PSLIBAR` and a line feed, the container version (u32, 1), the number of sections (u32), a
table with one entry per section (name length u32, name, size u64, SHA-256 of the payload as 64 hex digits) and then
the payloads in table order. Integers are little-endian. Sections are sorted by name:

| Section | Content |
| --- | --- |
| `code` | The serialized `IrModule` (`PSIR`, `serializeIr`) |
| `interface` | The text of `primec --emit=interface` |
| `manifest` | Lines of words: `primestruct-library 1`, `name <name>` (the root file's name without extension), `root <path>` (each owned namespace root, sorted: the first path segment of every public symbol, and below the stdlib the first three, so a stdlib library owns `/std/math/ColorRGB` and not `/std`), `stdlib <file>` and `stdimport <file> <path>` (only for a root file that lies in the stdlib: the file's path from the stdlib directory, `std/math/color.prime`, and the stdlib `import` paths it holds), `compiler <id>`, `irschema <version>`, `hash sha256`, `interface <hash>`, `library <name> <interface hash>` (each library the module imported, sorted by name) |
| `sources` | Only with `--with-sources`: the number of files (u32), then for each, sorted by relative path: path length u32, path, size u64, text |
| `symbols` | The text of `primec --emit=symbols` |

`compiler` is the SHA-256 of the `primec` executable that wrote the archive. There is no timestamp, no absolute path
and no host name in the file: two builds of one source, and builds from two checkouts, are byte-identical. A library is
usable only by the compiler build and the IR schema version recorded in it; there is no binary compatibility promise.

**Reading.** `primec::readLibraryArchiveFile` (`primec/pipeline/LibraryArchive.h`) loads and verifies an archive. Each
case has its own message, and the ones about an archive that is stale or damaged end in `; rebuild the library`:

| Case | Message begins |
| --- | --- |
| not an archive (no `PSLIBAR` mark) | `not a library archive` |
| another container version | `library archive format version N is not the version this compiler reads (1)` |
| cut short, trailing bytes, sections out of order, a manifest or code that does not load | `library archive is damaged (...)` |
| a section whose SHA-256 differs from its table entry | `library section <name> changed since the library was built (its hash differs)` |
| another compiler build | `library was built by another compiler build (<id>, this compiler is <id>)` |
| another IR schema version | `library was built with IR schema version 39, this compiler uses version 40` |
| another hash scheme | `library uses the hash scheme ..., which this compiler does not know` |
| manifest and symbol table with different interface hashes | `library manifest and symbol table name different interface hashes` |

The code must have no entry point. The checks run in this order: the container, the section hashes, the manifest, the
compiler build, the schema version, the interface hash, the code.

### Compiling Against a Library
`primec [--emit=...] --library <name>.psa [--library ...] <program.prime>` compiles a program against library archives
(docs/ModulesAndLibrariesPlan.md, sections 8 and 9) and links it with their code. Each archive is loaded and verified
as in Library Archive, Reading, with the message prefixed `cannot use library <path>:`.

**Import.** `import</ui>` (a logical import path that lies in a namespace root the library owns, `/ui/button` is in the
root `/ui`, or above one, `import</std/math>` names the roots `/std/math/ColorRGB` and `/std/math/Vec2` of two
libraries and splices both) splices the library's interface text into the source once, as a source unit of its own
(`SourceUnitKind::Library`, shown in diagnostics as the archive path, and listed by `--emit-deps`), however many imports
name it. The text is then ordinary source: `import /ui/*` and full paths work as in a program that defines the module.
An `import</root>` no library owns is `failed to read import`.

**Ownership.** A library owns the roots its manifest lists, and nothing else may define a path in them. Two libraries
whose roots are the same, or one inside the other, are refused when the set is loaded
(`library other and library ui both own /ui`). A definition in the program's own sources (the primary file and its
imports, not a library interface or the stdlib) at or below a root a library owns is
`/ui/draw is owned by library ui`, whether or not the program imports the library. A name that only starts like a root
(`/ui2`, `/app`) is not owned. Without `--library` nothing is checked.

**Stdlib libraries.** A library built from a file of the stdlib (`primec --emit=lib stdlib/std/math/color.prime`) owns
roots below `/std`, one per entry of its module (`/std/math/ColorRGB`, `/std/math/linearToSrgbChannel`), so the libraries
of the files of one module family (`color` and `vector` of `/std/math`) link together, and two that own overlapping roots
are refused as above. It also stands for its stdlib file: the stdlib pass of a program that links it does not read the
file (lazily or whole; `--emit-deps` does not list it) and splices the file's stdlib imports and the library's interface
(once) instead, so `import /std/path/*` alone brings the interface of the path library and `/std/text/*` for its
`StringView` parameters; `import</std/path>` before it is allowed and does the same splice first. The modules of the
family that have no library (matrix, quaternion) come from the stdlib source as before.

**The marker.** `[imported(<library>)]` declares a function the library defines. The declaration has an empty body and
an explicit return type, is not generic, and names exactly one library; each rule is a semantic diagnostic
(`imported declaration must have an empty body: /f`, `imported declaration requires an explicit return type: /f`,
`imported takes one library name, as in imported(ui): /f`). The marker is valid only in the interface of the library it
names: in any other source it is `imported is only valid in a library interface: /f`, and in the interface of another
library `imported(other) in the interface of library ui: /f`.

**Lowering and link.** A call of an imported function is a call of an import function (`IrFunction::isImport`) that
names the library. The link runs inside `prepareIrModule`, right after lowering and before validation and the
optimizer, so every back end sees one module: each library is merged by name (docs/spec/vm-design.md, IR link),
and `undefined symbol /ui/draw (library ui)` is reported for an import no library defines. Two decisions:

- *ABI.* An imported function is always called with the address ABI the archive was lowered with (a struct, `mut`,
  `this` or a string is one slot, a struct result goes through a block of the caller). A target without it (x86_64
  native, wasm, GLSL, the simple C++ emitters) is refused: `linking a library needs a target with the address ABI (the
  vm, optexe, optcpp and arm64 native): /ui/add is imported from library ui`. A declaration whose shape the ABI cannot
  take is `imported function /f cannot be called on this target: <reason>`.
- *Shared routines.* The archive holds the stdlib routines (`/std/...`) its roots reached as real calls, and the program
  lowers the same routines under the same names. A routine under `/std` that both define is one: the program's copy
  stays, the archive's is dropped, and the archive's callers use it. They are not compared; both come from the same
  stdlib source. The stdlib routines a library owns are not in this set: the program does not read their source. Any other name defined twice is `cannot link library ui: duplicate definition of /ui/f`.

**Sets of libraries.** A library compiled against another (`--emit=lib --library base.psa`) keeps the calls of its
functions as imports and records the library in its manifest. A set is checked when loaded: `library mid needs library
base` when `base` is not supplied, `library base changed since library mid was compiled; rebuild` when its interface
hash differs from the recorded one, `library base is supplied twice`. The link also checks that each interface the
program was compiled against is the archive's (`library ui changed since this program was compiled; rebuild`). A
program that uses no `--library` compiles as before.

**Uses.** `--emit-uses <file>` (or `--emit-uses=<file>`, on `primec` and `primevm`) writes, after a successful compile,
the library symbols the program resolved, one line each, `<path> <library> <signature hash> <content hash>`, sorted by
path, without duplicates, with the hashes of the library's symbol table (`--emit=symbols`). The walk starts at every
definition and execution of the program that is not a library's and follows names, resolved call targets and type
arguments through the library definitions it reaches, so a generic the program instantiated is listed under the
generic's path together with the private functions and types its body reaches (a function the library compiled is
listed, what its body calls is not: that code is in the archive). An overload is listed with its siblings. A program
that imports a library and refers to none of its symbols writes an empty file, and so does a compile with no
`--library`. The flag changes no other output; a file that cannot be written is a diagnostic and exit code 2, and a
failed compile writes no list. The build graph keys a step on this list (docs/ModulesAndLibrariesPlan.md, section 11).
```
import</ui>
import /ui/*

[return<i32>]
main() {
  return(add(1i32, 2i32) + sub(5i32, 1i32))
}
```
With `ui.psa` built from a module of five public functions, `primec --emit=optcpp --library ui.psa --emit-uses main.uses
main.prime -o main.cpp` leaves `main.uses` holding two lines, `/ui/add ui ...` and `/ui/sub ui ...`.

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
print `... skipped after a failure`, and the exit code is 1. A graph with a problem
(duplicate id, link to a missing target, a cycle of libraries) runs nothing and exits 1 with the problem. There is no
command step.

Libraries in a graph. A `library` node is a compile step with `--emit=lib` (no `--entry`) whose output `<name>.psa` goes to
the output directory. A `link` edge from an executable, a compile step or another library to a library makes the runner
pass `--library <out-dir>/<name>.psa` for the whole transitive closure of the libraries the step links (an executable
that links `mid`, which links `base`, is compiled with both, as a set must be complete) and start the step after them.
Libraries build in parallel with each other; a library that several steps link is one node, so it builds once. A library
that fails to build is reported by name, and the steps behind it are skipped. A cycle of libraries is a problem of the
graph. The cache is the one of Library steps below: a step that links libraries is rebuilt only when a symbol it used
changed.

Function steps. `b.step(name, inputs, outputs, source, function)` (`/std/build`, TODO-5919) adds a step that is a
PrimeStruct definition: `primec build` compiles the file `source` (relative to the project; the project directory is an
import path) with `function` as the entry, in the process of `primec`, and runs it on a VM thread next to the other steps
(at most `--jobs` steps at once; a task the definition spawns runs on its own thread too). The definition takes no
arguments and returns `i32`; 0 is success, any other value fails the step (`build step <name>: <function> returned <n>`).
Sandbox: the compile holds the definition to an effect ceiling of `heap_alloc`, `file_read`, `io_out`, `io_err`, `task`,
`atomic`, `lock` and `channel`, so it cannot declare `file_write`, `net_io` or `build_run`; the effect list the definition
declares is the sandbox inside that ceiling. It writes files with `write_output` of `/std/build_step`, whose host call
refuses a path outside the output directory with `build step <name> writes outside the output directory: <path>`, so the
step fails and prints that. What the definition prints is kept and shown only when the step fails. Declarations: every
input must be a file of the project (`the declared input <path> is not a file`), every output must be a place inside the
output directory, and a step that returns 0 without writing a declared output fails (`the step did not write its
declared output <path>`). Compiling is done one step at a time before the steps run, because the compile pipeline keeps
process-wide state; running is parallel. Steps are not ordered with respect to each other unless the script says so:
`b.after(stepId, dependencyId)` (call it after `stepId` is added; the ids are the node ids, `"compile:generated"` and
`"step:gen"`) makes `stepId` start only when `dependencyId` has finished successfully, so a generator and the compile of
what it writes build in one stage. It works between any two steps (compile, copy, function), is exported as
`"after":[ids]` (sorted, without duplicates; a node without edges has no `after` member, so an unchanged graph prints as
before), and the cache stays per step. When a step fails, no further step starts: the steps behind it are reported as
`skipped after a failure`. A dependency that is not in the graph (`step <id> waits for a missing step <dep>`) and a cycle
(`step <id> is in a cycle of after edges or waits behind one`, for every step that never becomes ready) are problems of
the export, and the runner checks the edges again before it starts anything. In a subproject the ids in `after` are
prefixed with the folder like the node ids. A step that needs a file another step wrote can also run in a later stage
(`graph.run()`), as the generator example below does.
Cache: a function step is cached in `.ps/cache` like a compile step. Its key is the hash of the compiler, the file, the
function, the inputs, the outputs and the output directory; its entry holds the hash of every file the compile read (the
file, the files it imports, the stdlib) and of every declared input, and the hash of all outputs. The step is up to date
when the key, all those hashes and every output are unchanged, so editing the definition, a helper it imports, an input
or an output re-runs only this step, and a step that is up to date is not even compiled. A file the definition reads
without declaring it as an input is not tracked.
Export: the node is `{"id":"step:<name>","kind":"function","name":..,"source":..,"function":..,"inputs":[..],"outputs":[..]}`
and the export has `"selfContained":false`, because only the runner can execute the graph. `primec build --dry-run` also
compiles each function step and adds `"closure":[..]` after `outputs`: the files of the import closure that are inside the
project (relative, sorted; the stdlib belongs to the compiler version and is not listed). A step file that does not compile
fails the dry run with `primec build: step <name> (<id>) does not compile` and the compile diagnostics. In a subproject
the `source` and `inputs` are rebased like other paths; the `outputs` are not, because the definition writes them by
name.

```
b.step("gen", inputs, outputs, "tools/gen.prime", "/generate")   // writes out/generated.prime
```

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
error. A function step (above) can write PrimeStruct source, so the first stage of a script can generate the files that
the second stage compiles (`tests/unit/backend_host/test_build_function_steps.cpp`).

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

Options. `b.option(name, default[, description])` (`/std/build`) declares an option and returns its value for this run:
the value of `--option name=value` when the command line has one, else the default. The type is the type of the
default: `bool` (`true` or `false`), `i32` (decimal, optional `-`, within the 32-bit range) or `String` (any text,
which may be empty). `--option` is repeatable and may stand anywhere after the directory (`--option=name=value` also
works); the value is everything after the first `=`. The runner passes the options to every script of the build, as the
program arguments after the stage (`name=value`, sorted by name so the flag order does not matter), so the project
script, each subproject script and each stage see the same values; they are not part of the compile of the script, so
changing an option does not recompile it (the script cache key is unchanged), while a script that branches on an option
exports a different graph and so different step keys: only the steps the option changes run again. The export lists the
declared options after `problems`: `"options":[{"name","type","default","description","value"[,"error"]}]`, sorted by
name, with the default and the value as text (an entry has `"error"` when the given value is not of the type; its
`value` is then the default). Declaring a name again with the same type and default is one declaration; another type
or default is a problem of the export. Subproject options are merged by name: one entry per name, and a clash of type
or default is a problem prefixed with the folder. Diagnostics (exit code 1 unless noted; no step runs): a value of the
wrong type, `primec build: option level (i32) expects a 32-bit integer, got "2.5"`; an `--option` no script declares,
`primec build: unknown option nope (declared: level, native_ui)` (or `no script declares options`), which a staged
script reports only after its last stage because a later stage may declare it; an argument without `=`, with a name
that is not a name (letters, digits, `_`, `-`, `.`, starting with a letter or `_`), or without its value, and a name
given twice (`--option a was given twice`) are command errors (exit code 2). `--list-options` prints one line per
declared option, `name: type, default <default> - description` (a `String` default is quoted, an empty description is
left out), sorted by name, or `no options`; for a staged script it lists the options declared up to the first `run`.
There are no environment-variable options and no option files.

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
files it read) and environment variables a host toolchain reads. There is no shared cache.

Library steps. A compile step of the graph can name library archives (`"libraries"` in the `compiler` object of its node, paths relative to the project); each becomes one `--library <path>` argument, so the paths are part of the key but the archives are not inputs: they are left out of the `dep` lines. Such a step is run with `--emit-uses` as well, and its cache file holds one `use <library> <signature> <content> <closure> <path>` line per library symbol the compile used. The signature and content hashes are the ones of the use list; the closure hash is read from the archive's symbol table when the entry is written (a use list names public symbols only, and a body change of a private function a used function calls moves the closure hash and nothing else). The step is up to date, whatever else the archives hold, when its key, output and inputs are current and every used function name (overloads share one name) still has exactly the recorded signature, content and closure hashes in the symbol table of the library with that name. A changed body of an unused function, a new public function and a changed signature of an unused function leave the step up to date; a change under a used name, a removed library, or an archive that cannot be read runs the step. The output of a step that stays up to date can differ from a fresh link only by the code of library functions the program cannot reach. A library step whose uses cannot be read or matched leaves no cache file.

The compiled `make.prime` has a cache entry of its own in the same directory (`make-script-<digest>.cache` and `.cache.ir`): the serialized VM IR of the script, keyed by the compiler version, the compile arguments, the text of the script (with the appended runner import) and the import paths, with the content hashes of every file the compile read as dependencies. `primec build` (and `--dry-run`) loads a current entry instead of compiling the script and then runs it, so the script's globs and file reads happen on every run; a script that fails to compile stores nothing. The same limits apply (a file that appears earlier on an import path is not seen).

The effect ceiling (`--allow-effects`) checks user code: definitions under `/std/` and definitions whose source is a
stdlib file are trusted, including stdlib types that live outside `/std/` such as `/File`. That is why the runner can
write to standard output under a ceiling of `file_read,heap_alloc,build_run` while `make.prime` cannot declare `file_write`.

### Lowering diagnostics

A lowering error names the target that lowers, in the words of the emit kind: `vm backend`, `native backend`,
`C++ backend` (`--emit=cpp`), `exe backend`, `optimized C++ backend`, `optimized exe backend`, `wasm backend`,
`GLSL backend`, `SPIR-V backend` and `IR backend` (`--emit=ir`). The registry names them in
`IrBackendDiagnostics::loweringBackendName`.

A lowering error is located at the code the user wrote: the primary span is the position, in the user's file, of the
innermost expression whose lowering failed (`IrLowerer::lower` records it through `ir_lowerer::LoweringErrorLocator`).
An expression of the standard library that was inlined into a call of the user's is passed over for that call. The text
form prints it as `file:line:column`, and `--emit-diagnostics` as `primary_span`. A failure that no expression
reported, for example one found while the lowerer sets up the program, has no span. The column is that of the
text-filtered source when a text filter rewrote the line (an operator written as `a + b` is lowered as `plus(a, b)`).

A generated name does not reach a message: a definition the compiler made for a template instance
(`/std/result/Result__arity2__t...`, `/std/collections/vector/Vector__t...`) is spelled as the program writes the type
(`Result<i32, Oops>`, `Vector<i32>`). `primec/support/DiagnosticNames.h` does the rewrite from the template and the
arguments that the instance's `Definition` records (`instanceOfTemplate`, `instanceTemplateArgs`).

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
