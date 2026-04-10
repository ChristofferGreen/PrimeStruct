# Semantic Product Backend Entrypoint Inventory

## Scope

This inventory tracks compile/runtime entrypoint families that either:

- consume the semantic product and lower/emit IR, or
- are intentionally non-consuming and should avoid semantic-product allocation.

It exists to support Group 15 `P1-06` follow-up slicing.

## Production Entrypoint Families

| Family | Primary callsites | Semantic product consumption | Status |
|---|---|---|---|
| `primec` backend emit dispatch (`cpp-ir`, `exe-ir`, `native`, `vm`, `ir`, `wasm`, `glsl-ir`, `spirv-ir`) | `src/main.cpp` (`runCompilePipeline` -> `runIrBackend` -> `prepareIrModule`) | Required | Consuming |
| `primec` unknown emit kind without dump stage | `src/main.cpp` (`resolveIrBackendEmitKind` / `findIrBackend` preflight) | Not required | Non-consuming (optimized in `P1-04`) |
| `primevm` VM execute/debug-json/debug-trace/debug-dap | `src/primevm_main.cpp` (`runCompilePipeline` -> `prepareIrModule`) | Required | Consuming |
| `primevm` debug-replay trace path (`--debug-replay` without dump stage) | `src/primevm_main.cpp` (trace parse + replay output) | Not required | Non-consuming (optimized in `P1-05`, source-locked in `test_ir_pipeline_backends_registry`) |
| Compile-pipeline dump boundary (`--dump-stage ast-semantic`) | `src/CompilePipeline.cpp` | Not required | Non-consuming (optimized in `P1-01`) |
| Compile-pipeline dump boundary (`--dump-stage semantic-product`) | `src/CompilePipeline.cpp` | Required | Consuming |

## Test/Helper Entrypoint Families

| Family | Primary callsites | Semantic product consumption | Status |
|---|---|---|---|
| IR helper parse/validate path with stdlib imports | `tests/unit/test_ir_pipeline_helpers.h` (`parseAndValidateThroughCompilePipeline`) | Mixed: optional, currently requested by default | Remaining non-consuming family (`P1-06a`) |
| Compile-pipeline dump helper | `include/primec/testing/CompilePipelineDumpHelpers.h` (`captureCompilePipelineDumpStageFromPath`) | Mixed: stage-dependent | Remaining mixed family (`P1-06b`) |
| Semantics helper compile-pipeline validation path | `tests/unit/test_semantics_helpers.h` (`dumpStage = ast_semantic`) | Not required | Non-consuming (already aligned with `P1-01`) |

## Remaining Non-Consuming Families With Explicit Follow-Up Leaves

| Family | Gap to close | Follow-up leaf |
|---|---|---|
| `tests/unit/test_ir_pipeline_helpers.h` parse/validate helper | Non-semantic-output calls still request semantic products by default. | `P1-06a` |
| `include/primec/testing/CompilePipelineDumpHelpers.h` dump helper | Semantic-product intent is inferred from stage instead of explicit at callsites. | `P1-06b` |
