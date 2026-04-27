# AGENTS.md for PrimeStruct

## Scope
Defines naming and coding rules plus helper workflows for contributors working in this repo.
PrimeStruct is an experimental language/compiler + VM effort; update this file as the
build and layout solidify.

## Repository focus
- PrimeStruct language spec, compiler pipeline, and PrimeScript VM.
- Canonical IR and multi-backend lowering (C++/native, GPU, VM).
- Deterministic semantics and tooling (IR dumps, diagnostics, conformance tests).

## Prerequisites
- Bash-compatible shell environment.
- CMake 3.20+.
- A C++23-capable compiler (`clang++` or `g++`).
- Python 3 for helper scripts and validation utilities.
- `rg` (`ripgrep`) for repository helper scripts such as `scripts/lines_of_code.sh`.
- For coverage: `clang++`, `llvm-profdata`, and `llvm-cov` available in `PATH` (or discoverable via `xcrun` on macOS).

## Naming rules (code)
- **Types (classes/structs/enums/aliases):** PascalCase (`PrimeStructParser`, `IrNode`).
- **Functions (free/member):** lowerCamelCase (`parseModule`, `emitIr`).
- **Variables/parameters/fields:** lowerCamelCase (`nodeId`, `tokenStream`).
- **File-local helpers:** lower_snake_case is acceptable when marked `static` in a `.cpp`.
- **Constants:** PascalCase (`DefaultIncludePath`, `MaxRecursionDepth`). Avoid `k`-prefix.

## Language/design docs
- Primary design doc: `docs/PrimeStruct.md`.
- PrimeStruct code-quality and example-style guide: `docs/CodeExamples.md`.
- When semantics change, update docs first (or alongside code) and keep examples aligned.
- When changing user-facing PrimeStruct examples or style guidance, keep
  `docs/CodeExamples.md` aligned with the supported surface syntax.
- For stdlib style work, treat `stdlib/std/math`, `maybe`, `file`, `image`,
  `ui`, the public collection wrapper files, and `stdlib/std/gfx/gfx.prime`
  as style-aligned surface code; treat `stdlib/std/bench_non_math`,
  `stdlib/std/collections/collections.prime`,
  `stdlib/std/collections/experimental_vector.prime`,
  `stdlib/std/collections/experimental_map.prime`,
  `stdlib/std/collections/experimental_soa_vector.prime`,
  `stdlib/std/collections/experimental_soa_vector_conversions.prime`,
  `stdlib/std/collections/internal_*`, and
  `stdlib/std/gfx/experimental.prime` as canonical/bridge code unless an
  active TODO explicitly retargets them.
- When ownership classification changes for a public type/surface, update the canonical `core` / `hybrid` / `stdlib-owned` matrix in `docs/PrimeStruct.md` and keep the summary note in `docs/todo.md` aligned in the same change.
- If new public syntax/IR features are added, document them with a minimal runnable
  example and expected IR snippet.
- When specs change, add a matching TODO entry only if implementation, validation, or follow-up work remains open; do not add a TODO for spec changes that are already fully completed in the same change. Use an explicit docs-only/no TODO note when that exception needs to be called out.
- If a TODO item becomes too large for one code-affecting commit, split it into
  smaller `TODO-XXXX` task blocks in `docs/todo.md` with explicit `scope`,
  `acceptance`, and `stop_rule`, then complete them incrementally across separate
  commits.

## Struct helper notes
- Planned construction rule: all value construction uses braces (`Type{...}` and context-typed `{...}`), never
  call syntax. `Type(...)` is an ordinary execution/call shape only. Struct constructor braces map arguments to fields
  (positional/labeled); they do not forward to `Create()`. Implementation is tracked in `docs/todo.md`.
- Lifecycle helpers (`Create`/`Destroy`) must be `void` and accept no parameters.
- Non-lifecycle helpers only get an implicit `this` when nested inside the struct body; helpers defined as `/Type/Name`
  outside the struct should use an explicit `self` parameter if they want method-call sugar.

