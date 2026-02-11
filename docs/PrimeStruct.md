# PrimeStruct Plan

PrimeStruct is built around a simple idea: program meaning comes from two primitives—**definitions** (potential) and **executions** (actual). Both share the same syntactic envelope, and compile-time transforms rewrite the surface into a small canonical core. That core is what we target to C++, GLSL, and the PrimeStruct VM. It also keeps semantics deterministic and leaves room for future tooling (PathSpace integration, visual editors).

### Source-processing pipeline
1. **Include resolver:** first pass walks the raw text and expands every `include<...>` inline (C-style) so the compiler always works on a single flattened source stream.
2. **Textual metafunction filters:** the flattened stream flows through ordered token/text filters that execute `[transform ...]` rules declared to operate pre-AST (operator sugar, project-specific macros, etc.).
3. **AST builder:** once textual transforms finish, the parser builds the canonical AST.
4. **Template & semantic resolver:** monomorphise templates, resolve namespaces, and apply semantic transforms (borrow checks, effects) so the tree is fully typed.
5. **IR lowering:** emit the shared SSA-style IR only after templates/semantics are resolved, ensuring every backend consumes an identical canonical form.

Each stage halts on error (reporting diagnostics immediately) and exposes `--dump-stage=<name>` so tooling/tests can capture the text/tree output just before failure. Text filters are configured via `--text-filters=<list>`; the default list enables `operators`, `collections`, and `implicit-utf8` (auto-appends `utf8` to bare string literals). Add `implicit-i32` to auto-append `i32` suffixes. Use `--no-transforms` to disable all text filters and require canonical syntax.

## Phase 0 — Scope & Acceptance Gates (must precede implementation)
- **Charter:** capture exactly which language primitives, transforms, and effect rules belong in PrimeStruct, and list anything explicitly deferred to later phases.
- **Success criteria:** define measurable gates (parser coverage, IR validation, backend round-trips, sample programs)
- **Ownership map:** assign leads for parser, IR/type system, first backend, and test infrastructure, plus security/runtime reviewers.
- **Integration plan:** describe how the compiler/test suite slots into the build (targets, CI loops, feature flags, artifact publishing).
- **Risk log:** record open questions (borrow checker, capability taxonomy, GPU backend constraints) and mitigation/rollback strategies.
- **Exit:** only after this phase is reviewed/approved do parser/IR/backend implementations begin; the conformance suite derives from the frozen charter instead of chasing a moving target.

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

### Compiler driver behavior (plan)
- `primec --emit=cpp input.prime -o hello.cpp`
- `primec --emit=exe input.prime -o hello`
  - Uses the C++ emitter plus the host toolchain (initially `clang++`).
  - Bundles a minimal runtime shim that maps `main` to `int main()`.
- `primec --emit=native input.prime -o hello`
  - Emits a self-contained macOS/arm64 executable directly (no external linker).
  - Lowers through the portable IR that also feeds the VM/network path.
  - Current subset: fixed-width integer/bool literals (`i32`, `i64`, `u64`), locals + assign, basic arithmetic/comparisons (signed/unsigned 64-bit), boolean ops (`and`/`or`/`not`), `convert<int/i32/i64/u64/bool>`, abs/sign/min/max/clamp/saturate, `if`, `print`, `print_line`, `print_error`, and `print_line_error` for numeric/bool or string literals/bindings, and pointer/reference helpers (`location`, `dereference`, `Reference`) in a single entry definition.
- `primevm input.prime --entry /main -- <args>`
  - Runs the source via the PrimeStruct VM (equivalent to `primec --emit=vm`). `--entry` defaults to `/main` if omitted.
- Defaults: if `--emit` and `-o` are omitted, `primec input.prime` uses `--emit=native` and writes the output using the input filename stem (still under `--out-dir`).
- All generated outputs land in the current directory (configurable by `--out-dir`).

## Goals
- Single authoring language spanning gameplay/domain scripting, UI logic, automation, and rendering shaders.
- Emit high-performance C++ for engine integration, GLSL/SPIR-V for GPU shading, and bytecode for an embedded VM without diverging semantics.
- Share a consistent standard library (math, texture IO, resource bindings, PathSpace helpers) across backends while preserving determinism for replay/testing.

## Proposed Architecture
- **Front-end parser:** C/TypeScript-inspired surface syntax with explicit types, deterministic control flow, and borrow-checked resource usage.
- **Transform pipeline:** ordered `[transform]` functions rewrite the forthcoming AST (or raw tokens) before semantic analysis. The default chain desugars infix operators, control-flow, assignment, etc.; projects can override via `--transform-list` flags.
- **Transform pipeline:** ordered `[transform]` functions rewrite the forthcoming AST (or raw tokens) before semantic analysis. The compiler can auto-inject transforms per definition/execution (e.g., attach `operator_infix_default` to every function) with optional path filters (`/math/*`, recurse or not) so common rewrites don’t have to be annotated manually. Transforms may also rewrite a definition’s own transform list (for example, `single_type_to_return`). The default chain desugars infix operators, control-flow, assignment, etc.; projects can override via `--transform-list` flags.
- **Intermediate representation:** strongly-typed SSA-style IR shared by every backend (C++, GLSL, VM, future LLVM). Normalisation happens once; backends never see syntactic sugar.
- **IR definition (draft):**
  - **Module:** `{ string_table, functions, entry_index, version }`.
  - **Function:** `{ name, instructions }` where instructions are linear, stack-based ops with immediates.
  - **Instruction:** `{ op, imm }`; `op` is an `IrOpcode`, `imm` is a 64-bit immediate payload whose meaning depends on `op`.
  - **Locals:** addressed by index; `LoadLocal`, `StoreLocal`, `AddressOfLocal` operate on the index encoded in `imm`.
  - **Strings:** string literals are interned in `string_table` and referenced by index in print ops (see PSIR versioning).
  - **Entry:** `entry_index` points to the entry function in `functions`; its signature is enforced by the front-end.
