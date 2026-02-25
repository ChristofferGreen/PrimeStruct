# PrimeStruct Plan

PrimeStruct is built around a simple idea: program meaning comes from two primitives—**definitions** (potential) and **executions** (actual). Both map to a single canonical **Envelope** in the AST; executions are envelopes with an implicit empty body. Compile-time transforms rewrite the surface into a small canonical core. That core is what we target to C++, GLSL, and the PrimeStruct VM. It also keeps semantics deterministic and leaves room for future tooling and visual editors.

### Source-processing pipeline
1. **Import resolver:** first pass walks the raw text and expands every `import<...>` source entry so the compiler always works on a single flattened source stream.
2. **Text transforms:** the flattened stream flows through ordered token-level transforms (operator sugar, collection literals, implicit suffixes, project-specific macros, etc.). Text transforms apply to the entire envelope (transform list, templates, parameters, and body). Executions are treated as envelopes with an implicit empty body, so the same rule applies. Text transforms may append additional text transforms to the same node.
3. **AST builder:** once text transforms finish, the parser builds the canonical AST.
4. **Template & semantic resolver:** monomorphise templates, resolve namespaces, and apply semantic transforms (effects) so the tree is fully resolved and contains only concrete envelopes.
5. **IR lowering:** emit the shared SSA-style IR only after templates/semantics are resolved; the base-level tree contains no templates or `auto`, and every backend consumes an identical canonical form.

Each stage halts on error (reporting diagnostics immediately) and exposes `--dump-stage=<name>` so tooling/tests can capture the text/tree output just before failure. Text transforms are configured via `--text-transforms=<list>`; the default list enables `collections`, `operators`, `implicit-utf8` (auto-appends `utf8` to bare string literals), and `implicit-i32` (auto-appends `i32` to bare integer literals). Order matters: `collections` runs before `operators` so map literal `key=value` pairs are rewritten as key/value arguments rather than assignment expressions. Semantic transforms are configured via `--semantic-transforms=<list>`. `--transform-list=<list>` is an auto-deducing shorthand that routes each transform name to its declared phase (text or semantic); ambiguous names are errors. Use `--no-text-transforms`, `--no-semantic-transforms`, or `--no-transforms` to disable transforms and require canonical syntax.

### Language levels (0.Concrete → 3.Surface)
PrimeStruct is organized into four language levels. Each higher level desugars into the level below it.
- **0.Concrete:** fully explicit envelopes only (definitions may omit an empty parameter list; executions still require parentheses). No text transforms, no templates, no `auto`. Explicit `return<T>`, explicit literal suffixes, canonical calls (`if(cond, then(){...}, else(){...})`, `loop/while/for` as calls).
- **1.Template:** canonical syntax plus explicit templates (`array<i32>`, `Pointer<T>`, `convert<T>(...)`). No `auto`.
- **2.Inference:** canonical syntax plus `auto`/omitted envelopes. Implicit template parameters are resolved per call site, then lowered to explicit templates.
- **3.Surface:** surface syntax and text transforms (operator sugar, collection literals, indexing sugar, `if(...) {}` blocks, etc.) that rewrite into canonical forms.

### Compilation model (v1)
- **Whole-program by default:** `import` expansion produces a single compilation unit, and semantic resolution runs over that full unit; implicit-template inference may use call sites anywhere in the expanded source. The v1 toolchain prioritises fast full rebuilds over incremental compilation.
- **Envelope stream boundary:** high-level features are lowered into the canonical envelope form, and backends consume this stable envelope stream. Emission can stream envelopes directly into IR/bytecode or native codegen without reintroducing surface syntax.
- **Deterministic emission:** canonicalization happens once, before backend selection, so all emitters see the same fully-resolved envelopes and produce consistent results.

### Language ethos (v1)
- **Simplified and coherent C:** keep the core small, explicit, and close to how the machine behaves when it matters.
- **Sane subset of C++:** keep value types, structs, and explicit control flow, but avoid implicit conversions, surprising overload rules, or hidden allocations.
- **Python spirit for ergonomics:** readable defaults and small conveniences (e.g., optional separators, concise literals), without sacrificing determinism or making meaning depend on tooling.
- **Ease of understanding first:** prefer features that are easy to explain and reason about, and reject features that add power without clarity.
- **Concrete rules:** explicit envelopes, explicit conversions, explicit effects, immutable-by-default bindings, and deterministic evaluation order.

## Phase 0 — Scope & Acceptance Gates (must precede implementation)
- **Charter:** capture exactly which language primitives, transforms, and effect rules belong in PrimeStruct, and list anything explicitly deferred to later phases.
- **Success criteria:** define measurable gates (parser coverage, IR validation, backend round-trips, sample programs)
- **Ownership map:** assign leads for parser, IR/envelope system, first backend, and test infrastructure, plus security/runtime reviewers.
- **Integration plan:** describe how the compiler/test suite slots into the build (targets, CI loops, feature flags, artifact publishing).
- **Risk log:** record open questions (borrow checker, capability taxonomy, GPU backend constraints) and mitigation/rollback strategies.
- **Exit:** only after this phase is reviewed/approved do parser/IR/backend implementations begin; the conformance suite derives from the frozen charter instead of chasing a moving target.

## Project Charter (v1 target)
- Language core: envelope syntax (definitions + executions), slash paths, namespaces, imports (source + namespace exposure), binding initializers, return annotations, effects annotations, and deterministic canonicalization rules.
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
| Borrow checker deferred in v1. | Resource safety rules vary by backend, leading to unsound or inconsistent behavior. | Document explicit resource rules and keep borrow checks out of v1. | Keep borrow-checking claims removed until a design lands. |
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
- Tooling integrations beyond basic CLI workflows.
- Backend support for software numeric envelopes (`integer`/`decimal`/`complex`) and mixed-mode numeric ops.
- Placement transforms (`stack`/`heap`/`buffer`) and placement-driven layout guarantees.
- Recursive struct layouts and cross-module layout stability guarantees.
- JIT, chunk caching, or dynamic recompilation tooling.
- IDE/LSP integration and editor tooling.
- Standard library packaging/version negotiation beyond a single in-tree reference set.
- `tools/PrimeStructc` feature parity with the main compiler and template codegen (PrimeStructc stays a minimal subset; template codegen and import version selection are explicitly out of scope for v1).
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
[return<i32>]
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
  - Current subset: fixed-width integer/bool/float literals (`i32`, `i64`, `u64`, `f32`, `f64`), locals + assign, basic arithmetic/comparisons (signed/unsigned integers plus floats), boolean ops (`and`/`or`/`not`), explicit conversions via `T{value}` for `i32/i64/u64/bool/f32/f64`, abs/sign/min/max/clamp/saturate, `if`, `print`, `print_line`, `print_error`, and `print_line_error` for integer/bool or string literals/bindings, and pointer/reference helpers (`location`, `dereference`, `Reference`) in a single entry definition.
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
- Emit high-performance C++ for engine integration, optional GLSL/SPIR-V targets via external toolchains, and bytecode for an embedded VM without diverging semantics.
- Share a consistent standard library (math, texture IO, resource bindings) across backends while preserving determinism for replay/testing.

## Proposed Architecture
- **Front-end parser:** C/TypeScript-inspired surface syntax with explicit envelope annotations, deterministic control flow, and explicit resource usage.
- **Transform pipeline:** ordered text transforms rewrite raw tokens before the AST exists; semantic transforms annotate the parsed AST before lowering. The compiler can auto-inject transforms per definition/execution (e.g., attach `operators` to every function) with optional path filters (`/std/math/*`, recurse or not) so common rewrites don’t have to be annotated manually. Transforms may also rewrite a definition’s own transform list (for example, `single_type_to_return`). The default text chain desugars infix operators, control-flow, assignment, etc.; the default semantic chain enables `single_type_to_return`. Projects can override via `--text-transforms` / `--semantic-transforms` or the auto-deducing `--transform-list`.
- **Intermediate representation:** envelope-tagged SSA-style IR shared by every backend (C++, GLSL, VM). Normalisation happens once; backends never see syntactic sugar.
- **IR definition (stable, PSIR v15):**
  - **Module:** `{ string_table, struct_layouts, functions, entry_index, version }`.
  - **Function:** `{ name, metadata, instructions }` where instructions are linear, stack-based ops with immediates.
  - **Metadata:** `{ effect_mask, capability_mask, scheduling_scope, instrumentation_flags }` (see PSIR binary layout).
  - **Instruction:** `{ op, imm }`; `op` is an `IrOpcode`, `imm` is a 64-bit immediate payload whose meaning depends on `op`.
  - **Locals:** addressed by index; `LoadLocal`, `StoreLocal`, `AddressOfLocal` operate on the index encoded in `imm`.
  - **Strings:** string literals are interned in `string_table` and referenced by index in print ops (see PSIR versioning).
  - **Entry:** `entry_index` points to the entry function in `functions`; its signature is enforced by the front-end.
  - **PSIR binary layout (little-endian):**
    - `u32 magic` (`0x50534952` = `"PSIR"`), `u32 version`, `u32 function_count`, `u32 entry_index`, `u32 string_count`.
    - `string_count` entries: `u32 byte_len` + raw bytes.
    - `u32 struct_count`.
    - `struct_count` entries: `u32 name_len` + name bytes, `u32 total_size`, `u32 alignment`, `u32 field_count`, then `field_count` entries:
      `u32 field_name_len` + bytes, `u32 envelope_len` + bytes, `u32 offset`, `u32 size`, `u32 alignment`,
      `u32 padding_kind`, `u32 category`, `u32 visibility`, `u32 is_static`.
    - `function_count` entries: `u32 name_len` + name bytes, `u64 effect_mask`, `u64 capability_mask`,
      `u32 scheduling_scope`, `u32 instrumentation_flags`, `u32 instruction_count`, then `instruction_count` entries:
      `u8 opcode` + `u64 imm`.
  - **PSIR opcode set:** see the `IrOpcode` enum and the “PSIR opcode set (v15, VM/native)” section below.
- **PSIR versioning:** serialized IR includes a version tag; v2 introduces `AddressOfLocal`, `LoadIndirect`, and `StoreIndirect` for pointer/reference lowering; v4 adds `ReturnVoid` to model implicit void returns in the VM/native backends; v5 adds a string table + print opcodes for stdout/stderr output; v6 extends print opcodes with newline/stdout/stderr flags to support `print`/`print_line`/`print_error`/`print_line_error`; v7 adds `PushArgc` for entry argument counts in VM/native execution; v8 adds `PrintArgv` for printing entry argument strings; v9 adds `PrintArgvUnsafe` to emit unchecked entry-arg prints for `at_unsafe`; v10 adds `LoadStringByte` for string indexing in VM/native backends; v11 adds struct layout manifests; v12 adds struct field visibility/static metadata; v13 adds float arithmetic/compare/convert opcodes; v14 adds float return opcodes (`ReturnF32`, `ReturnF64`); v15 adds per-function execution metadata (effect/capability masks plus scheduling/instrumentation fields).
  - **PSIR v2:** adds pointer opcodes (`AddressOfLocal`, `LoadIndirect`, `StoreIndirect`) to support `location`/`dereference`.
  - **PSIR v4:** adds `ReturnVoid` so void definitions can omit explicit returns without losing a bytecode terminator.
  - **Versioning policy:** the `version` field is a single, monotonically increasing integer for incompatible changes. There is no forward/backward compatibility guarantee today; tools reject unknown versions and require recompilation. Migration tooling may be added later, but no automatic migrations exist yet.