## Build/test workflow
- Prefer compiling the project and running tests in release mode via `./scripts/compile.sh --release`; use debug builds only when deeper debugging is needed.
- Default validation gate: run `./scripts/compile.sh --release` first and keep routine test verification in `build-release/`. Do not switch to `build-debug/` just to rerun ordinary failures faster; only do that when you specifically need debugger-oriented investigation or debug-only instrumentation.
- **Primary entry:** `./scripts/compile.sh --release` (Release build in `build-release`, runs all tests).
- **Debug entry:** `./scripts/compile.sh` (Debug build in `build-debug`, runs tests).
- **`compile.sh` options:** `--release` selects `build-release`; `--skip-tests` keeps configure/build but skips `ctest`.
- **`compile.sh` stability rule:** keep `scripts/compile.sh` limited to its current contract (default debug build+test plus the documented `--release` and `--skip-tests` options) and do not expand or refactor it unless the user explicitly asks for that change.
- **Hard RAM safety rule (non-negotiable):** never launch more than one heavy build/test command at the same time. Treat `./scripts/compile.sh`, `cmake --build`, `ctest`, and any `PrimeStruct_*_tests` binary as heavy commands and run them strictly sequentially.
- **No parallel heavy-tool orchestration:** do not use parallel tool execution for heavy build/test commands; if additional inspection commands are needed, wait until the active heavy command completes first.
- **Benchmark helper:** `./scripts/benchmark.sh --build-dir build-release` runs the benchmark suite against an existing build. Add `--report-json build-release/benchmarks/benchmark_report.json --baseline-json benchmarks/benchmark_baseline.json` for regression checks.
- **Optional Wasm runtime checks:** `./scripts/run_wasm_runtime_checks.sh` executes Wasm outputs with `wasmtime` when available and emits an explicit skip message otherwise.
- **Coverage helper:** `./scripts/code_coverage.sh` runs a clean debug coverage build, prints total function/line coverage, and writes reports to `build-debug/coverage/coverage.txt` plus `build-debug/coverage/html/`.
- **Lines-of-code helper:** `./scripts/lines_of_code.sh` reports line totals for `src/` and `include/`.
- **Test-count helper:** `./scripts/test_count.sh` reports total defined `TEST_CASE` macros under `tests/` and, when test binaries exist, sums doctest `--count` output across `build-release/PrimeStruct_*_tests`.
- **Top-lines helper:** `./scripts/top_lines_of_code.sh` reports the top files by line count across `src/`, `include/`, and `tests/` (default: top 10).
- **CTest:** prefer running from `build-release/` via `ctest --output-on-failure`; use `build-debug/` when investigating failures in more detail.
- **Direct test binary runs:** prefer executing the matching release-mode doctest binary from `build-release/` so compile-run suites can resolve `./primec` correctly. Use `PrimeStruct_backend_ir_tests` for IR-lowering contract coverage, `PrimeStruct_backend_runtime_tests` for backend-registry/runtime adapter coverage, `PrimeStruct_compile_run_tests` for compile-run suites, or the corresponding narrower release binary for parser, semantics, text-filter, or misc slices. Switch to the matching `build-debug/` binary only when deeper debugging is needed.
- **Failure triage rule:** if the full release gate fails, diagnose with the smallest relevant release-mode rerun (single `ctest` case or one release test binary slice), fix the issue, then return to the full `./scripts/compile.sh --release` gate instead of camping on long serial debug sweeps.

## Generated artifacts
- Debug builds go in `build-debug/`; release builds go in `build-release/`.
- `compile_commands.json` is exported in each build directory on every configure.
- Coverage artifacts live in `build-debug/coverage/` (`coverage.txt`, `html/`, and `PrimeStruct.profdata`).

## Semantics pipeline note
- `Semantics::validate` pass order and ownership metadata are declared by
  `semanticValidationPassManifest()` in `include/primec/SemanticValidationPlan.h`.
  Update that manifest whenever semantic validation order, AST mutation ownership,
  compatibility-rewrite status, or semantic-product publication boundaries change.
  When changing diagnostics, keep in mind template inference runs before the main validator.
- Rewrites that depend on implicit template inference (e.g., sugar that introduces templated calls)
  must run before template monomorphization.

## Tests
- Prefer deterministic, snapshot-style tests for parser/IR/transform stages.
- Include golden files for IR and diagnostic output; failures should be easy to diff.
- When adding a language feature, include at least:
  - one positive parse + IR test
  - one negative/diagnostic test
- **Doctest size guardrail:** split a doctest case by default once it grows beyond 10 `SUBCASE` blocks or equivalent subtests; prefer multiple focused `TEST_CASE`s or suite shards over one oversized umbrella case.
- **Doctest runtime guardrail:** if a doctest case with multiple subcases takes more than 5 seconds in routine release validation, split it into smaller focused cases; if a single-focus doctest still takes more than 5 seconds, optimize it or add a brief justification in the test source or nearby registration.

## TODO slicing workflow
- Follow `docs/todo.md` as the canonical open-work log; keep only `[ ]`/`[~]` tasks
  in that file and move completed tasks to `docs/todo_finished.md`.
- Use stable `TODO-XXXX` IDs and keep `Ready Now`, `Immediate Next 10`,
  `Priority Lanes`, and `Execution Queue` synchronized with task blocks.
- Every active leaf must include explicit `scope`, `acceptance`, and `stop_rule`,
  and must target a value outcome (behavior change, perf/memory gain, or
  compatibility subsystem deletion).
- If the highest-priority TODO is too large for one code-affecting commit, split it
  into explicit sub-items in `docs/todo.md` before continuing implementation.
