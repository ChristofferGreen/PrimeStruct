# PrimeStruct Syntax Spec (Draft)

This document defines the surface syntax and immediate semantics of PrimeStruct source files. It complements
`docs/PrimeStruct.md` by focusing on grammar, parsing rules, and the canonical core produced after text
transforms and desugaring.

## 1. Overview

PrimeStruct uses a single canonical form called an **Envelope** for definitions and executions in the AST:

```
[transform-list] name<template-list>(param-list) { body-list }
```

- `[...]` is a list of transforms (attributes) applied to a node.
- `<...>` is an optional template argument list.
- `(...)` contains parameters or arguments.
- `{...}` contains a definition body (bindings use `{...}` for initializers).

Executions are call-style forms with mandatory parentheses and no body. In the canonical AST they are envelopes with an implicit empty body:

```
foo()
foo(arg1, arg2)
```

Transforms operate in two phases:
- **Text transforms:** token-level rewrites that run before AST construction. They apply to the entire envelope (transform list, templates, parameters, and body) for the definition/execution they are attached to.
- **Semantic transforms:** AST-level annotations/validation that run after parsing.
Use `text(...)` and `semantic(...)` inside the transform list to force phase placement. Unqualified transforms are
auto-deduced by name; ambiguous names are errors. Text transforms may append additional text transforms to the same
node, which run after the current transform.

The parser accepts convenient surface forms (operator/infix sugar, `if(...) { ... } else { ... }`,
indexing `value[index]`), then rewrites them into a small canonical core before semantic analysis
and IR lowering.
Canonical/bottom-level syntax requires explicit return transforms and literal suffixes; surface syntax may omit
them and rely on inference/transforms to insert the canonical forms. When `--no-transforms` is active, source is
expected to already be in canonical form.

Transform template lists accept one or more envelope entries, so generic binding envelopes can be
spelled directly in transform position (e.g. `[array<i32>] values{...}`, `[map<i32, i32>] pairs{...}`).

The CLI supports `--dump-stage=pre_ast|ast|ir` to emit the text after include expansion/text transforms,
the parsed AST, or the IR view respectively. `--dump-stage` exits before lowering/emission. Text
transforms are configured via `--text-transforms=<list>` (the default list is `collections`, `operators`,
`implicit-utf8`), semantic transforms via `--semantic-transforms=<list>`, and `--transform-list=<list>` is
an auto-deducing shorthand that routes each transform name to its declared phase (text or semantic);
ambiguous names are errors. `--no-text-transforms`, `--no-semantic-transforms`, or `--no-transforms`
disable transforms.

Use `--emit=ir` to write serialized PSIR bytecode to the output path after semantic validation.

## 2. Lexical Structure

### 2.1 Identifiers

- Base identifier: `[A-Za-z_][A-Za-z0-9_]*`
- Slash path: `/segment/segment/...` where each segment is a base identifier.
- Identifiers are ASCII only; non-ASCII characters are rejected by the parser.
- Reserved keywords: `mut`, `return`, `include`, `import`, `namespace`, `true`, `false`, `if`, `else`, `loop`, `while`, `for`.
- Reserved keywords may not appear as identifier segments in slash paths.

### 2.2 Whitespace and Comments

- Whitespace separates tokens but is otherwise insignificant.
- Statements are separated by whitespace/newlines; there is no line-based parsing.
- Commas and semicolons are treated as whitespace separators and are ignored by the parser outside numeric literals.
- Line comments (`// ...`) and block comments (`/* ... */`) are supported and ignored by the lexer (treated as whitespace).
- Text transforms also treat comments as opaque spans; operator/collection rewrites never run inside comments.

### 2.3 Literals

- Integer literals:
  - Optional leading `-` is part of the literal token (e.g. `-1i32`).
  - Decimal and hex forms are accepted (`0x` / `0X` prefix for hex).
  - `i32`, `i64`, `u64` suffixes required unless `implicit-i32` is enabled (default).
  - Commas may appear between digits in the integer part as digit separators and are ignored for value.
  - Examples: `0i32`, `42i64`, `7u64`, `-1i32`, `0xFFu64`.
