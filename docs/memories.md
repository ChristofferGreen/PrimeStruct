# Repository Memories

This file stores durable session-derived facts that are useful in later work. Keep it short, factual, and easy to diff.

## Active Memories

### compile-run-tests-shell-out-to-build-binaries
- Updated: 2026-04-19
- Tags: tests, build, tooling
- Fact: Compile-run suites invoke `./primec` and `./primevm` from the active build directory, so focused reruns need fresh standalone tool binaries and not just freshly linked test binaries.
- Evidence: `tests/unit/test_compile_run_smoke_core_basic.cpp` and many other compile-run suites build shell commands like `./primec --emit=...` and `./primevm ...`.

### exact-stdlib-vector-import-covers-helper-surface
- Updated: 2026-04-19
- Tags: semantics, imports, collections
- Fact: Exact `import /std/collections/vector` is special-cased to expose the bare `vector(...)` alias and to treat `/std/collections/vector/*` helper paths as imported.
- Evidence: `src/semantics/SemanticsValidatorBuildImports.cpp`, `src/semantics/SemanticsValidatorInferCollectionCompatibility.cpp`, and `src/semantics/TemplateMonomorphCoreUtilities.h` all special-case that import path, and the behavior is covered in `tests/unit/test_compile_run_imports_operations.cpp` and `tests/unit/test_semantics_calls_and_flow_collections_bare_map_call_form_statement_args.cpp`.

### source-lock-tests-read-private-semantics-fragments-directly
- Updated: 2026-04-19
- Tags: tests, semantics, source-lock
- Fact: Some source-lock IR and semantics tests read individual `src/semantics/*.h` and `*.cpp` fragments directly, including private headers, so moving logic between fragments requires updating those file-path assertions instead of assuming the assembled include surface is enough.
- Evidence: `tests/unit/test_ir_pipeline_validation_fragments/test_ir_pipeline_validation_semantics_expr_source_delegation_01.h` explicitly loads `SemanticsValidatorPrivateExprValidation.h` and many sibling fragments with `readText(...)`.

## Maintenance Notes

- Keep entries sorted by slug within the section.
- Delete wrong entries instead of leaving contradictory facts behind.
- Prefer updating an existing entry over adding a near-duplicate.
- Avoid copying obvious facts from `AGENTS.md` or canonical design docs unless the shorter memory adds unique operational value.
