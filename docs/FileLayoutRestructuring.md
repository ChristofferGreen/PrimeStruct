# File Layout Restructuring

Status: Proposed

This document proposes restructuring the flat file layouts in `tests/unit/`,
`include/primec/`, and the top-level `src/` directory to improve discoverability,
CMake readability, and logical coupling visibility.

## Current State

| Area | Files | Issue |
|---|---|---|
| `tests/unit/` | 523 flat files | 200+ character names, no grouping by concern |
| `include/primec/` | 67 flat headers | Growing fast, no pipeline-stage grouping |
| `src/` (top-level) | ~20 loose `.cpp` files | VM, IR, and pipeline files lack directory grouping |

### Test file prefix distribution (top 10)

```
  208 compile_run
  157 ir_pipeline
   41 semantics_calls
   12 parser_basic
   11 semantics_entry
   11 semantics_capabilities
    8 semantics_bindings
    8 parser_errors
    7 text_filter
    7 semantics_manual
```

### Source subdirectory sizes

```
src/semantics:       219 files
src/ir_lowerer:      204 files
src/emitter:         116 files
src/parser:           11 files
src/text_filter:      11 files
src/native_emitter:    9 files
src/wasm_emitter:      9 files
src/glsl_emitter:      8 files
```

## Proposed Layout

### 1. Tests: mirror source module structure

```
tests/unit/
├── main.cpp                              # doctest entry (stays)
├── ast/                                  # 4 files
│   ├── test_ast_ir_dump.cpp
│   ├── test_ast_ir_dump_helpers.h
│   ├── test_ast_ir_dump_inference.cpp
│   └── test_ast_ir_dump_namespace.cpp
├── compile_run/                          # ~208 files
│   ├── bindings/
│   │   ├── test_compile_run_bindings_basic.cpp
│   │   ├── test_compile_run_bindings_and_examples.cpp
│   │   └── ...
│   ├── emitters/                         # 29 files
│   │   ├── test_compile_run_emitters.cpp
│   │   ├── test_compile_run_emitters_helpers.h
│   │   └── ...
│   ├── examples/                         # 13 files
│   │   └── ...
│   ├── imports/
│   │   └── ...
│   ├── map_conformance/                  # 7 files
│   │   └── ...
│   ├── native_backend/                   # 50 files
│   │   ├── test_compile_run_native_backend_core.cpp
│   │   └── ...
│   ├── smoke/                            # 18 files
│   │   └── ...
│   ├── text_filters/                     # 30 files
│   │   └── ...
│   ├── vector_conformance/               # 5 files
│   │   └── ...
│   ├── vm/                               # 35 files
│   │   └── ...
│   └── ...                               # remaining compile_run files
├── ir_pipeline/                          # ~157 files
│   ├── backends/                         # 20 files
│   ├── conversions/                      # 14 files
│   ├── serialization/                    # 15 files
│   ├── to_cpp/
│   ├── to_glsl/
│   ├── validation/                       # 93 files — largest cluster
│   ├── wasm/
│   ├── test_ir_pipeline.cpp
│   └── test_ir_pipeline_helpers.h
├── parser/                               # ~20 files
│   ├── basic/
│   └── errors/
├── semantics/                            # ~100 files
│   ├── bindings/
│   ├── calls_and_flow/
│   ├── capabilities/
│   ├── entry/
│   ├── result_helpers/
│   ├── type_resolution/
│   └── manual/
├── text_filter/                          # 7 files
├── vm_debug/                             # 3 files
├── compile_time/                         # 3 files
└── import_resolver/                      # 3 files
```

### 2. Include: group by pipeline stage

