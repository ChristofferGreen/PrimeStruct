# Modules and Static Libraries: Plan

Classification: **design direction**. Nothing here is implemented. It replaces the one-paragraph library note in
`docs/BuildSystemPlan.md` section 7 and is the design behind TODO-5922 to TODO-5933 (static libraries; the split is
section 16). It builds on `docs/LibrarySymbolManifestLazyImports.md`, whose deferred Phase 5 is the same
idea (a precompiled module interface, as in C++20 modules, Go export data and Rust metadata), and on `[namespace]` as a
definition (TODO-5841, `docs/AiWorkflowPlan.md` section 6.5).

## 1. Goal and terms

A project is split into parts that are compiled once, shipped as files, and linked into programs, so that a change in
one part does not make every other part be parsed, validated and lowered again, and so that the project can see the
public surface of a part without reading its code.

- **Module:** a namespace root that one part of the source owns, such as `/ui` or `/std/collections`. It is the unit of
  source, of ownership and of visibility. There is no new keyword: a module is the `[namespace]` envelope
  (`[namespace summary("...")] ui { ... }`) plus the `public` and `private` tags that already exist.
- **Library:** the compiled form of one module tree: its interface and its code, in one archive file.
- **Interface:** what a consumer needs in order to use a library without its source.
- **Link:** merging a program's code with the code of the libraries it uses, at the IR level, before optimization.

## 2. What exists today

Checked in the sources on 2026-11-11.

| Piece | Where | State |
| --- | --- | --- |
| Compilation unit | `docs/PrimeStruct_SyntaxSpec.md` (the expanded source has no module boundaries), `docs/spec/source-pipeline.md` | Whole program: imports splice into one stream that is parsed, validated and lowered as a whole |
| Stdlib module roots | `stdlib/std/modules.psmeta`, `docs/spec/type-system.md` (Stdlib Module Inclusion Manifest) | Maps a root such as `/std/gfx` to one source file |
| Per-symbol manifest | `include/primec/frontend/StdlibSymbolManifest.h`, `tools/generate_stdlib_manifest.cpp` | Each stdlib symbol has a path, a line range and a content hash (FNV-1a 64-bit, "not cryptographic"); the source slice is re-derived and re-verified against the hash |
| Lazy import expansion | `docs/LibrarySymbolManifestLazyImports.md`, Phase 3 | Landed and the default: an import pulls in only the symbols a program reaches, to a fixed point |
| Cross-process caching of validated symbols | the same document, Phase 5 | Deferred indefinitely: a stale hidden cache would be worse than a slow compile |
| Visibility | `docs/spec/transforms.md` | `public` and `private` on definitions control what an import can see; the default for a definition is private |
| IR module | `include/primec/ir/Ir.h`, `include/primec/ir/IrSerializer.h` | Functions, an entry index, a string table, struct layouts, an instruction source map, host imports; serialized with a schema version |
| Opcode operands | `include/primec/ir/IrOpcodeTable.h` | The table records which targets accept an opcode; **no table says which operands are function, string or host-import indices** (`Call`, `CallVoid`, `TaskSpawn`, `CallHost`, the string-printing and string-byte opcodes) |
| Lowering | `src/ir_lowerer/IrLowererLower.cpp` | Driven by one `entryPath`; whether it lowers only what the entry reaches was not checked |
| Canonical dump | `primec --dump-stage ast` | Looks like a debug rendering, not source that parses back (it prints `return plus(p.x, p.y)` and `Point(block() {  })`); not fed back to the parser, so a library interface needs its own printer or must reuse source slices |
| Build graph | `docs/BuildSystemPlan.md` | Reserves library and link node kinds |

## 3. The idea in short

A library is **the interface of a module as PrimeStruct text plus the unoptimized IR of its non-generic code, in one
deterministic archive**. A consumer is compiled against the interface (the import resolver reads it like source, with
function bodies absent), and linking merges the consumer's IR with the libraries' IR by name before the optimizer
runs. The build graph builds libraries, hashes interfaces symbol by symbol, and rebuilds a dependent only when a symbol
it used changed. The archive is an explicit file produced by the build, not a hidden cache.

This is the C++20 modules lever (a precompiled interface consumed without re-parsing the source), made visible, so the
risk that made Phase 5 of the manifest plan too dangerous (a stale cache shared by racing processes) does not arise:
the build graph owns invalidation, by hashes of declared inputs.

## 4. Modules

- **A module is a namespace root a library owns.** The library's manifest lists its roots. The linker and the compiler
  check that no two libraries own overlapping roots, and a program's own code may not define anything under a root a
  linked library owns. This turns today's "duplicate definition" error into an ownership rule and prevents shadowing.