- **PSIR versioning:** serialized IR includes a version tag; v2 introduces `AddressOfLocal`, `LoadIndirect`, and `StoreIndirect` for pointer/reference lowering; v4 adds `ReturnVoid` to model implicit void returns in the VM/native backends; v5 adds a string table + print opcodes for stdout/stderr output; v6 extends print opcodes with newline/stdout/stderr flags to support `print`/`print_line`/`print_error`/`print_line_error`; v7 adds `PushArgc` for entry argument counts in VM/native execution; v8 adds `PrintArgv` for printing entry argument strings; v9 adds `PrintArgvUnsafe` to emit unchecked entry-arg prints for `at_unsafe`.
  - **PSIR v2:** adds pointer opcodes (`AddressOfLocal`, `LoadIndirect`, `StoreIndirect`) to support `location`/`dereference`.
  - **PSIR v4:** adds `ReturnVoid` so void definitions can omit explicit returns without losing a bytecode terminator.
- **Backends:**
  - **C++ emitter** – generates host code or LLVM IR for native binaries/JITs.
  - **GLSL/SPIR-V emitter** – produces shader code; a Metal translation remains future work.
  - **VM bytecode** – compact instruction set executed by the embedded interpreter/JIT.
- **Tooling:** CLI compiler `primec`, plus the VM runner `primevm` and build/test helpers. The compiler accepts `--entry /path` to select the entry definition (default: `/main`). The VM/native subset now accepts a single `[array<string>]` entry parameter for command-line arguments; `args.count()` and `count(args)` are supported, `print*` calls accept `args[index]` (checked) or `at_unsafe(args, index)` (unchecked), and string bindings may be initialised from `args[index]` or `at_unsafe(args, index)` (print-only), while the C++ emitter supports full array/string operations. The definition/execution split maps cleanly to future node-based editors; full IDE/LSP integration is deferred until the compiler stabilises.
- **AST/IR dumps:** the debug printers include executions with their argument lists and body forms so tooling can capture scheduling intent in snapshots.
  - Dumps show collection literals after text-filter rewriting (e.g., `array<i32>{1i32,2i32}` becomes `array<i32>(1, 2)`).
  - Labeled execution arguments and body calls appear inline (e.g., `exec /execute_repeat([count] 2) { main(), main() }`).

## Language Design Highlights
- **Identifiers:** `[A-Za-z_][A-Za-z0-9_]*` plus the slash-prefixed form `/segment/segment/...` for fully-qualified paths. Unicode may arrive later, but identifiers are constrained to ASCII for predictable tooling and hashing. `mut`, `return`, `include`, `import`, `namespace`, `true`, and `false` are reserved keywords; any other identifier (including slash paths) can serve as a transform, path segment, parameter, or binding.
- **String literals:** surface forms accept `"..."utf8`, `"..."ascii`, or raw `"..."raw_utf8` / `"..."raw_ascii` (no escape processing in raw bodies). The `implicit-utf8` text filter (enabled by default) appends `utf8` when omitted in surface syntax. **Canonical/bottom-level form always uses `raw_utf8` or `raw_ascii` after escape decoding.** `ascii`/`raw_ascii` enforce 7-bit ASCII (the compiler rejects non-ASCII bytes). Example surface: `"hello"utf8`, `"moo"ascii`. Example canonical: `"hello"raw_utf8`. Raw example: `"C:\\temp"raw_ascii` keeps backslashes verbatim.
- **Numeric types (draft):** fixed-width `i32`, `i64`, `u64`, `f32`, and `f64` map directly to hardware instructions and are the only numeric types accepted by GPU backends. `integer` is an arbitrary-precision signed integer with exact arithmetic (no overflow/wrapping). `decimal` is an arbitrary-precision floating type with fixed 256-bit precision and deterministic round-to-nearest-even semantics. `complex` is a pair of `decimal` values (`real`, `imag`) using the same 256-bit precision and rounding rules. `integer`/`decimal`/`complex` are software-only and rejected by GPU backends; the current VM/native subset excludes them. Mixed ops between `integer`/`decimal`/`complex` and fixed-width types are rejected; use `convert<T>(value)` explicitly, and conversions that would overflow or lose information fail with diagnostics. When inference cannot select `integer`/`decimal`/`complex`, the existing `implicit-i32` rule still applies.
  - Float literals accept standard decimal forms, including optional fractional digits (e.g., `1.`, `1.0`, `1.f32`, `1.e2`).