```
include/primec/
├── ast/                                  # Ast.h, AstMemory.h, AstPrinter.h, AstCallPathHelpers.h
├── frontend/                             # Lexer.h, Parser.h, FrontendSyntax.h, ImportResolver.h,
│                                         # SemanticProduct.h, ExpandedSource.h, CompileTimeCallable.h,
│                                         # CompileTimeEvaluation.h, CompileTimeValue.h, Token.h,
│                                         # StringLiteral.h, SymbolInterner.h
├── semantics/                            # Semantics.h, SemanticValidationPlan.h, SemanticValidationResult.h,
│                                         # SemanticsDefinitionPrepass.h, SemanticsDefinitionPartitioner.h,
│                                         # SemanticReturnDependencyOrder.h, SemanticsBenchmark.h,
│                                         # Semantics.h, ...
├── ir/                                   # Ir.h, IrPrinter.h, IrSerializer.h, IrValidation.h, IrInliner.h,
│                                         # IrLowerer.h, IrPreparation.h, IrVirtualRegister*.h,
│                                         # SoaPathHelpers.h, StdlibCollectionPaths.h
├── backend/                              # Emitter.h, GlslEmitter.h, IrToCppEmitter.h, IrToGlslEmitter.h,
│                                         # NativeEmitter.h, WasmEmitter.h, IrBackendProfiles.h, IrBackends.h
├── runtime/                              # Vm.h, VmExecutionKernel.h, VmDebugAdapter.h, VmDebugDap.h,
│                                         # VmKernelBoundary.h
├── support/                              # Diagnostics.h, EmitKind.h, Options.h, OptionsParser.h,
│                                         # ProcessRunner.h, SourceLocationMapper.h, TempPaths.h,
│                                         # ExternalTooling.h, StdlibSurfaceRegistry.h, TransformRules.h,
│                                         # TransformRegistry.h, TextFilterPipeline.h
├── semantic_product/                     # (already exists)
├── testing/                              # (already exists)
└── pipeline/                             # CompilePipeline.h, CliDriver.h
```

### 3. Top-level src: consolidate loose files

| Current location | New location |
|---|---|
| `src/Vm.cpp` | `src/runtime/Vm.cpp` |
| `src/VmDebug*.cpp` (6 files) | `src/runtime/` |
| `src/VmExecution.cpp` | `src/runtime/` |
| `src/VmExecutionKernel.cpp` | `src/runtime/` |
| `src/VmExecutionNumeric.cpp` | `src/runtime/` |
| `src/VmHeapHelpers.cpp` | `src/runtime/` |
| `src/VmIoHelpers.cpp` | `src/runtime/` |
| `src/VmKernelBoundary.cpp` | `src/runtime/` |
| `src/VmControlFlowOpcodeShared.cpp` | `src/runtime/` |
| `src/VmNumericOpcodeShared.cpp` | `src/runtime/` |
| `src/IrPrinter.cpp`, `IrPrinterHelpers.cpp` | `src/ir/` |
| `src/IrSerializer.cpp` | `src/ir/` |
| `src/IrValidation.cpp` | `src/ir/` |
| `src/IrInliner.cpp` | `src/ir/` |
| `src/IrPreparation.cpp` | `src/ir/` |
| `src/IrVirtualRegister*.cpp` (6 files) | `src/ir/` |
| `src/IrBackendProfiles.cpp` | `src/ir/` |
| `src/CompilePipeline.cpp` | `src/pipeline/` |
| `src/CliDriver.cpp` | `src/pipeline/` |
| `src/main.cpp` | `src/bin/` (or keep at top) |
| `src/primevm_main.cpp` | `src/bin/` (or keep at top) |
| `src/ImportResolver.cpp`, `ImportResolverHelpers.cpp` | `src/frontend/` |

The `emitter/`, `ir_lowerer/`, `parser/`, `text_filter/`, `native_emitter/`,
`glsl_emitter/`, `wasm_emitter/` directories stay as-is.

## Migration Strategy

### Phase 1: Tests (lowest risk, highest impact)

