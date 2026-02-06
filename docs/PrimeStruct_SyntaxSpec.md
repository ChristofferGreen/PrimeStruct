# PrimeStruct Syntax Spec (Draft)

This document defines the surface syntax and immediate semantics of PrimeStruct source files. It complements
`docs/PrimeStruct.md` by focusing on grammar, parsing rules, and the canonical core produced after text
filters and desugaring.

## 1. Overview

PrimeStruct uses a **uniform envelope** for definitions, executions, and bindings:

```
[transform-list] name<template-list>(param-list) { body-list }
```

- `[...]` is a list of transforms (attributes) applied to a node.
- `<...>` is an optional template argument list.
- `(...)` contains parameters or arguments.
- `{...}` contains a definition body or execution body arguments.

The parser accepts convenient surface forms (operator expressions, `if(...) { ... } else { ... }`,
indexing `value[index]`), then rewrites them into a small canonical core before semantic analysis
and IR lowering.

## 2. Lexical Structure

### 2.1 Identifiers

- Base identifier: `[A-Za-z_][A-Za-z0-9_]*`
- Slash path: `/segment/segment/...` where each segment is a base identifier.
- Identifiers are ASCII only; Unicode is not yet accepted.
- Reserved keywords: `mut`, `return`, `include`, `namespace`, `true`, `false`.

### 2.2 Whitespace and Comments

- Whitespace separates tokens but is otherwise insignificant.
- Statements are separated by newlines; semicolons are not used.
- Comments are not yet specified; treat `//` and `/* ... */` as reserved for future use.

### 2.3 Literals

- Integer literals:
  - `i32`, `i64`, `u64` suffixes required (unless the `implicit-i32` text filter is enabled).
  - Examples: `0i32`, `42i64`, `7u64`.
- Float literals:
  - `f32`, `f64` suffixes required for canonical form (surface forms may be normalized by filters).
  - Examples: `0.5f`, `1.0f32`, `2.0f64`.
- Bool literals: `true`, `false`.
- String literals:
  - Canonical form requires `utf8` or `ascii` suffix: `"hello"utf8`, `"moo"ascii`.
  - The `implicit-utf8` filter appends `utf8` to bare strings.
  - `ascii` rejects non-ASCII bytes.

## 3. File Structure

A source file is a list of top-level items:

```
file = { include | namespace | definition | execution }
```

### 3.1 Includes

```
include<"/path", version="1.2.0">
```

Includes expand inline before text filters run. Multiple paths may be listed in a single include.
Version selection follows the rules in `docs/PrimeStruct.md`.

### 3.2 Namespace Blocks

```
namespace foo {
  ...
}
```

Namespaces prefix enclosed definitions/executions with `/foo`. Namespace blocks may be reopened.

## 4. Grammar (EBNF-style)

This grammar describes surface syntax before text-filter rewriting.

```
file           = { top_item } ;

top_item       = include_decl | namespace_decl | definition | execution ;

include_decl   = "include" "<" include_list ">" ;
include_list   = include_entry { "," include_entry } ;
include_entry  = string_literal | "version" "=" string_literal ;

namespace_decl = "namespace" identifier "{" { top_item } "}" ;

definition     = envelope body_block ;
execution      = envelope exec_body_opt ;

envelope       = transforms_opt name template_opt params ;

transforms_opt = [ "[" transform_list "]" ] ;
transform_list = transform { transform } ;
transform      = identifier template_opt args_opt ;

template_opt   = [ "<" template_list ">" ] ;
template_list  = type_expr { "," type_expr } ;

params         = "(" param_list_opt ")" ;
param_list_opt = [ param { "," param } ] ;
param          = binding | expr ;

body_block     = "{" stmt_list_opt "}" ;
exec_body_opt  = [ "{" exec_body_list_opt "}" ] ;
exec_body_list_opt = [ expr { "," expr } ] ;

stmt_list_opt  = [ stmt { stmt } ] ;
stmt           = binding | expr ;

binding        = transforms_opt name template_opt args_opt ;

args_opt       = [ "(" arg_list_opt ")" ] ;
arg_list_opt   = [ arg { "," arg } ] ;
arg            = named_arg | expr ;
named_arg      = identifier "=" expr ;

expr           = literal
               | name
               | call
               | index_expr
               | block_if_expr
               | block_then_expr
               | block_else_expr ;

call           = name template_opt args_opt ;
name           = identifier | slash_path ;

index_expr     = expr "[" expr "]" ;

block_if_expr  = "if" "(" expr ")" block_if_body ;
block_then_expr= "then" block_if_body ;
block_else_expr= "else" block_if_body ;
block_if_body  = "{" stmt_list_opt "}" ;

literal        = int_lit | float_lit | bool_lit | string_lit ;
```

