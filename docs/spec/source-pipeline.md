# Source Pipeline and Language Levels

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **normative**.

### Source-processing pipeline
1. **Import resolver:** first pass walks the raw text and expands every `import<...>` source entry so the compiler
   always works on a single flattened source stream. The compile pipeline also records an expanded-source provenance
   ledger for that stream: primary input, imported files, stdlib auto-included files, and generated separator text get
   stable source-unit ids plus flattened and original source ranges. Diagnostic remapping still remains a later stage.
2. **Text transforms:** the flattened stream flows through ordered token-level transforms (operator sugar, collection
   literals, implicit suffixes, project-specific macros, etc.). Text transforms apply to the entire envelope (transform
   list, templates, parameters, and body). Executions are treated as envelopes with an implicit empty body, so the same
   rule applies. Text transforms may append additional text transforms to the same node.
3. **AST builder:** once text transforms finish, the parser builds the canonical AST.
4. **Template & semantic resolver:** monomorphise templates, resolve namespaces, and apply semantic transforms (effects)
   so the tree is fully resolved and contains only concrete envelopes.
5. **IR lowering:** emit the shared SSA-style IR only after templates/semantics are resolved; the base-level tree
   contains no templates or `auto`, and every backend consumes an identical canonical form.

Pipeline operating rules:
- Each stage halts on error and exposes `--dump-stage=<name>` so tooling/tests can capture the text/tree output just
  before failure.
- The current dump surface is `pre_ast`, `ast`, `ast-semantic`, `semantic-product`, `ir` (an AST rendering), and the
  lowered-module stages `ir-lowered` and `ir-optimized`. `semantic-product`
  is the lowering-facing inspection surface between `ast-semantic` and `ir`, so tooling can inspect resolved semantic
  facts without forcing users to infer them from the canonicalized AST or from lowered IR.
- `--collect-diagnostics` enables stable multi-error reporting for parse-stage failures, semantic build-map failures
  (duplicate-definition/import/invalid-transform/return-kind), semantic definition/execution pass failures across
  independent definitions/executions, and multiple intra-body call diagnostics inside a single definition/execution
  body (unresolved targets plus resolved-call argument-shape/type and flow/effect errors such as duplicate/unknown
  named arguments, count mismatches, inferable parameter type mismatches, and effect/capability subset failures).
  Other validation inside a single definition/execution body remains fail-fast.
- Text transforms are configured via `--text-transforms=<list>`. The default list enables `collections`, `operators`,
  `implicit-utf8` (auto-appends `utf8` to bare string literals), and `implicit-i32` (auto-appends `i32` to bare
  integer literals). Order matters: `collections` runs before `operators` so array/vector/SoA bracket aliases are
  normalized before generic operator rewriting.
- Semantic transforms are configured via `--semantic-transforms=<list>`.
- `--transform-list=<list>` is an auto-deducing shorthand that routes each transform name to its declared phase (text
  or semantic); ambiguous names are errors.
- Use `--no-text-transforms`, `--no-semantic-transforms`, or `--no-transforms` to disable transforms and require
  canonical syntax.
- `--ir-inline` enables a post-validation IR inlining optimization pass before VM/native/IR emission.
- `-O0`, `-O1`, `-O2`, `-O3` select an optimization level (the last flag wins). Without a flag `primevm`, `--emit=vm`, `--emit=native` and
  `--emit=optexe`/`optcpp` use `-O2`; dumps, debug sessions and traces (`--debug-json`, `--debug-dap`, `--debug-trace`,
  `--debug-replay`) and every other emit kind (serialized IR, wasm, GLSL, SPIR-V, C++, exe) use `-O0`. The optimizer runs on
  the validated IR in `prepareIrModule`, after the optional `--ir-inline` phase and before backend emission, and the
  module is re-validated afterwards; it is shared by every backend (VM, native, C++, wasm, serialized IR).
  - Passes run in a fixed order (the manifest, printed by `--opt-list`) and the whole sequence repeats until nothing
    changes, at most four rounds. Current passes: `cfg-simplify` (constant branches, constants pushed into a shared
    `&&`/`||` test jump straight to the outcome, jump threading, no-op jumps, unreachable code), `const-fold` (pure
    arithmetic, comparisons and conversions of constants, with the VM's exact semantics), `peephole` (dead push/pop
    pairs, `dup; store; pop`, `x+0`, `x*1`, `x/1`, double negation, and `cmp; push 0; ne` on a comparison result),
    and `dead-store` (stores to locals that are never read, for locals not reachable through memory), all enabled from
    `-O1`; and `copy-prop` (loads of a copy read the original local, using a forward analysis over the CFG; skipped for
    functions that take local addresses) and `loop-rotate` (a loop whose header is two pure pushes, an integer comparison
    and the exit branch gets that test copied, inverted, onto its back edge, removing one jump per iteration; VM target
    only), enabled from `-O2`. `-O3` currently selects the same passes as `-O2`.
  - `--opt-pass <name>` enables a pass regardless of level, `--no-opt-pass <name>` disables one (a disable wins), and
    unknown names are errors. A pass that does not support the target (control-flow rewriting is skipped for wasm and
    GLSL/SPIR-V; GLSL/SPIR-V run no passes) is skipped when selected by level and is an error when named explicitly.
  - `--opt-report` prints the selected passes and per-pass instruction counts on stderr, `--opt-list` lists the
    manifest and exits, and `--opt-verify-each` re-validates the module and its operand-stack consistency after every
    pass that changed it. Optimization never changes observable behavior; `scripts/differential_opt_check.py`
    compares `-O0` against optimized runs over the VM test corpus.
  - Debug sessions and traces should use `-O0`: removed instructions change instruction pointers and local contents.
  The plan, pass catalogue and roadmap are in `docs/OptimizingBackendsPlan.md`.