- Keep the live execution queue short: no more than 8 leaf tasks in the `Ready Now` section at once. Additional dependency-blocked follow-up leaves may remain lower in `docs/todo.md`, but only `Ready Now` counts toward the active queue cap.

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
- **Implementation layout:** semantics is still composed from `.h` fragments included into a
  single `.cpp`; IR lowerer is being migrated toward compileable units under `src/ir_lowerer/`
  (for example `IrLowererLowerEffects.{h,cpp}`). Prefer adding new reusable lowering logic as
  explicit `.h/.cpp` units with clear interfaces instead of adding new include-only fragments.
- **CMake subsystem layout:** place new build sources in the narrowest library that fits:
  `primec_support_lib` (shared utilities), `primec_frontend_lib` (import/parser/semantics/text
  filtering), `primec_ir_lib` (IR preparation/lowering/validation tooling),
  `primec_backend_emitters_lib` (IR backend emitters and backend-only external tooling),
  `primec_runtime_lib` (VM runtime/debugger), and `primec_backend_registry_lib` (backend registry
  glue such as `IrBackends.cpp`). Keep `primec_codegen_lib`, `primec_backend_lib`, and
  `primec_lib` as compatibility umbrellas instead of adding new source files to them directly.
  Internal executables/tests should prefer direct subsystem links over those umbrella targets once
  their dependencies are explicit. When a target only needs IR preparation plus backend dispatch,
  prefer linking `primec_ir_lib` + `primec_backend_registry_lib` directly; when a target only
  needs IR preparation plus VM execution, prefer linking `primec_ir_lib` + `primec_runtime_lib`
  over the broader backend umbrellas. When a registry/adapter target only uses implementation
  libraries behind its `.cpp` boundary, prefer a narrower dedicated implementation target over a
  broad compatibility umbrella; use `PRIVATE` only when consumers do not need that implementation
  target propagated onto their final link surface.
- **Doctest target layout:** when a test source only exercises compile-pipeline/frontend/IR APIs,
  prefer a dedicated doctest binary linked to `primec_ir_lib` or `primec_frontend_lib` instead of
  routing it through one of the broader backend binaries (`PrimeStruct_backend_ir_tests`,
  `PrimeStruct_backend_runtime_tests`, `PrimeStruct_compile_run_tests`) or the `primec_lib`
  compatibility umbrella.
- **Include-layer guardrails:** `ctest` now runs `scripts/check_include_layers.py`. Public headers
  must not include `src/` or `tests/` files, production sources must not include `tests/`, and any
  remaining direct `tests -> src/` includes or `src/ir_lowerer -> src/semantics` private includes
  must be listed narrowly in `scripts/include_layer_allowlist.txt` until a stable testing, public,
  or shared API replaces them.
- **Testing helper APIs:** when a test needs stable access to parser/text-filter helper logic, add
  declarations under `include/primec/testing/` and move tests to that header before allowlisting a
  new `tests -> src/` include. When adding lowerer validation tests, prefer narrow
  `primec/testing/IrLowerer*Contracts.h` headers over the full
  `primec/testing/IrLowererHelpers.h` umbrella whenever the shard only needs one contract family.
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
- Reproduce and validate bugs in release mode first (`./scripts/compile.sh --release`, `build-release/ctest`, or the matching `build-release` test binary). Use debug-mode reruns only when the release path does not provide enough information to finish the fix.
- If the current `./scripts/compile.sh --release` or focused release `ctest` run has failing targets, prioritize reducing those failures before starting new TODO implementation slices.
- Do not claim a bug is fixed unless you can no longer reproduce it after the change.

## Git commit guidelines
- Use a clear, imperative subject line in present tense.
- The `commit-msg` hook reads `.githooks/commit-msg`, strips comment lines and trailing whitespace, and then validates the message.
- Avoid literal `\n` sequences in commit messages; use real newlines (multiple `-m` or an editor).
- The subject line must be non-empty, stay within 50 characters, start with a capital letter, and must not end with a period.
- The second line must be blank.
- A body is required: include at least one non-empty body line after the blank separator.
- Wrap every body line at 72 characters or less.
- Prefer one logical change per commit.

Example:
```
Add IR dump stage for transforms

Explain why the dump is needed and how it changes tooling workflows.
```

## Multi-agent workspaces
- Use git worktrees per agent under `workspaces/agent-<name>/PrimeStruct` where `<name>`
  is a short feature description.
- Each workspace should use its own release build dir by default (e.g., `build-release-<agent>`). Use a debug build dir only when deeper debugging is specifically needed for that workspace.
- Merge protocol: run the release validation path for that workspace first, commit, write a short summary in
  `workspaces/agent-<name>/MERGE.md`, then cherry-pick into root.
