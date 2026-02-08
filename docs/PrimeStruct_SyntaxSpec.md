# PrimeStruct Syntax Spec (Draft)

This document defines the surface syntax and immediate semantics of PrimeStruct source files. It complements
`docs/PrimeStruct.md` by focusing on grammar, parsing rules, and the canonical core produced after text
filters and desugaring.

## 1. Overview

PrimeStruct uses a single syntactic form called an **Envelope** for definitions, executions, and bindings:

```
[transform-list] name<template-list>(param-list) { body-list }
```

- `[...]` is a list of transforms (attributes) applied to a node.
- `<...>` is an optional template argument list.
- `(...)` contains parameters or arguments.
- `{...}` contains a definition body or execution body arguments.

The parser accepts convenient surface forms (operator/infix sugar, `if(...) { ... } else { ... }`,
indexing `value[index]`), then rewrites them into a small canonical core before semantic analysis
and IR lowering.

Transform template lists accept one or more type expressions, so generic binding types can be
spelled directly in transform position (e.g. `[array<i32>] values(...)`, `[map<i32, i32>] pairs(...)`).

## 2. Lexical Structure

### 2.1 Identifiers

- Base identifier: `[A-Za-z_][A-Za-z0-9_]*`
- Slash path: `/segment/segment/...` where each segment is a base identifier.
- Identifiers are ASCII only; non-ASCII characters are rejected by the parser.
- Reserved keywords: `mut`, `return`, `include`, `namespace`, `true`, `false`.
- Reserved keywords may not appear as identifier segments in slash paths.

### 2.2 Whitespace and Comments

- Whitespace separates tokens but is otherwise insignificant.
- Statements are separated by whitespace/newlines; there is no line-based parsing.
- Semicolons are rejected by the parser.
- Line comments (`// ...`) and block comments (`/* ... */`) are supported and ignored by the lexer (treated as whitespace).
- Text filters also treat comments as opaque spans; operator/collection rewrites never run inside comments.

### 2.3 Literals

- Integer literals:
  - Optional leading `-` is part of the literal token (e.g. `-1i32`).
  - Decimal and hex forms are accepted (`0x` / `0X` prefix for hex).
  - `i32`, `i64`, `u64` suffixes required (unless the `implicit-i32` text filter is enabled).
  - Examples: `0i32`, `42i64`, `7u64`, `-1i32`, `0xFFu64`.
- Float literals:
  - Optional leading `-` is part of the literal token.
  - Decimal forms with optional exponent `e`/`E` are accepted.
  - `f32`, `f64`, or `f` suffixes are accepted by the lexer; canonical form uses `f32`/`f64`.
  - Examples: `0.5f`, `1.0f32`, `2.0f64`, `1e-3f64`.
- Bool literals: `true`, `false`.
- String literals:
  - Double-quoted or single-quoted surface forms: `"hello"utf8`, `'hi'utf8`.
  - Raw surface form: `"raw text"raw_utf8` / `"raw text"raw_ascii` (no escape processing inside the raw body).
  - Surface form accepts `utf8`/`ascii` suffixes; the `implicit-utf8` filter appends `utf8` to bare strings.
  - **Canonical/bottom-level form uses only `raw_utf8` and `raw_ascii`.** After parsing and escape decoding,
    string literals are normalized to `raw_*` with decoded contents.
  - `ascii`/`raw_ascii` reject non-ASCII bytes.
  - Escape sequences in non-raw strings: `\\n`, `\\r`, `\\t`, `\\\\`, `\\\"`, `\\'`, `\\0`.
  - Any other escape sequence is rejected.

### 2.4 Punctuation Tokens

The lexer emits punctuation tokens for: `[](){}<>,.=;` and `.`. Semicolons are currently rejected by the parser.

## 3. File Structure

A source file is a list of top-level items:

```
file = { include | namespace | definition | execution }
```

### 3.1 Includes

```
include<"/path", version="1.2.0">
```

Includes expand inline before text filters run. Include paths are raw quoted strings without suffixes
and are parsed before the string-literal rules in this spec; treat them as literal paths. Multiple
paths may be listed in a single include. Version selection follows the rules in `docs/PrimeStruct.md`.
Whitespace is allowed between `include` and `<` and around `=` in the `version` attribute. Include paths
and version strings may use either single-quoted or double-quoted string literals.

