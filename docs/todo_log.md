# PrimeStruct TODO Investigation Log

Dated investigation/progress notes for currently-open `docs/todo.md`
task blocks. Each section's heading matches a `TODO-XXXX` in that
file. When a task closes, fold whatever's still relevant into its
resolution note in `docs/todo_finished.md` and delete its section
here - this file only ever tracks OPEN tasks' history, same
open-work-only scope rule as `docs/todo.md` itself.




## TODO-5749

- 2026-11-09: measured after TODO-5748, both versions (main.prime at
  2cead3370 and at its parent 279a3d4b3) built by the same primec,
  headless scenario `close:1`, with a probe dylib loaded through
  `DYLD_INSERT_LIBRARIES` that sums private dirty pages per VM region
  tag (`mach_vm_region_recurse`) at exit and reads `phys_footprint`.
- Native: 2.90 MB against 1.62 MB. Untagged pages 1.31 MB against
  0.28 MB (+1.03 MB): the native `HeapAlloc` maps every block on its
  own, about 80 pages of 16 KiB against 17 (Function boxes and handler
  vectors) - TODO-5750. Stack 459 KB against 229 KB (+0.23 MB): main's
  frame is 40,312 locals of 16 bytes against 16,672 because native
  inlines every registration and closure - TODO-5740. Malloc regions
  are the same.
- optexe: 1.90 MB against 1.54 MB (2.0 MB before TODO-5748).
  MALLOC_SMALL 0.51 MB against 0.21 MB (+0.30 MB): the optexe runtime's
  `VmHeapCore` slot vector - TODO-5751. Stack +16 KB (main's frame 3632
  slots against 1541).
- The first suspicion, frame size, explains only the 0.23 MB native
  stack part. Close this when TODO-5750, TODO-5751 and TODO-5740 land
  and the editor is measured again.
- 2026-11-09, after TODO-5750 (pooled arm64 heap): native 1.97-1.98 MB
  against 1.64-1.67 MB for the loop version (untagged 0.41 MB against
  0.28 MB; stack unchanged at 459 KB against 229 KB). optexe unchanged.
- 2026-11-09, after TODO-5751 (VmHeapCore reserved, zeroed storage and
  in-place growth): optexe median 1.75 MB against 1.57 MB (15 runs
  each; 1.88 MB before). The heap's touched slot storage is 64 KB; the
  rest of the gap is main's larger frame and allocator noise.

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
