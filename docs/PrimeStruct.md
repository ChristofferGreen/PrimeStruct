# PrimeStruct Plan

PrimeStruct is built around a simple idea: program meaning comes from two primitives—**definitions** (potential) and **executions** (actual). Both map to a single canonical **Envelope** in the AST; executions are envelopes with an implicit empty body. Compile-time transforms rewrite the surface into a small canonical core. That core is what we target to C++, GLSL, and the PrimeStruct VM. It also keeps semantics deterministic and leaves room for future tooling (PathSpace integration, visual editors).

### Source-processing pipeline
1. **Include resolver:** first pass walks the raw text and expands every `include<...>` inline (C-style) so the compiler always works on a single flattened source stream.
2. **Text transforms:** the flattened stream flows through ordered token-level transforms (operator sugar, collection literals, implicit suffixes, project-specific macros, etc.). Text transforms apply to the entire envelope (transform list, templates, parameters, and body). Executions are treated as envelopes with an implicit empty body, so the same rule applies. Text transforms may append additional text transforms to the same node.
3. **AST builder:** once text transforms finish, the parser builds the canonical AST.
4. **Template & semantic resolver:** monomorphise templates, resolve namespaces, and apply semantic transforms (borrow checks, effects) so the tree is fully resolved.
5. **IR lowering:** emit the shared SSA-style IR only after templates/semantics are resolved, ensuring every backend consumes an identical canonical form.

Each stage halts on error (reporting diagnostics immediately) and exposes `--dump-stage=<name>` so tooling/tests can capture the text/tree output just before failure. Text transforms are configured via `--text-transforms=<list>`; the default list enables `collections`, `operators`, `implicit-utf8` (auto-appends `utf8` to bare string literals), and `implicit-i32` (auto-appends `i32` to bare integer literals). Order matters: `collections` runs before `operators` so map literal `key=value` pairs are rewritten as key/value arguments rather than assignment expressions. Semantic transforms are configured via `--semantic-transforms=<list>`. `--transform-list=<list>` is an auto-deducing shorthand that routes each transform name to its declared phase (text or semantic); ambiguous names are errors. Use `--no-text-transforms`, `--no-semantic-transforms`, or `--no-transforms` to disable transforms and require canonical syntax.

## Phase 0 — Scope & Acceptance Gates (must precede implementation)
- **Charter:** capture exactly which language primitives, transforms, and effect rules belong in PrimeStruct, and list anything explicitly deferred to later phases.
- **Success criteria:** define measurable gates (parser coverage, IR validation, backend round-trips, sample programs)
- **Ownership map:** assign leads for parser, IR/envelope system, first backend, and test infrastructure, plus security/runtime reviewers.
- **Integration plan:** describe how the compiler/test suite slots into the build (targets, CI loops, feature flags, artifact publishing).
- **Risk log:** record open questions (borrow checker, capability taxonomy, GPU backend constraints) and mitigation/rollback strategies.
- **Exit:** only after this phase is reviewed/approved do parser/IR/backend implementations begin; the conformance suite derives from the frozen charter instead of chasing a moving target.

## Project Charter (v1 target)
- Language core: envelope syntax (definitions + executions), slash paths, namespaces, includes/imports, binding initializers, return annotations, effects annotations, and deterministic canonicalization rules.
- Transform pipeline: ordered text transforms, ordered semantic transforms, explicit `text(...)` / `semantic(...)` grouping, and auto-deduction for registered transforms.
- Determinism: stable diagnostics, fixed evaluation order, stable IR emission, and canonical string normalization across backends.
- IR: PSIR serialization with versioning, explicit opcode list, and a stable module layout shared by all backends.
- Backends: C++ emitter and VM bytecode are required targets; GLSL/SPIR-V is a supported optional target with explicit documented constraints.
- Standard library: a documented core subset (math + collections + IO primitives) with a per-backend support matrix.
- Tooling: `primec`, `primevm`, `--dump-stage` and snapshot-style tests for parser/IR/diagnostics.
- Change control: any feature outside this charter requires a docs update plus an explicit acceptance gate.

## Risk Log (Phase 0)
Each risk lists a mitigation and a concrete fallback so the project can keep moving if the
open question is unresolved.

| Risk | Impact | Mitigation | Fallback |
| --- | --- | --- | --- |
| Borrow checker design remains unclear. | Resource safety rules vary by backend, leading to unsound or inconsistent behavior. | Scope borrow checking to opt-in transforms only; document minimal safety checks first. | Remove borrow-checking claims from v1 and gate on explicit transforms only. |
| Capability taxonomy is not finalized. | Effects/capabilities drift between docs, diagnostics, and runtime logging. | Define a minimal capability list (IO + memory + GPU) and lock it for v1. | Treat capabilities as diagnostic-only metadata until taxonomy stabilizes. |
| GPU backend constraints are underspecified. | GLSL/SPIR-V output may accept unsupported effects or types. | Publish an explicit GPU support matrix and reject unsupported effects/types early. | Mark GPU backend as experimental and limit to math-only + POD structs. |
| IR/PSIR versioning slips. | Backends diverge on opcode interpretation or serialized layouts. | Freeze v1 opcode set, add migration notes, and version the serialized header. | Reject unknown IR versions and require recompile in tooling. |
| Struct layout guarantees are ambiguous. | ABI/layout mismatches across native/VM/GPU. | Require layout manifests and add conformance tests per backend. | Document layout as backend-defined and disallow cross-backend struct reuse. |
| Default effect policy changes. | Behavior differs between CLI defaults and docs. | Keep defaults centralized in `Options` and tests covering defaults. | Require explicit `[effects]` in examples and templates. |
| Execution emission is unclear. | Executions parse but are ignored by some backends. | Decide and document whether executions are lowered per backend. | Treat executions as diagnostics-only until a backend implements them. |
| Stdlib surface drifts across backends. | Code runs on one backend but fails on another. | Maintain a per-backend stdlib support table + tests. | Provide a “core subset” and gate all samples to it. |

## Deferred Features (not in v1)
- Borrow checker and lifetime enforcement beyond basic effects gating.
- Full capability taxonomy and policy-driven capability enforcement (beyond documented effects).
- PathSpace integration beyond `notify`/`insert`/`take` helpers and basic runtime hooks.
- Full software numeric envelopes (`integer`/`decimal`/`complex`) and mixed-mode numeric ops.
- `class<Name>(...)` surface syntax and composition/extends semantics.
- Placement transforms (`stack`/`heap`/`buffer`) and placement-driven layout guarantees.
- Recursive struct layouts and cross-module layout stability guarantees.
- Metal backend and LLVM backend support.
- JIT, chunk caching, or dynamic recompilation tooling.
- IDE/LSP integration and PathSpace-native editor tooling.
- Standard library packaging/version negotiation beyond a single in-tree reference set.
- `tools/PrimeStructc` feature parity with the main compiler and template codegen.
- Tail-call or tail-execution optimization guarantees across all backends.

## Phase 1 — Minimal Compiler That Emits an Executable
Goal: a tiny end-to-end compiler path that turns a single PrimeStruct source file into a runnable native executable. This is the smallest vertical slice that proves parsing, IR, a backend, and a host toolchain handoff.

### Acceptance criteria
- A single-file PrimeStruct program with one entry definition compiles to a native executable on macOS (initial target).
- The compiler can:
  - Parse a subset of the Envelope syntax (definitions + executions).
  - Build a canonical AST for that subset.
  - Lower to a minimal IR (calls, literals, return).
  - Emit C++ and invoke a host compiler to produce an executable.
- The produced executable runs and returns a deterministic exit code.

### Example source and expected IR (sketch)
PrimeStruct:
```
[return<int>]
main() {
  return(42i32)
}
```

Expected IR (shape only):
```
module {
  def main(): i32 {
    return 42
  }
}
```

### Compiler driver behavior
- `primec --emit=cpp input.prime -o hello.cpp`
- `primec --emit=exe input.prime -o hello`
  - Uses the C++ emitter plus the host toolchain (initially `clang++`).
  - Bundles a minimal runtime shim that maps `main` to `int main()`.