Include paths may also be written as unquoted slash paths (e.g. `include</std/io>`), which are treated
as logical include paths resolved via the configured include roots.

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
include_entry  = include_path | "version" "=" include_string ;
include_string = quoted_string ;
include_path   = quoted_string | slash_path ;

namespace_decl = "namespace" identifier "{" { top_item } "}" ;

definition     = envelope body_block ;
execution      = envelope exec_body_opt ;

envelope       = transforms_opt name template_opt params ;

transforms_opt = [ "[" transform_list "]" ] ;
transform_list = transform { transform } ;
transform      = identifier template_opt args_opt ;
// Commas between transforms are allowed but not required.
// Transform template args accept a single type expression (not a list).
// Transform argument lists accept identifier, number, or string tokens (no nested envelopes).

template_opt   = [ "<" template_list ">" ] ;
template_list  = type_expr { [ "," ] type_expr } ;
type_expr      = identifier [ "<" type_expr { "," type_expr } ">" ] ;

params         = "(" param_list_opt ")" ;
param_list_opt = [ param { [ "," ] param } ] ;
param          = binding ;

body_block     = "{" stmt_list_opt "}" ;
exec_body_opt  = [ "{" exec_body_list_opt "}" ] ;
exec_body_list_opt = [ form { [ "," ] form } ] ;
body_args_opt  = [ "{" exec_body_list_opt "}" ] ;

stmt_list_opt  = [ stmt { stmt } ] ;
stmt           = binding | form ;

binding        = transforms_opt name template_opt args_opt ;

args_opt       = [ "(" arg_list_opt ")" ] ;
arg_list_opt   = [ arg { [ "," ] arg } ] ;
arg            = named_arg | form ;
named_arg      = identifier "=" form ;

form           = literal
               | name
               | call
               | index_expr
               | block_if_expr ;

call           = name template_opt args_opt body_args_opt ;
name           = identifier | slash_path ;

index_expr     = form "[" form "]" ;

block_if_expr  = "if" "(" form ")" block_if_body "else" block_if_body ;
block_if_body  = "{" stmt_list_opt "}" ;

literal        = int_lit | float_lit | bool_lit | string_lit ;
```

Notes:
- `binding` reuses the Envelope; it becomes a local declaration.
- `execution` is syntactically the same as `definition` but has no body block or has an execution body list.
- `form` includes surface `if` blocks, which are rewritten into canonical calls.
- Transform lists may include optional commas between transforms; trailing commas are not allowed.
- Template, parameter, and argument lists allow optional commas between items; trailing commas are not allowed.
- Execution/body brace lists accept comma-separated or whitespace-separated forms; trailing commas are not allowed.
- Calls may include brace body arguments (e.g. `execute_task(...) { work() work() }` or `block{ ... }`).
- `quoted_string` in include declarations is a raw quoted string without suffixes.

## 5. Desugaring and Canonical Core

The compiler rewrites surface forms into canonical call syntax. The core uses prefix calls:

- Operator and control-flow sugar are applied by text filters and parser sugar before semantic analysis.
  The exact whitespace sensitivity of text filters is defined by the active filter set (see design doc).

- `return(value)` / `return()` are the only return forms; parentheses are always required.
- Control flow: `if(condition, trueBranch, falseBranch)` in canonical call form.
  - Signature: `if(Envelope, Envelope, Envelope)`
  - 1) must evaluate to a boolean (`bool`), either a boolean value or a function returning boolean
  - 2) must be a function or a value, the function return or the value is returned by the `if` if the first arg is `true`
  - 3) must be a function or a value, the function return or the value is returned by the `if` if the first arg is `false`
  - The surface form `if(cond) { ... } else { ... }` requires an `else` block and is accepted in any form position
    (including statement lists and call arguments); it is rewritten into canonical `if(...)` by wrapping the two blocks
    as envelopes.
  - Evaluation is lazy: the condition is evaluated first, then exactly one of the two branch envelopes is evaluated.
- `block{ ... }` is a builtin block form.
  - As a statement, it introduces a local scope.
  - As an expression, it evaluates each form in order and yields the last form’s value; the block must end with a
    non-binding expression.
- Operators are rewritten into calls:
  - `a + b` -> `plus(a, b)`
  - `a - b` -> `minus(a, b)`
  - `a * b` -> `multiply(a, b)`
  - `a / b` -> `divide(a, b)`
  - `a == b` -> `equal(a, b)` etc.
- Method calls:
  - `value.method(args...)` is parsed as a method call and later rewritten to the method namespace form
    `/type/method(value, args...)`. Parentheses are required after the method name.
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
execute_task(count = 2i32) { main() main() }
```

