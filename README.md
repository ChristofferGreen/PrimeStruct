# PrimeStruct

**PrimeStruct** is a systems programming language focused on expressive
structure and deterministic compilation. Every program is treated as a
single structural form and lowered to a canonical intermediate
representation (IR) that targets CPU, GPU, and VM backends.

The same core model supports both ahead-of-time compilation and scripted
execution via the **PrimeScript VM**, making the language approachable
from C while enabling powerful, consistent cross-backend semantics and
tooling.

------------------------------------------------------------------------

## Goals

PrimeStruct is designed to:

-   Provide **systems-level power comparable to C**, without implicit
    control flow or hidden allocation
-   Treat **all constructs uniformly as structure**, rather than
    special-casing functions, statements, or types
-   Support **multiple execution modes** (native, GPU, VM) from a single
    semantic core
-   Remain **deterministic and analyzable**, suitable for testing,
    replay, and tooling
-   Be **approachable from C**, with explicit types, predictable layout,
    and minimal magic

------------------------------------------------------------------------

## Core Ideas

### Everything Is Structure

In PrimeStruct, there is no fundamental distinction between:

-   functions\
-   statements\
-   blocks\
-   stack values\
-   control flow

All of these are expressed using a **single structural envelope**:

    [transforms] name<templates>(parameters) { body }

This uniform representation simplifies parsing, transformation, and
lowering, while keeping the language explicit and predictable.

------------------------------------------------------------------------

### Canonical IR

All PrimeStruct programs lower to a **single canonical IR** before code
generation.\
Backends (C++, GPU, VM) never see surface syntax or syntactic sugar.

This ensures:

-   consistent semantics across backends\
-   identical optimization opportunities\
-   deterministic behavior independent of target

------------------------------------------------------------------------

### Compiled vs Scripted Execution

PrimeStruct code can be used in two modes:

-   **Compiled**: ahead-of-time or JIT compilation to native CPU or GPU
    code\
-   **Scripted**: execution via the embedded **PrimeScript VM**

There is no separate "scripting language" --- **PrimeScript is
PrimeStruct running in VM mode**.

------------------------------------------------------------------------

## Basic Syntax

### Definitions

A definition introduces a structural unit with a body:

    add(a: i32, b: i32) {
      return(a + b)
    }

Definitions may represent functions, blocks, or higher-level constructs.

------------------------------------------------------------------------

### Executions

An execution invokes a definition:

    result(add(2, 3))

Executions never have bodies --- only arguments.

------------------------------------------------------------------------

### Stack Values

Local values are introduced as structural executions:

    count(i32, 10)
    exposure(float, 1.25)

Values are immutable by default.

------------------------------------------------------------------------

### Mutability

Mutation must be explicitly opted into:

    exposure(mut float, 1.25)
    assign(exposure, exposure + 0.5)

Only values declared as `mut` may be written to.

------------------------------------------------------------------------

## Control Flow

Control flow is expressed structurally, not via keywords.

### Conditional Execution

    if(cond,
       then {
         log("true branch")
       },
       else {
         log("false branch")
       })

There is no implicit branching or fallthrough --- control flow is
explicit and analyzable.

------------------------------------------------------------------------

### Loops

    repeat(count) {
      process()
    }

Loops are structural executions, not statements.

------------------------------------------------------------------------

## Structures

Structures are defined using the same envelope as everything else.

    color_grade() {
      exposure(mut float, 1.0)
      gamma(float, 2.2)
    }

There is no special "struct syntax" --- a structure is simply a
definition with structured members.

------------------------------------------------------------------------

### Construction

    grade(color_grade)
    assign(grade.exposure, 1.5)

Layout, alignment, and placement are explicit and validated during
compilation.

------------------------------------------------------------------------

## Examples

### Simple Function

    clamp_exposure(value: float) {
      result(mut float, value)

      if(result > 1.0,
         then { assign(result, 1.0) },
         else { })

      return(result)
    }

------------------------------------------------------------------------

### Scripted Execution (PrimeScript)

    log("Running scripted execution")

    value(float, 0.75)
    assign(value, clamp_exposure(value))

    log(value)

The same code can be:

-   executed immediately via the PrimeScript VM\
-   compiled to native code without modification

------------------------------------------------------------------------

## Tooling

Planned tooling includes:

-   **Compiler**: `primestructc`\
-   **VM / Interpreter**: PrimeScript VM\
-   **IR inspection**: dump and visualize canonical IR\
-   **Backends**:
    -   C++ / native\
    -   GPU shading (GLSL / SPIR-V)\
    -   VM bytecode

------------------------------------------------------------------------

## Status

PrimeStruct is currently **experimental** and under active design.

The syntax, IR, and execution model are being refined alongside the
compiler and VM implementation. Backward compatibility is **not yet
guaranteed**.

------------------------------------------------------------------------

## Philosophy

PrimeStruct favors:

-   explicit structure over implicit behavior\
-   uniform representation over convenience syntax\
-   deterministic semantics over cleverness\
-   systems clarity over abstraction layering

If something happens, you should be able to **see it in the structure**.