- **Public surface:** the `public` definitions under the owned roots. Private definitions stay inside the library.
- **The stdlib is a module tree like any other** (`/std/...`), and `modules.psmeta` generalizes: a project's module
  roots are declared by the `make.prime` that builds them, not by a file the compiler ships.
- **A module needs a root file** that holds its `[namespace]` envelope and imports the rest of its sources, so a
  library has a single place to start, as a program has `main`.

## 5. The interface

What it contains, in canonical PrimeStruct syntax:

| Item | Form in the interface |
| --- | --- |
| A public struct or enum | Complete: fields, their types and visibility (a consumer needs the layout), static fields, `[summary]` |
| A public non-generic function | A declaration with resolved parameter types, resolved return type, `[effects]` and `[summary]`, and no body. It carries `[imported(<library>)]`, a marker the compiler accepts only in interface files |
| A public generic definition | Complete, with its body, because instantiation needs the call sites; monomorphization happens in the consumer |
| A private definition a public generic reaches | A declaration (or the definition, if it is itself generic), marked private and in the same namespace, so the visibility check of the generic's body still passes |
| Struct lifecycle helpers (`Create`, `Destroy`, `Copy`, `Move`) | Declarations like any non-generic function |
| Namespace envelopes | The `[namespace]` definitions with their summaries |

Two forms come from two stages: non-generic declarations need types the compiler has resolved (inferred return types
are rewritten by the `single_type_to_return` transform), while generic definitions are taken before semantic
validation, as written after text transforms. A **declaration printer** produces both; it is also what the formatter
of `docs/AiWorkflowPlan.md` section 8 needs.

Alongside the text there is a **symbol table**, one entry per symbol: path, kind, a hash of its signature, a hash of
its content (its own text, or for a generic its body), and a hash of the closure of symbols it references. The
interface hash is the hash of the sorted table. Hashes are SHA-256; the FNV-1a hash of the stdlib manifest only detects
drift and is not enough to be an identity. The table is the per-symbol manifest of the lazy-import plan, extended from
the stdlib to any library.

The interface is also the **outline** of the library for the AI workflow plan: the box view can draw a linked library
from its archive, with effects and summaries, without its source.

## 6. The archive

One file, `<name>.psa`, a simple container of named sections. Deterministic: sorted sections, no timestamps, no
absolute paths.

| Section | Content |
| --- | --- |
| `manifest` | Name, owned namespace roots, compiler build id, IR schema version, hash scheme, interface hash, the libraries it links with name and interface hash |
| `interface` | The interface text of section 5 |
| `symbols` | The symbol table |
| `code` | A serialized `IrModule`: unoptimized, no entry point, struct layouts, host imports, source map |
| `sources` | Optional: the `.prime` files, for the debugger, the inspector and the box view |

**Versioning:** a library is usable only by the compiler build and IR schema version recorded in it. A mismatch is
refused with a message that says to rebuild. There is no binary compatibility promise.

## 7. Building a library

- **Roots, not an entry.** A library has no `main`. Lowering takes every public non-generic definition as a root (this
  generalizes `--entry`), and everything those roots reach is lowered with them.
- **Unoptimized IR.** Several passes are target-specific (`if-convert` runs on native only, `loop-rotate` on the VM),
  so the library cannot know which are valid. The link runs the optimizer on the merged module, and `--ir-inline`
  crosses library boundaries.
- **Debug information.** Local debug slots and the instruction source map are stored; source units name files relative
  to the library, so a debugger finds them in `sources` or on disk.