- `primec --emit=native input.prime -o hello`
  - Emits a self-contained macOS/arm64 executable directly (no external linker).
  - Lowers through the portable IR that also feeds the VM/network path.
  - Current subset: fixed-width integer/bool/float literals (`i32`, `i64`, `u64`, `f32`, `f64`), locals + assign, basic arithmetic/comparisons (signed/unsigned integers plus floats), boolean ops (`and`/`or`/`not`), `convert<int/i32/i64/u64/bool/f32/f64>`, abs/sign/min/max/clamp/saturate, `if`, `print`, `print_line`, `print_error`, and `print_line_error` for integer/bool or string literals/bindings, and pointer/reference helpers (`location`, `dereference`, `Reference`) in a single entry definition.
- `primec --emit=ir input.prime -o module.psir`
  - Emits serialized PSIR bytecode after semantic validation (no execution).
  - Output is written as `.psir` and includes a PSIR header/version tag.
- `primec --emit=spirv input.prime -o module.spv`
  - Emits SPIR-V by first generating GLSL and invoking `glslangValidator` or `glslc` (compute stage).
  - Requires `glslangValidator` or `glslc` on `PATH`.
- `primevm input.prime --entry /main -- <args>`
  - Runs the source via the PrimeStruct VM (equivalent to `primec --emit=vm`). `--entry` defaults to `/main` if omitted.
- Defaults: if `--emit` and `-o` are omitted, `primec input.prime` uses `--emit=native` and writes the output using the input filename stem (still under `--out-dir`).
- All generated outputs land in the current directory (configurable by `--out-dir`).

## Goals
- Single authoring language spanning gameplay/domain scripting, UI logic, automation, and rendering shaders.
- Emit high-performance C++ for engine integration, GLSL for GPU shading, SPIR-V via external toolchains, and bytecode for an embedded VM without diverging semantics.
- Share a consistent standard library (math, texture IO, resource bindings, PathSpace helpers) across backends while preserving determinism for replay/testing.

## Proposed Architecture
- **Front-end parser:** C/TypeScript-inspired surface syntax with explicit envelope annotations, deterministic control flow, and borrow-checked resource usage.
- **Transform pipeline:** ordered text transforms rewrite raw tokens before the AST exists; semantic transforms annotate the parsed AST before lowering. The compiler can auto-inject transforms per definition/execution (e.g., attach `operators` to every function) with optional path filters (`/math/*`, recurse or not) so common rewrites don’t have to be annotated manually. Transforms may also rewrite a definition’s own transform list (for example, `single_type_to_return`). The default text chain desugars infix operators, control-flow, assignment, etc.; projects can override via `--text-transforms` / `--semantic-transforms` or the auto-deducing `--transform-list`.
- **Intermediate representation:** envelope-tagged SSA-style IR shared by every backend (C++, GLSL, VM, future LLVM). Normalisation happens once; backends never see syntactic sugar.
- **IR definition (draft):**
  - **Module:** `{ string_table, struct_layouts, functions, entry_index, version }`.
  - **Function:** `{ name, instructions }` where instructions are linear, stack-based ops with immediates.
  - **Instruction:** `{ op, imm }`; `op` is an `IrOpcode`, `imm` is a 64-bit immediate payload whose meaning depends on `op`.
  - **Locals:** addressed by index; `LoadLocal`, `StoreLocal`, `AddressOfLocal` operate on the index encoded in `imm`.
  - **Strings:** string literals are interned in `string_table` and referenced by index in print ops (see PSIR versioning).
  - **Entry:** `entry_index` points to the entry function in `functions`; its signature is enforced by the front-end.
- **PSIR versioning:** serialized IR includes a version tag; v2 introduces `AddressOfLocal`, `LoadIndirect`, and `StoreIndirect` for pointer/reference lowering; v4 adds `ReturnVoid` to model implicit void returns in the VM/native backends; v5 adds a string table + print opcodes for stdout/stderr output; v6 extends print opcodes with newline/stdout/stderr flags to support `print`/`print_line`/`print_error`/`print_line_error`; v7 adds `PushArgc` for entry argument counts in VM/native execution; v8 adds `PrintArgv` for printing entry argument strings; v9 adds `PrintArgvUnsafe` to emit unchecked entry-arg prints for `at_unsafe`; v10 adds `LoadStringByte` for string indexing in VM/native backends; v11 adds struct layout manifests; v12 adds struct field visibility/static metadata; v13 adds float arithmetic/compare/convert opcodes; v14 adds float return opcodes (`ReturnF32`, `ReturnF64`).
  - **PSIR v2:** adds pointer opcodes (`AddressOfLocal`, `LoadIndirect`, `StoreIndirect`) to support `location`/`dereference`.
  - **PSIR v4:** adds `ReturnVoid` so void definitions can omit explicit returns without losing a bytecode terminator.
- **Backends:**
  - **C++ emitter** – generates host code or LLVM IR for native binaries/JITs.
  - **GLSL emitter** – produces shader code; SPIR-V output is available via `--emit=spirv`, while Metal output remains future work.
  - **VM bytecode** – compact instruction set executed by the embedded interpreter/JIT.
- **Tooling:** CLI compiler `primec`, plus the VM runner `primevm` and build/test helpers. The compiler accepts `--entry /path` to select the entry definition (default: `/main`). Entry definitions currently accept either no parameters or a single `[array<string>]` parameter for command-line arguments; `args.count()` and `count(args)` are supported, `print*` calls accept `args[index]` (checked) or `at_unsafe(args, index)` (unchecked), and string bindings may be initialised from `args[index]` or `at_unsafe(args, index)` (print-only). The C++ emitter maps the array argument to `argv` and otherwise uses the same restriction. The definition/execution split maps cleanly to future node-based editors; full IDE/LSP integration is deferred until the compiler stabilises.
- **AST/IR dumps:** the debug printers include executions with their argument lists so tooling can capture scheduling intent in snapshots.
  - Dumps show collection literals after text-transform rewriting (e.g., `array<i32>{1i32,2i32}` becomes `array<i32>(1, 2)`).
  - Labeled execution arguments appear inline (e.g., `exec /execute_task([count] 2)`).

## Language Design Highlights
- **Identifiers:** `[A-Za-z_][A-Za-z0-9_]*` plus the slash-prefixed form `/segment/segment/...` for fully-qualified paths. Unicode may arrive later, but identifiers are constrained to ASCII for predictable tooling and hashing. `mut`, `return`, `include`, `import`, `namespace`, `true`, `false`, `if`, `else`, `loop`, `while`, and `for` are reserved keywords; any other identifier (including slash paths) can serve as a transform, path segment, parameter, or binding.
- **String literals:** surface forms accept `"..."utf8` / `"..."ascii` with escape processing, or raw `'...'utf8` / `'...'ascii` with no escape processing. The `implicit-utf8` text transform (enabled by default) appends `utf8` when omitted in surface syntax. **Canonical/bottom-level form uses double-quoted strings with escapes normalized and an explicit `utf8`/`ascii` suffix.** `ascii` enforces 7-bit ASCII (the compiler rejects non-ASCII bytes). Example surface: `"hello"utf8`, `'moo'ascii`. Example canonical: `"hello"utf8`. Raw example: `'C:\temp'ascii` keeps backslashes verbatim.
- **Numeric envelopes (draft):** fixed-width `i32`, `i64`, `u64`, `f32`, and `f64` map directly to hardware instructions and are the only numeric envelopes accepted by GPU backends. `integer` is an arbitrary-precision signed integer envelope with exact arithmetic (no overflow/wrapping). `decimal` is an arbitrary-precision floating envelope with fixed 256-bit precision and deterministic round-to-nearest-even semantics. `complex` is a pair of `decimal` values (`real`, `imag`) using the same 256-bit precision and rounding rules. `integer`/`decimal`/`complex` are software-only and rejected by GPU backends; the current VM/native subset excludes them. Mixed ops between `integer`/`decimal`/`complex` and fixed-width envelopes are rejected; use `convert<T>(value)` explicitly, and conversions that would overflow or lose information fail with diagnostics. When inference cannot select `integer`/`decimal`/`complex`, the existing `implicit-i32` rule still applies.
- **Aliases:** `int` and `float` currently map to `i32` and `f32` in the compiler. For deterministic cross-target behavior, prefer explicit widths (`i32`, `i64`, `f32`, `f64`); future backends may widen the aliases.
  - Float literals accept standard decimal forms, including optional fractional digits (e.g., `1.`, `1.0`, `1.f32`, `1.e2`).