- **Backends:**
  - **C++ emitter** – generates host code for native binaries.
  - **GLSL emitter** – produces shader code; SPIR-V output is available via `--emit=spirv`.
  - **VM bytecode** – compact instruction set executed by the embedded interpreter/JIT.
- **Tooling:** CLI compiler `primec`, plus the VM runner `primevm` and build/test helpers. The compiler accepts `--entry /path` to select the entry definition (default: `/main`). Entry definitions currently accept either no parameters or a single `[array<string>]` parameter for command-line arguments; `args.count()` and `count(args)` are supported, `print*` calls accept `args[index]` (checked) or `at_unsafe(args, index)` (unchecked), and string bindings may be initialised from `args[index]` or `at_unsafe(args, index)` (print-only). The C++ emitter maps the array argument to `argv` and otherwise uses the same restriction. The definition/execution split maps cleanly to future node-based editors; full IDE/LSP integration is deferred until the compiler stabilises.
- **AST/IR dumps:** the debug printers include executions with their argument lists so tooling can capture scheduling intent in snapshots.
  - Dumps show collection literals after text-transform rewriting (e.g., `array<i32>{1i32,2i32}` becomes `array<i32>(1, 2)`).
  - Labeled execution arguments appear inline (e.g., `exec /execute_task([count] 2)`).

## Language Design Highlights
- **Identifiers:** `[A-Za-z_][A-Za-z0-9_]*` plus the slash-prefixed form `/segment/segment/...` for fully-qualified paths. Unicode may arrive later, but identifiers are constrained to ASCII for predictable tooling and hashing. `auto`, `mut`, `return`, `import`, `namespace`, `true`, `false`, `if`, `else`, `loop`, `while`, and `for` are reserved keywords; any other identifier (including slash paths) can serve as a transform, path segment, parameter, or binding.
-- **String literals:** surface forms accept `"..."utf8` / `"..."ascii` with escape processing, or raw `'...'utf8` / `'...'ascii` with no escape processing. The `implicit-utf8` text transform (enabled by default) appends `utf8` when omitted in surface syntax. **Canonical/bottom-level form uses double-quoted strings with escapes normalized and an explicit `utf8`/`ascii` suffix.** `ascii` enforces 7-bit ASCII (the compiler rejects non-ASCII bytes). Example surface: `"hello"utf8`, `'moo'ascii`. Example canonical: `"hello"utf8`. Raw example: `'C:\temp'ascii` keeps backslashes verbatim.
- **Numeric envelopes:** fixed-width `i32`, `i64`, `u64`, `f32`, and `f64` map directly to hardware instructions and are the only numeric envelopes supported across all backends today. Software numeric envelopes `integer`, `decimal`, and `complex` are accepted by the parser/semantic validator (bindings, returns, collections, and `convert<T>` targets), but current backends reject them at lowering/emission time. Mixed software/fixed arithmetic is rejected, and mixed software categories or ordered comparisons on `complex` are also diagnostics today.
- **Core type set (v1):** the closed set of envelopes that every backend must understand and that the type system treats as cross-backend portable is:
  - `bool`, `i32`, `i64`, `u64`, `f32`, `f64`, `string`
  - `array<T>`, `vector<T>`, `map<K, V>`
  - `Pointer<T>`, `Reference<T>`
  - User-defined struct types (including `[struct]`-tagged definitions)
  Backends may accept additional types, but any type outside this core set is backend-specific and must be rejected by backends that do not explicitly support it. For collections, element/key/value types must themselves be in the core set unless a backend explicitly documents wider support.
- **Aliases:** none for numeric widths; use explicit `i32`, `i64`, `u64`, `f32`, `f64`.
- **`auto` (implicit templates + local inference):** `auto` may appear on binding envelopes, parameters, or return transforms. In parameter/return positions it introduces an implicit template parameter (equivalent to adding a fresh type parameter) and is inferred per call site; omitted parameter envelopes are treated as `auto`. In bindings, `auto` requests local inference from the initializer and must resolve to a concrete envelope; unresolved or conflicting local inference is a diagnostic. Return `auto` is constrained by return statements; if all constraints resolve to a concrete envelope the definition becomes monomorphic.
  - Float literals accept standard decimal forms, including optional fractional digits (e.g., `1.`, `1.0`, `1.f32`, `1.e2`).
- **Envelope:** the canonical AST uses a single envelope form for definitions and executions: `[transform-list] identifier<template-list>(parameter-or-arg-list) {body-list}`. Surface definitions require an explicit `{...}` body; surface executions are call-style (`identifier<template-list>(arg-list)`) and map to an envelope with an implicit empty body. **Definitions may omit an empty parameter list** (e.g., `Foo { ... }` is treated as `Foo() { ... }`), and this is accepted even at the concrete level; executions still require parentheses. Local bindings use the form `[Envelope qualifiers…] name{initializer}` and may omit the envelope transform when the initializer lets the compiler infer it; failures to infer are diagnostics. Parameters that omit an explicit envelope are treated as `auto` and become implicit template parameters inferred per call site. Lists recursively reuse whitespace-separated tokens.
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
  - **Definition order:** call sites may reference definitions that appear later in the same file or namespace. Name resolution runs after import expansion and namespace expansion; unresolved names remain diagnostics.
  - Note: current VM/native/GLSL/C++ emitters only generate code for definitions; top-level executions are parsed/validated but not emitted (tooling-only for now).
- **Return annotation:** definitions declare return envelopes via transforms (e.g., `[return<f32>] blend<…>(…) { … }`). Definitions return values explicitly (`return(value)`); the desugared form is always canonical.
- **Surface vs canonical:** surface syntax may omit the return transform or use `return<auto>` (or `[auto]` with `single_type_to_return`) and rely on inference; canonical/bottom-level syntax always carries an explicit concrete `return<T>` after monomorphisation, and the base-level tree contains no templates or `auto`. Example surface: `main() { return(0) }` → canonical: `[return<i32>] main() { return(0i32) }`. If all return paths yield no value, `return<auto>` resolves to `return<void>`.
- **Default convenience:** the `single_type_to_return` transform rewrites a single bare envelope in the transform list into `return<envelope>` (e.g., `[i32] main()` → `[return<i32>] main()`), and it is enabled by default (disable via `--no-semantic-transforms` or override the semantic transform list). If the bare envelope is `auto`, the transform yields `return<auto>` and inference resolves it later.
Array returns use `return<array<T>>` (or `[array<T>]` with `single_type_to_return`) and surface as `array` in the IR.
Struct returns use `return<StructName>` (or inference when the body returns a struct constructor/value); they surface as `array` in the IR with the struct layout manifest attached.

Example:
```
[return<array<i32>>]
make_pair() {
  return(array<i32>{1i32, 2i32})
}
```

Expected IR (shape only):
```
module {
  def /make_pair(): array {
    return array(1, 2)
  }
}
```
- **Effects:** by default, definitions/executions start with `io_out` enabled so logging works without explicit annotations. Authors can override with `[effects(...)]` (e.g., `[effects(global_write, io_out)]`) or tighten to pure behavior by passing `primec --default-effects=none`. Standard library routines permit stdout/stderr logging via `io_out`/`io_err`; backends reject unsupported effects (e.g., GPU code requesting filesystem access). `primec --default-effects <list>` supplies the default effect set for any definition/execution that omits `[effects]` (comma-separated list; `default` and `none` are supported tokens). If `[capabilities(...)]` is present it must be a subset of the active effects (explicit or default). VM/native accept `io_out`, `io_err`, `heap_alloc`, `file_write`, and `gpu_dispatch`; GLSL only accepts `io_out` and `io_err`.
- **Execution effects:** executions may also carry `[effects(...)]`. The execution’s effects must be a subset of the enclosing definition’s active effects; otherwise it is a diagnostic. The default set is controlled by `--default-effects` in the compiler/VM.

### Backend Type Support (v1)
- **VM/native:** scalar `i32`, `i64`, `u64`, `bool`, `f32`, `f64`. `array`/`vector`/`map` support numeric/bool values; map string keys must be string literals or literal-backed bindings. Strings are limited to literals/literal-backed bindings for print/map contexts; string returns and string pointers/references are rejected. `convert<T>` supports `i32`, `i64`, `u64`, `bool`, `f32`, `f64`.
- **VM/native emitter restrictions (current):** recursive calls are rejected; lambdas are rejected (use the C++ emitter); string comparisons are rejected and string literals are limited to print/count/index/map contexts; string return types, string array returns, and string pointer/reference bindings are rejected; block arguments on non-control-flow calls and arguments on `if` branch blocks are rejected; `print*` and vector helper calls are statement-only; `File<Mode>(path)` requires a string literal or literal-backed binding; `Result.ok(value)` only accepts `i32`/`bool` payloads; unsupported math or GPU builtins fail.
- **GLSL:** numeric/bool scalar locals only (`i32`, `i64`, `u64`, `bool`, `f32`, `f64`); string literals and non-scalar bindings are rejected, and entry definitions must return `void`. `convert<T>` targets match the numeric/bool list above.
- **GLSL emitter restrictions (current):** at most one `return()` statement; static bindings are rejected; assign/increment/decrement require local mutable targets; control flow must use canonical forms (`if(cond, then() { ... }, else() { ... })`, `loop(count, body() { ... })`, `while(cond, body() { ... })`, `for(init, cond, step, body() { ... })`); builtins require positional args with no template/block arguments, and unsupported builtins fail.
- **GLSL type support (current):** scalar `bool`, `i32`, `u32`, `i64`, `u64`, `f32`, `f64` only. Using `i64`/`u64` or `f64` emits `GL_ARB_gpu_shader_int64`/`GL_ARB_gpu_shader_fp64` requirements. Arrays, strings, structs, pointers/references, maps, and vectors are rejected.
- **GLSL effects/capabilities (current):** only `io_out` and `io_err` are accepted as metadata; other effects/capabilities are rejected. `print*` calls are accepted but emitted as no-op expressions.
- **GLSL determinism (current):** only local scalar bindings are allowed; no static storage or heap/placement transforms. GPU backends are treated as deterministic with no external I/O.
- **GPU compute (draft):**
  - A definition tagged with `[compute]` is lowered as a GPU kernel. Kernels are `void` and write outputs via buffer parameters rather than return values.
  - `workgroup_size(x, y, z)` fixes the local group size for the kernel; only valid alongside `[compute]`.
  - Kernel bodies are restricted to the GPU-safe subset (POD/`gpu_lane` types, fixed-width numeric envelopes, no IO, no heap, no strings, no recursion). Backends reject unsupported features early with diagnostics.
  - Host-side submission uses `/std/gpu/dispatch(kernel, gx, gy, gz, args...)` and requires `effects(gpu_dispatch)`.
  - VM/native fallback currently requires `/std/gpu/buffer<T>(count)` to use a constant `i32` literal size.
  - GPU builtins live under `/std/gpu/*` (see Core library surface).
  - GPU ID helpers are scalar: `/std/gpu/global_id_x()`, `/std/gpu/global_id_y()`, `/std/gpu/global_id_z()` return `i32`.
