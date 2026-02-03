# primec (minimal)

This is a tiny proof-of-life compiler that accepts a small PrimeStruct subset and emits a native executable via C++.

## Build

```
clang++ -std=c++23 -O2 src/*.cpp -Iinclude -o primec
```

## Usage

```
./primec --emit=cpp input.prime -o build/hello.cpp
./primec --emit=exe input.prime -o build/hello --entry /main
./primec --dump-stage ast input.prime -o /tmp/ignored
```

## Supported subset

- Optional `[return<int>]` on definitions (only `int` supported so far).
- Integer literals with optional `i32` suffix.
- `return(<expr>)` where `<expr>` is a literal, name, or call.
- Call statements are allowed before `return(...)` inside a definition body.
- Definitions with zero or more parameters (all `int` for now).
- Calls to other definitions with matching argument counts.
- Template lists (`<...>`) are parsed and preserved but ignored by codegen.
- Executions parse argument expressions and `{...}` body argument lists (ignored by codegen).
- `--dump-stage ast` prints a simple AST dump and exits.
- `namespace` blocks for path prefixes.
- `--entry /path` selects the entry definition (defaults to `/main`).

## Examples

```
[return<int>]
main() {
  return(7i32)
}
```

```
namespace demo {
  [return<int>]
  get_value() {
    return(19i32)
  }

  [return<int>]
  main() {
    return(get_value())
  }
}
```

Use `--entry /demo/main` to run the namespace example.

```
[return<int>]
identity(x) {
  return(x)
}

[return<int>]
main() {
  return(identity(88i32))
}
```