- **Envelope:** the canonical AST uses a single envelope form for definitions and executions: `[transform-list] identifier<template-list>(parameter-or-arg-list) {body-list}`. Surface definitions require an explicit `{...}` body; surface executions are call-style (`identifier<template-list>(arg-list)`) and map to an envelope with an implicit empty body. Bindings use the form `[Envelope qualifiers…] name{initializer}` and may omit the envelope transform when the initializer lets the compiler infer it (defaulting to `int` if inference fails; `int` currently maps to `i32`). Lists recursively reuse whitespace-separated tokens.
  - Syntax markers: `[]` compile-time transforms, `<>` templates, `()` runtime parameters/calls, `{}` runtime code (definition bodies, binding initializers).
  - `[...]` enumerates metafunction transforms applied in order (see “Built-in transforms”).
  - `<...>` supplies compile-time envelopes/templates—primarily for transforms or when inference must be overridden.
  - `(...)` lists runtime parameters; calls always include `()` (even with no args), and `()` never appears on bindings.
  - **Parameters:** use the same binding envelope as locals: `main([array<string>] args, [i32] limit{10i32})`. Qualifiers like `mut`/`copy` apply here as well; defaults are optional and currently limited to literal/pure forms (no name references).
  - `{...}` holds runtime code for definition bodies and value blocks for binding initializers. Binding initializers evaluate the block and use its resulting value (last item or `return(value)`); use explicit constructor calls when passing multiple arguments (e.g., `[T] name{ T(arg1, arg2) }`).
  - Bindings are only valid inside definition bodies or parameter lists; top-level bindings are rejected.
- **Definitions vs executions:** definitions include a body (`{…}`) and optional transforms; executions are call-style (`execute_task<…>(args)`) with mandatory parentheses and no body, and map to an envelope with an implicit empty body. Calls always use `()`; the `name{...}` form is reserved for bindings so `execute_task{...}` is invalid.
  - Executions accept the same argument syntax as calls, including labeled arguments (`[param] value`).
  - Nested calls inside execution arguments still follow builtin rules (e.g., `array<i32>([first] 1i32)` is rejected).
  - Example: `execute_task([items] array<i32>(1i32, 2i32) [pairs] map<i32, i32>(1i32, 2i32))`.
  - Note: the current C++ emitter only generates code for definitions; executions are parsed/validated but not emitted.
- **Return annotation:** definitions declare return envelopes via transforms (e.g., `[return<float>] blend<…>(…) { … }`). Definitions return values explicitly (`return(value)`); the desugared form is always canonical.
  - **Surface vs canonical:** surface syntax may omit the return transform and rely on inference; canonical/bottom-level syntax always carries an explicit `return<T>`. Example surface: `main() { return(0) }` → canonical: `[return<int>] main() { return(0i32) }`.
  - **Optional convenience:** the `single_type_to_return` transform rewrites a single bare envelope in the transform list into `return<envelope>` (e.g., `[int] main()` → `[return<int>] main()`), but it only runs when explicitly enabled via `--semantic-transforms` / `--transform-list` or a per-definition transform.
- **Effects:** by default, definitions/executions start with `io_out` enabled so logging works without explicit annotations. Authors can override with `[effects(...)]` (e.g., `[effects(global_write, io_out)]`) or tighten to pure behavior by passing `primec --default-effects=none`. Standard library routines permit stdout/stderr logging via `io_out`/`io_err`; backends reject unsupported effects (e.g., GPU code requesting filesystem access). `primec --default-effects <list>` supplies the default effect set for any definition/execution that omits `[effects]` (comma-separated list; `default` and `none` are supported tokens). If `[capabilities(...)]` is present it must be a subset of the active effects (explicit or default).
- **Execution effects:** executions may also carry `[effects(...)]`. The execution’s effects must be a subset of the enclosing definition’s active effects; otherwise it is a diagnostic. The default set is controlled by `--default-effects` in the compiler/VM.
- **Capability taxonomy (v1):**
  - **IO:** `io_out` (stdout), `io_err` (stderr).
  - **Memory:** `heap_alloc` (dynamic allocation), `global_write` (mutating global state).
  - **Assets:** `asset_read`, `asset_write` (asset/database I/O).
  - **PathSpace:** `pathspace_notify`, `pathspace_insert`, `pathspace_take`.
  - **GPU/Render:** `render_graph` (GPU scheduling/pipeline access).
  - Unknown capability names are errors; capability identifiers are `lower_snake_case`.
- **Tooling vs runtime visibility:**
  - **Tooling surfaces:** declared effects/capabilities, resolved defaults, entry defaults, and backend allowlist violations (diagnostics).
  - **Runtime-only logs:** resolved effect/capability masks and execution identifiers (path hashes) for tracing; source spans are optional/debug-only.
- **Paths & includes:** every definition/execution lives at a canonical path (`/ui/widgets/log_button_press`). Authors can spell the path inline or rely on `namespace foo { ... }` blocks to prepend `/foo` automatically; includes simply splice text, so they inherit whatever path context is active. Include paths are parsed before text transforms, so they remain quoted without literal suffixes. `include<"/std/io", version="1.2.0">` searches the include path for a zipped archive or plain directory whose layout mirrors `/version/first_namespace/second_namespace/...`. The angle-bracket list may contain multiple quoted string paths—`include<"/std/io", "./local/io/helpers", version="1.2.0">`—and the resolver applies the same version selector to each path; mismatched archives raise an error before expansion. Versions live in the leading segment (e.g., `1.2/std/io/*.prime` or `1/std/io/*.prime`). If the version attribute provides one or two numbers (`1` or `1.2`), the newest matching archive is selected; three-part versions (`1.2.0`) require an exact match. Each `.prime` source file is inline-expanded exactly once and registered under its namespace/path (e.g., `/std/io`); duplicate includes are ignored. Folders prefixed with `_` remain private. `import /foo/*` is a lightweight alias that brings the immediate children of `/foo` into the root namespace, while `import /foo/bar` aliases a single definition (or builtin) by its final segment; imports do not inline source (use `include` for that), and unknown import paths are errors. `import /foo` is shorthand for `import /foo/*` (except `/math`, which is unsupported without `/*` or an explicit name). `import /math/*` brings all math builtins into the root namespace, or import a subset via `import /math/sin /math/pi`; `import /math` without a wildcard or explicit name is not supported. Imports are resolved after includes and can be listed as `import /math/*, /util/*` or whitespace-separated paths.
- **Transform-driven control flow:** control structures desugar into prefix calls that accept envelope arguments. A surface form like `if(condition) { … } else { … }` rewrites into `if(condition, then() { … }, else() { … })`. `loop(count) { … }`, `while(condition) { … }`, and `for(init cond step) { … }` rewrite into `loop(count, do() { … })`, `while(condition, do() { … })`, and `for(init, cond, step, do() { … })`. The envelope names (`do`, `then`, `else`) are for readability only; any name is accepted and ignored by the compiler. Infix operators (`a + b`) become canonical calls (`plus(a, b)`), ensuring IR/backends see a small, predictable surface.
- **Mutability:** bindings are immutable by default. Opt into mutation by placing `mut` inside the stack-value execution or helper (`[Integer mut] exposure{42}`, `[mut] Create()`). Transforms enforce that only mutable bindings can serve as `assign` or pointer-write targets.

### Transform phases (draft)
- **Two phases:** text transforms operate on raw tokens before AST construction; semantic transforms operate on the parsed AST node.
- **Explicit grouping:** use `text(...)` and `semantic(...)` inside the transform list to force phase placement.
  - Example:
    ```
    [text(operators, collections, implicit-utf8) semantic(return<i32>, effects(io_out))]
    main([i32] a, [i32] b) {
      return(a + b)
    }
    ```
