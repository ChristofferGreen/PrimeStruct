# PrimeStruct TODO Log

## Purpose

This file is the live open-work queue for PrimeStruct.

- Keep only open work here: `[ ]` queued or `[~]` in progress.
- Move completed work to `docs/todo_finished.md`.
- Do not keep completed-task summaries, historical rollout notes, or closed
  coverage snapshots in this file.
- When this file has no task blocks, the tracked TODO queue is empty.

## Operating Rules

1. Use one task block per item with a stable `TODO-XXXX` ID.
2. Every active leaf must be implementable by someone arriving with no session
   context, including an AI agent.
3. Every active leaf must include `owner`, `created_at`, `scope`,
   `acceptance`, and `stop_rule`.
4. Prefer small, testable leaves over broad epics; split work before starting
   when acceptance cannot be verified in one bounded change.
5. Every active leaf must target at least one value outcome:
   - user-visible behavior change
   - measurable perf/memory improvement
   - deletion of a real compatibility subsystem
6. Avoid standalone micro-cleanups unless bundled into a value outcome.
7. If a leaf misses its value target after two attempts, archive it as
   low-value and replace it with a different hotspot.
8. Keep `Ready Now`, `Immediate Next 10`, `Priority Lanes`, `Execution Queue`,
   and task blocks synchronized when adding, splitting, completing, or deleting
   a task.
9. Keep `Ready Now` capped at eight active leaf tasks.
10. Keep active work leaf-shaped: queue sections must not contain umbrella,
    tracker, phase, research-shaped, or "continue with another slice" items.
11. For parallel work, each `Ready Now` item must name a `parallel_track` and a
    primary surface. Do not put two same-track successors in `Ready Now` unless
    their task blocks prove they touch different source/test surfaces.
12. Treat disabled tests as debt: each retained `doctest::skip(true)` cluster
    must map to an active TODO leaf with a re-enable-or-delete outcome, or be
    removed once proven stale.
13. Treat failing release-test cases as the top priority queue item: before
    starting new implementation work, update `docs/failing_tests.md`, fix the
    oldest reproducible failure first, and keep `docs/todo.md` aligned with the
    active test-fix work.
14. Every release test run must record any failing cases in
    `docs/failing_tests.md` before broader work continues.
15. When completing a task, mark it `[x]`, add `finished_at` plus a short
    evidence note, move the full block to `docs/todo_finished.md`, and remove
    it from this file.
16. This file is read fresh by an LLM agent each session, not browsed by a
    human - optimize for grep-ability over prose. Keep each task block to
    its current state (scope/acceptance/stop_rule), not a narrative history.
    Dated investigation/progress notes go in `docs/todo_log.md` instead,
    under that task's own `## TODO-XXXX` heading - when a task closes, fold
    whatever's still relevant into its `docs/todo_finished.md` resolution
    note and delete its `docs/todo_log.md` section.
17. Every active leaf must set `status` to exactly one of `ready` (its own
    repro/acceptance gap is confirmed and nothing external blocks starting),
    `blocked` (a specific, currently-`[ ]` `TODO-XXXX` must close first -
    name it in `blocked_on`; if that TODO is actually closed, the leaf is
    not blocked - fix the status instead of leaving it stale), or `deferred`
    (deprioritized, needs further scoping, or of confirmed-low value, with
    no single external blocker). Only `ready` leaves belong in `Ready Now`.

## Task Template

```md
- [ ] TODO-<id>: Short title
  - owner: ai|human
  - status: ready|blocked|deferred
  - blocked_on: TODO-XXXX (required when status: blocked; omit otherwise)
  - created_at: YYYY-MM-DD
  - phase: Group/Phase name (optional)
  - parallel_track: short-track-name (required when listed in Ready Now)
  - depends_on: TODO-XXXX, TODO-YYYY (optional)
  - scope: ...
  - implementation_notes: optional, but required when source/test entry points are not obvious
  - acceptance:
    - ...
    - ...
  - stop_rule: ...
  - notes: optional
```

