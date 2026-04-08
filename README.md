# PrimeStruct

PrimeStruct is a systems programming language focused on explicit structure,
deterministic compilation, and a small canonical core that targets CPU, GPU,
and VM backends. Every program is treated as a single structural form and
lowered to a canonical IR before code generation.

The same core model supports both ahead-of-time compilation and scripted
execution via the PrimeScript VM. There is no separate scripting language:
PrimeScript is PrimeStruct running in VM mode.

------------------------------------------------------------------------

## Quick Start (Examples)

Hello, world:

    [return<int> effects(io_out)]
    main() {
      print_line("Hello, world!"utf8)
      return(0i32)
    }

Looping with a readable `for` header (semicolons are optional separators):

    [operators effects(io_out)]
    main() {
      for([int mut] i{0i32}; i < 3; ++i) {
        print_line("tick"utf8)
      }
      return(0i32)
    }

Conditionals use explicit `else` and desugar to labeled envelopes:

    [operators return<int>]
    clamp_to_zero([int] value) {
      if (value < 0i32) {
        return(0i32)
      } else {
        return(value)
      }
    }

------------------------------------------------------------------------

## Goals

PrimeStruct is designed to:

- Provide systems-level power comparable to C without hidden control flow
- Treat all constructs uniformly as structure, not special cases
- Support multiple execution modes (native, GPU, VM) from a single core
- Remain deterministic and analyzable for tooling and testing
- Stay approachable from C with explicit envelope annotations and minimal magic

------------------------------------------------------------------------

## Core Ideas

### Envelope

Everything in PrimeStruct maps to one canonical envelope ("Envelope") in the AST:

    [transforms] name<templates>(parameters) { body }

- `[...]` lists transforms (attributes) applied in order.
- `<...>` is an optional template list.
- `(...)` contains parameters or arguments.
- `{...}` contains a definition body or a binding initializer block (which yields a value).
- Executions are call-style envelopes with an implicit empty body in the AST.

This uniform representation simplifies parsing, transformation, and lowering.

------------------------------------------------------------------------

### Canonical IR

All PrimeStruct programs lower to a single canonical IR before code generation.
Backends never see surface syntax or syntactic sugar, ensuring consistent
semantics and deterministic behavior across targets.

------------------------------------------------------------------------

## Getting Started

### Prerequisites

- Bash-compatible shell environment
- CMake 3.20 or newer
- A C++23-capable compiler such as `clang++` or `g++`
- Python 3
- `rg` (`ripgrep`) for helper scripts such as `scripts/lines_of_code.sh`
- For coverage: `clang++`, `llvm-profdata`, and `llvm-cov` in `PATH` (or discoverable through `xcrun` on macOS)

### Build

    ./scripts/compile.sh

Release build and test:

    ./scripts/compile.sh --release

Optional Wasm runtime checks (auto-skip when `wasmtime` is unavailable):

    ./scripts/run_wasm_runtime_checks.sh

Coverage helper:

    ./scripts/code_coverage.sh

Benchmark helper (after a build, typically release):

    ./scripts/benchmark.sh --build-dir ./build-release

Lines-of-code helper:

    ./scripts/lines_of_code.sh

### Generated Artifacts

- Debug artifacts are written to `build-debug/`
- Release artifacts are written to `build-release/`
- `compile_commands.json` is generated in each build directory on configure
- Coverage outputs are written to `build-debug/coverage/`

### Run

Compile to a native executable:

    primec --emit=native examples/0.Concrete/hello_world.prime -o hello
    ./hello

Run via the VM:

    primevm examples/0.Concrete/hello_world.prime --entry /main

------------------------------------------------------------------------

## Basic Syntax

### Definitions

A definition introduces a named body with explicit return/effects transforms:

    [return<int> effects(io_out)]
    main([array<string>] args) {
      print_line("Hello, world!"utf8)
      return(0i32)
    }

Surface syntax may omit the return transform and rely on inference; canonical/bottom-level form always includes
`return<T>` and explicit literal suffixes. Example surface: `main() { return(0) }` → canonical:
`[return<int>] main() { return(0i32) }`.

### Executions

Executions are call-style envelopes with
mandatory parentheses and no body:

    execute_task([count] 2i32)

### Bindings (Stack Values)

Local values are introduced as stack-value executions:

    [i32] value{7i32}
    [string] message{"hi"utf8}

Bindings are immutable by default.

### Mutability

Mutation must be explicitly opted into with `mut` inside the binding:

    [i32 mut] value{1i32}
    assign(value, 6i32)