- **Auto-deduce by name:** transforms listed without `text(...)` or `semantic(...)` are assigned to their declared phase automatically. If a name exists in both registries, the compiler emits an ambiguity error.
- **Ordering:** the compiler scans transforms left-to-right, appending each to its phase list while preserving relative order within the text and semantic phases.
- **Scope:** a text transform may rewrite any token inside the enclosing definition/execution envelope (transform list, templates, parameters, and body). Nested definitions/lambdas receive their own transform lists.
- **Self-expansion:** text transforms may append additional text transforms to the same node; appended transforms run after the current transform.
- **Applicability limits (v1):**
  - **Definitions/executions only:** `return<T>`, `effects(...)`, `capabilities(...)`, `text(...)`, `semantic(...)`, `single_type_to_return`.
  - **Struct/tag only (definitions):** `struct`, `pod`, `handle`, `gpu_lane`, `align_bytes(n)`, `align_kbytes(n)`.
  - **Bindings only:** access/visibility markers (`public`, `private`, `package`, `static`).
  - **Reserved/rejected in v1:** `stack`, `heap`, `buffer` placement transforms (diagnostic).
  - Any transform outside its allowed scope is a compile-time error with a diagnostic naming the enclosing path.

### Example function syntax
```
import /math/*
namespace demo {
  [return<void> effects(io_out)]
  hello_values() {
    [string] message{"Hello PrimeStruct"utf8}
    [i32] iterations{3i32}
    [float mut] exposure{1.25f32}
    [float3] tone_curve{float3(0.8f32, 0.9f32, 1.1f32)}

    print_line(message)
    assign(exposure, clamp(exposure, 0.0f32, 2.0f32))
    loop(iterations) {
      apply_curve(tone_curve, exposure)
    }
  }
}
```
Statements are separated by whitespace (including newlines). Commas and semicolons are ignored separators with no
meaning outside numeric literals (where commas may act as digit separators). PrimeStruct does not distinguish between
statements and envelopes—any envelope can stand alone as a statement, and unused values are discarded (for example,
`helper()` or `1i32` can appear as standalone statements).

### Slash paths & textual operator transforms
- Slash-prefixed identifiers (`/pkg/module/thing`) are valid anywhere the Envelope expects a name; `namespace foo { ... }` is shorthand for prepending `/foo` to enclosed names, and namespaces may be reopened freely.
- Text transforms run before the AST exists. Operator transforms (e.g., divide) scan the raw character stream: when a `/` is sandwiched between non-whitespace characters they rewrite the surrounding text (`/foo / /bar` → `divide(/foo, /bar)`), but when `/` begins a path segment (start of line or immediately after whitespace/delimiters) the transform leaves it untouched (`/foo/bar/lol()` stays intact). Other operators follow the same no-whitespace rule (`a>b` → `greater_than(a, b)`, `a<b` → `less_than(a, b)`, `a>=b` → `greater_equal(a, b)`, `a<=b` → `less_equal(a, b)`, `a==b` → `equal(a, b)`, `a!=b` → `not_equal(a, b)`, `a&&b` → `and(a, b)`, `a||b` → `or(a, b)`, `!a` → `not(a)`, `-a` → `negate(a)`, `a=b` → `assign(a, b)`, `++a` / `a++` → `increment(a)`, `--a` / `a--` → `decrement(a)`).
- Because includes expand first, slash paths survive every transform untouched until the AST builder consumes them, and IR lowering never needs to reason about infix syntax.

### Struct & envelope categories (draft)
- **Struct tag as transform:** `[struct ...]` in the envelope is purely declarative. It records a layout manifest (field names, envelopes, offsets) and validates the body, but the underlying syntax remains a standard definition. Struct-tagged definitions are field-only: no parameters or return transforms, and no return statements. Un-tagged definitions may still be instantiated as structs; they simply skip the extra validation/metadata until another transform demands it.
- **Placement policy (draft):** where a value lives (stack/heap/buffer) is decided by allocation helpers plus capabilities, not by struct tags. Envelopes may express requirements (e.g., `pod`, `handle`, `gpu_lane`), but placement is a call-site decision gated by capabilities.
- **POD tag as validation:** `[pod]` asserts trivially-copyable semantics. Violations (hidden lifetimes, handles, async captures) raise diagnostics; without the tag the compiler treats the body permissively.
- **Member syntax:** every field is just a stack-value execution (`[float mut] exposure{1.0f32}`, `[handle<PathNode>] target{get_default()}`). Attributes (`[mut]`, `[align_bytes(16)]`, `[handle<PathNode>]`) decorate the execution, and transforms record the metadata for layout consumers.
- **Method calls & indexing:** `value.method(args...)` desugars to `/<envelope>/method(value, args...)` in the method namespace (no hidden object model), where `<envelope>` is the envelope name associated with `value`. For arrays, `value.count()` rewrites to `/array/count<T>(value)`; the helper `count(value)` simply forwards to `value.count()`. Indexing uses the safe helper by default: `value[index]` rewrites to `at(value, index)` with bounds checks; `at_unsafe(value, index)` skips checks.
- **Baseline layout rule:** members default to source-order packing. Backend-imposed padding is allowed only when the metadata (`layout.fields[].padding_kind`) records the reason; `[no_padding]` and `[platform_independent_padding]` fail the build if the backend cannot honor them bit-for-bit.
- **Alignment transforms:** `[align_bytes(n)]` (or `[align_kbytes(n)]`) may appear on the struct or field; violations again produce diagnostics instead of silent adjustments.
- **Stack value executions:** every local binding—including struct “fields”—materializes via `[Type qualifiers…] name{initializer}` so stack frames remain declarative (e.g., `[float mut] exposure{1.0f32}`). Default initializers are mandatory to keep frames fully initialized.
  - Multi-step initializer example:
    ```
    [return<float>]
    main() {
    [float] x{
      [float mut] tmp{1.0f32}
      tmp = tmp + 2.0f32
      tmp
    }
      return(x)
    }
    ```
- **Lifecycle helpers (Create/Destroy):** Within a struct-tagged or field-only struct definition, nested definitions named `Create` and `Destroy` gain constructor/destructor semantics. Placement-specific variants add suffixes (`CreateStack`, `DestroyHeap`, etc.). Without these helpers the field initializer list defines the default constructor/destructor semantics. `this` is implicitly available inside helpers. Add `mut` to the helper’s transform list when it writes to `this` (otherwise `this` stays immutable); omit it for pure helpers. Lifecycle helpers must return `void` and accept no parameters. We capitalise system-provided helper names so they stand out, but authors are free to use uppercase identifiers elsewhere—only the documented helper names receive special treatment.
  ```
  import /math/*
  namespace demo {
    [struct pod]
    color_grade() {
      [float mut] exposure{1.0f32}

      [mut]
      Create() {
        assign(this.exposure, clamp(this.exposure, 0.0f32, 2.0f32))
      }

      [effects(io_out)]
      Destroy() {
        print_line("color grade destroyed"utf8)
      }
    }
  }
  ```
- **IR layout manifest:** `[struct]` extends the IR descriptor with `layout.total_size_bytes`, `layout.alignment_bytes`, and ordered `layout.fields`. Each field record stores `{ name, envelope, offset_bytes, size_bytes, padding_kind, category }`. Placement transforms consume this manifest verbatim, ensuring C++, VM, and GPU backends share one source of truth.
- **Categories:** `[pod]`, `[handle]`, `[gpu_lane]` tags classify members for borrow/resource rules. Handles remain opaque tokens with subsystem-managed lifetimes; GPU lanes require staging transforms before CPU inspection.
- **Category mapping (draft):**
  - `pod`: stored inline; treated as trivially-copyable for layout and borrow checks.
  - `handle`: stored as an opaque reference token; lifetime is managed by the owning subsystem.
  - `gpu_lane`: stored as a GPU-only handle; CPU access requires explicit staging transforms.
  - Conflicts are rejected (`pod` with `handle` or `gpu_lane`, and `handle` with `gpu_lane`).

### Transforms (draft)
- **Purpose:** transforms are metafunctions that rewrite tokens (text transforms) or stamp semantic flags on the AST (semantic transforms). Later passes (borrow checker, backend filters) consume the semantic flags; transforms do not emit code directly.
- **Evaluation mode:** when the compiler sees `[transform ...]`, it routes through the metafunction's declared signature—pure token rewrites operate on the raw stream, while semantic transforms receive the AST node and in-place metadata writers.

