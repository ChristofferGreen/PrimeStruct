# PrimeStruct Syntax Spec (Stable)

This document defines the surface syntax and immediate semantics of PrimeStruct source files. It complements
`docs/PrimeStruct.md` by focusing on grammar, parsing rules, and the canonical core produced after text
transforms and desugaring.

Status: Stable as of March 6, 2026. Changes require synchronized updates to the design doc and conformance
tests.

## 1. Overview

PrimeStruct uses a single canonical form called an **Envelope** for definitions and executions in the AST:

```
[transform-list] name<template-list>(param-list) { body-list }
```

- `[...]` is a list of transforms (attributes) applied to a node.
- `<...>` is an optional template argument list.
- `(...)` contains parameters or arguments.
- `{...}` contains a definition body (bindings use `{...}` for initializers).

Executions are call-style forms with mandatory parentheses and no body. In the canonical AST they are envelopes with an
implicit empty body. Executions model behavior, not value construction:

```
foo()
foo(arg1, arg2)
```

Surface definitions also accept a post-parameter transform placement:

```
name<template-list>(param-list) [transform-list] { body-list }
```

This is definition-only sugar. The parser normalizes it to canonical prefix form before semantic validation and IR
lowering. `name[]() { ... }` is not valid syntax.

Transforms operate in two phases:
- **Text transforms:** token-level rewrites that run before AST construction. They apply to the entire envelope
  (transform list, templates, parameters, and body) for the definition/execution they are attached to.
- **Semantic transforms:** AST-level annotations/validation that run after parsing.
- **Definition AST-transform hooks:** user-authored hook definitions may be marked with `[ast]` and referenced from
  another definition's transform list. Metadata-only hooks resolve and record the hook path; executable
  `FunctionAst` hooks may rewrite the attached definition through the narrow v1 helper surface.
Use `text(...)` and `semantic(...)` inside the transform list to force phase placement. Unqualified transforms are
auto-deduced by name; ambiguous names are errors. Text transforms may append additional text transforms to the same
node, which run after the current transform.

User-authored AST hooks may use a metadata-only declaration:

```
[ast return<void>]
trace_calls() {
}

[trace_calls return<int>]
main() {
  return(1i32)
}
```

Executable v1 hooks use one `FunctionAst` input and return a replacement
`FunctionAst` through the syntax-owned `ct-eval ast-transform adapter`. The
adapter currently maps only the checked helper
`replace_body_with_return_i32(fn, value)` to
`/ct_eval/replace_body_with_return_i32` and rejects unknown helper targets or
contradictory helper inputs deterministically:

```
[ast return<FunctionAst>]
make_seven([FunctionAst] fn) {
  return(replace_body_with_return_i32(fn, 7i32))
}

[make_seven return<int>]
main() {
  return(1i32)
}
```

Imported hooks must be `public`. A definition attaches a visible hook by bare name, slash path, or an imported alias;
resolution records the hook's full path on the transform metadata, rejects ambiguous imports and private imported hooks,
and rejects `text(hook_name)` because AST hooks are semantic-phase metadata. Executable hooks are compile-time only and
are removed from the runtime program after the touched definition is rewritten. The canonical checked-in module pair is
`examples/4.Transforms/trace_calls_transform.prime` plus
`examples/4.Transforms/trace_calls_consumer.prime`.

The parser accepts convenient surface forms (operator/infix sugar, `if(...) { ... } else { ... }`,
indexing `value[index]`, collection method forms like `value.push(x)`), then rewrites them into a small canonical core
before semantic analysis
and IR lowering.
Canonical/bottom-level syntax requires explicit return transforms and literal suffixes; surface syntax may omit
them and rely on inference/transforms to insert the canonical forms. `auto` is permitted in signatures; in
parameter/return positions it introduces implicit template parameters that are resolved per instantiation, and
canonical form after monomorphisation carries explicit envelopes. The base-level core passed to lowering contains
no templates or `auto`. When `--no-transforms` is active, source is expected to already be in canonical form.

Struct definitions may omit an empty parameter list in all language levels (including Concrete); `[struct] A { ... }`
is treated as `[struct] A() { ... }`. Helpers inside struct bodies receive an implicit `this` (`Reference<Self>`)
unless marked `[static]`; `[static]` disables the implicit `this` and method-call sugar.

Language levels (0.Concrete → 3.Surface) follow the same ordering as the compiler pipeline:
- **0.Concrete:** canonical envelopes only, no text transforms, no templates, no `auto`. Definition transforms use
  prefix placement only (`[transforms] name(...) { ... }`).
- **1.Template:** canonical envelopes plus explicit templates.
- **2.Inference:** canonical envelopes plus `auto`/omitted envelopes (implicit template inference).
- **3.Surface:** surface syntax + text transforms that rewrite into canonical forms, including `name(...) [transforms] {
  ... }` definition sugar.

Import expansion runs before type checking and template inference. All definitions/executions live in a single
compilation unit after imports are expanded, so implicit-template inference may use call sites anywhere in the
expanded source; there are no module boundaries.

Transform template lists accept one or more envelope entries, so generic binding envelopes can be
spelled directly in transform position (e.g. `[array<i32>] values{...}`, `[map<i32, i32>] pairs{...}`).
Trait constraints are also encoded as transforms on definitions (e.g. `[Additive<i32>]`, `[Indexable<array<i32>,
i32>]`);
they follow the same transform-list grammar and are validated during semantic analysis.

The CLI supports `--dump-stage=pre_ast|ast|ast-semantic|ir` to emit the text after import expansion/text transforms,
the parsed AST, the post-semantics canonicalized AST (after semantic rewrites/inference), or the IR view respectively.
`--dump-stage` exits before lowering/emission. Text
transforms are configured via `--text-transforms=<list>` (the default list is `collections`, `operators`,
`implicit-utf8`), semantic transforms via `--semantic-transforms=<list>` (default: `single_type_to_return`), and
`--transform-list=<list>` is
an auto-deducing shorthand that routes each transform name to its declared phase (text or semantic);
ambiguous names are errors. `--no-text-transforms`, `--no-semantic-transforms`, or `--no-transforms`
disable transforms.

Planned dump-stage evolution for the semantics/lowering boundary:
- `ast-semantic` remains the current post-semantics public dump and is still AST-shaped.
- `semantic-product` now sits between `ast-semantic` and `ir`, exposing lowering-facing resolved facts without
  requiring tooling to infer them from the canonicalized AST.
- `ir` remains the first backend-facing dump after that semantic-product boundary.

Current semantic-product dump requirements:
- Deterministic ordering for modules, definitions, bindings, and diagnostics.
- Direct exposure of lowering-facing facts such as resolved call targets, binding/result types, effects/capabilities,
  struct/layout metadata, and provenance handles.
- No attempt to replace syntax-oriented dumps; syntax-faithful structure remains owned by `ast` / `ast-semantic`.

Planned pipeline-facing semantic-product conformance requirements:
- Pipeline-facing tests should verify the relationship between `ast-semantic`, the semantic-product stage, and
  `ir`, rather than treating any one of them as a substitute for the others.
- Semantic-product golden tests should stay narrow and formatter-focused; broader compile-pipeline C++/VM/native cases
  should prove that lowering consumes published semantic-product facts.
- Lowering-facing snapshot assertions that are currently expressed through AST-shaped helpers should migrate either to
  the semantic-product dump or to those pipeline-facing conformance cases.

Use `--emit=ir` to write serialized PSIR bytecode to the output path after semantic validation.

## 2. Lexical Structure

### 2.1 Identifiers

- Base identifier: `[A-Za-z_][A-Za-z0-9_]*`
- Slash path: `/segment/segment/...` where each segment is a base identifier.
- Identifiers are ASCII only; non-ASCII characters are rejected by the parser.
- Reserved keywords: `auto`, `mut`, `return`, `import`, `namespace`, `true`, `false`, `if`, `else`, `loop`, `while`,
  `for`.
- Reserved keywords may not appear as identifier segments in slash paths.

### 2.2 Whitespace and Comments

- Whitespace separates tokens but is otherwise insignificant.
- Statements are separated by whitespace/newlines; there is no line-based parsing.
- Commas and semicolons are treated as whitespace separators and are ignored by the parser outside numeric literals.
- Line comments (`// ...`) and block comments (`/* ... */`) are supported and ignored by the lexer (treated as
  whitespace).
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
  - Commas may appear between digits in the integer part as digit separators; commas are not allowed in the fractional
    or exponent parts.
  - `f32` or `f64` suffixes are accepted; when omitted, the literal defaults to `f32`. Canonical form uses `f32`/`f64`.
  - Single-letter `f` suffix is accepted and treated as `f32`.
  - Examples: `0.5f32`, `1.0f32`, `2.0f64`, `1e-3f64`, `1f`, `1.5f`.

Type aliases:
- No implicit width aliases; use explicit `i32`, `i64`, `u64`, `f32`, `f64`.
- Software numeric envelopes `integer`, `decimal`, and `complex` are supported in the language spec:
  - `integer` is arbitrary-precision signed integer with exact arithmetic (no overflow/wrapping).
  - `decimal` is arbitrary-precision floating with fixed 256-bit precision and deterministic round-to-nearest-even
    semantics.
  - `complex` is a pair of `decimal` values (`real`, `imag`) using the same 256-bit precision and rounding rules.
  - These envelopes are software-only; GPU backends reject them, and the current VM/native subset excludes them.
  - Mixed ops between software envelopes and fixed-width envelopes are rejected; use explicit conversion syntax
    (`T{value}`). Conversions are total and deterministic: float -> integer truncates toward zero then clamps to the
    target range (NaN -> 0, +Inf -> max, -Inf -> min), and integer width changes use sign/zero extension on widening and
    two's-complement truncation on narrowing.
  - Naming rationale: `integer`/`decimal`/`complex` model mathematical numbers and deterministic software arithmetic.
    Fixed-width envelopes (`i32`, `i64`, `u64`, `f32`, `f64`) model hardware behavior (modular integer arithmetic and
    IEEE-754 rounding), so they intentionally do not use the math-aligned names.
- Bool literals: `true`, `false`.
- String literals:
  - Double-quoted strings process escapes unless a raw suffix is used.
  - Single-quoted strings are raw and do not process escapes; `raw_utf8` / `raw_ascii` also force raw.
  - Surface form accepts `utf8`/`ascii`/`raw_utf8`/`raw_ascii` suffixes; the `implicit-utf8` text transform appends
    `utf8` to bare strings.
  - **Canonical/bottom-level form uses double-quoted strings with normalized escapes and an explicit `utf8`/`ascii`
    suffix.**
  - `ascii` rejects non-ASCII bytes.
  - Escape sequences in double-quoted strings: `\\n`, `\\r`, `\\t`, `\\\\`, `\\\"`, `\\'`, `\\0`.
  - Any other escape sequence is rejected.
  - To include a single quote, use a double-quoted string or a raw suffix.
  - VM/native backends only support `count`/`at`/`at_unsafe` on string literals or bindings initialized from literals;
    argv-derived strings are print-only.

