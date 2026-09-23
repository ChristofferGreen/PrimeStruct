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

## TODO-4752

- 2026-08-05: **the vm-side bug is confirmed fixed** - it was
  the same root cause as TODO-4757 (the `hasScalarOrVoidReturn`
  real-call-eligibility fix in `IrLowererRecursionAnalysis.cpp`
  already excludes `ContainerError` from real-call treatment). Both
  `expectContainerErrorConformance`'s `vm` branch and all 3
  conformance TEST_CASEs (`container error contract conformance in C++
  emitter`, `native imported container error contract conformance`,
  `runs vm imported container error contract conformance`) pass
  currently. The **native-side truncation bug is still open and is
  broader than originally scoped** - it is NOT specific to `why()`,
  `ContainerError`, or unbound temporaries: `[return<string>]
  makeMsg() { return("hello world"raw_utf8) }` then `[string]
  msg{makeMsg()}; print_line(msg)` (fully bound, no field access, no
  error-struct types involved at all) still prints only `h` on
  `--emit=native`, while the identical source prints the full string
  correctly on `--emit=vm`. A literal bound directly (`[string]
  msg{"hello world"raw_utf8}`, no function call) prints correctly on
  native too - so the truncation is specific to a `string` value that
  crossed a real (non-inlined) native function-call return boundary.
  Since `"string"` is not in `isSupportedScalarTypeName`
  (`IrLowererRecursionAnalysis.cpp:14-26`), it should already be
  ineligible for real-call treatment and forced to inline the same way
  the VM path now does for the four packed-error-struct types - the
  fact that native still truncates suggests the native/ARM64/x86_64
  emitter has its own, separate real-call/struct-return-ABI path that
  doesn't consult (or isn't governed by) this same eligibility
  analysis, and that path's handling of a struct-shaped return value
  (likely a `{Pointer<u8>, i32 length}`-shaped `string`) truncates the
  length to 1 when actually going through a real native call. This is
  a materially different, native-emitter-specific investigation from
  anything already traced for TODO-4757 - needs its own gdb/trace pass
  into the native/ARM64/x86_64 backend's call-emission code (not
  `IrLowererRecursionAnalysis.cpp`, which VM already correctly
  respects) before any fix. Not fixed this session; the acceptance
  criterion's native half remains unmet, so leaving this TODO open
  despite the vm half now being correct.

## TODO-4812

- 2026-08-07: triaged finding (1) (`.push()` sugar without
  import). Confirmed the exact asymmetry: `values.count()` on a
  `[soa<Particle> mut]` local resolves and runs fine with NO import at
  all, while `values.push(...)` on the identical receiver rejects with
  "unknown call target: push" - traced to
  `resolveMethodCallPath`/`matchesBuiltinSoaCollectionHelper`
  (`SemanticsValidatorExprMethodTargetResolution.cpp`, ~line 1947),
  whose always-visible-without-import allowlist covers
  `count`/`count_ref`/`get`/`get_ref`/`to_aos`/`to_aos_ref`/ref-like
  helpers but has no `push`/`reserve` entries at all - contrasted with
  this same TODO's own cross-reference elsewhere in this file
  describing `count`/`count_ref`/`get`/`get_ref`/`ref`/`ref_ref`/
  `to_aos`/`to_aos_ref`/`push`/`reserve` as one unified "same-path
  shadow family" for OTHER purposes, suggesting push/reserve's
  exclusion here specifically could be either an oversight or a
  deliberate "read methods always visible, write methods need explicit
  import" design choice - genuinely ambiguous either way from code
  alone. Went one step further and found this isn't a clean binary
  "bug or not": testing the explicitly-typed `[SoaVector<Particle> mut]`
  sibling (not `soa<Particle>`) with the identical no-import `push`
  call produces a THIRD, different behavior - it passes semantic
  validation cleanly (no "unknown call target" at all) but then fails
  at IR LOWERING with `"vm backend only supports arithmetic/.../
  increment/decrement calls in expressions (call=/std/collections/soa/push,
  ...)"`, an entirely different rejection class. So the three receiver
  spellings (`soa<Particle>` without import, `SoaVector<Particle>`
  without import, either with import) each hit a different code path
  with different behavior for the exact same logical operation - this
  is more tangled than a single allowlist gap and needs a design
  decision (should push/reserve require import like write-mutators
  elsewhere, or be uniformly visible like their sibling family members)
  before a fix should be attempted; not fixed this session, leaving
  for a session that can get that design question answered first
  rather than guess.

## TODO-4800