1. Create the target directory structure under `tests/unit/`.
2. Use `git mv` for every file to preserve history.
3. Update the CMake source lists in `CMakeLists.txt` to reflect new paths.
4. Verify: `./scripts/compile.sh --release` passes with zero failures.
5. Commit as one atomic change per directory group.

Suggested commit order:
- `tests/unit/ir_pipeline/` (157 files, biggest win)
- `tests/unit/compile_run/` (208 files)
- `tests/unit/semantics/` (~100 files)
- `tests/unit/parser/` (~20 files)
- Remaining small groups in one commit

### Phase 2: Include headers (medium risk)

1. Create target directories under `include/primec/`.
2. `git mv` headers into new locations.
3. Run `grep -r '#include.*primec/' src/ tests/` to find all consumers.
4. Update include paths in source files.
5. Run `scripts/check_include_layers.py` and update `scripts/include_layer_allowlist.txt`.
6. Verify: `./scripts/compile.sh --release` passes.

### Phase 3: Top-level src consolidation (lowest priority)

1. Create `src/runtime/`, `src/ir/`, `src/pipeline/`, `src/frontend/`, `src/bin/`.
2. `git mv` the loose files.
3. Update `CMakeLists.txt` source lists.
4. Verify: `./scripts/compile.sh --release` passes.

## Risks and Mitigations

| Risk | Mitigation |
|---|---|
| Include-path churn | Phase 2 is last; use a transitional redirect layer if needed |
| CMake source list breakage | Verify after each phase with full release gate |
| Test binary name changes | **Do not rename test binaries.** Only move source files. |
| `check_include_layers.py` breakage | Update the allowlist alongside Phase 2 |
| History blur from `git mv` | `git mv` preserves rename tracking in `git log --follow` |

## Acceptance Criteria

- All test binary names and CTest case names remain identical.
- `./scripts/compile.sh --release` passes with zero failures after each phase.
- `scripts/check_include_layers.py` passes after Phase 2.
- `scripts/check_include_layers.py` allowlist is updated, not expanded.
- `grep -r` for moved file paths returns zero stale hits.

## Test Name Quality Analysis

Full inventory: `tests/TEST_INVENTORY.md` (10,022 test cases across 459 files).

### Summary

| Metric | Count |
|---|---|
| Total test cases | 10,022 |
| Total test files | 459 |
| Names >120 chars (too long) | 53 |
| Names <20 chars (too vague) | 12 |
| `"compiles and runs"` prefix | 740 |
| Duplicate names across files | 8 |

### Problem Categories

#### 1. Overlong names (>120 characters)

These names are unreadable in test output and CI logs. They embed entire
diagnostic descriptions instead of naming the behavior under test.

Examples:
- `experimental soa_vector direct return inline location method-like
  borrowed helper-return reads reject retired helper path`
- `C++ emitter keeps canonical map unknown-target diagnostics on
  direct-call wrapper-returned canonical map reference access`
- `rejects vector alias compatibility template forwarding when unknown
  expected meets vector envelope binding in C++ emitter`

**Target**: rewrite each to focus on the behavior being verified, not the
internal mechanism. A name should answer "what does this prove?" in under
80 characters.

#### 2. Repeated `"compiles and runs"` prefix (740 cases)

740 test names start with `compiles and runs`. This prefix adds no
information — every compile-run test compiles and runs. The meaningful
part is what feature or behavior is being exercised.

Examples:
- `compiles and runs native map at helper` → `native map at helper`
- `compiles and runs native bool map access helpers` → `native bool map access helpers`
- `compiles and runs vm map at_unsafe helper` → `vm map at_unsafe helper`

**Target**: drop the `compiles and runs` prefix entirely. The test file
name and module grouping already convey that this is a compile-run test.

#### 3. Duplicate names across files

8 test names appear in multiple files, which makes CI failure
identification ambiguous.

