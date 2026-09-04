# Receiver-Target Resolution Consolidation Plan

Status: Step 1a landed (name-set library only, unwired). Step 0
(characterize the full rule table) not started. This is the sibling
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