- Float literals:
  - Optional leading `-` is part of the literal token.
  - Decimal forms with optional exponent `e`/`E` are accepted.
  - Commas may appear between digits in the integer part as digit separators; commas are not allowed in the fractional or exponent parts.
  - `f32` or `f64` suffixes are accepted; when omitted, the literal defaults to `f32`. Canonical form uses `f32`/`f64`.
  - Single-letter `f` suffix is accepted and treated as `f32`.
  - Examples: `0.5f32`, `1.0f32`, `2.0f64`, `1e-3f64`, `1f`, `1.5f`.

Type aliases:
- `int` and `float` are currently fixed to `i32` and `f32` in the compiler. Prefer explicit widths for deterministic behavior; future backends may widen these aliases.
- Software numeric envelopes `integer`, `decimal`, and `complex` are reserved but not supported yet. Using them as binding/return types, template arguments, or `convert<T>` targets is a semantic error (`software numeric types are not supported yet`).
- Bool literals: `true`, `false`.
- String literals:
  - Double-quoted strings process escapes unless a raw suffix is used.
  - Single-quoted strings are raw and do not process escapes; `raw_utf8` / `raw_ascii` also force raw.
  - Surface form accepts `utf8`/`ascii`/`raw_utf8`/`raw_ascii` suffixes; the `implicit-utf8` text transform appends `utf8` to bare strings.
  - **Canonical/bottom-level form uses double-quoted strings with normalized escapes and an explicit `utf8`/`ascii` suffix.**
  - `ascii` rejects non-ASCII bytes.
  - Escape sequences in double-quoted strings: `\\n`, `\\r`, `\\t`, `\\\\`, `\\\"`, `\\'`, `\\0`.
  - Any other escape sequence is rejected.
  - To include a single quote, use a double-quoted string or a raw suffix.
  - VM/native backends only support `count`/`at`/`at_unsafe` on string literals or bindings initialized from literals; argv-derived strings are print-only.

### 2.4 Punctuation Tokens

The lexer emits punctuation tokens for: `[](){}<>,.=;` and `.`. Commas and semicolons are treated as whitespace
separators by the parser outside numeric literals.

## 3. File Structure

A source file is a list of top-level items:

```
file = { include | import | namespace | definition | execution }
```

### 3.1 Includes

```
include<"/path", version="1.2.0">
```

Includes expand inline before text transforms run. Include paths are raw quoted strings without suffixes
and are parsed before the string-literal rules in this spec; treat them as literal paths. Multiple
paths may be listed in a single include. Version selection follows the rules in `docs/PrimeStruct.md`.
Whitespace is allowed between `include` and `<` and around `=` in the `version` attribute. Include paths
and version strings may use either single-quoted or double-quoted string literals.

Include paths may also be written as unquoted slash paths (e.g. `include</std/io>`), which are treated
as logical include paths resolved via the configured include roots. Paths (or any parent folders)
prefixed with `_` are private and rejected by the include resolver.

### 3.2 Imports

```
import /math/*
import /math/sin /math/pi
import /ui/*, /util/*
```

Imports are compile-time namespace aliases. `import /foo/*` contributes the immediate children of `/foo`
to the root namespace (e.g., `import /math/*` allows `sin(...)` as shorthand for `/math/sin(...)`), while
`import /foo/bar` aliases a single definition or builtin by its final segment. Imports must appear at the
top level (not inside `namespace` blocks).

`import /foo` is shorthand for `import /foo/*` (except `/math`, which is unsupported without a wildcard or
explicit name).

Import paths must resolve to a definition or builtin; unknown import paths are errors.

`import /math` (without a wildcard or explicit name) is not supported; use `import /math/*` or
`import /math/<name>` instead.

