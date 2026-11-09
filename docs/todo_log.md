# PrimeStruct TODO Investigation Log

Dated investigation/progress notes for currently-open `docs/todo.md`
task blocks. Each section's heading matches a `TODO-XXXX` in that
file. When a task closes, fold whatever's still relevant into its
resolution note in `docs/todo_finished.md` and delete its section
here - this file only ever tracks OPEN tasks' history, same
open-work-only scope rule as `docs/todo.md` itself.




## TODO-4737

- 2026-07-23c: implemented the safe, verifiable slice of option (b)
  above - deferred the unsafe slice rather than force it. Added
  `tests/unit/ir_pipeline/test_ir_pipeline_validation_ir_lowerer_helpers_classifies_builtin_method_call_targets.cpp`
  (7 true cases covering every exemption, 6 false cases covering
  wrong arity/wrong call name/unknown target) as a scoped,
  machine-checked version of the gap (c) invariant for this specific
  duplication. Verified with a real before/after diff, not just a
  green run: built and ran the full 1387/1389-case
  `ir.pipeline.validation` suite twice (pre-change via `git stash`,
  then with the change restored), diffed the full sorted list of
  failing `TEST CASE:` names from both untruncated runs - byte-for-
  byte identical set of 40 pre-existing failures both times (none
  touch method-call-target exemption logic), with the new file's 2
  cases / 14 assertions passing on top. Zero regressions, zero
  fixed-by-accident. A general "runs in the lowering pipeline,
  checks every published target" invariant pass still isn't
  implemented - only the duplicated flat-string half of the
  exemption surface is single-sourced and regression-tested.
- 2026-09-03: TODO-4724 closed without a reusable extracted seam
  covering the remaining fuzzy-path-matching lambda - confirmed by
  reading its finished-task entry, it wasn't one of the seams
  landed there (that task decomposed a different function
  entirely). This item's remaining scope is unchanged.