- **Envelope:** definitions/executions use `[transform-list] identifier<template-list>(parameter-list) {body-list}`; bindings use the form `[Type qualifiers…] name{initializer}` and may omit the type transform when the initializer lets the compiler infer it (defaulting to `int` if inference fails). Lists recursively reuse whitespace-separated tokens.
  - Syntax markers: `[]` compile-time transforms, `<>` templates, `()` runtime parameters/calls, `{}` runtime code (definition bodies, execution bodies, binding initializers).
  - `[...]` enumerates metafunction transforms applied in order (see “Built-in transforms”).
  - `<...>` supplies compile-time types/templates—primarily for transforms or when inference must be overridden.
  - `(...)` lists runtime parameters; calls always include `()` (even with no args), and `()` never appears on bindings.
  - **Parameters:** use the same binding envelope as locals: `main([array<string>] args, [i32] limit{10i32})`. Qualifiers like `mut`/`copy` apply here as well; defaults are optional and currently limited to literal/pure forms (no name references).
  - `{...}` holds runtime code for definition/execution bodies, and constructor arguments for binding initializers. Binding initializers are whitespace-separated argument forms (optionally labeled with `[param]`) that desugar to a type constructor call; use helpers for multi-step setup.
  - Bindings are only valid inside definition bodies or parameter lists; top-level bindings are rejected.
- **Definitions vs executions:** definitions include a body (`{…}`) and optional transforms; executions are call-style (`execute_task<…>(args)`), optionally followed by a brace list of body arguments for higher-order scheduling (`execute_task(...) { work(), work() }`). Execution bodies may only contain calls (no bindings, no `return`), and they are treated as scheduling payloads rather than executable function bodies. The compiler decides whether to emit callable artifacts or schedule work based on that presence. Calls always use `()`; the `name{...}` form is reserved for bindings so `execute_task{...}` is invalid.
  - Executions accept the same argument syntax as calls, including labeled arguments (`[param] value`).
  - Nested calls inside execution arguments still follow builtin rules (e.g., `array<i32>([first] 1i32)` is rejected).
  - Example: `execute_task([items] array<i32>(1i32, 2i32) [pairs] map<i32, i32>(1i32, 2i32))`.
  - Note: the current C++ emitter only generates code for definitions; executions are parsed/validated but not emitted.
  - Execution bodies are parsed as brace-delimited argument lists (e.g., `execute_repeat(2i32) { main(), main() }`).
- **Return annotation:** definitions declare return types via transforms (e.g., `[return<float>] blend<…>(…) { … }`). Executions return values explicitly (`return(value)`); the desugared form is always canonical. An optional `single_type_to_return` transform may rewrite a single bare type in the transform list into `return<type>` (e.g., `[int] main()` → `[return<int>] main()`), but it only runs when explicitly enabled via `--transform-list` or a per-definition transform.
- **Effects:** functions are pure by default. Authors opt into side effects with attributes such as `[effects(global_write, io_out)]`. Standard library routines permit stdout/stderr logging via `io_out`/`io_err`; backends reject unsupported effects (e.g., GPU code requesting filesystem access). `primec --default-effects <list>` supplies a default effect set for definitions/executions that omit `[effects]` (comma-separated list; `default` and `none` are supported tokens). If `[capabilities(...)]` is present it must be a subset of the active effects (explicit or default).
- **Paths & includes:** every definition/execution lives at a canonical path (`/ui/widgets/log_button_press`). Authors can spell the path inline or rely on `namespace foo { ... }` blocks to prepend `/foo` automatically; includes simply splice text, so they inherit whatever path context is active. Include paths are parsed before text filters, so they remain quoted without literal suffixes. `include<"/std/io", version="1.2.0">` searches the include path for a zipped archive or plain directory whose layout mirrors `/version/first_namespace/second_namespace/...`. The angle-bracket list may contain multiple quoted string paths—`include<"/std/io", "./local/io/helpers", version="1.2.0">`—and the resolver applies the same version selector to each path; mismatched archives raise an error before expansion. Versions live in the leading segment (e.g., `1.2/std/io/*.prime` or `1/std/io/*.prime`). If the version attribute provides one or two numbers (`1` or `1.2`), the newest matching archive is selected; three-part versions (`1.2.0`) require an exact match. Each `.prime` source file is inline-expanded exactly once and registered under its namespace/path (e.g., `/std/io`); duplicate includes are ignored. Folders prefixed with `_` remain private. `import /foo` is a lightweight alias that brings the immediate children of `/foo` into the root namespace; it does not inline source (use `include` for that). Imports are resolved after includes and can be listed as `import /math, /util`.
- **Transform-driven control flow:** control structures desugar into prefix calls (`if(condition, trueBranch, falseBranch)`). A surface form like `if(condition) { … } else { … }` is accepted and rewritten into the canonical call form by wrapping the two blocks as envelopes. Infix operators (`a + b`) become canonical calls (`plus(a, b)`), ensuring IR/backends see a small, predictable surface.
- **Mutability:** bindings are immutable by default. Opt into mutation by placing `mut` inside the stack-value execution or helper (`[Integer mut] exposure{42}`, `[mut] Create()`). Transforms enforce that only mutable bindings can serve as `assign` or pointer-write targets.