| Duplicate name | Files |
|---|---|
| `pointer plus accepts i64 offsets` | `test_semantics_bindings_pointers.cpp`, `test_ir_pipeline_pointers.cpp` |
| `block expression requires a value` | `test_semantics_manual_types.h`, `test_semantics_calls_and_flow_control_blocks.h` |
| `runs vm with map at_unsafe helper` | `test_compile_run_vm_maps.cpp`, `test_compile_run_vm_collections_vector_aliases_b.cpp` |
| `vector stdlib namespaced capacity expression keeps canonical precedence` | 2 files |
| `vector stdlib namespaced capacity expression keeps return mismatch diagnostics` | 2 files |
| `C++ emitter rejects canonical vector mutator methods with alias-only helper before emission` | 2 files |
| `ir lowerer inline param helper aliases pure pointer soa_vector variadic forwarding` | 2 files |
| `rejects vm vector method alias access struct method chain with array receiver diagnostics` | 2 files |

**Target**: disambiguate by prefixing with the test module or rewriting to
name the distinct behavior each test covers.

#### 4. Opaque shard suffixes (63 files, 19 base names)

When a test file exceeds the doctest size guardrail (10 SUBCASE blocks),
it gets split into shard files with letter suffixes: `_a.cpp`, `_b.cpp`,
`_c.cpp`, etc. The letters are arbitrary — they carry no information about
what subset of tests each shard covers.

Largest shard groups:

| Base name | Shards | Tests |
|---|---|---|
| `test_compile_run_text_filters_diagnostics` | 22 (`_a` through `_v`) | 244 |
| `test_compile_run_vm_collections_wrapper_temporaries` | 3 | 240 |
| `test_compile_run_vm_collections_shim_maps` | 3 | 121 |
| `test_compile_run_native_backend_collections_shims_maps` | 3 | 101 |
| `test_compile_run_vm_collections_vector_aliases` | 2 | 74 |
| `test_parser_basic_semantic_transforms` | 2 | 68 |
| `test_compile_run_native_backend_collections_vector_alias_rejects` | 2 | 61 |
| `test_compile_run_native_backend_core_image_io` | 4 | 37 |

When a CI failure hits `test_compile_run_text_filters_diagnostics_n.cpp`,
the letter `n` tells you nothing — you have to open the file to find out
it covers "collect-diagnostics keeps user wrapper count capacity pair
missing-arg diagnostics."

**Target**: rename each shard file to describe its content. For example:
- `_a.cpp` → `_parse_and_multi_payload.cpp`
- `_b.cpp` → `_argument_shape.cpp`
- `_c.cpp` → `_wrapper_index_and_count_capacity.cpp`

Keep the base name as-is; only replace the opaque letter with a short
topic tag. This preserves the shard split while making CI output
self-documenting.

#### 5. Vague or overly short names

12 names under 20 characters lack specificity. In CI output, `parses loop
form` tells you nothing about *what* about the loop form is being tested.

Examples:
- `parses loop form` → `parses loop form with body and condition`
- `runs program in vm` → `vm runs hello world entry point`
- `ir lowers clamp` → `ir lowers clamp with i32 operands`

**Target**: every name should be specific enough to identify the failure
without reading the test body.

### Recommended naming conventions

1. **Drop action prefixes** that are already implied by the test binary:
   no `compiles and runs`, `parses`, `rejects`, or `ir lowers` at the
   start unless they distinguish positive from negative cases.
2. **Lead with the feature or surface**: `map at_unsafe helper`, `vector
   alias template forwarding`, `result why method calls`.
3. **Negative cases**: use `rejects` or `diagnoses` to signal expected
   failure, but keep it short — `rejects map constructor odd args`.
4. **Max 80 characters** for a test name. If it's longer, the test is
   probably testing too many things at once — split it.
5. **No duplicate names** across the entire test suite. Prefix with the
   module if a behavior is tested in multiple contexts.
6. **Shard file suffixes** must be topic tags, not letters. When a file is
   split for the doctest size guardrail, name each shard after its content
   cluster (e.g., `_argument_shape.cpp`, `_wrapper_methods.cpp`).

