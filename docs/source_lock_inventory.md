# Source-Lock Inventory

This inventory classifies source-lock tests under `tests/unit` that read
private sources, private headers, public headers as architecture proxies, or
checked-in docs/examples. Source locks are acceptable when they guard an
intentional migration boundary, but each lock should have an eventual public
contract surface when the locked detail becomes stable language/compiler
behavior.

| Source-lock surface | Classification | Current purpose | Replacement surface |
| --- | --- | --- | --- |
| `tests/unit/test_compile_run_examples_docs_locks.cpp` reading `docs/todo*.md`, `AGENTS.md`, `docs/PrimeStruct*.md`, and checked-in examples | contract-worthy | Keeps contributor guardrails, active TODO queues, skipped-doctest state, and public example docs synchronized. | Keep as a docs/example drift contract. Replace only when the docs are split into a structured manifest. |
| `tests/unit/test_compile_run_examples_spinning_cube_native.cpp` and `tests/unit/test_compile_run_examples_metal_and_browser_hosts.cpp` reading checked-in host/example assets | contract-worthy | Keeps native/browser/Metal host examples aligned with the documented runnable asset surface. | Keep as asset drift contracts until the examples have a generated manifest or fixture registry. |
| `tests/unit/test_ir_pipeline_backends_graph_contexts.h` reading public headers plus `src/CompilePipeline.cpp`, `src/IrPreparation.cpp`, `src/SemanticProduct.cpp`, `src/semantics/*`, and `src/ir_lowerer/*` | temporary migration lock | Guards semantic-product publication, backend handoff, lowerer preflight, and compile-pipeline architecture while those seams are being split into public contracts. | Replace private-source assertions with focused public API, IR, diagnostic, and semantic-product contract tests. The `CompilePipelineResult` variant-shape lock is now paired with an explicit public type/runtime contract in `test_ir_pipeline_backends_registry.cpp`. |
| `tests/unit/test_ir_pipeline_validation_semantics_validator_infer_source_delegation_stays_stable.cpp` and `tests/unit/test_ir_pipeline_validation_fragments/test_ir_pipeline_validation_semantics_expr_source_delegation_01.h` reading `src/semantics/*.h` and `*.cpp` fragments | temporary migration lock | Guards delegation across private semantic validator fragments while inference and expression validation ownership is still being carved apart. | Replace with semantic-product/type-resolution graph contracts and public validator diagnostics that do not depend on fragment filenames. |
| `tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_source_delegation_stays_stable.cpp`, `tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_dispatch_buffer_and_native_tail_wrappers.cpp`, and related lowerer source-lock shards reading `src/ir_lowerer/*` | temporary migration lock | Guards call helper, dispatch-buffer, native-tail, setup-entry, and source-composition split points during lowerer compile-unit migration. | Replace with `primec/testing/ir_lowerer_helpers/*` helper contracts, emitted IR contracts, and deterministic diagnostics. |
| `tests/unit/test_ir_pipeline_validation_emitter_expr_source_delegation_stays_stable.cpp` reading private emitter/lowerer source | temporary migration lock | Guards expression-emitter delegation while C++/GLSL/VM emission boundaries are still being separated. | Replace with emitted C++/GLSL/VM IR contract checks and backend diagnostics. |
| Raw string checks in source-lock tests that only prove a public type spelling, such as `CompilePipelineResult` being a two-arm success/failure variant | stale lock candidate | These assertions catch text drift but do not prove the public API remains usable by consumers. | Pair or replace with compile-time and runtime public API assertions. `CompilePipelineResult` now has a direct variant contract test. |

Update this inventory when adding or retiring source-lock tests that read
`src/`, private headers, or docs/examples as architecture proxies.