- 2026-08-06: attempted a fix by loosening
  `emitVectorIndexedAccessBeforeInline`'s
  (`IrLowererLowerEmitExprTailDispatch.h`, ~line 1142) receiver-type
  gate from requiring `targetInfo.isVectorTarget` to also accept
  `targetInfo.isArgsPackTarget`, on the theory that `.at()`/`at()`
  method-call-sugar dispatch simply wasn't reaching the same
  `emitArrayVectorIndexedAccess` machinery that already correctly
  handles args-pack targets for plain bracket-index access (confirmed
  via code reading that `validateArrayVectorAccessTargetInfo` already
  explicitly permits `isStructArgsPackTarget`/`isMapArgsPackTarget`/
  `isVectorArgsPackTarget`/etc). This did NOT fix the repro - added a
  temporary debug print (reverted) right after the gate and it never
  fired for either the minimal repro's `.at(1i32)` inner call or the
  outer `.count()` chain, meaning `emitVectorIndexedAccessBeforeInline`
  is never even reached for this call shape - some earlier guard
  (`inlineDispatchExpr.kind != Expr::Kind::Call`, the `args.size()!=2`
  check, or the `getBuiltinArrayAccessName`/`resolveVectorHelperAliasName`
  resolution at the top of the lambda) must already be diverting this
  exact case elsewhere before this function's body ever runs, or this
  whole `IrLowererLowerEmitExprTailDispatch.h` code path is only
  reachable from a different call context than the one this repro
  exercises (it's plausible "TailDispatch" is specific to certain
  positions, e.g. return-statement tails, not the general nested
  method-chain-argument position `values.at(1i32).count()` puts the
  `.at()` call in). Reverted cleanly (verified via `git diff`). Next
  step for a future session: trace with a debug print or gdb
  breakpoint starting from the OUTER `.count()` call's dispatch (since
  that's what actually fails) to find where it tries to emit its
  receiver expression (`values.at(1i32)`) and thus discover which
  actual function handles (or fails to handle) `.at()` sugar on an
  args-pack in this nested-receiver position - `IrLowererLowerEmitExprTailDispatch.h`
  may simply be the wrong file for this repro shape entirely.
- 2026-08-08: traced one layer further using `gdb -batch
  -ex "break ... -ex run -ex bt"` on a fresh, simpler repro
  (`args<Pointer<uninitialized<i32>>>`, cross-referenced from
  TODO-4760(b) - see that TODO's own note for the exact source) that,
  unlike the 2026-08-06 attempt's `.count()`-chained repro, DOES reach
  `emitVectorIndexedAccessBeforeInline`
  (`IrLowererLowerEmitExprTailDispatch.h`, ~line 1117) - confirming the
  earlier session's hypothesis that reachability of this function
  depends on the call's syntactic position (this repro's `.at()` calls
  sit inside `init(dereference(values.at(1i32)), 2i32)` /
  `take(dereference(...))` statements, not chained after `.count()`).
  Re-tried the same fix the 2026-08-06 attempt proposed (loosening the
  `targetInfo.isVectorTarget`-only gate at ~line 1146 to also accept
  `targetInfo.isArgsPackTarget`) - this time the function IS reached,
  but the fix still didn't help: added debug prints and found
  `emitVectorIndexedAccessBeforeInline` bails out even EARLIER than the
  `targetInfo` gate, at the accessName-resolution step itself (~line
  1124): for the method-call form (`values.at(1i32)`,
  `resolvedAccessPath` reported as `/array/at`), `getBuiltinArrayAccessName`
  returns FALSE - so does the equivalent bracket-index-sugar form
  (`resolvedAccessPath` `/at`), meaning accessName must be getting set
  via `resolveVectorHelperAliasName` for whichever of the two actually
  works (not confirmed which, or whether the print's "isMethodCall=0"
  line really was the bracket form and not a coincidental substring
  match against an unrelated node also containing "at", since the
  debug filter used a loose `name.find("at") != npos` check). Reverted
  both the loosened gate and all debug prints cleanly (verified via
  `git diff`) rather than land a fix that doesn't actually work. Next
  step for a future session: instrument `resolveVectorHelperAliasName`
  itself (not just `getBuiltinArrayAccessName`) to find which
  resolution path the WORKING bracket-index form actually takes, then
  check why that same path doesn't also match the method-call form -
  the two forms clearly diverge before `emitVectorIndexedAccessBeforeInline`'s
  `targetInfo`/`isMethodCall` gates are ever reached, so fixing those
  gates (as both this and the 2026-08-06 attempt did) treats a symptom
  one layer too late.

## TODO-4801