### Example function syntax
```
import /math
namespace demo {
  [return<void> effects(io_out)]
  hello_values() {
    [string] message{"Hello PrimeStruct"utf8}
    [i32] iterations{3i32}
    [float mut] exposure{1.25f}
    [float3] tone_curve{float3(0.8f, 0.9f, 1.1f)}

    print_line(message)
    assign(exposure, clamp(exposure, 0.0f, 2.0f))
    execute_repeat(iterations) {
      apply_curve(tone_curve, exposure)
    }
  }
}
```
Statements are separated by newlines; semicolons never appear in PrimeStruct source. PrimeStruct does not distinguish
between statements and envelopes—any envelope can stand alone as a statement, and unused values are discarded (for
example, `helper()` or `1i32` can appear as standalone statements).

### Slash paths & textual operator filters
- Slash-prefixed identifiers (`/pkg/module/thing`) are valid anywhere the Envelope expects a name; `namespace foo { ... }` is shorthand for prepending `/foo` to enclosed names, and namespaces may be reopened freely.
- Textual metafunction filters run before the AST exists. Operator filters (e.g., divide) scan the raw character stream: when a `/` is sandwiched between non-whitespace characters they rewrite the surrounding text (`/foo / /bar` → `divide(/foo, /bar)`), but when `/` begins a path segment (start of line or immediately after whitespace/delimiters) the filter leaves it untouched (`/foo/bar/lol()` stays intact). Other operators follow the same no-whitespace rule (`a>b` → `greater_than(a, b)`, `a<b` → `less_than(a, b)`, `a>=b` → `greater_equal(a, b)`, `a<=b` → `less_equal(a, b)`, `a==b` → `equal(a, b)`, `a!=b` → `not_equal(a, b)`, `a&&b` → `and(a, b)`, `a||b` → `or(a, b)`, `!a` → `not(a)`, `-a` → `negate(a)`, `a=b` → `assign(a, b)`).
- Because includes expand first, slash paths survive every filter untouched until the AST builder consumes them, and IR lowering never needs to reason about infix syntax.

### Struct & type categories (draft)
- **Struct tag as transform:** `[struct ...]` in the envelope is purely declarative. It records a layout manifest (field names, types, offsets) and validates the body, but the underlying syntax remains a standard definition. Struct-tagged definitions are field-only: no parameters or return transforms, and no return statements. Un-tagged definitions may still be instantiated as structs; they simply skip the extra validation/metadata until another transform demands it.
- **Placement policy (draft):** where a value lives (stack/heap/buffer) is decided by allocation helpers plus capabilities, not by struct tags. Types may express requirements (e.g., `pod`, `handle`, `gpu_lane`), but placement is a call-site decision gated by capabilities.
- **POD tag as validation:** `[pod]` asserts trivially-copyable semantics. Violations (hidden lifetimes, handles, async captures) raise diagnostics; without the tag the compiler treats the body permissively.
- **Member syntax:** every field is just a stack-value execution (`[float mut] exposure{1.0f}`, `[handle<PathNode>] target{get_default()}`). Attributes (`[mut]`, `[align_bytes(16)]`, `[handle<PathNode>]`) decorate the execution, and transforms record the metadata for layout consumers.
- **Method calls & indexing:** `value.method(args...)` desugars to `/type/method(value, args...)` in the method namespace (no hidden object model). For arrays, `value.count()` rewrites to `/array/count<T>(value)`; the helper `count(value)` simply forwards to `value.count()`. Indexing uses the safe helper by default: `value[index]` rewrites to `at(value, index)` with bounds checks; `at_unsafe(value, index)` skips checks.
- **Baseline layout rule:** members default to source-order packing. Backend-imposed padding is allowed only when the metadata (`layout.fields[].padding_kind`) records the reason; `[no_padding]` and `[platform_independent_padding]` fail the build if the backend cannot honor them bit-for-bit.
- **Alignment transforms:** `[align_bytes(n)]` (or `[align_kbytes(n)]`) may appear on the struct or field; violations again produce diagnostics instead of silent adjustments.
- **Stack value executions:** every local binding—including struct “fields”—materializes via `[Type qualifiers…] name{initializer}` so stack frames remain declarative (e.g., `[float mut] exposure{1.0f}`). Default initializers are mandatory to keep frames fully initialized.
- **Lifecycle helpers (Create/Destroy):** Within a struct-tagged definition, nested definitions named `Create` and `Destroy` gain constructor/destructor semantics. Placement-specific variants add suffixes (`CreateStack`, `DestroyHeap`, etc.). Without these helpers the field initializer list defines the default constructor/destructor semantics. `this` is implicitly available inside helpers. Add `mut` to the helper’s transform list when it writes to `this` (otherwise `this` stays immutable); omit it for pure helpers. Lifecycle helpers must return `void` and accept no parameters. We capitalise system-provided helper names so they stand out, but authors are free to use uppercase identifiers elsewhere—only the documented helper names receive special treatment.
  ```
  import /math
  namespace demo {
    [struct pod]
    color_grade() {
      [float mut] exposure{1.0f}

      [mut]
      Create() {
        assign(this.exposure, clamp(this.exposure, 0.0f, 2.0f))
      }

      [effects(io_out)]
      Destroy() {
        print_line("color grade destroyed"utf8)
      }
    }
  }
  ```