### 2.4 Punctuation Tokens

The lexer emits punctuation tokens for: `[](){}<>,.=;` and `.`. Commas and semicolons are treated as whitespace
separators by the parser outside numeric literals.

## 3. File Structure

A source file is a list of top-level items:

```
file = { import | namespace | definition | execution }
```

### 3.1 Imports

```
import<"/path", version="1.2.0">
import /std/math/*
import /std/math/sin /std/math/pi
import /ui/*, /util/*
```

`import` brings the public symbols of the referenced path(s) into the current namespace. It also ensures
the referenced sources are loaded into the compilation unit before transforms and inference.
Imports must appear at the top level (not inside `namespace` blocks).
Legacy `include<...>` syntax is not supported. Import roots must be configured
with `--import-path` (or `-I`); `--include-path` is not supported.

- `import<...>` loads source packages from the import path. Entries may be quoted strings or slash paths.
  Multiple paths may be listed in a single import; version selection follows `docs/PrimeStruct.md` and is optional.
  Whitespace is allowed between `import` and `<` and around `=` in the `version` attribute. Paths (or any parent
  folders) prefixed with `_` are private and rejected by the import resolver.
- `import /foo/*` brings the immediate **public** children of `/foo` into the root namespace (e.g., `import /std/math/*`
  allows `sin(...)` as shorthand for `/std/math/sin(...)`).
- `import /foo/bar` brings a single **public** definition or builtin by its final segment; importing a non-public
  definition is a diagnostic.
- `import /foo` is shorthand for `import /foo/*` (except `/std/math`, which is unsupported without a wildcard or
  explicit name).
- `import /std/math` (without a wildcard or explicit name) is not supported; use `import /std/math/*` or
  `import /std/math/<name>` instead.

Definitions are private by default; add `[public]` to a definition to make it importable. Private definitions remain
callable within the same compilation unit; visibility only affects imports.

Unknown import paths are errors. Imports are resolved after import expansion, and the same syntax is accepted by
`primec`
and `primevm`.

### 3.2 Namespace Blocks

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

top_item       = import_decl | namespace_decl | definition | execution ;

import_decl          = "import" ( import_source_list | import_path_list ) ;
import_source_list   = "<" import_source_entry { [ "," | ";" ] import_source_entry } ">" ;
import_source_entry  = import_source_path | "version" "=" import_source_string ;
import_source_string = quoted_string ;
import_source_path   = quoted_string | slash_path ;
import_path_list     = import_path { [ "," | ";" ] import_path } ;
import_path          = slash_path [ "/*" ] ;

namespace_decl = "namespace" identifier "{" { top_item } "}" ;

definition     = canonical_definition | post_params_definition ;
execution      = transforms_opt call ;

canonical_definition = envelope body_block ;
post_params_definition = name template_opt params "[" transform_list "]" body_block ;

envelope       = transforms_opt name template_opt params_opt ;
params_opt     = params | /* empty */ ;

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
// Definitions may omit an empty parameter list; the parser treats it as (). Executions still require parentheses.
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

if_form        = "if" "(" form ")" block_if_body [ "else" block_if_body ] ;
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
- `execution` is a call-style form (optionally prefixed by transforms) with mandatory parentheses and no body block.
  Definitions require a body block.
  - AST mapping: `foo()` parses as a call-style execution and lowers to the canonical envelope `foo() { }` with an
    implicit empty body.
- Definitions may use post-parameter transform placement (`name(...) [transforms] { ... }`) in surface syntax. The
  parser rewrites this to canonical prefix form (`[transforms] name(...) { ... }`) before semantic validation and
  lowering.
- Post-parameter transform placement is definition-only and requires the transform list to be followed by a definition
  body. `name[]() { ... }` is rejected.
- `form` includes surface `if` blocks, which are rewritten into canonical calls.
- Surface `if(cond) { ... }` (without `else`) is allowed only in statement position; using it where a value/form result
  is required is a diagnostic.
- `execution` is valid anywhere a form is allowed, so transform-prefixed calls can appear inside bodies and argument
  lists.
- Definition order does not affect name resolution: calls may reference definitions that appear later in the same file
  or namespace. Resolution runs after import expansion and namespace expansion; unresolved names are diagnostics.
- Return transforms may name struct definitions; functions can return struct values, and the return type may be inferred
  from struct constructor/value returns or `return<auto>`.
- Commas and semicolons are treated as whitespace separators in transform lists and item lists; trailing separators are
  allowed.
- Example mixed separators:
  - `array<i32; i64  u64>`
  - `call(a, b; c  d)`
  - `[text(operators; collections, implicit-utf8)]`
- Calls may include a trailing body block (`foo(args) { ... }`) that becomes a list of body arguments on the call
  envelope.
- `loop`, `while`, and `for` are special surface forms that accept a body block and are rewritten into canonical calls.
- Canonical control-flow calls use definition envelopes as arguments (e.g., `if(cond, then() { ... }, else() { ... })`).
  Envelope names in this position are for readability only; any name is accepted and ignored by the compiler.
- `loop`, `while`, and `for` may be prefixed by transforms; `[shared_scope]` marks the loop body scope as shared across
  iterations, initializing loop-body bindings once before the loop begins and keeping them alive for the loop duration.
- Text transforms accept only identifier/literal arguments; semantic transforms may accept full forms. If a text
  transform is given a non-simple argument, it is a diagnostic.
- `brace_ctor` is the only constructor form. `Type{...}` in value positions maps positional/labeled entries to
  constructor fields. `Type(...)` is an ordinary execution/call and is not constructor syntax. In statement position,
  `name{...}` is parsed as a binding unless a surrounding type context makes it a value initializer.
- `block()` with a trailing body block is allowed in any form position; `block{...}` is invalid and parsed as a binding
  or brace constructor.
- `quoted_string` in `import<...>` declarations is a raw quoted string without suffixes.
- Placement transforms `[stack]`, `[heap]`, and `[buffer]` are reserved and rejected by the compiler.
- Recursive struct layouts (structs containing themselves by value, directly or indirectly) are rejected.

## 4A. Draft Variadic Pack Proposal (Parser/Semantics Implemented)

This appendix records the canonical model for variadic user-defined calls. Parser/canonicalization, call-binding
semantics, spread-aware binding into trailing packs, and the read-only body API are now implemented. Backend/runtime
materialization is partial; add a concrete TODO only for a newly reproduced unsupported element kind or runtime
materialization regression.

### 4A.1 Surface forms

```
collect(values...) { ... }
collect([string] values...) { ... }
forward(values...) { return(collect(values...)) }
```

- `name...` in parameter position is surface sugar for a trailing variadic pack parameter.
- `expr...` in call-argument position is surface sugar for pack expansion.

### 4A.2 Canonical parser form

The canonical parser form keeps all meaning in the envelope system:

```
collect<__pack_T>([args<__pack_T>] values) { ... }
forward<__pack_T>([args<__pack_T>] values) { return(collect<__pack_T>([spread] values)) }
```

- `args<T>` is the pack envelope.
- `[spread]` is the explicit canonical spread marker on an argument expression.
- No raw `...` survives below surface syntax.

### 4A.3 Concrete envelope form after monomorphisation

```
[return<vector<i32>>]
collect__i32([args<i32>] values) {
  return(vector__i32([spread] values))
}

[return<void>]
main() {
  [vector<i32>] xs{collect__i32(1i32, 2i32, 3i32)}
}
```

Bottom-level form therefore has:
- no templates,
- no `auto`,
- no semantic meaning attached to bare identifier spelling,
- one ordinary parameter whose envelope is `args<T>`.

### 4A.4 Current semantic restrictions and remaining body API

- At most one `args<T>` parameter per definition.
- The `args<T>` parameter must be last.
- `args<T>` is homogeneous.
- Named arguments bind only fixed parameters, never the `args<T>` parameter directly.
- At most one spread argument per call in the first slice, and it must appear in the final positional slot.
- Trailing positional arguments bind to the `args<T>` parameter after all fixed parameters have been satisfied.
- A final spread argument only binds to a trailing `args<T>` parameter; otherwise validation rejects it
  deterministically.
- A spread argument must evaluate to an existing `args<T>` value, and forwarding that pack also feeds omitted
  element-type inference for the callee's trailing variadic parameter.
- `count(values)`, `values.count()`, `values[index]`, `at(values, index)`, `values.at(index)`, and
  `values.at_unsafe(index)` are available on `args<T>` parameters.