- **Capability taxonomy (v1):**
  - **IO:** `io_out` (stdout), `io_err` (stderr), `file_write` (filesystem output).
  - **Memory:** `heap_alloc` (dynamic allocation), `global_write` (mutating global state).
  - **Assets:** `asset_read`, `asset_write` (asset/database I/O).
  - **GPU:** `gpu_dispatch` (host-side GPU submission/dispatch).
  - Unknown capability names are errors; capability identifiers are `lower_snake_case`.
- **Tooling vs runtime visibility:**
  - **Tooling surfaces:** declared effects/capabilities, resolved defaults, entry defaults, and backend allowlist violations (diagnostics).
  - **Runtime-only logs:** resolved effect/capability masks and execution identifiers (path hashes) for tracing; source spans are optional/debug-only.
- **Paths & imports:** every definition/execution lives at a canonical path (`/ui/widgets/log_button_press`). Authors can spell the path inline or rely on `namespace foo { ... }` blocks to prepend `/foo` automatically. Import expansion produces a single compilation unit; implicit-template inference may use call sites anywhere in that unit; there are no module boundaries. Import paths are parsed before text transforms, so they remain quoted without literal suffixes.
  - **Source imports:** `import<"/std/io", version="1.2.0">` resolves packages from the import path (zipped archive or plain directory) whose layout mirrors `/version/first_namespace/second_namespace/...`. The angle-bracket list may contain multiple quoted string paths—`import<"/std/io", "./local/io/helpers", version="1.2.0">`—and the resolver applies the same version selector to each path; mismatched archives raise an error before expansion. Versions live in the leading segment (e.g., `1.2/std/io/*.prime` or `1/std/io/*.prime`). If the version attribute provides one or two numbers (`1` or `1.2`), the newest matching archive is selected; three-part versions (`1.2.0`) require an exact match. Each `.prime` source file is expanded exactly once and registered under its namespace/path (e.g., `/std/io`); duplicate imports are ignored. Folders prefixed with `_` remain private.
  - **Namespace imports:** `import /foo/*` brings the immediate **public** children of `/foo` into the root namespace. `import /foo/bar` brings a single **public** definition (or builtin) by its final segment; importing a non-public definition is a diagnostic. `import /foo` is shorthand for `import /foo/*` (except `/std/math`, which is unsupported without `/*` or an explicit name). `import /std/math/*` brings all math builtins into the root namespace, or import a subset via `import /std/math/sin /std/math/pi`; `import /std/math` without a wildcard or explicit name is not supported. Imports are resolved after source imports and can be listed as `import /std/math/*, /util/*` or whitespace-separated paths.
  - **Exports:** definitions are private by default. Add `[public]` to a definition (function, struct, method) to make it importable; `[private]` explicitly marks it as non-exported. Private definitions are still callable within the same compilation unit; visibility only affects imports.
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
  - **Definitions only:** `compute`, `workgroup_size(x, y, z)`, `unsafe`.
  - **Struct/tag only (definitions):** `struct`, `pod`, `handle`, `gpu_lane`, `align_bytes(n)`, `align_kbytes(n)`.
  - **Definitions/bindings:** access/visibility markers (`public`, `private`). `static` is valid on bindings and struct helpers (disables implicit `this` on helpers).
  - **Reserved/rejected in v1:** `stack`, `heap`, `buffer` placement transforms (diagnostic).
  - Any transform outside its allowed scope is a compile-time error with a diagnostic naming the enclosing path.