- 2026-08-07: confirmed the bare method-call-sugar form
  (`count_ref(location(values))`) doesn't even exist as a callable
  spelling (`unknown call target: count_ref` at the semantic layer),
  and that the direct-call rejection also fires identically when the
  call result is first bound to a local rather than used inline in
  `return(...)` - ruling out both alternate framings the
  implementation_notes suggested checking. Found the dispatch chain in
  `IrLowererLowerStatementsExpr.h` (~line 1718) that handles this exact
  call shape for `count` specifically: it checks
  `resolveSameFamilyKeyValueHelperMemberName(...) == "count"` gated by
  `hasSemanticKeyValueHelperDefinition("count")`, then rewrites to the
  canonical helper path and emits an inline definition call - `count_ref`
  is never included in this check anywhere in the file (confirmed via
  grep - only bare `"count"` string-literal comparisons exist, no
  `"count_ref"` ones in any of the several map/vector count-dispatch
  blocks in this file). Attempted the obvious fix (add
  `|| keyValueCountHelperName == "count_ref"` alongside the existing
  `"count"` check at that site) and rebuilt/retested - it made no
  difference at all to the observed rejection, meaning this exact
  block either never gets reached for `count_ref` (some earlier guard
  in the same large `if`, e.g. the `keyValueHelperMetadata() != nullptr`
  check a few lines up, may already fail before this point) or
  `hasSemanticKeyValueHelperDefinition("count_ref")` itself returns
  false (i.e. `count_ref` may not be registered as a recognized stdlib
  surface member for maps at all, unlike vector's `count_ref`/`at_ref`
  family) - not disambiguated this session. Reverted the one-line
  attempt cleanly (verified via `git diff`) rather than land a no-op
  change. This has the same "single guarded dispatch site with several
  plausible failure points, none individually confirmed" shape as the
  exhausted TODO-4756 investigation - next session should add a debug
  print at the `keyValueHelperMetadata()`/`hasSemanticKeyValueHelperDefinition`
  checks specifically (not just the leaf-name comparison this session
  tried) to see which one actually rejects `count_ref` before
  attempting another fix.

## TODO-4806

- 2026-08-06: per the stop_rule, reproduced the direct-
  call form (`count(/vector/at(wrapValues(), 0i32))`, no slash-method
  chaining) - it fails identically with the same "struct parameter
  type mismatch" message, so this is NOT slash-method-call-specific;
  the bug is the broader "any call forwarding a helper-return vector
  into /vector/at inside count(...)" gap the stop_rule warned about.
  Root-caused via code reading (no instrumentation needed once the
  right function was found): `isWrapperReturnedKeyValueAccessCall` in
  `IrLowererLowerEmitExpr.h` (checked unconditionally at the very top
  of `emitExpr`'s `Expr::Kind::Call` case, before any other dispatch)
  unconditionally emits `"struct parameter type mismatch"` whenever
  `count(...)`'s single argument is itself a call whose resolved path
  or namespace-scoped name's trailing segment is `at`/`at_unsafe`/
  `at_ref`/`at_unsafe_ref` AND that call's own first argument is itself
  a call (the helper-return receiver, e.g. `wrapValues()`). The
  intended scope (per the name) is presumably map/key-value `at()`
  receivers whose element type is a struct, where wrapping a struct
  value in `count(...)` really is a mismatch - but the actual
  implementation has no such scoping: it fires for ANY `at`/`at_unsafe`
  leaf name regardless of receiver collection kind or actual return
  type, including a plain vector `/vector/at` returning `string` (a
  perfectly valid `count()` target, i.e. string length). Confirmed the
  intended narrower gate, `resolveKeyValueHelperAliasName` (the
  overload actually linked into this translation unit, in
  `IrLowererSetupTypeCollectionHelpers.cpp`), is a permanent stub that
  always returns `false` - so the only thing actually gating this
  check today is the unscoped leaf-name match. Attempted a narrow fix
  (skip the "struct parameter type mismatch" verdict when
  `getBuiltinArrayAccessName` recognizes the candidate as a genuine
  vector access) but verified via debug print that
  `getBuiltinArrayAccessName` itself returns `false` for a bare
  user-defined `/vector/at` path (it only recognizes canonical stdlib-
  registered spellings, by design, per its own "explicitly excluded"
  canonical-path branch found during TODO-4803's investigation) - so
  that exclusion never fires for this repro and doesn't fix it.
  Reverted the attempt (verified clean via `git diff`). A correct fix
  needs to positively determine the receiver `at()` call's actual
  return type (struct vs plain scalar) rather than pattern-matching on
  path shape, and `Definition` has no direct return-type field - the
  return type lives in `Definition::transforms` (e.g. `return<string>`)
  and would need whatever helper this codebase already uses elsewhere
  to extract a definition's declared return type from its transform
  list (not identified this session) before this heuristic can be
  made type-aware. Left open, not fixed.