**Text transforms (token-level)**
- **`append_operators`:** injects `operators` into the leading transform list when missing, enabling text-transform self-expansion without repeating `operators` everywhere.
- **`operators`:** desugars infix/prefix operators, comparisons, boolean ops, assignment, and increment/decrement (`++`/`--`) into canonical calls (`plus`, `less_than`, `assign`, `increment`, etc.).
- **`collections`:** rewrites `array<T>{...}` / `vector<T>{...}` / `map<K,V>{...}` literals into constructor calls.
- **`implicit-utf8`:** appends `utf8` to bare string literals.
- **`implicit-i32`:** appends `i32` to bare integer literals (enabled by default).
  - Text transform arguments are limited to identifiers and literals (no nested envelopes or calls).

**Semantic transforms (AST-level)**
- **`copy`:** force copy-on-entry for a parameter or binding, even when references are the default. Often paired with `mut`.
- **`mut`:** mark the local binding as writable; without it the binding behaves like a `const` reference. On definitions, `mut` is only valid on lifecycle helpers to make `this` mutable; executions do not accept `mut`.
- **`restrict<T>`:** constrain the accepted envelope to `T` (or satisfy concept-like predicates once defined). Applied alongside `copy`/`mut` when needed.
- **`return<T>`:** optional contract that pins the inferred return envelope. Recommended for public APIs or when disambiguation is required.
- **`single_type_to_return`:** optional transform that rewrites a single bare envelope in a transform list into `return<envelope>` (e.g., `[int] main()` → `[return<int>] main()`); disabled by default and only runs when enabled in the transform list or CLI (`--semantic-transforms` / `--transform-list`).
- **`effects(...)`:** declare side-effect capabilities; absence implies purity. Backends reject unsupported capabilities.
- **Transform scope:** `effects(...)` and `capabilities(...)` are only valid on definitions/executions, not bindings.
- **`align_bytes(n)`, `align_kbytes(n)`:** encode alignment requirements for struct members and buffers. `align_kbytes` applies `n * 1024` bytes before emitting the metadata.
- **`capabilities(...)`:** reuse the transform plumbing to describe opt-in privileges without encoding backend-specific scheduling hints.
- **`struct`, `pod`, `handle`, `gpu_lane`:** declarative tags that emit metadata/validation only. They never change syntax; instead they fail compilation when the body violates the advertised contract (e.g., `[pod]` forbids handles/async fields).
- **`public`, `private`, `package`:** field visibility tags; mutually exclusive.
- **`static`:** field storage tag; hoists storage to namespace scope while keeping the field in the layout manifest.
- **`stack`, `heap`, `buffer`:** placement transforms reserved for future backends; currently rejected in validation.
- **`shared_scope`:** loop-only transform that makes a loop body share one scope across all iterations. Valid on `loop`/`while`/`for` only. Bindings declared in the loop body are initialized once before the loop body runs and persist for the duration of the loop without escaping the surrounding scope.
The list above reflects the built-in transforms recognized by the compiler today; future additions will extend it here.

### Core library surface (draft)
- **Standard math (draft):** the core math set lives under `/math/*` (e.g., `/math/sin`, `/math/pi`). `import /math/*` brings these names into the root namespace so `sin(...)`/`pi` resolve without qualification. Unsupported envelope/operation pairs produce diagnostics. Fixed-width integers (`i32`, `i64`, `u64`) and `integer` use exact arithmetic; `f32`/`f64` follow their IEEE-754 behavior; `decimal` and `complex` use the 256-bit `decimal` precision and round-to-nearest-even rules.
  - **Constants:** `/math/pi`, `/math/tau`, `/math/e`.
  - **Basic:** `/math/abs`, `/math/sign`, `/math/min`, `/math/max`, `/math/clamp`, `/math/lerp`, `/math/saturate`.
  - **Rounding:** `/math/floor`, `/math/ceil`, `/math/round`, `/math/trunc`, `/math/fract`.
  - **Power/log:** `/math/sqrt`, `/math/cbrt`, `/math/pow`, `/math/exp`, `/math/exp2`, `/math/log`, `/math/log2`, `/math/log10`.
  - **Integer pow:** for integer operands, `pow` requires a non-negative exponent; negative exponents abort in VM/native (stderr + exit code `3`), and the C++ emitter mirrors this behavior.
  - **Trig:** `/math/sin`, `/math/cos`, `/math/tan`, `/math/asin`, `/math/acos`, `/math/atan`, `/math/atan2`, `/math/radians`, `/math/degrees`.
  - **Hyperbolic:** `/math/sinh`, `/math/cosh`, `/math/tanh`, `/math/asinh`, `/math/acosh`, `/math/atanh`.
  - **Float utils:** `/math/fma`, `/math/hypot`, `/math/copysign`.
  - **Predicates:** `/math/is_nan`, `/math/is_inf`, `/math/is_finite`.
- **`assign(target, value)`:** canonical mutation primitive; only valid when `target` carried `mut` at declaration time. The call evaluates to `value`, so it can be nested or returned.
- **`increment(target)` / `decrement(target)`:** canonical mutation helpers used by `++`/`--` desugaring. Only valid on mutable numeric bindings; they evaluate to the updated value.
- **`count(value)` / `value.count()` / `at(value, index)` / `at_unsafe(value, index)`:** collection helpers. `value.count()` is canonical; `count(value)` forwards to the method when available. `at` performs bounds checking; `at_unsafe` does not. In the VM/native backends, an out-of-bounds `at` aborts execution (prints a diagnostic to stderr and returns exit code `3`).
- **`print(value)` / `print_line(value)` / `print_error(value)` / `print_line_error(value)`:** stdout/stderr output primitives (statement-only). `print`/`print_line` require `io_out`, and `print_error`/`print_line_error` require `io_err`. VM/native backends support integer/bool values plus string literals/bindings; other string operations still require the C++ emitter.
- **`plus`, `minus`, `multiply`, `divide`, `negate`:** arithmetic wrappers used after operator desugaring. Operands must be numeric (`i32`, `i64`, `u64`, `f32`, `f64`); bool/string/pointer operands are rejected. Mixed signed/unsigned integer operands are rejected in VM/native lowering (`u64` only combines with `u64`), and `negate` rejects unsigned operands. Pointer arithmetic is only defined for `plus`/`minus` with a pointer on the left and an integer offset (see Pointer arithmetic below).
- **`greater_than(left, right)`, `less_than(left, right)`, `greater_equal(left, right)`, `less_equal(left, right)`, `equal(left, right)`, `not_equal(left, right)`, `and(left, right)`, `or(left, right)`, `not(value)`:** comparison wrappers used after operator/control-flow desugaring. Comparisons respect operand signedness (`u64` uses unsigned ordering; `i32`/`i64` use signed ordering), and mixed signed/unsigned comparisons are rejected in the current IR/native subset; `bool` participates as a signed `0/1`, so `bool` with `u64` is rejected as mixed signedness. Boolean combinators accept `bool` inputs only. Control-flow conditions (`if`/`while`/`for`) require `bool` results; use comparisons or `bool{value}` / `convert<bool>(value)` when needed. The current IR/native subset accepts integer/bool/float operands for comparisons; string comparisons still require the C++ emitter.
- **`/math/clamp(value, min, max)`:** numeric helper used heavily in rendering scripts. VM/native lowering supports integer clamps (`i32`, `i64`, `u64`) and float clamps (`f32`, `f64`) and follows the usual integer promotion rules (`i32` mixed with `i64` yields `i64`, while `u64` requires all operands to be `u64`). Mixed signed/unsigned clamps are rejected.
- **`if(condition, then() { ... }, else() { ... })`:** canonical conditional form after control-flow desugaring.
  - Signature: `if(Envelope, Envelope, Envelope)`
  - 1) must evaluate to a boolean (`bool`), either a boolean value or a function returning boolean
  - 2) must be a definition envelope; its body yields the `if` result when the condition is `true`
  - 3) must be a definition envelope; its body yields the `if` result when the condition is `false`
  - Evaluation is lazy: the condition is evaluated first, then exactly one of the two definition bodies is evaluated.
