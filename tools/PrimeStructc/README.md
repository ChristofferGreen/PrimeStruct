# PrimeStructc (minimal)

This is a tiny proof-of-life compiler that accepts a small PrimeStruct subset and emits a native executable via C++.

## Build

```
clang++ -std=c++23 -O2 tools/PrimeStructc/main.cpp -o tools/PrimeStructc/PrimeStructc
```

## Usage

```
./tools/PrimeStructc/PrimeStructc --emit=cpp input.prime -o build/hello.cpp
./tools/PrimeStructc/PrimeStructc --emit=exe input.prime -o build/hello --entry /main
```

## Supported subset

- `[return<int>]` on definitions.
- Integer literals with optional `i32` suffix.
- `return(<expr>)` where `<expr>` is a literal, name, or call.
- Definitions with zero or more parameters (all `int` for now).
- Calls to other definitions with matching argument counts.
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