Imports are resolved after includes expand, and the same syntax is accepted by `primec` and `primevm`.

### 3.3 Namespace Blocks

```
namespace foo {
  ...
}
```

Namespaces prefix enclosed definitions/executions with `/foo`. Namespace blocks may be reopened.

## 4. Grammar (EBNF-style)

This grammar describes surface syntax before text-transform rewriting.

```
file           = { top_item } ;

top_item       = include_decl | import_decl | namespace_decl | definition | execution ;

include_decl   = "include" "<" include_list ">" ;
include_list   = include_entry { [ "," | ";" ] include_entry } ;
include_entry  = include_path | "version" "=" include_string ;
include_string = quoted_string ;
include_path   = quoted_string | slash_path ;

import_decl    = "import" import_list ;
import_list    = import_path { [ "," | ";" ] import_path } ;
import_path    = slash_path [ "/*" ] ;

namespace_decl = "namespace" identifier "{" { top_item } "}" ;

definition     = envelope body_block ;
execution      = transforms_opt call ;

envelope       = transforms_opt name template_opt params ;

transforms_opt            = [ "[" transform_list "]" ] ;
transform_list            = transform_entry { transform_entry } ;
transform_entry           = transform_item | text_group | semantic_group ;
text_group                = "text" "(" text_transform_list ")" ;
semantic_group            = "semantic" "(" semantic_transform_list ")" ;
text_transform_list       = text_transform_item { [ "," | ";" ] text_transform_item } ;
semantic_transform_list   = semantic_transform_item { [ "," | ";" ] semantic_transform_item } ;
transform_item            = identifier template_opt args_opt ;
text_transform_item       = identifier template_opt text_args_opt ;
semantic_transform_item   = identifier template_opt args_opt ;
text_args_opt             = [ "(" text_arg_list_opt ")" ] ;
text_arg_list_opt         = [ text_arg { [ "," | ";" ] text_arg } ] ;
text_arg                  = identifier | literal ;
// Commas and semicolons between transforms are allowed but not required.
// Commas and semicolons in template/parameter/argument lists are optional separators with no semantic meaning.
// Transform template args accept one or more envelope entries.
// Text transform argument lists accept only identifiers or literals (no nested envelopes).
// Semantic transform argument lists accept full forms.
// The special transform groups `text(...)` and `semantic(...)` accept a list of transforms instead.

template_opt   = [ "<" template_list ">" ] ;
template_list  = envelope_ref { [ "," | ";" ] envelope_ref } ;
// Commas and semicolons inside template lists are optional separators with no semantic meaning.
envelope_ref   = identifier [ "<" envelope_ref { [ "," | ";" ] envelope_ref } ">" ] ;

params         = "(" param_list_opt ")" ;
param_list_opt = [ param { [ "," | ";" ] param } ] ;
param          = binding ;

body_block     = "{" stmt_list_opt "}" ;

stmt_list_opt  = [ stmt { stmt } ] ;
stmt           = binding | form ;

binding        = transforms_opt name template_opt binding_init_opt ;

binding_init_opt = [ value_block ] ;

args           = "(" arg_list_opt ")" ;
args_opt       = [ "(" arg_list_opt ")" ] ;
arg_list_opt   = [ arg { [ "," | ";" ] arg } ] ;
arg            = labeled_arg | form ;
labeled_arg    = "[" identifier "]" form ;

form           = literal
               | name
               | call
               | execution
               | brace_ctor
               | index_form
               | definition
               | if_form
               | loop_form
               | while_form
               | for_form ;

call           = name template_opt args body_args_opt ;
name           = identifier | slash_path ;

brace_ctor     = name template_opt value_block ;

index_form     = form "[" form "]" ;

if_form        = "if" "(" form ")" block_if_body "else" block_if_body ;
block_if_body  = "{" stmt_list_opt "}" ;

loop_form      = transforms_opt "loop" "(" form ")" block_if_body ;
while_form     = transforms_opt "while" "(" form ")" block_if_body ;
for_slot       = binding | form ;
for_form       = transforms_opt "for" "(" for_slot for_slot for_slot ")" block_if_body ;

body_args_opt  = [ "{" stmt_list_opt "}" ] ;
value_block    = "{" stmt_list_opt "}" ;

literal        = int_lit | float_lit | bool_lit | string_lit ;
```