Only mutable bindings can be assigned to.

------------------------------------------------------------------------

## Control Flow

PrimeStruct desugars control flow into canonical calls with labeled block
literals. The surface `if` form requires an `else` block:

    if(less_than(value, min)) {
      return(min)
    } else {
      return(value)
    }

Canonical form:

    if(
      less_than(value, min),
      then() { return(min) },
      else() { return(value) }
    )

`if` takes a boolean condition and two definition envelopes. Evaluation is lazy: the
condition is evaluated first, then exactly one of the two bodies runs.

Operators are also rewritten into prefix calls (`a + b` -> `plus(a, b)`).

Brace constructors are allowed in value positions and use the last value
of the block (or an explicit `return`):

    bool{35}        // true
    float{ return(1.0f32) }

------------------------------------------------------------------------

## Strings

- Double-quoted strings process escapes (`"..."utf8` / `"..."ascii`).
- Single-quoted strings are raw (`'...'utf8` / `'...'ascii`).
- `implicit-utf8` (enabled by default) appends `utf8` when omitted.
- Canonical form uses double-quoted strings with normalized escapes and an
  explicit `utf8`/`ascii` suffix.

Example:

    [string] path{'C:\temp'ascii}

------------------------------------------------------------------------

## Collections

Collections desugar into helper calls:

    array<i32>{1i32, 2i32, 3i32}
    map<i32, i32>{1i32 10i32 2i32 20i32}

------------------------------------------------------------------------

## Imports and Namespaces

Imports splice text inline before text transforms:

    import<"/std/io", version="1.2.0">

Compatibility note: `include<...>` is no longer accepted. Use `import<...>`
for source imports and `import /path/*` for namespace imports. Import roots
must be passed with `--import-path` (or `-I`); `--include-path` was removed.

Namespaces prefix enclosed names:

    namespace demo {
      [return<int>]
      get_value() {
        return(19i32)
      }
    }

------------------------------------------------------------------------

## Entry Definition

The default entry path is `/main`. For VM/native backends, the entry
definition must take a single `array<string>` parameter.

------------------------------------------------------------------------

## Tooling

- Compiler: `primec`
- VM runner: `primevm` (PrimeScript VM)
- IR inspection: `primec --emit=ir` (PSIR) and diagnostics exports
- Backends:
  - C++ / native
  - GPU shading (GLSL / SPIR-V)
  - VM bytecode

------------------------------------------------------------------------

## Backend Support

- Native (C++ emitter): primary target, full language surface.
- VM (PrimeScript): canonical IR execution for scripting/tests, aligns with native semantics.
- GPU (GLSL/SPIR-V): GPU-safe subset only (POD types, fixed-width numerics, no IO,
  strings, heap, or recursion).

See `docs/PrimeStruct.md` for detailed backend constraints and effect allowlists.

------------------------------------------------------------------------

## Docs

- Language design overview: `docs/PrimeStruct.md`
- Syntax spec: `docs/PrimeStruct_SyntaxSpec.md`
- Graphics API contract: `docs/Graphics_API_Design.md`
- Coding guidelines: `docs/Coding_Guidelines.md`
- Changelog notes: `docs/changelog.md`

------------------------------------------------------------------------

## Examples

See `examples/` for runnable samples organized by language level (0.Concrete → 3.Surface). Highlights:

- `examples/0.Concrete/hello_world.prime` – minimal entry definition
- `examples/3.Surface/if_else.prime` – `if` in statement position
- `examples/3.Surface/collections.prime` – array/vector/map literals

------------------------------------------------------------------------

## Contributing

See `AGENTS.md` for naming rules, build/test workflow, and contribution
guidelines.

------------------------------------------------------------------------

## Roadmap

- Source maps and stable JSON diagnostics for tooling and tests.
- Incremental compilation caches with `--watch`.
- IDE/LSP integration (go-to-definition, completion, PathSpace editor adapter).
- Runtime tracing and stack traces mapped via source maps.

------------------------------------------------------------------------

## Status

PrimeStruct is experimental and under active design. The syntax spec and
canonical IR are now stable enough for conformance testing, but execution
semantics and backend coverage are still evolving. Backward compatibility
is not yet guaranteed.

------------------------------------------------------------------------

## License

Licensed under the terms in `LICENSE`.

------------------------------------------------------------------------

## Philosophy

PrimeStruct favors explicit structure over implicit behavior, deterministic
semantics over cleverness, and systems clarity over abstraction layering. If
something happens, you should be able to see it in the structure.