- **`loop(count) { ... }`:** statement-only loop helper. `count` must be an integer envelope (`i32`, `i64`, `u64`, or `int` alias), and the body is required. Negative counts are errors.
- **`while(condition) { ... }`:** statement-only loop helper. `condition` must evaluate to `bool` (or a function returning `bool`).
- **`for(init cond step) { ... }`:** statement-only loop helper. `init`, `cond`, and `step` are evaluated in order; `cond` must evaluate to `bool`. Bindings are allowed in any slot. Semicolons and commas are optional separators (e.g., `for(init; cond; step)`).
- **Loop scope:** loop bodies default to per-iteration scope. Add `[shared_scope]` before the loop to share one scope across all iterations.
  - Example:
    ```
    [shared_scope]
    loop(3i32) {
      [i32 mut] total{0i32}
      assign(total, plus(total, 1i32))
    }
    ```

### Loop/For/While Examples (surface → canonical)
Surface forms:
```
loop(5i32) { work() }

while(i < 10i32) { work(i) }

for([int mut] i{0i32}; i < 10; ++i) {
  work(i)
}
```

Canonical (after desugaring):
```
loop(5i32, do() { work() })

while(less_than(i, 10i32), do() { work(i) })

for(
  [int mut] i{0i32},
  less_than(i, 10i32),
  increment(i),
  do() { work(i) }
)
```
- **`notify(path, payload)`, `insert`, `take`:** PathSpace integration hooks for signaling and data movement.
- **`return(value)`:** explicit return primitive; may appear as a statement inside control-flow blocks. For `void` definitions, `return()` is allowed. Implicit `return(void)` fires at end-of-body when omitted. Non-void definitions must return on all control paths; fallthrough is a compile-time error. Inside value blocks (binding initializers / brace constructors), `return(value)` returns from the block and yields its value.
- **IR note:** VM/native IR lowering supports numeric/bool `array<T>(...)` and `vector<T>(...)` calls plus `array<T>{...}` and `vector<T>{...}` literals, along with `count`/`at`/`at_unsafe` on those sequences. Map literals are supported in VM/native for numeric/bool values, and string-keyed maps work when the keys are string literals or bindings backed by literals (string table entries). VM/native vectors use fixed capacity; `push`/`reserve` succeed while capacity allows and error once exceeded, and shrinking helpers (`pop`, `clear`, `remove_at`, `remove_swap`) continue to work against the fixed capacity.

### Standard Library Reference (draft, v0)
- **Status:** this reference is a snapshot of the current builtin surface. It will be versioned once the standard library is split into packages.
- **Versioning (planned):**
  - Each package declares a semantic version (e.g., `1.2.0`).
  - `include<..., version="1.2.0">` selects a specific package revision.
  - The compiler will expose `--stdlib-version` to pin the default package set.
- **Namespaces:**
  - `/math/*` is imported via `import /math/*` (or explicit names like `import /math/sin /math/pi`).
  - Core builtins (`assign`, `count`, `print*`, `convert`, etc.) live in the root namespace.
- **Conformance note:** the VM/native subset may reject functions that the C++ emitter supports (e.g., float-heavy math); the reference will mark such cases explicitly.
- **Core builtins (root namespace):**
  - **`assign(target, value)`** (statement): mutates a mutable binding or dereferenced pointer.
  - **`increment(target)` / `decrement(target)`** (statement): mutation helpers used by `++`/`--` desugaring.
- **`convert<T>(value)`**: explicit conversion for `int/i32/i64/u64/bool/f32/f64` in VM/native.
  - **`if(cond, then() { ... }, else() { ... })`**: canonical conditional form after desugaring.
  - **`loop(count, do() { ... })`** (statement): loop helper with integer count.
  - **`while(cond, do() { ... })`** (statement): loop helper with bool condition.
  - **`for(init, cond, step, do() { ... })`** (statement): loop helper with init/cond/step.
  - **`return(value)`**: explicit return helper.
  - **`count(value)` / `value.count()`**: collection length.
  - **`at(value, index)` / `at_unsafe(value, index)`**: bounds-checked and unchecked indexing.
  - **`print*`**: `print`, `print_line`, `print_error`, `print_line_error`.
  - **Collections:** `array<T>(...)`, `vector<T>(...)`, `map<K, V>(...)`.
  - **Pointer helpers:** `location`, `dereference`.
  - **PathSpace helpers:** `notify`, `insert`, `take`.
  - **Operators (desugared forms):** `plus`, `minus`, `multiply`, `divide`, `negate`, `increment`, `decrement`.
  - **Comparisons/booleans:** `greater_than`, `less_than`, `greater_equal`, `less_equal`, `equal`, `not_equal`, `and`, `or`, `not`.

## Runtime Stack Model (draft)
- **Frames:** each execution pushes a frame recording the instruction pointer, constants, locals, captures, and effect mask. Frames are immutable from the caller’s perspective; `assign` creates new bindings.
- **Deterministic evaluation:** arguments evaluate left-to-right; boolean `and`/`or` short-circuit; `return(value)` unwinds the current definition. In value blocks, `return(value)` exits the block and yields its value. Implicit `return(void)` fires if a definition body reaches the end.
- **Transform boundaries:** rewrites annotate frame entry/exit so the VM, C++, and GLSL backends share a consistent calling convention.
- **Resource handles:** PathSpace references/handles live inside frames as opaque values; lifetimes follow lexical scope.
- **Tail execution (planned):** future optimisation collapses tail executions to reuse frames (VM optional, GPU required).
- **Effect annotations:** purity by default; explicit `[effects(...)]` opt-ins. Standard library defaults to stdout-only effects (stderr requires `io_err`).

### Execution Metadata (draft)
- **Scheduling scope:** queue/thread selection stays host-driven; there are no stack- or runner-specific annotations yet, so executions inherit the embedding runtime’s default placement.
- **Capabilities:** effect masks double as capability descriptors (IO, global write, GPU access, etc.). Additional attributes can narrow capabilities (`[capabilities(io_out, pathspace_insert)]`).
- **Instrumentation:** executions carry metadata (source file/line plus effect/capability masks) for diagnostics and tracing.
- **Open design items:** finalise the capability taxonomy and determine which instrumentation fields flow into inspector tooling vs. runtime-only logs.

## Type & Class Semantics (draft)
- **Structural classes:** `[return<void>] class<Name>(members{…})` desugars into namespace `Name::` plus constructors/metadata. Instances are produced via constructor executions.
- **Composition over inheritance:** “extends” rewrites replicate members and install delegation logic; no hidden virtual dispatch unless a transform adds it.
- **Generics:** classes accept template parameters (`class<Vector<T>>(…)`) and specialise through the transform pipeline.
- **Nested generics:** template arguments may themselves be generic envelopes (`map<i32, array<i32>>`), and the parser preserves the nested envelope string for later lowering.
- **Interop:** generated code treats classes as structs plus free functions (`Name::method(instance, …)`); VM closures follow the same convention.
- **Field visibility:** stack-value declarations accept `[public]`, `[package]`, or `[private]` transforms (default: private); they are mutually exclusive. The compiler records `visibility` metadata per field so tooling and backends enforce access rules consistently. `[package]` exposes the field to any module compiled into the same package; `[public]` emits accessors in the generated namespace surface.
- **Static members:** add `[static]` to hoist storage to namespace scope while reusing the field’s visibility transform. Static fields still participate in the struct manifest so documentation and reflection stay aligned, but only one storage slot exists per struct definition.
- **Example:**
  ```
  import /math/*
  namespace demo {
    [struct]
    brush_settings() {
      [public float] size{12.0f32}
      [private float] jitter{0.1f32}
      [package static handle<Texture>] palette{load_default_palette()}

      [public]
      Create() {
        assign(this.size, clamp(this.size, 1.0f32, 64.0f32))
      }
    }
  }
  ```
- **Open design items:** decide constructor semantics (beyond the `Create`/`Destroy` helpers) and constant member behaviour once the package/effect designs settle.

