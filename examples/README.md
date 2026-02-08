# PrimeStruct examples

PrimeStruct does not currently support comments in source files, so these examples
are intentionally minimal and self-describing via filenames.

## Running

From `build-debug/`:

```bash
./primec --emit=exe ../examples/hello_world.prime -o /tmp/hello --entry /main
/tmp/hello
```

## Files

- `examples/hello_world.prime`: printing + effects.
- `examples/argv.prime`: entry args (`[array<string>] args`).
- `examples/assign_mutable.prime`: `mut` + `assign`.
- `examples/collections.prime`: brace collection literals.
- `examples/collections_brackets.prime`: bracket collection literals + indexing.
- `examples/if_else.prime`: `if` in statement position.
- `examples/if_expression.prime`: `if` in expression position (both branches yield values).
- `examples/infer_method_call.prime`: omitted binding type inference feeding a method call.
- `examples/pointers_and_references.prime`: `Pointer<T>`, `Reference<T>`, `location`, `dereference`.
- `examples/executions.prime`: top-level executions (parsed/validated; not emitted by the C++ backend).
