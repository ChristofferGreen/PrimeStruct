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
- The current dump surface is `pre_ast`, `ast`, `ast-semantic`, `semantic-product`, and `ir`. `semantic-product`
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
- `-O0`, `-O1`, `-O2`, `-O3` select an optimization level (default `-O0`; the last flag wins), and
  `--opt-pass <name>`, `--no-opt-pass <name>`, `--opt-report`, `--opt-verify-each` refine it. They are accepted by
  `primec` and `primevm` but no optimization passes exist yet, so every combination currently behaves like `-O0`.
  The plan and pass catalogue are in `docs/OptimizingBackendsPlan.md`.
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