## Lambdas & Higher-Order Functions (draft)
- **Syntax mirrors definitions:** lambdas omit the identifier (`[capture] <T>(params){ body }`). Captures rewrite into explicit parameters/structs.
- **Capture semantics:** support `[]`, `[=]`, `[&]`, and explicit notations (`[value x, ref y]`). Captures compile to generated structs with `invoke` methods.
- **First-class values:** closures are storable, passable, and returnable. Backends emit them as struct + function pointer (C++), inline function objects (GLSL, where legal), or closure objects (VM).
- **Inlining transforms:** standard transforms may inline pure lambdas; async/task-oriented lambdas stay as closures.
- **PathSpace interop:** captured handles respect frame lifetimes; transforms force reference-count or move semantics as required.

## Literals & Composite Construction (draft)
- **Numeric literals:** decimal, float, hexadecimal with optional width suffixes (`42i64`, `42u64`, `1.0f64`).
- Integer literals require explicit width suffixes (`42i32`, `42i64`, `42u64`) unless `implicit-i32` is enabled (on by default). Omit it from `--text-transforms` (or use `--no-text-transforms`) to require explicit suffixes.
- Float literals accept `f32` or `f64` suffixes; when omitted, they default to `f32`. Exponent notation (`1e-3`, `1.0e6f32`) is supported.
- Commas may appear between digits in the integer part as digit separators and are ignored for value (e.g., `1,000i32`, `12,345.0f32`). Commas are not allowed in the fractional or exponent parts, and `.` is the only decimal separator.
- **Strings:** double-quoted strings process escapes unless a raw suffix is used; single-quoted strings are raw and do not process escapes. `raw_utf8` / `raw_ascii` force raw mode on either quote style. Surface literals accept `utf8`/`ascii`/`raw_utf8`/`raw_ascii` suffixes; the canonical/bottom-level form uses double-quoted strings with normalized escapes and an explicit `utf8`/`ascii` suffix. `implicit-utf8` (enabled by default) appends `utf8` when omitted.
- **Boolean:** keywords `true`, `false` map to backend equivalents.
- **Composite constructors:** structured values are introduced through standard envelope executions (`ColorGrade([hue_shift] 0.1f32 [exposure] 0.95f32)`) or brace constructor forms (`ColorGrade{ ... }`) in value positions; brace constructors evaluate the block and pass its resulting value to the constructor. If the block executes `return(value)`, that value is used; otherwise the last item is used. Binding initializers are value blocks; to pass multiple constructor arguments use an explicit call (e.g., `[ColorGrade] grade{ ColorGrade([hue_shift] 0.1f32 [exposure] 0.95f32) }`). Labeled arguments map to fields, and every field must have either an explicit argument or an envelope-provided default before validation. Labeled arguments may only be used on user-defined calls.
- **Labeled arguments:** labeled arguments use a bracket prefix (`[name] value`) and may be reordered (including on executions). Positional arguments fill the remaining parameters in declaration order, skipping labeled entries. Builtin calls (operators, comparisons, clamp, convert, pointer helpers, collections) do not accept labeled arguments.
  - Example: `sum3(1i32 [c] 3i32 [b] 2i32)` is valid.
  - Example: `array<i32>([first] 1i32)` is rejected because collections are builtin calls.
  - Duplicate labeled arguments are rejected for definitions and executions (`execute_task([a] 1i32 [a] 2i32)`).
- **Collections:** `array<Type>{ … }` / `array<Type>[ … ]`, `vector<Type>{ … }` / `vector<Type>[ … ]`, `map<Key,Value>{ … }` / `map<Key,Value>[ … ]` rewrite to standard builder functions. The brace/bracket forms desugar to `array<Type>(...)`, `vector<Type>(...)`, and `map<Key,Value>(key1, value1, key2, value2, ...)`. Map literals supply alternating key/value forms.
  - Requires the `collections` text transform (enabled by default in `--text-transforms`).
  - Map literal entries are read left-to-right as alternating key/value forms; an odd number of entries is a diagnostic.
  - String keys are allowed in map literals (e.g., `map<string, i32>{"a"utf8 1i32}`), and nested forms inside braces are rewritten as usual.
  - Collections can appear anywhere forms are allowed, including execution arguments.
  - Numeric/bool array literals (`array<i32>{...}`, `array<i64>{...}`, `array<u64>{...}`, `array<bool>{...}`) lower through IR/VM/native.
  - `vector<T>` is a resizable, owning sequence. `vector<T>{...}` and `vector<T>(...)` are variadic constructors (0..N). Vector operations that grow capacity require `effects(heap_alloc)` (or the active default effects set); literals with one or more elements are treated as growth operations. VM/native implement vectors with fixed capacity; `push`/`reserve` are allowed only up to the existing capacity (exceeding it is a runtime error), and `reserve` does not reallocate.
  - Vector helpers: `count(value)` / `value.count()`, `at(value, index)`, `at_unsafe(value, index)`, `push(value, item)`, `pop(value)`, `reserve(value, capacity)`, `capacity(value)`, `clear(value)`, `remove_at(value, index)`, `remove_swap(value, index)` (method-call forms like `value.push(...)` are accepted for helpers).
  - Mutation helpers (`push`, `pop`, `reserve`, `clear`, `remove_at`, `remove_swap`) are statement-only.
  - Numeric/bool map literals (`map<i32, i32>{...}`, `map<u64, bool>{...}`) also lower through IR/VM/native (construction, `count`, `at`, and `at_unsafe`).
  - String-keyed map literals lower through VM/native when keys are string literals or bindings backed by literals; other string key expressions require the C++ emitter (which uses `std::string_view` keys).
- **Conversions:** no implicit coercions. Use explicit executions (`convert<float>(value)` or `bool{value}`) or custom transforms. The builtin `convert<T>(value)` is the default cast helper and supports `int/i32/i64/u64/bool/f32/f64` in the minimal native subset (integer width conversions currently lower as no-ops in the VM/native backends, while the C++ emitter uses `static_cast`; `convert<bool>` compares against zero, so any non-zero value—including negative integers—yields `true`). Float ↔ integer conversions lower to dedicated PSIR opcodes in VM/native.
- **Float note:** VM/native lowering supports float literals, bindings, arithmetic, comparisons, numeric conversions, and `/math/*` helpers.
- **String note:** VM/native lowering supports string literals and string bindings in `print*`, plus `count`/indexing (`at`/`at_unsafe`) on string literals and string bindings that originate from literals; other string operations still require the C++ emitter for now.
  - `convert<bool>` and `bool{...}` are valid for integer operands (including `u64`) and treat any non-zero value as `true`.
- **Mutability:** values immutable by default; include `mut` in the stack-value execution to opt-in (`[float mut] value{...}`).
- **Open design:** finalise literal suffix catalogue, raw string semantics across backends, and the composite-constructor defaults/validation rules.

## Pointers & References (draft)
- **Explicit envelopes:** `Pointer<T>`, `Reference<T>` mirror C++ semantics; no implicit conversions.
- **Surface syntax:** canonical syntax uses explicit calls (`location`, `dereference`, `plus`/`minus`); the `operators` text transform rewrites `&name`/`*name` sugar into those calls.
- **Reference binding:** `Reference<T>` bindings are initialized from `location(...)` and behave like `*Pointer<T>` in use. Use `mut` on the reference binding to allow `assign(ref, value)`.
- **Core pointer calls:** `location(value)` yields a pointer to a local binding (location only accepts a local binding name); `location(ref)` returns the pointer stored by a `Reference<T>` binding; `dereference(ptr)` reads through a pointer/reference form; `assign(dereference(ptr), value)` writes through the pointer. Pointer writes require the pointer binding to be declared `mut`; attempting to assign through an immutable pointer or reference is rejected.
- **Pointer arithmetic:** `plus(ptr, offset)` and `minus(ptr, offset)` treat `offset` as a byte offset. VM/native frames currently space locals in 16-byte slots, so adding or subtracting `16` advances one local slot. Offsets accept `i32`, `i64`, or `u64` in the front-end; non-integer offsets are rejected, and the native backend lowers all three widths.
  - Pointer + pointer is rejected; only pointer ± integer offsets are allowed.
  - Offsets are interpreted as unsigned byte counts at runtime; negative offsets require signed operands (e.g., `-16i64`).
  - Example: `plus(location(second), -16i64)` steps back one 16-byte slot.
  - Example: `minus(location(second), 16u64)` also steps back one 16-byte slot.
  - You can use `location(ref)` inside pointer arithmetic when starting from a `Reference<T>` binding.
  - The C++ emitter performs raw pointer arithmetic on actual addresses; offsets are only well-defined within the same allocated object.
  - Minimal runnable example:
    ```
    [return<int>]
    main() {
      [i32 mut] value{2i32}
      [Pointer<i32> mut] ptr{location(value)}
      assign(dereference(ptr), 4i32)
      return(value)
    }
    ```
  - Expected IR (shape only):
    ```
    PushI32 2
    StoreLocal value
    AddressOfLocal value
    StoreLocal ptr
    LoadLocal ptr
    PushI32 4
    StoreIndirect
    Pop
    LoadLocal value
    ReturnI32
    ```