- **IR layout manifest:** `[struct]` extends the IR descriptor with `layout.total_size_bytes`, `layout.alignment_bytes`, and ordered `layout.fields`. Each field record stores `{ name, type, offset_bytes, size_bytes, padding_kind, category }`. Placement transforms consume this manifest verbatim, ensuring C++, VM, and GPU backends share one source of truth.
- **Categories:** `[pod]`, `[handle]`, `[gpu_lane]` tags classify members for borrow/resource rules. Handles remain opaque tokens with subsystem-managed lifetimes; GPU lanes require staging transforms before CPU inspection.
- **Category mapping (draft):**
  - `pod`: stored inline; treated as trivially-copyable for layout and borrow checks.
  - `handle`: stored as an opaque reference token; lifetime is managed by the owning subsystem.
  - `gpu_lane`: stored as a GPU-only handle; CPU access requires explicit staging transforms.
  - Conflicts are rejected (`pod` with `handle` or `gpu_lane`, and `handle` with `gpu_lane`).

### Built-in transforms (draft)
- **Purpose:** built-in transforms are metafunctions that stamp semantic flags on the AST; later passes (borrow checker, backend filters) consume those flags. They do not emit code directly.
- **Evaluation mode:** when the compiler sees `[transform ...]`, it routes through the metafunction's declared signature—pure token rewrites operate on the raw stream, while semantic transforms receive the AST node and in-place metadata writers.
- **`copy`:** force copy-on-entry for a parameter or binding, even when references are the default. Often paired with `mut`.
- **`mut`:** mark the local binding as writable; without it the binding behaves like a `const` reference. On definitions, `mut` is only valid on lifecycle helpers to make `this` mutable; executions do not accept `mut`.
- **`restrict<T>`:** constrain the accepted type to `T` (or satisfy concept-like predicates once defined). Applied alongside `copy`/`mut` when needed.
- **`return<T>`:** optional contract that pins the inferred return type. Recommended for public APIs or when disambiguation is required.
- **`single_type_to_return`:** optional transform that rewrites a single bare type in a transform list into `return<type>` (e.g., `[int] main()` → `[return<int>] main()`); disabled by default and only runs when enabled in the transform list or CLI.
- **`effects(...)`:** declare side-effect capabilities; absence implies purity. Backends reject unsupported capabilities.
- **Transform scope:** `effects(...)` and `capabilities(...)` are only valid on definitions/executions, not bindings.
- **`align_bytes(n)`, `align_kbytes(n)`:** encode alignment requirements for struct members and buffers. `align_kbytes` applies `n * 1024` bytes before emitting the metadata.
- **`capabilities(...)`:** reuse the transform plumbing to describe opt-in privileges without encoding backend-specific scheduling hints.
- **`struct`, `pod`, `handle`, `gpu_lane`:** declarative tags that emit metadata/validation only. They never change syntax; instead they fail compilation when the body violates the advertised contract (e.g., `[pod]` forbids handles/async fields).
- **`public`, `private`, `package`:** field visibility tags; mutually exclusive.
- **`static`:** field storage tag; hoists storage to namespace scope while keeping the field in the layout manifest.
- **`stack`, `heap`, `buffer`:** placement transforms reserved for future backends; currently rejected in validation.
- **Documentation TODO:** ship a full catalog of built-in transforms once the borrow checker and effect model solidify; this list captures the current baseline only.

### Core library surface (draft)
- **Standard math (draft):** the core math set lives under `/math/*` (e.g., `/math/sin`, `/math/pi`). `import /math` brings these names into the root namespace so `sin(...)`/`pi` resolve without qualification. Unsupported type/operation pairs produce diagnostics. Fixed-width integers (`i32`, `i64`, `u64`) and `integer` use exact arithmetic; `f32`/`f64` follow their IEEE-754 behavior; `decimal` and `complex` use the 256-bit `decimal` precision and round-to-nearest-even rules.
  - **Constants:** `/math/pi`, `/math/tau`, `/math/e`.
  - **Basic:** `/math/abs`, `/math/sign`, `/math/min`, `/math/max`, `/math/clamp`, `/math/lerp`, `/math/saturate`.
  - **Rounding:** `/math/floor`, `/math/ceil`, `/math/round`, `/math/trunc`, `/math/fract`.
  - **Power/log:** `/math/sqrt`, `/math/cbrt`, `/math/pow`, `/math/exp`, `/math/exp2`, `/math/log`, `/math/log2`, `/math/log10`.
  - **Trig:** `/math/sin`, `/math/cos`, `/math/tan`, `/math/asin`, `/math/acos`, `/math/atan`, `/math/atan2`, `/math/radians`, `/math/degrees`.
  - **Hyperbolic:** `/math/sinh`, `/math/cosh`, `/math/tanh`, `/math/asinh`, `/math/acosh`, `/math/atanh`.
  - **Float utils:** `/math/fma`, `/math/hypot`, `/math/copysign`.
  - **Predicates:** `/math/is_nan`, `/math/is_inf`, `/math/is_finite`.