- **Command line:** `primec --emit=lib <root.prime> -o <name>.psa`, with the library name and roots from the root file
  (the name is the file's name without its extension; the roots are the first path segments of the public symbols, and the first three below `/std`);
  `--with-sources` adds the `sources` section. `--emit=interface` prints the interface text and the symbol table
  without the code, for tests and for tools.

## 8. Compiling against a library

- `--library <name>.psa`, repeatable, registers a library. The import resolver finds its modules in the archive's
  interface and treats it as source; semantic validation, the effects check and visibility need no change except that
  a declaration with `[imported]` has no body.
- Lowering turns a call of an imported function into a `Call` to a function with no instructions, flagged as imported
  and naming its library. That is a schema change to `IrFunction`.
- The consumer records which library symbols it resolved, with their hashes: `--emit-uses <file>`, next to
  `--emit-deps` of the build plan. The build graph uses it (section 11).
- A transitive library (one the named library links with) is found through its manifest and must be supplied, or the
  link fails naming it.

## 9. Linking

The link runs in the compiler, after lowering and before `prepareIrModule`, so every backend sees one merged module and
no separate linker tool exists for the first version.

1. Load the libraries and their manifests, check compiler build and schema versions, and check that each interface
   hash the consumer was compiled against equals the library's: otherwise `library ui changed since this program was
   compiled; rebuild`.
2. Merge functions by name. A name defined twice is an error (`duplicate definition`), except an instantiation of a
   generic, which is allowed when the two bodies are identical after remapping (merged to one) and an error otherwise.
3. Merge struct layouts by name; two layouts of one name must be equal.
4. Merge host imports by name and signature.
5. Concatenate string tables and **remap every operand** that is a function index (`Call`, `CallVoid`, `TaskSpawn`),
   a host-import index (`CallHost`) or a string index (the string-printing and string-byte opcodes, string
   immediates). This needs a table that classifies each opcode's operand, with a check script in the style of
   `scripts/check_ir_opcode_table.py`, so a new opcode cannot be added without saying how it links.
6. Merge source maps and debug slots, shifting debug ids.
7. Resolve every imported function to its definition; an unresolved one is `undefined symbol /ui/draw (library ui)`.
8. Validate the merged module (`IrValidation`), take the entry from the consumer, and continue as today.

Status of step 1 to 8 for the IR link (TODO-5922; the merge itself is `primec/ir/IrLinker.h`, driven by
`primevm <program> --link <module.psir>`, and documented in docs/spec/vm-design.md, IR link): it covers steps 2 to 8
except the interface hashes and the manifest of step 1, and the generic merge of step 2, which wait for the leaves
that write them (TODO-5924, TODO-5926). The operand table of step 5 is `primec/ir/IrOperandTable.h`. Writing it
showed one thing the plan did not say: a string literal is a `PushI32` or `PushI64`, so no opcode says that its
operand is a string index. TODO-6031 closed that gap: lowering marks each string literal push
(`IrInstruction::isStringLiteral`, PSIR v39; a flag in the instruction's padding rather than a `PushString` opcode, so
the VM kernels, the emitters and the optimizer needed no change), and the linker shifts exactly the marked pushes
wherever the string flows, so a library function can take a string as a parameter or return a literal. A library
written before v39 has no marks and is refused with a message that asks for a rebuild; the pattern match that found
the pushes by their consumer is gone.

## 10. Generics, helpers and other hard parts

- **Instantiations in both places.** A library instantiates `Vector<i32>` for its own code and the consumer may do the
  same. Merging by identical content is the rule of section 9 step 2 (the content-addressed model argued in the
  manifest plan). Instantiated names are deterministic and content-derived (`/ui/Box__t<hex>`, from the template
  arguments), so no prerequisite was needed; the linker (`primec/ir/IrLinker.h`) merges two definitions of such a name
  when their code is the same and refuses them otherwise. Generics ship as parsed definitions (validation replaces a
  generic by its instances, so the interface is printed from the program before it), together with the private
  definitions they reach, which stay private; a private function is a declaration the library lowers.
- **Compiler-synthesized helpers.** Field-wise copy and destroy of a struct are expanded at each use site, never
  functions, and so is a user `Create`/`Destroy`/`Copy`/`Move`. The rule that came out: the interface prints the
  user's lifecycle helpers in full (the library compiles no function for them), and the consumer expands them as in one
  program.
- **Inference across the boundary.** Implicit template inference needs the call sites of a generic. Because generics
  ship as definitions and are instantiated in the consumer, a library's generic API infers as it does in one program.
- **Effects** come from the interface declaration; the archive hash ties the interface to the code it was built with.
- **Closures and `Function` values** passed across the boundary are ordinary values; a closure's code is in the
  library that defined it.

## 11. The build graph

- `b.library(name, cc, root)` is a node that produces a `.psa`; `exe.link(lib)` is an edge that becomes
  `--library` on the compile step. Libraries build in parallel; a library that links with another records it in its
  manifest, and the runner passes the transitive closure.
- **Early cutoff, by symbol.** A compile step depends on the symbols it used, with their hashes (`--emit-uses`), and
  not on the whole interface. A change to a body does not rebuild a dependent, and a change to a symbol the dependent
  does not use does not either. A body change still re-links, which is cheap compared with compiling.
- The cache holds archives and the use lists in `ProjectRoot/.ps/cache/`, as in the build plan; there is no second
  cache.

## 12. The stdlib as a library

Optional and last. Lazy import expansion has landed, and it already pulls in only what a program reaches, so most of
the front-end win is taken. A prebuilt `std.psa`, produced by the project's own build, would additionally skip
re-parsing and re-validating the non-generic stdlib per compile; generic collections would still ship as definitions.
Measure first with the benchmarks of the manifest plan and do it only if it shows a gain.

Measured 2026-11-20 (TODO-5931): no gain. A linked library costs about 1.2G instructions per compile, more than compiling the
same stdlib file from source (color: 1226M against 425M; vector: 1247M against 759M), and most stdlib modules do not build
with `--emit=lib` yet. Not built; see `docs/todo_finished.md`.

Measured again 2026-11-20 after the build id was hashed with SHA-2 instructions on arm64 (TODO-6092): a linked library now
costs 120M to 140M instructions over an empty `main` (61.5M), so a one-module library is 2 to 3.3 times cheaper than its
source (color 182M against 380M, vector 200M against 657M). A program that imports several modules is dominated by
`/std/text/*` (5750M of 7508M, `String`, which cannot be a library yet), and with text from source three libraries save
20.1 percent of it, one (path) 9 percent. The real stdlib is also not consumable as a library: every stdlib library owns
the root `/std` (two of them are refused) and an `import` of a module a library owns still splices the stdlib source
(duplicate definition). Still archived; the numbers, the list of modules that build and the open design points (the root of
a stdlib library, `String`) are in `docs/todo_log.md` under TODO-6093.

Decided 2026-11-20: roots below `/std`, so several stdlib libraries link, and the stdlib pass skips every module a linked
library owns (TODO-6097); the compiler build id becomes the linker's UUID with the full hash as the fallback (TODO-6093).

Done 2026-11-20 (TODO-6097). The decision said one root per module family, but `/std/math` holds the files color, vector,
matrix and quaternion, and a library is built per file, so one root per family would make color and vector refuse each
other. The root of a library below `/std` is therefore the first three segments of its public symbols (`/std/math/ColorRGB`,
`/std/math/linearToSrgbChannel`), derived from the interface, and the first segment elsewhere (`/ui`); two libraries conflict
only when a root of one lies in a root of the other. A library built from a stdlib file also records that file and the
stdlib imports it holds (`stdlib` and `stdimport` lines of the manifest), and the stdlib pass skips a recorded file without
reading it and splices the recorded imports and the library interface (once) in its place. `import</std/math>` splices every library whose root lies in or
below the path. `sharedRoots` stays for the stdlib routines a library's code reached, minus the rule that a library which
owned `/std` was exempt. Measured in docs/todo_log.md under TODO-6097: color and vector together make a program that imports
`/std/math/*` 27 percent cheaper, color, vector and path together make one that also imports the text modules 17.8 percent
cheaper, which is under the 20 percent bar, so the stdlib libraries are consumable but nothing is added to the default build.

## 13. Backends: what IR-level libraries do not buy

Linking at the IR level saves the front end (parse, validate, lower) and gives early cutoff. It does not save the back
end: `--emit=optexe` generates C++ for the whole merged module and compiles it, and `--emit=native` writes the whole
executable, so every link repeats that work. Separate compilation of the back end needs libraries that carry their own
C++ translation unit or machine code, linked by a real linker, which would also give C callers an `.a` file. That is a
second phase (section 15, last leaf) and needs an object writer for the native backend; it is not part of the first
version.

## 14. Other uses

- **Plugins:** the embed API (`Script`) could load a library at run time, since the VM runs IR. A PrimeScroll script
  could then use a precompiled library. Not committed; it follows if the archive format proves out.
- **Review:** an AI-written change to a library shows up as a change of symbol hashes, which is the "what changed"
  lens of the AI workflow plan; a change that leaves the interface hash alone is a body-only change.

## 15. Correctness

- **Differential harness.** The lazy-import plan held every change to a differential harness (whole-program against
  lazy). The same standard here: every program of a corpus is built as one whole program and as a library plus a
  consumer, and the two must give the same output and the same exit code; the merged IR must equal the whole-program
  IR after sorting functions by name, apart from instantiation order. This is the cache-off lane the manifest plan
  asks for.
- Golden tests of the interface text and the symbol table; negative tests for each link error of section 9; a
  determinism test (two builds give byte-identical archives).
- A library built on one machine and linked on another gives the same result (no absolute paths in the archive).

## 16. Slices

These are the leaves TODO-5922 to TODO-5933 of `docs/todo.md`, in this order: slice 1 is TODO-5922, slice 2 is
TODO-5923, slice 3 is TODO-5924, slice 4 is TODO-5925, slice 5 is TODO-5926, slice 6 is TODO-5927, slice 7 is
TODO-5928 and TODO-5929, slice 8 is TODO-5930 and the deferred slice 10 is TODO-5931 to TODO-5933. The differential
harness of slice 9 is part of the acceptance of TODO-5925 and TODO-5926, not a leaf of its own.

1. **IR link, no frontend.** Operand classification of every opcode (with the check script), the `IrFunction`
   imported flag (schema bump), and a merge of two serialized modules (`primevm a.psir --link b.psir`), tested with
   hand-built modules, including the remap and every link error.
2. **Interface and symbol table.** The declaration printer and `primec --emit=interface`: text plus symbol table with
   SHA-256 hashes, for a module root with no entry point.
   Landed (TODO-5923): `primec --emit=interface` and `--emit=symbols`, documented in
   `docs/spec/integration-and-tooling.md`, Module Interface. A function declaration is printed with an empty body `{}`
   until the `imported` marker gives declarations a grammar (TODO-5925); the interface hash covers `path kind signature`.
3. **Multiple-root lowering and `--emit=lib`.** Every public non-generic definition as a root; the archive writer and
   reader; deterministic output.
   Landed (TODO-5924): `primec --emit=lib`, documented in `docs/spec/integration-and-tooling.md`, Library Archive.
   Lowering takes the public functions as roots through `IrLowerer::lower(..., libraryRoots)`: each is reserved as a
   real call with the VM target's address ABI, behind a stub entry that is removed afterwards. A root whose shape the
   real-call ABI cannot take is a diagnostic (TODO-6032 for the shapes). The archive holds the stdlib functions its roots
   reached as real calls, which slice 4 must merge or leave out. The compiler build id was the SHA-256 of the `primec`
   executable (now the linker's UUID with that hash as the fallback), the manifest is lines of words, and the container is a table of sections with a SHA-256 each.
4. **Compiling against a library.** `--library`, the resolver reading the interface, `[imported]` declarations,
   lowering of imported calls, the link in the pipeline, the version and interface-hash checks.
   Landed (TODO-5925, in three parts, with a differential corpus of 23 programs on the VM and optexe): documented in `docs/spec/integration-and-tooling.md`,
   Compiling Against a Library. `import</root>` splices the interface of the library that owns the root; a call of an
   imported function always uses the address ABI, so a target without it is refused; the stdlib routines (`/std`) both
   the archive and the program hold are one (the program's copy stays).
5. **Generics and helpers.** Generic definitions in the interface, instantiation merge, synthesized helpers owned by
   their library, the private declarations a public generic reaches.
6. **Namespace ownership.** Owned roots in the manifest and the overlap and shadowing checks.
7. **Uses and early cutoff.** `--emit-uses` and the runner's symbol-level rebuild decision. `--emit-uses` has landed
   (docs/spec/integration-and-tooling.md, Compiling Against a Library, Uses): it walks the validated program from its
   own definitions through the library definitions they reach, and needed no new symbol-table entry, because the
   consumer holds the generics' bodies and the private declarations and finds what they reach itself. The runner's
   decision has landed too (docs/spec/integration-and-tooling.md, Build Command, Library steps): the step's cache
   entry records the used symbols, with their closure hashes read from the archive's symbol table (the use list has
   no closure hash, and a private function a used function calls changes only that one), and the archives are not
   dependencies. The compiler still links in the same run, so a step that stays up to date keeps an output that can
   differ from a fresh link by the dead code of unused library functions; a separate re-link is not made.
8. **Build graph nodes run.** `b.library`, `exe.link`, transitive libraries, parallel builds (TODO-5913 and the
   runner, then these). Landed (TODO-5930): the runner resolves `link` edges into the closure of `--library` arguments
   and `after` edges (`resolveBuildLibraryLinks`); `b.linkLibrary` links a library to another.
9. **The differential harness** of section 15, which runs as part of every leaf above from the fourth on.
10. **Later, deferred:** the stdlib as a library (measure first), libraries that carry back-end code (an object writer
    for the native backend, per-library C++), loading a library from the embed API.

## 17. Open decisions

- **Interface text or source slices.** The declaration printer (proposed) against slicing the original source with the
  manifest's line ranges. A printer gives canonical, resolved types and is reusable by the formatter; slices keep the
  author's text and comments but cannot give resolved return types or drop bodies.
- **The marker `[imported]`** and whether it is also the C foreign-function marker later.
- **One archive file or a directory.** One file is easier to hash and ship; a directory is easier to read.
- **Whether private declarations reachable from public generics go in the interface** (proposed) or the generic's
  body is re-resolved in the library's namespace.
- **Link inside the compiler** (proposed) or a separate `primec link` tool; the first is simpler for a first version.
- **Whether a library may be linked with a different compiler build**: refused here.
- **Dynamic libraries** are out of scope.