Dated investigation history for this task goes in `docs/todo_log.md` under
a matching `## TODO-<id>` heading, not inline here.

## Open Tasks

### Queue Summary

Generated from each task block's own `status`/`parallel_track` fields -
re-derive after editing any block rather than hand-editing this table out
of sync with them.

| ID | Title | Status | Track |
| --- | --- | --- | --- |
| TODO-5320 | ast-semantic `.to_aos()` spelling vs resolved `/to_aos` shadow | ready | ast-semantic-to-aos-spelling |
| TODO-5309 | Rename the soa `ref_ref` builtin to `ref_borrowed` | ready | soa-accessor-naming |
| TODO-4737 | Add a lowered-module invariant for method-call targets | ready | lowered-module-invariant |

### Ready Now

- TODO-5320 (track: ast-semantic-to-aos-spelling, surface: ast-semantic `.to_aos()` rewrite in `src/semantics/` plus the two pinned dump cases in `tests/unit/compile_run/text_filters/test_compile_run_text_filters_dumps.cpp`): make the dump spell the call the way the semantic product resolves it.
- TODO-5309 (track: soa-accessor-naming, surface: `stdlib/std/collections/soa.prime` `ref_ref` plus its call sites in `tests/unit/` and `docs/PrimeStruct.md`): rename `ref_ref` to `ref_borrowed`.
- TODO-4737 (track: lowered-module-invariant, surface: `src/ir_lowerer/IrLowererSetupTypeMethodCallResolution.cpp` and the shared `isBuiltinClassifiedMethodCallTarget` helper in `IrLowererHelpers.{h,cpp}`): share one builtin-classification predicate across all method-call-target sites, then add the lowered-module invariant pass.

### Immediate Next 10

1. TODO-5320 - smallest and fully scoped; removes a latent wrong-target risk between the AST and the semantic product.
2. TODO-5309 - public stdlib rename with wide but mechanical test churn; do it while no other soa work is in flight.
3. TODO-4737 - largest; needs a before/after diff of the full `ir.pipeline.validation` suite, so take it last.

### Priority Lanes

- Semantics/dump fidelity: TODO-5320
- Stdlib naming: TODO-5309
- Lowering correctness tooling: TODO-4737

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10; all three are on disjoint tracks and surfaces, so they may also run in parallel.

### Task Blocks