- **`assign(target, value)`:** canonical mutation primitive; only valid when `target` carried `mut` at declaration time. The call evaluates to `value`, so it can be nested or returned.
- **`count(value)` / `value.count()` / `at(value, index)` / `at_unsafe(value, index)`:** collection helpers. `value.count()` is canonical; `count(value)` forwards to the method when available. `at` performs bounds checking; `at_unsafe` does not. In the VM/native backends, an out-of-bounds `at` aborts execution (prints a diagnostic to stderr and returns exit code `3`).
- **`print(value)` / `print_line(value)` / `print_error(value)` / `print_line_error(value)`:** stdout/stderr output primitives (statement-only). `print`/`print_line` require `io_out`, and `print_error`/`print_line_error` require `io_err`. VM/native backends support numeric/bool values plus string literals/bindings; other string operations still require the C++ emitter.
- **`plus`, `minus`, `multiply`, `divide`, `negate`:** arithmetic wrappers used after operator desugaring. Operands must be numeric (`i32`, `i64`, `u64`, `f32`, `f64`); bool/string/pointer operands are rejected. Mixed signed/unsigned integer operands are rejected in VM/native lowering (`u64` only combines with `u64`), and `negate` rejects unsigned operands. Pointer arithmetic is only defined for `plus`/`minus` with a pointer on the left and an integer offset (see Pointer arithmetic below).
- **`greater_than(left, right)`, `less_than(left, right)`, `greater_equal(left, right)`, `less_equal(left, right)`, `equal(left, right)`, `not_equal(left, right)`, `and(left, right)`, `or(left, right)`, `not(value)`:** comparison wrappers used after operator/control-flow desugaring. Comparisons respect operand signedness (`u64` uses unsigned ordering; `i32`/`i64` use signed ordering), and mixed signed/unsigned comparisons are rejected in the current IR/native subset; `bool` participates as a signed `0/1`, so `bool` with `u64` is rejected as mixed signedness. Boolean combinators accept `bool` or integer inputs and treat zero as `false` and any non-zero value as `true`. The current IR/native subset accepts only integer/bool operands for comparisons and boolean ops (float/string comparisons are not yet supported outside the C++ emitter).
- **`/math/clamp(value, min, max)`:** numeric helper used heavily in rendering scripts. VM/native lowering supports integer clamps (`i32`, `i64`, `u64`) and follows the usual integer promotion rules (`i32` mixed with `i64` yields `i64`, while `u64` requires all operands to be `u64`). Mixed signed/unsigned clamps are rejected. The C++ emitter also handles floats.
- **`if(condition, trueBranch, falseBranch)`:** canonical conditional form after control-flow desugaring.
  - Signature: `if(Envelope, Envelope, Envelope)`
  - 1) must evaluate to a boolean (`bool`), either a boolean value or a function returning boolean
  - 2) must be a function or a value, the function return or the value is returned by the `if` if the first arg is `true`
  - 3) must be a function or a value, the function return or the value is returned by the `if` if the first arg is `false`
  - Evaluation is lazy: the condition is evaluated first, then exactly one of the two branch envelopes is evaluated.
- **`repeat(count) { ... }`:** statement-only loop helper. `count` must be integer/bool, and the body is required.
- **`notify(path, payload)`, `insert`, `take`:** PathSpace integration hooks for signaling and data movement.
- **`return(value)`:** explicit return primitive; may appear as a statement inside control-flow blocks. For `void` definitions, `return()` is allowed. Implicit `return(void)` fires at end-of-body when omitted. Non-void definitions must return on all control paths; fallthrough is a compile-time error.
- **IR note:** VM/native IR lowering supports numeric/bool `array<T>(...)` and `vector<T>(...)` calls plus `array<T>{...}` and `vector<T>{...}` literals, along with `count`/`at`/`at_unsafe` on those sequences. Map literals are supported in VM/native when both key and value types are numeric/bool; string-keyed maps remain C++-emitter-only for now. Vector growth operations still require heap allocation and are not supported in VM/native yet.

### Standard Library Reference (draft, v0)
- **Status:** this reference is a snapshot of the current builtin surface. It will be versioned once the standard library is split into packages.
- **Versioning (planned):**
  - Each package declares a semantic version (e.g., `1.2.0`).
  - `include<..., version="1.2.0">` selects a specific package revision.
  - The compiler will expose `--stdlib-version` to pin the default package set.
- **Namespaces:**
  - `/math/*` is imported via `import /math`.
  - Core builtins (`assign`, `count`, `print*`, `convert`, etc.) live in the root namespace.
- **Conformance note:** the VM/native subset may reject functions that the C++ emitter supports (e.g., float-heavy math); the reference will mark such cases explicitly.