- Backend/runtime materialization for concrete `args<T>` parameters and their read-only body API is now present in the
  legacy C++ emitter (including final-spread forwarding into a trailing variadic slot). IR-backed VM/native lowering
  now covers direct numeric/bool/string pack materialization, pure final-spread forwarding of an existing pack, and
  mixed explicit-prefix + final-spread rebuilding from known-size numeric/bool/string packs, including indexed
  downstream string helpers. Struct packs now also materialize for direct calls plus pure/mixed forwarding across
  `count(...)`, checked/unchecked indexed access, and downstream field/helper resolution. `Result<T, Error>` packs now
  preserve indexed `Result.why(...)` and `?` behavior across direct, pure-spread, and mixed-forwarded IR-backed
  materialization, status-only `Result<Error>` packs preserve indexed `Result.error(...)` and `Result.why(...)`
  behavior across those same forwarding modes, `FileError` packs preserve indexed downstream `why()` mapping across
  those same forwarding modes, `Reference<FileError>` packs preserve indexed downstream `dereference(...).why()`
  mapping across those same forwarding modes, and `Pointer<FileError>` packs preserve indexed downstream
  `dereference(...).why()` mapping across those same forwarding modes. `File<Mode>` packs preserve indexed downstream
  file-handle method access, `Reference<File<Mode>>` packs preserve indexed downstream `dereference(...).write*` /
  `flush()` access, `Pointer<File<Mode>>` packs preserve indexed downstream `dereference(...).write*` / `flush()`
  access, `Buffer<T>` packs preserve indexed downstream `buffer_load(...)` and `buffer_store(...)` on the IR/VM GPU
  path, `Reference<Buffer<T>>` packs preserve indexed downstream `buffer_load(dereference(...), ...)` and
  `buffer_store(dereference(...), ...)` on that same IR/VM GPU path, `Pointer<Buffer<T>>` packs preserve indexed
  downstream `buffer_load(dereference(...), ...)` and `buffer_store(dereference(...), ...)` on that same IR/VM GPU
  path, and `array<T>`, `Reference<array<T>>`, `Pointer<array<T>>`, `vector<T>`, `Reference<vector<T>>`,
  `Pointer<vector<T>>`,
  empty/header-only `soa_vector<T>`, `Reference<soa_vector<T>>`, `Pointer<soa_vector<T>>`, `map<K, V>`,
  `Reference<map<K, V>>`, plus `Pointer<map<K, V>>`
  packs preserve indexed downstream `count()` resolution across those
  same forwarding modes, `vector<T>` packs also preserve indexed downstream `capacity()` plus statement-mutator
  resolution, borrowed/pointer array and vector packs preserve explicit indexed `dereference(...)`
  receiver wrappers for downstream checked/unchecked access, and borrowed/pointer vector packs preserve that same
  indexed `capacity()` and statement-mutator surface through explicit `dereference(...)` receiver wrappers, those same
  value, borrowed, and pointer map packs preserve indexed downstream
  `tryAt(...)` payload-kind inference for `auto` bindings, `Pointer<map<K, V>>` packs also preserve indexed downstream
  `contains()` / `at()` /
  `at_unsafe()` lookup access, and borrowed/pointer map packs preserve that same count and lookup surface through
  explicit indexed `dereference(...)` receiver wrappers. Scalar `Pointer<T>` plus scalar `Reference<T>` packs preserve
  indexed downstream
  `dereference(...)` and struct `Pointer<T>` plus struct `Reference<T>` packs preserve indexed downstream
  field/helper access; other unsupported non-string element support remains a separate follow-up slice.

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
  - Surface `if(cond) { ... } else { ... }` is accepted in any form position (including statement lists and call
    arguments) and is rewritten into canonical `if(...)` by wrapping the two bodies as definition envelopes
    (`then() { ... }` / `else() { ... }`).
  - Surface `if(cond) { ... }` (no `else`) is statement-only sugar and rewrites to
    `if(cond, then() { ... }, else() { })`.
  - Evaluation is lazy: the condition is evaluated first, then exactly one of the two definition bodies is evaluated.
- Loops:
  - `loop(count) { ... }` rewrites to `loop(count, do() { ... })`.
  - `while(cond) { ... }` rewrites to `while(cond, do() { ... })`.
  - `for(init cond step) { ... }` rewrites to `for(init, cond, step, do() { ... })`; commas/semicolons are optional
    separators.
  - Envelope names are ignored (`do() { ... }` and `body() { ... }` are equivalent in the loop body position).
  - Bindings are allowed in any `for` slot; `cond` must evaluate to `bool`.
- `repeat(count) { ... }` is a statement-only builtin; `count` accepts integer or `bool`, and non-positive counts skip
  the body.
- Operators are rewritten into calls:
  - Binary operators allow optional whitespace around the operator token (e.g., `a = b`, `a= b`, `a =b`), but the
    operator token itself is still contiguous (`==` cannot be written as `= =`).
  - Precedence matches conventional rules: `*`/`/` bind tighter than `+`/`-`, comparisons bind tighter than `&&`/`||`,
    and assignment (`=`) is the lowest precedence and right-associative.
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
- Operator compatibility rules (validated after desugaring):
  - Arithmetic operators (`plus`, `minus`, `multiply`, `divide`, `negate`, `increment`, `decrement`) require numeric
    operands (`i32`, `i64`, `u64`, `f32`, `f64`). `negate` rejects unsigned operands. Mixed signed/unsigned operands
    are rejected, as are mixed integer/float operands.
  - Draft matrix/quaternion operators follow an explicit compatibility matrix with no implicit conversions:
    `plus`/`minus` require matching envelopes, `multiply` allows scalar scale + matrix/vector + matrix/matrix +
    quaternion/quaternion + quaternion/`Vec3`, and `divide` allows only composite-by-scalar.
  - Pointer arithmetic is only defined for `plus`/`minus` with a pointer on the left and an integer offset on the
    right (`i32`, `i64`, `u64`). Pointer + pointer is rejected, and a pointer on the right is rejected.
  - Comparisons (`greater_than`, `less_than`, `greater_equal`, `less_equal`, `equal`, `not_equal`) accept
    numeric/bool/string operands. String comparisons require both operands to be strings. Numeric/bool comparisons
    reject mixed signed/unsigned operands and mixed integer/float operands. `bool` participates as signed `0/1`, so
    `bool` with `u64` is rejected as mixed signedness while `bool` with signed ints is allowed.
- Collection literals (via the `collections` text transform) are constructor forms:
  - `array<T>{...}` / `array<T>[...]`
  - `vector<T>{...}` / `vector<T>[...]`
  - `map<K, V>{...}` / `map<K, V>[...]`
  - Map literals accept `key = value` pairs as shorthand for entry construction (e.g., `map<i32, i32>{1i32=2i32}`).
  - The text transform preserves brace construction, normalizes bracket aliases to braces, and rewrites map pair
    shorthand inside the braces; retained call-shaped collection forms are compatibility helpers.
  - Planned stdlib-owned map lowering target: once variadic packs are available for user-defined constructors, the
    intended canonical helper target is `map<K, V>(entry(key1, value1), entry(key2, value2), ...)` rather than
    alternating raw key/value user-defined variadic parameters.
- `/std/math/*` builtins include the core set (`abs`, `sign`, `min`, `max`, `clamp`, `saturate`, `lerp`, `pow`, `sqrt`,
  `sin`, `cos`, etc.) plus `floor`, `ceil`, `round`, `trunc`, `fract`, `is_nan`, `is_inf`, and `is_finite`.
- Math builtin operand rules:
  - `abs`, `sign`, and `saturate` accept numeric operands (`i32`, `i64`, `u64`, `f32`, `f64`).
  - `min`, `max`, `clamp`, `lerp`, and `pow` accept numeric operands but reject mixed signed/unsigned or mixed
    integer/float operands.
  - Integer `pow` requires a non-negative exponent; VM/native backends abort on negative exponents (stderr + exit code
    `3`), and the C++ emitter mirrors this behavior.
  - All remaining `/std/math/*` builtins require float operands. `atan2`, `hypot`, and `copysign` take two float
    operands, while `fma` takes three float operands.
  - Scalar math builtins remain scalar-only unless a builtin explicitly documents vector/matrix/quaternion overloads.
- Method calls:
  - `value.method(args...)` is parsed as a method call and later rewritten to the method namespace form
    `/<envelope>/method(value, args...)`, where `<envelope>` is the envelope name associated with `value`.
    Parentheses are required after the method name.
  - Method calls may include template arguments: `value.method<T>(args...)`.
- Indexing:
  - `value[index]` -> `at(value, index)` (safe indexing)
  - `value.at(index)` is equivalent to `value[index]` and rewrites to `at(value, index)`.
  - `value.at_unsafe(index)` rewrites to `at_unsafe(value, index)`.

The canonical core is what semantic validation and IR lowering consume.

## 6. Definitions and Executions

### 6.1 Definitions

```
[return<i32> effects(io_out)]
main() {
  return(0i32)
}
```

- Definitions may have a body block with statements.
- Transform list on the definition declares return envelope and effects.
- A definition may omit a return transform or use `return<auto>`; the compiler infers the return envelope from
  all return paths. If all return paths yield no value, `return<auto>` resolves to `return<void>`. Conflicting
  return types are a diagnostic.
  Return `auto` participates in implicit-template inference; if constraints resolve to a concrete envelope the
  definition becomes monomorphic.

### 6.2 Executions

```
execute_task([count] 2i32)
```

- Executions are call-style constructs with mandatory parentheses and no body.
- Executions may be prefixed with a transform list (e.g., `[effects(io_out)] log()`).
- Executions are parsed and validated but not emitted by the current C++ emitter.

### 6.3 Unsafe Scopes

```
[unsafe] block {
  [Reference<i32> mut] b{location(value)}
  assign(b, 3i32)
}
block()
```

- `[unsafe]` is a semantic transform on a definition; the body is an unsafe scope.
- Unsafe scopes may violate `Reference<T>` aliasing rules and allow pointer-to-reference initialization from
  pointer-like expressions (`Pointer<T>`/`Reference<T>` values and pointer arithmetic results) when the pointee type
  matches.
- References created inside an unsafe scope must not escape that scope (return, assign to outer bindings, or store into
  aggregates that outlive the scope).
- `[unsafe] block { ... }` defines a local definition; it executes only when called (`block()`).
- Unsafe definitions may be called from safe code; unsafe does not propagate to the caller when escape rules are
  satisfied.

## 7. Parameters, Arguments, and Bindings

### 7.1 Bindings

```
[i32 mut] count{1i32}
[string] message{"hi"utf8}
```

- A binding is a stack value execution.
- Binding initializers are value blocks. The block is evaluated left-to-right; its resulting value (last item
  or `return(value)`) becomes the initializer value. Use brace constructors when you need to construct a value with
  multiple fields (e.g., `[T] name{T{[left] arg1, [right] arg2}}`). Commas and semicolons are treated as whitespace
  separators. `Type(...)` is a call/execution form, not construction.
- If a local binding omits an initializer, it is a diagnostic unless the binding envelope is a struct type with a
  zero-argument constructor **and** the compiler can prove the construction has no outside effects. In that case the
  initializer is treated as `Type{}` (e.g., `[A] a` -> `[A] a{A{}}`). Struct fields still require explicit initializers.
  - **No outside effects (definition):**
    - Empty effects/capabilities mask (no `effects(...)`/`capabilities(...)`, and no callees with non-empty effects).
    - Writes limited to the newly constructed value (`this`) and local temporaries; writes through pointers/references
      or to non-local bindings are disallowed.
    - Field initializers and helper calls must be effect-free transitively.
    - If the compiler cannot prove these constraints, omitted-initializer bindings are rejected.
- Struct constructors treat non-static field bindings as parameters. Positional and labeled entries map to
  fields; any field not supplied by the constructor uses its field initializer expression. Missing fields without
  initializers are a semantic error (struct fields require initializers).
- **Zero-arg constructor:** exists when every field has an initializer, or when a `Create()` helper exists and
  initializes every field (including `uninitialized<T>` fields via `init`). Field initializers run first, then
  `Create()` runs (if present) and may override field values.
- If the binding omits an explicit envelope annotation, the compiler first tries to infer the envelope from the
  initializer form; if inference fails, it is a diagnostic. Parameters are handled separately.
- For struct fields, omitted envelope annotations follow the same rule (`center{Vec3{...}}` is valid when inference
  resolves one concrete envelope). Field envelopes must be concrete before layout manifest emission; unresolved or
  ambiguous inference is a diagnostic.
- Field visibility tags (`[public]` / `[private]`) default to `public` and control field-access metadata only; they do
  not affect top-level import/export visibility.
