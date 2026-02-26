# AGENTS.md for PrimeStruct

## Scope
Defines naming and coding rules plus helper workflows for contributors working in this repo.
PrimeStruct is an experimental language/compiler + VM effort; update this file as the
build and layout solidify.

## Repository focus
- PrimeStruct language spec, compiler pipeline, and PrimeScript VM.
- Canonical IR and multi-backend lowering (C++/native, GPU, VM).
- Deterministic semantics and tooling (IR dumps, diagnostics, conformance tests).

## Naming rules (code)
- **Types (classes/structs/enums/aliases):** PascalCase (`PrimeStructParser`, `IrNode`).
- **Functions (free/member):** lowerCamelCase (`parseModule`, `emitIr`).
- **Variables/parameters/fields:** lowerCamelCase (`nodeId`, `tokenStream`).
- **File-local helpers:** lower_snake_case is acceptable when marked `static` in a `.cpp`.
- **Constants:** PascalCase (`DefaultIncludePath`, `MaxRecursionDepth`). Avoid `k`-prefix.

## Language/design docs
- Primary design doc: `docs/PrimeStruct.md`.
- When semantics change, update docs first (or alongside code) and keep examples aligned.
- If new public syntax/IR features are added, document them with a minimal runnable
  example and expected IR snippet.
- When specs change, add a matching TODO entry unless explicitly marked as docs-only/no TODO.

## Struct helper notes
- Struct constructor calls (`Type(...)`) map arguments to fields (positional/labeled); they do not forward to `Create()`.
- Lifecycle helpers (`Create`/`Destroy`) must be `void` and accept no parameters.
- Non-lifecycle helpers only get an implicit `this` when nested inside the struct body; helpers defined as `/Type/Name`
  outside the struct should use an explicit `self` parameter if they want method-call sugar.

## Build/test workflow
- **Primary entry:** `./scripts/compile.sh` (Debug build in `build-debug`, runs tests).
- **Release build:** `./scripts/compile.sh --release` (Release build in `build-release`).
- **Configure only:** `./scripts/compile.sh --configure` (regenerates build dir only).
- **Skip tests:** `./scripts/compile.sh --skip-tests` (build only).
- **CTest:** from `build-debug/` run `ctest --output-on-failure`.
- **Direct test binary runs:** execute `build-debug/PrimeStruct_tests` from `build-debug/` so compile-run suites can resolve `./primec`.

## Semantics pipeline note
- `Semantics::validate` runs in this order: apply semantic transforms → maybe-constructor rewrite →
  template monomorphization → convert-constructor rewrite → validator passes → omitted struct
  initializer rewrite. When changing diagnostics, keep in mind template inference runs before the
  main validator.
- Rewrites that depend on implicit template inference (e.g., sugar that introduces templated calls)
  must run before template monomorphization.

## Tests
- Prefer deterministic, snapshot-style tests for parser/IR/transform stages.
- Include golden files for IR and diagnostic output; failures should be easy to diff.
- When adding a language feature, include at least:
  - one positive parse + IR test
  - one negative/diagnostic test

## Code guidelines
- **Language:** C++23 for compiler/VM tooling unless otherwise documented.
- **Errors:** prefer explicit error types/`Expected`-style flows over exceptions.
- **Chrono:** use `std::chrono` types for durations/timeouts.
- **Logging:** use a centralized logger (to be defined) instead of `printf/std::cout` in
  production paths.
- **Parser disambiguation:** in statement position, `name{...}` is parsed as a binding; nested
  definitions must use `name()` to avoid ambiguity.
- **AST ordering:** nested definitions are appended to `Program.definitions` before their parent
  definition; build a prepass when parent metadata is needed (struct/helper relationships, etc).
- **Semantic rewrites:** `Semantics::validate` mutates the AST in-place (enum expansion, loop
  desugaring, omitted struct initializers), so backend passes should assume canonicalized forms.
- **Implementation layout:** large subsystems (semantics, IR lowerer) are composed from `.h`
  fragments included into a single `.cpp`; these fragment headers are not standalone translation units.
- **Visibility transforms:** `public`/`private` are valid on definitions (controls import
  visibility) and bindings (field visibility). Executions still reject them.
- **VM/native strings:** string values are represented as string-table indices; dynamic
  string construction is unavailable, so string returns and `Result.why` hooks must
  return literal-backed strings (entry args or string literals).
- **IR stability:** avoid silent changes to serialized IR; include versioning or migration
  notes when the format changes.
- **Determinism:** no unordered iteration that affects outputs; sort keys before emitting
  diagnostics/IR when order might vary.

## Bug-fix workflow
- Before fixing a bug, find a concrete way to reproduce it.
- Do not claim a bug is fixed unless you can no longer reproduce it after the change.

## Git commit guidelines
- Use a clear, imperative subject line in present tense.
- Avoid literal `\n` sequences in commit messages; use real newlines (multiple `-m` or an editor).
- Keep the subject line <= 50 characters, start with a capital letter, no trailing period.
- Separate subject/body with a blank line; wrap body lines at ~72 chars.
- Prefer one logical change per commit.

Example:
```
Add IR dump stage for transforms

Explain why the dump is needed and how it changes tooling workflows.
```

## Multi-agent workspaces
- Use git worktrees per agent under `workspaces/agent-<name>/PrimeStruct` where `<name>`
  is a short feature description.
- Each workspace should use its own build dir (e.g., `build-debug-<agent>`).
- Merge protocol: run tests, commit, write a short summary in
  `workspaces/agent-<name>/MERGE.md`, then cherry-pick into root.
