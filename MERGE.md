# TODO-4270 Merge Summary

Branch: `codex/todo-4270-integer-template-args`
Worktree: `/Users/chrgre01/src/PrimeStruct/workspaces/agent-integer-template-4270/PrimeStruct`

## Completed

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

## Focused Validation

- `cmake --build build-release --target PrimeStruct_parser_tests`
- `cd build-release && ./PrimeStruct_parser_tests --test-suite=primestruct.parser.templates`
- `cmake --build build-release --target PrimeStruct_misc_tests PrimeStruct_compile_run_tests`
- `cd build-release && ./PrimeStruct_misc_tests --test-case="monomorphizes integer template arguments distinctly,integer template arguments cannot substitute type positions"`
- `cmake --build build-release --target PrimeStruct_compile_run_tests`
- `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"`

All focused validation above passed after test/source-lock expectation fixes.
Broad `./scripts/compile.sh --release` validation was intentionally skipped by
the lite workflow.