Notes:
- `binding` reuses the Envelope; it becomes a local declaration.
- `execution` is a call-style form (optionally prefixed by transforms) with mandatory parentheses and no body block. Definitions require a body block.
  - AST mapping: `foo()` parses as a call-style execution and lowers to the canonical envelope `foo() { }` with an implicit empty body.
- `form` includes surface `if` blocks, which are rewritten into canonical calls.
- `execution` is valid anywhere a form is allowed, so transform-prefixed calls can appear inside bodies and argument lists.
- Commas and semicolons are treated as whitespace separators in transform lists and item lists; trailing separators are allowed.
- Example mixed separators:
  - `array<i32; i64  u64>`
  - `call(a, b; c  d)`
  - `[text(operators; collections, implicit-utf8)]`
- Calls may include a trailing body block (`foo(args) { ... }`) that becomes a list of body arguments on the call envelope.
- `loop`, `while`, and `for` are special surface forms that accept a body block and are rewritten into canonical calls.
- Canonical control-flow calls use definition envelopes as arguments (e.g., `if(cond, then() { ... }, else() { ... })`).
  Envelope names in this position are for readability only; any name is accepted and ignored by the compiler.
- `loop`, `while`, and `for` may be prefixed by transforms; `[shared_scope]` marks the loop body scope as shared across iterations, initializing loop-body bindings once before the loop begins and keeping them alive for the loop duration.
- Text transforms accept only identifier/literal arguments; semantic transforms may accept full forms. If a text transform is given a non-simple argument, it is a diagnostic.
- `brace_ctor` is a constructor form: `Type{...}` in value positions evaluates the value block and passes its value to the constructor. If the block executes `return(value)`, that value is used; otherwise the last item is used. In statement position, `name{...}` is parsed as a binding.
- `block()` with a trailing body block is allowed in any form position; `block{...}` is invalid and parsed as a binding or brace constructor.
- `quoted_string` in include declarations is a raw quoted string without suffixes.

## 5. Desugaring and Canonical Core

The compiler rewrites surface forms into canonical call syntax. The core uses prefix calls:

- Operator and control-flow sugar are applied by text transforms and parser sugar before semantic analysis.
  The exact whitespace sensitivity of text transforms is defined by the active transform set (see design doc).

- `return(value)` / `return()` are the only return forms; parentheses are always required.
- Control flow: `if(condition, then() { ... }, else() { ... })` in canonical call form.
  - Signature: `if(Envelope, Envelope, Envelope)`
  - 1) must evaluate to a boolean (`bool`), either a boolean value or a function returning boolean
  - 2) must be a definition envelope; its body yields the `if` result when the condition is `true`
  - 3) must be a definition envelope; its body yields the `if` result when the condition is `false`
  - The surface form `if(cond) { ... } else { ... }` requires an `else` block and is accepted in any form position
    (including statement lists and call arguments); it is rewritten into canonical `if(...)` by wrapping the two bodies
    as definition envelopes (`then() { ... }` / `else() { ... }`).
  - Evaluation is lazy: the condition is evaluated first, then exactly one of the two definition bodies is evaluated.
- Loops:
  - `loop(count) { ... }` rewrites to `loop(count, do() { ... })`.
  - `while(cond) { ... }` rewrites to `while(cond, do() { ... })`.
  - `for(init cond step) { ... }` rewrites to `for(init, cond, step, do() { ... })`; commas/semicolons are optional separators.
  - Envelope names are ignored (`do() { ... }` and `body() { ... }` are equivalent in the loop body position).
  - Bindings are allowed in any `for` slot; `cond` must evaluate to `bool`.
