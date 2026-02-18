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
- ✓ Support lambdas inside definition bodies (spec references lambdas; parser only allows bindings/expressions).
- ✓ Allow execution-style transforms inside bodies and argument lists (SyntaxSpec allows `[effects(io_out)] log()` anywhere a form is allowed; parser treats leading `[...]` as a binding and rejects `()`).
- ✓ Support brace constructor forms in value positions (`Type{ ... }` in arguments/returns); parser currently requires `()` or treats braces as bindings.

**Types & Semantics**
- ✓ Implement software numeric envelopes (`integer`, `decimal`, `complex`) or document their explicit rejection in the syntax spec.
- ✓ Align `int`/`float` aliases with the spec’s target-chosen widths (code currently fixes them to `i32`/`f32`) or update the docs to match.
- ✓ Add template monomorphization for user-defined definitions (templates are parsed but only builtins/collections use them today).
- ✓ Complete struct lowering (layout metadata, alignment enforcement, `Create`/`Destroy` semantics, and backend emission).
- ✓ Implement `copy`/`public`/`private`/`package`/`static` binding semantics and metadata (currently validated but unused).
- ✓ Allow non-primitive `Pointer<T>`/`Reference<T>` targets (or document the primitive-only restriction enforced today).
- ✓ Allow untagged definitions to be used as struct types in bindings (docs say struct tags are optional for instantiation).
- ✓ Treat `if` block envelope names as ignored even if they collide with definitions (branch blocks should not resolve to defs).
- ✓ Align default effects behavior: docs say entry defaults include `io_out`, but code applies no default effects unless `--default-effects` is provided.
- ✓ Implement binding initializer block semantics: `{ ... }` should allow multi-statement blocks and `return(value)` with last-expression value; current parser treats multiple expressions as constructor args (requires explicit type) and forbids `return`.

**Backends & IR**
- ✓ Add GLSL backend (docs mention GPU lowering, but there is no backend in `src/`).
- ✓ Add SPIR-V output for the GLSL backend.
- ✓ Expand VM/native float support (parser/semantics/C++ emitter accept floats; VM/native reject float literals/types).
- ✓ Expand VM/native `/math/*` coverage (docs list full math set; VM/native currently support only a subset like abs/sign/min/max/clamp/lerp/pow).
- ✓ Implement VM/native string indexing (`at` / `at_unsafe`) for string literals/bindings or update the SyntaxSpec (native lowerer rejects string indexing).
- ✓ Update PSIR version history in docs (serializer is at v12; docs list up to v12).

**Docs Alignment**
- ✓ Clarify VM/native string limits in `docs/PrimeStruct_SyntaxSpec.md`: count/indexing currently only work for string literals or bindings backed by literals (argv-derived bindings are print-only).
- ✓ Document the `repeat(count) { ... }` statement builtin (count accepts integers or bool; non-positive counts skip the body).
- ✓ Align block call syntax with `docs/PrimeStruct.md` (require `block()` before a body block).
- ✓ Document map literal `key=value` syntax supported by the `collections` text filter.
- ✓ Document unary `&` / `*` operator sugar (`location` / `dereference`) in the `operators` text transform or remove the feature.
- ✓ Document single-letter float suffix `f` (treated as `f32`) or reject it in the lexer/parser.
- ✓ Fix string literal rules: single-quoted strings are raw and canonical strings use `utf8`/`ascii` escapes.
- ✓ Document canonical string normalization: parser emits `raw_utf8` / `raw_ascii` with unescaped content, not the escaped `"..."utf8/ascii` form described in the spec.
- ✓ Document vector helper calls (`push`, `pop`, `reserve`, `clear`, `remove_at`, `remove_swap`) as statement-only or allow them in expression contexts.
- ✓ Add missing `/math/*` builtins present in code: `floor`, `ceil`, `round`, `trunc`, `fract`, `is_nan`, `is_inf`, `is_finite`.
- ✓ Add PathSpace builtins (`notify`, `insert`, `take`) to `docs/PrimeStruct_SyntaxSpec.md` (currently only in the design doc).

