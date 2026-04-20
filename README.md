# PrimeStruct

PrimeStruct is an experimental systems programming language built around one
structural core.

The same source language can be:
- compiled to a native executable directly
- compiled to C++ and then to a native executable
- run directly on the PrimeScript VM
- lowered to GPU-oriented backends for the supported subset

Everything lowers through one canonical IR first. The goal is a language that
stays readable, keeps important behavior visible, and still compiles through
one deterministic core.

At a glance:
- pure helpers can stay short
- effects such as allocation and IO can be part of the function boundary
- locals can stay concise when the initializer already shows enough
- the readable surface still lowers through one small canonical core before IR

## The Envelope Model

PrimeStruct is built around one canonical form: the envelope.

```prime
[transform-list] name<template-list>(param-list) { body }
```

Definitions and executions both use that same shape in the AST. A definition
has a body. An execution is the same envelope with an implicit empty body, so
`fib(10)` and `print_line("hello")` are execution envelopes.

The syntax pieces are:
- `[...]`: transforms and attributes. This is where return envelopes, effects,
  visibility, traits, and markers such as `struct` live.
- `name`: the definition name or call target.
- `<...>`: optional template arguments.
- `(...)`: parameters on a definition, arguments on an execution.
- `{...}`: a definition body. For bindings, braces hold the initializer.

For example, in `[int] fib([int] n) { ... }`, the first `[int]` says the
definition returns an `int`, `fib` is the name, `([int] n)` is the parameter
list, and `{ ... }` is the body.

This envelope idea underpins the whole language. Surface features such as
operator syntax, `if (...) { ... } else { ... }`, indexing, and method sugar
are rewritten into a smaller envelope-based core before semantic analysis and
IR lowering.

## Reading The Surface

The surface syntax is compact. These rules make the examples below easier to
read:
- `name{expr}` introduces a local binding when the initializer already fixes
  the type.
- `[Type] name{expr}` is the same idea with an explicit type pin.
- `[effects(io_err), int]` means "this helper returns an `int` and may perform
  `io_err` effects." A pure helper can often stay as just `[int]`.
- The top-level surface is not the compiler's bottom-level internal spelling.
  The compiler fills in common `return<...>` wrapping, literal suffixes, and
  some low-risk default effects before lowering.

## Language Levels

PrimeStruct is organized into four language levels. Each higher level lowers
into the one below it:
- `0.Concrete`: fully explicit canonical envelopes
- `1.Template`: canonical syntax plus explicit templates
- `2.Inference`: canonical syntax plus `auto` and omitted envelopes
- `3.Surface`: the readable surface with operator sugar, indexing, collection
  literals, and block-style control flow

The `examples/` tree follows the same naming, so `examples/0.Concrete/` shows
the fully explicit form and `examples/3.Surface/` shows the intended user-facing
surface.

## Examples

Hello world:

```prime
[int]
main() {
  print_line("Hello, world!")
  return(0)
}
```

Fibonacci:

```prime
[int]
fib([int] n) {
  if (n < 2) {
    return(n)
  } else {
    return(fib(n - 1) + fib(n - 2))
  }
}

[int]
main() {
  return(fib(10))
}
```

Struct defaults and field labels:

```prime
[struct]
Rect {
  [int] width{1}
  [int] height{2}
}

area_with_default_height() {
  rect{Rect([width] 4)}
  return(rect.width * rect.height)
}
```

Initializer-first local bindings:

```prime
[int]
clamp_to_limit([int] start) {
  [mut] current{start}
  limit{5}

  while(current < limit) {
    current = current + 1
  }

  return(current)
}
```

If you want the type spelled out, use `[int] limit{5}`.

Error handling:

```prime
import /std/file/*

[Result<int, FileError>]
load_count() {
  return(Result.ok(7))
}

[effects(io_err)]
report_file_error([FileError] err) {
  print_line_error(err.why())
}

[int effects(io_err) on_error<FileError, report_file_error>]
main() {
  count{load_count()}
  return(count?)
}
```

The entrypoint is explicit about both error handling and effects without
forcing that ceremony onto every pure helper.

Named arguments:

```prime
[int]
compute_score([int] base, [int] bonus, [int] penalty) {
  return(base + bonus - penalty)
}

[int]
main() {
  return(compute_score([base] 10, [bonus] 4, [penalty] 2))
}
```

Collections:

```prime
import /std/collections/*

lookup_value() {
  pairs{map<int, int>{7=10, 9=4}}
  return(pairs[7] + pairs[9])
}
```

Use `import /std/collections/*` for top-level collection examples that rely on
bare collection names, method sugar, or direct indexing.

## What PrimeStruct Emphasizes

PrimeStruct is not aiming to look maximally familiar. It is trying to make a
few things easier to see in source code:
- what a function returns
- what effects it may perform
- when a local binding is simply "bind this name to this initializer"
- how readable surface syntax maps to one deterministic canonical core

If some syntax looks unusual at first glance, that is usually because
PrimeStruct favors visible structure over inherited C-family defaults.

## Concurrency And Threads

PrimeStruct takes a conservative view of multithreading.

The language tries to make the pieces that matter for concurrent code explicit:
- bindings are immutable by default
- `mut` is spelled on writable state
- effects are part of the function boundary
- `move(...)` and `Reference<T>` make ownership and borrowing visible

At the same time, PrimeStruct does not currently bake queue or thread placement
into ordinary source code. Scheduling is host-driven today, so programs do not
carry thread-affinity or runner annotations in the language core yet.

The idea is to keep the core semantics deterministic and portable across the
VM, direct native emission, C++-backed native builds, and GPU-oriented targets.
Ownership, borrowing, effects, and evaluation order are part of the language;
thread placement and runtime scheduling policy are currently part of the host
environment.

## Quick Start

Prerequisites:
- Bash-compatible shell
- CMake 3.20+
- A C++23 compiler such as `clang++` or `g++`
- Python 3
- `rg` (`ripgrep`)

Build the release configuration and run the test gate:

```bash
./scripts/compile.sh --release
```

Run a program on the VM:

```bash
./build-release/primevm examples/0.Concrete/hello_world.prime --entry /main
```

Compile a program to a native executable through the C++ path:

```bash
./build-release/primec --emit=exe examples/0.Concrete/hello_world.prime -o hello
./hello
```

Compile a program to a native executable through the direct native path:

```bash
./build-release/primec --emit=native examples/0.Concrete/hello_world.prime -o hello_native
./hello_native
```

If you want the default debug build instead:

```bash
./scripts/compile.sh
```

For more runnable samples, start in:
- [`examples/0.Concrete/`](examples/0.Concrete/)
- [`examples/3.Surface/`](examples/3.Surface/)

Inspect the pipeline stages directly:

```bash
./build-release/primec --dump-stage=pre_ast examples/0.Concrete/hello_world.prime
./build-release/primec --dump-stage=ast examples/0.Concrete/hello_world.prime
./build-release/primec --dump-stage=ast-semantic examples/0.Concrete/hello_world.prime
./build-release/primec --dump-stage=semantic-product examples/0.Concrete/hello_world.prime
./build-release/primec --dump-stage=ir examples/0.Concrete/hello_world.prime
```

## Where To Go Next

- [`docs/PrimeStruct.md`](docs/PrimeStruct.md) for the main language and
  compiler design document
- [`docs/PrimeStruct_SyntaxSpec.md`](docs/PrimeStruct_SyntaxSpec.md) for syntax
  and entry-point rules
- [`docs/CodeExamples.md`](docs/CodeExamples.md) for readable PrimeStruct
  example style
- [`docs/Coding_Guidelines.md`](docs/Coding_Guidelines.md) for broader code and
  API guidance
- [`examples/`](examples/) for runnable samples by language level
- [`AGENTS.md`](AGENTS.md) for contributor workflow and repo rules

## Useful Facts

- Default entry path: `/main`
- Entry definitions may take either no parameters or one `array<string>`
  parameter
- `primec` is the compiler and `primevm` runs programs on the VM
- `primec --emit=native` emits a native executable directly
- `primec --emit=cpp` emits C++, and `primec --emit=exe` uses the C++ path to
  produce a native executable
- imports are expanded before semantics, so compilation works over one
  flattened unit by default
- `--dump-stage=pre_ast|ast|ast-semantic|semantic-product|ir` exposes the main
  compiler stages for debugging and tooling
- Release builds live in `build-release/`
- Debug builds live in `build-debug/`

## Status

PrimeStruct is experimental. The language and toolchain are evolving quickly,
and backward compatibility is not guaranteed yet.

## License

Licensed under the terms in [`LICENSE`](LICENSE).