- `repeat(count) { ... }` is a statement-only builtin; `count` accepts integer or `bool`, and non-positive counts skip the body.
- Operators are rewritten into calls:
  - `a + b` -> `plus(a, b)`
  - `a - b` -> `minus(a, b)`
  - `a * b` -> `multiply(a, b)`
  - `a / b` -> `divide(a, b)`
  - `a = b` -> `assign(a, b)`
  - `++a` / `a++` -> `increment(a)`
  - `--a` / `a--` -> `decrement(a)`
  - `a == b` -> `equal(a, b)`
  - `a != b` -> `not_equal(a, b)`
  - `a > b` -> `greater_than(a, b)`
  - `a < b` -> `less_than(a, b)`
  - `a >= b` -> `greater_equal(a, b)`
  - `a <= b` -> `less_equal(a, b)`
  - `a && b` -> `and(a, b)`
  - `a || b` -> `or(a, b)`
  - `!a` -> `not(a)`
  - `&a` -> `location(a)`
  - `*a` -> `dereference(a)`
- Collection literals (via the `collections` text transform) rewrite to constructor calls:
  - `array<T>{...}` / `array<T>[...]` -> `array<T>(...)`
  - `vector<T>{...}` / `vector<T>[...]` -> `vector<T>(...)`
  - `map<K, V>{...}` / `map<K, V>[...]` -> `map<K, V>(...)`
  - Map literals accept `key = value` pairs as shorthand for alternating arguments (e.g., `map<i32, i32>{1i32=2i32}` -> `map<i32, i32>(1i32, 2i32)`).
- `/math/*` builtins include the core set (`abs`, `sign`, `min`, `max`, `clamp`, `saturate`, `lerp`, `pow`, `sqrt`, `sin`, `cos`, etc.) plus `floor`, `ceil`, `round`, `trunc`, `fract`, `is_nan`, `is_inf`, and `is_finite`.
- PathSpace builtins (`notify(path, payload)`, `insert(path, payload)`, `take(path)`) live in the root namespace and act as PathSpace integration hooks.
- Method calls:
  - `value.method(args...)` is parsed as a method call and later rewritten to the method namespace form
    `/<envelope>/method(value, args...)`, where `<envelope>` is the envelope name associated with `value`.
    Parentheses are required after the method name.
  - Method calls may include template arguments: `value.method<T>(args...)`.
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
- Transform list on the definition declares return envelope and effects.

### 6.2 Executions

```
execute_task([count] 2i32)
```

- Executions are call-style constructs with mandatory parentheses and no body.
- Executions may be prefixed with a transform list (e.g., `[effects(io_out)] log()`).
- Executions are parsed and validated but not emitted by the current C++ emitter.

## 7. Parameters, Arguments, and Bindings

### 7.1 Bindings

```
[i32 mut] count{1i32}
[string] message{"hi"utf8}
```

- A binding is a stack value execution.
- Binding initializers are value blocks. The block is evaluated left-to-right; its resulting value (last item
  or `return(value)`) becomes the initializer value. Use explicit constructor calls when you need to pass multiple
  arguments (e.g., `[T] name{ T(arg1, arg2) }`). Commas and semicolons are treated as whitespace separators.
  A brace constructor form (`Type{...}`) is allowed in value positions (e.g., `bool{35}` in arguments
  or returns) and passes the block’s resulting value to the constructor.
- If the binding omits an explicit envelope annotation, the compiler first tries to infer the envelope from the
  initializer form; if inference fails, the envelope defaults to `int`.
- `mut` marks the binding as writable; otherwise immutable.

### 7.2 Parameters

Parameters use the same binding envelope as locals:

```
main([array<string>] args, [i32] limit{10i32}) { ... }
```

Defaults are limited to literal/pure forms (no name references).

### 7.3 Labeled Arguments

