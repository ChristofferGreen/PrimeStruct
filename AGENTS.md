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

## Build/test workflow
- **Primary entry:** `./scripts/compile.sh` (Debug build in `build-debug`, runs tests).
- **Release build:** `./scripts/compile.sh --release` (Release build in `build-release`).
- **Configure only:** `./scripts/compile.sh --configure` (regenerates build dir only).
- **Skip tests:** `./scripts/compile.sh --skip-tests` (build only).
- **CTest:** from `build-debug/` run `ctest --output-on-failure`.

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
- **IR stability:** avoid silent changes to serialized IR; include versioning or migration
  notes when the format changes.
- **Determinism:** no unordered iteration that affects outputs; sort keys before emitting
  diagnostics/IR when order might vary.

## Git commit guidelines
- Use a clear, imperative subject line in present tense.
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