**Spec Completion & Open Design**
- ✓ Finalize the project charter/deferred-features list in `docs/PrimeStruct.md`.
- ✓ Create a risk log for open questions (borrow checker, capability taxonomy, GPU constraints) and mitigation plan.
- ✓ Finalize capability taxonomy and decide which fields are surfaced in tooling vs runtime-only logs.
- ○ Define the diagnostics/tooling roadmap (source maps, error reporting pipeline, incremental tooling, PathSpace-native editor plan).
- ✓ Finalize transform phases documentation and confirm phase ordering/auto-deduction rules.
- ○ Audit and document supported text/semantic transforms; consider adding a CLI listing or richer diagnostics for unsupported transforms.
- ✓ Document transform applicability limits (e.g., alignment/layout/placement not allowed on executions).
- ○ Close the SyntaxSpec draft: review feedback, reconcile grammar, and mark `docs/PrimeStruct_SyntaxSpec.md` as stable.
- ○ Decide on Unicode identifier support and update lexer/parser/docs accordingly.
- ○ Finalize the literal suffix catalog, including canonical raw string behavior across backends.
- ○ Define composite-constructor defaults and validation rules for multi-argument initialization blocks.
- ○ Finalize literals & composite construction semantics beyond suffix rules.
- ○ Decide on software numeric envelope support (`integer`/`decimal`/`complex`) in the main design doc and align with implementation.
- ○ Specify pointer qualifier syntax and aliasing rules (e.g., restrict/readonly) in the core language.
- ○ Finalize pointers & references semantics (beyond qualifiers/aliasing).
- ○ Define pointer/reference target envelope whitelist and conversion rules per backend.
- ○ Define constructor semantics beyond `Create`/`Destroy`, including constant member behavior and default initialization rules.
- ○ Finalize struct/envelope category rules, placement policy, and category mapping.
- ○ Implement `stack`/`heap`/`buffer` placement transforms or remove them from the supported transform list.
- ○ Support recursive struct layouts or document them as explicitly unsupported.
- ○ Implement the `class<Name>(...)` surface (composition/extends semantics) or remove it from docs.
- ○ Finalize lambdas & higher-order function semantics (captures, purity, backend support).
- ○ Define borrow-checker/resource rules (pod/handle/gpu_lane lifetimes) or remove borrow-checking claims.
- ○ Finalize core library surface/standard math spec and document backend-specific limits.
- ○ Define numeric operator type-compatibility rules (signed/unsigned mixing, pointer arithmetic) per backend.
- ○ Define math builtin operand type rules and backend-specific restrictions (e.g., integer pow errors).
- ○ Finalize and version the Standard Library Reference (currently draft v0).
- ○ Decide on standard library package versioning and implement `--stdlib-version` (or remove the planned flag).
- ○ Define and implement stdlib conformance markers for VM/native subset differences.
- ○ Define PathSpace runtime integration beyond `notify`/`insert`/`take` (host hooks, scheduling, effects).
- ○ Define GPU backend constraints (allowed effects, memory model, supported envelope set, and determinism rules).
- ○ Define execution metadata (scheduling scope, instrumentation fields) and thread it through IR/backends.
- ○ Finalize the IR definition/PSIR spec and mark it stable in `docs/PrimeStruct.md` (includes opcodes, module layout).
- ○ Add PSIR versioning policy/migrations (forward/backward compatibility) beyond “unsupported IR version.”
- ○ Finalize the runtime stack model doc and align VM/emitters with the specified frame semantics.
- ○ Finalize VM design section (bytecode model, execution semantics, memory/GC strategy).
- ○ Define reference counting / heap value lifetime model for VM/native backends.
- ○ Specify tail execution optimization semantics and when backends are allowed/required to apply it.
- ○ Define a per-backend type support matrix (allowed binding/return/local types by backend).
- ○ Specify allowed binding/return/convert target types and keep diagnostics in sync with the spec.
- ○ Document backend effect/capability allowlists and align diagnostics for unsupported effects.
- ○ Define the supported IR opcode subset per backend (native/VM/GLSL) and align validation to reject unsupported cases earlier.
- ○ Enumerate native emitter unsupported IR opcodes and add tests/allowlist documentation.
- ○ Enumerate GLSL emitter unsupported statement/expression cases and add tests/allowlist documentation.
- ○ Enumerate VM backend unsupported opcodes/runtime errors and add tests/allowlist documentation.
- ○ Document runtime error reporting/exit codes for VM/native backends (e.g., vector capacity, invalid pow exponent).
- ○ Expand GLSL emitter coverage (statements/expressions) or document explicit GLSL backend restrictions.
- ○ Decide whether executions should be emitted by the C++ emitter (implement or document as permanently ignored).
- ○ Implement Metal backend output or remove it from the backend roadmap.
- ○ Decide on LLVM backend support (implement or remove future-LLVM references).
- ○ Add optional chunk caching and/or LLVM-backed JIT or remove from the optimization roadmap.
- ○ Scope and plan IDE/LSP integration once the compiler stabilizes.
- ○ Decide whether `tools/PrimeStructc` stays a minimal subset or is brought in sync with the main spec.
- ○ For `tools/PrimeStructc`, decide whether to support template codegen or explicitly lock it to a minimal subset.
- ○ For `tools/PrimeStructc`, decide whether to honor include `version="..."` (or document that it is ignored).
- ○ Refresh README once the spec stabilizes (current status, backend support, and roadmap).
