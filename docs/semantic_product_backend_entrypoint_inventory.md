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
| IR helper parse/validate path with stdlib imports | `tests/unit/test_ir_pipeline_helpers.h` (`parseAndValidateThroughCompilePipeline`) | Mixed: required only when semantic output is requested | Non-consuming path optimized in `P1-06a` (source-locked in `test_ir_pipeline_backends_registry`) |
| Compile-pipeline dump helper | `include/primec/testing/CompilePipelineDumpHelpers.h` (`captureCompilePipelineDumpStageFromPath`) | Mixed: stage-dependent | Explicit semantic-product intent is now required at callsites (`P1-06b`, source-locked in `test_ir_pipeline_backends_registry`) |
| Semantics helper compile-pipeline validation path | `tests/unit/test_semantics_helpers.h` (`dumpStage = ast_semantic`) | Not required | Explicit non-consuming semantic-product intent wired in `P1-12` (source-locked in `test_ir_pipeline_backends_registry`) |

## Remaining Non-Consuming Families With Explicit Follow-Up Leaves

None. The inventory currently has no remaining non-consuming families with open follow-up leaves.