- Labeled arguments are allowed in calls and executions.
- Syntax: `[param] form` in an argument list.
- Labeled arguments may appear in any order and may be interleaved with positional arguments.
- Positional arguments fill remaining parameters in declaration order, skipping labeled entries.
- Each parameter may be bound once; duplicates or unknown labels are rejected.
- Labeled arguments are rejected for builtin calls (e.g. `assign`, `plus`, `count`, `at`, `print*`).

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

### 8.2 Vectors

```
vector<i32>(1i32, 2i32)
vector<i32>{1i32 2i32}
vector<i32>[1i32 2i32]
```

Vectors are resizable sequences. Construction is variadic; zero or more arguments are accepted.
Helpers:
- `count(value)` / `value.count()`
- `at(value, index)` / `at_unsafe(value, index)`
- `push(value, item)` / `pop(value)`
- `reserve(value, capacity)` / `capacity(value)`
- `clear(value)` / `remove_at(value, index)` / `remove_swap(value, index)`

Mutation helpers (`push`, `pop`, `reserve`, `clear`, `remove_at`, `remove_swap`) are statement-only
(not valid in expression contexts).

### 8.3 Maps

```
map<i32, i32>(1i32, 2i32)
map<i32, i32>{1i32 2i32}
map<i32, i32>[1i32 2i32]
```

Map literals supply alternating key/value forms; an odd number of entries is a diagnostic.

Helpers:
- `count(value)` / `value.count()`
- `at(value, key)`
- `at_unsafe(value, key)`

Map IR lowering is currently limited in VM/native backends: numeric/bool values only, with string keys allowed when they come from string literals or bindings backed by literals.

## 9. Effects

- Effects are declared via `[effects(...)]` on definitions or executions.
- Defaults can be supplied by `primec --default-effects` (the compiler enables `io_out` by default unless set to `none`).
- Backends reject unsupported effects.
  - Execution effects must be a subset of the enclosing definition’s active effects; otherwise the compiler emits a diagnostic.
  - Recognized v1 capabilities: `io_out`, `io_err`, `heap_alloc`, `global_write`, `asset_read`, `asset_write`,
    `pathspace_notify`, `pathspace_insert`, `pathspace_take`, `render_graph`.

## 10. Return Semantics

- `return(value)` is explicit and canonical; parentheses are required.
- `return()` is valid for `void` definitions.
- Non-void definitions must return along all control paths.

## 11. Entry Definition

- The compiler entry point is selected with `--entry /path` (default `/main`).
- The entry definition may take either zero parameters or a single `array<string>` parameter.
  - If a parameter is present, it must be exactly one `array<string>` parameter.
  - The entry parameter does not allow a default value.
- `args.count()` and `count(args)` are supported; indexing `args[index]` is bounds-checked unless
  `at_unsafe` is used.

## 12. Backend Notes (Syntax-Relevant)

- VM/native backends accept a restricted subset of envelopes/operations (see design doc).
- Strings are supported for printing, `count`, and indexing on string literals and string bindings in VM/native backends.
- The C++ emitter supports broader operations and full string handling.

## 13. Error Rules (Selected)

- `at(value, index)` on non-collections is rejected.
- Duplicate labeled arguments are rejected.
- Unknown labeled arguments are rejected.
- Labeled arguments are rejected for builtin calls.
- `return` is valid inside definition bodies and value blocks. In a value block, `return(value)` returns from the block and yields its value.
- Transform argument lists may not be empty.
- `if` statement sugar requires an `else` block.
- `if(condition, then() { ... }, else() { ... })` requires both branches to produce a value when used in value position.
- `if` / `while` / `for` conditions must evaluate to `bool` (or a function returning `bool`).
- `loop` counts must be integer envelopes; negative counts are errors.
- `loop` / `while` / `for` require a body envelope (e.g., `do() { ... }`).
- `and` / `or` / `not` accept only `bool` operands; use `bool{value}` or `convert<bool>(value)` to coerce integers.