Notes:
- `binding` reuses the uniform envelope; it becomes a local declaration.
- `execution` is syntactically the same as `definition` but has no body block or has an execution body list.
- `expr` includes surface `if` blocks, which are rewritten into canonical calls.

## 5. Desugaring and Canonical Core

The compiler rewrites surface forms into canonical call syntax. The core uses prefix calls:

- `return(value)` is the only return form.
- Control flow: `if(cond, then{...}, else{...})` with `then{}` and `else{}` blocks as arguments.
- Operators are rewritten into calls:
  - `a + b` -> `plus(a, b)`
  - `a - b` -> `minus(a, b)`
  - `a * b` -> `multiply(a, b)`
  - `a / b` -> `divide(a, b)`
  - `a == b` -> `equal(a, b)` etc.
- Indexing:
  - `value[index]` -> `at(value, index)`

The canonical core is what semantic validation and IR lowering consume.

## 6. Definitions and Executions

### 6.1 Definitions

```
[return<int> effects(io_out)]
main() {
  return(0i32)
}
```

- Definitions may have a body block with statements.
- Transform list on the definition declares return type and effects.

### 6.2 Executions

```
execute_task(count = 2i32) { main(), main() }
```

- Executions are call-style constructs that may include a body list of calls.
- Execution bodies may only contain calls (no bindings, no `return`).
- Executions are parsed and validated but not emitted by the current C++ emitter.

## 7. Parameters, Arguments, and Bindings

### 7.1 Bindings

```
[i32 mut] count(1i32)
[string] message("hi"utf8)
```

- A binding is a stack value execution; type defaults to `int` when omitted.
- `mut` marks the binding as writable; otherwise immutable.

### 7.2 Parameters

Parameters use the same binding envelope as locals:

```
main([array<string>] args, [i32] limit(10i32)) { ... }
```

Defaults are limited to literal/pure expressions (no name references).

### 7.3 Named Arguments

- Named arguments are allowed in calls and executions.
- Positional arguments must come before named arguments.
- Each parameter may be bound once; duplicates are rejected.

## 8. Collections and Helpers

### 8.1 Arrays

```
array<i32>(1i32, 2i32)
array<i32>{1i32, 2i32}   // surface form
```

Helpers:
- `count(value)` / `value.count()`
- `at(value, index)`
- `at_unsafe(value, index)`

### 8.2 Maps

```
map<i32, i32>(1i32, 2i32)
map<i32, i32>{1i32=2i32}
```

Map IR lowering is currently limited in VM/native backends.

## 9. Effects

- Effects are declared via `[effects(...)]` on definitions or executions.
- Defaults can be supplied by `primec --default-effects`.
- Backends reject unsupported effects.

## 10. Return Semantics

- `return(value)` is explicit and canonical.
- `return()` is valid for `void` definitions.
- Non-void definitions must return along all control paths.

## 11. Entry Definition

- The compiler entry point is selected with `--entry /path` (default `/main`).
- For VM/native backends, the entry definition must take a single `array<string>` parameter.
- `args.count()` and `count(args)` are supported; indexing `args[index]` is bounds-checked unless
  `at_unsafe` is used.

## 12. Backend Notes (Syntax-Relevant)

- VM/native backends accept a restricted subset of types/operations (see design doc).
- Strings are supported for printing and entry-arg access in VM/native backends.
- The C++ emitter supports broader operations and full string handling.

## 13. Error Rules (Selected)

- `at(value, index)` on non-collections is rejected.
- Named arguments after positional ones are rejected.
- Duplicate named arguments are rejected.
- `return` in execution bodies is rejected.

