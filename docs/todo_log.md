# PrimeStruct TODO Investigation Log

Dated investigation/progress notes for currently-open `docs/todo.md`
task blocks. Each section's heading matches a `TODO-XXXX` in that
file. When a task closes, fold whatever's still relevant into its
resolution note in `docs/todo_finished.md` and delete its section
here - this file only ever tracks OPEN tasks' history, same
open-work-only scope rule as `docs/todo.md` itself.

## TODO-4710

- 2026-08-13: this TODO's entire premise was moot. Every `compile_run`
  test spawns a fresh `./primec` subprocess (confirmed by TODO-4709's
  audit), so there is no shared process for a cross-test-run parse
  cache to live in - "process-local cache keyed on file path + mtime"
  has nothing to persist across, since each test gets a brand new
  process. While measuring this premise directly (`--dump-stage`
  breakdown on a minimal vector-importing compile), found the real,
  much bigger cost this TODO was gesturing at from the wrong angle: a
  SINGLE compile invocation that imports `/std/collections/vector/*`
  and uses it takes ~2.0-2.2s vs ~7-10ms for an otherwise-identical
  no-import compile - a ~250-300x difference, all CPU-bound (confirmed
  with `valgrind --tool=callgrind`), not I/O or cold-cache. The
  redundant work isn't stdlib text re-read across test PROCESSES, it's
  binding-type-name string parsing (`normalizeBindingTypeName`,
  `splitTemplateTypeName`, `splitTopLevelTemplateArgs`) re-deriving
  the same answers from scratch millions of times WITHIN a single
  process's one compile, with zero memoization. Real tracking entry
  is now TODO-5230 (closed), which fixed the memoizable part of this
  (verified: 99.99% cache hit rate, ~5.8% total retired-instruction
  reduction) and documented why the call-VOLUME itself (not the
  per-call string-parse cost) is the larger remaining piece, requiring
  deeper restructuring out of scope for a leaf-sized fix. Left open
  but pointing at TODO-5230 as the actual tracking entry, per the
  same superseded-but-not-duplicated pattern as TODO-4740 -> TODO-4804.

## TODO-4712

- 2026-08-08: TODO-4708's measurement (now resolved) found per-shard
  fixed overhead is ~5-9ms - negligible against the measured ~4748s
  total suite time. This TODO's whole premise (grow shard size to
  amortize that fixed cost) is real but now known to be **low-value**:
  even eliminating all fixed overhead from all 1954 shards entirely
  would save on the order of ~15-20s, not a meaningful fraction of
  runtime. Deprioritized relative to the real cost drivers identified
  in `docs/TestRuntimeOptimization.md`'s 2026-08-08 log entry (a
  handful of pathologically slow tests dominate total time; see
  TODO-5220/5221/5222 for the higher-ROI follow-up chain). At the
  time, not closed outright since TODO-4707 (cross-test-case
  pollution) was still open and independently worth fixing for
  correctness reasons even without the perf motivation.
- 2026-09-23 (docs/todo.md cleanup pass): both `depends_on` entries
  (TODO-4707, TODO-4708) are now closed - `blocked` would be
  inaccurate. Reclassified `deferred`: unblocked, but the 2026-08-08
  low-value finding above still stands, so this isn't worth picking up
  ahead of the `ready` items in Queue Summary without a reason to
  revisit the value case.

## TODO-4732

- 2026-07-23: investigated with real measurements before attempting a
  migration, rather than guessing at candidates. Two findings, one
  very good and one that narrows the win:
1. The infrastructure this TODO envisions ALREADY EXISTS and is
   proven at scale - it doesn't need to be built from scratch.
   `include/primec/testing/CompilePipelineDumpHelpers.h` provides
   `runCompilePipelineBackendConformanceForTesting`/
   `prepareCompilePipelineIr` (drives `primec::runCompilePipeline`
   IN-PROCESS, no subprocess, no clang) plus
   `CompilePipelineBackendConformance::findDirectCallTarget`/
   `findMethodCallTarget`/`resolvedDirectCallPath`/
   `resolvedMethodCallPath` for asserting directly on the semantic
   product's routing tables, and
   `captureSemanticBoundaryDumpsForTesting` for in-process
   ast-semantic/semantic-product/ir dump-stage text capture. This
   exact pattern is already load-bearing at scale in
   `tests/unit/semantics/test_semantics_type_resolution_graph_snapshots.cpp`
   (8722 lines). So "combine a stored artifact" doesn't need new
   golden-file tooling - it needs `compile_run` cases that are
   really routing-decision checks moved onto this existing
   in-process helper surface instead of shelling out to
   `./primec --emit=... ` + optionally running the binary.
2. The "obvious" migration candidates (compile-time REJECT cases -
   diagnostic-only, no execution) mostly don't have cost left to
   save. Verified directly on
   `test_compile_run_emitters_wrapper_map_count_sugar.cpp`'s
   "C++ emitter keeps canonical map count diagnostics on wrapper
   slash return method sugar" case: its diagnostic ("argument type
   mismatch for /std/collections/map/count parameter marker")
   fires at the `semantic` stage (confirmed via matching
   `--dump-stage semantic-product` output, including exit code 2
   and the identical diagnostic text, against the full `--emit=exe`
   invocation) - i.e. the current subprocess already fails BEFORE
   reaching clang/link, same as a golden-comparison version would.
   Timed both forms directly: ~10-11ms either way, no measurable
   win. A `grep`-based sweep for the `compileCmd`-but-no-`exePath`
   shape (reject-only tests with no execution) found ~292 matches
   across `tests/unit/compile_run/*.cpp` - a large candidate pool,
   but this timing result means most of them likely have the same
   "already short-circuits before the expensive part" property and
   would need per-case verification (not a blanket migration) to
   confirm which ones are worth moving.
- what still needs doing before a real migration: the ACCEPT-and-
   run cases are where the real clang+link+execute cost lives, but
   distinguishing "exit code is only a routing-decision proxy"
   from "exit code encodes real computed program output" (e.g.
   `bare map count through canonical helper in C++ emitter"`
   asserts the executed binary returns exactly 92, i.e. genuine
   runtime-behavior verification, not just routing) requires
   working through TODO-4709's audit output
   (`docs/TODO4709CompileRunAudit.md`) case by case to classify each
   candidate - TODO-4709 itself is closed (it was scoped audit-only,
   no migrations, and delivered exactly that); the classification
   pass is this task's own remaining scope, not a blocker on another
   open TODO. Also flagging a fidelity
   trap for whoever does the migration: the existing in-process
   helpers default to `emitKind = "native"`
   (`detail::captureCompilePipelineDumpStageFromPath`) while the
   `compile_run/*emitters*` test files are specifically exercising
   the C++ ("cpp"/exe) emitter by name - a migration must pass the
   matching `emitKind` explicitly rather than accept the default,
   or it silently tests a different backend than the original
   case intended (the same class of regression this session hit
   for real during TODO-4733's exe->vm migration, caught there via
   a before/after diff rather than assumed away).
- 2026-09-23 (docs/todo.md cleanup pass): corrected the stale
  "TODO-4709 already scoped and left undone" framing above -
  TODO-4709 is closed and did exactly what it scoped (audit only).
  `status` stays `deferred`, not `blocked`, since nothing open is
  stopping this task; it just needs the classification pass above
  done before a real migration can start.

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