- An explicit `auto` envelope on a binding must resolve to a concrete envelope; it does not default to any width.
- `mut` marks the binding as writable; otherwise immutable.

### 7.1A Planned Sum Types and Brace-Only Construction

Algebraic sum types use field-like variant declarations:

```prime
[sum]
Shape {
  [Circle] circle
  [Rectangle] rectangle
  [Text] text
}
```

Each payload variant has a lowerCamelCase variant name and one payload envelope. A sum value has exactly one active
variant at runtime. Unit/no-payload variants use bare lowerCamelCase variant names such as `none` rather than an empty
struct payload workaround. They are parsed, validated, and published in semantic-product sum variant metadata with
`has_payload=false` and an empty payload type. Executable unit construction stores only the active tag. Explicit payload
sum construction uses the same brace-only construction model as structs:

```prime
[Shape] explicitLabeled{[circle] Circle{[radius] 3.4}}
[Shape] explicitPositional{[circle] Circle{3.4}}
[Shape] inferredVariant{Circle{3.4}}
```

The explicit `[variant] payload` form always selects that variant. The inferred form is valid only when the target sum
type is known from context and exactly one variant payload type accepts the constructed payload. If zero variants match,
construction is a type error. If more than one variant matches, construction is ambiguous and the source must use the
explicit `[variant]` form.

```prime
[sum]
Message {
  [Text] title
  [Text] body
}

[Message] bad{Text{"hello"}}          // ambiguous: title and body both accept Text
[Message] ok{[title] Text{"hello"}}   // explicit variant selection
```

Generic sums use the same template syntax as other generic definitions:

```prime
[sum]
Maybe<T> {
  none
  [T] some
}

[sum]
Result<T, E> {
  [T] ok
  [E] err
}

[Maybe<i32>] a{}                  // defaults to first variant because it is unit
[Maybe<i32>] b{none}              // explicit unit variant
[Maybe<i32>] c{[some] 1i32}
[Result<i32, string>] d{[ok] 2i32}
```

Template parameters may appear in payload envelopes. Concrete uses monomorphize before validation, so semantic-product
sum metadata publishes substituted payload type text while preserving source-order variant indices and tag values.
Invalid template arity is diagnosed. Recursive inline payloads such as `Bad<T> { [Bad<T>] again }` are rejected until
recursive sum layout is designed. The stdlib `Maybe<T>` surface consumes this generic substrate, and `/std/result/*`
now exposes an imported value-carrying `Result<T, E>` sum. Typed imported value-carrying sum locals/returns may use
legacy `Result.ok(value)` as an `ok`-variant compatibility initializer on IR-backed VM/native paths, typed imported
value-carrying sum locals/returns may use legacy `Result.map(result, fn)` or `Result.and_then(result, fn)` when the
source is a local imported stdlib Result sum or a direct call returning one, and may use legacy
`Result.map2(left, right, fn)` when both sources are local imported stdlib Result sums or direct calls returning them.
`Result.error(value)` / `Result.why(value)` can inspect that imported sum shape.
Status-only `Result<E>` remains a packed-status compatibility bridge and is not pickable as a stdlib Result sum;
`pick(status)` on status-only `Result<E>` reports a deterministic compatibility diagnostic. `try(...)` semantic
validation and semantic-product metadata accept both `Result<T, E>` and `/std/result/Result<T, E>` value-result
spellings. IR-backed `try(...)` can now consume local imported stdlib value-result sums for
`return<int> on_error<...>` status-code flows by branching on the `ok`/`error` tag, and Result-returning functions can
propagate local imported stdlib value-result sum errors by copying the `error` payload into the declared return
`Result` sum after running the active `on_error` handler. Direct calls that return imported stdlib value-result sums
can also be consumed through postfix `?` on those same VM/native sum-backed paths, and dereferenced local
`Reference<Result<T, E>>` / `Pointer<Result<T, E>>` values can feed `try(...)`, `Result.error(...)`, and
`Result.why(...)` when they point at imported stdlib Result sums. Broader result shapes and status-only results remain
compatibility surfaces until their dedicated migration tasks land.

Default sum construction is valid only when the first declared variant is a unit variant. The default active variant is
therefore tag `0`, following source order. Payload variants are never default-constructed implicitly, so if the first
variant has a payload, `[Sum] value{}` remains a diagnostic even when that payload type has its own default constructor.
Variant tag order is layout/serialization-sensitive, so reordering variants changes ABI-like behavior and must be
treated as a version-sensitive change.

`pick` is the semantically validated exhaustive pattern form for sums. Each arm names a variant and binds its payload;
missing variants are diagnostics unless an explicit fallback form is later specified. Payload variants require a binder.
Unit variants use binder-free arms such as `none { ... }`; payload binders on unit variants are rejected.

```prime
pick(shape) {
  circle(c) {
    draw_circle(c.radius)
  }
  rectangle(r) {
    draw_rect(r.w, r.h)
  }
  text(t) {
    draw_text(t.s)
  }
}

pick(maybeValue) {
  none {
    return(0)
  }
  some(value) {
    return(value)
  }
}
```

### 7.2 Parameters

Parameters use the same binding envelope as locals and may omit the envelope:

```
main([array<string>] args, [i32] limit{10i32}) { ... }
function1(param1 param2) { ... }
```