- `--dump-stage=ir-lowered` and `--dump-stage=ir-optimized` print the lowered IR module as text (`ir_module_v1`: string
  table, host imports, struct layouts, and one numbered instruction per line). `ir-lowered` is the module as lowering
  produced it; `ir-optimized` applies the selected `-O` level and passes first. Both work with `primec` and `primevm`.
- Release validation failures are tracked in `docs/failing_tests.md`. Every
  release test run must record newly failing doctest cases there before new
  implementation work starts, and the TODO queue must prioritize fixing those
  failures ahead of fresh feature work.

### Language levels (0.Concrete → 3.Surface)
PrimeStruct is organized into four language levels. Each higher level desugars into the level below it.
- **0.Concrete:** fully explicit envelopes only (definitions may omit an empty parameter list; executions still require
  parentheses). Definition transforms use prefix placement only (`[transforms] name(...) { ... }`). No text transforms,
  no templates, no `auto`. Explicit `return<T>`, explicit literal suffixes, canonical calls (`if(cond, then(){...},
  else(){...})`, `loop/while/for` as calls).
- **1.Template:** canonical syntax plus explicit templates (`array<i32>`, `Pointer<T>`, `convert<T>(...)`). No `auto`.
- **2.Inference:** canonical syntax plus `auto`/omitted envelopes. Implicit template parameters are resolved per call
  site, then lowered to explicit templates.
- **3.Surface:** surface syntax and text transforms (operator sugar, collection literals, indexing sugar, `if(...) {}`
  blocks, and `name() [transforms] { ... }` definition sugar) that rewrite into canonical forms.

Procedural compile-time genericity extends this ladder without changing the
lowering contract: `<...>` is the compile-time argument channel, `(...)`
remains the runtime argument channel, and type-level work can be written as
ordinary left-to-right commands over named compile-time facts. For example, a
generic helper can bind `[type] ValueT { typeof<value> }`, define a local
`[struct] BoxT { ... }`, use `BoxT` as internal temporary storage, and return a
caller-known value such as `box.value`. Returning the local generated type
itself is rejected because the caller has no stable source name for that type.
These forms canonicalize before IR lowering so backends see only concrete
definitions, calls, and types.

### Compilation model (v1)
- **Whole-program by default:** `import` expansion produces a single compilation unit, and semantic resolution runs over
  that full unit; implicit-template inference may use call sites anywhere in the expanded source. The v1 toolchain
  prioritises fast full rebuilds over incremental compilation.
- **Envelope stream boundary:** high-level features are lowered into the canonical envelope form, and backends consume
  this stable envelope stream. Emission can stream envelopes directly into IR/bytecode or native codegen without
  reintroducing surface syntax.
- **Deterministic emission:** canonicalization happens once, before backend selection, so all emitters see the same
  fully-resolved envelopes and produce consistent results.
- **Backend boundary policy:** all codegen modes consume canonical IR via
  `IrBackend`, including production aliases (`cpp`, `exe`, `glsl`, `spirv`)
  that resolve to canonical IR backend kinds before dispatch.
- **Semantic ownership boundary policy:** graph-backed inference facts, validator-local scratch state, and the published
  semantic product follow an explicit ownership split; benchmark-only legacy
  shadow comparisons must stay isolated from production
  lowering/publication paths.