### Migration approach

This is a low-risk cosmetic change — test names and file names affect no
behavior. Do it in batches by test binary, verifying
`./scripts/compile.sh --release` passes after each batch:

1. **Duplicates** (8 names, highest CI ambiguity)
2. **Shard renames** (63 files across 19 base names)
3. **Overlong names** (53 names >120 chars)
4. **`compiles and runs` prefixes** (~740 names)
5. **Vague/short names** (12 names <20 chars)

## Oversized Files

### Source files (`.cpp` + `.h` under `src/`)

Total: 241,774 lines across 663 files.

| Size range | Files | Notes |
|---|---|---|
| 3000+ lines | 3 | `SemanticsValidate.cpp` (8025), `SemanticsValidatorExprMethodTargetResolution.cpp` (3765), `TemplateMonomorphExpressionRewrite.h` (3226) |
| 2000–3000 | 7 | Mostly `IrLowerer*.h` include-only fragments |
| 1000–2000 | 41 | Mix of semantics and IR lowerer files |
| 500–1000 | 118 | Acceptable range |
| <500 | 494 | Small and focused |

#### Problem 1: Include-only `.h` fragments

15 headers under `src/` contain function implementations directly with no
`#include` directives — they are textually included into a single `.cpp`
file. The largest:

| File | Lines | Statements |
|---|---|---|
| `src/semantics/TemplateMonomorphExpressionRewrite.h` | 3,226 | ~1,221 |
| `src/ir_lowerer/IrLowererLowerSumHelpers.h` | 2,876 | ~1,193 |
| `src/ir_lowerer/IrLowererLowerStatementsExpr.h` | 2,615 | ~1,010 |
| `src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h` | 1,674 | ~642 |
| `src/semantics/TemplateMonomorphImplicitTemplateInference.h` | 1,361 | — |
| `src/ir_lowerer/IrLowererLowerEmitExprTryHelpers.h` | 1,239 | — |
| `src/ir_lowerer/IrLowererLowerInlineCalls.h` | 1,021 | — |
| `src/semantics/TemplateMonomorphExperimentalCollectionReceiverResolution.h` | 894 | — |
| `src/semantics/TemplateMonomorphCoreUtilities.h` | 820 | — |
| `src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h` | 818 | — |

These fragments defeat separate compilation, inflate the translation unit
that includes them, and make it impossible to build or test the contained
logic in isolation.

The IR lowerer is already being migrated toward compileable `.h/.cpp` pairs
(e.g., `IrLowererLowerEffects.{h,cpp}`). The semantics module is not yet
migrated.

#### Problem 2: `SemanticsValidate.cpp` at 8,025 lines

This file includes 13 Semantics-related headers and contains ~213 function
definitions. It is the main entry point for the semantics validation pass
but has accumulated logic that should be split into focused compilation
units.

#### Problem 3: Oversized test files

| File | Lines | Tests |
|---|---|---|
| `test_ir_pipeline_backends_registry.cpp` | 10,003 | 156 |
| `test_semantics_type_resolution_graph_snapshots.cpp` | 8,645 | — |
| `test_ir_pipeline_validation_ir_lowerer_statement_call_helper_validates_buffer_store_diagnostics.cpp` | 7,004 | — |
| `test_semantics_calls_and_flow_collections_container_error_and_result_helpers.cpp` | 5,806 | — |
| `test_stdlib_map_ownership.cpp` | 5,322 | — |

The doctest size guardrail says split at 10 SUBCASEs. The
`test_ir_pipeline_backends_registry.cpp` file has 156 TEST_CASE macros
with no split at all.

### Recommended refactoring priorities

1. **Split include-only `.h` fragments into `.h/.cpp` pairs** — each
   fragment becomes a compilation unit with a clean header interface. Start
   with the IR lowerer fragments (the migration pattern already exists),
   then tackle the `TemplateMonomorph*.h` semantics fragments.