- [ ] TODO-4737: Add a lowered-module invariant - no published method-call target without a materialized definition or builtin classification
  - owner: ai
  - status: ready
  - created_at: 2026-07-20
  - phase: Test infrastructure
  - parallel_track: lowered-module-invariant
  - scope: the TODO-4731 gap (c) class (semantic product publishes a
    method-call target the lowerer has no definition for) is invisible
    to semantic-product goldens and only surfaced case-by-case. A
    single validator pass over every lowered module asserting the
    invariant catches the whole class everywhere, replacing dozens of
    per-shape "does this lower" cases and guarding future
    materialization gaps by construction. The "materialized definition
    or builtin classification" check already exists but is scattered
    across 6 call sites in two files
    (`IrLowererInlineNativeCallDispatch.cpp:1605,1916` and
    `IrLowererSetupTypeMethodCallResolution.cpp:650,709,753,759`), each
    independently reimplementing a similar-but-not-identical exemption
    whitelist via hardcoded string-literal comparisons - see
    implementation_notes for the full exemption inventory and why a
    general invariant can't just be a static path allowlist.
  - implementation_notes: a genuinely correct, general invariant pass
    needs either (a) fully re-deriving and generalizing all 6 call
    sites' accumulated special-casing (real risk of shipping false
    positives, or missing a nuance and shipping a pass that doesn't
    actually catch anything), or (b) having each of the 6 call sites
    call OUT to one new shared `isBuiltinClassifiedMethodCallTarget(target,
    semanticProgram, callExpr)` helper instead of their own inline
    exemption list, then having the new invariant pass call that SAME
    shared helper - the safer design, provably consistent with existing
    lowering behavior by construction. Half of (b) already shipped (see
    log): `IrLowererInlineNativeCallDispatch.cpp`'s two call sites had a
    byte-identical 5-clause exemption predicate (`/string/count`,
    `/std/collections/vector/count`, `/vector/capacity`, `/soa/count`,
    `/vector/at`+`/at_unsafe`, `/soa/to_aos`), extracted into
    `isBuiltinClassifiedMethodCallTarget(target, callExpr)` in
    `IrLowererHelpers.h`/`.cpp`. `IrLowererSetupTypeMethodCallResolution.cpp`'s
    other 4 call sites remain unextracted: their exemptions
    (`routesExplicitVectorCountMethodThroughArgsPackCount`,
    `directTargetKeepsSyntheticCollectionFallback`,
    `allowsReceiverResolvedVectorMetadataFallback`) are woven into a
    local `resolveLoweredDefinitionPath` lambda doing receiver-type-
    dependent fuzzy path matching, not extractable into a target-string-
    only predicate without either unsafely re-deriving that logic or a
    dedicated decomposition pass over that function first (TODO-4724
    decomposed a *different* function - `SemanticsValidator::resolveMethodTarget`
    in `SemanticsValidatorExprMethodTargetResolution.cpp` - and closed
    without touching this one; it provides no seam here, so this is not
    `blocked_on` anything, just unstarted scoped work). Once a seam
    exists and both remaining files call the shared helper, the new
    invariant pass itself is comparatively small: iterate
    `semanticProgram->methodCallTargets`/`directCallTargets`, for each
    published target call `resolveLoweredDefinitionPath` and the shared
    helper, and error if neither the definition nor the
    builtin-classification check succeeds.
  - acceptance: invariant runs in the lowering pipeline under a test
    flag; deliberately re-introducing the gap (c) bug trips it.
  - stop_rule: do not re-derive `IrLowererSetupTypeMethodCallResolution.cpp`'s
    4 remaining fuzzy-path exemptions from scratch without a real
    before/after diff of the full `ir.pipeline.validation` suite (as the
    2026-07-23c log entry below did for the first 2 sites) - a subtly
    wrong re-derivation would silently narrow or widen which method-call
    targets require a materialized definition, exactly the class of bug
    this task exists to catch.

- [ ] TODO-5320: Make ast-semantic `.to_aos()` spelling match the resolved root `/to_aos` shadow
  - owner: ai
  - status: ready
  - created_at: 2026-09-25
  - phase: Hidden test failure remediation
  - parallel_track: ast-semantic-to-aos-spelling
  - depends_on: (none)
  - scope: split from TODO-4812 finding (2). Behaviour is correct: a root
    `/to_aos([soa<Particle>] values)` (or `[SoaVector<Particle>]`) user
    definition returning 7 is what `values.to_aos()` runs on vm and native
    (exit 7), including the helper-return receiver
    `holder.cloneValues().to_aos()` case. But for a `soa<Particle>`-typed
    local or a helper-return receiver, the ast-semantic dump spells the
    call as `/std/collections/soa/to_aos__t<hash>(values)` while the
    semantic product's `direct_call_targets` entry for it has
    `call_name=/std/collections/soa/to_aos__t<hash>`
    `resolved_path=/to_aos`. An explicit `[SoaVector<Particle>]` local
    dumps `/to_aos(values)` correctly. An AST call name that disagrees with
    the semantic-product target is the same shape as TODO-4756's root
    cause (a lowerer fast path trusting one over the other), so it is a
    latent wrong-target risk plus misleading dump output. A `/soa/to_aos`
    shadow is honoured and dumped correctly for method sugar, like its
    `/soa/count|get|ref|push|reserve` siblings (bare `to_aos(values)`
    does not route to `/soa/to_aos`, like bare get/ref/push/reserve).
  - implementation_notes: the pinned sites are "dump ast-semantic
    rewrites nested struct body soa method shadows" and "dump ast-semantic
    keeps helper-return experimental soa to_aos with same-path helper" in
    `test_compile_run_text_filters_dumps.cpp`; their `TODO-4756 (extends)`
    comments say the shadow is not honoured, which is wrong about
    behaviour - fix those comments when re-pinning.
  - acceptance:
    - the ast-semantic dump shows `/to_aos(values)` (not
      `/std/collections/soa/to_aos__...`) whenever the semantic product
      resolves the call to `/to_aos`, for `soa<T>` locals and helper-return
      receivers.
    - both pinned cases re-pinned with corrected comments;
      `./scripts/compile.sh --release` back at baseline.
  - stop_rule: if the AST rewrite cannot see the shadow without moving
    shadow resolution earlier in `semanticValidationPassManifest()`, stop
    and document the pass-order constraint instead of reordering passes.