- **Reference example:** `Reference<T>` behaves like a dereferenced pointer in value positions while still storing the address.
  - Minimal runnable example:
    ```
    [return<int>]
    main() {
      [i32 mut] value{2i32}
      [Reference<i32> mut] ref{location(value)}
      assign(ref, 4i32)
      return(ref)
    }
    ```
  - You can recover a raw pointer from a reference via `location(ref)` when needed for pointer arithmetic or passing into APIs.
  - Expected IR (shape only):
    ```
    PushI32 2
    StoreLocal value
    AddressOfLocal value
    StoreLocal ref
    LoadLocal ref
    PushI32 4
    StoreIndirect
    Pop
    LoadLocal ref
    LoadIndirect
    ReturnI32
    ```
  - References can be used directly in arithmetic (e.g., `plus(ref, 2i32)`), and `location(ref)` yields the underlying pointer.
- **Ownership:** references are non-owning, frame-bound views. Pointers can be tagged `raw`, `unique`, `shared` via transform-generated wrappers around PathSpace-aware allocators.
- **Raw memory:** `memory::load/store` primitives expose byte-level access; opt-in to highlight unsafe operations.
- **Layout control:** attributes like `[packed]` guarantee interop-friendly layouts for C++/GLSL.
- **Open design:** pointer qualifier syntax, aliasing rules (restrict/readonly), and GPU backend constraints remain TBD.

## VM Design (draft)
- **Instruction set:** ~50 stack-based ops covering control flow, stack manipulation, memory/pointer access, optional coroutine primitives. No implicit conversions; opcodes mirror the canonical language surface.
- **PSIR versioning:** current portable IR is PSIR v14 (adds float return opcodes on top of v13’s float arithmetic/compare/convert opcodes and v12’s struct field visibility/static metadata, `LoadStringByte`, `PrintArgvUnsafe`, `PrintArgv`, `PushArgc`, pointer helpers, `ReturnVoid`, and print opcode upgrades).
- **Frames & stack:** per-call frame with IP, constants, locals, capture refs, effect mask; tail calls reuse frames. Data stack stores tagged `Value` union (primitives, structs, closures, buffers).
- **Bytecode chunks:** compiler emits a chunk (bytecode + const pool) per definition. Executions reference chunks by index; constant pools hold literals, handles, metadata.
- **Native interop:** `CALL_NATIVE` bridges to host/PathSpace helpers via a function table. Effect masks gate what natives can do.
- **Closures:** compile to closure structs (chunk pointer + capture data). Captured handles obey lifetime/ownership rules.
- **Optimisation:** reference counting for heap values; optional chunk caching; future LLVM-backed JIT can feed on the same bytecode when needed.
- **Deployment target:** the VM serves as the sandboxed runtime for user-supplied scripts (e.g., on iOS) where native code generation is unavailable. Effect masks and capabilities enforce per-platform restrictions.

## Examples (sketch)
```
// Hello world (native/VM-friendly)
[return<int> effects(io_out)]
main() {
  print_line("Hello, world!"utf8)
  return(0i32)
}

// Pull std::io at version 1.2.0
include<"/std/io", version="1.2.0">

import /math/*

[operators return<int>] add<int>(a, b) { return(plus(a, b)) }

[operators] execute_add<int>(x, y)

clamp_exposure(img) {
  if(
    greater_than(img.exposure, 1.0f32),
    then() { notify("/warn/exposure"utf8, img.exposure) },
    else() { }
  )
}

tweak_color([copy mut restrict<Image>] img) {
  assign(img.exposure, clamp(img.exposure, 0.0f32, 1.0f32))
  assign(img.gamma, plus(img.gamma, 0.1f32))
  apply_grade(img)
}

[return<int>]
convert_demo() {
  return(convert<int>(1.5f32))
}

[return<int>]
labeled_args_demo() {
  return(sum3(1i32 [c] 3i32 [b] 2i32))
}

[operators return<float>]
blend([float] a, [float] b) {
  [float mut] result{(a + b) * 0.5f32}
  if (result > 1.0f32) {
    result = 1.0f32
  } else {
  }
  return(result)
}

// Canonical, post-transform form
[return<float>] blend([float] a, [float] b) {
  [float mut] result{multiply(plus(a, b), 0.5f32)}
  if(
    greater_than(result, 1.0f32),
    then() { assign(result, 1.0f32) },
    else() { }
  )
  return(result)
}

// IR sketch
module {
  def /convert_demo(): i32 {
    return convert(1.5f32)
  }
  def /labeled_args_demo(): i32 {
    return sum3(1, [c] 3, [b] 2)
  }
}
```

## Integration Points
- **Build system:** extend CMake/tooling to run `primec`, track dependency graphs, and support incremental builds.
- **PathSpace Scene Graph:** helper APIs map PrimeStruct modules to scenes/renderers and manage path-based resource bindings.
- **Testing:** unit/looped regression suites verify backend parity (C++, VM, GLSL).
- **Diagnostics:** metrics/logs land under `diagnostics/PrimeStruct/*`. Effect annotations drive error messaging.
- **PathSpace runtime wiring:** generated code uses PathSpace helper APIs (`insert`, `take`, `notify`). Transforms wrap high-level IO primitives so emitted C++/VM code interacts through envelope-tagged handles; GLSL outputs map onto renderer inputs. Lifetimes/ownership align with PathSpace nodes.

## Dependencies & Related Work
- Stable IR definition & serialization (std::serialization once available).
- Scene graph/rendering plans (`docs/finished/Plan_SceneGraph_Renderer_Finished.md`).
- PathIO/device plans for IO abstractions (`docs/AI_Paths.md`).

## Risks & Research Tasks
- **Memory model** – value/reference semantics, handle lifetimes, POD vs GPU staging rules.
- **Resource bindings** – unify descriptors across CPU/GPU backends.
- **Debugging** – source maps, stack inspection across VM/GPU.
- **Performance** – ensure parity with handwritten code; benchmark harnesses.
- **Adoption** – migration strategy from existing shaders/scripts.
- **Diagnostics** – map compile/runtime errors back to PrimeStruct source across backends.
- **Concurrency** – finalise coroutine/await integrations with PathSpace scheduling.
- **Security** – sandbox policy for transforms/archives.
- **Tooling** – IDE/LSP roadmap, incremental compilation caches, metadata outputs.

## Validation Strategy
- Golden tests comparing outputs from C++, VM, GLSL for shared modules.
- Performance benchmarks recorded under `benchmarks/`.
- Static analysis/lint integrated into CI to catch undefined constructs before codegen.

## Next Steps (Exploratory)
1. Draft detailed syntax/semantics spec and circulate for review. _(Draft captured in a separate syntax spec on 2025-11-21; review/feedback tracking TBD.)_
2. Prototype parser + IR builder (Phase 0).
3. Evaluate reuse of existing shader toolchains (glslang, SPIRV-Cross) vs bespoke emitters.
4. Design import/package system (module syntax, search paths, visibility rules, transform distribution).
5. Define library/versioning strategy so include resolution enforces compatibility.
6. Flesh out stack/class specifications (calling convention, class sugar transforms, dispatch strategy) across backends.
7. Lock down composite literal syntax across backends and add conformance tests.
8. Decide machine-code strategy (C++ emission, direct LLVM IR, third-party JIT) and prototype.
9. Define diagnostics/tooling plan (source maps, error reporting pipeline, incremental tooling, future PathSpace-native editor).
10. Document staffing/time requirements before promoting PrimeStruct onto the active PathSpace roadmap.
