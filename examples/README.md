# PrimeStruct examples

PrimeStruct supports line (`// ...`) and block (`/* ... */`) comments, but these examples
are intentionally minimal and self-describing via filenames.

## Running

From `build-debug/`:

```bash
./primec --emit=exe ../examples/0.Concrete/hello_world.prime -o /tmp/hello --entry /main
/tmp/hello
./primevm ../examples/0.Concrete/hello_world.prime --entry /main
```

## Files

### 0.Concrete
- `examples/0.Concrete/hello_world.prime`: printing + effects.
- `examples/0.Concrete/print_io.prime`: stdout/stderr print helpers.
- `examples/0.Concrete/return_7.prime`: minimal `return`.
- `examples/0.Concrete/return_123.prime`: literal return.
- `examples/0.Concrete/return_void.prime`: explicit `void` returns.
- `examples/0.Concrete/call_statement.prime`: call as statement.
- `examples/0.Concrete/identity_call.prime`: simple call/return.
- `examples/0.Concrete/assign_mutable.prime`: `mut` + `assign`.
- `examples/0.Concrete/binding_value.prime`: binding + return.
- `examples/0.Concrete/forward_definition.prime`: call before definition.
- `examples/0.Concrete/namespace_call.prime`: namespace blocks + slash-path calls.
- `examples/0.Concrete/return_struct.prime`: return a struct from a function.

### 1.Template
- `examples/1.Template/math_pow.prime`: integer + float `pow` via `/std/math/*`.
- `examples/1.Template/pointers_and_references.prime`: `Pointer<T>`, `Reference<T>`, `location`, `dereference`.

### 2.Inference
- `examples/2.Inference/infer_method_call.prime`: omitted binding type inference feeding a call.

### 3.Surface
- `examples/3.Surface/argv.prime`: entry args (`[array<string>] args`) + indexing sugar.
- `examples/3.Surface/collections.prime`: brace collection literals.
- `examples/3.Surface/collections_brackets.prime`: bracket collection literals + indexing.
- `examples/3.Surface/copy_constructor.prime`: `Copy` constructor shorthand inside structs.
- `examples/3.Surface/if_else.prime`: `if` in statement position.
- `examples/3.Surface/if_expression.prime`: `if` in expression position (both branches yield values).
- `examples/3.Surface/loop_while_for.prime`: `loop`, `while`, and `for` statement forms.
- `examples/3.Surface/operator_plus.prime`: operator sugar.
- `examples/3.Surface/param_defaults.prime`: default parameter values.
- `examples/3.Surface/hello_world_if.prime`: surface `if` blocks.
- `examples/3.Surface/syntax_braces.prime`: mixed `{}` bindings and surface `if`.
- `examples/3.Surface/result_helpers.prime`: `Result.error`/`Result.why` helpers + status check (`--emit=exe`/`--emit=vm`; not part of the IR example sweep).
- `examples/3.Surface/features_overview.prime`: high-level tour (imports, namespace, collections, loops, operators, indexing, effects).
- `examples/3.Surface/gpu_compute.prime`: compute kernel + `/std/gpu/dispatch` fallback in VM/native.
- `examples/3.Surface/raytracer.prime`: simple ray tracer (reflective spheres + checkerboard) emitting PPM.
- `examples/3.Surface/soa_vector_ecs.prime`: canonical ECS-style `soa_vector` wrapper update loop with deferred structural mutation.

### 4.Transforms
- `examples/4.Transforms/trace_calls_transform.prime`: user-authored `FunctionAst` transform hook module.
- `examples/4.Transforms/trace_calls_consumer.prime`: imports the hook module and attaches `[trace_calls return<int>]`.

### borrow_checker_negative
- `examples/borrow_checker_negative/`: intentionally non-compiling borrow-checker examples.
- Each `.prime` file has a matching `.expected.txt` with required diagnostics JSON fragments.
- These files are validated by the compile-run test suite and excluded from the regular
  "compile all examples to IR" sweep.
