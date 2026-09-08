# Receiver-Target Resolution Consolidation Plan

Status: Step 1a landed (name-set library only, unwired). Step 0
(characterize the full rule table) in progress - see "Step 0 Rule Table"
below; semantics-stage method-target resolvers substantially covered,
the four snapshot-collection mechanisms and the ir_lowerer/
monomorphization stages remain. This is the sibling
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

### Row category E: the four snapshot-collection mechanisms (pointers only - not yet individually branch-enumerated)

Documented in narrative form in the "Step 0 Progress" Update sections
above from the TODO-4760 investigation; recorded here as table rows for
completeness, with file:line pointers for whoever expands each into full
branch enumeration next:

| # | mechanism | entry point | shares logic with |
|---|---|---|---|
| R10 | `direct_call_targets` (naive pass) | `collectDirectCallExpr`, `SemanticsValidatorSnapshots.cpp:1578-1636` | feeds `bridge_path_choices` from the same untouched local resolution (confirmed R10/R12 share one broken local variable) |
| R11 | `direct_call_targets` (local-aware overwrite pass) | `inferCallSnapshotData`, `SemanticsValidatorSnapshotLocals.cpp:91-160` | only overwrites R10's answer when its own is non-empty; both ultimately call the same `preferredCollectionHelperResolvedPath` → `resolveCalleePath` fallback chain (`SemanticsValidatorBuildInitializerInference.cpp:105`) |
| R12 | `bridge_path_choices` | populated from R10's naive pass, gated by `isSemanticCollectorEnabled(buildConfig, "bridge_path_choices")`, `SemanticsValidatorSnapshots.cpp:1857,1867` | R10 |
| R13 | `collection_specializations` | populated in `src/frontend/SemanticProduct.cpp` (not yet traced into semantics-stage resolution logic this round) | unknown - not yet traced |

All four confirmed (2026-09-04, see Update sections above) to
independently compute the same wrong answer for the TODO-4760 repro
before that bug's actual root cause (Row category D, not any of these
four) was found - i.e. these four are a real, demonstrated instance of
duplicated receiver-family logic *within* the semantics stage alone, but
turned out not to be the specific TODO-4760 defect's root cause. They
remain open Step 0 rows regardless: `preferredCollectionHelperResolvedPath`
is explicitly, by-design receiver-blind (`CollectionReceiverFamily::None`
hardcoded per the compat-spelling document's own scope decision), so
any future receiver-family classifier work here needs to treat R10-R13
as call sites needing a receiver-aware answer plumbed in, not as
receiver-logic to consolidate directly.

### What remains for Step 0

Not yet characterized to the same branch level as categories A-C above:
monomorphization's `resolveMethodCallTemplateTarget`
(`TemplateMonomorphMethodTargets.cpp`) and
`TemplateMonomorphCollectionCompatibilityPaths.cpp` (including the
`unwrapCollectionReceiverEnvelope` gap from TODO-5286/TODO-5292, see
`docs/todo.md`'s note on those); `ir_lowerer`'s
`IrLowererSetupTypeMethodCallResolution.cpp`,
`IrLowererSetupTypeReceiverTargetHelpers.cpp`, and
`IrLowererSetupTypeCollectionHelpers.cpp` beyond the one gate already
covered in Row category D; and Row category E's four mechanisms need
individual branch enumeration, not just the pointer-level summary above.
Per this document's own Step 0 description, this is real, multi-session
work - not expected to complete in one round.

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
