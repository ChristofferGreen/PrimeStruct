# Receiver-Target Resolution Consolidation Plan

Status: Step 1a landed (name-set library only, unwired). Step 0
(characterize the full rule table) in progress - see "Step 0 Rule Table"
below; semantics-stage method-target resolvers, all five
snapshot-collection mechanisms, and monomorphization are now fully
branch-enumerated; the `ir_lowerer` stage's own
`resolveMethodCallDefinitionFromExpr` (Row G) is now enumerated too, but
its two sibling receiver-target-helper/collection-helper files remain
open. This is the sibling
problem `docs/CompatPathResolutionConsolidation.md` explicitly deferred as
a non-goal: "Method-call *receiver* inference (which type a method call
dispatches on) stays where it is; the classifier only decides spelling
disposition, not receiver typing." That document's own Risks section
predicted this: "If Step 0 uncovers a third layer of the same shape, that
is a signal to stop and reassess." This is that third layer.

## The Problem, Verified

Given a receiver expression's inferred type and a method/call name, which
definition it resolves to is implemented independently in at least three
places:

- **Semantics**: `resolveArgsPackElementMethodTarget`
  (`SemanticsValidatorMethodTargetArgsPackResolvers.cpp:152-217`) and its
  siblings (`SemanticsValidatorMethodTargetVectorResolvers.cpp`,
  `..KeyValueResolvers.cpp`, `..StringResolver.cpp`,
  `..StructSumResolvers.cpp`), called from `resolveMethodTarget`
  (`SemanticsValidatorExprMethodTargetResolution.cpp`).
- **Monomorphization**: `resolveMethodCallTemplateTarget`
  (`TemplateMonomorphMethodTargets.cpp`) and
  `TemplateMonomorphCollectionCompatibilityPaths.cpp`.
- **IR lowering**: `IrLowererSetupTypeMethodCallResolution.cpp` plus
  `IrLowererSetupTypeReceiverTargetHelpers.cpp` and
  `IrLowererSetupTypeCollectionHelpers.cpp` (confirmed to independently
  re-parse type text and re-derive vector/soa/key-value/Buffer/File family
  membership, including a `SoaVector__`/specialization-suffix case with no
  counterpart in the other two stages).

Unlike the compat-spelling problem, the low-level primitives are *not*
even shared here: each stage has its own `normalizeBindingTypeName`,
`splitTemplateTypeName`, `isKeyValueSurfaceTypeName`, and
`isInternalSoaCollectionTypeName`-equivalent, so the divergence risk is
higher, not lower, than the already-solved problem.

## Evidence This Is Load-Bearing, Not Theoretical

Two bugs found and narrowed (not fixed) in the same session that produced
this document:

- **TODO-4753**: `.remove_at(...)`/`.remove_swap()` method-call sugar is
  broken on both `vm` and `exe` backends. Traced to
  `resolveMethodCallTemplateTarget` reaching a plausible generic
  vector-family fallback that computes `/vector/remove_at`, while a
  separate, adjacent import-alias-priority mechanism
  (`stdlibSurfaceImportAliasPriority` in
  `TemplateMonomorphFinalOrchestration.cpp`, which deliberately ranks
  `ConstructorFamily` above `HelperFamily`) turned out to be load-bearing
  for unrelated map-heavy tests: a narrow attempted fix regressed 67 tests
  in `PrimeStruct_compile_run_tests` and was fully reverted.
- **TODO-4760**: an `args<map<i32, i32>>` receiver's `.at(...)` call
  resolves to `/std/collections/map/at` (key-lookup semantics) even though
  `--dump-stage semantic-product` confirms the receiver's inferred type is
  correctly `args<map<i32, i32>>` — the args-pack-element family should
  have won, not the bare stdlib map surface. Proven via direct evidence,
  not fixed (same regression-risk neighborhood as TODO-4753).

Both bugs are priority/precedence disagreements between independently
re-derived family classifications for the same receiver — exactly the
shape this document exists to consolidate.

## Goal

One authoritative decision function each stage calls instead of
re-deriving receiver-type family membership itself, mirroring
`classifyCollectionHelperSpelling`'s shape: shared *pure* logic
(fixed name sets, family ordering) plus stage-supplied callbacks for the
struct-metadata-backed lookups (`isKeyValueSurfaceTypeName`,
`isInternalSoaCollectionTypeName`-equivalents) that legitimately differ per
stage because each stage has its own struct/definition maps.

## Non-Goals

- No behavior changes anywhere yet. Step 1a below adds an unwired,
  independently-tested module; nothing calls it.
- Not touching `stdlibSurfaceImportAliasPriority` or
  `shouldSkipWildcardAlias` without first fully understanding what
  currently depends on the `ConstructorFamily` > `HelperFamily` ordering —
  this is the TODO-4753 lesson, and it applies to any future migration
  step here too.
- Compat-spelling disposition (already solved by
  `CollectionSpellingClassifier`) is out of scope; this document is
  strictly about receiver/element *type-family* classification feeding
  method-target resolution.

## Plan

### Step 0 — Characterize (not started)

Build the rule table the way `CompatPathResolutionConsolidation.md`'s
Step 0 did: for each of the three implementations, enumerate every branch,
its guard conditions (type shape, method name, import visibility, shadow
definitions), and which test pins it. TODO-4753 and TODO-4760 are the
first two rows. This is real, multi-session characterization work — not
attempted in this session beyond the semantics-side single-function audit
below.

### Step 1a — Shared name-set library (Complete, this session)