- Executions are call-style constructs that may include a body list of calls.
- Execution bodies may only contain calls (no bindings, no `return`).
- Executions are parsed and validated but not emitted by the current C++ emitter.
- Execution body brace lists accept comma-separated or whitespace-separated calls.
  - The parser accepts any form in brace lists, but semantic validation rejects non-call entries in executions.

## 7. Parameters, Arguments, and Bindings

### 7.1 Bindings

```
[i32 mut] count(1i32)
[string] message("hi"utf8)
```

- A binding is a stack value execution.
- If the binding omits an explicit type, the compiler first tries to infer the type from the
  initializer expression; if inference fails, the type defaults to `int`.
- `mut` marks the binding as writable; otherwise immutable.

### 7.2 Parameters

Parameters use the same binding envelope as locals:

```
main([array<string>] args, [i32] limit(10i32)) { ... }
```

Defaults are limited to literal/pure forms (no name references).

### 7.3 Named Arguments

- Named arguments are allowed in calls and executions.
- Positional arguments must come before named arguments.
- Each parameter may be bound once; duplicates are rejected.
- Named arguments are rejected for builtin calls (e.g. `assign`, `plus`, `count`, `at`, `print*`).
- Parameter defaults do not accept named arguments.

## 8. Collections and Helpers

### 8.1 Arrays

```
array<i32>(1i32, 2i32)
array<i32>{1i32, 2i32}   // surface form
array<i32>[1i32, 2i32]   // surface form
```

Helpers:
- `count(value)` / `value.count()`
- `at(value, index)`
- `at_unsafe(value, index)`

### 8.2 Maps

```
map<i32, i32>(1i32, 2i32)
map<i32, i32>{1i32=2i32}
map<i32, i32>[1i32=2i32]
```

Helpers:
- `count(value)` / `value.count()`
- `at(value, key)`
- `at_unsafe(value, key)`

Map IR lowering is currently limited in VM/native backends (currently numeric/bool keys and values only).

## 9. Effects

- Effects are declared via `[effects(...)]` on definitions or executions.
- Defaults can be supplied by `primec --default-effects`.
- Backends reject unsupported effects.

## 10. Return Semantics

- `return(value)` is explicit and canonical; parentheses are required.
- `return()` is valid for `void` definitions.
- Non-void definitions must return along all control paths.

## 11. Entry Definition

- The compiler entry point is selected with `--entry /path` (default `/main`).
- For VM/native backends, the entry definition may take a single `array<string>` parameter.
  - If a parameter is present, it must be exactly one `array<string>` parameter.
  - The entry parameter does not allow a default value.
- `args.count()` and `count(args)` are supported; indexing `args[index]` is bounds-checked unless
  `at_unsafe` is used.

## 12. Backend Notes (Syntax-Relevant)

- VM/native backends accept a restricted subset of types/operations (see design doc).
- Strings are supported for printing, `count`, and indexing on string literals and string bindings in VM/native backends.
- The C++ emitter supports broader operations and full string handling.

## 13. Error Rules (Selected)

- `at(value, index)` on non-collections is rejected.
- Named arguments after positional ones are rejected.
- Duplicate named arguments are rejected.
- `return` in execution bodies is rejected.
- Semicolons are rejected.
- Transform argument lists may not be empty.
- `if` statement sugar requires an `else` block.
- `if(condition, thenBlock, elseBlock)` requires both branches to produce a value when used in expression position.