## Runtime Stack Model (draft)
- **Frames:** each execution pushes a frame recording the instruction pointer, constants, locals, captures, and effect mask. Frames are immutable from the caller’s perspective; `assign` creates new bindings.
- **Deterministic evaluation:** arguments evaluate left-to-right; boolean `and`/`or` short-circuit; `return(value)` unwinds automatically. Implicit `return(void)` fires if the body reaches the end.
- **Transform boundaries:** rewrites annotate frame entry/exit so the VM, C++, and GLSL backends share a consistent calling convention.
- **Resource handles:** PathSpace references/handles live inside frames as opaque values; lifetimes follow lexical scope.
- **Tail execution (planned):** future optimisation collapses tail executions to reuse frames (VM optional, GPU required).
- **Effect annotations:** purity by default; explicit `[effects(...)]` opt-ins. Standard library defaults to stdout/stderr effects.

### Execution Metadata (draft)
- **Scheduling scope:** queue/thread selection stays host-driven; there are no stack- or runner-specific annotations yet, so executions inherit the embedding runtime’s default placement.
- **Capabilities:** effect masks double as capability descriptors (IO, global write, GPU access, etc.). Additional attributes can narrow capabilities (`[capabilities(io_out, pathspace_insert)]`).
- **Instrumentation:** executions carry metadata (source file/line plus effect/capability masks) for diagnostics and tracing.
- **Open design items:** finalise the capability taxonomy and determine which instrumentation fields flow into inspector tooling vs. runtime-only logs.