- [ ] TODO-5309: Rename the soa `ref_ref` builtin to `ref_borrowed`
  - owner: ai
  - status: ready
  - created_at: 2026-09-24
  - phase: Naming/API clarity
  - parallel_track: soa-accessor-naming
  - depends_on: (none)
  - scope: `/std/collections/soa/ref_ref<T>([Reference<SoaVector<T>>] values,
    [i32] index)` (`stdlib/std/collections/soa.prime:230-233`) is the one
    member of the soa accessor family (`count`/`count_ref`/`get`/`get_ref`/
    `ref`/`ref_ref`) whose name doubles a suffix instead of composing two
    distinct axes: which value it returns (`get` = value, `ref` =
    `Reference<T>`) and whether the receiver is borrowed (bare name = by
    value `SoaVector<T>`, `_ref` suffix = `Reference<SoaVector<T>>`).
    `ref_ref` collapses "returns a reference" and "receiver is borrowed"
    into one doubled token, which reads like a typo and was genuinely
    confusing enough to prompt a user question outside any specific bug
    investigation. Rename to `ref_borrowed` (or another name that keeps
    `ref`'s existing "returns a reference" meaning and makes "receiver is
    borrowed" explicit rather than doubling the suffix - confirm exact
    spelling before implementing, this scope intentionally doesn't lock it
    in). TODO-5295/5307/5308 (closed 2026-09-23/24)
    finished the fixes in this accessor's same-path-shadow resolution, so the
    code has settled.
  - implementation_notes: this is a public stdlib rename, not a local
    refactor - `/std/collections/soa/ref_ref` is `[public]` and callable
    by name from user `.prime` code, and its rooted spelling
    (`/std/collections/soa/ref_ref`) plus the same-path-shadow spelling
    (`/soa/ref_ref`) both appear throughout `tests/unit/` (several dozen
    sites, many added/touched by TODO-5295/5307/5308's fixes literally
    today). A safe migration needs: (1) add the new name as the real
    implementation, (2) decide whether the old name stays as a
    deprecated/compatibility alias or is deleted outright (check this
    repo's usual policy for renaming public stdlib symbols - search
    `docs/PrimeStruct.md`/`docs/CompatPathResolutionConsolidation.md` for
    precedent), (3) update every call site across `stdlib/`, `tests/`, and
    any docs that reference `ref_ref` by name.
  - acceptance:
    - The soa accessor family's naming consistently encodes "returns a
      reference" and "receiver is borrowed" as two separable axes, not a
      doubled suffix.
    - Every test and stdlib call site is updated to the new name (or the
      old name is kept working as a documented compatibility alias, per
      whatever migration policy step (2) above settles on).
    - `docs/PrimeStruct.md` (or wherever this accessor family is
      documented) reflects the new name.
  - stop_rule: if a compatibility alias for the old name would be needed in
    more than the stdlib and `tests/unit/`, stop and record the migration
    policy decision here instead of widening the rename.

