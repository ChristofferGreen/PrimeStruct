# PrimeStruct

PrimeStruct is an experimental systems programming language built around one
structural core.

The same source language can be:
- compiled to a native executable
- run directly on the PrimeScript VM
- lowered to GPU-oriented backends for the supported subset

Everything lowers through one canonical IR first. The project is aiming for
explicit structure, deterministic compilation, and a surface language that
stays readable while still making important behavior visible at the boundary.

PrimeStruct is for readers who want a systems language to be explicit without
becoming syntactically heavy:
- Pure helpers can stay lightweight.
- Effects such as allocation and IO can be stated at the function boundary.
- Locals can stay concise when the initializer already tells the story.
- The readable surface still lowers through one small canonical core before IR.

## Why It Feels Different

PrimeStruct's surface syntax is intentionally compact. A few quick reading rules
make the examples below much easier to parse:
- `name{expr}` introduces a local binding when the initializer already fixes
  the type.
- `[Type] name{expr}` is the same idea with an explicit type pin.
- `[effects(io_err), int]` means "this helper returns an `int` and may perform
  `io_err` effects." A pure helper can often stay as just `[int]`.
- The top-level readable surface is not the compiler's bottom-level internal
  spelling. The compiler fills in common `return<...>` wrapping, literal
  suffixes, and some low-risk default effects before lowering.

## Small Tour

Hello world stays minimal:

```prime
[int]
main() {
  print_line("Hello, world!")
  return(0)
}
```

Struct defaults and field labels stay close to the data:

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

Initializer-first local bindings keep obvious code short:

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

If you want the type spelled out instead of inferred from the initializer, use
`[int] limit{5}`.

Error handling keeps fallibility and effects visible at the boundary:

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

The entrypoint stays explicit about both error handling and effects without
forcing that ceremony onto every pure helper.

Named arguments are available when a call benefits from them:

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

Collections use the same surface style:

```prime
import /std/collections/*

lookup_value() {
  pairs{map<int, int>{7=10, 9=4}}
  return(pairs[7] + pairs[9])
}
```

Current note: use `import /std/collections/*` for top-level collection examples
that rely on bare collection names, method sugar, or direct indexing.

## What PrimeStruct Is Trying To Buy

PrimeStruct is not trying to win by being maximally familiar. It is trying to
make a few things easier to see in source code:
- what a function returns
- what effects it may perform
- when a local binding is just "bind this name to this initializer"
- how readable surface syntax maps to one deterministic canonical core

If the syntax looks unusual at first glance, that is usually because PrimeStruct
prefers making structure explicit over inheriting C-family defaults.

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

Compile a program to a native executable:

```bash
./build-release/primec --emit=exe examples/0.Concrete/hello_world.prime -o hello
./hello
```

If you want the default debug build instead:

```bash
./scripts/compile.sh
```

For more runnable samples, start in:
- `examples/0.Concrete/`
- `examples/3.Surface/`

## Where To Go Next

- `docs/PrimeStruct.md` for the main language and compiler design document
- `docs/PrimeStruct_SyntaxSpec.md` for syntax and entry-point rules
- `docs/CodeExamples.md` for readable PrimeStruct example style
- `docs/Coding_Guidelines.md` for broader code and API guidance
- `examples/` for runnable samples by language level
- `AGENTS.md` for contributor workflow and repo rules

## Useful Facts

- Default entry path: `/main`
- Entry definitions may take either no parameters or one `array<string>`
  parameter
- `primec` is the compiler and `primevm` runs programs on the VM
- Release builds live in `build-release/`
- Debug builds live in `build-debug/`

## Status

PrimeStruct is experimental. The language and toolchain are evolving quickly,
and backward compatibility is not guaranteed yet.

## License

Licensed under the terms in `LICENSE`.
