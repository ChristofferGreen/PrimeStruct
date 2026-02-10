# PrimeStruct

PrimeStruct is a systems programming language focused on explicit structure,
deterministic compilation, and a small canonical core that targets CPU, GPU,
and VM backends. Every program is treated as a single structural form and
lowered to a canonical IR before code generation.

The same core model supports both ahead-of-time compilation and scripted
execution via the PrimeScript VM. There is no separate scripting language:
PrimeScript is PrimeStruct running in VM mode.

------------------------------------------------------------------------

## Goals

PrimeStruct is designed to:

- Provide systems-level power comparable to C without hidden control flow
- Treat all constructs uniformly as structure, not special cases
- Support multiple execution modes (native, GPU, VM) from a single core
- Remain deterministic and analyzable for tooling and testing
- Stay approachable from C with explicit types and minimal magic

------------------------------------------------------------------------

## Core Ideas

### Envelope

Everything in PrimeStruct uses one structural envelope ("Envelope"):

    [transforms] name<templates>(parameters) { body }

- `[...]` lists transforms (attributes) applied in order.
- `<...>` is an optional template list.
- `(...)` contains parameters or arguments.
- `{...}` contains a definition body or execution body arguments.

This uniform representation simplifies parsing, transformation, and lowering.

------------------------------------------------------------------------

### Canonical IR

All PrimeStruct programs lower to a single canonical IR before code generation.
Backends never see surface syntax or syntactic sugar, ensuring consistent
semantics and deterministic behavior across targets.

------------------------------------------------------------------------

## Basic Syntax

### Definitions

A definition introduces a named body with explicit return/effects transforms:

    [return<int> effects(io_out)]
    main([array<string>] args) {
      print_line("Hello, world!"utf8)
      return(0i32)
    }

### Executions

Executions are call-style constructs (top-level scheduling nodes). They may
include brace bodies that list calls:

    execute_repeat(count = 2i32) { main() main() }

Execution bodies may only contain calls (no bindings, no `return`).

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

PrimeStruct desugars control flow into canonical calls. The surface `if` form
requires an `else` block:

    if(less_than(value, min)) {
      return(min)
    } else {
      return(value)
    }

Canonical form:

    if(
      less_than(value, min),
      return(min),
      return(value)
    )

`if` takes three **envelopes**:

1) The condition: must evaluate to a boolean (`bool`), either a boolean value or a function returning boolean.
2) The true branch: must be a function or a value; the function return or the value is returned by the `if` if the condition is `true`.
3) The false branch: must be a function or a value; the function return or the value is returned by the `if` if the condition is `false`.

Evaluation is lazy: the condition is evaluated first, then exactly one of the two branch envelopes is evaluated.

Operators are also rewritten into prefix calls (`a + b` -> `plus(a, b)`).

------------------------------------------------------------------------

## Strings

- Surface strings accept `"..."utf8`, `"..."ascii`, or raw forms
  `"..."raw_utf8` / `"..."raw_ascii` (no escape processing in raw bodies).
- `implicit-utf8` (enabled by default) appends `utf8` when omitted.
- Canonical/bottom-level form always uses `raw_utf8` / `raw_ascii` after
  escape decoding.

Example:

    [string] path{"C:\\temp"raw_ascii}

------------------------------------------------------------------------

## Collections

Collections desugar into helper calls:

    array<i32>{1i32, 2i32, 3i32}
    map<i32, i32>{1i32=10i32, 2i32=20i32}

------------------------------------------------------------------------

## Includes and Namespaces

Includes splice text inline before text filters:

    include<"/std/io", version="1.2.0">

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
- IR inspection: dump canonical IR and diagnostics
- Backends:
  - C++ / native
  - GPU shading (GLSL / SPIR-V)
  - VM bytecode

------------------------------------------------------------------------

## Status

PrimeStruct is experimental and under active design. Syntax, IR, and
execution semantics are still evolving, and backward compatibility is
not yet guaranteed.

------------------------------------------------------------------------

## Philosophy

PrimeStruct favors explicit structure over implicit behavior, deterministic
semantics over cleverness, and systems clarity over abstraction layering. If
something happens, you should be able to see it in the structure.
