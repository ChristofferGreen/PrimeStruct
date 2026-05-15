# Parallel TODO Merge Notes

## TODO-4270 Merge Summary

Branch: `codex/todo-4270-integer-template-args`
Worktree: `/Users/chrgre01/src/PrimeStruct/workspaces/agent-integer-template-4270/PrimeStruct`

### Completed

- Added typed AST metadata for template arguments so parsed calls, method calls,
  lambdas, executions, and transforms can distinguish type arguments from
  compile-time integer values.
- Extended template-list parsing to accept non-negative unsuffixed integer
  literal arguments and reject floats, strings, bools, negative values, and
  other non-template expressions with stable diagnostics.
- Updated template specialization hashing and binding to keep integer template
  arguments distinct from type arguments, and reject integer template
  parameters when they are substituted into type positions.
- Documented integer template-argument syntax and moved TODO-4270 to
  `docs/todo_finished.md`; TODO-4275 is now the ready tuple-type-packs
  successor.

### Focused Validation

- `cmake --build build-release --target PrimeStruct_parser_tests`
- `cd build-release && ./PrimeStruct_parser_tests --test-suite=primestruct.parser.templates`
- `cmake --build build-release --target PrimeStruct_misc_tests PrimeStruct_compile_run_tests`
- `cd build-release && ./PrimeStruct_misc_tests --test-case="monomorphizes integer template arguments distinctly,integer template arguments cannot substitute type positions"`
- `cmake --build build-release --target PrimeStruct_compile_run_tests`
- `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"`

## TODO-4532 Merge Notes

Branch: `codex/todo-4532-map-native-dispatch`
Worktree: `/Users/chrgre01/src/PrimeStruct/workspaces/agent-map-native-dispatch-4532/PrimeStruct`

### Summary

- Routed inline native dispatch canonical map helper checks through shared
  stdlib-surface helper classification.
- Removed inline native dispatch's local semantic map type/kind recognizers and
  uses shared `resolveMapAccessTargetInfo(...)` for map target classification.
- Removed `IrLowererInlineNativeCallDispatch.cpp` from map backing trace
  inventory rows and ratcheted its map surface cap from 39 to 9.
- Updated source-lock coverage and TODO/testcase bookkeeping.

### Validation

- `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .`
- `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_backing_traces.py --root .`
- `PYTHONDONTWRITEBYTECODE=1 python3 tests/scripts/test_check_map_surface_trace_inventory.py --repo-root . && PYTHONDONTWRITEBYTECODE=1 python3 tests/scripts/test_check_map_backing_traces.py --repo-root .`
- `git diff --check`
- `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`
- `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline dispatch map helper deferral uses semantic receiver facts first,ir lowerer inline dispatch collection access fallback uses semantic receiver facts first,ir lowerer inline native dispatch prefers published canonical map access helpers"`
- `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership`

## TODO-4526 Merge Summary

Branch: `codex/todo-4526-soa-semantic-residue`
Worktree: `/Users/chrgre01/src/PrimeStruct/workspaces/agent-soa-semantic-residue-4526/PrimeStruct`

### Completed

- Routed semantic validator SoA compatibility checks through shared helper
  APIs instead of public or legacy helper path literals.
- Reduced semantic validator SoA trace inventory rows for call resolution,
  method targets, vector helpers, statement bindings, snapshots, and
  initializer inference.
- Updated source-lock tests for shared SoA helper routing, the retired
  `soa_vector.prime` module boundary, and the completed TODO queue state.
- Moved TODO-4526 to finished history and promoted TODO-4527 as the next
  SoA zero-audit successor.

### Validation

- `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_soa_surface_trace_inventory.py --root .`
- `git diff --check`
- `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`
- `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"`
- `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"`

Focused validation above passed in the worker worktrees. Broad
`./scripts/compile.sh --release` validation was intentionally skipped by the
lite workflow.