### Example function syntax
```
import /std/math/*
namespace demo {
  [return<void> effects(io_out)]
  hello_values() {
    [string] message{"Hello PrimeStruct"utf8}
    [i32] iterations{3i32}
    [f32 mut] exposure{1.25f32}
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
When an expression is immediately followed by a binding transform list (`[...] name{...}`), the parser treats the
binding as a new statement rather than indexing sugar on the previous expression; use `at(value, index)` or a semicolon
if you intended to index.

### Slash paths & textual operator transforms
- Slash-prefixed identifiers (`/pkg/module/thing`) are valid anywhere the Envelope expects a name; `namespace foo { ... }` is shorthand for prepending `/foo` to enclosed names, and namespaces may be reopened freely.
- Text transforms run before the AST exists. Operator transforms scan the raw character stream and rewrite when they see a left operand and right operand, allowing optional whitespace around the operator token. Slash paths remain intact when `/` begins a path segment with no left operand (start of line or immediately after whitespace/delimiters). Binary operators respect standard precedence and associativity: `*`/`/` bind tighter than `+`/`-`, comparisons (`<`, `>`, `<=`, `>=`, `==`, `!=`) bind tighter than `&&`/`||`, and assignment (`=`) is lowest precedence and right-associative. Operators follow the same operand-based rule (`a > b` → `greater_than(a, b)`, `a < b` → `less_than(a, b)`, `a >= b` → `greater_equal(a, b)`, `a <= b` → `less_equal(a, b)`, `a == b` → `equal(a, b)`, `a != b` → `not_equal(a, b)`, `a && b` → `and(a, b)`, `a || b` → `or(a, b)`, `!a` → `not(a)`, `-a` → `negate(a)`, `a = b` → `assign(a, b)`, `++a` / `a++` → `increment(a)`, `--a` / `a--` → `decrement(a)`).
- Because imports expand first, slash paths survive every transform untouched until the AST builder consumes them, and IR lowering never needs to reason about infix syntax.

### Struct & envelope categories
- **Struct tag as transform:** any of `[struct]`, `[pod]`, `[handle]`, or `[gpu_lane]` marks the envelope as a struct-style definition. It records a layout manifest (field names, envelopes, offsets) and validates the body, but the underlying syntax remains a standard definition. Struct-tagged definitions are field-only: no parameters or return transforms, and no return statements. The body may include nested helper definitions; only stack-value bindings contribute to layout. Non-lifecycle helpers lower into the struct method namespace (`/<Struct>/name`) and can be called via method sugar or as plain namespace functions. Un-tagged definitions may still be instantiated as structs; they simply skip the extra validation/metadata until another transform demands it.
- **Placement policy:** where a value lives (stack/heap/buffer) is decided by allocation helpers plus capabilities, not by struct tags. Envelopes may express requirements (e.g., `pod`, `handle`, `gpu_lane`), but placement is a call-site decision gated by capabilities. The `stack`/`heap`/`buffer` transforms remain reserved and rejected in v1.
- **POD tag as validation:** `[pod]` on a struct-style definition asserts trivially-copyable semantics. Violations (hidden lifetimes, handles, async captures) raise diagnostics; without the tag the compiler treats the body permissively.
- **Member syntax:** every field is just a stack-value execution (`[f32 mut] exposure{1.0f32}`, `[handle<PathNode>] target{get_default()}`). Attributes (`[mut]`, `[align_bytes(16)]`, `[handle<PathNode>]`) decorate the execution, and transforms record the metadata for layout consumers.
- **Method calls & indexing:** `value.method(args...)` desugars to `/<envelope>/method(value, args...)` in the method namespace (no hidden object model), where `<envelope>` is the envelope name associated with `value`. For struct helpers with implicit `this`, the receiver becomes the hidden `this` parameter; static helpers do not accept method-call sugar. For arrays, `value.count()` rewrites to `/array/count<T>(value)`; the helper `count(value)` simply forwards to `value.count()`. Indexing uses the safe helper by default: `value[index]` rewrites to `at(value, index)` with bounds checks; `at_unsafe(value, index)` skips checks.
- **Struct body semantics (example):**
  ```
  [struct]
  BrushSettings {
    // Field bindings contribute to layout.
    [f32] size{12.0f32}
    [f32] hardness{0.75f32}

    // Helpers are ordinary definitions in /BrushSettings/* and do not affect layout.
    // Non-static helpers have an implicit `this` (Reference<Self>).
    [public return<f32>]
    clampSize([f32] value) {
      return(clamp(value, 1.0f32, 64.0f32))
    }

    // Method-call sugar supplies the implicit `this` (settings.normalize()).
    [public return<f32>]
    normalize() {
      return(this.clampSize(this.size))
    }

    // Static helper: no implicit `this`.
    [public static return<f32>]
    defaultSize() {
      return(12.0f32)
    }

    // `mut` makes `this` writable inside the helper.
    [public mut return<void>]
    setSize([f32] value) {
      assign(this.size, this.clampSize(value))
    }
  }
  ```
- **Baseline layout rule:** members default to source-order packing. Backend-imposed padding is allowed only when the metadata (`layout.fields[].padding_kind`) records the reason; `[no_padding]` and `[platform_independent_padding]` fail the build if the backend cannot honor them bit-for-bit.
- **Alignment transforms:** `[align_bytes(n)]` (or `[align_kbytes(n)]`) may appear on the struct or field; violations again produce diagnostics instead of silent adjustments.
- **Stack value executions:** every local binding—including struct “fields”—materializes via `[Type qualifiers…] name{initializer}` so stack frames remain declarative (e.g., `[f32 mut] exposure{1.0f32}`). Default initializers are mandatory for fields; local bindings may omit the initializer only when the envelope is a struct type with a zero-argument constructor **and** the compiler can prove the construction has no outside effects. In that case the binding desugars to `Type()` (e.g., `[BrushSettings] s` → `[BrushSettings] s{BrushSettings()}`).
  - **No outside effects (definition):** zero-arg construction is outside-effect-free only when all of the following hold:
    - The constructor path has an empty effects/capabilities mask (no `effects(...)`/`capabilities(...)`, and no callees with non-empty effects).
    - Writes are limited to the newly constructed value (`this`) and local temporaries; any writes through pointers/references or to non-local bindings are disallowed.
    - Field initializers are effect-free under the same rules, and all called helpers are effect-free transitively.
    - If the compiler cannot prove these constraints, omitted-initializer bindings are rejected.
  - Multi-step initializer example:
    ```
    [return<f32>]
    main() {
    [f32] x{
      [f32 mut] tmp{1.0f32}
      tmp = tmp + 2.0f32
      tmp
    }
      return(x)
    }
    ```
- **Lifecycle helpers (Create/Destroy):** Within a struct-tagged or field-only struct definition, nested definitions named `Create` and `Destroy` gain constructor/destructor semantics. Placement-specific variants add suffixes (`CreateStack`, `DestroyHeap`, etc.). Without these helpers the field initializer list defines the default constructor/destructor semantics. Struct helpers receive an implicit `this` unless marked `[static]`; add `mut` to the helper’s transform list when it writes to `this` (otherwise `this` stays immutable). Lifecycle helpers must return `void` and accept no parameters. We capitalise system-provided helper names so they stand out, but authors are free to use uppercase identifiers elsewhere—only the documented helper names receive special treatment.
  - **Helper visibility:** nested non-lifecycle helpers are normal definitions in the struct method namespace. Use `[public]` on the helper definition to export it; private helpers remain callable within the same compilation unit. Non-static helpers receive an implicit `this` (`Reference<Self>`); `[static]` disables the implicit `this` and method-call sugar.
  ```
  import /std/math/*
  namespace demo {
    [struct pod]
    color_grade() {
      [f32 mut] exposure{1.0f32}

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
- **Categories:** `[pod]`, `[handle]`, `[gpu_lane]` tags classify members for resource rules. Handles remain opaque tokens with subsystem-managed lifetimes; GPU lanes require staging transforms before CPU inspection.
- **Category mapping:**
  - `pod`: stored inline; treated as trivially-copyable for layout.
  - `handle`: stored as an opaque reference token; lifetime is managed by the owning subsystem.
  - `gpu_lane`: stored as a GPU-only handle; CPU access requires explicit staging transforms.
  - Un-tagged fields default to `default`.
  - Conflicts are rejected (`pod` with `handle` or `gpu_lane`, `handle` with `gpu_lane`, and `pod` definitions containing `handle`/`gpu_lane` fields).

### Transforms (draft)
- **Purpose:** transforms are metafunctions that rewrite tokens (text transforms) or stamp semantic flags on the AST (semantic transforms). Later passes (backend filters) consume the semantic flags; transforms do not emit code directly.
- **Evaluation mode:** when the compiler sees `[transform ...]`, it routes through the metafunction's declared signature—pure token rewrites operate on the raw stream, while semantic transforms receive the AST node and in-place metadata writers.
- **Registry note:** only registered text transforms can appear in `text(...)` groups or `--text-transforms`; only registered semantic transforms can appear in `semantic(...)` groups or `--semantic-transforms`. Other transform names are treated as semantic directives and validated by the semantics pass (they cannot be forced into `text(...)`).

**Text transforms (token-level, registered)**
- **`append_operators`:** injects `operators` into the leading transform list when missing, enabling text-transform self-expansion without repeating `operators` everywhere.
- **`operators`:** desugars infix/prefix operators, comparisons, boolean ops, assignment, and increment/decrement (`++`/`--`) into canonical calls (`plus`, `less_than`, `assign`, `increment`, etc.).
  - Example: `a = b` rewrites to `assign(a, b)`.
- **`collections`:** rewrites `array<T>{...}` / `vector<T>{...}` / `map<K,V>{...}` literals into constructor calls.
- **`implicit-utf8`:** appends `utf8` to bare string literals.
- **`implicit-i32`:** appends `i32` to bare integer literals (enabled by default).
  - Text transform arguments are limited to identifiers and literals (no nested envelopes or calls).

**Semantic transforms (AST-level, registered)**
- **`single_type_to_return`:** semantic transform that rewrites a single bare envelope in a transform list into `return<envelope>` (e.g., `[i32] main()` → `[return<i32>] main()`); enabled by default, but can be disabled or overridden via `--no-semantic-transforms`, `--semantic-transforms`, or `--transform-list`.

**Semantic directives (AST-level, validated)**
- **`copy`:** force a copy (instead of a move) on entry for a parameter or binding. Only valid for `Copy` types; otherwise a diagnostic. Often paired with `mut`.
- **`mut`:** mark the local binding as writable; without it the binding behaves like a `const` reference. On definitions, `mut` is valid on struct helpers (including lifecycle helpers) to make the implicit `this` mutable; using `mut` on a `[static]` helper is a diagnostic. Executions do not accept `mut`.
- **`restrict<T>`:** constrain the accepted envelope to `T`. For bindings/parameters this is equivalent to writing the envelope directly (e.g., `[i32] x{...}`), and canonicalization rewrites `[i32]` into `[restrict<i32>]` at the low level.
- **`unsafe`:** marks a definition body as an unsafe scope. Aliasing rules and pointer-to-reference conversions are relaxed within the body, but references created there must not escape the unsafe scope.
- **`return<T>`:** optional contract that pins the inferred return envelope. `return<auto>` requests inference; unresolved or conflicting returns are diagnostics.
- **`effects(...)`:** declare side-effect capabilities; absence implies purity. Backends reject unsupported capabilities.
- **Transform scope:** `effects(...)` and `capabilities(...)` are only valid on definitions/executions, not bindings.
- **`align_bytes(n)`, `align_kbytes(n)`:** encode alignment requirements for struct members and buffers. `align_kbytes` applies `n * 1024` bytes before emitting the metadata.
- **`no_padding`, `platform_independent_padding`:** layout constraints for struct definitions; they reject backend-added padding or non-deterministic padding respectively.
- **`capabilities(...)`:** reuse the transform plumbing to describe opt-in privileges without encoding backend-specific scheduling hints.
- **`compute`:** marks a definition as a GPU kernel; kernel bodies are validated against the GPU-safe subset.
- **`workgroup_size(x, y, z)`:** fixes the kernel's local workgroup size (must appear with `compute`).
- **`struct`, `pod`, `handle`, `gpu_lane`:** declarative tags that emit metadata/validation only. They never change syntax; instead they fail compilation when the body violates the advertised contract (e.g., `[pod]` forbids handles/async fields).
- **`public`, `private`:** visibility tags. On definitions, they control export visibility for imports (default: private). On bindings, they control field visibility (default: public). Mutually exclusive.
- **`static`:** on fields, hoists storage to namespace scope while keeping the field in the layout manifest. On struct helpers, disables the implicit `this` parameter and method-call sugar.
- **`stack`, `heap`, `buffer`:** placement transforms reserved for future backends; currently rejected in validation.
- **`shared_scope`:** loop-only transform that makes a loop body share one scope across all iterations. Valid on `loop`/`while`/`for` only. Bindings declared in the loop body are initialized once before the loop body runs and persist for the duration of the loop without escaping the surrounding scope.
The lists above reflect the built-in transforms recognized by the compiler today; future additions will extend them here.

### Core library surface (draft)
- **Standard math (draft):** the core math set lives under `/std/math/*` (e.g., `/std/math/sin`, `/std/math/pi`). `import /std/math/*` brings these names into the root namespace so `sin(...)`/`pi` resolve without qualification. Unsupported envelope/operation pairs produce diagnostics. Only fixed-width numeric envelopes are supported today; software numeric envelopes are parsed/typed but are rejected by current backends.
  - **Constants:** `/std/math/pi`, `/std/math/tau`, `/std/math/e`.
  - **Basic:** `/std/math/abs`, `/std/math/sign`, `/std/math/min`, `/std/math/max`, `/std/math/clamp`, `/std/math/lerp`, `/std/math/saturate`.
  - **Rounding:** `/std/math/floor`, `/std/math/ceil`, `/std/math/round`, `/std/math/trunc`, `/std/math/fract`.
  - **Power/log:** `/std/math/sqrt`, `/std/math/cbrt`, `/std/math/pow`, `/std/math/exp`, `/std/math/exp2`, `/std/math/log`, `/std/math/log2`, `/std/math/log10`.
  - **Operand rules (current):** `abs`, `sign`, `saturate`, `min`, `max`, `clamp`, `lerp`, and `pow` accept numeric operands (`i32`, `i64`, `u64`, `f32`, `f64`). `min`, `max`, `clamp`, `lerp`, and `pow` reject mixed signed/unsigned or mixed integer/float operands. All remaining `/std/math/*` builtins require float operands (`atan2`, `hypot`, `copysign` are binary; `fma` is ternary).
  - **Integer pow:** for integer operands, `pow` requires a non-negative exponent; negative exponents abort in VM/native (stderr + exit code `3`), and the C++ emitter mirrors this behavior.
  - **Trig:** `/std/math/sin`, `/std/math/cos`, `/std/math/tan`, `/std/math/asin`, `/std/math/acos`, `/std/math/atan`, `/std/math/atan2`, `/std/math/radians`, `/std/math/degrees`.
  - **Hyperbolic:** `/std/math/sinh`, `/std/math/cosh`, `/std/math/tanh`, `/std/math/asinh`, `/std/math/acosh`, `/std/math/atanh`.
  - **Float utils:** `/std/math/fma`, `/std/math/hypot`, `/std/math/copysign`.
  - **Predicates:** `/std/math/is_nan`, `/std/math/is_inf`, `/std/math/is_finite`.
  - **Backend limits (current):** VM/native math uses fast approximations and is validated against the C++/exe baseline within tolerances. Large-magnitude trig/log/exp inputs are only required to stay finite and within basic range bounds (e.g., `|sin(x)| <= 1`), and may diverge from the C++/exe baseline. Conformance tests use tolerance or range checks for these cases.
  - **Vector & color types (draft):** stdlib ships `.prime` definitions for `Vec2`, `Vec3`, `Vec4`, `ColorRGB`, `ColorRGBA`, `ColorSRGB`, `ColorSRGBA`. These are distinct types (colors are not aliases of vectors) with their own method surfaces.
    - **Vectors:** constructors, component accessors, and member methods like `length()`, `normalize()` (in-place), and `toNormalized()` (returns a new value).
    - **Colors:** constructors plus color-space helpers (e.g., sRGB/linear conversions) and per-channel ops. sRGB types remain distinct from linear `ColorRGB`/`ColorRGBA`.
- **`assign(target, value)`:** canonical mutation primitive; only valid when `target` carried `mut` at declaration time. The call evaluates to `value`, so it can be nested or returned.
- **`increment(target)` / `decrement(target)`:** canonical mutation helpers used by `++`/`--` desugaring. Only valid on mutable numeric bindings; they evaluate to the updated value.
- **`count(value)` / `value.count()` / `at(value, index)` / `at_unsafe(value, index)`:** collection helpers. `value.count()` is canonical; `count(value)` forwards to the method when available. `at` performs bounds checking; `at_unsafe` does not. In the VM/native backends, an out-of-bounds `at` aborts execution (prints a diagnostic to stderr and returns exit code `3`).
- **Runtime error reporting (VM/native):** runtime guards (bounds checks, missing map keys, vector capacity, invalid integer `pow` exponents) abort execution, print a diagnostic to stderr, and return exit code `3`. Parse/semantic/lowering errors remain exit code `2`.
- **`print(value)` / `print_line(value)` / `print_error(value)` / `print_line_error(value)`:** stdout/stderr output primitives (statement-only). `print`/`print_line` require `io_out`, and `print_error`/`print_line_error` require `io_err`. VM/native backends support integer/bool values plus string literals/bindings; other string operations still require the C++ emitter.
- **`plus`, `minus`, `multiply`, `divide`, `negate`:** arithmetic wrappers used after operator desugaring. Operands must be numeric (`i32`, `i64`, `u64`, `f32`, `f64`); bool/string/pointer operands are rejected. Mixed signed/unsigned integer operands are rejected in VM/native lowering (`u64` only combines with `u64`), and `negate` rejects unsigned operands. Pointer arithmetic is only defined for `plus`/`minus` with a pointer on the left and an integer offset (see Pointer arithmetic below).
- **`greater_than(left, right)`, `less_than(left, right)`, `greater_equal(left, right)`, `less_equal(left, right)`, `equal(left, right)`, `not_equal(left, right)`, `and(left, right)`, `or(left, right)`, `not(value)`:** comparison wrappers used after operator/control-flow desugaring. Comparisons respect operand signedness (`u64` uses unsigned ordering; `i32`/`i64` use signed ordering), and mixed signed/unsigned comparisons are rejected in the current IR/native subset; `bool` participates as a signed `0/1`, so `bool` with `u64` is rejected as mixed signedness. Boolean combinators accept `bool` inputs only. Control-flow conditions (`if`/`while`/`for`) require `bool` results; use comparisons or `bool{value}` when needed. The current IR/native subset accepts integer/bool/float operands for comparisons; string comparisons still require the C++ emitter.
- **`/std/math/clamp(value, min, max)`:** numeric helper used heavily in rendering scripts. VM/native lowering supports integer clamps (`i32`, `i64`, `u64`) and float clamps (`f32`, `f64`) and follows the usual integer promotion rules (`i32` mixed with `i64` yields `i64`, while `u64` requires all operands to be `u64`). Mixed signed/unsigned clamps are rejected.
- **`if(condition, then() { ... }, else() { ... })`:** canonical conditional form after control-flow desugaring.
  - Signature: `if(Envelope, Envelope, Envelope)`
  - 1) must evaluate to a boolean (`bool`), either a boolean value or a function returning boolean
  - 2) must be a definition envelope; its body yields the `if` result when the condition is `true`
  - 3) must be a definition envelope; its body yields the `if` result when the condition is `false`
  - Evaluation is lazy: the condition is evaluated first, then exactly one of the two definition bodies is evaluated.
- **`loop(count) { ... }`:** statement-only loop helper. `count` must be an integer envelope (`i32`, `i64`, `u64`), and the body is required. Negative counts are errors.
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

for([i32 mut] i{0i32}; i < 10; ++i) {
  work(i)
}
```

Canonical (after desugaring):
```
loop(5i32, do() { work() })

while(less_than(i, 10i32), do() { work(i) })

for(
  [i32 mut] i{0i32},
  less_than(i, 10i32),
  increment(i),
  do() { work(i) }
)
```
- **`return(value)`:** explicit return primitive; may appear as a statement inside control-flow blocks. For `void` definitions, `return()` is allowed. Implicit `return(void)` fires at end-of-body when omitted. Non-void definitions must return on all control paths; fallthrough is a compile-time error. Inside value blocks (binding initializers / brace constructors), `return(value)` returns from the block and yields its value.
- **IR note:** VM/native IR lowering supports numeric/bool `array<T>(...)` and `vector<T>(...)` calls plus `array<T>{...}` and `vector<T>{...}` literals, along with `count`/`at`/`at_unsafe` on those sequences. Map literals are supported in VM/native for numeric/bool values, and string-keyed maps work when the keys are string literals or bindings backed by literals (string table entries). VM/native vectors use fixed capacity; `push`/`reserve` succeed while capacity allows and error once exceeded, and shrinking helpers (`pop`, `clear`, `remove_at`, `remove_swap`) continue to work against the fixed capacity.

### Standard Library Reference (v0)
- **Status:** stable snapshot of the current builtin surface (v0).
- **Versioning (planned):**
  - Each package declares a semantic version (e.g., `1.2.0`).
  - `import<..., version="1.2.0">` selects a specific package revision.
  - There is no compiler flag to pin the default package set yet; use explicit versions in `import` declarations.
- **Namespaces:**
  - `/std/math/*` is imported via `import /std/math/*` (or explicit names like `import /std/math/sin /std/math/pi`).
  - Core builtins (`assign`, `count`, `print*`, `convert`, etc.) live in the root namespace.
- **Conformance markers:**
  - **`C++`**: supported only by the C++ emitter (VM/native reject).
  - **`VM/native`**: supported by the VM + native backends.
  - **`VM/native (limited)`**: supported with the listed restrictions.
- **Conformance notes:**
  - String comparisons are **`C++`** only; VM/native reject them.
  - `map<K, V>` values must be numeric/bool for **`VM/native`**; string values are **`C++`** only.
  - String indexing in **`VM/native (limited)`** requires string literals or bindings backed by literals.
  - VM/native vectors are fixed-capacity; `push`/`reserve` beyond capacity is a runtime error.
- **Core builtins (root namespace):**
  - **`assign(target, value)`** (statement): mutates a mutable binding or dereferenced pointer.
  - **`increment(target)` / `decrement(target)`** (statement): mutation helpers used by `++`/`--` desugaring.
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
  - **Ownership helpers:** `move`, `clone`.
  - **Uninitialized helpers (draft):** `init`, `drop`, `take`, `borrow`.
  - **GPU builtins (draft):**
    - `/std/gpu/global_id_x()` → `i32` (kernel invocation x coordinate).
    - `/std/gpu/global_id_y()` → `i32` (kernel invocation y coordinate).
    - `/std/gpu/global_id_z()` → `i32` (kernel invocation z coordinate).
    - `/std/gpu/buffer_load(Buffer<T>, index)` / `/std/gpu/buffer_store(Buffer<T>, index, value)` for storage buffers.
    - `/std/gpu/buffer<T>(count)` / `/std/gpu/upload(array<T>)` / `/std/gpu/readback(Buffer<T>)` for host-side resource management.
    - `/std/gpu/dispatch(kernel, gx, gy, gz, args...)` submits a compute kernel and requires `effects(gpu_dispatch)`. For determinism in v1, dispatch is blocking and `/std/gpu/readback` returns only after completion.
  - **Operators (desugared forms):** `plus`, `minus`, `multiply`, `divide`, `negate`, `increment`, `decrement`.
  - **Comparisons/booleans:** `greater_than`, `less_than`, `greater_equal`, `less_equal`, `equal`, `not_equal`, `and`, `or`, `not`.
  - **Result helpers (draft):**
    - `Result<Error>` is a status-only wrapper for fallible operations; `Result<T, Error>` carries a value on success.
    - `Result.ok()` (or `Result.ok(value)` for value-carrying results) constructs a success value.
    - `Result.error()` returns `true` when the result is an error.
    - `Result.why()` returns an owned `string` describing the error (heap-allocated by default).
    - The postfix `?` operator unwraps a `Result` or propagates the error (see Error Handling).
    - `Result.map(result, fn)` applies `fn` to the success value (if any) and returns a new `Result`.
    - `Result.and_then(result, fn)` (a.k.a. bind) applies `fn` to the success value and flattens the result.
    - `Result.map2(a, b, fn)` applies `fn` if both results are ok; otherwise returns the first error.

Signature sketches (not surface syntax):
```
Result.map      : Result<T, Error> x (T -> U) -> Result<U, Error>
Result.and_then : Result<T, Error> x (T -> Result<U, Error>) -> Result<U, Error>
Result.map2     : Result<A, Error> x Result<B, Error> x (A, B -> C) -> Result<C, Error>
```

Example (surface):
```
[return<Result<i32, ParseError>>]
parse_and_double([string] text) {
  return(Result.map(parse_i32(text), []([i32] v) { return(multiply(v, 2i32)) }))
}

[return<Result<i32, FileError>>]
sum_two_files([string] a, [string] b) {
  return(Result.map2(read_i32(a), read_i32(b), []([i32] x, [i32] y) {
    return(plus(x, y))
  }))
}
```

### Error Handling (draft)
- **`Result` propagation:** the postfix `?` operator unwraps `Result<T, Error>` in-place; on error, it invokes a local
  `on_error` handler and then returns the error from the current function.
  - **Monadic view:** `value?` is equivalent to binding the success value and early-returning the error; it matches
    `Result.and_then` semantics and is the recommended shorthand for fallible sequencing.
- **Local handlers:** error handling is explicit and local to the scope that declares it.
  - `on_error<ErrorType, Handler>(args...)` is a semantic transform that attaches an error handler to a definition or
    block body.
  - Handlers do **not** flow into nested blocks; any nested block using `?` must declare its own `on_error`.
  - A missing `on_error` for a `?` usage is a compile-time error.
  - The handler signature is `Handler(ErrorType err, args...)`.
  - Bound arguments are evaluated when the error is raised, not at declaration time.

### File I/O (draft)
- **RAII object:** `File<Mode>` is the owning file handle with automatic close on scope exit (`Destroy`).
  - `File<Mode>` is move-only; `Clone` is a compile-time error.
  - `close()` disarms the handle so `Destroy` becomes a no-op.
- **Modes:** `Read`, `Write`, `Append`.
- **Construction:** `File<Mode>(path)` returns `Result<File<Mode>, FileError>`.
  - `path` is a `string` (string literal or literal-backed binding in VM/native).
- **Methods (all return `Result<FileError>`):**
  - `write(values...)` (variadic text write)
  - `write_line(values...)` (variadic + newline)
  - `write_byte(u8_value)`
  - `write_bytes(array<u8>)`
  - `flush()`
  - `close()`
- **Error type:** `FileError` carries `why()` (owned `string`).
- **Effect requirement:** all file operations require `effects(file_write)`.
- **Example:**
  ```
  [return<Result<FileError>> effects(file_write, io_err)
   on_error<FileError, /log_file_error>(path)]
  write_ppm([string] path, [i32] width, [i32] height) {
    [File<Write>] file{ File<Write>(path)? }
    file.write_line("P3")?
    file.write_line(width, " ", height)?
    file.write_line("255")?
    file.write_line(255, " ", 0, " ", 0)?
    return(Result.ok())
  }

  [effects(io_err)]
  /log_file_error([FileError] err, [string] path) {
    print_line_error(path)
    print_line_error(": "utf8)
    print_line_error(err.why())
  }
  ```

## Runtime Stack Model
- **Frames:** VM/native lowering currently emits a single entry frame; user-defined calls are inlined, so there is no runtime call stack or captures in PSIR v15. Locals live in fixed 16-byte slots; `location(...)` yields a byte offset into this slot space and `dereference` uses `LoadIndirect`/`StoreIndirect`.
- **Deterministic evaluation:** arguments evaluate left-to-right; boolean `and`/`or` short-circuit; `return(value)` unwinds the current definition. In value blocks, `return(value)` exits the block and yields its value. Implicit `return(void)` fires if a definition body reaches the end.
- **Indirect alignment:** indirect addresses must be 16-byte aligned; misaligned dereferences are VM runtime errors.
- **Transform boundaries:** text/semantic rewrites decide where bodies inline; IR lowering preserves left-to-right argument evaluation inside the single frame.
- **Resource handles:** handles live inside frame slots as opaque values; lifetimes follow lexical scope.
- **Tail execution:** lowering marks tail-position calls in metadata; backends may reuse frames when the tail flag is set. VM/native currently ignore the hint; GPU backends may require tail-safe forms for determinism.
- **Effect annotations:** purity by default; explicit `[effects(...)]` opt-ins. Effects are validated during lowering; runtime enforcement is limited to builtin checks.

### Execution Metadata
- **Scheduling scope:** queue/thread selection stays host-driven; there are no stack- or runner-specific annotations yet, so executions inherit the embedding runtime’s default placement. Lowering writes `scheduling_scope = 0` in PSIR metadata until explicit scheduling transforms exist.
- **Effects & capabilities:** lowering records `effect_mask` and `capability_mask` in PSIR metadata. If no `capabilities` transform is supplied, `capability_mask` mirrors the active effects for the definition; explicit capability lists narrow the mask.
- **Instrumentation:** `instrumentation_flags` are reserved for tracing/source-map hooks. Bit 0 (`tail_execution`) is set when the final statement is `return(def_call(...))` or (for `void` definitions) a final `def_call(...)`. Backends may treat this as a tail-call hint; no backend currently requires it.

## Type System v1 (draft)

### Goals
- Deterministic, explicit typing across backends.
- Minimal implicit behavior; no hidden conversions.
- Portable core type set with explicit backend extensions.
- Whole-program checks remain supported, but local reasoning is preferred.

### Type Grammar (canonical)
- **Atomic:** `bool`, `i32`, `i64`, `u64`, `f32`, `f64`, `string`, `void`, `Self`.
- **Composite:** `array<T>`, `vector<T>`, `map<K, V>`, `Pointer<T>`, `Reference<T>`.
- **User types:** struct definitions and named aliases.
- **Template applications:** `Name<T1, T2, ...>`.

### Core Type Set (portable)
- `bool`, `i32`, `i64`, `u64`, `f32`, `f64`, `string`.
- `array<T>`, `vector<T>`, `map<K, V>` where parameters are core types.
- `Pointer<T>`, `Reference<T>` where `T` is primitive or a struct type.
- User-defined structs with layout manifests.
- Types outside this set are backend-specific and must be rejected by backends that do not support them.

### Backend Profiles
- A definition is well-typed only with respect to a backend profile.
- Profiles include: `vm_native`, `glsl`, `cpp`.
- Each profile declares supported types, effects, and constructs.
- A definition may declare `[profile(vm_native, cpp)]`. If omitted, the compiler target profile is used.

### Typing Context
- Type checking runs after import expansion and namespace resolution.
- The typing context maps local bindings, parameters, signatures, struct layouts, and import aliases to resolved types.

### Expression Rules
- **Literals:** fixed type determined by suffix (`1i32`, `2u64`, `1.0f32`, `"hi"utf8`, `true`).
- **Bindings:** declared type in the transform list, or `auto` if omitted.
- **Calls:** well-typed if the callee resolves to a definition or builtin, and every argument matches the parameter type.
- **Constructors:** struct constructors follow call rules; all fields must be provided or defaulted.
- **`convert<T>` and `T{...}`:** explicit conversions; `T` must be a supported conversion target.
- **`assign(target, value)`:** allowed only when `target` is mutable and `value` has the exact same type.
- **Control flow:** `if`, `while`, and `for` conditions must be `bool`. `loop(count)` requires an integer type.
- **Pointers/references:** targets are restricted as described in pointer rules; no implicit conversions.

### Definition Rules
- Parameters use binding syntax and must resolve to concrete types before lowering (per instantiation).
- Return types are explicit (`return<T>`) or inferred from `return(value)` sites when `return<auto>` is used.
- All `return(value)` statements in a definition must agree on the same type.
- `return()` is only valid for `void`.

### Inference Rules
- `auto` is allowed on bindings, parameters, and return transforms.
- Signature `auto` introduces implicit template parameters; template arguments are inferred per call site from
  parameter and return constraints.
- Binding `auto` is inferred from the initializer within the definition.
- Unresolved or conflicting `auto` is a diagnostic.

### Traits and Constraints
- Traits are explicit requirements over named functions (not operators).
- Trait constraints are explicit in signatures; no inference of trait bounds.
- A type satisfies a trait if all required functions resolve in the current program and backend profile.
- Minimal built-in traits:
  - `Additive<T>` requires `plus(T, T) -> T`.
  - `Multiplicative<T>` requires `multiply(T, T) -> T`.
  - `Comparable<T>` requires `equal(T, T) -> bool` and `less_than(T, T) -> bool`.
  - `Indexable<T, Elem>` requires `count(T) -> i32` and `at(T, i32) -> Elem`.

### Parametric Types
- Type parameters are explicit in signatures or implicit via `auto` in signatures.
- Type arguments must be fully concrete before lowering.
- Template argument inference is limited to local call sites.

### Algebraic Data Types (v1.1 target)
- `enum` introduces tagged union types.
- Enum syntax follows the standard envelope rules; the `enum` transform rewrites the declaration to an ordinary struct
  plus static bindings so the base language never needs a dedicated enum form.
- Enums use an integer backing type. The default is `i32`; `enum<i64>` and `enum<u64>` select other widths.
- Entries may specify explicit integer literal values (via `Name = 5i32` or `assign(Name, 5i32)`), and omitted values
  auto-increment from `0` or the previous entry. Values must fit in the backing type; unsigned enums reject negatives.
- Pattern matching must be exhaustive and type-checked.
- `match` expressions have a single result type.

Enum example (surface):
```
[enum]
Colors {
  Blue = 5
  Red
  Green
}
```

Enum example (desugared shape):
```
[struct]
Colors() {
  [i32] value{0i32}

  [public static Colors] Blue{Colors(5i32)}
  [public static Colors] Red{Colors(6i32)}
  [public static Colors] Green{Colors(7i32)}
}
```

### Ownership and Mutability
- `mut` marks writeable bindings.
- `move(x)` consumes a binding and forbids use until reinitialized.
- References cannot be moved.
- **Borrow rules (safe scope):**
  - A `Reference<T>` is a borrow of a storage location.
  - Many immutable borrows are allowed, or one mutable borrow; they may not overlap.
  - Borrows end at last use (non-lexical lifetimes), not necessarily at block end.
  - Borrowing a field borrows the whole struct value (no field-splitting in v1).
  - Borrowed bindings cannot be reassigned or moved until all borrows end.
  - `Reference<T>` values cannot escape the lifetime of their source; returning a reference is only allowed when it is exactly a parameter reference (no locals/derived references).
- **Unsafe scopes:** `[unsafe]` on a definition allows aliasing and pointer-to-reference conversions within that body, but references created there must not escape the unsafe scope. Unsafe scopes are aliasing barriers for optimization.
- **Unsafe calls:** unsafe definitions may be called from safe code; the call does not taint the caller as long as unsafe-created references do not escape.

### Layout and Struct Semantics
- Structs record layout manifests in IR.
- Recursive-by-value struct layouts are rejected.
- `[pod]` asserts trivially-copyable semantics and is validated.

### Diagnostics
- Diagnostics include expected type, actual type, source span, and backend profile.
- Diagnostic messages must be stable for snapshot testing.

### Conformance Tests (required)
- **Positive typing:** literals, calls, struct construction, pointers/references.
- **Negative typing:** return mismatch, invalid assign, invalid convert, invalid pointer target.
- **Inference:** binding inference, implicit-template inference, unresolved `auto`.
- **Traits:** satisfied, missing requirement, incorrect signature.

### Type Semantics (draft)
- **Nested generics:** template arguments may themselves be generic envelopes (`map<i32, array<i32>>`), and the parser preserves the nested envelope string for later lowering.
- **Field visibility:** stack-value declarations accept `[public]` or `[private]` transforms (default: public); they are mutually exclusive. The compiler records `visibility` metadata per field so tooling and backends enforce access rules consistently. `[public]` emits accessors in the generated namespace surface.
- **Static members:** add `[static]` to hoist storage to namespace scope while reusing the field’s visibility transform. Static fields still participate in the struct manifest so documentation and reflection stay aligned, but only one storage slot exists per struct definition.
- **Example:**
  ```
  import /std/math/*
  namespace demo {
    [struct]
    brush_settings() {
      [f32] size{12.0f32}
      [private f32] jitter{0.1f32}
      [private static handle<Texture>] palette{load_default_palette()}

      [public]
      Create() {
        assign(this.size, clamp(this.size, 1.0f32, 64.0f32))
      }
    }
  }
  ```
- **Constructor semantics:** struct constructors use field initializers as defaults; `Create`/`Destroy` remain optional hooks. Constant member behavior follows the normal `mut` rules (immutable unless declared `mut`).
  - **Zero-arg constructor exists when:** either (a) every field has an initializer, or (b) a `Create()` helper exists and initializes every field (including `uninitialized<T>` fields via `init`).
  - **Execution order:** field initializers run first, then `Create()` runs (if present) and may override field values.
  - **Effect interaction:** a zero-arg constructor is outside-effect-free only when both `Create()` (if present) and all field initializers are effect-free under the “no outside effects” rules.

## Move/Clone/Destroy
- **Lifecycle set:** structured types can define `Create`, `Move`, `Clone`, and `Destroy` helpers. `Create`/`Destroy` are optional hooks; `Move`/`Clone` must be nested inside the struct, return `void`, and accept exactly one parameter.
- **Clone signature:** the canonical clone constructor is `Clone([Reference<Self>] other) { ... }`. A shorthand `Clone(other) { ... }` desugars to the reference form.
- **Move-by-default:** assignments, argument passing, and returns consume values unless the type is `Copy` or the value is a `Reference<T>`.
- **`Copy` types (Rust-aligned):** values that can be duplicated by a simple bitwise copy with no custom destruction.
  - Built-in `Copy` types: `bool`, `i32`, `i64`, `u64`, `f32`, `f64`, `Pointer<T>`, `Reference<T>`.
  - Structs are `Copy` when they are `[pod]`, all fields are `Copy`, and they do **not** define `Destroy` or `Clone`.
  - Types with `handle`/`gpu_lane` fields are not `Copy` (they cannot be `[pod]`).
- **Explicit clone:** `clone(value)` invokes the `Clone` helper and returns a new value. It is a compile-time error if the type does not define `Clone`.
- **Move signature:** the canonical move constructor is `Move([Reference<Self>] other) { ... }`. A shorthand `Move(other) { ... }` desugars to the reference form.
- **Explicit move:** `move(value)` is the explicit consume helper. It requires a local binding or parameter name (no arbitrary expressions) and marks the source binding as moved-from while returning its value.
- **Use-after-move:** any use of a moved-from binding is a compile error until it is re-initialized (e.g., `assign(value, ...)`). The analysis is conservative across control flow.
- **Pointer behavior:** pointers can be moved; moved-from pointers are treated as invalid without being auto-zeroed (no implicit `null` literal; `0x0` is just a numeric value).
- **References:** `move(...)` rejects `Reference<T>` bindings; references do not participate in move semantics.
- **Backend note:** `move(...)` is a semantic ownership marker. VM/native lower it as a passthrough; the C++ emitter emits `std::move`.

## Uninitialized Storage (draft)
- **Purpose:** model explicit, inline uninitialized storage without implicit construction (C-style tagged storage and optional values).
- **Envelope:** `uninitialized<T>` allocates space for `T` but does not construct a `T` value.
- **Allowed locations:** local bindings and struct fields only. `uninitialized<T>` is rejected for parameters, return types,
  collection elements, pointer/reference targets, or template arguments to user-defined types.
- **Initialization:** `init(storage, value)` constructs `T` in-place.
  - `storage` must be `uninitialized<T>` and must be definitely uninitialized at the call site.
  - `value` is consumed (move-by-default); use `clone(value)` to duplicate.
  - After `init`, the storage is definitely initialized.
- **Destruction:** `drop(storage)` destroys the in-place value and marks the storage uninitialized.
  - `storage` must be definitely initialized; otherwise a diagnostic is emitted.
- **Access:**
  - `take(storage)` moves the value out and leaves the storage uninitialized.
  - `borrow(storage)` returns `Reference<T>` and participates in normal borrow rules; the storage must be initialized.
  - Any other use of an `uninitialized<T>` value is a type error.
- **Lifetime rules:** using a storage value after `drop`/`take` is a compile-time error until it is reinitialized.
- **`Destroy` handling:** structs owning `uninitialized<T>` fields must explicitly `drop` them when initialized.

## Optional Values (Maybe) (draft)
- **Purpose:** represent either "no value" or a value of `T` without heap allocation.
- **Naming:** `Maybe<T>` is the canonical optional type in PrimeStruct; there is no separate `Option<T>`.
- **Concrete representation:** a boolean tag plus uninitialized storage for `T`.
- **Required primitives:** `uninitialized<T>` storage, `init(storage, value)` to construct in-place, and `drop(storage)` to destroy.
- **Ergonomic constructor surface:** `Maybe()` yields empty; `Maybe(value)` yields present.
- **Example shape:**
  ```
  [struct]
  Maybe<T>() {
    [bool] empty{true}
    [uninitialized<T>] value{uninitialized<T>()}

    [public]
    Create() { }

    [public]
    Create([T] v) {
      init(this.value, v) // consumes v
      assign(this.empty, false)
    }

    [public]
    Destroy() {
      if(not(this.empty)) {
        drop(this.value)
      }
    }
  }
  ```

Draft tests (shape only):
```
// Positive: create empty, create filled, drop only when filled.
[return<i32>]
maybe_basic() {
  [Maybe<i32>] a{Maybe<i32>()}
  [Maybe<i32>] b{Maybe<i32>(1i32)}
  if(not(b.empty)) { drop(b.value) }
  return(0i32)
}

// Negative: double init.
[return<void>]
bad_double_init() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  init(storage, 1i32)
  init(storage, 2i32) // error: storage already initialized
}

// Negative: drop before init.
[return<void>]
bad_drop_uninit() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  drop(storage) // error: storage not initialized
}

// Negative: use after take.
[return<void>]
bad_use_after_take() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  init(storage, 1i32)
  [i32] v{take(storage)}
  drop(storage) // error: storage not initialized
}
```

## Lambdas & Higher-Order Functions
- **Syntax mirrors definitions:** lambdas omit the identifier (`[captures] <T>(params){ body }`). The capture list is required but may be empty (`[]`). Template arguments are optional.
- **Capture semantics:** supported forms are `[]`, `[=]`, `[&]`, and explicit entries (`[name]`, `[value name]`, `[ref name]`). Entries are comma/semicolon separated. Explicit names must resolve to parameters or locals in the enclosing scope; duplicates are diagnostics. `=` or `&` capture-all tokens cannot be repeated. `ref` marks a by-reference capture; otherwise captures are by value.
- **Parameters:** lambda parameters must use binding syntax. Defaults are allowed but must be literal/pure expressions and may not use named arguments.
- **Backend support (current):** lambdas are supported only by the C++ emitter, which lowers them to native C++ lambdas with the same capture list. IR/VM/GLSL backends reject lambdas; use named definitions when targeting those backends.
- **Inlining transforms:** standard transforms may inline pure lambdas in C++ emission paths; non-pure lambdas remain as closures.

## Literals & Composite Construction
- **Numeric literals:** decimal, float, hexadecimal with optional width suffixes (`42i64`, `42u64`, `1.0f64`).
- Integer literals require explicit width suffixes (`42i32`, `42i64`, `42u64`) unless `implicit-i32` is enabled (on by default). Omit it from `--text-transforms` (or use `--no-text-transforms`) to require explicit suffixes.
- Float literals accept `f32` or `f64` suffixes; when omitted in surface syntax they default to `f32`. Canonical form requires `f32`/`f64`. Exponent notation (`1e-3`, `1.0e6f32`) is supported.
- Commas may appear between digits in the integer part as digit separators and are ignored for value (e.g., `1,000i32`, `12,345.0f32`). Commas are not allowed in the fractional or exponent parts, and `.` is the only decimal separator.
- **Strings:** double-quoted strings process escapes unless a raw suffix is used; single-quoted strings are raw and do not process escapes. `raw_utf8` / `raw_ascii` force raw mode on either quote style. Surface literals accept `utf8`/`ascii`/`raw_utf8`/`raw_ascii` suffixes; the canonical/bottom-level form uses double-quoted strings with normalized escapes and an explicit `utf8`/`ascii` suffix. `implicit-utf8` (enabled by default) appends `utf8` when omitted.
- **Boolean:** keywords `true`, `false` map to backend equivalents.
- **Composite constructors:** structured values are introduced through standard envelope executions (`ColorGrade([hue_shift] 0.1f32 [exposure] 0.95f32)`) or brace constructor forms (`ColorGrade{ ... }`) in value positions; brace constructors evaluate the block and pass its resulting value to the constructor. If the block executes `return(value)`, that value is used; otherwise the last item is used. Binding initializers are value blocks; to pass multiple constructor arguments use an explicit call (e.g., `[ColorGrade] grade{ ColorGrade([hue_shift] 0.1f32 [exposure] 0.95f32) }`). Labeled arguments map to fields, and every field must have either an explicit argument or an envelope-provided default before validation. Labeled arguments may only be used on user-defined calls.
  - **Defaults & validation:** struct constructors accept positional and labeled arguments. Missing fields are filled from their field initializers (struct fields always declare an initializer). Extra arguments are a semantic error (`argument count mismatch`).
  - **Multi-expression blocks:** `{ ... }` is a value block, not an argument list. If you need to pass multiple constructor arguments, use `Type(arg1, arg2)` (or labeled arguments). A brace block with multiple expressions still produces a single value via `return(value)` or the final expression.
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
- **Conversions:** no implicit coercions. Use explicit executions (`convert<f32>(value)` or `bool{value}`) or custom transforms. The builtin `convert<T>(value)` is the default cast helper and supports `i32/i64/u64/bool/f32/f64` in the minimal native subset (integer width conversions currently lower as no-ops in the VM/native backends, while the C++ emitter uses `static_cast`; `convert<bool>` compares against zero, so any non-zero value—including negative integers—yields `true`). Float ↔ integer conversions lower to dedicated PSIR opcodes in VM/native, and converting NaN/Inf to an integer is a runtime error (stderr + exit code `3`).
  - **Convert constructor resolution (v1):**
    - Builtin fast-path: when `T` is `bool/i32/i64/u64/f32/f64` and `value` is numeric/bool, use the builtin conversion rules; user-defined conversions do not override this.
    - Otherwise, resolve `convert<T>(value)` as a call to `T.Convert(value)` in the struct method namespace (`/T/Convert`).
    - Signature must match exactly after monomorphisation: `[return<T>] Convert([U] value)`. No implicit conversions are applied to match the parameter type.
    - If no match exists, emit a `no conversion found` diagnostic. If multiple matches exist, emit an `ambiguous conversion` diagnostic with the candidate list.
    - Visibility follows import rules: only `[public]` `Convert` helpers are visible across imports.
    - `convert<T>(value)` does not fall back to `T(value)` or `T{...}`.
- **Float note:** VM/native lowering supports float literals, bindings, arithmetic, comparisons, numeric conversions, and `/std/math/*` helpers.
- **String note:** VM/native lowering supports string literals and string bindings in `print*`, plus `count`/indexing (`at`/`at_unsafe`) on string literals and string bindings that originate from literals; other string operations still require the C++ emitter for now.
  - **Struct note:** VM/native lowering supports struct values when fields are numeric/bool or other struct values (nested structs). Struct fields with strings or templated envelopes still require the C++ emitter.
  - `bool{...}` is valid for integer operands (including `u64`) and treats any non-zero value as `true`.
- **Mutability:** values immutable by default; include `mut` in the stack-value execution to opt-in (`[f32 mut] value{...}`).
- **Open design:** raw string semantics across backends.

## Pointers & References
- **Explicit envelopes:** `Pointer<T>`, `Reference<T>` mirror C++ semantics; no implicit conversions.
- **Qualifiers:** `restrict<T>` is allowed on bindings and parameters only; it must match the binding type (including template args) and acts as an explicit type constraint. There is no `readonly` qualifier yet; use `mut` to opt into mutation.
- **Target whitelist:** `Pointer<T>` targets must be primitive or struct types. `Reference<T>` targets may be primitive, struct, or `array<T>` (array references are allowed). `Pointer<array<T>>` and other template types are rejected by the front-end.
- **Backend limits:** VM/native lowering rejects `Pointer<string>` / `Reference<string>` (string pointers are not supported). Entry-argument arrays are not addressable (`location(args)` is invalid).
- **Surface syntax:** canonical syntax uses explicit calls (`location`, `dereference`, `plus`/`minus`); the `operators` text transform rewrites `&name`/`*name` sugar into those calls.
- **Reference binding:** `Reference<T>` bindings are initialized from `location(...)` and behave like `*Pointer<T>` in use. Use `mut` on the reference binding to allow `assign(ref, value)`.
- **Array references:** `Reference<array<T>>` is allowed; treat the reference like an array value for `count`/`at` and other array operations while still using `location(...)` to form the reference.
- **Core pointer calls:** `location(value)` yields a pointer to a local binding or parameter (entry argument arrays are not addressable); `location(ref)` returns the pointer stored by a `Reference<T>` binding; `dereference(ptr)` reads through a pointer/reference form; `assign(dereference(ptr), value)` writes through the pointer. Pointer writes require the pointer binding to be declared `mut`; attempting to assign through an immutable pointer or reference is rejected.
- **Pointer arithmetic:** `plus(ptr, offset)` and `minus(ptr, offset)` treat `offset` as a byte offset. VM/native frames currently space locals in 16-byte slots, so adding or subtracting `16` advances one local slot. Offsets accept `i32`, `i64`, or `u64` in the front-end; non-integer offsets are rejected, and the native backend lowers all three widths.
  - Pointer + pointer is rejected; only pointer ± integer offsets are allowed.
  - Offsets are interpreted as unsigned byte counts at runtime; negative offsets require signed operands (e.g., `-16i64`).
  - Example: `plus(location(second), -16i64)` steps back one 16-byte slot.
  - Example: `minus(location(second), 16u64)` also steps back one 16-byte slot.
  - You can use `location(ref)` inside pointer arithmetic when starting from a `Reference<T>` binding.
  - The C++ emitter performs raw pointer arithmetic on actual addresses; offsets are only well-defined within the same allocated object.
  - Minimal runnable example:
    ```
    [return<i32>]
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
    [return<i32>]
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
- **Ownership:** references are non-owning, frame-bound views. Pointer ownership tags (`raw`, `unique`, `shared`) are reserved for future allocator integrations.
- **Raw memory:** `memory::load/store` primitives expose byte-level access; opt-in to highlight unsafe operations.
- **Layout control:** attributes like `[packed]` guarantee interop-friendly layouts for C++/GLSL.
- **Open design:** pointer qualifier syntax, aliasing rules (restrict/readonly), and GPU backend constraints remain TBD.

## VM Design
- **Instruction set:** stack-based ops covering control flow, stack manipulation, memory/pointer access, IO, and explicit conversions. No implicit conversions; opcodes mirror the canonical language surface.
- **PSIR opcode set (v15, VM/native):** `PushI32`, `PushI64`, `PushF32`, `PushF64`, `PushArgc`, `LoadLocal`, `StoreLocal`,
  `AddressOfLocal`, `LoadIndirect`, `StoreIndirect`, `Dup`, `Pop`, `AddI32`, `SubI32`, `MulI32`, `DivI32`, `NegI32`,
  `AddI64`, `SubI64`, `MulI64`, `DivI64`, `DivU64`, `NegI64`, `AddF32`, `SubF32`, `MulF32`, `DivF32`, `NegF32`,
  `AddF64`, `SubF64`, `MulF64`, `DivF64`, `NegF64`, `CmpEqI32`, `CmpNeI32`, `CmpLtI32`, `CmpLeI32`, `CmpGtI32`,
  `CmpGeI32`, `CmpEqI64`, `CmpNeI64`, `CmpLtI64`, `CmpLeI64`, `CmpGtI64`, `CmpGeI64`, `CmpLtU64`, `CmpLeU64`,
  `CmpGtU64`, `CmpGeU64`, `CmpEqF32`, `CmpNeF32`, `CmpLtF32`, `CmpLeF32`, `CmpGtF32`, `CmpGeF32`, `CmpEqF64`,
  `CmpNeF64`, `CmpLtF64`, `CmpLeF64`, `CmpGtF64`, `CmpGeF64`, `ConvertI32ToF32`, `ConvertI32ToF64`,
  `ConvertI64ToF32`, `ConvertI64ToF64`, `ConvertU64ToF32`, `ConvertU64ToF64`, `ConvertF32ToI32`, `ConvertF32ToI64`,
  `ConvertF32ToU64`, `ConvertF64ToI32`, `ConvertF64ToI64`, `ConvertF64ToU64`, `ConvertF32ToF64`, `ConvertF64ToF32`,
  `JumpIfZero`, `Jump`, `ReturnVoid`, `ReturnI32`, `ReturnI64`, `ReturnF32`, `ReturnF64`, `PrintI32`, `PrintI64`,
  `PrintU64`, `PrintString`, `PrintArgv`, `PrintArgvUnsafe`, `LoadStringByte`, `FileOpenRead`, `FileOpenWrite`,
  `FileOpenAppend`, `FileClose`, `FileFlush`, `FileWriteI32`, `FileWriteI64`, `FileWriteU64`, `FileWriteString`,
  `FileWriteByte`, `FileWriteNewline`.
- **GLSL note:** GLSL emission bypasses PSIR and lowers from the canonical AST directly; PSIR opcode validation only applies to VM/native consumers.
- **PSIR versioning:** current portable IR is PSIR v15 (adds execution metadata on top of v14’s float return opcodes, v13’s float arithmetic/compare/convert opcodes, and v12’s struct field visibility/static metadata, `LoadStringByte`, `PrintArgvUnsafe`, `PrintArgv`, `PushArgc`, pointer helpers, `ReturnVoid`, and print opcode upgrades).
- **Frames & stack:** a single entry frame stores locals in 16-byte slots; the operand stack stores raw `u64` values interpreted by opcode (ints, floats as bits, and indices). Indirect addresses are byte offsets into the local slot space and must be 16-byte aligned.
- **Module layout:** `IrModule` bundles functions, string table, and struct layouts; the VM executes only the `entryIndex` function because user-defined calls are inlined during lowering (recursion is rejected).
- **Strings & IO:** string values are indices into the module string table; `PrintString`/`LoadStringByte` read from it. File operations use OS descriptors stored as `i64` values and must be explicitly closed or they close on scope end via lowering.
- **Memory/GC:** there is no heap or GC in the VM today. Arrays/vectors are inline locals with fixed-capacity headers (count/capacity) plus element slots; native output mirrors this with stack storage and OS handles. No reference counting is performed.
- **Errors:** guard rails emit errors by printing to stderr and returning error codes (e.g., bounds checks), while VM runtime faults (stack underflow, invalid addresses) surface as `VM error:` with exit code 3.
- **Deployment target:** the VM serves as the sandboxed runtime for user-supplied scripts (e.g., on iOS) where native code generation is unavailable. Effect masks and capabilities enforce per-platform restrictions.

## Examples (sketch)
```
// Hello world (native/VM-friendly)
[return<i32> effects(io_out)]
main() {
  print_line("Hello, world!"utf8)
  return(0i32)
}

// Pull std::io at version 1.2.0
import<"/std/io", version="1.2.0">

import /std/math/*

[operators return<i32>] add<i32>(a, b) { return(plus(a, b)) }

[operators] execute_add<i32>(x, y)

clamp_exposure(img) {
  if(
    greater_than(img.exposure, 1.0f32),
    then() { print_line_error("exposure too high"utf8) },
    else() { }
  )
}

tweak_color([copy mut restrict<Image>] img) {
  assign(img.exposure, clamp(img.exposure, 0.0f32, 1.0f32))
  assign(img.gamma, plus(img.gamma, 0.1f32))
  apply_grade(img)
}

restrict_demo() {
  [restrict<array<i32>>] ok{array<i32>(1i32, 2i32)}
  [restrict<array<i32>>] bad{array<u64>(1u64, 2u64)} // diagnostic: expected array<i32>
}

// Canonicalization example for parameters:
// Surface: f1([i32] a [f32] b) { ... }
// Canonical: f1([restrict<i32>] a [restrict<f32>] b) { ... }

[return<i32>]
convert_demo() {
  return(i32{1.5f32})
}

[return<i32>]
labeled_args_demo() {
  return(sum3(1i32 [c] 3i32 [b] 2i32))
}

[operators return<f32>]
blend([f32] a, [f32] b) {
  [f32 mut] result{(a + b) * 0.5f32}
  if (result > 1.0f32) {
    result = 1.0f32
  } else {
  }
  return(result)
}

// Compute shader sketch (GPU backend)
[compute workgroup_size(64, 1, 1)]
/add_one(
  [Buffer<i32>] input,
  [Buffer<i32>] output,
  [i32] count
) {
  [i32] x{ /std/gpu/global_id_x() }
  if (x >= count) {
    return()
  } else {
  }
  [i32] value{ /std/gpu/buffer_load(input, x) }
  /std/gpu/buffer_store(output, x, plus(value, 1i32))
}

[effects(gpu_dispatch) return<i32>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32, 4i32)}
  [Buffer<i32>] input{ /std/gpu/upload(values) }
  [Buffer<i32>] output{ /std/gpu/buffer<i32>(4i32) }
  /std/gpu/dispatch(/add_one, 4i32, 1i32, 1i32, input, output, 4i32)
  [array<i32>] result{ /std/gpu/readback(output) }
  return(plus(plus(result[0i32], result[1i32]), plus(result[2i32], result[3i32])))
}

// Canonical, post-transform form
[return<f32>] blend([f32] a, [f32] b) {
  [f32 mut] result{multiply(plus(a, b), 0.5f32)}
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
    return i32{1.5f32}
  }
  def /labeled_args_demo(): i32 {
    return sum3(1, [c] 3, [b] 2)
  }
}
```

## Integration Points
- **Build system:** extend CMake/tooling to run `primec`, track dependency graphs, and support incremental builds.
- **Testing:** unit/looped regression suites verify backend parity (C++, VM, GLSL).
- **Diagnostics:** metrics/logs land under `diagnostics/PrimeStruct/*`. Effect annotations drive error messaging.

### Diagnostics & Tooling Roadmap
- **Phase 0 (now):** standardize diagnostic records (`severity`, `code`, `message`, `notes`, source span), include canonical call paths, and ensure CLI output is deterministic. Establish a stable JSON diagnostic export (`--emit-diagnostics`) for tooling/tests.
- **Phase 1 (source maps):** attach token spans to AST, keep span provenance through text/semantic transforms, and emit IR-to-source maps for VM/native/GLSL. Require every diagnostic to carry at least one source span.
- **Phase 2 (incremental):** content-addressed caches for AST/IR, dependency graph tracking for imports, and invalidation rules per transform phase. Add `--watch` to reuse caches and stream diagnostics.
- **Phase 3 (IDE/LSP):** go-to-definition, completion, and signature help using the same symbol tables as the compiler. Provide diagnostics in LSP format plus an editor adapter.
- **Phase 4 (runtime):** VM/native stack traces mapped via source maps, crash reports emitted with IR/AST hashes, and opt-in runtime tracing for effect/capability usage.

### Semantics Parallelism (Investigation)
We plan to parallelize semantic validation across root functions using a
deterministic diagnostics pipeline. See `docs/Semantics_Multithreaded_Pass.md`
for the proposed phases, refactors, and test plan.

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
- Runtime debugging or VM execution control (await Phase 4 tooling).
- GPU-specific editor features beyond diagnostics and symbol navigation.

## Dependencies & Related Work
- Stable IR definition & serialization (std::serialization once available).
- Scene graph/rendering plans (`docs/finished/Plan_SceneGraph_Renderer_Finished.md`).

## Risks & Research Tasks
- **Memory model** – value/reference semantics, handle lifetimes, POD vs GPU staging rules.
- **Resource bindings** – unify descriptors across CPU/GPU backends.
- **Debugging** – source maps, stack inspection across VM/GPU.
- **Performance** – ensure parity with handwritten code; benchmark harnesses.
- **Adoption** – migration strategy from existing shaders/scripts.
- **Diagnostics** – map compile/runtime errors back to PrimeStruct source across backends.
- **Concurrency** – finalise coroutine/await integrations.
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
5. Define library/versioning strategy so import resolution enforces compatibility.
6. Flesh out stack/class specifications (calling convention, class sugar transforms, dispatch strategy) across backends.
7. Lock down composite literal syntax across backends and add conformance tests.
8. Decide machine-code strategy (C++ emission, third-party JIT) and prototype.
9. Define diagnostics/tooling plan (source maps, error reporting pipeline, incremental tooling, editor roadmap).
10. Document staffing/time requirements before promoting PrimeStruct onto the active roadmap.
