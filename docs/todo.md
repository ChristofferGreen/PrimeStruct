# PrimeStruct Spec ↔ Code TODO

Legend:
- ○ Not started
- ◐ Started
- ✓ Finished

**Pipeline & CLI**
- ✓ Implement semantic transform phase and registry (`--semantic-transforms`, `--no-semantic-transforms`) and the `text(...)` / `semantic(...)` grouping syntax in transform lists.
- ✓ Make per-envelope transform lists drive text/semantic transforms (today text filters are global and ignore `[operators]`, `[collections]`, etc).
- ✓ Add auto-injected transform rules and path filters (`/math/*`, recurse flags) described in docs; currently only a global text filter list exists.
- ✓ Support text transforms appending additional text transforms to the same node (self-expansion in the transform pipeline).
- ✓ Align CLI naming and behavior with docs: support `--text-transforms`, `--no-text-transforms`, and true auto-deduction in `--transform-list` (currently `--text-filters` only).
- ✓ Apply `single_type_to_return` via semantic transforms or per-definition markers (it is currently toggled via the text filter list).
- ✓ Document `--emit=ir` (PSIR bytecode output) in the docs or remove it from the CLI.
- ✓ Enforce canonical-only parsing when `--no-transforms` is active (disable parser sugar like `if(...) {}` / `value[index]` and require explicit return transforms); currently still accepts surface forms.

**Syntax & Surface Features**
- ✓ Treat semicolons as optional separators everywhere the SyntaxSpec allows them (top-level/bodies, transform/template/param/arg lists, include/import lists); currently hard errors.
- ✓ Treat commas as optional separators everywhere the SyntaxSpec treats them as whitespace (statement lists and binding initializers, not just argument/template lists).
- ✓ Allow trailing separators (comma/semicolon) in lists where the SyntaxSpec permits them; parser rejects trailing commas and semicolons entirely.
- ✓ Allow whitespace-separated include entries (grammar allows separators to be optional; include lists currently require commas).
- ✓ Enforce reserved keywords `if`, `else`, `loop`, `while`, `for` in identifiers and slash paths (parser/include resolver currently allow them).
- ✓ Implement control-flow sugar for `loop`, `while`, and `for` (only `repeat(count) { ... }` is implemented today).
- ✓ Implement operator sugar for `++` / `--` (`increment` / `decrement` are documented but not rewritten by the text filter).
- ✓ Support comma digit separators in numeric literals (lexer currently splits `1,000i32` into multiple tokens).
- ✓ Support nested definitions inside definition bodies (nested definitions receive their own transforms).
- ○ Support lambdas inside definition bodies (spec references lambdas; parser only allows bindings/expressions).
- ✓ Allow execution-style transforms inside bodies and argument lists (SyntaxSpec allows `[effects(io_out)] log()` anywhere a form is allowed; parser treats leading `[...]` as a binding and rejects `()`).
- ✓ Support brace constructor forms in value positions (`Type{ ... }` in arguments/returns); parser currently requires `()` or treats braces as bindings.

**Types & Semantics**
- ○ Implement software numeric envelopes (`integer`, `decimal`, `complex`) or document their explicit rejection in the syntax spec.
- ○ Align `int`/`float` aliases with the spec’s target-chosen widths (code currently fixes them to `i32`/`f32`) or update the docs to match.
- ○ Add template monomorphization for user-defined definitions (templates are parsed but only builtins/collections use them today).
- ◐ Complete struct lowering (layout metadata, alignment enforcement, `Create`/`Destroy` semantics, and backend emission are still missing; only validation exists).
- ○ Implement `copy`/`public`/`private`/`package`/`static` binding semantics and metadata (currently validated but unused).
- ✓ Allow non-primitive `Pointer<T>`/`Reference<T>` targets (or document the primitive-only restriction enforced today).
- ○ Allow untagged definitions to be used as struct types in bindings (docs say struct tags are optional for instantiation).
- ✓ Treat `if` block envelope names as ignored even if they collide with definitions (branch blocks should not resolve to defs).
- ✓ Align default effects behavior: docs say entry defaults include `io_out`, but code applies no default effects unless `--default-effects` is provided.
- ✓ Implement binding initializer block semantics: `{ ... }` should allow multi-statement blocks and `return(value)` with last-expression value; current parser treats multiple expressions as constructor args (requires explicit type) and forbids `return`.

**Backends & IR**
- ○ Add GLSL/SPIR-V backend (docs mention GPU lowering, but there is no backend in `src/`).
- ◐ Expand VM/native float support (parser/semantics/C++ emitter accept floats; VM/native reject float literals/types).
- ◐ Expand VM/native `/math/*` coverage (docs list full math set; VM/native currently support only a subset like abs/sign/min/max/clamp/lerp/pow).
- ○ Implement VM/native string indexing (`at` / `at_unsafe`) for string literals/bindings or update the SyntaxSpec (native lowerer rejects string indexing).
- ✓ Update PSIR version history in docs (serializer is at v10; docs list up to v9).

**Docs Alignment**
- ○ Clarify VM/native string limits in `docs/PrimeStruct_SyntaxSpec.md`: count/indexing currently only work for string literals or bindings backed by literals (argv-derived bindings are print-only).
- ○ Document the `repeat(count) { ... }` statement builtin (count accepts integers or bool; non-positive counts skip the body).
- ○ Document `block{ ... }` calls and the general `{}` body-argument form after `()` (parser accepts call bodies; spec currently limits bodies to special forms).
- ○ Document map literal `key=value` syntax supported by the `collections` text filter.
- ○ Document unary `&` / `*` operator sugar (`location` / `dereference`) in the `operators` text transform or remove the feature.
- ○ Document single-letter float suffix `f` (treated as `f32`) or reject it in the lexer/parser.
- ○ Fix string literal rules: single-quoted strings are not raw in code (rawness is controlled by `raw_utf8`/`raw_ascii` suffix only).
- ○ Document canonical string normalization: parser emits `raw_utf8` / `raw_ascii` with unescaped content, not the escaped `"..."utf8/ascii` form described in the spec.
- ○ Document vector helper calls (`push`, `pop`, `reserve`, `clear`, `remove_at`, `remove_swap`) as statement-only or allow them in expression contexts.
- ○ Add missing `/math/*` builtins present in code: `floor`, `ceil`, `round`, `trunc`, `fract`, `is_nan`, `is_inf`, `is_finite`.
- ○ Add PathSpace builtins (`notify`, `insert`, `take`) to `docs/PrimeStruct_SyntaxSpec.md` (currently only in the design doc).