Auditing `resolveArgsPackElementMethodTarget` to build a byte-faithful
standalone classifier (originally the goal for this session's Step 1)
surfaced two behavioral quirks that must be resolved by Step 0's rule
table before any call site can safely delegate to a shared function:

- **Method-name gating inside a type-family branch.** The FileError branch
  only commits to the FileError family when `normalizedMethodName` is one
  of `{why, is_eof, status, result}`; for any other method name on a
  FileError-typed element, the function falls through to the
  struct-type-path fallback instead of, say, returning "no FileError
  method." Family classification there is not a pure function of type
  alone — `(type, methodName)` jointly decide.
- **Template-shape gating.** The vector/array/soa/Buffer/key-value/File
  checks only run when the element type text is template-shaped (`X<...>`
  per that stage's own `splitTemplateTypeName`, which requires a matching
  `>` at the exact end of the string). A bare non-template `"Buffer"` or
  `"File"` element type text skips all of those checks and falls straight
  to the struct-path fallback — meaning an args-pack element literally
  typed as `Buffer` (no generic parameter) never reaches Buffer-family
  dispatch in this function today. Whether that is intended or a latent
  bug is undetermined without the Step 0 rule table.

Given those findings, extracting a decision function this session would
have meant guessing at behavior instead of characterizing it — the same
mistake `CompatPathResolutionConsolidation.md`'s reverted first attempt
made. Landed instead, scoped to what is provably safe without a rule
table:

- `include/primec/support/ReceiverElementFamilyClassifier.h` /
  `src/support/ReceiverElementFamilyClassifier.cpp`: the *type-name-only*
  parts that are genuinely fixed, stage-independent data — the
  vector/array base-name set, the Buffer accessor method-name set, the
  File handle method-name set, and the primitive-name set — extracted
  verbatim from `resolveArgsPackElementMethodTarget`,
  `isFileMethodName`/`SemanticsValidatorMethodTargetResolutionDetail.cpp`,
  and `isPrimitiveBindingTypeName`/`SemanticsBindingTypeHelpers.cpp`, with
  unit tests pinning each set
  (`tests/unit/semantics/test_semantics_receiver_element_family_classifier.cpp`).
  `classifyReceiverElementFamily` composes them into the same family
  ordering (string → FileError → vector/array → soa → Buffer → key-value →
  File → primitive → struct fallback) as a *documented approximation*,
  explicitly not wired into any call site — its header states the two
  quirks above as open scope gaps.
- The struct-metadata-backed families (Soa, KeyValue) take stage-supplied
  predicates, the same pattern `CollectionSpellingClassifier` uses for
  `CollectionDefinitionExistsFn`.

### Step 1b — Full classifier plus differential-audit harness (not started)

Once Step 0's rule table exists, extend the Step 1a module (or replace it)
to take the same `(type, methodName, templateShape)` joint inputs each
stage already computes, wire it in behind a
`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1`-style env-gated comparison at
one call site at a time (semantics first — it is the reference behavior,
per the compat-spelling precedent's own governing principle), and drive it
to zero divergence across the full 3-suite battery before any call site
actually delegates.

### Step 2 — Migrate stage by stage (not started)

Same order and discipline as the compat-spelling consolidation: semantics
first (reference behavior), then monomorphization, then `ir_lowerer`, each
gated on zero-divergence across `PrimeStruct_semantics_tests`,
`PrimeStruct_backend_ir_tests`, and `PrimeStruct_compile_run_tests`.

## Step 0 Progress: TODO-4760 Traced Further (2026-09-04)

Continued tracing TODO-4760 (`args<map<i32, i32>>` receiver's `.at(...)`
misrouting to bare `/std/collections/map/at`) to find the exact defect,
without landing a fix - the same discipline as Step 1a.

**A real, separate bug found and ruled out as this one's cause.**
Monomorphization's `unwrapCollectionReceiverEnvelope`
(`TemplateMonomorphCollectionCompatibilityPaths.cpp:259-316`) unwraps
`Reference<T>`/`Pointer<T>` envelopes down to `T`'s own family when `T` is
a recognized collection receiver type, but has no equivalent case for
`args<T>` - `isCollectionReceiverTypeName` only recognizes
`array`/`vector`/`soa`/`map`/`string`, so an `args<map<i32, i32>>`
binding's envelope unwraps to the literal, unrecognized base name
`"args"` instead of recursing into its element type. This silently
defeats every `typeName == "map"`-gated branch in
`resolveMethodCallTemplateTarget` for any args-pack-of-collection
receiver. Confirmed by direct code reading (mirrors the already-handled
Reference/Pointer case exactly) and reproducible in isolation, but
**a fix for it does not change TODO-4760's repro's compile output or
`--dump-stage semantic-product` resolution** - tried and reverted in full
(`git checkout --`). Root cause: `direct_call_targets[65]`'s wrong
`resolved_path` is recorded by the **semantics stage**, not
monomorphization; `unwrapCollectionReceiverEnvelope` is monomorphization-
only code that never runs before that fact is captured. This is real,
separate technical debt (its own future rule-table row - args-pack
receiver family is unrecognized by this one function), not the TODO-4760
fix, and should not be attempted again without first finding the
semantics-stage call site that actually produces `direct_call_targets[65]`
for a bare/direct-call-form `at(values, 0i32)` (not a method-call form) -
`setIndexedArgsPackKeyValueMethodTarget`
(`SemanticsValidatorMethodTargetKeyValueResolvers.cpp:508-557`) was
checked and ruled out: it only fires when the *receiver expression itself*
is a nested indexed-access call (`pack[i].method()`), not when the pack
is passed directly as the call's own first argument, which is this
repro's shape.

**Found the actual dispatcher (2026-09-04).** The repro's
`[map<i32, i32>] head{at(values, 0i32)}` is a *binding-initializer* call
shape (the third call-shape dimension from
`CompatPathResolutionConsolidation.md`'s own rule table, alongside direct
and method calls), handled by
`SemanticsValidatorBuildInitializerInference.cpp`'s local-binding-type
inference cascade. That cascade has explicit early special-cases for
several initializer shapes (`take`/`borrow`, field access, sum
constructors, task spawn/wait, graph-local auto-binding) but has *no*
args-pack-positional-indexing case at all - confirmed by grepping the
whole file for `resolveArgsPackAccessTarget`/
`resolveIndexedArgsPackElementType`/`getBuiltinArrayAccessName` (zero
hits). So `at(values, 0i32)` falls straight through to the generic
resolver (`preferredCollectionHelperResolvedPath` → `resolveCalleePath` →
`resolveExprConcreteCallPath`), which matches `/std/collections/map/at`
as an ordinary 2-arg `at` definition - there is no args-pack-aware
candidate anywhere in this cascade to compete with it. This is a fourth,
independent gap in the same "each call shape has to remember to check
args-pack-ness first" pattern `resolveMethodTarget`'s own args-pack
branches already handle for method calls - direct evidence that this
isn't one localized bug but the general shape this document exists to
consolidate. See `docs/todo.md` TODO-4760's `investigated_2026-09-04`
note for the precise fix sketch.

**That fix sketch was tried and was wrong (2026-09-04, continued).**
`inferBindingTypeFromInitializer` never runs for `head` because it has an
*explicit* declared type (`auto`-only inference function) - reverted in
full. The real decision is a two-function pair in the snapshot-collection
machinery: a naive first pass (`collectDirectCallExpr`,
`SemanticsValidatorSnapshots.cpp:1578-1636`, no per-call local context)
and a "local-aware" second pass (`inferCallSnapshotData`,
`SemanticsValidatorSnapshotLocals.cpp:91-160`) that only overwrites the
first pass's answer when its own answer is non-empty - both share the
identical `preferredCollectionHelperResolvedPath` (confirmed
receiver-blind: it calls `classifyCollectionHelperSpelling` with
`CollectionReceiverFamily::None` hardcoded, by design, per this
consolidation's own sibling document's scope) → `resolveCalleePath`
(a plain import-alias name lookup, the same `stdlibSurfaceImportAliasPriority`
machinery TODO-4753 found load-bearing) fallback, so both compute the
same wrong answer and the "overwrite" is a no-op. A correct fix needs
both functions to agree, and the naive pass lacks the per-call local
context to run the same check the second pass can - see the todo.md note
for the full detail and next steps. Still not landed; this is now a
precisely localized two-function fix, not an open-ended search.

**That two-function fix was implemented, worked exactly as designed, and
still didn't fix the repro (2026-09-04, continued further).** Both
`direct_call_targets` and `query_facts` correctly stopped reporting the
wrong resolution after the fix. The repro still failed identically,
because `--dump-stage semantic-product` then showed a third mechanism
(`bridge_path_choices`, populated by the same naive first pass from its
own untouched local resolution) and a fourth
(`collection_specializations`, not yet traced) independently computing
the same wrong answer. Reverted both changes in full rather than keep
chasing mechanisms one at a time - the naive first pass alone feeds at
least two of these lists from one shared, still-broken local variable.

This is no longer a hypothesis - it is direct, empirical proof of this
document's central thesis. Within the *semantics stage alone*, before
even reaching monomorphization or `ir_lowerer`, this exact receiver-type
gap is duplicated across at least four independent collection mechanisms
(`direct_call_targets` × 2 producers, `query_facts`, `bridge_path_choices`,
`collection_specializations`). Patching them as discovered is the same
whack-a-mole pattern that already cost 67 tests once this session
(TODO-4753) - Step 0 (enumerate every mechanism up front, the way the
sibling document's Step 0 did) is now a demonstrated prerequisite for
fixing this bug safely, not an optional nicety. See TODO-4760's
`investigated_2026-09-04` note (continued further) in `docs/todo.md` for
full detail.

**Found the true root gate (2026-09-04, final).** It has nothing to do
with any of the four semantics-stage mechanisms above.
`ir_lowerer`'s `getBuiltinArrayAccessName`
(`IrLowererBuiltinNameHelpers.cpp:485-608` - the function deciding
whether a call is builtin positional array/pack-index access at all) has
an early-out, `resolvesKeyValueHelperSurfacePath(scopedName)`, where
`scopedName` is built purely from the call's own literal
`name`/`namespacePrefix` - never from any resolved-call fact, which is
why none of the semantics-stage fixes could reach it. For a bare,
unrooted call name (`"at"`, no `/`), the underlying
`resolveStdlibSurfaceMemberName` skips its path-validation gate entirely
and falls back to pure string-equality against the stdlib surface's
member-name list. Since `"at"` is a real map helper name, this excludes
*any* bare call literally named `at` from array-access treatment,
**regardless of receiver type** - not receiver-blind in the sense of
"picks the wrong resolution," but receiver-*absent*: the gate never
looks at the receiver at all, it is a pure name collision.

`getBuiltinArrayAccessName` is called from **70+ sites across ~30 files**
in `ir_lowerer` - the highest-fan-out function found in this whole
investigation. The exclusion is almost certainly deliberately protecting
the common case (a real map receiver's own `.at()`/`.count()`) from
being mistreated as array-access; making it receiver-type-aware without
an unbounded regression audit across all 70+ sites is its own dedicated,
carefully-scoped project - not something to attempt inside this
investigation. TODO-4760(a) is now fully root-caused but intentionally
left unfixed; see its own `investigated_2026-09-04` (final) note in
`docs/todo.md`.

This closes the immediate investigation loop with an important
correction to this document's own framing: the earlier sections describe
receiver-type resolution as duplicated-and-disagreeing across stages
(semantics/monomorphization/`ir_lowerer` each computing their own
answer). This final finding is a different failure mode layered on top -
a gate that doesn't compute a receiver-dependent answer at all, it
matches on the call's bare spelling before receiver information is even
available. Both are real and both block this bug; a complete Step 0
rule table needs to capture this "name-collision, receiver-blind by
construction" class as its own row category, not fold it into the
"stages disagree on receiver type" framing the rest of this document
uses.

**Attempted the narrow fix anyway; it regressed 46 tests (2026-09-04,
one more round).** The semantics-stage sibling of this exact function
(`SemanticsBuiltinPathHelpers.cpp:1186`'s `getBuiltinArrayAccessName`)
turned out to already guard against bare unrooted names in its own
key-value lookup helper - `resolveKeyValueHelperMemberNameLocal` requires
a `/` before attempting a surface-member match at all. Adding the
identical guard to ir_lowerer's copy looked like a precise, minimal,
well-precedented fix (one function, matching an established sibling
convention). It changed behavior correctly on a simplified repro, but
`PrimeStruct_backend_ir_tests` came back with 46 failures against a
baseline of 1 known flake. Reverted in full. This is this session's
third instance of "narrow, well-reasoned fix in this exact neighborhood
regresses broadly when actually run against the full suite" (after
TODO-4753's 67-test regression and the two-function TODO-4760 attempt
above) - strong, repeated, empirical confirmation that nothing in this
area is safe to touch without the full characterization Step 0 calls
for, no matter how principled the reasoning behind a specific change
looks in isolation.

Separately confirmed the fix (even before reverting) did not help the
actual pinned regression test at all - its multi-function/`[spread]`
scenario fails earlier and differently than the simplified repro used
throughout this investigation. See TODO-4760's own notes in `docs/todo.md`
for full detail.

## Update (2026-09-05/06): the "46-test regression" was a false alarm, and two real fixes landed

The `resolvesKeyValueHelperSurfacePath` fix described above was re-verified
properly and found to be entirely safe: the "46 failures" were a
**pre-existing baseline state** that had never actually been confirmed for
`PrimeStruct_backend_ir_tests` earlier in this session (every prior
verification round on this suite had implicitly compared against an
assumed-clean baseline from a much earlier session, never re-checked). A
fresh `git stash` + rebuild + rerun of the unmodified baseline reproduced
the exact same 46 failures, byte-for-byte identical by name. The fix was
restored and landed (commit `5468344`), verified via proper name-level
diffs against freshly-confirmed baselines across all three suites:
`PrimeStruct_backend_ir_tests` identical, `PrimeStruct_semantics_tests`
identical (1 known flake), `PrimeStruct_compile_run_tests` improved by
one with zero new failures.

A **second** real bug was then found and fixed (commit `a3fa55d`):
`rewritePublishedKeyValueConstructorExpr`
(`IrLowererInlinePackedArgs.cpp`) rewrote every map-constructor call
inside an args-pack element to a bare, arity-blind canonical family
name, with the original call's `semanticNodeId` zeroed - downstream
resolution then fell back to a plain exact-key `defMap` lookup with no
overload disambiguation, so every pack element resolved to whichever
one concrete overload happened to be registered at that shared bare
key. Sibling pack elements needing the same arity (e.g. two 2-pair maps)
worked by coincidence; different arities (one 2-pair, one 4-pair) broke
with "argument count mismatch for /std/collections/map/map". Fixed by
using the already-correctly-resolved `callee->fullPath` (resolved via
the original call's still-intact `semanticNodeId`, before it gets
zeroed) instead of the bare canonical name.

With both fixes landed, the pinned regression test's own "argument
count mismatch" failure is gone entirely - its assertion was updated to
match. **A third, distinct issue remains**, found and precisely located
but not fixed: `IrLowererLowerStatementsExpr.h`'s expression-emission
cascade has several checks that route `at`/`at_unsafe` 2-arg calls to
the working `emitArrayVectorIndexedAccess` helper, but every one of
them gates on the receiver being a **vector** specifically - none
recognize an args-pack-of-struct-shaped-elements (a map, here) as a
case needing the same treatment. This looks like a missing codegen
capability (positionally indexing a struct-shaped pack element as an
*expression value*, not a statement) rather than a resolution-priority
bug - the kind of thing this document's classifier proposal doesn't
address, since there is no existing per-stage answer to reconcile, just
an absent one. See TODO-4760's own notes in `docs/todo.md` for the full
trace and a concrete narrowing suggestion for whoever picks this back up.

## Update (2026-09-06): the third issue is fixed - TODO-4760 fully resolved

The "missing codegen capability" theory was correct in spirit but not in
scale: the capability already existed (a working, pre-existing path
through `emitArrayVectorIndexedAccess` for struct-shaped args-pack
elements, proven by the map constructor's own internal
`args<Entry<K,V>>` access using it correctly), it just wasn't reachable
for a genuine `args<map<K,V>>` element and, once reached, mishandled one
storage-layout case.

Two coordinated fixes:

1. **Dispatch routing** (`IrLowererLowerStatementsExpr.h`): the cascade's
   `isKeyValueAccessTarget` gate used `resolveCollectionPairTypeInfo`,
   whose `isKeyValueTarget` can't distinguish a genuine
   `args<map<K,V>>` pack element from the map constructor's own internal
   `args<Entry<K,V>>` pack - both are key-value-shaped args-pack
   elements. An earlier attempt to fix this with a crude "any args-pack
   receiver" guard corrupted the `Entry<K,V>` path (confirmed via a
   bounded-recursion-guard trace: genuine runaway recursion, not a false
   alarm). The correct, precise discriminator turned out to be
   `structTypeName` emptiness: a `map<K,V>`-element's `LocalInfo` has an
   EMPTY `structTypeName`, while `Entry<K,V>`'s has a populated
   `Entry__t...` path. Added `isKeyValueAccessReceiverArgsPackOfMap`,
   true only for the empty-`structTypeName` case, and excluded exactly
   that case from the deferral.
2. **Load-vs-copy contract** (`IrLowererIndexedAccessEmit.cpp`): once
   routing was fixed, `count()` on the retrieved map returned a garbage
   value. `isInlineMapArgsPackTarget` used `elemSlotCount > 0` to decide
   "struct-copy from address, don't load" - but a key-value pack element
   with `elemSlotCount == 1` is stored as a single heap pointer (the same
   convention used elsewhere for `map<K,V>` bindings), which needs a
   plain load, not an address left for a copy. Changed the threshold to
   `elemSlotCount > 1`.

Verified via three hand-built repros (single-map `count()`: exit 2;
two-map summed `count()`: exit 3; the full pinned-test source with
`[spread]`/`forward`/`forward_mixed` across 4 functions: exit 11,
independently confirmed correct via manual arithmetic on the call
graph) and the full 3-suite battery with a fresh name-level diff:
`backend_ir` and `semantics` unchanged from their established
baselines; `compile_run` dropped from 164 to 5 failures with **zero new
failures and 159 net fixes** - this bug was blocking far more of the
map-conformance harness than just the one pinned test. TODO-4760 is now
fully resolved and moved to `docs/todo_finished.md`.

This consolidation document's own scope (a shared receiver-classifier
library, `ReceiverElementFamilyClassifier`) remains unimplemented at the
call-site level - the classifier exists and is unit-tested but
deliberately unwired, per its own header caveats. TODO-4760's resolution
did not go through that classifier; it used a narrower, one-off
discriminator scoped to this exact pair of cases. Whether that
discriminator generalizes into the shared classifier, or stays a local
special case, is unresolved and left for whoever next works this
consolidation's Step 0.

## Step 0 Rule Table (progress, 2026-09-08)

Following `CompatPathResolutionConsolidation.md`'s Step 0 method: for each
implementation, enumerate every branch, its guard conditions, and which
test (if any) pins it. Per this document's own stated order, semantics is
covered first as reference behavior. This round covers the method-target
resolver family in full and the four snapshot-collection mechanisms and
the ir_lowerer name-collision gate at a pointer/cross-reference level (not
yet full branch enumeration for those). Row IDs are local to this table
(prefixed `R`, distinct from the compat-spelling table's numbering).

### Row category A: `resolveArgsPackElementMethodTarget` and siblings (method-call-on-args-pack-element dispatch)

`resolveArgsPackElementMethodTarget`
(`SemanticsValidatorMethodTargetArgsPackResolvers.cpp:152-217`) is the
entry point once an args-pack element's type text is known. Branch order
matters - it is a strict if/else-return cascade, first match wins:

| # | guard condition | disposition | pinned by |
|---|---|---|---|
| R1 | `collectionElemType == "string"` (after `Reference`/`Pointer` unwrap) OR bare base type text is literally `"string"` | dispatch to `/string/<method>` unconditionally, no method-name check | args-pack corpus (string element method calls) |
| R2 | `collectionElemType == "FileError"` AND `normalizedMethodName ∈ {why, is_eof, status, result}` | dispatch to `preferredFileErrorHelperTarget` | args-pack corpus (FileError element `.why()` etc.) |
| R2b | `collectionElemType == "FileError"` AND method name NOT in that set | **falls through** past this branch entirely to the struct-type-path fallback (R7) - not rejected here, not treated as FileError | UNPINNED - documented as an open quirk in Step 1a ("method-name gating inside a type-family branch"); no test found asserting this fallthrough is intended vs. accidental |
| R3 | element type text is template-shaped (`splitTemplateTypeName` succeeds) AND base ∈ `{vector, array}` or `isInternalSoaCollectionTypeName(base)` | dispatch to `/<base>/<method>` unconditionally, no method-name check | args-pack corpus (vector/array/soa element method calls) |
| R4 | template-shaped AND base == `"Buffer"` AND method ∈ `{count, empty, is_valid, readback, load, store}` | dispatch to `preferredBufferMethodTarget` | args-pack corpus (Buffer element helper calls) |
| R4b | template-shaped AND base == `"Buffer"` AND method NOT in that set | falls through to R7 (struct-path fallback) | UNPINNED |
| R5 | template-shaped AND `isKeyValueSurfaceTypeName(base)` | delegates to `setPreferredKeyValueMethodTarget` (Row category C below) - method name NOT re-checked here | args-pack corpus (map element method calls) |
| R6 | template-shaped AND base == `"File"` AND `isFileMethodName(method)` | dispatch to `preferredFileHelperTarget` | args-pack corpus (File element method calls) |
| R6b | element type text is **not** template-shaped (bare, no `<...>`), even if the bare text is literally `"Buffer"` or `"File"` | R3-R6 never evaluated at all - falls straight to R7/R8 | UNPINNED - documented in Step 1a as "template-shape gating"; a bare (non-generic) `Buffer`/`File`-typed args-pack element never reaches Buffer/File dispatch in this function. Undetermined whether any such element type is reachable in practice (bare `Buffer`/`File` without a template arg may not be a real user-facing type shape) - flagged, not resolved |
| R7 | none of the above; `isPrimitiveBindingTypeName(normalizedElemBaseType)` | dispatch to `/<baseType>/<method>` unconditionally | args-pack corpus (primitive element method calls, e.g. `args<i32>` `.method()`) |
| R8 | none of the above; struct-type-path resolution (`resolveMethodTargetStructTypePath` then `resolveTypePath`) succeeds | dispatch to `<resolvedType>/<method>` | args-pack corpus (struct/sum element method calls) - this is also FileError's/Buffer's/File's de facto fallback per R2b/R4b/R6b |
| R9 | none of the above | return `false` (unresolved - caller emits "unknown method") | — |

Note: `isBuiltinOut` is set `true` only for R2 when the resolved path is
exactly `/file_error/why`, and for R6 always when the resolved path starts
with `/file/`; every other branch (R1, R3, R4, R7, R8) leaves it at its
caller-supplied default. This asymmetry (some branches mark
builtin-ness, most don't) is itself unpinned - no test found asserting the
semantics of `isBuiltinOut` per branch.

### Row category B: vector-family resolvers (`SemanticsValidatorMethodTargetVectorResolvers.cpp`)

- **`classifyExplicitVectorHelperReceiver`** (line 80): tries, **in fixed
  order**, `resolveCollectionVectorValueTarget` (legacy experimental
  family) → `resolveVectorTarget` → `resolveSoaVectorTarget` →
  `resolveArrayTarget` → `resolveStringTarget` → `resolveKeyValueTarget`,
  returning the family name of the **first** that matches. This ordering
  is itself a receiver-family-priority table with no shared counterpart in
  monomorphization or `ir_lowerer` - a receiver expression that could
  satisfy more than one of these (unclear if any real type shape does) is
  resolved by this order alone. UNPINNED as an explicit priority
  contract; no test found asserting the order itself, only its
  consequences per concrete receiver type.
- **`resolveBorrowedVectorReceiver`** (line 224): recognizes a vector
  receiver through up to 3 layers of indirection, each its own guard:
  (a) direct `vector<T>`-typed binding; (b) `Reference<vector<T>>` /
  `Pointer<vector<T>>`-typed binding (checked via
  `binding.typeTemplateArg`, a **different** code path than (c)); (c) a
  `location(...)`/`dereference(...)`-wrapped call, recursing on the
  wrapped argument; (d) a call expression with no direct binding, falling
  back to `inferQueryExprTypeText` then re-parsing the inferred type text
  for the same `Reference`/`Pointer`-of-`vector` shape as (b) - this is a
  **fourth, separately-coded** implementation of the same
  Reference/Pointer-unwrap-then-check-vector logic already done twice
  above (BindingInfo-based in (b), type-text-based here). Three
  independent copies of "is this a borrowed vector" inside one function.
- **`preferExplicitCanonicalVectorHelperForReceiver`** (line 120) /
  **`tryResolveExplicitCanonicalVectorCountMethodTarget`** (line 141):
  the `count` method on an explicit canonical-vector-namespaced helper
  path has a receiver-family-conditional diagnostic split found nowhere
  else in this file - a map receiver whose type comes from an **explicit**
  `[return<map<...>>]` annotation on the callee gets a different
  "unknown call target" message than a map receiver whose type is
  **body-inferred** (no explicit annotation), which keeps the older
  "unknown method: <path>" diagnostic. This is a real, deliberately-coded
  behavioral fork on *how* the receiver's type was determined, not just
  *what* it is - pinned by name in the source comment referencing
  "...keeps wrapper array/string same-path helper" and "wrapper temporary
  canonical vector count slash-method rejects map receiver" test names
  (not independently re-verified this round which literal test files
  those map to - flagged for a future pass).

### Row category C: key-value-family resolvers (`SemanticsValidatorMethodTargetKeyValueResolvers.cpp`)

- **Two independently-gated key-value receiver predicates that are not
  each other's superset/subset in an obvious way**:
  `isCanonicalKeyValueReceiver` (line 119, checks
  `extractAnyKeyValueTypes` i.e. canonical-or-experimental map-shaped
  binding/field/call-return, OR `resolveCallCollectionTypePath(...) ==
  "/map"`) vs. `isWrappedKeyValueReceiver` (line 86, checks
  `Reference<map<K,V>>`/`Pointer<map<K,V>>`-wrapped binding/field, OR an
  indexed-args-pack-element access resolving to a wrapped map type). A
  receiver can satisfy one, the other, both, or neither depending on
  exactly which of binding/field/call/indexed-access shape it has - each
  shape is its own guarded branch, none share a common "is this
  map-family" primitive.
  `extractAnyKeyValueTypes` itself is `extractKeyValueCollectionTypes(...)
  || extractExperimentalKeyValueFieldTypes(...)` (line 79) - **two**
  further independent extractors OR'd together, "canonical" and
  "experimental" field-shaped maps, each presumably with its own struct-
  metadata assumptions not audited this round.
- **`preferredKeyValueMethodTarget`** (line 219): the real decision
  function `setPreferredKeyValueMethodTarget` delegates to. Guard
  cascade: (1) if `explicitKeyValueHelperPath` is empty, first resolve
  the borrowed-vs-owned helper name via
  `borrowedKeyValueHelperNameForReceiver` (itself gated on
  `isWrappedKeyValueReceiver` - wrapped receivers get borrowed-variant
  method names, e.g. `at` → registry-driven borrowed name, `at_unsafe` →
  hardcoded `at_unsafe_ref`, the one case TODO-4690/4691 left
  un-registry-migrated per its own comment); (2) if the receiver is
  "compatible experimental" (`resolveExperimentalKeyValueTarget` true),
  prefer the canonical path if declared/imported, else fall back to
  `preferredCanonicalExperimentalKeyValueHelperTarget`; (3) else if an
  explicit spelling was given and resolves to a canonical helper name,
  require the receiver to be experimental-compatible OR canonical OR a
  published key-value constructor call, else return empty (reject); (4)
  else if the canonical path is declared/imported, require
  experimental-compatible OR canonical, else return empty. Four
  receiver-shape-conditional branches, each independently deciding
  accept/reject/rewrite - this is the same "priority disagreement between
  independently-derived family classifications" shape the doc's Evidence
  section names for TODO-4753/TODO-4760, just one level deeper (inside a
  single stage's own key-value resolver, not yet crossing to another
  stage).
- **`resolveKeyValueTarget`** (line 300, the boolean predicate used
  elsewhere as a receiver-family test) has its own, **separately coded**
  cascade for call-expression receivers: indexed-args-pack-element
  (3 sub-variants: direct, dereferenced, wrapped - each its own already-
  audited function from Row category A's sibling helpers) →
  `getBuiltinArrayAccessName` gate (cross-reference: this is the
  ir_lowerer-side function's semantics-stage sibling, see Row category D
  below) → `resolveCallCollectionTypePath(...) == "/map"` (with an odd
  sub-branch: if collection-template-args resolve to exactly 2, or a
  builtin-collection-name check independently also passes, or **neither**
  - all three sub-paths `return true` unconditionally, i.e. once
  `collectionTypePath == "/map"` is established this function cannot
  return false for that receiver regardless of arg-count validity) →
  definition-return-type inference → transform-annotation fallback. This
  is a fifth independent "is this a map receiver" implementation
  alongside the two predicates above and the two extractors inside them.

### Row category D: the ir_lowerer name-collision gate (cross-referenced, not re-derived)

`getBuiltinArrayAccessName`'s `ir_lowerer`-side early-out via
`resolvesKeyValueHelperSurfacePath` (`IrLowererBuiltinNameHelpers.cpp:485-
608`) is a distinct row *category*, not a stage-disagreement row: it is
"name-collision, receiver-blind by construction" per this document's own
2026-09-04 finding above - a bare, unrooted call literally spelled `at`
is excluded from array-access treatment by pure string match against the
stdlib member-name list, before any receiver type is even consulted.
Already fully characterized by TODO-5288's work (see the Update sections
above and TODO-5293's task block in `docs/todo.md` for the semantics-stage
sibling's own divergent branches: capitalized `At`/`AtUnsafe` spellings,
`stripTemplateSpecializationSuffix`, and a member-name-string-returning
contract vs. the ir_lowerer side's bool-only contract, SOA-column
handling, and receiver-base disambiguation) - not re-traced line-by-line
here to avoid duplicating that existing audit. Row for this table: guard
= `scopedName` (from literal call spelling only) matches a stdlib
surface member name; disposition = excluded from builtin-array-access
regardless of receiver type; pinned by `PrimeStruct_backend_ir_tests`
(46 tests, per the 2026-09-04/05 near-regression finding above).

### Row category E: the five snapshot-collection mechanisms (full branch enumeration, 2026-09-08; R14/`query_facts` added 2026-09-08 second round)

Documented in narrative form in the "Step 0 Progress" Update sections
above from the TODO-4760 investigation; expanded here to full branch
level per row (previous round left this at file:line pointers only).
The narrative above (see "Found the true root gate") also names a fifth
sibling, `query_facts`, as independently duplicating the same gap - see
R14 below, added this round.

**R10 - `direct_call_targets` naive pass**
(`collectDirectCallExpr`, `SemanticsValidatorSnapshots.cpp:1578-1640`,
called for every `Definition`'s parameters/statements/returnExpr and every
`Execution`'s arguments/bodyArguments when `!useMergedWorkerPublicationFacts`,
lines 1642-1660). Recurses unconditionally into `expr.args` and
`expr.bodyArguments` regardless of whether the outer `expr` itself
qualified (lines 1638-1639), which is why nested direct calls each get
their own entry. For each visited `expr`:

| # | guard condition | disposition | pinned by |
|---|---|---|---|
| E1 | `expr.kind != Expr::Kind::Call \|\| expr.isMethodCall` | not a direct-call candidate; skip straight to the unconditional recursion into args/bodyArguments | — |
| E2 | `isTaskWaitExpr(expr)` | `resolvedPath = "/task/wait"` unconditionally (no method-name or arg check beyond what `isTaskWaitExpr` itself does) | task-wait tests generally; not specifically pinned to this snapshot collector |
| E2b | **no `isTaskSpawnExpr(expr)` check exists in this function at all** - confirmed by grep, zero hits for `isTaskSpawnExpr` in `SemanticsValidatorSnapshots.cpp` | a task-spawn call falls through to E3 (`preferredCollectionHelperResolvedPath`/`resolveCalleePath`) exactly like an ordinary call, instead of getting a forced `/task/spawn` path the way R11 (below) gives it | UNPINNED - newly found this round, not previously called out. See "R10/R11 divergence" note below for when this is actually reachable (not merely latent) |
| E3 | none of the above; `resolvedPath = preferredCollectionHelperResolvedPath(expr)` | receiver-blind lookup (`CollectionReceiverFamily::None` hardcoded, confirmed by the 2026-09-04 narrative above) | compile_run_benchmark_harness.cpp real-compile dump tests (see below) |
| E4 | `resolvedPath` still empty after E3 | `resolvedPath = resolveCalleePath(expr)` (plain import-alias-priority lookup, the same machinery TODO-4753 found load-bearing) | same |
| E5 | `resolvedPath` non-empty AND `splitSoaSurfaceHelperPath(resolvedPath, ...)` succeeds AND `usesPublicSurface` | redirect to `preferredSoaHelperTargetForCurrentImports(soaHelperName)` if that's non-empty and differs from the current path | UNPINNED as an isolated branch in this snapshot mechanism specifically; SOA public-surface redirection generally exercised elsewhere |
| E6 | `resolvedPath` non-empty; contains `"__t"` with no `/` after it | strip the `__t<hash>` specialization suffix | UNPINNED here specifically |
| E7 | `resolvedPath` non-empty | run all three legacy-SOA canonicalizers (`canonicalizeLegacySoaGetHelperPath`/`RefHelperPath`/`ToAosHelperPath`) **unconditionally** - no "does the current path already have a real backing definition" guard | **Divergence from R11**: R11 (below) added a `canonicalResolvedPathHasRealDefinition` guard around the identical three-canonicalizer call per `docs/CompatPathResolutionConsolidation.md`'s D5 shadow-precedence rule; R10 never got that guard. UNPINNED - not confirmed whether any real call shape reaches R7 with a real-definition-backed path that R10 would then wrongly redirect, but the asymmetry is real and unaudited |
| E8 | `resolvedPath` non-empty; `collectionBridgeChoiceFromResolvedPath(resolvedPath)` returns a value | push a `CollectedBridgePathChoiceEntry` (feeds R12) | compile_run_benchmark_harness.cpp (vector bridge-choice dump, see below) |
| E9 | `resolvedPath` non-empty (regardless of E8) | push a `CollectedDirectCallTargetEntry` | compile_run_benchmark_harness.cpp lines ~1874-1889 (real compile + `--dump-stage semantic-product`, two direct calls `id`/`plus`) |
| E10 | `resolvedPath` still empty after E2-E4 | **no entry created** - the call is silently absent from `direct_call_targets`, not recorded as an error or an "unresolved" placeholder | UNPINNED - no test asserts a specific call is *absent* from this collector; only positive-presence assertions found |

**R11 - `direct_call_targets` local-aware overwrite pass**
(`inferCallSnapshotData`, `SemanticsValidatorSnapshotLocals.cpp:91-206`
[not 91-160 as an earlier round's pointer estimated - the function runs
to line 206], invoked via `forEachLocalAwareSnapshotCall` only when
`!useMergedWorkerPublicationFacts && !skipLocalAwareCallRefinement_`,
`SemanticsValidatorSnapshots.cpp:1662`). For each call `expr` visited:

| # | guard condition | disposition | pinned by |
|---|---|---|---|
| F1 | `isTaskWaitExpr(expr)` AND `inferTaskWaitBinding(...)` succeeds | `resolvedPath = "/task/wait"`, `binding = waitBinding`, **return true immediately** - skips every later branch (F3 onward) entirely, including the canonicalizer cascade R10 always runs for this same call shape | task-wait tests generally; not specifically pinned to this collector's overwrite behavior |
| F2 | `isTaskSpawnExpr(expr)` AND `inferTaskSpawnBinding(...)` succeeds | `resolvedPath = "/task/spawn"`, same early-return shape as F1 | same caveat as F1 |
| F3 | none of F1/F2; `preferredCollectionHelperResolvedPath(expr)` non-empty | that becomes the base `resolvedPath` | compile-and-dump tests generally |
| F4 | `resolvedPath` still empty; `expr.kind==Call && !expr.isMethodCall && expr.args.size()==1` AND (`isUnqualifiedCollectionBuiltinName(expr,"count")` OR `...("capacity")`) | try `resolveVectorHelperMethodTarget` on the single arg; use its result if non-empty | UNPINNED to this collector specifically - this whole branch has **no counterpart in R10 at all** (R10 never tries a vector-helper-method resolution for bare `count`/`capacity` calls); another R10/R11 divergence, in addition to the task-spawn one above |
| F5 | `resolvedPath` still empty AND NOT (`expr.kind==Call && expr.isMethodCall`) | `resolvedPath = resolveCalleePath(expr)` | same fallback R10 uses at E4 |
| F6 | `expr.kind==Call && expr.isMethodCall && !expr.args.empty()` | try `resolveMethodTarget(...)` on the receiver (`expr.args.front()`); overwrite `resolvedPath` if it succeeds - this is the one path where a *method*-shaped call can still reach this "direct call targets" collector's overwrite pass, since R10 only ever visits `!expr.isMethodCall` calls (E1) | UNPINNED specifically for this collector; `resolveMethodTarget` itself is covered extensively by Row categories A-C above |
| F7 | `resolvedPath` non-empty (from F3/F4/F5/F6) | run `resolveExprConcreteCallPath` to attempt a more concrete resolution; if non-empty, adopt it | UNPINNED to this collector specifically |
| F8 | `resolvedPath` non-empty | strip `__t<hash>` suffix (same as R10's E6) | same |
| F9 | `resolvedPath` non-empty; `canonicalResolvedPathHasRealDefinition` is **false** (`defMap_.count(...)==0 && !hasDefinitionFamilyPath(...)`) | run the same three legacy-SOA canonicalizers as R10's E7, but **only** under this guard | this is the D5 shadow-precedence guard R10 lacks (see R10's E7 divergence note) |
| F9b | `canonicalResolvedPathHasRealDefinition` is true | **skip** all three canonicalizers - the already-real-definition path is trusted as-is | UNPINNED as an isolated branch; the guard's existence is documented at D5 in the compat-spelling doc, not re-verified against a live test this round |
| F10 | `resolvedPath` non-empty | `inferResolvedDirectCallBindingType(resolvedPath, ...)`; if it yields a non-empty type name, set `out.binding` from it | UNPINNED to this collector specifically |
| F11 | `out.binding.typeName` still empty | fall back to `inferBindingTypeFromInitializer(expr, ...)` | same |
| F12 | return value | `!out.resolvedPath.empty() \|\| !out.binding.typeName.empty()` - the caller (`SemanticsValidatorSnapshots.cpp:1662-1721`) only overwrites R10's already-collected entry (erasing the old one by `semanticNodeId` or by scope/name/line/column match, then re-inserting) when this returns true **and** `callData.resolvedPath` is itself non-empty (line 1671) - a call where `inferCallSnapshotData` returns true solely because it found a binding type but no resolved path leaves R10's original (possibly wrong) `direct_call_targets`/`bridge_path_choices` entries untouched | UNPINNED - no test found specifically exercising "local-aware pass found a binding but no path" as distinct from "found nothing at all" |

**R10/R11 divergence, when it's reachable, not just latent:** both
E2b (task-spawn) and F4 (bare `count`/`capacity`) are real behavioral
differences between the naive and local-aware passes, but R11 only runs
at all when `!useMergedWorkerPublicationFacts && !skipLocalAwareCallRefinement_`
(`SemanticsValidatorSnapshots.cpp:1662`). `skipLocalAwareCallRefinement_`
is forced `true` for the duration of `collectPilotRoutingSemanticProductFacts()`
(lines 1061-1063) when validation partitions a definition range for
worker-parallel "pilot routing" - i.e. **for that code path only R10 runs
at all**, and its task-spawn/count/capacity gaps versus R11 are live, not
merely latent, for whatever definitions get routed through the pilot
path. Not confirmed this round whether/how often the pilot-routing path
is exercised in the test corpus for a definition actually containing a
bare task-spawn, `count`, or `capacity` call - flagged, not resolved.

**R12 - `bridge_path_choices`**
(populated only from R10's naive pass at E8 above - R11's overwrite path
also pushes into `collectedBridgePathChoices_`, at
`SemanticsValidatorSnapshots.cpp:1699-1711`, using the *same*
`collectionBridgeChoiceFromResolvedPath` helper on `callData.resolvedPath`,
so R12 is really "R10's E8 or R11's equivalent overwrite", not a
free-standing third mechanism). The helper itself
(`collectionBridgeChoiceFromResolvedPath`, lines 326-446) is its own
small cascade:

| # | guard condition | disposition | pinned by |
|---|---|---|---|
| G1 | `isInternalSoaCollectionTypePath(normalizedResolvedPath)` (after stripping a `__t<hash>` suffix that has a `/` before it and no `/` after - a **third**, textually-similar-but-not-identical suffix-stripping implementation alongside R10's E6 and R11's F8) | family = `internalSoaCollectionTypeName()` for both elements of the returned pair | not independently verified this round which literal test pins this |
| G2 | `findStdlibSurfaceMetadataByResolvedPath(resolvedPath)` returns non-null AND matches `vectorHelperSurfaceMetadata()`/`vectorConstructorSurfaceMetadata()` | family = `"vector"` | compile_run_benchmark_harness.cpp (`bridge_path_choices[0]: ... collection_family="vector"`) |
| G3 | metadata non-null AND `isMapCollectionSurfaceMetadata(*metadata)` (matches key-value helper or constructor surface metadata) | family = `"map"` | UNPINNED to this specific collector (map bridge-choice dump not found in the grepped test set this round) |
| G4 | metadata non-null AND `metadata->id` is `CollectionsColumnarHelpers`/`CollectionsColumnarConstructors` | family = `internalSoaCollectionTypeName()` | UNPINNED to this specific collector |
| G5 | metadata non-null, none of G2-G4 | `return std::nullopt` - no bridge-choice entry at all for this call | UNPINNED |
| G6 | metadata null; resolved path matches one of several hardcoded SOA-compat prefixes (`samePathSoaHelperTargetPath`, `compatibilitySoaHelperTargetPath`, the experimental-SOA-vector prefix with an 8-way hardcoded method-name table, or the experimental-SOA-conversions prefix with a 2-way table) | family = `internalSoaCollectionTypeName()`, helper name = the matched/mapped name | UNPINNED to this specific collector; the underlying compat-path tables are covered by `CompatPathResolutionConsolidation.md`'s own corpus, not re-verified here |
| G7 | metadata null, none of G6's prefixes match | `return std::nullopt` | UNPINNED |
| G8 | metadata non-null, family resolved (G2-G4); `resolveStdlibSurfaceMemberName(*metadata, resolvedPath)` returns empty | `return std::nullopt` even though a family was found - i.e. family resolution alone is not sufficient, the member-name extraction can still veto the whole entry | UNPINNED - no test found isolating this specific veto path |

Production gate: `isSemanticCollectorEnabled(buildConfig, "bridge_path_choices")`
(`SemanticsValidatorSnapshots.cpp:1857,1867`) controls whether the
already-collected `collectedBridgePathChoices_` vector is moved into the
publication surface at all - the collection itself (E8/F-equivalent
above) always runs regardless of this flag; only the final hand-off is
gated. Same "collect always, gate only at publish time" pattern as R13
below.

**R13 - `collection_specializations`**
(producer traced this round: `publishCollectionSpecializationForBinding`,
`src/semantics/SemanticPublicationBuilders.cpp:735-804`, called once per
binding fact from `publishBindingFacts`, line 1635 - i.e. this mechanism
runs off *binding facts*, not off call expressions at all, unlike R10-R12).
`classifyCollectionSpecialization` (lines 600-653) is the real branch
cascade, driven by `bindingEntry.bindingTypeText`:

| # | guard condition | disposition | pinned by |
|---|---|---|---|
| H1 | `splitTemplateTypeName` fails on the current type text (not template-shaped) | `return false` - no specialization entry at all for this binding | implicit in every non-collection-typed binding in the corpus |
| H2 | base (after `normalizeCollectionSpecializationTypeName`) is `"Reference"` or `"Pointer"`, with exactly one template arg | set `isReference`/`isPointer` (OR'd - a `Reference<Pointer<vector<T>>>` chain would set **both** flags true, since the loop unwraps repeatedly and both flags are cumulative OR, never reset), unwrap one layer, loop again | test at `test_semantics_type_resolution_graph_snapshots_semantic_product_publishes_ids.cpp` (`pairsRef`/`particleRefs` entries both assert `isReference==true, isPointer==false` for single-`Reference<...>` wraps; **no test found for a `Pointer<...>` wrap, nor for a doubly-wrapped `Reference<Pointer<...>>` or `Pointer<Reference<...>>` chain** - UNPINNED for those shapes) |
| H2b | `splitTopLevelTemplateArgs` fails or yields != 1 arg for a `Reference`/`Pointer` base | `return false` | UNPINNED |
| H3 | base == `"vector"` (after normalization, which maps `/vector`, `/std/collections/vector`, `"Vector"`, and the experimental `Vector` spelling all to the bare string `"vector"`), with exactly one template arg | family=`"vector"`, `elementTypeText`=`valueTypeText`=the single arg | `test_semantics_type_resolution_graph_snapshots_semantic_product_publishes_ids.cpp` vector-entry assertions |
| H3b | template-arg count != 1 for `"vector"` base | `return false` | UNPINNED |
| H4 | base == `"soa"` (after normalization, which maps `/soa`, the legacy SOA folder root, `kSoaVectorTypeName` bare/rooted, and the `soa/SoaVector` member-path spellings all to `kLegacySoaVectorFolder`) with exactly one template arg | family=`"soa"`, `elementTypeText`=`valueTypeText`=the arg | same test file's `particleRefs`/`soaEntry` assertions |
| H4b | template-arg count != 1 for `"soa"` base | `return false` | UNPINNED |
| H5 | base == `"map"` (normalization maps anything matching the key-value helper surface's canonical path or import-alias spellings, or an unspecialized experimental `Map` backing type, to `"map"`) with exactly two template args | family=`"map"`, `keyTypeText`/`valueTypeText` = the two args in order | same test file's `mapEntry` assertions (including `structPath` construction via `collectionSpecializationStructPath`, which itself only produces a non-empty path for the `"map"` family) |
| H5b | template-arg count != 2 for `"map"` base | `return false` | UNPINNED |
| H6 | base matches none of `Reference`/`Pointer`/`vector`/`soa`/`map` after normalization | `return false` - e.g. a `vector<vector<T>>` binding's *outer* classification succeeds as `"vector"` with `elementTypeText` left as the literal nested-template text (`"vector<T>"`), not itself recursively re-classified into a nested specialization entry | UNPINNED - no test found asserting nested-collection element-type text is or isn't itself expanded |

Production gate: unlike R12, there is **no** `isSemanticCollectorEnabled(buildConfig,
"collection_specializations")` check anywhere in the source tree - grepped
the whole `src/` tree for the literal string `"collection_specializations"`
and the only hit is the dump-formatter's label string
(`src/frontend/SemanticProduct.cpp:1581`), not a collector-enable gate.
Production is entirely piggybacked on the **`"binding_facts"`** collector
flag instead (`isSemanticCollectorEnabled(buildConfig, "binding_facts")`,
`SemanticsValidatorSnapshots.cpp:1897`, which populates
`publicationSurface.bindingFacts`, which `publishBindingFacts` then always
walks unconditionally at line 1635 whenever it's non-empty). This means a
build config that disables `binding_facts` specifically but leaves
`collection_specializations` in its collector allowlist gets **no**
`collection_specializations` entries at all, silently - there is no way to
request one without the other. UNPINNED as an intentional-vs-accidental
design choice; not found documented anywhere as deliberate.

**R14 - `query_facts`** (full branch enumeration, 2026-09-08 - the fifth
sibling this document's own 2026-09-04 narrative named but never gave a
row to). Producer: `SemanticsValidator::ensureQuerySnapshotFactCaches`
(`SemanticsValidatorSnapshotLocals.cpp:420-`), which builds
`queryFactSnapshotCache_` in two passes, both driven by
`forEachLocalAwareSnapshotCall` (the same local-aware traversal R11 uses)
and both delegating the actual per-call inference to
`inferQuerySnapshotData` (`SemanticsValidatorSnapshotLocals.cpp:9-89`).

Important correction to the 2026-09-04 narrative's framing: `query_facts`
is **not** an independently-derived fifth answer at the `resolvedPath`
level. `inferQuerySnapshotData`'s very first step
(`SemanticsValidatorSnapshotLocals.cpp:34-38`) calls
**`inferCallSnapshotData` directly** - the exact same function R11 already
fully characterizes above - and takes its `resolvedPath`/`binding`
verbatim. So `query_facts` inherits every one of R11's already-documented
branches and divergences (F1/F2's task-wait/task-spawn early-returns,
F4's bare-`count`/`capacity` R10-less branch, F9/F9b's D5 shadow-guard) at
the resolvedPath layer for free - it is a *consumer* of R11's answer, not
a sixth independent re-derivation of it. What genuinely is independent
here is the `typeText`/`resultInfo`/`receiverBinding` machinery layered
on top:

| # | guard condition | disposition | pinned by |
|---|---|---|---|
| Q1 | `inferCallSnapshotData(defParams, activeLocals, expr, callData)` succeeds | `out.resolvedPath`/`out.binding` = `callData`'s (see R11 above - not re-derived here) | R11's own pinning |
| Q2 | `out.typeText = bindingTypeText(out.binding)`; if that's empty | fall back to `inferQueryExprTypeText(expr, defParams, activeLocals, out.typeText)` (its own, separate type-text inference function - not itself branch-enumerated this round, flagged as a further-detail opportunity) | UNPINNED to this collector specifically |
| Q3 | `!out.binding.typeName.empty()` | `resolveResultTypeFromTypeName(out.typeText, out.resultInfo)`; if it fails or `!isResult`, reset `resultInfo = {}` | UNPINNED |
| Q3b | `out.binding.typeName.empty()` | `resolveResultTypeForExpr(expr, defParams, activeLocals, out.resultInfo)` instead (a **different** function than Q3's, its own independent Result-type inference path); same `!isResult` reset rule | UNPINNED - two independently-coded Result-type inference paths selected purely by whether `Q1` already produced a typed binding, mirroring this document's general "priority disagreement between independently-derived answers" shape one level deeper |
| Q4 | `expr.kind==Call && !args.empty() && !out.resolvedPath.empty() && (expr.isMethodCall \|\| resolvedPath starts with "/std/collections/" \|\| resolvedPath starts with "/array/")` | receiver-query-candidate gate for `Q5`/`Q5b` below - this specific 3-way OR (method-call-shaped, or a collections-prefixed, or an array-prefixed resolved path) has no counterpart anywhere in R10-R13 | UNPINNED |
| Q5 | Q4 true; `receiverExpr.kind == Name` | `findBinding(defParams, activeLocals, receiverExpr.name)`, if found, sets `out.receiverBinding` | UNPINNED to this collector |
| Q5b | `out.receiverBinding.typeName` still empty AND `expr.isMethodCall` | `inferBindingTypeFromInitializer(receiverExpr, ...)` fallback; on failure or an empty resulting `typeName`, `out.receiverBinding` is reset to `{}` (matching R11's F11's own use of the identical helper for a different field) | UNPINNED |
| Q6 | overall return value | `true` iff any of `resolvedPath`/`typeText`/`binding.typeName`/`resultInfo.isResult`/`receiverBinding.typeName` is non-empty/true | — |

Producer's own two passes, both gated by `!out.resolvedPath.empty()`
before an entry is ever pushed (`ensureQuerySnapshotFactCaches`,
`SemanticsValidatorSnapshotLocals.cpp:420-`):

- **Pass 1** (line 427): visits **every** `Call`-kind expr the local-aware
  traversal reaches (no method-call/receiver-shape filter at all, unlike
  Q4's gate which only restricts `Q5`/`Q5b`) via `inferQuerySnapshotData`;
  pushes a `QueryFactSnapshotEntry` unconditionally whenever
  `resolvedPath` is non-empty - **no de-duplication** for this pass.
- **Pass 2** (line 467): specifically for `pick(...)` calls
  (`isSimpleCallName(expr, "pick") && expr.args.size()==1 &&` the call has
  body arguments) whose single argument is itself a `Call` - runs
  `inferQuerySnapshotData` on that **inner target call**, not on the
  `pick(...)` call itself, and appends only if an identical entry
  (matched by scope path, call name, source line/column, resolvedPath,
  and semanticNodeId) isn't already present. UNPINNED as to whether Pass 2
  ever contributes an entry Pass 1 didn't already produce: since Pass 1's
  traversal already recurses into every expr's `args` (including the
  `Call` nested inside `pick`'s own single argument), the inner target
  call would ordinarily already have its own Pass-1-produced entry at the
  same line/column/semanticNodeId - not independently verified this round
  whether some traversal-order or scope difference makes Pass 2 load-
  bearing for any real call shape, or whether it is pure redundant
  insurance.

**Production gate and a new pilot-routing asymmetry, not yet in R10-R13's
list.** `query_facts` is gated by the `"query_facts"` collector flag
(`SemanticsValidatorSnapshots.cpp:1917-1920`); when
`useMergedWorkerPublicationFacts` is true it uses
`mergedWorkerPublicationFacts_.queryFacts` (merged across parallel
workers), otherwise it calls `queryFactSnapshotForSemanticProduct()`
fresh. Unlike R11's own call site
(`SemanticsValidatorSnapshots.cpp:1662`, gated on
`!useMergedWorkerPublicationFacts && !skipLocalAwareCallRefinement_`),
`ensureQuerySnapshotFactCaches`'s call into `forEachLocalAwareSnapshotCall`
has **no** `skipLocalAwareCallRefinement_` check anywhere in its own code
or call chain (confirmed by reading
`SemanticsValidatorSnapshotLocals.cpp` in full) - so if
`queryFactSnapshotForSemanticProduct()` were ever reached while that flag
is forced true, `query_facts` would keep running full local-aware
refinement (and thus keep tracking R11's own answer, task-spawn handling
included) exactly when `direct_call_targets` has fallen back to R10-only.
On a first read this looks latent rather than live: the two known
call sites that force `skipLocalAwareCallRefinement_ = true`
(`SemanticsValidatorSnapshots.cpp:1061-1063`) bracket a call to
`collectPilotRoutingSemanticProductFacts()` specifically, while
`queryFactSnapshotForSemanticProduct()`'s own call site
(`SemanticsValidatorSnapshots.cpp:1917-1920`, inside
`takeSemanticPublicationSurfaceForSemanticProduct`) is a separate function
not nested inside that bracket - not proven either way this round, flagged
for whoever next audits the worker-parallel pilot-routing path as a whole.

All four of R10-R13 were confirmed (2026-09-04, see Update sections
above) to independently compute the same wrong answer for the TODO-4760
repro before that bug's actual root cause (Row category D, not any of
these four) was found - i.e. these four are a real, demonstrated instance
of duplicated receiver-family logic *within* the semantics stage alone,
but turned out not to be the specific TODO-4760 defect's root cause. They
remain open Step 0 rows regardless: `preferredCollectionHelperResolvedPath`
is explicitly, by-design receiver-blind (`CollectionReceiverFamily::None`
hardcoded per the compat-spelling document's own scope decision), so any
future receiver-family classifier work here needs to treat R10-R13 as
call/binding sites needing a receiver-aware answer plumbed in, not as
receiver-logic to consolidate directly. The R10/R11 divergences found
in the first Row-E round (task-spawn, bare count/capacity, the D5-guard
asymmetry) are new findings beyond what TODO-4760's investigation already
established - none of them were the TODO-4760 repro's root cause either
(that was Row category D), but they are real, currently-uncharacterized-
elsewhere behavioral differences between the two `direct_call_targets`
producers. R14/`query_facts`, added this round, is a different case again:
it is not a sixth independent receiver-family re-derivation at all (its
`resolvedPath` is R11's own `inferCallSnapshotData` output, reused
directly) - its independent surface is narrower (the `typeText`/
`resultInfo`/`receiverBinding` layer on top, per R14's own table above),
but that narrower surface has its own two independently-coded Result-type
inference paths (Q3/Q3b) and its own receiver-binding gate (Q4/Q5/Q5b)
with no counterpart in R10-R13.

### Row category F: monomorphization stage (`resolveMethodCallTemplateTarget` and its collection-compatibility-path helpers, 2026-09-08)

Covers `TemplateMonomorphMethodTargets.cpp` (single entry point,
`resolveMethodCallTemplateTarget`, lines 105-719) and
`TemplateMonomorphCollectionCompatibilityPaths.cpp` (helper functions it
calls, fully read this round - `unwrapCollectionReceiverEnvelope`,
`normalizeCollectionReceiverTypeName`, `isCollectionReceiverTypeName`, and
the removed/compat-alias predicates). Per this document's own prior
finding, TODO-5286 already characterized `unwrapCollectionReceiverEnvelope`'s
`args<T>` gap in full (see `docs/todo_finished.md`, September 6 entry) -
cross-referenced below at F-args, not re-derived. No dedicated unit test
file calls `resolveMethodCallTemplateTarget` or any of the
`TemplateMonomorphCollectionCompatibilityPaths.cpp` functions directly by
name (grepped the whole `tests/` tree); this whole stage is exercised
only indirectly, through the `compile_run` corpus's observable compiled
output - same "no direct pinning, only end-to-end absorption" pattern
TODO-5286's investigation already established for this file specifically.

`resolveMethodCallTemplateTarget` is a strict cascade; branch order
matters, first match returns:

| # | guard condition | disposition | pinned by |
|---|---|---|---|
| F0 | `!expr.isMethodCall \|\| expr.args.empty() \|\| expr.name.empty()` | `return false` immediately | — |
| F1 | receiver is a bare `Expr::Kind::Name` whose **literal spelling** (after `normalizeBindingTypeName`) is exactly `"FileError"` (not a binding lookup - the receiver identifier text itself must read `FileError`) AND method name ∈ `{result, status, why, is_eof, eof}` (5 names) | dispatch to a hardcoded `/std/file/FileError/<method>` path via `selectStaticHelperOverloadPath` | UNPINNED to this specific literal-receiver-spelling shape; not independently verified this round which test (if any) calls methods on a receiver expression that is literally the identifier `FileError` rather than a `FileError`-typed binding |
| F1-not | receiver is `Name` spelled `FileError` but method NOT in that 5-name set | falls through to the rest of the cascade (F2 onward) exactly as if F1 didn't exist - `typeName` inference below will not treat this as a `FileError` at all (nothing in the receiver-type-inference block special-cases a bare `FileError`-spelled Name), so this receiver shape typically reaches `typeName.empty()` at F5 and returns false | UNPINNED |
| F2 | `resolveIndexedArgsPackMapMethodTarget()` succeeds - a narrow shape: receiver is a non-binding, non-method `Call` named exactly `at`/`at_unsafe` with 2 args, whose own first arg is a `Name` bound (in `locals`) to an args-pack element type that itself extracts as a key-value (map) element type | dispatch to `metadataBackedCanonicalKeyValueHelperPath(helperName)`, with `count`/`contains`/`tryAt`/`at`/`at_unsafe`/`insert` renamed to their `_ref` borrowed variant when the args-pack element type is itself `Reference<...>`/`Pointer<...>`-wrapped | UNPINNED to this exact call shape this round; note this check runs **after** the full receiver-type-inference block (F3 below) has already run and possibly set `typeName`/`wrappedReceiverTypeName`, but **before** the `typeName.empty()` early-return (F5) - i.e. it can override an already-successfully-inferred `typeName`'s dispatch entirely if the args-pack-map shape also matches, a priority-ordering fact not documented anywhere in-source |
| F3 | receiver-type inference (full branch enumeration below, 2026-09-08) | sets `typeName`/`wrappedReceiverTypeName`/`isBorrowedSoaReceiver` for `Name`/`Literal`/`BoolLiteral`/`FloatLiteral`/`StringLiteral`/`Call`-kind receivers | see "F3 detail" table below |
| F5 | `typeName.empty()` after F3 (and F2 didn't already return) | `return false` | — |
| F6 | `!expr.templateArgs.empty()` (explicit method-call template args given) AND `wrappedReceiverTypeName`'s base (post-normalization) is `Reference`/`Pointer` with a non-empty arg | try a "wrapper method path" `/<Reference\|Pointer>/<method>`; dispatch to it **only if** both `hasTemplatedDefinitionFamilyPath` and `hasDefinitionFamilyPath` confirm a real definition family exists there - preferred over unwrapping to the inner type `T`'s own family when it applies | UNPINNED; source comment references a "wrapper temporary canonical vector count slash-method" test name (mirrors Row category B's `preferExplicitCanonicalVectorHelperForReceiver` note) - not independently re-verified which literal test file |
| F7 | `typeName == "File"` or its leaf is `"File"`, AND `isFileMethodName(normalizedMethodName)` (`write`/`writeLine`/`write_line`/`writeByte`/`write_byte`/`readByte`/`read_byte`/`writeBytes`/`write_bytes`/`flush`/`close`) | dispatch via `preferredFileMethodTarget`, itself gated: builtin `/file/<name>` for most names; for `write`/`write_line` specifically, builtin also if `expr.args.size() > 10` or receiver is literally the `self` binding; otherwise prefer `/File/<name>` if a real definition exists there, else fall back to builtin | UNPINNED to this exact branch |
| F8 | `isExplicitRemovedCollectionMethodAlias(typeName, rawMethodName)` (per-family removed-helper-name check via `CollectionSpellingClassifier`, one sub-cascade per family: SOA, vector/array, map) | `return false` - explicitly rejected as a removed compatibility spelling | covered by `CompatPathResolutionConsolidation.md`'s own corpus, not re-verified here |
| F9 | `isPrimitiveBindingTypeName(typeName)` | dispatch to `/<typeName>/<normalizedMethodName>` unconditionally | UNPINNED to this branch specifically |
| F10 | `typeName`'s leaf (post-`normalizeCollectionReceiverTypeName`) is exactly `"args"` | only `count`/`at`/`at_unsafe` (array-normalized) dispatch to `/array/<name>`; any other method `return false` - this is the **direct downstream consumer of the TODO-5286 gap**: `unwrapCollectionReceiverEnvelope` has no `args<T>` case (confirmed, see cross-reference below), so an `args<map<K,V>>` receiver's `typeName` never becomes `"map"` and always lands here instead, meaning the map-family dispatch below (F13/generic) is unreachable for any args-pack-of-map receiver reaching this function - this is the same closed-loop TODO-5286 already traced end-to-end (found unreachable-in-effect, not landed) | args-pack corpus for the reachable `count`/`at`/`at_unsafe` cases; TODO-5286's closed investigation for the unreached map-family gap |
| F11 | leaf is `FileError`/`ImageError`/`ContainerError`/`GfxError` AND method name (after `normalizeFileErrorMethodName`, which only maps `isEof`→`is_eof`) ∈ `{why, is_eof, status, result}` (**4** names - `eof` is absent here) | dispatch to the matching `/std/<domain>/<Error>/​<method>` static path (`GfxError` additionally prefers an experimental-namespace path over the canonical one when a definition exists there, per an in-source comment explaining this was a real, previously-fixed bug) | UNPINNED to this branch; the `eof` gap below is a new finding |
| F11-eof | leaf is `FileError` AND method is `eof` | **not matched here at all** - `eof` only dispatches via F1's literal-`Name`-spelled-`FileError`-receiver path above; a bound `FileError`-typed variable's `.eof()` call (as opposed to a call where the receiver expression is literally spelled `FileError`) has no matching branch in this function and falls through to the final generic-resolution fallback (F16) instead of the intended static path | UNPINNED - newly found this round; not confirmed whether this is reachable in practice (whether `.eof()` is even a real supported method name outside the literal-`FileError`-receiver shape) or whether the semantics stage already rejects/rewrites it before monomorphization runs, mirroring the TODO-5286 "real gap, unconfirmed live impact" shape |
| F12 | `isTemplateMonomorphSoaReceiverType(normalizedTypeName)` (generic, not-yet-resolved-to-a-concrete-definition SOA receiver) AND method ∈ one of 4 borrowed/owned pairs (`count`/`count_ref`, `toAos`/`toAosRef` name variants, `get`/`get_ref`, `ref`/`ref_ref`) OR `push`/`reserve` (no pair) | dispatch via the matching `preferredSamePath*MethodTarget` helper; for the 4 paired methods, `isBorrowedSoaReceiver` is consulted to substitute the `_ref` wrapper name via `borrowedSoaWrapperMethodName` first | UNPINNED to this branch |
| F13 | none of F7-F12; `ctx.sourceDefs` lacks a definition at `resolveTypePath(typeName, ...)`, resolved via optional import-alias substitution first, AND `typeName` ∈ `{array, vector, map, soa-family}` | dispatch to `/<typeName>/<normalizedMethodName>`, run through `preferVectorStdlibHelperPath` | UNPINNED to this branch specifically |
| F13b | same "no definition" condition, `typeName == "string"` | dispatch to `/string/<normalizedMethodName>` | UNPINNED |
| F13c | same "no definition" condition, neither a collection family nor string | `return false` | UNPINNED |
| F14 | `ctx.sourceDefs` **does** have a definition at `resolvedType`, AND it is specifically an experimental-SOA-*specialized* type path (`isConcreteExperimentalSoaReceiver`), AND method matches one of the same 4 pairs/`push`/`reserve` as F12 | dispatch via the same `preferredSamePath*MethodTarget` helpers as F12, but **without ever consulting `isBorrowedSoaReceiver`/`borrowedSoaWrapperMethodName`** - the borrowed-vs-owned renaming F12 applies for the generic (not-yet-concrete) SOA case is silently skipped once the receiver resolves to a concrete experimental-SOA type | UNPINNED - newly found this round; a genuine asymmetry between F12 and F14 for what should be the same logical distinction (borrowed vs. owned SOA receiver), not confirmed whether any real borrowed-and-concrete-experimental-SOA receiver shape is reachable to expose it |
| F15 | none of the above; a `receiverHelperFamilyLeaf`-derived "rooted" path (`/<leaf>/<method>`) has a real definition family but the "same-path" `<resolvedType>/<method>` does not, and the two differ | dispatch to the rooted path instead of the same-path one | UNPINNED |
| F16 | fallback (always reached if nothing above returned) | dispatch to `<resolvedType>/<normalizedMethodName>`, run through `preferVectorStdlibHelperPath` then `selectHelperOverloadPath` - **always returns true**, this function has no final "unresolved" `return false` once a definition exists at `resolvedType` | UNPINNED to this exact branch; this is also F11-eof's actual landing branch per the finding above |

**F3 detail - receiver-type-inference sub-cascade (full branch enumeration, 2026-09-08).**
`resolveMethodCallTemplateTarget`'s `typeName`/`wrappedReceiverTypeName`/
`isBorrowedSoaReceiver` are computed by a receiver-kind dispatch
(`TemplateMonomorphMethodTargets.cpp:402-478`), traced in full this round.
Note first: `resolveIndexedArgsPackMapMethodTarget()` (F2's guard) is
*textually* evaluated once this whole block finishes (line 479), not
before it - so a call reaching F2's dispatch has already paid for the
full F3 cascade below, and F2's dispatch (when it fires) discards
whatever F3 computed, exactly as F2's own row above already notes.

| # | guard condition | disposition | pinned by |
|---|---|---|---|
| F3-N1 | `receiver.kind == Name` AND `receiver.name` found in `locals` | sets `wrappedReceiverTypeName`/`isBorrowedSoaReceiver`/`typeName` from the bound local's type text (`bindingTypeText` → `qualifyImportedCollectionTypeText` → `unwrapImportedCollectionReceiverType`/`isBorrowedSoaReceiverType`) | args-pack/vector/map corpus generally |
| F3-N2 | `receiver.kind == Name` AND NOT found in `locals` | `typeName` stays empty - no other branch in this cascade covers an unbound `Name` receiver, so this call falls straight to F5's `return false` unless F2's args-pack-map shape happens to also match (it can't, since F2 requires a `Call`-kind receiver) | UNPINNED - newly found; undetermined whether an unbound-`Name` method-call receiver is reachable past semantics-stage validation (which normally rejects unbound identifiers earlier) |
| F3-L | `receiver.kind == Literal` | `typeName` = `"u64"`/`"i64"`/`"i32"` from `isUnsigned`/`intWidth` | primitive-literal-receiver corpus |
| F3-B | `receiver.kind == BoolLiteral` | `typeName = "bool"` | same |
| F3-Fl | `receiver.kind == FloatLiteral` | `typeName` = `"f64"`/`"f32"` from `floatWidth` | same |
| F3-S | `receiver.kind == StringLiteral` | `typeName = "string"` | same |
| F3-C1 | `receiver.kind == Call`; `inferBindingTypeForMonomorph(receiver, ...)` succeeds | sets `wrappedReceiverTypeName`/`isBorrowedSoaReceiver`/`typeName` from the inferred `BindingInfo`, same helper chain as F3-N1 | call-receiver corpus generally |
| F3-C2 | `receiver.kind == Call`; `typeName` still empty after F3-C1 | `inferExprTypeTextForTemplatedVectorFallback(...)` sets `typeName`/`isBorrowedSoaReceiver` from its own inferred type text - **`wrappedReceiverTypeName` is NOT updated here**, an asymmetry with every other assignment site in this cascade (all of which set all three fields together) | UNPINNED - newly found; matters because F6 (the wrapper-method-path branch) reads `wrappedReceiverTypeName` specifically, so a receiver that only resolves via this fallback can never reach F6's wrapper-path dispatch even if it is genuinely `Reference`/`Pointer`-wrapped |
| F3-C3 | `receiver.kind == Call`; `!receiver.isBinding` (always evaluated, regardless of whether F3-C1/C2 already set `typeName`) | resolves `receiver` itself as a call - `resolveMethodCallTemplateTarget` (recursive) if `receiver.isMethodCall`, else `resolveCalleePath` - to a `resolved` path | — |
| F3-C3a | `resolved` found in `ctx.sourceDefs` AND it is a struct definition | **immediate return true**, `pathOut = resolved + "/" + methodName` - bypasses the rest of F3, F5, and the entire F6-F16 cascade for this call entirely, without any further type-family classification | UNPINNED to this branch specifically |
| F3-C3b | `resolved` found, not a struct definition; first `return<T>` transform annotation with a non-`"auto"` arg | **unconditionally overwrites** `wrappedReceiverTypeName`/`isBorrowedSoaReceiver`/`typeName` with the annotation's type - this fires and wins **even if F3-C1/C2 already produced a `typeName`**, silently discarding it; no priority between "receiver's own inferred type" and "receiver call's declared return-type annotation" is documented anywhere in-source | UNPINNED - newly found; undetermined whether any real receiver shape has both a genuine F3-C1/C2 answer and a competing `return<T>` annotation that disagrees with it |
| F3-C3c | `resolved` found, not a struct, no matching `return<T>` transform, AND `typeName` is (still) empty at this point | `inferDefinitionReturnBindingForTemplatedFallback(...)` sets `typeName`/`wrappedReceiverTypeName`/`isBorrowedSoaReceiver` if it succeeds - unlike F3-C3b, this step **is** gated on `typeName` being empty | UNPINNED |
| F3-C3d | `resolved` NOT found in `ctx.sourceDefs` at all | `getBuiltinCollectionName(receiver, collection)`, if it succeeds, **unconditionally overwrites** `typeName = collection` - the same override-priority gap as F3-C3b, just via a different helper and a different "resolved" outcome (no definition at all, vs. a non-struct definition) | UNPINNED - newly found |

Cross-reference: `unwrapCollectionReceiverEnvelope` and
`normalizeCollectionReceiverTypeName` (`TemplateMonomorphCollectionCompatibilityPaths.cpp:215-316`)
are the receiver-family-normalization primitives F3/F10 depend on. Fully
read this round (not previously done): `normalizeCollectionReceiverTypeName`
recognizes `vector` (canonical path, `Vector`/`Vector__*` bare spellings,
legacy-experimental vector path), the SOA family (via
`isExperimentalSoaVectorTypePath`), and `map` (via
`isTemplateMonomorphMapCollectionRoot`'s canonical-path-or-import-alias
match, or a bare/generated `Map`/`Map__*` experimental backing-type name)
- everything else passes through unchanged, including a bare `"args"`
(confirmed: no `args`-handling branch anywhere in this function, matching
TODO-5286's finding one level up in its caller,
`unwrapCollectionReceiverEnvelope`, which is the one TODO-5286 already
fully characterized: no `args<T>` case in either of its two structurally
near-identical unwrap loops, lines 259-276 and 278-316 - two more
independently-coded near-duplicate unwrap loops in the same function,
not previously called out as a within-function duplication in TODO-5286's
own writeup, though the net *effect* TODO-5286 measured (args stays
"args", never unwraps) is unchanged by which loop copy hits it).

### Row category G: `ir_lowerer` stage (`resolveMethodCallDefinitionFromExpr`, 2026-09-08)

New row category for this round - the `ir_lowerer` stage's own
independent receiver-target resolver, beyond the one name-collision gate
Row D already covers. Entry point:
`resolveMethodCallDefinitionFromExpr`
(`IrLowererSetupTypeMethodCallResolution.cpp:377-1247`, ~870 lines - the
largest single cascade found in this whole investigation, larger than
monomorphization's 719-line file). It is the `ir_lowerer`-stage sibling of
Row A's `resolveArgsPackElementMethodTarget` and Row F's
`resolveMethodCallTemplateTarget`: given a method-call `Expr`, decide
which `Definition*` it lowers to. Two sibling files it calls into,
`IrLowererSetupTypeReceiverTargetHelpers.cpp` (778 lines -
`resolveMethodCallReceiverExpr`, `resolveMethodReceiverTarget`) and
`IrLowererSetupTypeCollectionHelpers.cpp` (1143 lines - the
`preferredFileErrorHelperTarget` family, `canonicalKeyValueHelperPath`,
`normalizeCollectionHelperPath`, the `isExplicit*AliasPath` predicates,
etc.), are cross-referenced by name below but **not** branch-enumerated
this round - left open per this round's own budget note at the end.

Branch order matters; this is a strict cascade, first successful
`return` wins, enumerated at the same major-branch granularity Row F used
(sub-helpers cited by name, not further expanded, matching Row F's
treatment of F7/F12):

| # | guard condition | disposition | pinned by |
|---|---|---|---|
| G0 | `callExpr.kind != Call \|\| callExpr.isBinding` | `return nullptr` immediately | — |
| G0b | `!callExpr.isMethodCall` (a direct-call, not a method-call) | `resolvedPath = resolveExprPath(callExpr)`; dispatch to `defMap[resolvedPath]` if found, else `nullptr` - a wholly separate, one-line path; nothing below this row ever runs for a direct call | — |
| G1 | the (possibly-prefix-stripped) method name normalizes to one of the 12 canonical key-value helper names (`count`/`count_ref`/`size`/`contains`/`contains_ref`/`tryAt`/`tryAt_ref`/`at`/`at_ref`/`at_unsafe`/`at_unsafe_ref`/`insert`/`insert_ref`) AND (the receiver's `LocalInfo` has key-value kinds OR `resolveCollectionPairTypeInfo(...).isKeyValueTarget`) | try `canonicalKeyValueHelperPath(helperName)` at matching arity via `resolveDefinitionFamilyByArity`; dispatch if found, else **fall through** (not a hard return) | UNPINNED to this branch specifically |
| G2 | `isExplicitKeyValueMethodAliasPath(explicitMethodPath)` (an explicit canonical map-helper spelling used directly) | if `semanticProgram` present and the receiver isn't itself a nested `Call`: try semantic-product method-call-target, then direct-call-target, then bridge-path-choice, in that order, dispatching via `resolveDefinitionFamilyByArity` on the first non-empty hit; **always terminates** the function from here - hard "unknown method" error and `return nullptr` if nothing resolved | UNPINNED |
| G3 | `semanticProgram != nullptr` (the semantic-product-driven path - the dominant path for non-legacy builds) | large sub-cascade, detailed below; almost always returns from inside this block | see G3a-G3e below |
| G3a | (within G3) `callExpr.semanticNodeId==0` AND (`sourceLine<=0 \|\| sourceColumn<=0 \|\| name.empty()`) AND (`args.empty() \|\| args.front().kind != Call`) | hard error "missing semantic-product method-call semantic id", `return nullptr` | — |
| G3b | `resolvedPath = findSemanticProductMethodCallTarget(...)`; `resolvedPath == "/std/collections/soa/to_aos"` | clear error, `return nullptr` (deliberate "no definition, not an error" sentinel) | UNPINNED |
| G3c | `resolvedPath` empty | chain of **four** semantic-product fallback lookups for `fallbackDirectTarget` (direct-call-target, then bridge-path-choice, then - only if that was empty - `findKeyValueConstructorBridgePathChoiceBySource`, a source-position-matched variant with no counterpart anywhere in Row E's R10-R14); see G3c-i..v below for what happens once that's found | — |
| G3c-i | `fallbackDirectTarget` non-empty AND call is literally `at`/`at_unsafe` AND `blocksSyntheticCollectionFallbackDirectTarget(fallbackDirectTarget)` | resolve and dispatch **only if** the resolved def is non-null AND has a non-empty body (`!statements.empty()`) - the one dispatch site in this whole function that checks body-emptiness, no counterpart elsewhere in this cascade | UNPINNED |
| G3c-ii | else: `fallbackDirectTarget` non-empty AND ((call is `count`/`capacity`/`at`/`at_unsafe` and not blocked) OR (call is literally `map` and the fallback is a key-value-constructor path)) | dispatch via `resolveLoweredDefinitionPath`; on failure, hard error "missing lowered definition", `return nullptr` | UNPINNED |
| G3c-iii | else: call's method leaf is a builtin File-handle name (`write`/`write_line`/`write_byte`/`read_byte`/`write_bytes`/`flush`/`close`) | clear error, `return nullptr` (deliberately silent - handled elsewhere, e.g. codegen-level file intrinsics) | UNPINNED |
| G3c-iv | else: NOT `allowsReceiverResolvedVectorMetadataFallback` AND receiver isn't a nested `Call` | hard error "missing semantic-product method-call target", `return nullptr` | UNPINNED |
| G3c-v | else (none of G3c-i..iv) | **falls through past the entire G3 block** to G4+ below - the one escape hatch where `semanticProgram != nullptr` doesn't force a decision from inside G3 | UNPINNED - the only path by which the "legacy" cascade (G4+) is reachable at all when `semanticProgram != nullptr` |
| G3d | `resolvedPath` non-empty (skips G3c entirely) | several count-method-specific sub-guards (`routesExplicitVectorCountMethodThroughMapMethodTarget`/`ThroughBuiltinScalarTarget`/`ThroughArgsPackCount`, each its own condition), then a general `resolveLoweredDefinitionPath(preferredResolvedPath)` attempt, then a further handful of "clear error, `return nullptr`" sentinels for `/file/*` paths, misfired `/string/count`, and misfired `soa/to_aos`, else a final hard "missing lowered definition" error | UNPINNED to each individual sub-guard |
| G3e | none of G3a-G3d returned (reachable only via internal fallthrough, distinct from G3c-v) | `if (errorOut.empty())` sets a final generic "missing lowered definition" error and returns `nullptr` | — |
| G4 | reached only when `semanticProgram == nullptr` OR G3c-v fired (the "legacy"/local cascade) | computes five independent builtin-classifier booleans up front (`isBuiltinAccessCall` via `getBuiltinArrayAccessName` - Row D's own function - plus count/capacity/mutator variants, each via its own helper), combined into `allowBuiltinFallback`, which is purely an **error-message-selection** flag (whether a later failure reports the original semantic-product-stage error or this cascade's own), not itself a dispatch decision | — |
| G5 | `resolveMethodCallReceiverExpr(...)` (own function, `IrLowererSetupTypeReceiverTargetHelpers.cpp` - not re-derived this round) extracts `receiver` | on failure: restore `priorError` if `allowBuiltinFallback`, `return nullptr` | — |
| G6 | `receiver->kind == Name` AND NOT found in `localsIn` | bare `FileError`/`ImageError`/`ContainerError`/`GfxError` static-error-family dispatch (mirrors Row F's F1/F11 and Row A's R2) - normalizes the method name by stripping one of 5 collection-prefix spellings first, then a 4-family table (`FileError`: `why`/`is_eof`/`eof`/`status`/`result` - **5** names, matching Row F's F1, not Row F's F11's 4-name set; `ImageError`/`ContainerError`/`GfxError`: `why`/`status`/`result` only, **no** `eof`/`is_eof`) dispatches to the matching `preferred*ErrorHelperTarget` (all four in `IrLowererSetupTypeCollectionHelpers.cpp`, cross-referenced not re-derived); if resolved and found in `defMap`, immediate return - otherwise **falls through** to G7 | UNPINNED to this exact branch |
| G7 | `resolveMethodReceiverTarget(*receiver, ...)` (own function, sibling file - not re-derived this round) sets `typeName`/`resolvedTypePath` | on failure: restore `priorError` if `allowBuiltinFallback`, `return nullptr` | — |
| G8 | `resolveMethodDefinitionFromReceiverTarget(explicitMethodPath, typeName, resolvedTypePath, defMap, lookupError)` (own function, cross-referenced not re-derived) | the "normal" dispatch attempt | — |
| G9 | G8 failed AND `resolvedTypePath.empty()` AND `receiver->kind == Call` | large receiver-is-itself-a-call recovery sub-cascade, G9a-G9f below | — |
| G9a | (within G9) resolve receiver's own path (`resolveExprPath`, else semantic-product direct-call-target, else bridge-path-choice, else the receiver's own literal name as a rooted path) to `receiverPath`; look up `receiverDef` via arity-aware `findDefinitionByReceiverPath` | — | — |
| G9b | `receiverDef` not found AND `receiver->isMethodCall` | **recurses into `resolveMethodCallDefinitionFromExpr` itself** on the receiver expression to get `receiverDef`; a further guard (`isExplicitVectorReceiverProbeHelperExpr` + non-empty nested error) can propagate that nested error out immediately instead of continuing | UNPINNED |
| G9c | `receiverDef` found | `inferStructReturnPathFromReceiverDef(*receiverDef)` (its own recursive struct-return-path inference, using `resolveStructTypePathFromScope` - **the function containing the `SoaVector__` special-case**, see below) yields `resolvedTypePath`; if non-empty, retry `resolveMethodDefinitionFromReceiverTarget` | UNPINNED |
| G9d | still unresolved; `inferReceiverTypeFromDeclaredReturn(*receiverDef, typeName)` succeeds | retry via `resolveMethodDefinitionFromTypeNameWithAliasFallback` (its own further alias-fallback: type name directly, then a `"vector"`-literal special-case retry via `collectionTypePath("vector")`, then an import-alias lookup) | UNPINNED |
| G9e | `receiverDef` found but G9d's declared-return check failed | `resolveReturnInfoKindForPath` (a **different**, value-kind-based inference mechanism) yields a `typeName`; retry via the same alias-fallback wrapper | UNPINNED |
| G9f | `receiverDef` unresolved entirely, or none of G9c-e produced a definition | a further receiver-kind-inference fallback: four independent "blocks this fallback" probe predicates (explicit key-value-receiver-probe, a bare 2-arg key-value-access-shaped-call probe, a bare 2-arg `tryAt` key-value probe, an explicit-vector-receiver-probe-kind blocker) gate whether `inferBuiltinAccessReceiverResultKind`/`inferExprKind` is even consulted; if none block it and a kind is inferred, retry via the alias-fallback wrapper; failing that, a receiver-path-candidate loop (`collectionHelperPathCandidates`) tries declared-return inference against each syntactic path variant of the receiver's resolved definition path | UNPINNED |
| G10 | still `resolvedDef == nullptr` after G9 | three "blocks the bare-vector fallback" checks (count/access/mutator-shaped call whose already-inferred `typeName == "vector"`) decide whether to restore `priorError` (when `allowBuiltinFallback` and none block it) or surface `lookupError` as the final error | — |

**The `SoaVector__`/specialization-suffix case, located (2026-09-08).**
This document's own "Problem, Verified" section names
`IrLowererSetupTypeCollectionHelpers.cpp` as (implicitly) where the
`SoaVector__`/specialization-suffix handling would live, alongside the
other two files. That is not where it actually is: it is entirely
contained in `IrLowererSetupTypeMethodCallResolution.cpp` itself -
`isExperimentalSoaVectorSpecializedStructPath` (lines 95-99, matching
three prefix shapes: the canonical specialized-type prefix, its bare
variant, and the literal string `"SoaVector__"`) and
`resolveSpecializedExperimentalSoaVectorStructPath` (lines 108-153, which
recursively unwraps `Reference<T>`/`Pointer<T>` envelopes one layer at a
time before checking, and once it reaches a bare `soa<T>` shape with
exactly one template arg, calls
`specializedExperimentalSoaVectorStructPathForElementType(T)` to build
the specialized struct path for `T`). It is consulted from
`resolveStructTypePathFromScope` (line 949, used by G9c above) as the
**first** check, before the normal `structNames`-scoped lookup, the
namespace-prefix walk, or the import-alias fallback - i.e. a receiver
whose inferred type recursively unwraps to a `soa<T>` shape is diverted
to this specialized-struct-path resolution before any of the other three
lookup strategies in that function ever run.
`IrLowererSetupTypeCollectionHelpers.cpp` (grepped in full this round) has
**no** `SoaVector__` handling at all - it contains the family-membership
predicates (`isBuiltinCollectionTypeName`, `isExperimentalCollectionTypeName`,
`normalizeCollectionHelperPath`, etc.) that this file's cascade calls
into, but the specialization-suffix case itself is unique to
`IrLowererSetupTypeMethodCallResolution.cpp`.

`IrLowererSetupTypeReceiverTargetHelpers.cpp`
(`resolveMethodCallReceiverExpr` at line 142,
`resolveMethodReceiverTarget` at line 527) and the remainder of
`IrLowererSetupTypeCollectionHelpers.cpp` (the `preferred*ErrorHelperTarget`
family at lines 227-393, the `isExplicit*AliasPath`/`isBuiltin*`
predicates at lines 573-810, the canonical-path builders) are **not**
branch-enumerated this round - left open per this round's own budget,
flagged again below.

### Row category G continued (I): `IrLowererSetupTypeReceiverTargetHelpers.cpp` (2026-09-08, fourth round)

778 lines, fully read this round. This is the sibling file Row G's G5
(`resolveMethodCallReceiverExpr`) and G7 (`resolveMethodReceiverTarget`)
call into; both are branch-enumerated below, along with every other
function in the file (all are called transitively from the G-cascade,
directly or as sub-helpers of G5/G7).

**`resolveMethodCallReceiverExpr` (G5's own function, lines 142-200).**
Extracts the receiver `Expr*` from a method-call `Expr`, or fails.

| # | guard condition | disposition | pinned by |
|---|---|---|---|
| RT1a | `callExpr.kind != Call \|\| callExpr.isBinding \|\| !callExpr.isMethodCall` | `return false` silently (no error text) | — |
| RT1b | `callExpr.args.empty()` | error "method call missing receiver", `return false` | — |
| RT1c | computes a **local, function-scoped** `allowBuiltinFallback` boolean from five independent classifier calls (builtin array-access, unqualified `count`/`capacity`, bare vector-capacity-method, unqualified vector-mutator names `push`/`pop`/`reserve`/`clear`/`remove_at`/`remove_swap`, unqualified key-value `contains`/`tryAt`/`insert`, plus the three injected classifier callbacks `isArrayCountCall`/`isVectorCapacityCall`/`isEntryArgsName`) AND-gated on **not** being one of three "explicit alias path" shapes (`isExplicitRemovedVectorMethodAliasPath`, `isExplicitKeyValueMethodAliasPath`, `isExplicitKeyValueContainsOrTryAtMethodPath`) or the bare-vector-capacity-method shape | — (feeds RT1d) | — |
| RT1d | `isEntryArgsName(receiver, localsIn)` (an args-pack-`Entry`-shaped receiver, via injected callback) | if `allowBuiltinFallback`: silent `return false`; else: error "unknown method target for `<scopedPath>`", `return false` | UNPINNED |
| RT1e | none of the above | `receiverOut = &receiver`, `return true` | — |

**Cross-stage note:** this function computes its own `allowBuiltinFallback`
with a formula distinct from (though overlapping with) the main file's
G4 `allowBuiltinFallback` — same *name*, different scope, different
formula, different purpose (G4 gates error-message selection at the end
of the whole cascade; RT1c/RT1d gates only whether a bare-`Entry`
receiver silently defers vs. hard-errors, early in the cascade). Two
independently-computed booleans sharing a name across sibling files in
the same cascade is exactly the kind of naming collision this
consolidation effort is trying to surface — flagged as a
characterization finding, not a bug (the two never interact directly).

**`resolveMethodReceiverTypeFromLocalInfo` (lines 202-289).** Given a
bound local's `LocalInfo`, decides `typeNameOut`/`resolvedTypePathOut`.
Strict if/return cascade:

| # | guard condition | disposition |
|---|---|---|
| RT2a | `localInfo.isFileHandle` | `typeNameOut = "File"` |
| RT2b | `!localInfo.structTypeName.empty()` | `resolvedTypePathOut = structTypeName` |
| RT2c | `!localInfo.errorHelperNamespacePath.empty()` | `resolvedTypePathOut = errorHelperNamespacePath` |
| RT2d | `!localInfo.errorTypeName.empty()` | `typeNameOut = errorTypeName` |
| RT2e | `kind == Array` | `typeNameOut = "array"` |
| RT2f | `isSoaVector` | `typeNameOut = "soa"` |
| RT2g | `kind == Vector` | `typeNameOut = "vector"` |
| RT2h | `kind == Value` AND `hasKeyValueKinds` | `typeNameOut = "map"` |
| RT2i | `kind == Buffer` | `typeNameOut = "Buffer"` |
| RT2j | `kind == Reference` AND (`referenceToArray \|\| referenceToVector \|\| hasKeyValueKinds \|\| referenceToBuffer`) | if `structTypeName` non-empty, `resolvedTypePathOut = structTypeName`; else `typeNameOut` = `"map"`/`"soa"` or `"vector"`/`"Buffer"`/`"array"` by sub-priority (key-value first, then vector, then buffer, then array) |
| RT2k | `kind == Pointer && pointerToArray` | `typeNameOut = "array"` |
| RT2l | `kind == Pointer && pointerToVector` | `typeNameOut = "soa"`/`"vector"` |
| RT2m | `kind == Pointer` AND `hasKeyValueKinds` | `typeNameOut = "map"` |
| RT2n | `kind == Pointer && pointerToBuffer` | `typeNameOut = "Buffer"` |
| RT2o | `kind == Reference && !structTypeName.empty()` | `resolvedTypePathOut = structTypeName` |
| RT2p | `kind == Pointer \|\| kind == Reference` (none of the above matched) | `return false` |
| RT2q | `kind == Value && !structTypeName.empty()` | `resolvedTypePathOut = structTypeName` |
| RT2r | fallback | `typeNameOut = typeNameForValueKind(valueKind)`, `return true` unconditionally |

**Finding (new, this round): RT2o and RT2q are dead code.** RT2b, at the
very top of the function, already unconditionally returns as soon as
`structTypeName` is non-empty — for *any* `LocalInfo::Kind*`, not just
`Reference`/`Value`. So by the time execution could reach RT2o or RT2q,
`structTypeName` is guaranteed empty (RT2b would already have returned
otherwise), making both branches' guard conditions (`!structTypeName.empty()`)
unreachable in practice. This reads as historical residue from a
refactor that hoisted the `structTypeName` check to the top without
removing its now-redundant copies further down — harmless (dead code,
not a behavior bug) but worth noting as exactly the kind of drift that
accumulates when the same field is checked independently in multiple
places within one cascade, which is the broader pattern this whole
consolidation effort is about. UNPINNED either way, since the branches
never execute.

**`resolveMethodReceiverTypeNameFromCallExpr` (lines 291-324).** Given a
receiver that is itself a `Call`, and an already-inferred
`LocalInfo::ValueKind`, classifies collection-constructor-shaped calls:
Buffer-constructor-by-path check first (`Buffer`/4 rooted spellings, 1
template arg), then `getBuiltinCollectionName` dispatch for
`array`/`vector` (1 template arg), `map` (2 template args), `Buffer` (1
template arg, a **second**, independent Buffer-detection path alongside
the first), `soa` (1 template arg); falls back to
`typeNameForValueKind(inferredKind)` if none match. Two independently
coded Buffer-detection paths in the same 34-line function (path-string
match vs. `getBuiltinCollectionName` classification) is a minor internal
duplication, not cross-stage — noted for completeness, not flagged as a
priority finding.

**`inferBuiltinAccessReceiverResultKind` (lines 326-445).** Infers the
*element* `ValueKind` of a builtin collection-access call
(`array[i]`/`at(...)`-shaped), used to seed `inferExprKind` results
upstream. Guarded first by four independent "block this inference"
predicates (explicit key-value method-alias path, explicit key-value
contains/tryAt path, `isExplicitKeyValueHelperFallbackPath` — **always
false**, see the CollectionHelpers finding below —, and
`blocksExplicitVectorReceiverProbeKindFallbackExpr`); then requires the
call itself to be a 2-arg builtin-access or literal `at` call. From
there: `Name`-kind access-receiver branches on `inferExprKind` (String
→ `Int32`, i.e. character access), then `LocalInfo` lookup (vector/array
family → element `valueKind`; key-value family → `keyValueValueKind`;
plain `String`-kind `Value` local with **no** inferred kind → `Int32`);
`Call`-kind access-receiver branches on `getBuiltinCollectionName`
(`string`→`Int32`; `vector`/`array`/1-arg → element kind; `map`/2-arg →
value-position kind), then a second, independent `inferExprKind`
String-check, then `defMap`-lookup-driven declared-return-collection
inference, then `getReturnInfo`-driven inference as a final fallback
(String → `Int32` again, a **third** occurrence of the same
String-receiver-means-character-access rule inside this one function).

**`isSoaVectorReceiverExpr` (lines 447-483).** A **fourth** independent
SOA-family-membership predicate in this investigation (alongside Row
B's semantics-stage SOA checks, F12/F14's `isTemplateMonomorphSoaReceiverType`
in monomorphization, and `normalizeCollectionReceiverTypeName`'s SOA
branch also in monomorphization) — but unlike those, this one operates
on receiver **Expr shape**, not a resolved type-name string: bound
`Name` local with `isSoaVector` set; `getBuiltinCollectionName(...) ==
"soa"` constructor call; or a `dereference(...)`-wrapped local/args-pack-element
that is itself SOA. No shared implementation with any of the
type-name-string-based SOA checks elsewhere — this is Expr-shape
classification feeding *into* the type-name-string classification the
other three do, so not a direct behavioral duplication, but one more
independently-coded predicate that answers the same underlying "is this
a SOA vector" question.

**`resolveMethodReceiverTypeFromNameExpr` (lines 485-525).** For a bare
`Name`-kind receiver not found in `localsIn`: unconditionally matches
literal receiver spelling `FileError`/`ImageError`/`ContainerError`/`GfxError`
and sets a hardcoded type/path (`FileError`→`("FileError",
"/std/file/FileError")`, `ImageError`→`("ImageError",
"/std/image/ImageError")`, `ContainerError`→`("ContainerError",
"/std/collections/ContainerError")`, `GfxError`→`("GfxError", "")` — the
one family with **no** `resolvedTypePathOut`, left for later
disambiguation between canonical and experimental namespaces, matching
Row F's F11 in-source-documented `GfxError` dual-namespace history).

**Cross-reference/finding (new, this round): unconditional vs.
method-name-gated bare-error-Name handling.** The main cascade's G6
(in `IrLowererSetupTypeMethodCallResolution.cpp`) *also* handles a bare
`Name`-kind receiver not found in `localsIn` spelled one of these four
names — but G6 gates dispatch on the method name matching one of
`why`/`is_eof`/`eof`/`status`/`result` (`FileError`) or
`why`/`status`/`result` (the other three) via the `preferred*ErrorHelperTarget`
family, and only *falls through* to G7 (which reaches this function) if
that gate fails to find a matching `defMap` entry. This function, reached
from G7, has **no method-name gate at all** — it unconditionally treats
*any* method called on a bare `FileError`/`ImageError`/`ContainerError`/`GfxError`-spelled
receiver as that error type, regardless of the method name, then lets
`resolveMethodDefinitionFromReceiverTarget` (G8) fail on an unknown
method separately. Net effect is probably the same final outcome (an
"unknown method" error either way), but the *error text and code path*
differ depending on whether G6's method-name gate matched: this is a
second, differently-scoped reimplementation of "is this receiver one of
the four bare error-family names" within the same G5→G10 cascade, not
previously documented. UNPINNED; not confirmed whether the two paths'
differing error messages are independently tested.

**`resolveMethodReceiverTarget` (G7's own function, lines 527-776).**
The largest function in the file — the receiver-type-inference dispatcher
matching monomorphization's F3 in role (feeds `typeName`/`resolvedTypePathOut`
for the rest of the G-cascade). Top-level dispatch on `receiverExpr.kind`:

| # | guard condition | disposition |
|---|---|---|
| RT3a | `kind == Name` | try `resolveMethodReceiverTypeFromNameExpr` (bound-local path via `resolveMethodReceiverTypeFromLocalInfo`, or the bare-error-Name fallback above); on failure, try `resolveStructTypePathFromName` (an unrelated struct-type-name-by-namespace-walk lookup, lines 28-74 of this file, with its own import-alias fallback distinct from `resolveMethodReceiverTypeFromNameExpr`'s hardcoded 4-family table); `return false` only if both fail |
| RT3b | `kind == Call` | large sub-cascade, RT3b-i..vii below |
| RT3c | neither `Name` nor `Call` | `typeNameOut = typeNameForValueKind(inferExprKind(receiverExpr, localsIn))`, `return true` unconditionally (never fails) |

RT3b (`kind == Call`) sub-cascade, first match wins within each block
but several blocks can each independently set output and `return true`
without falling through to later ones:

| # | guard condition | disposition |
|---|---|---|
| RT3b-i | builtin-access/`at`-shaped 2-arg call whose first arg is a `Name` bound to an args-pack local | large table of `argsPackElementKind`-driven type assignments (mirrors RT2's `LocalInfo`-kind cascade but operating on `argsPackElementKind` instead of `kind`, and re-deriving FileError/File/struct/vector/array/soa/map/Buffer membership from scratch rather than delegating to RT2 — a **fifth** independent re-derivation of "what type family does this LocalInfo represent", this time args-pack-scoped) — if none of its ~13 sub-checks match, falls through to RT3b-ii |
| RT3b-ii | `dereference(...)`-wrapped call, 1 arg | delegates to a local lambda (`resolveDereferencedCollectionOrFileErrorReceiver`) that re-derives the **same** family classification a **sixth** time, this time keyed off `receiverKind` (which is `argsPackElementKind` or plain `kind` depending on whether the dereferenced target is itself an args-pack local) — covers `isFileError`/`isFileHandle`/array/vector(soa-aware)/map(both `Value`-kind-with-KV and Reference/Pointer-with-KV shapes)/Buffer |
| RT3b-iii | bare key-value-access-shaped probe (`isBareKeyValueAccessReceiverProbeExpr`, a local lambda checking `resolveCollectionPairTypeInfo(...).isKeyValueTarget` on the access call's first arg) | `typeNameOut = "map"`, `return true` |
| RT3b-iv | none of the above matched; a `blocks*` predicate quartet (explicit-key-value-receiver-probe, bare-key-value-access-probe [same lambda as RT3b-iii, re-evaluated], bare-key-value-tryAt-probe, explicit-vector-receiver-probe) gates whether `inferExprKind` is even consulted | if not blocked, `inferredKind = inferExprKind(receiverExpr, localsIn)`; `typeNameOut = resolveMethodReceiverTypeNameFromCallExpr(receiverExpr, inferredKind, resolveExprPath)` |
| RT3b-v | `typeNameOut` still empty after RT3b-iv, none of the four blockers fired, AND `receiverExpr.isMethodCall && args.size()==2` | a **String-receiver-means-character-access** check (String inferredKind on the access call's own first arg) → `typeNameOut = "i32"` — a **fourth** occurrence of this same rule within this pair of functions (the other three are inside `inferBuiltinAccessReceiverResultKind` above) |
| RT3b-vi | `typeNameOut` still empty | `resolvedTypePathOut = resolveMethodReceiverStructTypePathFromCallExpr(...)` (own function, cross-referenced not re-derived — declared in the header this file includes but not defined in either file read this round; likely lives in `IrLowererSetupTypeHelpers.cpp` or a sibling not in this round's scope) |
| RT3b-vii | (always) | `return true` — this whole `Call`-kind branch, like RT3c, **never returns false**; the only failure exit for the entire `resolveMethodReceiverTarget` function is RT3a's `Name`-kind path |

**Divergence tally for this file:** the "what type family does this
receiver represent" question is independently re-derived **at least six
times** across this file alone (RT2's `LocalInfo`-kind cascade, RT3b-i's
`argsPackElementKind`-keyed cascade, RT3b-ii's `dereference`-wrapped
lambda, plus the three String-means-character-access occurrences spread
across `inferBuiltinAccessReceiverResultKind` and RT3b-v) — on top of
the file's own `isSoaVectorReceiverExpr` and the main cascade's RT2
already noted above, and *on top of* the file's `resolveMethodReceiverTypeFromLocalInfo`
being conceptually the same question Row F's F3-N1 answers for
monomorphization and Row B/C answer for semantics. This file alone is a
microcosm of the whole consolidation problem, not just a contributor to
it.

### Row category G continued (II): `IrLowererSetupTypeCollectionHelpers.cpp` (2026-09-08, fourth round)

1143 lines, fully read this round (all of it, correcting Row G's earlier
note that only the `SoaVector__` search had been done). This file holds
path-normalization primitives, the `preferred*ErrorHelperTarget` family
already cross-referenced from G6, and the `isExplicit*AliasPath`
predicate family cross-referenced from G1/G2/RT1c/RT3b-iv. It has **no**
`SoaVector__` handling anywhere (confirmed by this round's full read,
consistent with Row G's earlier finding that the specialization case
lives entirely in `IrLowererSetupTypeMethodCallResolution.cpp` instead).

**Headline finding (new, this round): the key-value-alias-name resolver
is a permanent no-op stub, asymmetric with its vector counterpart.**
`resolveKeyValueHelperAliasName` (lines 483-487) and
`resolveBorrowedKeyValueHelperAliasName` (lines 489-493) are both:

```cpp
bool resolveKeyValueHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  (void)expr;
  helperNameOut.clear();
  return false;
}
```

— unconditionally `return false` for *every* input, with the parameter
explicitly void-cast (reads as a deliberate stub, not an accidental
omission — there is no dead branch above it, it is the entire function
body). Its vector-family counterpart, `resolveVectorHelperAliasName`
(lines 404-481), is fully implemented: it resolves array/vector/SOA
(canonical, internal, experimental namespaces)/experimental-vector alias
paths through the stdlib surface registry, with per-namespace helper-name
remapping (e.g. `soaVectorCount`→`count`). This is a live, not latent,
asymmetry: `resolveKeyValueHelperAliasName`'s permanent-`false` return
propagates directly into:
- `isExplicitKeyValueHelperFallbackPath` (line 756) — **always returns
  false**, since its only path to `true` requires
  `resolveKeyValueHelperAliasName` to succeed. This function is one of
  the four "block this inference" guards gating
  `inferBuiltinAccessReceiverResultKind` (Row G continued (I) above) —
  it can never actually block anything, making that guard dead weight
  in practice (always evaluates to "not blocked by this predicate").
- `isExplicitKeyValueReceiverProbeHelperExpr` (line 770) — same stub
  dependency, **always returns false**; this is one of RT3b-iv's four
  `blocks*` predicates in `resolveMethodReceiverTarget` above, so it too
  can never actually block that inference path.
- `getNamespacedCollectionHelperName` (line 1124) — tries
  `resolveVectorHelperAliasName` first (works), then
  `resolveKeyValueHelperAliasName` as the map-family fallback (always
  fails) — so this function can classify vector-family helper paths but
  can **never** classify a key-value/map-family helper path by this
  route, silently. Not confirmed this round whether any live caller
  actually depends on this function's map-family branch (it is declared
  in the header but no call site was found in either of this round's two
  files); flagged as UNPINNED and worth a grep in a future round before
  assuming it's inert everywhere.

Whether this asymmetry is intentional (key-value aliasing routed through
a wholly different mechanism elsewhere, making these two functions
deliberately vestigial) or a genuine gap was not determined this round —
no comment in-source explains it, unlike the `resolveVectorHelperAliasName`/`isRemovedVectorCompatibilityHelper`
pairing nearby, which *does* carry an explanatory comment pointing at
`CollectionSpellingClassifier.h`. Framed here purely as a Step 0
characterization finding, per this document's non-goal of not fixing
anything found.

**The rest of the file, branch-enumerated:**

| function | lines | role | branch summary |
|---|---|---|---|
| `stripGeneratedHelperSuffix` | 19-25 | strip a `__`-suffixed generated-name tail | single `find`/`erase`, no branching beyond the `npos` check |
| `matchesRegistrySpellingSet` | 27-33 | membership test against a stdlib-registry spelling list | `std::any_of` wrapper, no independent logic |
| `normalizePublishedCollectionPath`/`stripCollectionConstructorPathSuffix` | 35-53 | path canonicalization for constructor-family lookups | leading-slash insertion gated on 3 root-prefix checks (`std/collections/`, key-value alias root, `vector/`); suffix-stripping keyed off the leaf segment (last `/`) rather than the whole string, unlike `normalizeCollectionHelperPath`'s file-scope twin (line 643) which does the same suffix-strip differently-scoped — a **second**, structurally distinct implementation of "strip the generated-suffix tail from a path", noted but not flagged as high-priority (both agree on outcome for the shapes this round's read could verify by inspection) |
| `findPublishedStdlibSurfaceMetadata`/`findCollectionSurfaceMetadataByCanonicalPath`/`findCollectionConstructorSurfaceMetadataForHelper` | 55-98 | stdlib-surface-registry metadata lookup by spelling/canonical-path/domain+shape | straight-line registry queries, no cross-stage relevance found |
| `matchesResolvedRootedPublishedCollectionMemberPath` | 100-114 | "is this path `<root>/<one-segment-member>`" check | 4-condition guard (non-empty root, longer than root, root-prefix match, `/`-boundary at the root) then single-segment-membership test |
| `rebuildScopedCollectionHelperPath` | 116-128 | rebuild a fully-scoped path from `expr.namespacePrefix`+`expr.name`, only when `expr.name` has no `/` already | feeds `normalizeCollectionHelperPath` |
| `keyValueHelperSurfaceId`/`keyValueConstructorSurfaceId`/`isKeyValueHelperSurfaceId`/`resolveKeyValueSurfaceMemberToken` | 130-159 | registry-backed key-value surface-id/member-token resolution (the **working** key-value path, distinct from the stubbed `resolveKeyValueHelperAliasName` above) | thin wrappers; `resolveKeyValueSurfaceMemberToken` is what `isExplicitKeyValueMethodAliasPath`/`isExplicitKeyValueContainsOrTryAtMethodPath` actually use — i.e. explicit-path key-value classification (G1/G2/RT1c) is registry-driven and works; it is specifically the **alias-name-from-Expr** path (the stubbed functions) that is dead, not key-value classification generally |
| `isBorrowedKeyValueHelperSurface` | 161-173 | `_ref`-suffixed borrowed-helper detection via the registry | works (registry-backed), feeds `isExplicitKeyValueHelperFallbackPath`'s early-out — note this means the *stub* dependency is the reason that function is dead, not this borrowed-check, which is itself fine |
| `vectorHelperSurfaceId`/`resolveVectorSurfaceMemberToken`/`resolveVectorSurfaceExprMemberName` | 175-198 | registry-backed vector-family equivalents of the key-value ones above | fully working, used throughout Row G continued (I) |
| `vectorHelperSurfaceMetadata`/`keyValueHelperSurfaceMetadata`/`keyValueConstructorSurfaceMetadata` | 202-219 | public accessors for the above, keyed by canonical collection type path (`/std/collections/vector`, `/std/collections/map`) and shape (`HelperFamily`/`ConstructorFamily`) | straight delegation |
| `allowsArrayVectorCompatibilitySuffix` | 221-225 | 9-name blocklist (`count`/`capacity`/`at`/`at_unsafe`/`push`/`pop`/`reserve`/`clear`/`remove_at`/`remove_swap`) | gates `collectionHelperPathCandidates`'s array→vector compat-candidate generation below |
| `preferredFileErrorHelperTarget`/`preferredImageErrorHelperTarget`/`preferredContainerErrorHelperTarget`/`preferredGfxErrorHelperTarget` | 227-392 | already cross-referenced from Row G's G6; fully read this round | confirms Row G's characterization: `FileError` has 5 helper names (`why`/`is_eof`/`eof`/`status`/`result`), each independently probing 2-3 candidate `defMap` paths in a fixed preference order (namespaced-canonical, then bare, then — for `is_eof`/`eof`/`status`/`result` only, not `why` — a legacy free-function fallback path e.g. `/std/file/fileReadEof`); `ImageError`/`ContainerError` both have exactly 3 names (`why`/`status`/`result`, no `eof`/`is_eof`), each with the same namespaced/bare/legacy-free-function 3-tier preference (mirroring `FileError`'s pattern but with a smaller name set, matching Row G's G6 finding of the 5-vs-3 asymmetry at the call site); `GfxError` is structurally different — no name-specific branching at all, instead a single generic `helperForBasePath` used across an early exact-`resolvedTypePath`-match fast path and a `hasCanonical`/`hasExperimental` existence-based tiebreak, with **no legacy free-function fallback tier** unlike the other three families |
| `isRemovedVectorCompatibilityHelper` | 394-402 | delegates to the shared `CollectionSpellingClassifier` (per its own comment, decision D2's authoritative removed-name set) | comment explicitly frames this as the "other registry-membership variant" already reconciled by a D2 agreement unit test — cross-referenced, not re-derived |
| `resolveVectorHelperAliasName` | 404-481 | full branch table below | see RT-adjacent finding above; this is the real, working half of the vector/key-value asymmetry |
| `stdCollectionsRoot`/`collectionTypePath`/`collectionMemberRoot`/`collectionMemberPath`/`canonicalKeyValueHelperPath`/`canonicalKeyValueConstructorPath`/`experimentalCollectionMemberRoot`/`experimentalCollectionTypePath`/`collectionWrapperAlias`/`keyValueCollectionAliasRoot` | 495-571 | canonical-path builders, all straight-line string concatenation with a `leadingSlash` toggle; `canonicalKeyValueHelperPath`/`canonicalKeyValueConstructorPath` fall back to a hardcoded `/std/collections/map`-rooted path if the registry metadata lookup fails (`metadata == nullptr`) | no branching of note; hardcoded-fallback-when-registry-lookup-fails is the one pattern worth flagging as a general theme across this file — the registry is treated as authoritative but every consumer has its own hardcoded escape hatch for when it isn't found |
| `isBuiltinCollectionTypeName`/`isExperimentalCollectionTypeName` | 573-602 | type-name-string family-membership tests (bare name, rooted name, canonical path, rooted-canonical path, and `<name` template-prefix variants for each) | a **seventh** independent "is this type name X family" predicate pair in this investigation, this time string-shape-based rather than `LocalInfo`/`Expr`-based like the ones in Row G continued (I) |
| `keyValueStorageStructRootPath`/`isKeyValueStorageStructPath` | 604-622 | key-value backing-struct-path identity check (canonical + experimental root, `__`-suffixed generated variants) | registry-backed for the canonical root, hardcoded for the experimental one (`experimentalCollectionTypePath("map", "Map")`) — same registry/hardcoded-escape-hatch pattern as above |
| `normalizeBuiltinCollectionStructPath`/`normalizeExperimentalCollectionTypePath` | 624-641 | path normalization, single-purpose | straight-line |
| `normalizeCollectionHelperPath` | 643-664 | the file-scope suffix-stripping/leading-slash-insertion primitive used throughout both files (declared as a free function outside the anonymous namespace, at line 15, so it is genuinely shared internal-file surface) | leading-slash insertion gated on 6 root-prefix checks (broader than `normalizePublishedCollectionPath`'s 3, see the duplication note above); suffix strip is leaf-scoped like the other copy |
| `isExplicitRemovedVectorMethodAliasPath` | 666-690 | explicit-path-shape removed-vector-helper check (feeds RT1c/RT3b's `allowBuiltinFallback`/`blocksExplicitVectorReceiverProbeKindFallbackExpr`) | 3-prefix dispatch (`array/`, canonical vector root, experimental vector root), each delegating to `isRemovedVectorCompatibilityHelper` or the registry token resolver |
| `isExplicitKeyValueMethodAliasPath`/`isExplicitKeyValueContainsOrTryAtMethodPath` | 692-744 | already cross-referenced from Row G's G1/G2; fully read this round | both 2-prefix dispatch (key-value alias root, canonical map root), both registry-token-resolver-backed (the *working* key-value path, per the note above) — `isExplicitKeyValueMethodAliasPath` gates on `count`/`at`/`at_unsafe`/`insert`, `isExplicitKeyValueContainsOrTryAtMethodPath` on `contains`/`tryAt`; together they partition the same 6-name set G1's cascade uses (`count`/`contains`/`tryAt`/`at`/`at_unsafe`/`insert`), but `count_ref`/`at_ref`/`at_unsafe_ref`/`insert_ref`/`contains_ref`/`tryAt_ref` (the `_ref` borrowed variants G1's own guard condition lists) are **not** matched by either of these two path-shape checks — an explicit-path receiver spelled with a `_ref` suffix takes neither of these routes, a narrower gap than G1's own guard but not previously called out at this granularity |
| `isUnqualifiedCollectionBuiltinName` | 746-754 | exact-name, no-namespace-prefix, no-`/`-in-name unqualified-builtin-call check | used by RT1c above |
| `isExplicitKeyValueHelperFallbackPath`/`isExplicitKeyValueReceiverProbeHelperExpr` | 756-779 | **dead per the headline finding above** | both always `false` |
| `isExplicitVectorAccessHelperPath`/`isExplicitVectorAccessHelperExpr`/`isExplicitVectorReceiverProbeHelperExpr`/`blocksExplicitVectorReceiverProbeKindFallbackExpr` | 781-829 | vector-family counterparts of the above, fully working (registry-backed) | `isExplicitVectorAccessHelperPath` checks only `at`/`at_unsafe`; `isExplicitVectorReceiverProbeHelperExpr` checks the broader `at`/`at_unsafe`/`count`/`capacity` (4 names); `blocksExplicitVectorReceiverProbeKindFallbackExpr` further gates on `expr.isMethodCall` (blocks unconditionally) vs. explicit-path spelling (delegates to `isExplicitRemovedVectorMethodAliasPath` on the reconstructed scoped path) — used directly by RT3b-iv/RT1c above |
| `isAllowedResolvedMapDirectCallPath`/`isAllowedResolvedVectorDirectCallPath` | 831-853 | "does this resolved definition path match one of the call path's allowed candidates" checks, used to validate a direct-call dispatch didn't drift onto an unrelated family's helper | both delegate to `collectionHelperPathCandidates`; the map variant only applies its restriction when `isExplicitKeyValueMethodAliasPath` already matched (else permissive `true`), the vector variant only applies when the call path is already vector-rooted (else permissive `true`) — same shape, independently coded per family, not shared |
| `resolvePublishedStdlibSurfaceMemberToken`/`resolvePublishedStdlibSurfaceExprMemberName`/`resolvePublishedStdlibSurfaceConstructorMemberName`/`resolvePublishedStdlibSurfaceConstructorExprMemberName`/`isResolvedCanonicalPublishedStdlibSurfaceConstructorPath`/`isPublishedStdlibSurfaceConstructorExpr` | 855-991 | generic (family-agnostic, parameterized by `StdlibSurfaceId`) registry-backed member/constructor-name resolution — the shared mechanism `canonicalKeyValueHelperPath` et al. and the vector-family resolvers both build on | this is the one place in the file where vector and key-value families **do** share one real implementation, parameterized by surface id, rather than being independently reimplemented — worth noting as a partial counterexample to the "everything duplicated" pattern, i.e. this file demonstrates both ends: some primitives genuinely shared (this block), others independently duplicated per family (most of the rest), and one stubbed out entirely (key-value alias-name resolution) |
| `inferPublishedKeyValueStorageStructPathFromConstructorPath` | 993-1022 | constructor-path → backing-storage-struct-path inference, key-value-specific | single-purpose, registry-backed |
| `resolvePublishedSemanticStdlibSurfaceMemberName` | 1024-1054 | semantic-product-driven member-name resolution — tries bridge-path-choice, then method-call-target, then direct-call-target, in that order, each gated on the semantic-product's own recorded `StdlibSurfaceId` matching the requested one | **cross-stage note:** this is the same three-tier semantic-product lookup order (bridge-path-choice → method-call-target → direct-call-target) Row G's G2 and G3c both use independently for their own fallback chains — a fourth site in this investigation reimplementing that same three-way (or four-way, counting G3c's extra `findKeyValueConstructorBridgePathChoiceBySource` tier) semantic-product consultation order, though this one is gated per-surface-id rather than being method-name-driven like G2/G3c |
| `resolvePublishedStdlibSurfaceMemberName`/`isPublishedStdlibSurfaceLoweringPath`/`isCanonicalPublishedStdlibSurfaceHelperPath` | 1056-1089 | further registry-backed path/spelling classification | straight delegation |
| `normalizeMapImportAliasPath` | 1091-1093 | **identity function** — `return path;` unconditionally, no transformation at all | dead-looking in a different way from the key-value alias-name stubs above (this one is a genuine identity no-op, not a permanent-`false`); not confirmed this round whether any caller relies on this being a hook point for future logic vs. being vestigial itself — worth a name-search in a future round |
| `collectionHelperPathCandidates` | 1095-1122 | builds the candidate-path list `isAllowedResolvedMapDirectCallPath`/`isAllowedResolvedVectorDirectCallPath` (and `isAllowedResolvedMapDirectCallPath`'s R-row analogues elsewhere) validate against | starts with `{path, normalizedPath}`, then — only for paths rooted at `/array/` and only when `allowsArrayVectorCompatibilitySuffix` allows the suffix — appends the registry's canonical vector-helper path for that suffix as a third candidate; no equivalent map-rooted candidate-expansion branch exists in this function at all (asymmetric with the array→vector expansion, though not clearly a gap since map has no compatibility-alias-root the way array/vector do) |
| `getNamespacedCollectionHelperName` | 1124-1141 | already covered under the headline finding — vector branch works, map branch dead |

**`resolveVectorHelperAliasName` (lines 404-481), branch table** (the
function whose map-family sibling is the stubbed-out
`resolveKeyValueHelperAliasName`):

| # | guard condition | disposition |
|---|---|---|
| CH-V1 | `expr.name.empty()` | `return false` |
| CH-V2 | rebuilt scoped path, stripped of leading `/`, starts with `vector/` | `return false` (this is the *canonical, unprefixed* vector root — deliberately excluded from the "alias" concept; presumably handled elsewhere as the non-alias case) |
| CH-V3 | `resolveVectorSurfaceExprMemberName` succeeds (registry-backed) | `return true` immediately, before any of the hardcoded prefix checks below run |
| CH-V4 | normalized path starts with `array/` | strip suffix, reject if `isRemovedVectorCompatibilityHelper`, else accept |
| CH-V5 | starts with the canonical `std/collections/vector/` member root | strip suffix, accept unconditionally (no removed-name check here, asymmetric with CH-V4's array-prefix branch) |
| CH-V6 | starts with `std/collections/soa/` | strip suffix, remap `soaVectorCount`/`soaVectorCountRef`→`count`/`count_ref`, accept only if the resulting name is one of `count`/`count_ref`/`get`/`get_ref`/`ref`/`ref_ref` |
| CH-V7 | starts with the experimental-SOA-vector module prefix | strip suffix, remap the same two `soaVectorCount*` names, accept **only** those two (narrower than CH-V6 — `get`/`ref` variants are not accepted from the experimental-SOA prefix at all) |
| CH-V8 | starts with the internal-SOA-vector module prefix | same as CH-V7 (narrower two-name acceptance, structurally identical block, independently coded) |
| CH-V9 | starts with the experimental-vector member root | delegates back to `resolveVectorSurfaceExprMemberName` (registry) |
| CH-V10 | none matched | `return false` |

CH-V4 vs. CH-V5's asymmetry (array-prefix rejects removed-compat names,
canonical-vector-prefix does not) and CH-V6 vs. CH-V7/CH-V8's asymmetry
(canonical-SOA-prefix accepts 6 names, both non-canonical SOA prefixes
accept only 2) are both new findings this round — neither documented
in-source, neither confirmed live-vs-latent without a reachability check
this round's budget didn't extend to.

### What remains for Step 0

Done as of this round (2026-09-08, third round): F3 within Row category F
(the receiver-type-inference sub-cascade feeding `typeName` into
`resolveMethodCallTemplateTarget`) now has full branch-level enumeration
(F3-N1/N2, F3-L/B/Fl/S, F3-C1-C3d), surfacing two new override-priority
gaps not previously documented (F3-C3b/C3d can silently overwrite an
already-computed `typeName` from F3-C1/C2 with no documented priority
rule) and one field-asymmetry (F3-C2 updates `typeName`/
`isBorrowedSoaReceiver` but not `wrappedReceiverTypeName`, which matters
for F6's wrapper-path branch). Row category E gained R14 (`query_facts`),
with an important correction to this document's own 2026-09-04 framing:
`query_facts` is not a sixth independent receiver-family re-derivation at
the `resolvedPath` level - it reuses R11's `inferCallSnapshotData`
directly - though its `typeText`/`resultInfo`/`receiverBinding` layer on
top (Q1-Q6) is genuinely independent, including two separately-coded
Result-type inference paths (Q3/Q3b) and a new, unresolved question about
whether its own local-aware traversal is exempt from the
`skipLocalAwareCallRefinement_` pilot-routing guard that R11 respects. A
new Row category G was established for the `ir_lowerer` stage
(`resolveMethodCallDefinitionFromExpr`, ~870 lines, the largest cascade
found in this investigation, `IrLowererSetupTypeMethodCallResolution.cpp`)
and fully branch-enumerated at the same major-branch granularity Row F
used, including locating the `SoaVector__`/specialization-suffix case
this document's own "Problem, Verified" section named but hadn't located
- it turned out to live in this file, not
`IrLowererSetupTypeCollectionHelpers.cpp` as that section's phrasing
implied.

Done as of this round (2026-09-08, fourth round): both files Row G's
third round left open are now fully read and branch-enumerated -
`IrLowererSetupTypeReceiverTargetHelpers.cpp` (778 lines - Row G
continued (I): `resolveMethodCallReceiverExpr`/G5,
`resolveMethodReceiverTypeFromLocalInfo`,
`resolveMethodReceiverTypeNameFromCallExpr`,
`inferBuiltinAccessReceiverResultKind`, `isSoaVectorReceiverExpr`,
`resolveMethodReceiverTypeFromNameExpr`, and
`resolveMethodReceiverTarget`/G7) and
`IrLowererSetupTypeCollectionHelpers.cpp` (1143 lines - Row G continued
(II): the full `preferred*ErrorHelperTarget`/`isExplicit*AliasPath`/
`canonicalKeyValueHelperPath`/`normalizeCollectionHelperPath` family,
plus everything else in the file). This completes explicit branch-level
coverage of all three `ir_lowerer` files this document's "Problem,
Verified" section originally named. New findings this round, in
descending order of concreteness:
- **Dead stub, live impact**: `resolveKeyValueHelperAliasName`/
  `resolveBorrowedKeyValueHelperAliasName` in
  `IrLowererSetupTypeCollectionHelpers.cpp` unconditionally `return
  false` for every input (a stub, not an accidental gap - the parameter
  is explicitly void-cast), while their vector-family counterpart
  (`resolveVectorHelperAliasName`) is fully implemented. This silently
  makes `isExplicitKeyValueHelperFallbackPath` and
  `isExplicitKeyValueReceiverProbeHelperExpr` permanently return `false`
  - both are live "block this inference" guards consulted from
  `inferBuiltinAccessReceiverResultKind` and `resolveMethodReceiverTarget`
  in the sibling file, so this is a real (not latent) asymmetry between
  how vector and key-value receivers are probed, not merely dead code in
  isolation.
- **Dead code, no impact**: two branches in
  `resolveMethodReceiverTypeFromLocalInfo` (`Reference && !structTypeName.empty()`
  and `Value && !structTypeName.empty()`) are unreachable, because the
  same function already unconditionally returns on `!structTypeName.empty()`
  at its very top regardless of `LocalInfo::Kind` - reads as refactor
  residue, harmless.
- **A sixth-and-seventh-plus tally of independently-coded "what type
  family is this receiver" predicates**, now counted precisely within
  just these two files: `resolveMethodReceiverTypeFromLocalInfo`'s
  `LocalInfo`-kind cascade, `resolveMethodReceiverTarget`'s
  `argsPackElementKind`-keyed cascade and its `dereference`-wrapped-local
  lambda (both re-deriving the same family classification a third and
  fourth way within one function), `isSoaVectorReceiverExpr` as a
  fifth (Expr-shape-based) SOA-membership test, and
  `isBuiltinCollectionTypeName`/`isExperimentalCollectionTypeName` as a
  sixth (string-shape-based) family-membership pair - on top of the
  four already tallied across Rows B/C/F. The "String receiver access
  means character access, so treat result as `Int32`" rule specifically
  recurs *four* separate times across these two files alone.
- Two bare-error-family-Name-receiver handlers
  (`IrLowererSetupTypeMethodCallResolution.cpp`'s G6 and this round's
  `resolveMethodReceiverTypeFromNameExpr`) both match a literal
  `FileError`/`ImageError`/`ContainerError`/`GfxError`-spelled receiver,
  but G6 gates on a method-name allowlist via
  `preferred*ErrorHelperTarget` while `resolveMethodReceiverTypeFromNameExpr`
  (reached only when G6's allowlist match fails to find a `defMap` entry)
  has no method-name gate at all - a second, differently-scoped
  reimplementation of the same "is this receiver a bare error-family
  name" question within one cascade.
- Several smaller, lower-confidence asymmetries flagged inline above
  (CH-V4 vs. CH-V5's removed-name-check asymmetry, CH-V6 vs. CH-V7/CH-V8's
  narrower non-canonical-SOA-prefix acceptance set, the array→vector
  compat-candidate expansion in `collectionHelperPathCandidates` having
  no map-rooted counterpart, and `normalizeMapImportAliasPath` being a
  pure identity function of undetermined purpose).

Still open, not attempted this round: `inferQueryExprTypeText`/
`resolveResultTypeForExpr`/`resolveResultTypeFromTypeName` (Q2/Q3/Q3b's
own sub-helpers, cited by name only in R14);
`resolveMethodReceiverStructTypePathFromCallExpr` (cited by name in
RT3b-vi above, declared in a header included by
`IrLowererSetupTypeReceiverTargetHelpers.cpp` but not defined in either
file read this round or last round - its defining file wasn't
identified); Row category G's (and this round's RT/CH rows') many
`UNPINNED` sub-guards would benefit from a pass cross-referencing them
against `PrimeStruct_backend_ir_tests` and the semantics/monomorphization
test suites by name, not attempted in any round so far; whether R14's
local-aware traversal is genuinely exempt from
`skipLocalAwareCallRefinement_` in practice (flagged, not resolved, in
R14's own section); and whether `getNamespacedCollectionHelperName`'s
now-confirmed-dead map-family branch and `normalizeMapImportAliasPath`'s
identity-function body have any live caller anywhere in the tree outside
these two files (neither round's grep scope extended past
`src/ir_lowerer/`).

### Step 0 synthesis (2026-09-08, fourth round): is Step 0 substantially
complete enough to scope Step 1b?

All three stages this document's "Problem, Verified" section names as
independently reimplementing receiver-target resolution now have
explicit, branch-level characterization: semantics (Rows A-E, covering
the args-pack/vector/key-value method-target resolver families and all
five snapshot-collection mechanisms including `query_facts`),
monomorphization (Row F, including its F3 receiver-type-inference
sub-cascade), and `ir_lowerer` (Row G plus this round's two
continuations, covering all three of `IrLowererSetupTypeMethodCallResolution.cpp`,
`IrLowererSetupTypeReceiverTargetHelpers.cpp`, and
`IrLowererSetupTypeCollectionHelpers.cpp`). That is the literal scope
the "Problem, Verified" section named, and it is now done.

**In favor of treating Step 0 as substantially complete:** the evidence
pattern has been consistent and has stopped changing in kind across four
rounds. Every stage answers the same handful of underlying questions
(is this receiver a vector/SOA/array/map/Buffer/error-type; is it
borrowed or owned; does an args-pack-wrapped or `Reference`/`Pointer`-wrapped
receiver unwrap correctly) with its own independently-coded predicate,
and every round of this investigation - regardless of which file it
targeted - has found more instances of the same pattern rather than a
qualitatively new one. This round's two files alone contained at least
six more independent re-derivations of "what type family is this
receiver" on top of the ones already tallied in Rows A-G. The dead-stub
finding (key-value alias-name resolution) is new *in kind* (a
permanently-false predicate, not just a divergent one) but it still
fits the meta-pattern: it exists specifically because vector and
key-value families are handled by parallel-but-not-shared code paths, so
one path can silently rot while its sibling stays live. A shared-name-set
library and a differential-audit harness (Step 1a/1b) are aimed exactly
at this class of problem, and there is now a large, concrete rule table
across three stages and roughly a dozen files to drive that harness's
test-case generation from.

**Specific gap that could still matter before Step 1b scoping starts:**
none of the `UNPINNED` guards in Rows F or G (or this round's RT/CH
rows) have been cross-referenced against the actual test suites by name
- every round including this one has deferred that pass. A
differential-audit harness's value depends on knowing which of these
branches already have *some* test coverage (so a differential check adds
confidence) versus which have none (so a differential check is the
*first* signal ever generated for that branch). Scoping Step 1b without
that pass risks either duplicating existing coverage or, worse,
producing a harness that reports "all branches agree" for branches no
existing test exercises in a way that would catch a real disagreement.
This is a scoping-quality gap, not a completeness-of-inventory gap - the
rule table itself is unlikely to gain much more from further Step 0
rounds at this point, since four consecutive rounds have found
diminishing marginal *new kinds* of divergence (mostly refinements and
tallies of already-established patterns by this round) even as the
absolute count of individually-named divergences keeps growing.

**Overall assessment**: the inventory is broad and deep enough that
another full Step 0 round targeting a *new file* is unlikely to change
the qualitative picture - the case for consolidation is already
overwhelming on the evidence gathered. The one thing worth doing before
committing to Step 1b's specific scope is the test-cross-reference pass
named above (even a partial one, focused on the highest-confidence
"live, not latent" findings from this round and Row F), so that Step
1b's harness is designed to fill the coverage gaps this investigation
has found rather than only re-confirming what's already pinned. This is
an assessment for whoever picks up the next phase to weigh, not a
decision made here - per this task's own instructions, Step 1b work is
explicitly not started in this round regardless of this assessment.

## Step 0 UNPINNED test-coverage cross-reference (2026-09-08, fifth round)

Per the fourth round's own synthesis ("Specific gap that could still
matter before Step 1b scoping starts"), this round cross-references a
prioritized subset of the accumulated `UNPINNED` rule-table rows against
the actual test suites by name — grepping `tests/` for the exact function
names and distinctive literal shapes each row names, and building small
`.prime` repros compiled with `--dump-stage semantic-product` (or plain
`--emit=native`) via the existing `build-release/primec` binary where a
grep alone couldn't settle reachability. No source file was modified; all
repro `.prime` files were written under the session scratchpad and never
copied into the repo (confirmed via `git status`/`git diff --stat` before
committing).

**Starting count.** 68 individually-`UNPINNED`-tagged rows/sub-guards
across Row categories A-G (counted by grepping this document's own
`UNPINNED` occurrences), plus several inline "not independently verified"
notes that don't carry the literal tag. Per this task's own budget
instruction, this round did not attempt all 68 — it prioritized (a) rows
this document's own fourth-round synthesis flagged as reachability-
relevant or "live, not latent", then (b) rows in the most heavily-
duplicated FileError/args-pack-element/collection-specialization logic,
then stopped once a representative, evidence-based sample was in hand.
**14 rows were audited this round**; the rest remain open for a future
round (full list at the end of this section).

### Resolved to "has coverage" (no longer UNPINNED)

None of the 14 audited rows resolved to "yes, a specific existing test
exercises exactly this branch" — the rows chosen for this round were
deliberately the ones this document's own text already flagged as
suspicious gaps, and the grep/repro evidence below confirms the suspicion
in every case rather than turning up an overlooked test. This is not
surprising given the prioritization criterion (rows already flagged
"live, not latent" or newly-found-this-round were preferentially chosen
over rows more likely to already be incidentally covered) — a future round
sampling further down this document's own `UNPINNED` list (e.g. the many
`RT`/`CH`/`G3c`/`G9` sub-guards in Row G, which weren't touched this round)
is more likely to turn up incidental hits.

### Confirmed genuinely zero test coverage

- **R2b** (`resolveArgsPackElementMethodTarget`'s FileError-args-pack-
  element-with-non-{why,is_eof,status,result}-method fallthrough to R7).
  Grepped the whole `tests/` tree for every `args<FileError` occurrence
  (3 hits, all in near-identical fixtures —
  `test_compile_run_native_backend_core_error_and_file_variadics.cpp`,
  `test_compile_run_emitters_loop_sugar_runtime.cpp`,
  `test_ir_pipeline_conversions_variadic_file_errors.cpp`) — every one
  calls only `.why()` on the args-pack `FileError` element, never any
  other method name. Built a repro
  (`score_errors([args<FileError>] values) { return(count(values[0i32].bogus_method())) }`)
  confirming the shape compiles far enough to reach a generic "unable to
  infer return type" semantic error rather than crashing or hitting an
  unrelated diagnostic — the fallthrough path is reachable, just never
  pinned by name.
- **R4b** (Buffer-args-pack-element-with-non-helper-method fallthrough)
  and **R6b** (bare, non-template `Buffer`/`File`-typed args-pack element
  never reaching R3-R6 dispatch at all). Grepped the whole `tests/` tree
  for `args<Buffer>` and `args<File>` — **zero hits for either**, in any
  file. Both branches' entire guard shape (an args-pack element literally
  typed `Buffer<...>`/`Buffer`/`File<...>`/`File`) is unexercised by the
  test corpus, not just the specific fallthrough sub-case.
- **R13/H2's `Pointer<...>`-wrapped and doubly-wrapped
  `Reference<Pointer<...>>`/`Pointer<Reference<...>>` collection-
  specialization shapes.** `collection_specializations` is exercised by
  exactly one test file
  (`test_semantics_type_resolution_graph_snapshots_semantic_product_publishes_ids.cpp`
  — confirmed via `grep -rl "collection_specializations" tests/unit/`,
  one hit). Every `isPointer` assertion in that file is `CHECK_FALSE`
  (`mapEntry`/`soaEntry`) — there is no fixture anywhere in it with a
  genuine `Pointer<vector<T>>`/`Pointer<map<K,V>>`/`Pointer<soa<T>>`
  binding, so `isPointer==true` is never observed, let alone a doubly-
  wrapped chain.
- **E2b / R11's F4, specifically in the pilot-routing-reachable window**
  (`skipLocalAwareCallRefinement_` forced `true`, i.e. worker-count > 1
  validation). This is the row this document's own fourth-round text
  explicitly named as "live, not merely latent, for whatever definitions
  get routed through the pilot path" and "not confirmed this round" —
  now confirmed. Found every test that actually drives
  `benchmarkSemanticDefinitionValidationWorkerCount`/
  `definitionValidationWorkerCount` > 1 (4 call sites:
  `test_semantics_tsan_smoke.cpp`'s two `TEST_CASE`s via
  `buildParallelSuccessFixture`/`buildParallelDiagnosticFixture`, and
  `test_ir_pipeline_backends_registry_semantic_pipeline_benchmark_compile.cpp`'s
  three worker-count stress cases via `buildMathStressSemanticSource`,
  all three helpers defined in
  `tests/unit/ir_pipeline/backends/test_ir_pipeline_backends_registry_shared.h`
  and `test_semantics_tsan_smoke.cpp` itself — read all three source
  generators in full). None contains a `[spawn]` transform or a bare
  unqualified `count(...)`/`capacity(...)` call — every one is built from
  `plus`/`assign`/`/std/math/abs`/simple leaf-function calls only. Task-
  spawn (`[spawn]`) and bare `count(...)` are each independently
  well-tested *elsewhere* in the corpus (confirmed: `[spawn]` appears in
  5 other test files; bare `count(...)` appears throughout
  `test_semantics_calls_and_flow_collections_count_helpers_and_bare_map_calls.cpp`),
  just never together with worker-count > 1 in the same compilation unit
  — so the E2b/F4 divergence is exercised in isolation but the specific
  condition that makes it *reachable* (R10-only execution via forced
  `skipLocalAwareCallRefinement_`) is not.

### Resolved as latent-only (not "has coverage", but the open reachability question itself is answered)

Two rows carried an explicit "not confirmed whether reachable in
practice" qualifier; both were resolved this round via direct repro,
without landing any fix (per this document's own non-goal), and without
finding an existing named test either — so they remain formally
`UNPINNED` for test-coverage purposes, but the harder question ("is this
branch even reachable for a real program") is now answered rather than
open:

- **F11-eof** (monomorphization: a *bound* `FileError`-typed variable's
  `.eof()` call, as opposed to `.is_eof()`, has no matching branch in
  `resolveMethodCallTemplateTarget` and would fall through to the generic
  F16 fallback). Repro: `[FileError] err{0i32} ... err.eof()` — rejected
  at the **semantics** stage itself with `unknown method: /FileError/eof`
  before monomorphization ever runs. Grepped the whole `tests/` tree for
  bound-variable `.eof()` calls (as opposed to the literal-Name-receiver
  `FileError.eof()` constructor-call shape, which is well-tested and
  matches F1, not F11-eof) — zero hits, consistent with this being
  rejected upstream for any program that passes semantics validation.
  This resolves the document's own "not confirmed... mirroring the
  TODO-5286 'real gap, unconfirmed live impact' shape" note at F11-eof:
  it is real-but-unreachable, confirmed by direct evidence rather than
  left open.
- **F1-not** (monomorphization: a literal-`Name`-spelled `FileError`
  receiver — not a bound variable — whose method isn't in the 5-name set
  falls through the rest of the cascade "as if F1 didn't exist"). Repro:
  `FileError.bogus_method()` — also rejected at the **semantics** stage
  (`unknown method target for bogus_method`) before reaching
  monomorphization. Same conclusion as F11-eof: real-but-unreachable for
  any semantically-valid program. This strongly suggests (though this
  round did not independently re-verify) that Row G's structurally
  identical `ir_lowerer`-side gaps in this neighborhood (G6's bare-error-
  Name method-name gate, and `resolveMethodReceiverTypeFromNameExpr`'s
  unconditional-vs-gated divergence at line ~1193) are likely also
  latent-only for the same reason — semantics validation gates method
  names on FileError/ImageError/ContainerError/GfxError receivers before
  either later stage ever sees the call — but that inference was not
  independently repro'd for the `ir_lowerer`-specific shapes this round;
  flagged for whoever picks this up next rather than asserted as
  confirmed.

### Inconclusive after reasonable effort (left `UNPINNED`, with what was tried)

- **`getNamespacedCollectionHelperName`'s dead map-family branch** (fed
  by the stubbed `resolveKeyValueHelperAliasName`, doc line ~1287-1288).
  Broadened the grep beyond the two Row-G-continued files' own scope to
  all of `src/` and `include/`: found the `ir_lowerer`-side function
  (`IrLowererSetupTypeCollectionHelpers.cpp:1124`) does have three live
  callers within `src/ir_lowerer/`
  (`IrLowererLowerInferenceFallbackSetup.cpp`,
  `IrLowererLowerInferenceCallReturnSetup.cpp`,
  `IrLowererLowerInferenceDispatchSetup.cpp`), all of which only check
  `helperName == "count"` on the result — i.e. they only care about the
  vector-family branch (`resolveVectorHelperAliasName`, which works), not
  the dead map-family branch specifically. Whether any of those three
  call sites is ever reached with a receiver that would only resolve
  through the map-family branch (making the dead branch load-bearing by
  omission, silently returning "not count" for a map-family receiver that
  should also return "not count" anyway) was not traced further this
  round — the three call sites' full receiver-reachability conditions
  weren't read in this pass. Also worth noting: there are **two entirely
  separate functions** named `getNamespacedCollectionHelperName` in this
  codebase (`SemanticsBuiltinPathHelpers.cpp:1272` for the semantics
  stage, `IrLowererSetupTypeCollectionHelpers.cpp:1124` for `ir_lowerer`)
  — a same-name-different-implementation collision this document hadn't
  previously called out by name, though it's consistent with the general
  pattern the whole document documents.
- **RT1d** (`isEntryArgsName`-gated silent-defer-vs-hard-error split for
  a bare args-pack-`Entry`-shaped receiver in
  `resolveMethodCallReceiverExpr`). Traced `isEntryArgsName`'s actual
  definition to `IrLowererCountAccessHelpers.cpp` (the map-constructor's
  internal `args<Entry<K,V>>` machinery TODO-4760's final fix also
  touched) but did not find or build a repro isolating this exact
  silent-defer-vs-error split within this round's budget — left open,
  flagged for a future round rather than guessed at.

### Still open — not attempted this round

The remaining ~54 `UNPINNED`-tagged rows were not individually
cross-referenced this round, budgeted per this task's own instruction to
prioritize breadth-appropriate depth over exhaustive coverage of every
tag. Grouped by document row category for whoever picks this up next:

- **Row A**: the `isBuiltinOut` per-branch-asymmetry note (no dedicated
  row ID).
- **Row B**: `classifyExplicitVectorHelperReceiver`'s fixed-order-priority
  contract (the order itself, not its per-family consequences).
- **Row E**: E5, E6, E7's R10-lacks-D5-guard asymmetry, E10 (silent
  absence, not recorded), F6, F7, F9b, F10, F12, R12's G3/G4/G5/G7/G8
  sub-rows, R13's H2b/H3b/H4b/H5b/H6 (the `return false` short-circuits,
  as opposed to H2's positive-shape gap already resolved above), R13's
  production-gate-piggyback-on-`binding_facts` design question, R14's
  Q2/Q3/Q3b/Q4/Q5/Q5b and its own Pass-2-redundancy question.
- **Row F**: F6 (wrapper-method-path), F7 (File-method dispatch details),
  F9/F13/F13b/F13c (primitive/collection-family no-definition fallbacks),
  F12/F14's SOA borrowed-vs-owned asymmetry, F15, F16, F3-N2 (unbound
  `Name` receiver reachability), F3-C3a/b/c/d (the struct-return-path and
  `return<T>`-annotation override-priority gaps).
- **Row G**: G1, G2, G3b, G3c-i through G3c-v, G3d's per-sub-guard detail,
  G9b through G9f, RT2's dead-code note (moot — branches never execute),
  the CH-V4/CH-V5 and CH-V6/CH-V7/CH-V8 asymmetries in
  `resolveVectorHelperAliasName`, and `normalizeMapImportAliasPath`'s
  identity-function purpose.

This round's evidence (5 confirmed-zero, 2 resolved-to-latent-only, 2
inconclusive, out of 14 sampled) is consistent with the fourth round's
prediction: rows this document already flagged as suspicious gaps tend to
in fact be gaps, and FileError-shape method-name-gating divergences at
the monomorphization/`ir_lowerer` layer are frequently masked by earlier,
stricter semantics-stage validation — a pattern worth keeping in mind when
Step 1b scopes which branches actually need differential-audit
scaffolding versus which are dead weight for any semantically-valid
input.

## Risks

- Same environment-noise and rule-table-surfaces-real-inconsistencies
  risks as the compat-spelling consolidation's Risks section — see that
  document; they apply unchanged here.
- Higher risk than the compat-spelling case: the two quirks found during
  Step 1a's audit, plus the `stdlibSurfaceImportAliasPriority` finding from
  TODO-4753, show this area has more interacting, undocumented rules per
  branch than the spelling-disposition problem did. Budget Step 0
  accordingly — it will likely be larger than the compat-spelling
  document's own Step 0.