2. **Split `SemanticsValidate.cpp`** — extract logical groups (validation
   passes, snapshot helpers, publication builders, benchmark orchestration)
   into focused `.cpp` files that include a shared context header.

3. **Split oversized test files** — apply the same doctest shard split
   pattern already used elsewhere. Name shards by topic, not letters.

4. **Split oversized test case bodies** — files like
   `test_compile_run_vm_collections_wrapper_temporaries_a.cpp` (3,519
   lines, 189 tests) should be split by feature area.

## TODO IDs

When implementing, assign these IDs from the task queue:

| ID | Title | Phase |
|---|---|---|
| TODO-4637 | Move `ir_pipeline` test shard into subdirectory | 1 |
| TODO-4638 | Move `compile_run` test shard into subdirectory | 1 |
| TODO-4639 | Move `semantics` test shard into subdirectory | 1 |
| TODO-4640 | Move `parser` + remaining test shards into subdirectories | 1 |
| TODO-4641 | Group `include/primec/` headers by pipeline stage | 2 |
| TODO-4642 | Consolidate loose top-level `src/` files into directories | 3 |
| TODO-4643 | Fix 8 duplicate test names across files | 4 |
| TODO-4644 | Rewrite 53 overlong test names (>120 chars) | 4 |
| TODO-4645 | Drop `compiles and runs` prefix from ~740 test names | 4 |
| TODO-4646 | Tighten 12 vague/short test names | 4 |
| TODO-4647 | Rename 63 opaque shard files with topic suffixes | 4 |
| TODO-4648 | Split `SemanticsValidate.cpp` into focused compilation units | 5 |
| TODO-4649 | Convert IR lowerer include-only `.h` fragments to `.h/.cpp` pairs | 5 |
| TODO-4650 | Convert `TemplateMonomorph*.h` semantics fragments to `.h/.cpp` pairs | 5 |
| TODO-4651 | Split oversized test files (10K+ lines, 100+ tests) | 5 |
| TODO-4652 | Split oversized single test case bodies (>1000 lines) | 5 |
| TODO-4653 | Add dedicated IrPrinter unit tests | 6 |
| TODO-4654 | Add [public] annotations to stdlib modules | 6 |
| TODO-4655 | Add compile-run tests for language level examples | 6 |

## Remaining Gaps

### IrPrinter has no dedicated tests

`src/IrPrinter.cpp` (with `IrPrinterInternal.h`) is only tested indirectly
through `test_ast_ir_dump*.cpp` files. There are no tests that directly
verify IrPrinter output format, edge cases, or error handling.

### Stdlib modules lack `[public]` annotations

None of the 31 `.prime` files under `stdlib/` use `[public]` on
definitions. The visibility system exists in the language but is not yet
applied to the standard library. This means all stdlib definitions are
implicitly visible to importers, regardless of whether they are intended
as public API or internal implementation.

### `internal_soa_storage.prime` is 4,842 lines

This single file is the largest in the stdlib and exceeds the line count
of any source file except `SemanticsValidate.cpp`. It should be split
alongside the `internal_*` module collapse in TODO-4633.

### Language level examples are sparse

| Level | Examples | Notes |
|---|---|---|
| `0.Concrete` | 12 | Reasonable coverage |
| `1.Template` | 2 | Needs more template examples |
| `2.Inference` | 5 | Adequate |
| `3.Surface` | 16 | Good coverage |
| `4.Transforms` | 2 | Needs more transform examples |

### Examples are mostly untested

Only 8 test references to `examples/` exist across the entire test suite.
Most language-level examples are not validated by the test gate, so they
can silently drift from the compiler's actual behavior.

### Small forwarding `.cpp` files

~10 source files under `src/` are under 30 lines — thin forwarding or
delegation wrappers. These are not harmful but add file-count overhead.
Consider inlining them into their callers when the forwarding adds no
value.