## Type & Class Semantics (draft)
- **Structural classes:** `[return<void>] class<Name>(members{…})` desugars into namespace `Name::` plus constructors/metadata. Instances are produced via constructor executions.
- **Composition over inheritance:** “extends” rewrites replicate members and install delegation logic; no hidden virtual dispatch unless a transform adds it.
- **Generics:** classes accept template parameters (`class<Vector<T>>(…)`) and specialise through the transform pipeline.
- **Nested generics:** template arguments may themselves be generic types (`map<i32, array<i32>>`), and the parser preserves the nested type string for later lowering.
- **Interop:** generated code treats classes as structs plus free functions (`Name::method(instance, …)`); VM closures follow the same convention.
- **Field visibility:** stack-value declarations accept `[public]`, `[package]`, or `[private]` transforms (default: private); they are mutually exclusive. The compiler records `visibility` metadata per field so tooling and backends enforce access rules consistently. `[package]` exposes the field to any module compiled into the same package; `[public]` emits accessors in the generated namespace surface.
- **Static members:** add `[static]` to hoist storage to namespace scope while reusing the field’s visibility transform. Static fields still participate in the struct manifest so documentation and reflection stay aligned, but only one storage slot exists per struct definition.
- **Example:**
  ```
  import /math
  namespace demo {
    [struct]
    brush_settings() {
      [public float] size{12.0f}
      [private float] jitter{0.1f}
      [package static handle<Texture>] palette{load_default_palette()}

      [public]
      Create() {
        assign(this.size, clamp(this.size, 1.0f, 64.0f))
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
- Integer literals require explicit width suffixes (`42i32`, `42i64`, `42u64`). Higher-level filters may add suffixes before parsing; primec enables implicit `i32` suffixing only when `implicit-i32` is included in `--text-filters`.
- Float literals accept `f`, `f32`, or `f64` suffixes; when omitted, they default to `f32`. Exponent notation (`1e-3`, `1.0e6f`) is supported.
- **Strings:** quoted with escapes (`"…"`) or raw (`"…"raw_utf8` / `"…"raw_ascii`). Surface literals accept `utf8`/`ascii` suffixes, but the canonical/bottom-level form uses only `raw_utf8`/`raw_ascii` after escape decoding. `implicit-utf8` (enabled by default) appends `utf8` when omitted.
- **Boolean:** keywords `true`, `false` map to backend equivalents.
- **Composite constructors:** structured values are introduced through standard type executions (`ColorGrade([hue_shift] 0.1f [exposure] 0.95f)`) or helper transforms that expand the Envelope. Bindings pass constructor arguments directly in `{}` (e.g., `[ColorGrade] grade{[hue_shift] 0.1f [exposure] 0.95f}`), which desugars to a type constructor call internally. Labeled arguments map to fields, and every field must have either an explicit argument or a type-provided default before validation. Labeled arguments may only be used on user-defined calls.
- **Labeled arguments:** labeled arguments use a bracket prefix (`[name] value`) and may be reordered (including on executions). Positional arguments fill the remaining parameters in declaration order, skipping labeled entries. Builtin calls (operators, comparisons, clamp, convert, pointer helpers, collections) do not accept labeled arguments.
  - Example: `sum3(1i32 [c] 3i32 [b] 2i32)` is valid.
  - Example: `array<i32>([first] 1i32)` is rejected because collections are builtin calls.
  - Duplicate labeled arguments are rejected for definitions and executions (`execute_task([a] 1i32 [a] 2i32)`).
- **Collections:** `array<Type>{ … }` / `array<Type>[ … ]`, `vector<Type>{ … }` / `vector<Type>[ … ]`, `map<Key,Value>{ … }` / `map<Key,Value>[ … ]` rewrite to standard builder functions. The brace/bracket forms desugar to `array<Type>(...)`, `vector<Type>(...)`, and `map<Key,Value>(key1, value1, key2, value2, ...)`. Map literals supply alternating key/value forms.
  - Requires the `collections` text filter (enabled by default in `--text-filters`).
  - Map literal entries are read left-to-right as alternating key/value forms; an odd number of entries is a diagnostic.
  - String keys are allowed in map literals (e.g., `map<string, i32>{"a"utf8 1i32}`), and nested forms inside braces are rewritten as usual.
  - Collections can appear anywhere forms are allowed, including execution arguments.
  - Numeric/bool array literals (`array<i32>{...}`, `array<i64>{...}`, `array<u64>{...}`, `array<bool>{...}`) lower through IR/VM/native.
  - `vector<T>` is a resizable, owning sequence. `vector<T>{...}` and `vector<T>(...)` are variadic constructors (0..N). Vector operations that grow capacity require `effects(heap_alloc)` (or the active default effects set); literals with one or more elements are treated as growth operations. Backends that do not support heap allocation reject vector operations.
  - Vector helpers: `count(value)` / `value.count()`, `at(value, index)`, `at_unsafe(value, index)`, `push(value, item)`, `pop(value)`, `reserve(value, capacity)`, `capacity(value)`, `clear(value)`, `remove_at(value, index)`, `remove_swap(value, index)`.
  - Numeric/bool map literals (`map<i32, i32>{...}`, `map<u64, bool>{...}`) also lower through IR/VM/native (construction, `count`, `at`, and `at_unsafe`).
  - String-keyed map literals compile through the C++ emitter only, using `std::string_view` keys.
- **Conversions:** no implicit coercions. Use explicit executions (`convert<float>(value)`) or custom transforms. The builtin `convert<T>(value)` is the default cast helper and supports `int/i32/i64/u64/bool` in the minimal native subset (integer conversions currently lower as no-ops in the VM/native backends, while the C++ emitter uses `static_cast`; `convert<bool>` compares against zero, so any non-zero value—including negative integers—yields `true`). Float conversions are currently supported only by the C++ emitter.
- **Float note:** VM/native lowering currently rejects float literals, float bindings, and float arithmetic; use the C++ emitter for float-heavy scripts until float opcodes land in PSIR.
- **String note:** VM/native lowering supports string literals and string bindings in `print*`, plus `count`/indexing (`at`/`at_unsafe`) on string literals and string bindings that originate from literals; other string operations still require the C++ emitter for now.
  - `convert<bool>` is valid for integer operands (including `u64`) and treats any non-zero value as `true`.
- **Mutability:** values immutable by default; include `mut` in the stack-value execution to opt-in (`[float mut] value{...}`).
- **Open design:** finalise literal suffix catalogue, raw string semantics across backends, and the composite-constructor defaults/validation rules.

## Pointers & References (draft)
- **Explicit types:** `Pointer<T>`, `Reference<T>` mirror C++ semantics; no implicit conversions.
- **Surface syntax:** canonical syntax uses explicit calls (`location`, `dereference`, `plus`/`minus`); the `operators` text filter rewrites `&name`/`*name` sugar into those calls.
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
- **PSIR versioning:** current portable IR is PSIR v9 (adds `PrintArgvUnsafe` for unchecked entry-arg prints, plus `PrintArgv`, `PushArgc`, pointer helpers, `ReturnVoid`, and print opcode upgrades).
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

import /math

[return<int> default_operators] add<int>(a, b) { return(plus(a, b)) }

[default_operators] execute_add<int>(x, y)

clamp_exposure(img) {
  if(
    greater_than(img.exposure, 1.0f),
    on_true{ notify("/warn/exposure"utf8, img.exposure) },
    on_false{ }
  )
}

tweak_color([copy mut restrict<Image>] img) {
  assign(img.exposure, clamp(img.exposure, 0.0f, 1.0f))
  assign(img.gamma, plus(img.gamma, 0.1f))
  apply_grade(img)
}

[return<int>]
convert_demo() {
  return(convert<int>(1.5f))
}

[return<int>]
labeled_args_demo() {
  return(sum3(1i32 [c] 3i32 [b] 2i32))
}

[return<float> default_operators control_flow desugar_assignment]
float blend(float a, float b) {
  float result = (a + b) * 0.5f;
  if (result > 1.0f) {
    result = 1.0f;
  }
  return result;
}

// Canonical, post-transform form
[return<float>] blend<float>(a, b) {
  assign(result, multiply(plus(a, b), 0.5f))
  if(
    greater_than(result, 1.0f),
    on_true{ assign(result, 1.0f) },
    on_false{ }
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
- **PathSpace runtime wiring:** generated code uses PathSpace helper APIs (`insert`, `take`, `notify`). Transforms wrap high-level IO primitives so emitted C++/VM code interacts through typed handles; GLSL outputs map onto renderer inputs. Lifetimes/ownership align with PathSpace nodes.

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