Omitted parameter envelopes are treated as `auto` and become implicit template parameters. Template arguments
are inferred per call site from argument types and return constraints. Unresolved or conflicting parameter
inference is a diagnostic. Defaults are limited to literal/pure forms (no name references).

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
array<i32>{1i32, 2i32}
array<i32>[1i32, 2i32]   // alternate surface form
```

Arrays are fixed-size contiguous value sequences once constructed (C++ `std::array`-like behavior).
PrimeStruct keeps runtime-count `array<T>` as the long-term contract; envelope-level length forms (`array<T, N>`) are
unsupported.
Helpers:
- `value.count()` (canonical equivalent: `count(value)`)
- `value.at(index)` / `value[index]` (canonical equivalent: `at(value, index)`)
- `value.at_unsafe(index)` (canonical equivalent: `at_unsafe(value, index)`)

Architectural direction for type ownership:
- `array<T>` remains a core language/runtime envelope.
- `vector<T>` and `map<K, V>` remain portable surface envelopes today, but their public constructor/helper behavior
  should converge on stdlib `.prime` implementations over generic substrate.
- `soa_vector<T>` is a promoted stdlib-owned public collection surface over
  generic SoA substrate.
- `Maybe<T>` is stdlib-owned, imported value-carrying `Result<T, Error>` construction has a stdlib sum surface, and
  legacy `Result` propagation/helpers, `File<Mode>`, `Buffer<T>`, and `/std/gfx/*` remain hybrid surfaces with minimal
  builtin/runtime substrate.

### 8.2 Vectors

```
vector<i32>{1i32, 2i32}
vector<i32>[1i32, 2i32]
```

Vectors are C++-style dynamic contiguous sequences. Brace construction is variadic; zero or more entries are accepted.
Current call-shaped vector helpers remain compatibility helper surfaces; they are not value construction syntax.
Planned stdlib-owned replacement: the user-defined builder helper is intended to become `vector(values...)`,
canonically represented as a trailing `[args<T>]` parameter rather than fixed-arity helper families.
The current builtin/vector-envelope behavior is transitional; the intended steady state is stdlib-owned public vector
behavior over generic allocation/access substrate.
Growth operations (`push`, `reserve`) may reallocate and invalidate references/pointers into vector storage.
Binding forms:
- `[vector<T> mut] v{vector<T>{}}`
- `[mut] v{vector<T>{}}`
- `[vector<T> mut] v{}` (rewrites to `[vector<T> mut] v{vector<T>{}}`)
Helpers:
- `value.count()` (canonical equivalent: `count(value)`)
- `value.at(index)` / `value[index]` / `value.at_unsafe(index)` (canonical equivalents: `at(value, index)`,
  `at_unsafe(value, index)`)
- `value.push(item)` / `value.pop()` (canonical equivalents: `push(value, item)`, `pop(value)`)
- `value.reserve(capacity)` / `value.capacity()` (canonical equivalents: `reserve(value, capacity)`, `capacity(value)`)
- `value.clear()` / `value.remove_at(index)` / `value.remove_swap(index)` (canonical equivalents: `clear(value)`,
  `remove_at(value, index)`, `remove_swap(value, index)`)

Mutation helpers (`push`, `pop`, `reserve`, `clear`, `remove_at`, `remove_swap`) are statement-only
(not valid in expression contexts).
Vector helper surface syntax currently requires `import /std/collections/*` so the canonical stdlib wrappers are
available during semantic validation.

Current ownership contract:
- `pop` / `clear` require drop-trivial element types.
- Builtin `vector` `remove_swap` and `remove_at` now support ownership-sensitive and relocation-sensitive
  element types on the capacity-guarded heap-backed builtin vector runtime path, because lowered removed-slot
  destruction and survivor-motion paths are both wired. There is no corresponding builtin `soa_vector`
  indexed-removal surface yet.
- `push` / `reserve` require relocation-trivial element types.

Implementation status note: VM/native now use heap-backed vector locals with a
`count/capacity/data_ptr` record; push/reserve growth reallocates backing
storage and preserves existing elements. The remaining runtime ceiling is the
local dynamic-capacity limit (`256`), and `TODO-4281` tracks lifting that limit
once the allocator/runtime contract is widened. Add a separate dynamic-storage
TODO before changing runtime behavior outside that scope.

### 8.3 Maps

```
map<i32, i32>{1i32=2i32, 3i32=4i32}
map<i32, i32>[1i32=2i32, 3i32=4i32]
```

Map literals supply alternating key/value forms; an odd number of entries is a diagnostic.
Maps likewise remain portable envelopes today, but the intended end-state is a stdlib-owned public map surface over
generic memory/error substrate rather than permanent compiler-owned collection semantics.

Helpers:
- `value.count()` (canonical equivalent: `count(value)`)
- `value.at(key)` (canonical equivalent: `at(value, key)`)
- `value.at_unsafe(key)` (canonical equivalent: `at_unsafe(value, key)`)

Map IR lowering is currently limited in VM/native backends: numeric/bool values only, with string keys allowed when they
come from string literals or bindings backed by literals. Within that numeric-key/value envelope, builtin canonical
`map<K, V>` insert growth now covers local bindings, borrowed/pointer mutation surfaces, and non-local field/lvalue
receivers through one shared grow/copy/repoint plus write-back path instead of the older local-only runtime special case.

### 8.4 SoA Vectors (Draft)

`soa_vector<T>` is a distinct structure-of-arrays container and is not interchangeable with
`vector<T>`. The language must not perform implicit AoS/SoA rewriting. The target implementation
model is a stdlib `.prime` container on top of generic SoA substrate; `soa_vector` should not
remain a permanent compiler-owned collection. The current public spellings are
the canonical `/std/collections/soa_vector/*` and
`/std/collections/soa_vector_conversions/*` surfaces; the experimental SoA
namespaces are retained compatibility shims for targeted tests rather than
ordinary public imports.

Draft surface shape:
- `soa_vector<T>{}`
- `value.count()`
- `value.push(item)` / `value.reserve(capacity)` (requires `effects(heap_alloc)`)
- Field-wise access for struct fields: `value.field_name()[index]`
- Whole-element access helpers: `value.get(index)` and optional proxy `value.ref(index)`

Draft constraints:
- `T` must be a SoA-safe struct type under backend policy (initially fixed-size fields; no
  pointer/reference/string/template fields unless explicitly allowed).
- AoS/SoA conversions are explicit helpers only (`to_soa(vector<T>)`, `to_aos(soa_vector<T>)`).
- Reallocation invalidates SoA field views/proxies.
- Draft ownership/invalidation contract:
  - `get(...)` is value-style element access.
  - `ref(...)` and future field views are borrowed SoA views and are invalid after structural mutation.
  - Structural mutation boundaries are `push`, `reserve`, `remove_at`, `remove_swap`, `clear`,
    `to_soa`, and `to_aos`.
  - The same invalidation rule should apply to borrowed wrapper receivers reached through
    `location(...)`, helper-return receivers, and method-like helper-return receivers; those
    surfaces do not create a new ownership domain.
  - ECS-style updates should use a two-phase loop: stable-size read/update pass, then deferred structural writes.

Current implementation status: parser/text-transform support accepts surface `soa_vector<T>{...}`/`soa_vector<T>[...]`,
and semantic validation now accepts `soa_vector` usage when constraints hold
(`soa_vector` struct element requirement, `soa_vector` literal/return template-arity checks, and deterministic
`soa_vector field envelope is unsupported on /Type/field/...: ...` diagnostics for disallowed direct/nested
element-field envelopes in literal, binding, and return validation paths). Builtin `count`/`get`/`ref` validation and
current lowering behavior are temporary scaffolding while the language grows the generic SoA substrate needed for a
real stdlib-owned implementation. Method-form/call-form field-view names now route through the shared
`/soa_vector/field_view/<field>` helper path onto `soaVectorFieldView<Struct, Field>` (or a same-path user helper
when visible), returning `SoaFieldView` values that can be bound with a tracked borrow root; direct builtin
field-view call-argument/return escapes remain deterministic `field-view escapes ...` diagnostics while current IR lowering routes
`count(...)` on `soa_vector` through the native count path for current SoA bindings
while empty `soa_vector<T>{}` literals lower to header-only storage. The stdlib wrapper/helper surface now also covers
direct canonical `/std/collections/soa_vector/*` helper calls plus imported wrapper `to_aos` helper/method routing
across C++/native/VM; canonical conversion helpers use `SoaVector<T>` and
`Reference<SoaVector<T>>` receiver spellings while routing through canonical
`/std/collections/soa_vector/*` helper paths. Valid root bare/method/old-explicit
`count`/`get`/`ref` plus bare/direct/method `to_aos`
calls on builtin `soa_vector<T>` bindings now rewrite onto that same canonical helper path unless a visible
old-surface user helper shadows them. Imported root bare/direct/method builtin `to_aos` forms now also clear
semantics on that same canonical helper path instead of resolving directly against the experimental wrapper
conversion helper. Valid root bare/method/old-explicit `push`/`reserve` calls on builtin
`soa_vector<T>` bindings now also rewrite onto `/std/collections/soa_vector/push|reserve` unless a visible
old-surface user helper shadows them, and old-explicit builtin mutator spellings now also share the same remaining
`push|reserve requires mutable vector binding` lowerer contract on builtin `soa_vector<T>` receivers instead of
falling through to the generic unsupported-expression-call rejection. Builtin `ref(...)` currently rejects direct,
old-explicit, and helper-return local binding persistence plus call-argument and call-form/method-form return escapes
with `unknown method: /std/collections/soa_vector/ref` until the borrowed-view substrate exists.
Imported raw-builtin bare/method canonical `count/get/ref/push/reserve` forms now also clear semantics on that same
canonical helper surface, imported method `get(...).field` / `ref(...).field` now resolves the element struct before
lowering, and imported method `push/reserve` now also reaches the same canonical wrapper-mismatch boundary instead of
the older mutable-vector-binding gap. Root and imported builtin bare/direct `to_aos` forms on raw `soa_vector<T>`
bindings now also lower through the canonical stdlib shim instead of stopping on the earlier builtin-to-wrapper
parameter mismatch, and imported plus no-import root builtin bare/direct/method/slash-method `to_aos` now
materialize `/std/collections/soa_vector/to_aos__...` before lowering. Representative wildcard canonical
helper/conversion coverage now runs across C++ emitter, VM, and native without direct experimental SoA imports in the
test source.
The fixed-width `.prime` SoA storage substrate now reaches sixteen columns. Reflected structs can now generate
`SoaSchemaFieldCount`, `SoaSchemaFieldName`, `SoaSchemaFieldType`, and `SoaSchemaFieldVisibility` helpers to bridge the
current constant-index metadata boundary, and can now also generate `SoaSchemaChunkCount`,
`SoaSchemaChunkFieldStart`, and `SoaSchemaChunkFieldCount` helpers that group those flat field lists into deterministic
sixteen-column storage chunks. They can now also generate `/Type/SoaSchemaStorage`,
`/Type/SoaSchemaStorageNew()`, `/Type/SoaSchemaStorageCount(...)`, `/Type/SoaSchemaStorageCapacity(...)`,
`/Type/SoaSchemaStorageReserve(...)`, `/Type/SoaSchemaStorageClear(...)`, and `/Type/SoaSchemaStorage/Destroy()`
so reflected schemas have deterministic chunked allocation, grow/realloc, explicit clear, and implicit free/drop
cleanup on top of that fixed-width substrate.
Helper-return builtin bare/method `count/get/ref` reads on global and explicit `/Type/helper` receivers now also clear
semantics on that same canonical surface instead of degrading to `does not accept template arguments`, and
helper-return bare/method `push/reserve` local binding plus call-argument and direct-return escapes on those same
receivers now preserve same-path `/soa_vector/push|reserve` helpers instead of degrading to builtin statement-only
semantics or synthetic non-templated-definition template-argument errors.
Vector-target wrong-receiver bare/method `count/get/ref` calls, plus old-explicit method
`count/get/ref`, now also preserve visible same-path
`/soa_vector/count|get|ref` user helpers instead of being pinned to the builtin `soa_vector`
target-mismatch path.
Vector-target root bare/method/old-explicit `get`/`ref` misuses now also keep the same canonical
`/std/collections/soa_vector/get` and `/std/collections/soa_vector/ref` reject contracts instead of the old builtin
`get requires soa_vector target` / `ref requires soa_vector target` diagnostics. Visible vector-target same-path
`/soa_vector/push|reserve` helper shadows now also preserve plain method forms, and old-explicit slash-method forms
rewrite to direct `/soa_vector/push(values, ...)` / `/soa_vector/reserve(values, ...)` calls instead of degrading to
`unknown call target: /std/collections/soa_vector/push`. Vector-target root bare/direct/method
`to_aos` misuses now also keep that same canonical
`/std/collections/soa_vector/to_aos` reject contract instead of the old builtin `to_aos requires soa_vector target`
diagnostic, while visible same-path `/to_aos` user helpers now still win on vector-target wrong-receiver forms
through semantics and dump rewriting. Numeric builtin-vector helper-shadow cases also clear lowering and run across
C++/native/VM now, so those paths no longer depend on the old builtin conversion scaffolding even though the separate
struct-vector literal/runtime boundary still remains outside that helper-routing slice.
Explicit canonical
experimental-wrapper slash-method `values./std/collections/soa_vector/to_aos()` now also validates on that same
canonical helper path and reaches the current lowerer `struct parameter type mismatch` boundary instead of
degrading to an unknown wrapper-method path. Inline lowering also
no longer keeps a dedicated builtin `soa_vector` count/get/ref helper bridge, instead using the shared
definition-resolution plus count/access fallback path for those helper shapes, and the old
backend-specific root builtin `soa_vector` `push|reserve` rejection path is gone too. Lowerer
count/access classifier fallback also no longer treats raw `/soa_vector/count` as a builtin count alias once
semantics has canonicalized those old-surface forms earlier in validation, and the remaining
inferred/method fallback resolution for builtin `soa_vector` count receivers now prefers the
canonical `/std/collections/soa_vector/count` helper path while still preserving same-path
`/soa_vector/count` user-helper shadowing on helper-return receivers, and bare helper-return
`count(...)` local binding, call-argument, and direct return escapes now also preserve that
same-path helper shadow instead of degrading to a synthetic template-argument error. Bare wildcard-imported
helper names on helper-return builtin `soa_vector` receivers now also canonicalize from the
receiver family before experimental wrapper rewriting, so `count(holder.cloneValues())` and
`get(holder.cloneValues(), i).field` stay on the canonical helper surface instead of leaking
through unresolved root helper names. Helper-return expression-position bare and method
`push(...)` / `reserve(...)` now also preserve same-path `/soa_vector/push` and
`/soa_vector/reserve` user-helper shadowing instead of degrading to the old builtin
statement-only mutator contract. Helper-return experimental-wrapper method
`count/get/ref/push/reserve` now also rewrites to visible same-path `/soa_vector/*` helpers
before validation/lowering on both global helper-return and explicit `/Type/helper`
method-like struct-helper return receivers instead of leaking through wrapper methods in
compile-run paths. Nested struct-body helper returns that materialize wrapper values through
`return(soaVectorSingle<Particle>(Particle{...}))` now also clear that same direct/bound
helper/conversion substrate plus same-path `/soa_vector/*` and `/to_aos` helper-shadow method
surfaces instead of failing during specialization or later expression lowering. The equivalent
helper-return method/infer fallback for builtin `soa_vector` `get` receivers now also prefers
the canonical `/std/collections/soa_vector/get` helper path while still preserving same-path
`/soa_vector/get` user-helper shadowing, helper-return builtin bare/method `get(...)` reads on
global and explicit `/Type/helper` receivers now also clear semantics on that same canonical
surface instead of degrading to `get does not accept template arguments`, and bare helper-return
`get(...)` local binding, call-argument, and direct return escapes now also preserve that
same-path helper shadow instead of degrading to a synthetic template-argument error. The equivalent helper-return method/infer fallback for
builtin `soa_vector` `ref` receivers now also prefers the canonical
`/std/collections/soa_vector/ref` helper path while still preserving same-path
`/soa_vector/ref` user-helper shadowing. Read-only helper-return builtin `ref(...).field`
reads on global and explicit `/Type/helper` receivers now also clear semantics on that same
canonical surface instead of degrading to `ref does not accept template arguments`. Local `auto` inference on builtin and helper-return
`ref(...)` method sugar now also respects that same-path helper return type instead of collapsing
to the builtin element type, bare helper-return `ref(...)` local binding, call-argument, and
direct return escapes now also preserve that same-path helper shadow instead of degrading to a
synthetic template-argument error, and builtin borrowed-view recognizers in initializer and monomorph
handling now accept that canonical resolved helper path too. The equivalent
helper-return method/infer fallback for builtin `soa_vector` `to_aos` receivers now also
prefers the canonical `/std/collections/soa_vector/to_aos` helper path, and vector-result
recognizers now accept that canonical resolved helper path too. Same-path `/to_aos`
user-helper shadowing now also survives helper-return builtin `to_aos` bare, direct-call,
method, and infer fallback instead of canonicalizing early onto the canonical helper path.
Experimental wrapper helper-return `to_aos()` method
rewrites now also stop short of forcing `soaVectorToAos(...)` when a same-path `/to_aos`
helper is visible, and that helper-preserving rewrite now also covers explicit `/Type/helper`
method-like wrapper receivers instead of leaving `holder.cloneValues().to_aos()` on the raw
wrapper-method path. Lowerer-side count target classification now also treats
direct canonical `/std/collections/soa_vector/to_aos` call results as vector targets for nested
count dispatch instead of depending on the old raw `/to_aos` call shape. C++ emitter
vector-target classification now also treats those same direct canonical
`/std/collections/soa_vector/to_aos` call results as vector targets for nested helper dispatch
instead of depending on bound vector temporaries, and the shared native/VM vector-capacity
classifier now does the same for nested backend helper dispatch. Those backend-side classifier
checks now share one generic AST call-path helper instead of each backend file carrying its own
`soa_vector/to_aos` matcher. Imported and no-import root bare/direct/method/slash-method
builtin `to_aos` forms on raw `soa_vector<T>` bindings therefore now reach the same canonical
semantic rewrite, materialize `/std/collections/soa_vector/to_aos__...`, and clear lowering plus
C++/VM/native execution on that bridged conversion path. Representative wildcard canonical
helper/conversion flows now also run on those three entrypoints without direct experimental SoA imports. The remaining raw-builtin
conversion-specific compiler-owned code is now reduced to invalid-target/user-shadow fallback
handling instead of a native runtime trap. The diagnostic harness now also locks both direct-canonical and
imported-helper experimental-wrapper `to_aos` forms to that same canonical helper path at
`ast-semantic`, so helper-call/conversion diagnostic cleanup is complete and the remaining SoA work is now richer
field-view behavior rather than more runtime/diagnostic special-case removal.
The shared compiler helper path for `/soa_vector/field_view/<field>` now routes
through `soaVectorFieldView<Struct, Field>` and returns a strided
`SoaFieldView<Field>` value. Standalone method-form and call-form field-view
access on direct locals, borrowed locals, helper-return receivers, method-like
struct-helper return receivers, and inline `location(...)`-wrapped variants now
validate through that shared helper path instead of stopping on the old pending
diagnostic. Mutating standalone method/call writes such as
`assign(value.field(), next)` now lower through `soaVectorFieldView(...)` plus
`soaFieldViewRef<T>(..., 0)` dereference writes on the same helper path, while
indexed and explicit borrowed-slot writes such as `assign(value.field()[i], next)`,
`assign(ref(value, i).field, next)`, and `assign(value.ref(i).field, next)` still
lower through the existing `soaVectorRef<T>(..., i).field` substrate. Borrowed
`Reference<SoaVector<T>>` read-only method sugar `borrowed.get(i)`,
`borrowed.ref(i)`, and `borrowed.to_aos()` plus bare helper forms
`count(...)`, `get(...)`, `ref(...)`, and `to_aos(...)` now also validate on the
existing wrapper helper/conversion path for local, parameter, helper-return,
inline `location(...)`, inline `dereference(location(...))`, inline
location-wrapped borrowed helper-return, method-like struct-helper return, and
inline location-wrapped method-like struct-helper return receivers instead of
stopping on raw builtin target mismatch or the old helper-return lowerer
mismatch. The remaining compiler-owned pending cleanup for builtin SoA field
views now focuses only on the borrowed-view `ref(...)` lifetime rules, not on
field-view helper routing or assignment plumbing.
Validator-side fixed unavailable-method rejects now also route through one
shared visibility-aware helper instead of recomputing the `/soa_vector/ref`
visibility and pending/unavailable split inline, while validator-side visible
same-path `/soa_vector/count|get|push|reserve` helper checks plus
definition-return and collection-return same-path `/soa_vector/<field>`
helper visibility now also route through that same shared
definition-visibility helper instead of repeating import/declared probes
or local same-path wrappers across count/capacity builtin validation,
SoA builtin validation, method-target, infer-time helper-shadow,
vector-helper routing, vector-helper same-path `/soa_vector/push|reserve`
mutator-shadow checks, preferred same-path versus canonical target
selection in method-target, infer-time, and vector-helper mutator
routing, return-inference paths, and builtin SoA access/count helper
fallback validation,
while collection-return visible same-path `/soa_vector/get|ref` helper
detection now uses one shared helper-name path instead of split `get` / `ref`
branches, while preferred
same-path-versus-canonical `soa_vector`
helper target selection in method-target, infer-time, vector-helper
mutator routing, plus builtin `get/ref` call-shape detection in direct
validation and collection-return inference, plus direct same-path
`/soa_vector/<helper>` visibility checks in count/builtin/method/vector
routing, direct field-view helper detection, and return-inference
helper-shadow probes now all route through shared validator helpers,
with the remaining vector-target and builtin count/get/ref same-path
probes now also using shared target helpers instead of direct
`/soa_vector/count|get|ref` checks,
with the validator-side direct builtin `ref(...)` visibility split now also
using that same shared helper-target predicate instead of a hardcoded
`/soa_vector/ref` probe,
while monomorph-side visible `/soa_vector/ref` fallback detection now also
uses that same shared definition-visibility helper directly instead of a
dedicated ref-specific wrapper or local visibility cache, while
monomorph-side old-surface `/soa_vector/ref` and `/soa_vector/<field>`
visibility now also uses direct `sourceDefs` / `helperOverloads` checks
inside monomorph implicit-template fallback instead of a local helper-target
probe or a monomorph-local visible-path wrapper, while
monomorph-side fixed method pending rejects now call
the shared pending helper directly instead of a monomorph-local wrapper, while
the remaining validator-side fixed unavailable-method rejects now call
`soaUnavailableMethodDiagnostic(...)` directly with that same shared `ref`
helper-target predicate instead of open-coded preferred-target comparisons, while
direct builtin field-view/ref pending reporters now also route through one
shared expr-to-path helper and the validator-local direct pending reporter
wrapper plus the thin `isBuiltinSoaRefExpr(...)` wrapper are gone, while
monomorph-side visible same-path
`/soa_vector/<field>` helper checks now also route through that same shared
definition-visibility helper instead of probing `sourceDefs` and
`helperOverloads` inline, while monomorph-side fixed method pending rejects
now also route through one shared visibility-aware helper instead of
open-coding the optional pending lookup around that same visibility probe, and
the low-level field-view and
borrowed-view pending string builders are now
file-local to the shared helper implementation instead of part of the public
semantics-helper surface, while direct collection-access, count/capacity
builtin validation, SoA builtin validation, collection dispatch inference,
direct access helper routing, and method-target/infer borrowed experimental
receiver handling now also share one borrowed experimental receiver probe
end-to-end instead of keeping duplicated inline lambdas, wrapper lambdas, direct receiver
adapters, or local borrowed-target wrappers, while direct SoA builtin validation now also
calls that shared borrowed receiver probe directly instead of a local direct-receiver adapter.
Standalone builtin field-view call
forms now route through the shared synthetic `/soa_vector/field_view/<field>`
or same-path `/soa_vector/<field>` path instead of a dedicated
`SemanticsValidatorExprMapSoaBuiltins.cpp` fallback, and resolved helper-form
field-view rejects now reuse the shared unavailable-method helper path there
instead of an inline pending branch. The remaining live
follow-ups are now reduced to invalidation plus richer borrowed-view semantics on top of that
substrate. The current completed borrowed foothold now includes direct whole-value `ref(...)`
values across direct wrapper locals, borrowed locals, explicit `dereference(...)` receivers,
borrowed helper-return/method-like helper-return receivers, and inline
`location(...)`-wrapped borrowed receivers in addition to the indexed/field-level borrowed
projection surface (`ref(...).field`, `.ref(i).field`, and `value.field()[i]`-style
reads/writes). Those projections are recomputed per use through the existing
`soaVectorGet(...).field` / `soaVectorRef(...).field` helper path, so later invalidation
work still starts at standalone borrowed values and standalone borrowed field-view values
rather than the already-completed per-use projection surface. The next implementation step is therefore after the now-completed
slot-backed borrowed-value carrier exposure through `soaColumnBorrowSlot<T>(...)` /
`vectorBorrowSlot<T>(...)`. Those stdlib helpers now validate through `[return<Reference<T>>]`
with slot-pointer provenance preserved through local `slot` aliases, and public
`soaColumnRef<T>(...)` plus experimental helper `soaVectorRef<T>(...)` now also preserve that
standalone borrowed carrier instead of dereferencing back to whole-element `T`, and the shared
canonical/helper-method route for `ref(values, i)` / `values.ref(i)` now validates through the
same `Reference<T>` carrier as well. Projected `.ref(i).field` reads/writes already ride the
existing per-use `soaVectorRef(...).field` rewrite and stay with the later standalone borrowed
field-view queue rather than this whole-value helper step.
Read-only wrapper field-view indexing now routes both method-form `values.field()[i]`
and call-form `field(values)[i]` reflected reads, plus borrowed local `borrowed.field()[i]`,
inline `location(values).field()[i]` / `field(dereference(location(values)))[i]`,
explicitly dereferenced borrowed `dereference(borrowed).field()[i]`, borrowed helper-return `pickBorrowed(...).field()[i]`,
method-like struct-helper return `holder.pickBorrowed(...).field()[i]` / `field(holder.pickBorrowed(...))[i]`,
inline location-wrapped borrowed helper-return `location(pickBorrowed(...)).field()[i]` / `field(location(pickBorrowed(...)))[i]`,
inline location-wrapped method-like struct-helper return `location(holder.pickBorrowed(...)).field()[i]` /
`field(location(holder.pickBorrowed(...)))[i]`,
and explicitly dereferenced borrowed helper-return
`dereference(pickBorrowed(...)).field()[i]` reads onto the existing
`soaVectorGet<T>(..., i).field` helper path for both single-field and multi-field structs.
Direct bare helper-field reads such as `ref(values, i).field`, `get(values, i).field`,
method-form helper-field reads such as `values.ref(i).field` / `values.get(i).field`,
bound method-like helper-return forms such as `[i32] field{get(holder.pickBorrowed(...), i).field}`
and `[i32] field{holder.pickBorrowed(...).ref(i).field}`,
and bound inline location-wrapped method-like helper-return forms such as
`[i32] field{get(location(holder.pickBorrowed(...)), i).field}` and
`[i32] field{location(holder.pickBorrowed(...)).ref(i).field}` now also ride that same
generic struct-field path, and that successful read path no
longer depends on lowerer/emitter/backend-local `field_view` or `soaVectorGet|soaVectorRef`
routing branches. Direct return/root-expression borrowed helper-return reads such as
`return(pickBorrowed(...).count())`, `return(pickBorrowed(...).field()[i])`,
`return(get(pickBorrowed(...), i).field)`, `return(pickBorrowed(...).get(i).field)`,
`return(pickBorrowed(...).ref(i).field)`, direct return/root-expression method-like borrowed
helper-return reads such as `return(holder.pickBorrowed(...).count())`,
`return(holder.pickBorrowed(...).field()[i])`, `return(get(holder.pickBorrowed(...), i).field)`,
`return(holder.pickBorrowed(...).get(i).field)`, `return(holder.pickBorrowed(...).ref(i).field)`,
and inline `location(...)`-wrapped variants now route through that same helper/indexing
substrate. The standalone `ref(...)` receiver families are therefore in place, and those
whole-value carriers now also survive local binding, helper pass-through, and direct helper
return surfaces. Live standalone `Reference<T>` carriers now also reject later `push` /
`reserve` growth on that same wrapper across direct local,
direct borrowed/dereferenced receiver, helper-return, pass-through, and
return-rooted surfaces. Standalone borrowed field-view values do not exist yet,
but once they do they inherit this same invalidation contract from the richer
field-view design note below. The compiler-owned direct unsupported field-view
path now remains only for standalone borrowed reads; indexed field-view writes
already lower through the `soaVectorRef<T>(..., i).field` substrate and standalone
method/call writes now lower through `soaFieldViewRef<T>(..., 0)` on the shared
helper path.
Locally bound borrowed field-view values such as `borrowed.field()` /
`field(borrowed)` now become non-owning column views over the same wrapper
storage instead of remaining on the pending-diagnostic path. That borrowed-view
surface covers the same receiver families that already succeed for
`value.field()[i]`: direct borrowed locals, explicit dereference, borrowed helper
returns, method-like helper returns, and inline `location(...)`-wrapped borrowed
receivers. Bound field-view values inherit the same invalidation contract as
`ref(...)`, stay borrowed rather than materializing owning vectors, and report
borrowed-binding diagnostics when a live projection overlaps a later structural
mutation. Direct pass/return escapes remain rejected until that provenance
contract is promoted. The implementation now has the reusable non-owning `SoaColumn<T>`
borrowed-view helper substrate plus reflected layout facts for reflect-enabled
structs. `SoaColumn<T>` still stores contiguous whole `T` elements, and the
current `SoaSchema*` helpers now expose validated field byte offsets and
whole-element stride through `SoaSchemaFieldOffset(...)` /
`SoaSchemaElementStride()`. Existing buffer helpers now expose byte-addressable
reinterpretation and offsetting, and the stdlib builds
`soaColumnFieldSlotUnsafe<Struct, Field>(...)` plus the strided `SoaFieldView<T>`
carrier on top of those reflected layout facts. Field-view binding initializers now route through the shared
`/soa_vector/field_view/<field>` helper path onto `soaVectorFieldView<...>` and
return a non-owning strided view. Bound field-view values now preserve borrowed
semantics by resolving their receiver roots through direct locals,
`location(...)`/`dereference(...)`, and helper-return receivers;
later structural mutation on that live root reports the same borrowed-binding
invalidation diagnostic used by `ref(...)` carriers. Direct call-argument and
return escapes remain rejected until the pass/return provenance contract is
promoted.
Standalone mutating method/call field-view writes now lower through
`soaVectorFieldView(...)` plus `soaFieldViewRef<T>(..., 0)` dereference writes for
direct wrapper locals, mutable borrowed locals, borrowed helper returns, method-like
helper returns, and inline `location(...)`-wrapped variants instead of stopping on
the old pending diagnostic. Those writes preserve the same invalidation rules as
other borrowed SoA views and do not reintroduce builtin-only mutation branches
outside the experimental helper path. The remaining invalidation boundary now
starts at later whole-value `ref(...)` shrink/motion, storage-replacement/destruction,
and provenance/escape rules after the later standalone borrowed field-view values
themselves land.
Non-empty `soa_vector` literals now lower through deterministic builtin
materialization in backend literal lowering: struct-element payload slots are
heap materialized and copied into SoA storage-compatible contiguous memory
instead of emitting the former direct unsupported diagnostic boundary.
These compiler-owned `soa_vector` materialization paths are transitional and
should be deleted once generic SoA substrate owns the remaining backend storage
work behind the promoted stdlib wrapper surface.
Canonical example source: `examples/3.Surface/soa_vector_ecs.prime` imports
`/std/collections/soa_vector/*` and
`/std/collections/soa_vector_conversions/*` for the supported wrapper flow.
That wrapper flow is the promoted public shape for `soa_vector<T>`; direct
experimental SoA imports remain compatibility shims for targeted tests only.

### 8.5 Matrix and Quaternion Types (Draft)

`Mat2`, `Mat3`, `Mat4`, and `Quat` now ship as nominal stdlib math types.
They are not aliases of `array`, `vector`, or `Vec4`.

Current matrix surface:
- `Mat2`, `Mat3`, and `Mat4` use ordinary struct construction and public scalar `mRC` fields (`m00`, `m01`, ...).
- The first component index is the row and the second component index is the column.

Current quaternion surface:
- `Quat` uses ordinary struct construction and public scalar `x`, `y`, `z`, and `w` fields.
- `Quat.toNormalized()` returns a normalized quaternion, and `Quat.normalize()` is the method-sugar equivalent.
- `quat_to_mat3(q)` returns a `Mat3` rotation matrix after normalizing `q`.
- `quat_to_mat4(q)` returns a homogeneous `Mat4` rotation matrix after normalizing `q`.
- `mat3_to_quat(m)` returns a normalized `Quat` from a `Mat3` rotation basis.

Draft constraints:
- No implicit scalar/vector/matrix/quaternion conversion; use explicit constructors/helpers.
- `plus`/`minus` require matching envelopes and dimensions.
- Current implementation status: semantics already enforces the `Mat*`/`Quat` `plus` and `minus` rules, the documented
  `Mat*`/`Quat` `multiply` allowlist, `Mat* / scalar` plus `Quat / scalar` divide validation, and deterministic
  binding/return/call diagnostics for implicit `Mat*`/`Quat` family conversions. VM/native, Wasm, and the C++ emitter
  now also lower component-wise `Mat2`/`Mat3`/`Mat4` and `Quat` `plus` + `minus`, scalar-left/right matrix/quaternion
  scaling, matrix/quaternion-by-scalar divide, matrix-vector multiply, matching matrix-matrix multiply,
  quaternion-quaternion Hamilton products, and quaternion-`Vec3` rotation through the documented contract. GLSL now
  lowers nominal `Vec2`/`Vec3`/`Vec4`, `Quat`, `Mat2`/`Mat3`/`Mat4` values, direct vector/quaternion/matrix field
  access, component-wise vector/quaternion `plus`/`minus`, vector/quaternion scalar scale/divide, `MatN * VecN` interop,
  matching matrix-matrix multiply, quaternion-quaternion Hamilton products, quaternion-`Vec3` rotation, and the explicit
  conversion helpers `quat_to_mat3`, `quat_to_mat4`, and `mat3_to_quat`.
- `multiply` supports:
  - Scalar scaling (`S * VecN`, `VecN * S`, `S * Mat`, `Mat * S`, `S * Quat`, `Quat * S`)
  - Matrix-vector (`Mat * VecN`) when inner dimensions match
  - Matrix-matrix (`MatRxC * MatCxK`) when inner dimensions match
  - Quaternion-quaternion Hamilton product
  - Quaternion-vector rotation (`Quat * Vec3`)
- `divide` supports only composite-by-scalar (`VecN / S`, `Mat / S`, `Quat / S`).
- Matrix/vector multiplication follows column-vector semantics (`result = Mat * Vec`); no implicit transpose.
- Conformance reference tests for float matrix/quaternion outputs use an absolute `1e-4` tolerance policy.
- Equality operators remain exact component-wise comparisons; tolerance checks use explicit helpers.

## 9. Effects

- Effects are declared via `[effects(...)]` on definitions or executions.
- Defaults can be supplied by `primec --default-effects` (the compiler enables `io_out` by default unless set to
  `none`).
- Backends reject unsupported effects.
  - Execution effects must be a subset of the enclosing definition’s active effects; otherwise the compiler emits a
    diagnostic.
  - Recognized v1 capabilities: `io_out`, `io_err`, `file_read`, `file_write`, `heap_alloc`, `global_write`,
    `asset_read`, `asset_write`,
    `gpu_dispatch`.
  - VM/native backends allow `io_out`, `io_err`, `heap_alloc`, `file_read`, `file_write`, `gpu_dispatch`, and
    `pathspace_*` effects (`pathspace_notify`, `pathspace_insert`, `pathspace_take`, `pathspace_bind`,
    `pathspace_schedule`); `file_write` also implies `file_read`. GLSL allows `io_out`, `io_err`, and `pathspace_*`
    effects/capabilities.

### 9.1 Backend Type Support (v1)

- VM/native:
  - Scalar values: `i32`, `i64`, `u64`, `bool`, `f32`, `f64`.
  - Collections: `array`, `vector`, and `map` of numeric/bool values; map string keys are limited to string literals or
    literal-backed bindings.
  - Strings: supported for literals and literal-backed bindings used in print/map contexts; string returns are supported
    for literal-backed values, while string pointers/references are rejected.
  - `convert<T>` targets: `i32`, `i64`, `u64`, `bool`, `f32`, `f64` (software numeric envelopes are rejected).
- GLSL:
  - Scalar values: numeric/bool (`i32`, `i64`, `u64`, `bool`, `f32`, `f64`).
  - Nominal vector values: `Vec2`, `Vec3`, and `Vec4`.
  - Nominal quaternion values: `Quat`.
  - Nominal matrix values: `Mat2`, `Mat3`, and `Mat4`.
  - String literals and other unsupported composites are rejected; entry definitions must return `void`.
  - `convert<T>` targets match the numeric/bool list above.
- Matrix/quaternion status:
  - VM/native, Wasm, and the C++ emitter currently support nominal matrix/quaternion values, conversion helpers,
    component-wise `Mat2`/`Mat3`/`Mat4` and `Quat` `plus` + `minus`, matrix/quaternion scalar scaling + divide,
    `Mat2`/`Mat3`/`Mat4` matrix-vector multiply, matching matrix-matrix multiply, quaternion-quaternion Hamilton
    products, and quaternion-`Vec3` rotation.
  - GLSL currently supports nominal `Vec2`/`Vec3`/`Vec4`, `Quat`, and `Mat2`/`Mat3`/`Mat4` values, direct
    vector/quaternion/matrix field access, component-wise vector/quaternion `plus`/`minus`, vector/quaternion
    scalar scale/divide, `MatN * VecN` interop, matching matrix-matrix multiply, quaternion-quaternion Hamilton
    products, quaternion-`Vec3` rotation, and the explicit conversion helpers `quat_to_mat3`, `quat_to_mat4`,
    and `mat3_to_quat`.

## 10. Error Handling (Draft)

- `Result<Error>` is a status-only wrapper for fallible operations; `Result<T, Error>` carries a success value.
- Imported value-carrying `Result<T, Error>` construction has a stdlib sum surface under `/std/result/*`; typed
  locals/returns may use legacy `Result.ok(value)` as an `ok`-variant compatibility initializer, typed locals/returns
  may use legacy `Result.map(result, fn)` or `Result.and_then(result, fn)` when the source is a local imported stdlib
  Result sum or a direct call returning one, and may use legacy `Result.map2(left, right, fn)` when both sources are
  local imported stdlib Result sums or direct calls returning them. `Result.error(value)` / `Result.why(value)` can
  read that sum shape on IR-backed VM/native paths.
  Status-only `Result<Error>` remains a packed-status bridge and reports a deterministic compatibility diagnostic
  when used as a `pick` target. `?` propagation remains a hybrid compiler/runtime bridge until its migration TODO
  lands.
- The postfix `?` operator unwraps a `Result` in-place. On error, it invokes a local handler and returns the error
  from the current definition.
  - **Monadic view:** `value?` is equivalent to binding the success value and early-returning the error; it matches
    `Result.and_then` semantics and is the recommended shorthand for fallible sequencing.
  - **Type rule:** `?` is valid inside a definition (or block) whose return envelope is `Result<_, ErrorType>`, and
    also inside `return<int>` definitions that declare a matching `on_error<ErrorType, Handler>(...)`.
    In the `return<int>` form, an error invokes the handler and returns the raw error code from the current
    definition. The nearest `on_error<ErrorType, Handler>(...)` must match the error type, and the `?` expression
    yields the success payload type.
- Error handlers are declared with a semantic transform on the same scope as the `?` usage:
  - `on_error<ErrorType, Handler>(args...)`
  - The handler signature is `Handler(ErrorType err, args...)`.
  - Bound arguments are evaluated when the error is raised.
  - `on_error` does not apply to nested blocks; any block containing `?` must declare its own `on_error`.
  - Missing `on_error` for a `?` usage is a compile-time error.
- VM/native runtime errors (bounds checks, missing map keys, vector capacity, invalid integer `pow`, etc.) abort
  execution, print a diagnostic to stderr, and return exit code `3`. Parse/semantic/lowering errors remain exit code
  `2`.

## 11. Return Semantics

- `return(value)` is explicit and canonical; parentheses are required.
- `return()` is valid for `void` definitions.
- Non-void definitions must return along all control paths.

## 12. Entry Definition

- The compiler entry point is selected with `--entry /path` (default `/main`).
- The entry definition may take either zero parameters or a single `array<string>` parameter.
  - If a parameter is present, it must be exactly one `array<string>` parameter. An omitted envelope or `auto`
    on the entry parameter resolves to `array<string>`.
  - The entry parameter does not allow a default value.
- `args.count()` and `count(args)` are supported; checked indexing is available via
  `args[index]` or `args.at(index)`; unchecked indexing uses `at_unsafe(args, index)` or
  `args.at_unsafe(index)`.

## 13. Backend Notes (Syntax-Relevant)

- VM/native backends accept a restricted subset of envelopes/operations (see design doc).
- Strings are supported for printing, `count`, and indexing on string literals and string bindings in VM/native
  backends.
- Struct values are supported in VM/native backends when fields are numeric/bool or other struct values; string or
  templated struct fields are rejected.
- The C++ emitter supports broader operations and full string handling.
- Wasm backend profiles use `primec --emit=wasm --wasm-profile=wasi|browser` (`wasi` default). The current normative
  Wasm limit IDs are `WASM-LIMIT-MEM-ON-DEMAND`, `WASM-LIMIT-MEM-SINGLE`, `WASM-LIMIT-IMPORTS-WASI`,
  `WASM-LIMIT-PROFILE-BROWSER`, and `WASM-LIMIT-PROFILE-WASI-ALLOWLIST` (see `docs/PrimeStruct.md`).
- GPU backends accept only GPU-safe envelopes/operations; kernel bodies are validated against a GPU-safe allowlist.
- VM/native emitter restrictions (current):
  - Recursive calls are rejected.
  - Lambdas are rejected.
  - String comparisons are rejected; string literals are limited to print/count/index/map contexts.
  - String return types, string array returns, and string pointer/reference bindings are rejected.
  - Block arguments on non-control-flow calls and arguments on `if` branch blocks are rejected.
  - `print*` and vector helper calls are statement-only; expression usage is rejected.
  - `File<Mode>(path)` requires a string literal or literal-backed binding; with `import /std/file/*`, that
    constructor-shaped compatibility helper surface rewrites through stdlib `/File/open_read(...)`,
    `/File/open_write(...)`, or `/File/open_append(...)` wrappers while the underlying file-open substrate remains
    builtin. It is not value construction syntax.
  - `Result.ok(value)` plus `Result.map(...)`, `Result.and_then(...)`, and `Result.map2(...)` currently accept
    `i32`, `bool`, `f32`, literal-backed `string`, the single-slot int-backed stdlib error structs, ordinary
    user structs that can stay on the existing stack-backed struct path, packed `File<Mode>` handles, and
    `Buffer<T>` handles when downstream `try(...)` consumers stay explicitly typed. Direct
    `Result.ok(...)` plus downstream `try(...)` also preserve the current single-slot int-backed stdlib error
    structs (`FileError`, `ImageError`, `ContainerError`, `GfxError`), `array<T>` / `vector<T>` handles whose
    element kinds already fit the current collection contract, and `map<K, V>` handles whose key/value kinds already
    fit that same map contract. On IR-backed backends, value-carrying `Result.and_then(...)` lambdas may end in an
    explicit `return(Result...)` or in a final `if(...)` expression whose `then(){...}` / `else(){...}` branches
    each produce a `Result`. IR-backed `[args<Result<T, Error>>]`, `[args<Reference<Result<T, Error>>>]`, and
    `[args<Pointer<Result<T, Error>>>]` packs preserve indexed `try(...)`, `Result.error(...)`, and `Result.why(...)`
    access across direct, pure-spread, and mixed variadic forwarding when `T` already fits that same payload
    contract; local dereferenced `Reference<Result<T, Error>>` and `Pointer<Result<T, Error>>` values also preserve
    those helpers when they point at imported stdlib Result sums. Downstream `try(...)`, `Result.error(...)`, and
    success/error `Result.why(...)` preserve those
    handle/error-struct payloads, rebuild single-slot struct payloads, and keep multi-slot struct payloads on that
    same pointer-backed path on VM/native; other remaining wider non-struct payloads remain unsupported.
  - Unsupported math or GPU builtins fail during lowering.
- Executions are parsed/validated but are not emitted by VM/native/GLSL/C++ backends; only definitions reachable from
  the entry definition are lowered.
- VM/native consume the PSIR v16 opcode set (see design doc) and deserialization rejects unknown opcodes.
- GLSL emitter restrictions (current):
  - Entry definitions must return `void` and may contain at most one `return()` statement.
  - Bindings may use GLSL-supported scalar/vector/quaternion/matrix types only; arrays, maps, general structs, and
    string literals are rejected.
  - Static bindings are rejected.
  - Assign/increment/decrement targets must be local mutable bindings.
  - Control flow must use the canonical forms (`if(cond, then() { ... }, else() { ... })`,
    `loop(count, body() { ... })`, `while(cond, body() { ... })`, `for(init, cond, step, body() { ... })`).
  - Builtins require positional arguments; template/block arguments are rejected and unsupported builtins fail.

## 14. Error Rules (Selected)

- `at(value, index)` on non-collections is rejected.
- Duplicate labeled arguments are rejected.
- Unknown labeled arguments are rejected.
- Labeled arguments are rejected for builtin calls.
- `return` is valid inside definition bodies and value blocks. In a value block, `return(value)` returns from the block
  and yields its value.
- Transform argument lists may not be empty.
- Single-branch surface `if(cond) { ... }` is allowed only in statement position; using it in value/form position is
  rejected.
- `if(condition, then() { ... }, else() { ... })` requires both branches to produce a value when used in value position.
- `if` / `while` / `for` conditions must evaluate to `bool` (or a function returning `bool`).
- `loop` counts must be integer envelopes; negative counts are errors.
- `loop` / `while` / `for` require a body envelope (e.g., `do() { ... }`).
- `and` / `or` / `not` accept only `bool` operands; use `bool{value}` to coerce integers.

## 15. GPU Compute (Draft)

- `[compute]` marks a definition as a GPU kernel. Kernels are `void` and write outputs via buffer parameters.
- `workgroup_size(x, y, z)` fixes the kernel's local workgroup size and must appear alongside `[compute]`.
- Host-side submission uses `/std/gpu/dispatch(kernel, gx, gy, gz, args...)` and requires `effects(gpu_dispatch)`.
- GPU builtins live under `/std/gpu/*`, starting with `/std/gpu/global_id_x()`, `/std/gpu/global_id_y()`,
  `/std/gpu/global_id_z()`.
- GPU ID helpers are scalar (`i32`) per axis.
- Storage buffers use `Buffer<T>` plus `/std/gpu/buffer_load` / `/std/gpu/buffer_store` helpers.
- Windowed graphics API shape (`/std/gfx/*`, frame/present flow, vertex wire contracts) is specified in
  `docs/Graphics_API_Design.md` and is separate from this compute-focused section.
